/*
 *  Authors:
 *    Michael Mehl (mehl@dfki.de)
 *    Ralf Scheidhauer (scheidhr@ps.uni-sb.de)
 *
 *  Contributors:
 *    Kostja Popow (popow@ps.uni-sb.de)
 *    Christian Schulte <schulte@ps.uni-sb.de>
 *
 *  Copyright:
 *    Organization or Person (Year(s))
 *
 *  Last change:
 *    $Date$ by $Author$
 *    $Revision$
 *
 *  This file is part of Mozart, an implementation
 *  of Oz 3:
 *     http://www.mozart-oz.org
 *
 *  See the file "LICENSE" or
 *     http://www.mozart-oz.org/LICENSE.html
 *  for information on usage and redistribution
 *  of this file, and for a DISCLAIMER OF ALL
 *  WARRANTIES.
 *
 */

#ifdef INTERFACE
#pragma implementation
#endif

#include "unify.hh"
#include "fset.hh"
#include "thr_int.hh"


/* -------------------------------------------------------------------------
 * Binding
 * -------------------------------------------------------------------------*/

#ifdef DEBUG_CHECK
static
Bool checkHome(TaggedRef *vPtr) {
  TaggedRef val = oz_deref(*vPtr);
  return (!oz_isVariable(val) ||
	  oz_isBelow(oz_currentBoard(), GETBOARD(tagged2Var(val))));
}
#endif

/*
 * oz_bindVar: bind a variable to a value
 *   - trail global vars
 *   - wakeup suspensions
 *   - relink suspensions of local vars
 *   - dispose local vars
 *   - redirect REF
 */
void oz_bindGlobalVar(OzVariable *ov, TaggedRef *varPtr, TaggedRef term)
{
  Assert(tagged2Var(*varPtr)==ov);
  Assert(!oz_isLocalVar(ov));
  Assert(am.inEqEq() || checkHome(varPtr));
  oz_checkSuspensionList(ov, pc_std_unif);
  trail.pushBind(varPtr);
  doBind(varPtr,term);
}

void oz_bindLocalVar(OzVariable *ov, TaggedRef *varPtr, TaggedRef term)
{
  Assert(tagged2Var(*varPtr)==ov);
  Assert(oz_isLocalVar(ov));
  Assert(!am.inEqEq());
  oz_checkSuspensionList(ov, pc_std_unif);
  DEREF(term,termPtr,_);
  if (oz_isVar(term)) {
    OzVariable *sv=tagged2Var(term);
    Assert(sv!=ov);
    ov->relinkSuspListTo(sv);
    term=makeTaggedRef(termPtr);
  } 
  // kost@ : cut away so far;
  /*
    else if (oz_isUVar(term)){
      // mm2: problems with fsp_monitorIn, monitorArity,...
      // Assert(ov->isEmptySuspList());
      term = makeTaggedRef(termPtr);
    }
  */
  oz_var_dispose(ov);
  doBind(varPtr,term);
}

void oz_bind_global(TaggedRef var, TaggedRef term)
{
  DEREF(var,varPtr,_);
  if (!oz_isOptVar(var)) {
    OzVariable *ov=tagged2Var(var);
    oz_checkSuspensionList(ov, pc_all);
    DEREF(term,termPtr,_);
    if (oz_isVar(term)) {
      OzVariable *sv=tagged2Var(term);
      Assert(sv!=ov);
      ov->relinkSuspListTo(sv);
      term=makeTaggedRef(termPtr);
    } else {
      Assert(ov->isEmptySuspList());
    }
    oz_var_dispose(ov);
  }
  doBind(varPtr,term);
}

/* -------------------------------------------------------------------------
 * Unification
 * -------------------------------------------------------------------------*/

inline
Board *getVarBoard(TaggedRef var)
{
  CHECK_ISVAR(var);
  return tagged2Var(var)->getBoardInternal();
}

inline
Bool isMoreLocal(TaggedRef var1, TaggedRef var2)
{
  Board *board1 = getVarBoard(var1);
  Board *board2 = getVarBoard(var2)->derefBoard();
  return oz_isBelow(board1,board2);
}

//
// kost@ : Variables with GREATER types are bound to variables with
// SMALLER types;
#define CMPVAR(v1, v2)		(v1->getType() - v2->getType())

const StackEntry mark=(StackEntry)-1;
class UnifyStack : public Stack {
public:
  UnifyStack() : Stack(100,Stack_WithMalloc) {}
  void pushMark() { push(mark); }
  Bool isMark() { return topElem()==mark; }
};

// global vars!!!
static UnifyStack unifyStack;
static Stack rebindTrail(100,Stack_WithMalloc);

inline
void rebind(TaggedRef *refPtr, TaggedRef term2)
{
  rebindTrail.ensureFree(2);
  rebindTrail.push(refPtr,NO);
  rebindTrail.push(ToPointer(*refPtr),NO);
  doBind(refPtr,term2);
}

#define PopRebindTrail(value,refPtr)                    \
    TaggedRef value   = ToInt32(rebindTrail.pop());     \
    TaggedRef *refPtr = (TaggedRef*) rebindTrail.pop();


OZ_Return oz_unify(TaggedRef t1, TaggedRef t2)
{
  unifyStack.pushMark();
  CHECK_NONVAR(t1); CHECK_NONVAR(t2);

  OZ_Return result = FAILED;

  TaggedRef term1 = t1;
  TaggedRef term2 = t2;
  TaggedRef *termPtr1 = &term1;
  TaggedRef *termPtr2 = &term2;

loop:
  int argSize;

  _DEREF(term1,termPtr1,tag1);
  _DEREF(term2,termPtr2,tag2);

  // identical terms ?
  if (oz_isVar(term1) ? termPtr1 == termPtr2 : term1 == term2) {
    goto next;
  }

  if (oz_isVariable(term1)) {
    if (oz_isVariable(term2)) {
      goto var_var;
    } else {
      goto var_nonvar;
    }
  } else {
    if (oz_isVariable(term2)) {
      Swap(term1,term2,TaggedRef);
      Swap(termPtr1,termPtr2,TaggedRef*);
      Swap(tag1,tag2,TypeOfTerm);
      goto var_nonvar;
    } else {
      goto nonvar_nonvar;
    }
  }


 /*************/
 var_nonvar:

  if (oz_isOptVar(term1)) {
    doBind(termPtr1, term2);
    goto next;
  } else {
    int res = oz_var_bind(tagged2Var(term1),termPtr1, term2);
    if (res == PROCEED) {
      goto next;
    } else {
      result = res;
      goto fail;
    }
  }
  Assert(0);


 /*************/
 var_var:

  /*
   * Specially optimized: local simple vars without suspensions
   */

  if (oz_isOptVar(term1)) {
    // Both have the same home, order them according to age
    if (term1 == term2 && heapNewer(termPtr2, termPtr1))
      doBind(termPtr2, makeTaggedRef(termPtr1));
    else
      doBind(termPtr1, makeTaggedRef(termPtr2));
    goto next;
  }
  
  if (oz_isOptVar(term2)) {
    // Both have the same home, order them according to age
    if (term1 == term2 && heapNewer(termPtr1, termPtr2))
      doBind(termPtr1, makeTaggedRef(termPtr2));
    else
      doBind(termPtr2, makeTaggedRef(termPtr1));
    goto next;
  }
  

  /*
   * Prescribe in which order to bind:
   *  - home(term1) below home(term2) ``term1 is more local''
   *  - never bind the strange guys (see var_base.hh)
   */

  {
    Board * tb1 = getVarBoard(term1)->derefBoard();
    Board * tb2 = getVarBoard(term2)->derefBoard();

    if (oz_isBelow(tb2, tb1)) {
      // t2 should be bound to t1
      Swap(term1, term2, TaggedRef);
      Swap(termPtr1, termPtr2, TaggedRef*);
      Swap(tb1, tb2, Board*);
    } else if (tb1 == tb2 &&
	       CMPVAR(tagged2Var(term1), tagged2Var(term2)) < 0) {
      Swap(term1, term2, TaggedRef);
      Swap(termPtr1, termPtr2, TaggedRef*);
      Swap(tb1, tb2, Board *);
    }

    // Make the variable to which the binding is done fit!

    int res = oz_var_cast(termPtr2, tb2, tagged2Var(term1)->getType());

    // termPtr2 is changed!

    if (res != PROCEED) {
      result = res;
      goto fail;
    }
    
    res = oz_var_unify(tagged2Var(*termPtr1), termPtr1, termPtr2);
    
    if (res == PROCEED)
      goto next;
    result = res;
    goto fail;
  }

 /*************/
 nonvar_nonvar:

  if (tag1 != tag2)
    goto fail;

  switch ( tag1 ) {

  case TAG_LTUPLE:
    {
      LTuple *lt1 = tagged2LTuple(term1);
      LTuple *lt2 = tagged2LTuple(term2);

      rebind(termPtr2,term1);
      argSize = 2;
      termPtr1 = lt1->getRef();
      termPtr2 = lt2->getRef();
      goto push;
    }

  case TAG_SRECORD:
    {
      SRecord *sr1 = tagged2SRecord(term1);
      SRecord *sr2 = tagged2SRecord(term2);

      if (! sr1->compareFunctor(sr2))
	goto fail;

      rebind(termPtr2,term1);
      argSize  = sr1->getWidth();
      termPtr1 = sr1->getRef();
      termPtr2 = sr2->getRef();
      goto push;
    }

  case TAG_SMALLINT:
    if (smallIntEq(term1,term2))
      goto next;
    goto fail;

  case TAG_CONST:
    switch (tagged2Const(term1)->getType()) {
    case Co_Extension:
      {
	int res = tagged2Extension(term1)->eqV(term2);
	if (res == PROCEED)
	  goto next;
	result = res;
	goto fail;
      }

    case Co_Float:
      if (floatEq(term1,term2))
	goto next;
      break;
    case Co_FSetValue:
      if (((FSetValue *) tagged2FSetValue(term1))->unify(term2))
	goto next;
      break;
    case Co_BigInt:
      if (bigIntEq(term1,term2))
	goto next;
      break;
    default:
      break;
    }
    goto fail;

  case TAG_LITERAL:
    /* literals and constants unify if their pointers are equal */
  default:
    goto fail;
  }


 /*************/

next:
  if (unifyStack.isMark()) {
    result = PROCEED;
    goto exit;
  }

  termPtr2 = (TaggedRef*) unifyStack.pop();
  termPtr1 = (TaggedRef*) unifyStack.pop();
  argSize  = ToInt32(unifyStack.pop());
  // fall through

push:
  if (argSize>1) {
    unifyStack.ensureFree(3);
    unifyStack.push(ToPointer(argSize-1),NO);
    unifyStack.push(termPtr1+1,NO);
    unifyStack.push(termPtr2+1,NO);
  }
  term1=tagged2NonVariable(termPtr1);
  term2=tagged2NonVariable(termPtr2);
  goto loop;

fail:
  Assert(result!=PROCEED);
  while (!unifyStack.isMark()) {
    unifyStack.pop();
  }
  // fall through

exit:
  Assert(unifyStack.isMark());
  unifyStack.pop(); // pop mark

  while (!rebindTrail.isEmpty ()) {
    PopRebindTrail(value,refPtr);
    doBind(refPtr,value);
  }

  return result;
}

