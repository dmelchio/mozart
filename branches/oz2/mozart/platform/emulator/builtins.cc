/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: mehl
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$
  */

#include "wsock.hh"

#include <ctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#if defined(LINUX) || defined(SOLARIS_SPARC) || defined(SUNOS_SPARC) || defined(IRIX5_MIPS) || defined(OSF1_ALPHA)
#   define DLOPEN 1
#endif

#ifdef DLOPEN
#ifdef SUNOS_SPARC
#define RTLD_NOW 1
extern "C" void * dlopen(char *, int);
extern "C" char * dlerror(void);
extern "C" void * dlsym(void *, char *);
extern "C" int dlclose(void *);
#else
#include <dlfcn.h>
#endif
#endif

#ifdef IRIX5_MIPS
#include <bstring.h>
#include <sys/time.h>
#endif

#ifdef HPUX_700
#include <dl.h>
#endif

#include "am.hh"
#include "builtins.hh"

#include "genvar.hh"
#include "ofgenvar.hh"
#include "fdbuilti.hh"
#include "fdhook.hh"
#include "solve.hh"
#include "oz_cpi.hh"
#include "dictionary.hh"

/********************************************************************
 * Macros that overload stuff from foreign.cc
 ******************************************************************** */

#define OZ_unify(t1, t2) \
  (am.fastUnify(t1,t2,OK) ? PROCEED : FAILED)

#define OZ_float(f) \
  makeTaggedFloat(f)

#define OZ_atom(s) \
  makeTaggedAtom(s)

#define OZ_int(i) \
  makeInt(i)

/********************************************************************
 * Macros
 ******************************************************************** */

#define NONVAR(X,term)				\
TaggedRef term = X;				\
{ DEREF(term,_myTermPtr,_myTag);		\
  if (isAnyVar(_myTag)) return SUSPEND;		\
}

#define NONVAR1(X,term)				\
TaggedRef term = X;				\
{ DEREF(term,_myTermPtr,_myTag);		\
  if (isAnyVar(_myTag)) {			\
    OZ_suspendOn(X);				\
    return SUSPEND;				\
  }						\
}

// mm2
// Suspend on UVAR and SVAR:
#define NONSUVAR(X,term,tag)			\
TaggedRef term = X;				\
TypeOfTerm tag;					\
{ DEREF(term,_myTermPtr,myTag);			\
  tag = myTag;					\
  if (isNotCVar(tag)) return SUSPEND;		\
}


#define DECLAREBI_USEINLINEREL1(Name,InlineName)	\
OZ_C_proc_begin(Name,1)					\
{							\
  OZ_Term arg1 = OZ_getCArg(0);				\
  OZ_Return state = InlineName(arg1);			\
  if (state == SUSPEND)	{				\
    OZ_suspendOn(arg1);					\
  } else {						\
    return state;					\
  }							\
}							\
OZ_C_proc_end


#define DECLAREBI_USEINLINEREL2(Name,InlineName)	\
OZ_C_proc_begin(Name,2)					\
{							\
  OZ_Term arg0 = OZ_getCArg(0);				\
  OZ_Term arg1 = OZ_getCArg(1);				\
  OZ_Return state = InlineName(arg0,arg1);		\
  if (state == SUSPEND) {				\
    OZ_suspendOn2(arg0,arg1);				\
  } else {						\
    return state;					\
  }							\
}							\
OZ_C_proc_end


#define DECLAREBI_USEINLINEREL3(Name,InlineName)	\
OZ_C_proc_begin(Name,3)					\
{							\
  OZ_Term arg0 = OZ_getCArg(0);				\
  OZ_Term arg1 = OZ_getCArg(1);				\
  OZ_Term arg2 = OZ_getCArg(2);				\
  OZ_Return state = InlineName(arg0,arg1,arg2);		\
  if (state == SUSPEND) {				\
    OZ_suspendOn3(arg0,arg1,arg2);			\
  } else {						\
    return state;					\
  }							\
}							\
OZ_C_proc_end


#define DECLAREBI_USEINLINEFUN1(Name,InlineName)	\
OZ_C_proc_begin(Name,2)					\
{							\
  OZ_Term help;						\
							\
  OZ_Term arg1 = OZ_getCArg(0);				\
  OZ_Return state = InlineName(arg1,help);		\
  switch (state) {					\
  case SUSPEND:						\
    OZ_suspendOn(arg1);					\
  case PROCEED:						\
    return(OZ_unify(help,OZ_getCArg(1)));		\
  default:						\
    return state;					\
  }							\
							\
}							\
OZ_C_proc_end


#define DECLAREBI_USEINLINEFUN2(Name,InlineName)	\
OZ_C_proc_begin(Name,3)					\
{							\
  OZ_Term help;						\
							\
  OZ_Term arg0 = OZ_getCArg(0);				\
  OZ_Term arg1 = OZ_getCArg(1);				\
  OZ_Return state=InlineName(arg0,arg1,help);		\
  switch (state) {					\
  case SUSPEND:						\
    OZ_suspendOn2(arg0,arg1);				\
  case PROCEED:						\
    return(OZ_unify(help,OZ_getCArg(2)));		\
  default:						\
    return state;					\
  }							\
							\
}							\
OZ_C_proc_end


// When InlineName suspends, then Name creates thread containing BlockName
#define DECLAREBINOBLOCK_USEINLINEFUN2(Name,BlockName,InlineName) \
OZ_C_proc_begin(Name,3)					          \
{							          \
  OZ_Term help;						          \
							          \
  OZ_Term arg0 = OZ_getCArg(0);				          \
  OZ_Term arg1 = OZ_getCArg(1);				          \
  OZ_Return state=InlineName(arg0,arg1,help);		          \
  switch (state) {					          \
  case SUSPEND:						          \
    {                                                             \
      RefsArray x=allocateRefsArray(3, NO);                       \
      x[0]=OZ_getCArg(0);                                         \
      x[1]=OZ_getCArg(1);                                         \
      x[2]=OZ_getCArg(2);                                         \
      OZ_Thread thr=OZ_makeSuspendedThread(BlockName,x,3);        \
      OZ_addThread(arg0,thr);                                     \
      OZ_addThread(arg1,thr);                                     \
      return PROCEED;                                             \
    }                                                             \
  case PROCEED:						          \
    return(OZ_unify(help,OZ_getCArg(2)));		          \
  default:						          \
    return state;					          \
  }							          \
							          \
}							          \
OZ_C_proc_end 


// When InlineName suspends, then create thread on second argument
#define DECLARETHR2BI_USEINLINEFUN2(Name,InlineName) \
OZ_C_proc_begin(Name,3)                                           \
{                                                                 \
  OZ_Term help;                                                   \
                                                                  \
  OZ_Term arg0 = OZ_getCArg(0);                                   \
  OZ_Term arg1 = OZ_getCArg(1);                                   \
  OZ_Return state=InlineName(arg0,arg1,help);                     \
  switch (state) {                                                \
  case SUSPEND:                                                   \
    {                                                             \
      RefsArray x=allocateRefsArray(3, NO);                       \
      x[0]=OZ_getCArg(0);                                         \
      x[1]=OZ_getCArg(1);                                         \
      x[2]=OZ_getCArg(2);                                         \
      OZ_Thread thr=OZ_makeSuspendedThread(Name,x,3);             \
      OZ_addThread(arg1,thr);                                     \
      return PROCEED;                                             \
    }                                                             \
  case PROCEED:                                                   \
    return(OZ_unify(help,OZ_getCArg(2)));                         \
  default:                                                        \
    return state;                                                 \
  }                                                               \
                                                                  \
}                                                                 \
OZ_C_proc_end


#define DECLAREBI_USEINLINEFUN3(Name,InlineName)	\
OZ_C_proc_begin(Name,4)					\
{							\
  OZ_Term help;						\
							\
  OZ_Term arg0 = OZ_getCArg(0);				\
  OZ_Term arg1 = OZ_getCArg(1);				\
  OZ_Term arg2 = OZ_getCArg(2);				\
  OZ_Return state=InlineName(arg0,arg1,arg2,help);	\
  switch (state) {					\
  case SUSPEND:						\
    OZ_suspendOn3(arg0,arg1,arg2);			\
  case PROCEED:						\
    return(OZ_unify(help,OZ_getCArg(3)));		\
  default:						\
    return state;					\
  }							\
							\
}							\
OZ_C_proc_end

#define DECLAREBOOLFUN1(BIfun,BIifun,BIirel)		\
OZ_Return BIifun(TaggedRef val, TaggedRef &out)		\
{							\
  OZ_Return state = BIirel(val);			\
  switch(state) {					\
  case PROCEED: out = NameTrue;  return PROCEED;	\
  case FAILED:  out = NameFalse; return PROCEED;	\
  default: return state;				\
  }							\
}							\
DECLAREBI_USEINLINEFUN1(BIfun,BIifun)

#define DECLAREBOOLFUN2(BIfun,BIifun,BIirel)				\
OZ_Return BIifun(TaggedRef val1, TaggedRef val2, TaggedRef &out)	\
{									\
  OZ_Return state = BIirel(val1,val2);					\
  switch(state) {							\
  case PROCEED: out = NameTrue;  return PROCEED;			\
  case FAILED:  out = NameFalse; return PROCEED;			\
  default: return state;						\
  }									\
}									\
DECLAREBI_USEINLINEFUN2(BIfun,BIifun)

#define CheckLocalBoard(Object,Where);				\
  if (am.currentBoard != Object->getBoard()) {			\
    am.currentBoard->incSuspCount();				\
    return am.raise(E_ERROR,E_KERNEL,"globalState",1,OZ_atom(Where));	\
  }

/********************************************************************
 * BuiltinTab
 ******************************************************************** */

#ifdef BUILTINS2

BuiltinTab builtinTab(750);


BuiltinTabEntry *BIadd(char *name,int arity, OZ_CFun funn, IFOR infun)
{
  BuiltinTabEntry *builtin = new BuiltinTabEntry(name,arity,funn,infun);

  if (builtinTab.htAdd(name,builtin) == NO) {
    warning("BIadd: failed to add %s/%d\n",name,arity);
    delete builtin;
    return NULL;
  }
  return builtin;
}

// add specification to builtin table
void BIaddSpec(BIspec *spec)
{
  for (int i=0; spec[i].name; i++) {
    BIadd(spec[i].name,spec[i].arity,spec[i].fun,spec[i].ifun);
  }
}

/********************************************************************
 * `builtin`
 ******************************************************************** */


OZ_C_proc_begin(BIbuiltin,3)
{
  OZ_declareAtomArg(0,name);
  OZ_declareIntArg(1,arity);

  OZ_Term ret = OZ_getCArg(2);

  BuiltinTabEntry *found = builtinTab.find(name);

  if (found == htEmpty) {
    return am.raise(E_ERROR,E_SYSTEM,"builtinUndefined",1,OZ_getCArg(0));
  }

  if (arity!=-1 && (arity != found->getArity())) {
    return am.raise(E_ERROR,E_SYSTEM,"builtinArity",3,
		    OZ_getCArg(0),OZ_getCArg(1),
		    makeTaggedSmallInt(found->getArity()));
  }

  return OZ_unify(ret,makeTaggedConst(found));
}
OZ_C_proc_end


/********************************************************************
 * Type tests
 ******************************************************************** */

OZ_Return isValueInline(TaggedRef val)
{ 
  NONVAR( val, _1);
  return PROCEED;
}
DECLAREBI_USEINLINEREL1(BIisValue,isValueInline)


OZ_C_proc_begin(BIwaitOr,2) {
  TaggedRef a = OZ_getCArg(0);
  DEREF(a, a_ptr, a_tag);

  if (!isAnyVar(a_tag)) 
    return PROCEED;
  
  TaggedRef b = OZ_getCArg(1);
  DEREF(b, b_ptr, b_tag);

  if (!isAnyVar(b_tag)) 
    return PROCEED;
  
  Assert(isAnyVar(a_tag) && isAnyVar(b_tag));
  
  am.addSuspendVarList(a_ptr);
  am.addSuspendVarList(b_ptr);
  
  return SUSPEND;
}
OZ_C_proc_end


OZ_Return isLiteralInline(TaggedRef t)
{ 
  NONSUVAR( t, term, tag );
  if (isCVar(tag)) {
      switch (tagged2CVar(term)->getType()) {
      case OFSVariable:
	{
          GenOFSVariable *ofsvar=tagged2GenOFSVar(term);
          if (ofsvar->getWidth()>0) return FAILED;
          return SUSPEND;
	}
      case FDVariable:
      case BoolVariable:
          return FAILED;
      default:
          return SUSPEND;
      }
  }
  return isLiteral(tag) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisLiteral,isLiteralInline)
DECLAREBOOLFUN1(BIisLiteralB,isLiteralBInline,isLiteralInline)


OZ_Return isAtomInline(TaggedRef t)
{
  NONSUVAR( t, term, tag );
  if (isCVar(tag)) {
      switch (tagged2CVar(term)->getType()) {
      case OFSVariable:
	{
          GenOFSVariable *ofsvar=tagged2GenOFSVar(term);
          TaggedRef lbl=ofsvar->getLabel();
          DEREF(lbl,_1,lblTag);
	  if (isLiteral(lblTag) && !isAtom(lbl)) return FAILED;
          if (ofsvar->getWidth()>0) return FAILED;
          return SUSPEND;
	}
      case FDVariable:
      case BoolVariable:
          return FAILED;
      default:
          return SUSPEND;
      }
  }
  return isAtom(term) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisAtom,isAtomInline)
DECLAREBOOLFUN1(BIisAtomB,isAtomBInline,isAtomInline)

OZ_Return isLockInline(TaggedRef t)
{
  NONSUVAR( t, term, tag );
  return isLock(term) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisLock,isLockInline)
DECLAREBOOLFUN1(BIisLockB,isLockBInline,isLockInline)




OZ_Return isFreeRelInline(TaggedRef term) {
  DEREF(term, _1, tag);
  switch (tag) {
  case UVAR: case SVAR: return PROCEED;
  default:              return FAILED;
  }
}

DECLAREBI_USEINLINEREL1(BIisFreeRel,isFreeRelInline)
DECLAREBOOLFUN1(BIisFree,isFreeInline,isFreeRelInline)

OZ_Return isKindedRelInline(TaggedRef term) {
  DEREF(term, _1, tag);
  return isCVar(tag) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisKindedRel,isKindedRelInline)
DECLAREBOOLFUN1(BIisKinded,isKindedInline,isKindedRelInline)

OZ_Return isDetRelInline(TaggedRef term) {
  DEREF(term, _1, tag);
  return isAnyVar(tag) ? FAILED : PROCEED;
}

DECLAREBI_USEINLINEREL1(BIisDetRel,isDetRelInline)
DECLAREBOOLFUN1(BIisDet,isDetInline,isDetRelInline)



OZ_Return isNameInline(TaggedRef t)
{
  NONSUVAR( t, term, tag );
  if (isCVar(tag)) {
      switch (tagged2CVar(term)->getType()) {
      case OFSVariable:
	{
          GenOFSVariable *ofsvar=tagged2GenOFSVar(term);
          TaggedRef lbl=ofsvar->getLabel();
          Assert(lbl!=makeTaggedNULL());
          DEREF(lbl,_1,lblTag);
          if (isAtom(lbl)) return FAILED;
          if (ofsvar->getWidth()>0) return FAILED;
          return SUSPEND;
	}
      case FDVariable:
      case BoolVariable:
          return FAILED;
      default:
          return SUSPEND;
      }
  }
  if (!isLiteral(tag)) return FAILED;
  return isAtom(term) ? FAILED: PROCEED;
}

DECLAREBI_USEINLINEREL1(BIisName,isNameInline)
DECLAREBOOLFUN1(BIisNameB,isNameBInline,isNameInline)


OZ_Return isTupleInline(TaggedRef t)
{
  NONSUVAR( t, term, tag );
  if (isCVar(tag)) {
      switch (tagged2CVar(term)->getType()) {
      case OFSVariable:
	{
          GenOFSVariable *ofsvar=tagged2GenOFSVar(term);
          if (ofsvar->getWidth()>0) return FAILED;
          return SUSPEND;
	}
      case FDVariable:
      case BoolVariable:
          return FAILED;
      default:
          return SUSPEND;
      }
  }
  return isTuple(term) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisTuple,isTupleInline)
DECLAREBOOLFUN1(BIisTupleB,isTupleBInline,isTupleInline)


OZ_Return isRecordInline(TaggedRef t)
{
  NONSUVAR( t, term, tag );
  if (isCVar(tag)) {
      switch (tagged2CVar(term)->getType()) {
      case FDVariable:
      case BoolVariable:
          return FAILED;
      default:
          return SUSPEND;
      }
  }
  return isRecord(term) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisRecord,isRecordInline)
DECLAREBOOLFUN1(BIisRecordB,isRecordBInline,isRecordInline)


OZ_Return isProcedureInline(TaggedRef t)
{
  NONVAR( t, term);
  return isProcedure(term) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisProcedure,isProcedureInline)
DECLAREBOOLFUN1(BIisProcedureB,isProcedureBInline,isProcedureInline)

OZ_Return isChunkInline(TaggedRef t)
{
  NONSUVAR( t, term, tag );
  if (isCVar(tag)) {
      switch (tagged2CVar(term)->getType()) {
      case OFSVariable:
      case FDVariable:
      case BoolVariable:
          return FAILED;
      default:
          return SUSPEND;
      }
  }
  return isChunk(term) ? PROCEED : FAILED;
}
DECLAREBI_USEINLINEREL1(BIisChunk,isChunkInline)
DECLAREBOOLFUN1(BIisChunkB,isChunkBInline,isChunkInline)

OZ_Return procedureArityInline(TaggedRef procedure, TaggedRef &out)
{
  NONVAR( procedure, pterm );

  if (isProcedure(pterm)) {
    int arity;
    ConstTerm *rec = tagged2Const(pterm);

    switch (rec->getType()) {
    case Co_Abstraction:
      arity = ((Abstraction *) rec)->getArity();
      break;
    case Co_Builtin:
      arity = ((BuiltinTabEntry *) rec)->getArity();
      break;
    default:
      goto typeError;
    }
    out = newSmallInt(arity);
    return PROCEED;
  }
  goto typeError;

typeError:
  out = nil();
  TypeErrorT(0,"Procedure");
}

DECLAREBI_USEINLINEFUN1(BIprocedureArity,procedureArityInline)

OZ_Return isCellInline(TaggedRef cell)
{
  NONVAR( cell, term);
  return isCell(term) ? PROCEED : FAILED;
}
DECLAREBI_USEINLINEREL1(BIisCell,isCellInline)
DECLAREBOOLFUN1(BIisCellB,isCellBInline,isCellInline)

OZ_Return isPortInline(TaggedRef port)
{
  NONVAR( port, term );
  return isPort(term) ? PROCEED : FAILED;
}
DECLAREBI_USEINLINEREL1(BIisPort,isPortInline)
DECLAREBOOLFUN1(BIisPortB,isPortBInline,isPortInline)

/*********************************************************************
 * OFS Records
 *********************************************************************/

/*
 * Constrain term to a record, with given label (wait until
 * determined), with an initial size sufficient for at least tNumFeats
 * features.  If term is already a record, do nothing.
 */

OZ_C_proc_begin(BIsystemTellSize,3)
{
  TaggedRef label = OZ_getCArg(0);
  TaggedRef tNumFeats = OZ_getCArg(1);
  TaggedRef t = OZ_getCArg(2); 

  // Wait for label
  DEREF(label,labelPtr,labelTag);
  DEREF(t, tPtr, tag);

  /* most probable case first */
  if (isLiteral(labelTag) && isNotCVar(tag)) {
    DEREF(tNumFeats, nPtr, ntag);
    if (!isSmallInt(tNumFeats)) TypeErrorT(1,"Int");
    dt_index numFeats=smallIntValue(tNumFeats);
    dt_index size=ceilPwrTwo((numFeats<=FILLLIMIT) ? numFeats
			     : (int)ceil((double)numFeats/FILLFACTOR));
    GenOFSVariable *newofsvar=new GenOFSVariable(label,size);
    Bool ok=am.unify(makeTaggedRef(newTaggedCVar(newofsvar)),makeTaggedRef(tPtr));
    Assert(ok);
    return PROCEED;
  }

  switch (labelTag) {
  case LTUPLE:
  case SRECORD:
    TypeErrorT(0,"Literal");
  case LITERAL:
    break;
  case UVAR:
  case SVAR:
    OZ_suspendOn (makeTaggedRef(labelPtr));
  case CVAR:
    switch (tagged2CVar(label)->getType()) {
    case OFSVariable:
      {
        GenOFSVariable *ofsvar=tagged2GenOFSVar(label);
        if (ofsvar->getWidth()>0) return FAILED; 
       OZ_suspendOn (makeTaggedRef(labelPtr));
      }
    case FDVariable:
    case BoolVariable:
      TypeErrorT(0,"Literal");
    default:
      OZ_suspendOn (makeTaggedRef(labelPtr));
    }
  default:
    TypeErrorT(0,"Literal");
  }

  Assert(labelTag == LITERAL);

  // Create record:
  switch (tag) {
  case LTUPLE:
    return literalEq(label, AtomCons) ? PROCEED : FAILED;
  case LITERAL:
    return literalEq(label, t) ? PROCEED : FAILED;
  case SRECORD:
    return literalEq(label, tagged2SRecord(t)->getLabel()) ? PROCEED : FAILED;
  case CVAR:
    if (tagged2CVar(t)->getType()==OFSVariable) {
       OZ_Return ret=OZ_unify(tagged2GenOFSVar(t)->getLabel(),label);
       tagged2GenOFSVar(t)->propagateOFS();
       return ret;
    }
    // else fall through to creation case
  case UVAR:
  case SVAR:
    {
      // Calculate initial size of hash table:
      DEREF(tNumFeats, nPtr, ntag);
      if (!isSmallInt(tNumFeats)) TypeErrorT(1,"Int");
      dt_index numFeats=smallIntValue(tNumFeats);
      dt_index size=ceilPwrTwo((numFeats<=FILLLIMIT) ? numFeats
                                                     : (int)ceil((double)numFeats/FILLFACTOR));
      // Create newofsvar with unbound variable as label & given initial size:
      GenOFSVariable *newofsvar=new GenOFSVariable(label,size);
      // Unify newofsvar and term:
      Bool ok=am.unify(makeTaggedRef(newTaggedCVar(newofsvar)),makeTaggedRef(tPtr));
      Assert(ok);
      return PROCEED;
    }
  default:
    return FAILED;
  }
}
OZ_C_proc_end


// Constrain term to a record, with given label (wait until determined).
OZ_C_proc_begin(BIrecordTell,2)
{
  TaggedRef label = OZ_getCArg(0);
  TaggedRef t = OZ_getCArg(1);

  // Wait for label
  DEREF(t, tPtr, tag);
  DEREF(label,labelPtr,labelTag);

  /* most probable case first */
  if (isLiteral(labelTag) && isNotCVar(tag)) {
    GenOFSVariable *newofsvar=new GenOFSVariable(label);
    Bool ok=am.unify(makeTaggedRef(newTaggedCVar(newofsvar)),makeTaggedRef(tPtr));
    Assert(ok);
    return PROCEED;
  }

  switch (labelTag) {
  case LTUPLE:
  case SRECORD:
    TypeErrorT(0,"Literal");
  case LITERAL:
    break;
  case UVAR:
  case SVAR:
    OZ_suspendOn (makeTaggedRef(labelPtr));
  case CVAR:
    switch (tagged2CVar(label)->getType()) {
    case OFSVariable:
      {
        GenOFSVariable *ofsvar=tagged2GenOFSVar(label);
        if (ofsvar->getWidth()>0) return FAILED;
        OZ_suspendOn (makeTaggedRef(labelPtr));
      }
    case FDVariable:
    case BoolVariable:
      TypeErrorT(0,"Literal");
    default:
      OZ_suspendOn (makeTaggedRef(labelPtr));
    }
  default:
    TypeErrorT(0,"Literal");
  }

  Assert(labelTag == LITERAL);
  // Create record:
  switch (tag) {
  case LTUPLE:
    return literalEq(label, AtomCons) ? PROCEED : FAILED;
  case LITERAL:
    return literalEq(label, t) ? PROCEED : FAILED;
  case SRECORD:
    return literalEq(label, tagged2SRecord(t)->getLabel()) ? PROCEED : FAILED;
  case CVAR:
    if (tagged2CVar(t)->getType()==OFSVariable) {
       OZ_Return ret=OZ_unify(tagged2GenOFSVar(t)->getLabel(),label);
       tagged2GenOFSVar(t)->propagateOFS();
       return ret;
    }
    TypeErrorT(0,"Record");
    // else fall through to creation case
  case UVAR:
  case SVAR:
    {
      // Create newofsvar with unbound variable as label & given initial size:
      GenOFSVariable *newofsvar=new GenOFSVariable(label);
      // Unify newofsvar and term:
      Bool ok=am.unify(makeTaggedRef(newTaggedCVar(newofsvar)),makeTaggedRef(tPtr));
      Assert(ok);
      return PROCEED;
    }
  default:
    return FAILED;
  }
}
OZ_C_proc_end


// Suspend until can determine whether term is a record or not.
// This routine extends isRecordInline to accept undetermined records.
OZ_Return isRecordCInline(TaggedRef t)
{
  DEREF(t, tPtr, tag);
  switch (tag) {
  case LTUPLE:
  case LITERAL:
  case SRECORD:
    break;
  case CVAR:
    switch (tagged2CVar(t)->getType()) {
    case OFSVariable:
        break;
    case FDVariable:
    case BoolVariable:
        return FAILED;
    default:
        return SUSPEND;
    }
    break;
  case UVAR:
  case SVAR:
    return SUSPEND;
  default:
    return FAILED;
  }
  return PROCEED;
}

DECLAREBI_USEINLINEREL1(BIisRecordC,isRecordCInline)
DECLAREBOOLFUN1(BIisRecordCB,isRecordCBInline,isRecordCInline)



//
OZ_C_proc_begin(BIisRecordCVarB,2)
{
  TaggedRef t = OZ_getCArg(0); 
  DEREF(t, tPtr, tag);
  switch (tag) {
  case LTUPLE:
  case LITERAL:
  case SRECORD:
    break;
  case CVAR:
    if (tagged2CVar(t)->getType()!=OFSVariable) 
      return (OZ_unify (OZ_getCArg (1), NameFalse));
    break;
  case UVAR:
  case SVAR:
    return (OZ_unify (OZ_getCArg (1), NameFalse));
  default:
    return (OZ_unify (OZ_getCArg (1), NameFalse));
  }
  return (OZ_unify (OZ_getCArg (1), NameTrue));
}
OZ_C_proc_end


/*
 * {RecordC.widthC X W} -- builtin that constrains number of features
 * of X to be equal to finite domain variable W.  Will constrain X to
 * a record and W to a finite domain.  This built-in installs a
 * WidthPropagator.
 */
OZ_C_proc_begin(BIwidthC, 2)
{
    OZ_EXPECTED_TYPE("record,finite domain");

    TaggedRef rawrec=OZ_getCArg(0);
    TaggedRef rawwid=OZ_getCArg(1);
    TaggedRef rec=OZ_getCArg(0);
    TaggedRef wid=OZ_getCArg(1);
    DEREF(rec, recPtr, recTag);
    DEREF(wid, widPtr, widTag);

    // Wait until first argument is a constrained record (OFS, SRECORD, LTUPLE, LITERAL):
    switch (recTag) {
    case UVAR:
    case SVAR:
      OZ_suspendOn(rawrec);
    case CVAR:
      switch (tagged2CVar(rec)->getType()) {
      case OFSVariable:
          break;
      case FDVariable:
      case BoolVariable:
          TypeErrorT(0,"Record");
      default:
          OZ_suspendOn(rawrec);
      }
      break;
    case SRECORD:
    case LITERAL:
    case LTUPLE:
      break;
    default:
      TypeErrorT(0,"Record");
    }

    // Ensure that second argument wid is a FD or integer:
    switch (widTag) {
    case UVAR:
    case SVAR:
    {
        // Create new fdvar:
        GenFDVariable *fdvar=new GenFDVariable(); // Variable with maximal domain
        // Unify fdvar and wid:
        Bool ok=am.unify(makeTaggedRef(newTaggedCVar(fdvar)),rawwid);
        Assert(ok);
        break;
    }
    case CVAR:
        if (tagged2CVar(wid)->getType()!=FDVariable) return FAILED;
        break;
    case BIGINT:
    case SMALLINT:
        break;
    default:
        return FAILED;
    }

    // This completes the propagation abilities of widthC.  However, entailment is
    // still hard, so this rule will not be added now--we'll wait until people need it.
    //   // If propagator exists already on the variable, just unify the widths
    //   // Implements rule: width(x,w1)/\width(x,w2) -> width(x,w1)/\(w1=w2)
    //   if (recTag==CVAR) {
    //       TaggedRef otherwid=am.getWidthSuspension((void*)BIpropWidth,rec);
    //       if (otherwid!=makeTaggedNULL()) {
    //           return (am.unify(otherwid,rawwid) ? PROCEED : FAILED);
    //       }
    //   }

    OZ_Expect pe;
    OZ_EXPECT(pe, 0, expectRecordVar);
    OZ_EXPECT(pe, 1, expectIntVarAny);

    return pe.spawn(new WidthPropagator(rawrec, rawwid)); // OZ_args[0], OZ_args[1]));
}
OZ_C_proc_end

OZ_CFun WidthPropagator::spawner = BIwidthC;

// Used by BIwidthC built-in
// {PropWidth X W} where X is OFS and W is FD width.
// Assume: rec is OFS or SRECORD or LITERAL.
// Assume: wid is FD or SMALLINT or BIGINT.
// This is the simplest most straightforward possible
// implementation and it can be optimized in many ways.
OZ_Return WidthPropagator::run(void)
{
    int res;
    int recwidth;
    OZ_Return result = SLEEP;

    TaggedRef rec=rawrec;
    TaggedRef wid=rawwid;
    DEREF(rec, recptr, recTag);
    DEREF(wid, widptr, widTag);

    switch (recTag) {
    case SRECORD:
    record:
    case LITERAL:
    case LTUPLE:
    {
        // Impose width constraint
        recwidth=(recTag==SRECORD) ? tagged2SRecord(rec)->getWidth() :
                 ((recTag==LTUPLE) ? 2 : 0);
        if (isGenFDVar(wid)) {
            // GenFDVariable *fdwid=tagged2GenFDVar(wid);
            // res=fdwid->setSingleton(recwidth);
            res=am.unify(makeTaggedSmallInt(recwidth),rawwid);
            if (res==FAILED) { result = FAILED; break; }
        } else if (isSmallInt(widTag)) {
            int intwid=smallIntValue(wid);
            if (recwidth!=intwid) { result = FAILED; break; }
        } else if (isBigInt(widTag)) {
            // BIGINT case: fail
            result = FAILED; break;
        } else {
            error("unexpected wrong type for width in determined widthC");
        }
        result = PROCEED;
        break;
    }
    case CVAR:
    {
        Assert(tagged2CVar(rec)->getType() == OFSVariable);
        // 1. Impose width constraint
        GenOFSVariable *recvar=tagged2GenOFSVar(rec);
        recwidth=recvar->getWidth(); // current actual width of record
        if (isGenFDVar(wid)) {
            // Build fd with domain recwidth..fd_sup:
            OZ_FiniteDomain slice;
            slice.initRange(recwidth,fd_sup);
            OZ_FiniteDomain &dom = tagged2GenFDVar(wid)->getDom();
            if (dom.getSize() > (dom & slice).getSize()) { 
                GenFDVariable *fdcon=new GenFDVariable(slice);
                res=am.unify(makeTaggedRef(newTaggedCVar(fdcon)),rawwid);
                // No loc/glob handling: res=(fdwid>=recwidth);
                if (res==FAILED) { result = FAILED; break; }
            }
        } else if (isSmallInt(widTag)) {
            int intwid=smallIntValue(wid);
            if (recwidth>intwid) { result = FAILED; break; }
        } else if (isBigInt(widTag)) {
            // BIGINT case: fail
            result = FAILED; break;
        } else {
            error("unexpected wrong type for width in undetermined widthC");
        }
        // 2. Convert representation if necessary
        // 2a. Find size and value (latter is valid only if goodsize==TRUE):
        int goodsize,value;
        DEREF(wid,_3,newwidTag);
        if (isGenFDVar(wid)) {
            GenFDVariable *newfdwid=tagged2GenFDVar(wid);
            goodsize=(newfdwid->getDom().getSize())==1;
            value=newfdwid->getDom().getMinElem();
        } else if (isSmallInt(newwidTag)) {
            goodsize=TRUE;
            value=smallIntValue(wid);
        } else {
            goodsize=FALSE;
	    value = 0; // make gcc quiet
        }
        // 2b. If size==1 and all features and label present, 
        //     then convert to SRECORD or LITERAL:
        if (goodsize && value==recwidth) {
            TaggedRef lbl=tagged2GenOFSVar(rec)->getLabel();
            DEREF(lbl,_4,lblTag);
            if (isLiteral(lblTag)) {
                result = PROCEED;
                if (recwidth==0) {
                    // Convert to LITERAL:
                    res=am.unify(rawrec,lbl);
                    if (res==FAILED) error("unexpected failure of Literal conversion");
		} else {
                    // Convert to SRECORD or LTUPLE:
                    // (Two efficiency problems: 1. Creates record & then unifies,
                    // instead of creating & only binding.  2. Rec->normalize()
                    // wastes the space of the original record.)
                    TaggedRef alist=tagged2GenOFSVar(rec)->getTable()->getArityList();
                    Arity *arity=aritytable.find(alist);
                    SRecord *newrec = SRecord::newSRecord(lbl,arity);
		    newrec->initArgs(am.currentUVarPrototype); 
                    res=am.unify(rawrec,newrec->normalize());
                    Assert(res!=FAILED);
                }
            }
        }
        break;
    }
    default:
      // mm2: type error ?
      warning("unexpected bad first argument to widthC");
      result=FAILED;
    }

    return (result);
}



// {RecordC.monitorArity X K L} -- builtin that tracks features added to OFS X
// in the list L.  Goes away if K is determined (if K is determined on first call,
// L is list of current features).  monitorArity imposes that X is a record (like
// RecordC) and hence fails if X is not a record.
OZ_C_proc_begin(BImonitorArity, 3)
{
    OZ_EXPECTED_TYPE("any(record),any,any(list)");

    OZ_Term rec = OZ_getCArg(0);
    OZ_Term kill = OZ_getCArg(1);
    OZ_Term arity = OZ_getCArg(2);

    OZ_Term tmpkill=OZ_getCArg(1);
    DEREF(tmpkill,_1,killTag);
    Bool isKilled = !isAnyVar(killTag);

    OZ_Term tmprec=OZ_getCArg(0);
    DEREF(tmprec,_2,recTag);
    switch (recTag) {
    case LTUPLE:
        return am.unify(arity,makeTupleArityList(2)) ? PROCEED : FAILED;
    case LITERAL:
        // *** arity is nil
        return (am.unify(arity,AtomNil)? PROCEED : FAILED);
    case SRECORD:
    record:
        // *** arity is known set of features of the SRecord
        return am.unify(arity,tagged2SRecord(tmprec)->getArityList()) ? PROCEED : FAILED;
    case UVAR:
    case SVAR:
        OZ_suspendOn(rec);
    case CVAR:
        switch (tagged2CVar(tmprec)->getType()) {
        case OFSVariable:
            break;
        case FDVariable:
        case BoolVariable:
            TypeErrorT(0,"Record");
        default:
            OZ_suspendOn(rec);
        }
        // *** arity is calculated from the OFS; see below
        break;
    default:
        TypeErrorT(0,"Record");
    }
    tmprec=OZ_getCArg(0);
    DEREF(tmprec,_3,_4);

    // At this point, rec is OFS and tmprec is dereferenced and undetermined

    if (isKilled) {
        TaggedRef featlist;
        featlist=tagged2GenOFSVar(tmprec)->getArityList();

        return (am.unify(arity,featlist)? PROCEED : FAILED);
    } else {
        TaggedRef featlist;
        TaggedRef feattail;
        Board *home=am.currentBoard;
        featlist=tagged2GenOFSVar(tmprec)->getOpenArityList(&feattail,home);

        if (!am.unify(featlist,arity)) return FAILED;

        OZ_Expect pe;
        OZ_EXPECT(pe, 0, expectRecordVar);
        OZ_EXPECT(pe, 1, expectVar);

        TaggedRef uvar=makeTaggedRef(newTaggedUVar(home));
        return pe.spawn(
            new MonitorArityPropagator(rec,kill,feattail,uvar,uvar),
            OFS_flag);
    }

    return PROCEED;
}
OZ_C_proc_end // BImonitorArity

OZ_CFun MonitorArityPropagator::spawner = BImonitorArity;

// The propagator for the built-in RecordC.monitorArity 
// {PropFeat X K L FH FT} -- propagate features from X to the list L, and go
// away when K is determined.  L is closed when K is determined.  X is used to
// check in addFeatOFSSuspList that the suspension is waiting for the right
// variable.  FH and FT are a difference list that holds the features that
// have been added.
OZ_Return MonitorArityPropagator::run(void)
{
    // Check if killed:
    TaggedRef kill=K;
    TaggedRef tmpkill=kill;
    DEREF(tmpkill,_2,killTag);
    Bool isKilled = !isAnyVar(killTag);

    TaggedRef tmptail=FT;
    DEREF(tmptail,_3,_4);

    // Get featlist (a difference list stored in the arguments):
    TaggedRef fhead = FH;
    TaggedRef ftail = FT;

    if (tmptail!=AtomNil) {
        // The record is not determined, so reinitialize the featlist:
        // The home board of uvar must be taken from outside propFeat!
        // Get the home board for any new variables:
        Board* home=tagged2VarHome(tmptail);
        TaggedRef uvar=makeTaggedRef(newTaggedUVar(home));
        FH=uvar;
        FT=uvar;
    } else {
        // Precaution for the GC?
        FH=makeTaggedNULL();
        FT=makeTaggedNULL();
    }

    // Add the features to L (the tail of the output list)
    TaggedRef arity=L;
    if (!am.unify(fhead,arity)) return FAILED; // No further updating of the suspension
    L=ftail; // 'ftail' is the new L in the suspension

    if (tmptail!=AtomNil) {
        // The record is not determined, so the suspension is revived:
        if (!isKilled) return (SLEEP);
        else return (am.unify(ftail,AtomNil)? PROCEED : FAILED);
    }
    return PROCEED;
}





// Create new thread on suspension:
OZ_Return uparrowInline(TaggedRef, TaggedRef, TaggedRef&);
DECLAREBI_USEINLINEFUN2(BIuparrow,uparrowInline)

// Block current thread on suspension:
OZ_Return uparrowInlineBlocking(TaggedRef, TaggedRef, TaggedRef&);
DECLAREBI_USEINLINEFUN2(BIuparrowBlocking,uparrowInlineBlocking)


/*
 * NOTE: similar functions are dot, genericSet, uparrow
 */
// X^Y=Z: add feature Y to open feature structure X (Tell operation).
OZ_Return genericUparrowInline(TaggedRef term, TaggedRef fea, TaggedRef &out, Bool blocking)
{
    TaggedRef termOrig=term;
    TaggedRef feaOrig=fea;
    DEREF(term, termPtr, termTag);
    DEREF(fea,  feaPtr,  feaTag);
    int suspFlag=FALSE;

    // optimize the most common case: adding or reading a feature
    if (isCVar(termTag) && tagged2CVar(term)->getType()==OFSVariable &&
	isFeature(feaTag)) {
      GenOFSVariable *ofsvar=tagged2GenOFSVar(term);

      TaggedRef t=ofsvar->getFeatureValue(fea);
      if (t!=makeTaggedNULL()) {
	// Feature exists
	out=t;
	return PROCEED;
      } 
      
      if (am.currentBoard == ofsvar->getBoard()) {
	TaggedRef uvar=makeTaggedRef(newTaggedUVar(am.currentBoard));
	Bool ok=ofsvar->addFeatureValue(fea,uvar);
	Assert(ok);
	ofsvar->propagateOFS();
	out=uvar;
	return PROCEED;
      }
    }

    // Constrain term to a record:
    switch (termTag) {
    case LTUPLE:
    case LITERAL:
    case SRECORD:
        break;
    case UVAR:
    case SVAR:
	if (!isFeature(feaTag)) {
            // Create newofsvar with unbound variable as label:
            GenOFSVariable *newofsvar=new GenOFSVariable();
            // Unify newofsvar and term:
            Bool ok=am.unify(makeTaggedRef(newTaggedCVar(newofsvar)),makeTaggedRef(termPtr));
            Assert(ok);
            term=makeTaggedRef(termPtr);
            DEREF(term, termPtr2, tag2);
            termPtr=termPtr2;
            termTag=tag2;
	}
        break;
    case CVAR:
        if (tagged2CVar(term)->getType()!=OFSVariable) goto typeError1;
        break;
    default:
        goto typeError1;
    }

    // Wait until Y is a feature:
    if (isNotCVar(feaTag)) suspFlag=TRUE;
    if (isCVar(feaTag)) {
        suspFlag=TRUE;
        if (tagged2CVar(fea)->getType()==OFSVariable) {
            GenOFSVariable *ofsvar=tagged2GenOFSVar(fea);
            if (ofsvar->getWidth()>0) goto typeError2;
        }
    }
    if (suspFlag) {
        if (blocking) {
            return SUSPEND;
        } else {
            // Create thread containing relational blocking version of uparrow:
            RefsArray x=allocateRefsArray(3, NO);
            out=makeTaggedRef(newTaggedUVar(am.currentBoard));
            x[0]=termOrig;
            x[1]=feaOrig;
            x[2]=out;
            OZ_Thread thr=OZ_makeSuspendedThread(BIuparrowBlocking,x,3); 
            OZ_addThread(feaOrig,thr);
            return PROCEED;                     
        }
    }
    if (!isFeature(feaTag)) goto typeError2;

    // Add feature and return:
    Assert(term!=makeTaggedNULL());
    switch (termTag) {
    case CVAR:
      {
        Assert(tagged2CVar(term)->getType() == OFSVariable);
        GenOFSVariable *ofsvar=tagged2GenOFSVar(term);
        TaggedRef t=ofsvar->getFeatureValue(fea);
        if (t!=makeTaggedNULL()) {
            // Feature exists
            out=t;
        } else {
            // Feature does not yet exist
            // Add feature by (1) creating new ofsvar with one feature,
            // (2) unifying the new ofsvar with the old.
            if (am.currentBoard == ofsvar->getBoard()) {
                // Optimization:
                // If current board is same as ofsvar board then can add feature directly
                TaggedRef uvar=makeTaggedRef(newTaggedUVar(am.currentBoard));
                Bool ok=ofsvar->addFeatureValue(fea,uvar);
                Assert(ok);
                ofsvar->propagateOFS();
                out=uvar;
            } else {
                // Create newofsvar:
                GenOFSVariable *newofsvar=new GenOFSVariable();
                // Add feature to newofsvar:
                TaggedRef uvar=makeTaggedRef(newTaggedUVar(am.currentBoard));
                Bool ok1=newofsvar->addFeatureValue(fea,uvar);
                Assert(ok1);
                out=uvar;
                // Unify newofsvar and term (which is also an ofsvar):
                Bool ok2=am.unify(makeTaggedRef(newTaggedCVar(newofsvar)),makeTaggedRef(termPtr));
                Assert(ok2);
            }
        }
        return PROCEED;
      }

    case UVAR:
    case SVAR:
      {
        // Create newofsvar:
        GenOFSVariable *newofsvar=new GenOFSVariable();
        // Add feature to newofsvar:
        TaggedRef uvar=makeTaggedRef(newTaggedUVar(am.currentBoard));
	Bool ok1=newofsvar->addFeatureValue(fea,uvar);
	Assert(ok1);
        out=uvar;
        // Unify newofsvar (CVAR) and term (SVAR or UVAR):
	Bool ok2=am.unify(makeTaggedRef(newTaggedCVar(newofsvar)),makeTaggedRef(termPtr));
	Assert(ok2);
        return PROCEED;
      }
  
    case SRECORD:
    record:
      {
        // Get the SRecord corresponding to term:
        SRecord* termSRec=makeRecord(term);

        TaggedRef t=termSRec->getFeature(fea);
        if (t!=makeTaggedNULL()) {
            out=t;
            return PROCEED;
        }
        return FAILED;
      }

    case LTUPLE:
      {
        if (!isSmallInt(fea)) return FAILED;
        int i2 = smallIntValue(fea);
        switch (i2) {
        case 1:
          out=tagged2LTuple(term)->getHead();
          return PROCEED;
        case 2:
          out=tagged2LTuple(term)->getTail();
          return PROCEED;
        }
        return FAILED;
      }

    case LITERAL:
        return FAILED;

    default:
        goto typeError1;
    }
typeError1:
    TypeErrorT(0,"Record");
typeError2:
    TypeErrorT(1,"Feature");
}


OZ_Return uparrowInline(TaggedRef term, TaggedRef fea, TaggedRef &out)
{
    return genericUparrowInline(term, fea, out, FALSE);
}

OZ_Return uparrowInlineBlocking(TaggedRef term, TaggedRef fea, TaggedRef &out)
{
    return genericUparrowInline(term, fea, out, TRUE);
}

// ---------------------------------------------------------------------
// Spaces
// ---------------------------------------------------------------------

#define declareSpace()					\
  OZ_Term tagged_space = OZ_getCArg(0);			\
  DEREF(tagged_space, space_ptr, space_tag);		\
  if (isAnyVar(space_tag))				\
    OZ_suspendOn(makeTaggedRef(space_ptr));		\
  if (!isSpace(tagged_space))				\
    TypeErrorT(0, "Space");				\
  Space *space = (Space *) tagged2Const(tagged_space);


OZ_C_proc_begin(BInewSpace, 2) {
  OZ_Term proc = OZ_getCArg(0);

  DEREF(proc, proc_ptr, proc_tag);
  if (isAnyVar(proc_tag)) 
    OZ_suspendOn(makeTaggedRef(proc_ptr));

  if (!isProcedure(proc))
    TypeErrorT(0, "Procedure");

  Board* CBB = am.currentBoard;
  int    CPP = am.currentThread->getPriority();

  ozstat.incSolveCreated();
  // creation of solve actor and solve board
  SolveActor *sa = new SolveActor(CBB, CPP);

  // thread creation for {proc root}
  sa->inject(CPP, proc);
    
  // create space   
  return OZ_unify(OZ_getCArg(1), makeTaggedConst(new Space(CBB,sa->getSolveBoard())));
} OZ_C_proc_end


OZ_C_proc_begin(BIisSpace, 2) {
  OZ_Term tagged_space = OZ_getCArg(0);

  DEREF(tagged_space, space_ptr, space_tag);

  if (isAnyVar(space_tag))
    OZ_suspendOn(makeTaggedRef(space_ptr));

  return OZ_unify(OZ_getCArg(1), isSpace(tagged_space) ? NameTrue : NameFalse);
} OZ_C_proc_end


OZ_C_proc_begin(BIaskSpace, 2) {
  declareSpace();

  if (space->isFailed())
    return OZ_unify(OZ_args[1], AtomFailed);
  
  if (space->isMerged())
    return OZ_unify(OZ_args[1], AtomMerged);
  
  TaggedRef answer = space->getSolveActor()->getResult();
  
  DEREF(answer, answer_ptr, answer_tag);

  if (isAnyVar(answer_tag))
    OZ_suspendOn(makeTaggedRef(answer_ptr));

  return OZ_unify(OZ_args[1], 
		  (isSTuple(answer) && 
		   literalEq(tagged2SRecord(answer)->getLabel(), 
			     AtomSucceeded))
                    ? AtomSucceeded : answer);
} OZ_C_proc_end


OZ_C_proc_begin(BIaskVerboseSpace, 2) {
  declareSpace();

  if (space->isFailed())
    return OZ_unify(OZ_args[1], AtomFailed);
  
  if (space->isMerged())
    return OZ_unify(OZ_args[1], AtomMerged);

  if (space->getSolveActor()->isBlocked()) {
    SRecord *stuple = SRecord::newSRecord(AtomBlocked, 1);
    stuple->setArg(0, am.currentUVarPrototype);

    if (OZ_unify(OZ_args[1], makeTaggedSRecord(stuple)) == FAILED)
      return FAILED;

    OZ_args[1] = stuple->getArg(0);
  } 
  
  TaggedRef answer = space->getSolveActor()->getResult();
  
  DEREF(answer, answer_ptr, answer_tag);

  if (isAnyVar(answer_tag))
    OZ_suspendOn(makeTaggedRef(answer_ptr));

  return OZ_unify(OZ_args[1], answer);
} OZ_C_proc_end


OZ_C_proc_begin(BImergeSpace, 2) {
  declareSpace();

  if (space->isMerged())
    return am.raise(E_ERROR,E_KERNEL,"spaceMerged",1,tagged_space);

  if (space->isFailed())
    return FAILED;

  if (am.isBelow(am.currentBoard,space->getSolveBoard()->derefBoard()))
    return am.raise(E_ERROR,E_KERNEL,"spaceSuper",1,tagged_space);
      
  Board *CBB = am.currentBoard;

  // Check board
  
  TaggedRef result = space->getSolveActor()->getResult();

  if (result == makeTaggedNULL())
    return FAILED;

  if (OZ_isVariable(result) && OZ_unify(result, AtomMerged) == FAILED)
    return FAILED;

  TaggedRef root = space->getSolveActor()->merge(CBB);
  space->merge();

  if (root == makeTaggedNULL())
    return FAILED;

  return OZ_unify(root, OZ_getCArg(1));
} OZ_C_proc_end


OZ_C_proc_begin(BIcloneSpace, 2) {
  declareSpace();

  if (space->isMerged())
    return am.raise(E_ERROR,E_KERNEL,"spaceMerged",1,tagged_space);

  Board* CBB = am.currentBoard;

  if (space->isFailed())
    return OZ_unify(OZ_getCArg(1),
		    makeTaggedConst(new Space(CBB, (Board *) 0)));

  TaggedRef result = space->getSolveActor()->getResult();
  
  DEREF(result, result_ptr, result_tag);
  if (isAnyVar(result_tag)) 
    OZ_suspendOn(makeTaggedRef(result_ptr));

  return OZ_unify(OZ_getCArg(1),
		  makeTaggedConst(new Space(CBB, 
					    space->getSolveActor()->clone(CBB))));

} OZ_C_proc_end


OZ_C_proc_begin(contChooseInternal, 2) {
  int left  = smallIntValue(OZ_getCArg(0)) - 1;
  int right = smallIntValue(OZ_getCArg(1)) - 1;

  int status = 
    SolveActor::Cast(am.currentBoard->getActor())->choose(left,right);

  if (status==-1) {
    return am.raise(E_ERROR,E_KERNEL,"spaceNoChoices",0);
  } else if (status==0) {
    return FAILED;
  } 

  return PROCEED;
} OZ_C_proc_end



OZ_C_proc_begin(BIchooseSpace, 2) {
  declareSpace();

  if (space->isMerged())
    return am.raise(E_ERROR,E_KERNEL,"spaceMerged",1,tagged_space);

  if (space->isFailed())
    return PROCEED;

  TaggedRef result = space->getSolveActor()->getResult();
  
  DEREF(result, result_ptr, result_tag);
  if (isAnyVar(result_tag)) 
    OZ_suspendOn(makeTaggedRef(result_ptr));

  TaggedRef choice = OZ_getCArg(1);
  
  DEREF(choice, choice_ptr, choice_tag);

  if (isAnyVar(choice_tag))
    OZ_suspendOn(makeTaggedRef(choice_ptr));

  TaggedRef left, right;

  if (isSmallInt(choice_tag)) {
    left  = choice;
    right = choice;
  } else if (isSTuple(choice) &&
	     literalEq(AtomPair,
		       tagged2SRecord(choice)->getLabel()) &&
	     tagged2SRecord(choice)->getWidth() == 2) {
    left  = tagged2SRecord(choice)->getArg(0);
    DEREF(left, left_ptr, left_tag);

    if (isAnyVar(left_tag))
      OZ_suspendOn(makeTaggedRef(left_ptr));

    right = tagged2SRecord(choice)->getArg(1);
    
    DEREF(right, right_ptr, right_tag);

    if (isAnyVar(right_tag))
      OZ_suspendOn(makeTaggedRef(right_ptr));
  } else {
    TypeErrorT(1, "Integer or pair of integers");
  }

  if (am.currentBoard != space->getSolveBoard()->getParent()) 
    return am.raise(E_ERROR,E_KERNEL,"spaceParent",1,tagged_space);
    
  space->getSolveActor()->unsetGround();
  space->getSolveActor()->clearResult(space->getBoard());

  RefsArray args = allocateRefsArray(2, NO);
  args[0] = left;
  args[1] = right;

  Thread *it = am.mkRunnableThread(am.currentThread->getPriority(), 
				   space->getSolveBoard(),
				   OK);
  it->pushCFunCont(contChooseInternal, args, 2, NO);
  am.scheduleThread(it);

  return PROCEED;
} OZ_C_proc_end


OZ_C_proc_begin(BIinjectSpace, 2) {
  declareSpace();

  if (space->isMerged())
    return am.raise(E_ERROR,E_KERNEL,"spaceMerged",1,tagged_space);

  // Check whether space is failed!
  if (space->isFailed())
    return PROCEED;

  if (am.currentBoard != space->getSolveBoard()->getParent()) 
    return am.raise(E_ERROR,E_KERNEL,"spaceParent", 1, tagged_space);

  OZ_Term proc = OZ_getCArg(1);

  DEREF(proc, proc_ptr, proc_tag);

  if (isAnyVar(proc_tag)) 
    OZ_suspendOn(makeTaggedRef(proc_ptr));

  if (!isProcedure(proc))
    TypeErrorT(1, "Procedure");

  Board      *sb = space->getSolveBoard();
  SolveActor *sa = space->getSolveActor();

  // clear status
  sa->unsetGround();
  sa->clearResult(space->getBoard());

  // inject
  sa->inject(sa->getPriority(), proc);
    
  return PROCEED;
} OZ_C_proc_end


#undef declareSpace


// ---------------------------------------------------------------------
// Tuple
// ---------------------------------------------------------------------

OZ_Return tupleInline(TaggedRef label, TaggedRef argno, TaggedRef &out)
{
  DEREF(argno,_1,argnoTag);
  DEREF(label,_2,labelTag);

  if (isSmallInt(argnoTag)) {
    if (isLiteral(labelTag)) {
      int i = smallIntValue(argno);
  
      if (i < 0) {
	goto typeError1;
      }

      // literals
      if (i == 0) {
	out = label;
	return PROCEED;
      }

      {
	SRecord *s = SRecord::newSRecord(label,i);

	TaggedRef newVar = am.currentUVarPrototype;
	for (int j = 0; j < i; j++) {
	  s->setArg(j,newVar);
	}

	out = s->normalize();
	return PROCEED;
      }
    }
    if (isAnyVar(labelTag)) {
      return SUSPEND;
    }
    goto typeError0;
  }
  if (isAnyVar(argnoTag)) {
    if (isAnyVar(labelTag) || isLiteral(labelTag)) {
      return SUSPEND;
    }
    goto typeError0;
  }
  goto typeError1;

 typeError0:
  TypeErrorT(0,"Literal");
 typeError1:
  TypeErrorT(1,"(non-negative small) Int");
}

DECLAREBI_USEINLINEFUN2(BItuple,tupleInline)


// ---------------------------------------------------------------------
// Tuple & Record
// ---------------------------------------------------------------------


OZ_Return labelInline(TaggedRef term, TaggedRef &out)
{
  // Wait for term to be a record with determined label:
  // Get the term's label, if it exists
  DEREF(term,_1,tag);
  switch (tag) {
  case LTUPLE:
    out=AtomCons;
    return PROCEED;
  case LITERAL:
    out=term;
    return PROCEED;
  case SRECORD:
  record:
    out=tagged2SRecord(term)->getLabel();
    return PROCEED;
  case UVAR:
  case SVAR:
    return SUSPEND;
  case CVAR:
    switch (tagged2CVar(term)->getType()) {
    case OFSVariable:
      {
        TaggedRef thelabel=tagged2GenOFSVar(term)->getLabel(); 
        DEREF(thelabel,_1,_2);
        if (isAnyVar(thelabel)) return SUSPEND;
        out=thelabel;
        return PROCEED;
      }
    case FDVariable:
    case BoolVariable:
        TypeErrorT(0,"Record");
    default:
        return SUSPEND;
    }
  default:
    TypeErrorT(0,"Record");
  }
}

DECLAREBI_USEINLINEFUN1(BIlabel,labelInline)

OZ_Return hasLabelInline(TaggedRef term, TaggedRef &out)
{
  // Wait for term to be a record with determined label:
  // Get the term's label, if it exists
  DEREF(term,_1,tag);
  switch (tag) {
  case LTUPLE:
    out=NameTrue;
    return PROCEED;
  case LITERAL:
    out=NameTrue;
    return PROCEED;
  case SRECORD:
  record:
    out=NameTrue;
    return PROCEED;
  case UVAR:
  case SVAR:
    out=NameFalse;
    return PROCEED;
  case CVAR:
    switch (tagged2CVar(term)->getType()) {
    case OFSVariable:
      {
        TaggedRef thelabel=tagged2GenOFSVar(term)->getLabel(); 
        DEREF(thelabel,_1,_2);
        out = isAnyVar(thelabel) ? NameFalse : NameTrue;
	return PROCEED;
      }
    case FDVariable:
    case BoolVariable:
      TypeErrorT(0,"Record");
    default:
      out=NameFalse;
      return PROCEED;
    }
  default:
    TypeErrorT(0,"Record");
  }
}

DECLAREBI_USEINLINEFUN1(BIhasLabel,hasLabelInline)

/*
 * NOTE: similar functions are dot, genericSet, uparrow
 */
OZ_Return genericDot(TaggedRef term, TaggedRef fea, TaggedRef *out, Bool dot)
{
  DEREF(fea, _1,feaTag);
LBLagain:
  DEREF(term, _2, termTag);

  if (isAnyVar(feaTag)) {
    switch (termTag) {
    case LTUPLE:
    case SRECORD:
    case SVAR:
    case UVAR:
      return SUSPEND;
    case CVAR:
      switch (tagged2CVar(term)->getType()) {
      case FDVariable:
      case BoolVariable:
          goto typeError0;
      default:
          return SUSPEND;
      }
      // if (tagged2CVar(term)->getType() == OFSVariable) return SUSPEND;
      // if (tagged2CVar(term)->getType() == AVAR) return SUSPEND;
      // goto typeError0;
    case LITERAL:
      goto typeError0;
    default:
      if (isChunk(term)) return SUSPEND;
      goto typeError0;
    }
  }

  if (!isFeature(feaTag)) goto typeError1;

  switch (termTag) {
  case LTUPLE:
    {
      if (!isSmallInt(fea)) {
	if (dot) goto raise; else return FAILED;
      }
      int i2 = smallIntValue(fea);
      
      if (i2 == 1) {
	if (out) *out = tagged2LTuple(term)->getHead();
	return PROCEED;
      }
      if (i2 == 2) {
	if (out) *out = tagged2LTuple(term)->getTail();
	return PROCEED;
      }

      if (dot) goto raise; else return FAILED;
    }
    
  case SRECORD:
    {
      TaggedRef t = tagged2SRecord(term)->getFeature(fea);
      if (t == makeTaggedNULL()) {
	if (dot) goto raise; else return FAILED;
      }
      if (out) *out = t;
      return PROCEED;
    }
    
  case UVAR:
  case SVAR:
    if (!isFeature(feaTag)) {
      TypeErrorT(1,"Feature");
    }
    return SUSPEND;

  case CVAR:
    {
      if (tagged2CVar(term)->getType() == OFSVariable) {
	GenOFSVariable *ofs=(GenOFSVariable *)tagged2CVar(term);
	TaggedRef t = ofs->getFeatureValue(fea);
	if (t == makeTaggedNULL()) return SUSPEND;
	if (out) *out = t;
	return PROCEED;
      }
      // mm2?
      goto typeError0;
    }

  case LITERAL:
    if (dot) goto raise; else return FAILED;

  default:
    if (isChunk(term)) {
      TaggedRef t;
      switch (tagged2Const(term)->getType()) {
      case Co_Chunk:
	t = tagged2SChunk(term)->getFeature(fea);
	break;
      case Co_Object:
	t = tagged2Object(term)->getFeature(fea);
	break;
      case Co_Array:
      case Co_Dictionary:
      default:
	// no public known features
	t = makeTaggedNULL();
	break;
      }
      if (t == makeTaggedNULL()) {
	if (dot) goto raise; else return FAILED;
      }
      if (out) *out = t;
      return PROCEED;
    }

    /* special case for cells (mm 17.3.95) */
    if (ozconf.cellHack && isCell(term)) {
      Cell *cell= tagged2Cell(term);
      term = cell->getValue();
      goto LBLagain;
    }

    goto typeError0;
  }
typeError0:
  TypeErrorT(0,"Record or Chunk");
typeError1:
  TypeErrorT(1,"Feature");
raise:
  return am.raise(E_ERROR,E_KERNEL,".",2,term,fea);
}


OZ_Return dotInline(TaggedRef term, TaggedRef fea, TaggedRef &out)
{
  return genericDot(term,fea,&out,TRUE);
}
DECLAREBI_USEINLINEFUN2(BIdot,dotInline)


OZ_Return hasFeatureInline(TaggedRef term, TaggedRef fea)
{
  return genericDot(term,fea,0,FALSE);
}

DECLAREBOOLFUN2(BIhasFeatureB,hasFeatureBInline,hasFeatureInline)

OZ_Return subtreeInline(TaggedRef term, TaggedRef fea, TaggedRef &out)
{
  return genericDot(term,fea,&out,FALSE);
}

extern OZ_Return subtreeInline(TaggedRef term, TaggedRef fea, TaggedRef &out);


/*
 * fun {matchDefault Term Attr Defau}
 *    if X in Term.Attr = X then X else Defau fi
 * end
 */

OZ_Return matchDefaultInline(TaggedRef term, TaggedRef attr, TaggedRef defau,
	                     TaggedRef &out)
{
  switch(subtreeInline(term,attr,out)) {
  case PROCEED:
    return PROCEED;
  case SUSPEND:
    return SUSPEND;
  case FAILED:
  default:
    out = defau;
    return PROCEED;
  }
}

DECLAREBI_USEINLINEFUN3(BImatchDefault,matchDefaultInline)


OZ_Return widthInline(TaggedRef term, TaggedRef &out)
{
  DEREF(term,_,tag);

  switch (tag) {
  case LTUPLE:
    out = makeTaggedSmallInt(2);
    return PROCEED;
  case SRECORD:
  record:
    out = makeTaggedSmallInt(tagged2SRecord(term)->getWidth());
    return PROCEED;
  case LITERAL:
    out = makeTaggedSmallInt(0);
    return PROCEED;
  case UVAR:
  case SVAR:
    return SUSPEND;
  case CVAR:
    switch (tagged2CVar(term)->getType()) {
    case OFSVariable:
        return SUSPEND;
    case FDVariable:
    case BoolVariable:
        break;
    default:
        return SUSPEND;
    }
    break;
  default:
    break;
  }

  TypeErrorT(0,"Record");
}

DECLAREBI_USEINLINEFUN1(BIwidth,widthInline)


// ---------------------------------------------------------------------
// Unit
// ---------------------------------------------------------------------

OZ_C_proc_begin(BIgetUnit,1)
{
  return OZ_unify(NameUnit,OZ_getCArg(0));
}
OZ_C_proc_end

OZ_Return isUnitInline(TaggedRef t)
{
  NONSUVAR( t, term, tag);
  if (isCVar(tag)) {
    switch (tagged2CVar(term)->getType()) {
    case OFSVariable:
      {
	GenOFSVariable *ofsvar=tagged2GenOFSVar(term);
	if (ofsvar->getWidth()>0) return FAILED;
	TaggedRef lbl=ofsvar->getLabel();
	DEREF(lbl,_1,lblTag);
	if (isLiteral(lblTag)) {
	  if (isAtom(lbl)) {
	    if (literalEq(term,NameUnit))
	      return SUSPEND;
	    else
	      return FAILED;
	  } else { // isName
	    return FAILED;
	  }
	}
	return SUSPEND;
      }
    case FDVariable:
    case BoolVariable:
      return FAILED;
    default:
      return SUSPEND;
    }
  }
  if (literalEq(term,NameUnit))
    return PROCEED;
  else
    return FAILED;
}

DECLAREBI_USEINLINEREL1(BIisUnit,isUnitInline)
DECLAREBOOLFUN1(BIisUnitB,isUnitBInline,isUnitInline)

// ---------------------------------------------------------------------
// Bool things
// ---------------------------------------------------------------------

OZ_C_proc_begin(BIgetTrue,1)
{
  return OZ_unify(NameTrue,OZ_getCArg(0));
}
OZ_C_proc_end


OZ_C_proc_begin(BIgetFalse,1)
{
  return OZ_unify(NameFalse,OZ_getCArg(0));
}
OZ_C_proc_end


OZ_Return isBoolInline(TaggedRef t)
{
  NONSUVAR( t, term, tag);
  if (isCVar(tag)) {
    switch (tagged2CVar(term)->getType()) {
    case OFSVariable:
      {
	GenOFSVariable *ofsvar=tagged2GenOFSVar(term);
	if (ofsvar->getWidth()>0) return FAILED;
	TaggedRef lbl=ofsvar->getLabel();
	DEREF(lbl,_1,lblTag);
	if (isLiteral(lblTag)) {
	  if (isAtom(lbl)) {
	    if (literalEq(term,NameTrue) ||
		literalEq(term,NameFalse))
	      return SUSPEND;
	    else
	      return FAILED;
	  } else { // isName
	    return FAILED;
	  }
	}
	return SUSPEND;
      }
    case FDVariable:
    case BoolVariable:
      return FAILED;
    default:
      return SUSPEND;
    }
  }
  if (literalEq(term,NameTrue) || literalEq(term,NameFalse))
    return PROCEED;
  else
    return FAILED;
}

DECLAREBI_USEINLINEREL1(BIisBool,isBoolInline)
DECLAREBOOLFUN1(BIisBoolB,isBoolBInline,isBoolInline)

OZ_Return notInline(TaggedRef A, TaggedRef &out)
{
  NONVAR(A,term);

  if (literalEq(term,NameTrue)) {
    out = NameFalse;
    return PROCEED;
  } else {
    if (literalEq(term,NameFalse)) {
      out = NameTrue;
      return PROCEED;
    }
  }

  TypeErrorT(0,"Bool");
}

DECLAREBI_USEINLINEFUN1(BInot,notInline)

// and: not specified (mm)
OZ_Return andInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  TaggedRef nt = NameTrue;

  if (literalEq(A,nt)) {
    if (isAnyVar(B)) {
      return SUSPEND;
    }

    out = literalEq(B,nt) ? nt : NameFalse;
    return PROCEED;
  }

  if (isAnyVar(A)) {
    if (isAnyVar(B) || literalEq(B,nt)) {
      return SUSPEND;
    }
    out = NameFalse;
    return PROCEED;
  }

  out = NameFalse;
  return PROCEED;  
}

DECLAREBI_USEINLINEFUN2(BIand,andInline)


// or: not specified (mm)
OZ_Return orInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  TaggedRef nt = NameTrue;
  
  if (literalEq(A,nt)) {
    out = nt;
    return PROCEED;
  }

  if (literalEq(B,nt)) {
    out = nt;
    return PROCEED;
  }

  if (isAnyVar(tagA) || isAnyVar(tagB)) {
    return SUSPEND;
  }
  
  out = NameFalse;
  return PROCEED;
}

DECLAREBI_USEINLINEFUN2(BIor,orInline)



// ---------------------------------------------------------------------
// Atom
// ---------------------------------------------------------------------

OZ_Return atomToStringInline(TaggedRef t, TaggedRef &out)
{
  DEREF(t,_1,_2);
  if (isAnyVar(t)) return SUSPEND;

  if (!isAtom(t)) {
    TypeErrorT(-1,"atom");
  }
  
  out = OZ_string(tagged2Literal(t)->getPrintName());
  return PROCEED;
}
DECLAREBI_USEINLINEFUN1(BIatomToString,atomToStringInline)


OZ_C_proc_begin(BIstringToAtom,2)
{
  OZ_declareProperStringArg(0,str);
  OZ_Term out = OZ_getCArg(1);

  OZ_Return ret = OZ_unifyAtom(out,str);

  return ret;
}
OZ_C_proc_end

// ---------------------------------------------------------------------
// Virtual Strings
// ---------------------------------------------------------------------

inline
TaggedRef vs_suspend(SRecord *vs, int i, TaggedRef arg_rest) {
  if (i == vs->getWidth()-1) {
    return arg_rest;
  } else {
    SRecord *stuple = SRecord::newSRecord(AtomPair, vs->getWidth() - i);
    stuple->setArg(0, arg_rest);
    i++;
    for (int j=1 ; i < vs->getWidth() ; (j++, i++))
      stuple->setArg(j, vs->getArg(i));
    return makeTaggedSRecord(stuple);
  }
}

static OZ_Return vs_check(OZ_Term vs, OZ_Term *rest) {
  DEREF(vs, vs_ptr, vs_tag);

  if (isAnyVar(vs_tag)) {
    *rest = makeTaggedRef(vs_ptr);
    OZ_suspendOn(*rest);
  } else if (isInt(vs_tag)) {
    return PROCEED;
  } else if (isFloat(vs_tag)) {
    return PROCEED;
  } else if (isLiteral(vs_tag) && tagged2Literal(vs)->isAtom()) {
    return PROCEED;
  } else if (isCons(vs_tag)) {
    TaggedRef cdr  = vs;
    TaggedRef prev = vs;

    while (1) {
      DEREF(cdr, cdr_ptr, cdr_tag);

      if (isNil(cdr))
	return PROCEED;

      if (isAnyVar(cdr_tag)) {
	*rest = prev;
	OZ_suspendOn(makeTaggedRef(cdr_ptr));
      }

      if (!isCons(cdr_tag))
	return FAILED;

      TaggedRef car = tagged2LTuple(cdr)->getHead();
      DEREF(car, car_ptr, car_tag);

      if (isAnyVar(car_tag)) {
	*rest = cdr;
	OZ_suspendOn(makeTaggedRef(car_ptr));
      } else if (!isSmallInt(car_tag) ||
		 (smallIntValue(car) < 0) ||
		 (smallIntValue(car) > 255)) {
	return FAILED;
      } else {
	prev = cdr;
	cdr  = tagged2LTuple(cdr)->getTail();
      }
    };
    
    return FAILED;

  } else if (isSTuple(vs) && 
	     literalEq(tagged2SRecord(vs)->getLabel(),AtomPair)) {
    for (int i=0; i < tagged2SRecord(vs)->getWidth(); i++) {
      TaggedRef arg_rest;
      OZ_Return status = vs_check(tagged2SRecord(vs)->getArg(i), &arg_rest);

      if (status == SUSPEND) {
	*rest = vs_suspend(tagged2SRecord(vs), i, arg_rest);

	return SUSPEND;
      } else if (status==FAILED) {
	return FAILED;
      }
    }
    return PROCEED;
  } else {
    return FAILED;
  }
}


static OZ_Return vs_length(OZ_Term vs, OZ_Term *rest, int *len) {
  DEREF(vs, vs_ptr, vs_tag);

  if (isAnyVar(vs_tag)) {
    *rest = makeTaggedRef(vs_ptr);
    OZ_suspendOn(*rest);
  } else if (isInt(vs_tag)) {
    *len = *len + strlen(toC(vs));
    return PROCEED;
  } else if (isFloat(vs_tag)) {
    *len = *len + strlen(toC(vs));
    return PROCEED;
  } else if (isLiteral(vs_tag) && tagged2Literal(vs)->isAtom()) {
    if (literalEq(vs,AtomPair) || 
	literalEq(vs,AtomNil)) 
      return PROCEED;
    *len = *len + ((Atom*)tagged2Literal(vs))->getSize();
    return PROCEED;
  } else if (isCons(vs_tag)) {
    TaggedRef cdr  = vs;
    TaggedRef prev = vs;

    while (1) {
      DEREF(cdr, cdr_ptr, cdr_tag);

      if (isNil(cdr))
	return PROCEED;

      if (isAnyVar(cdr_tag)) {
	*rest = prev;
	Assert((*len)>0);
	*len = *len - 1;
	OZ_suspendOn(makeTaggedRef(cdr_ptr));
      }

      if (!isCons(cdr_tag))
	return FAILED;

      TaggedRef car = tagged2LTuple(cdr)->getHead();
      DEREF(car, car_ptr, car_tag);

      if (isAnyVar(car_tag)) {
	*rest = cdr;
	OZ_suspendOn(makeTaggedRef(car_ptr));
      } else if (!isSmallInt(car_tag) ||
		 (smallIntValue(car) < 0) ||
		 (smallIntValue(car) > 255)) {
	return FAILED;
      } else {
	prev = cdr;
	cdr  = tagged2LTuple(cdr)->getTail();
	*len = *len + 1;
      }
    };
    
    return FAILED;

  } else if (isSTuple(vs) && 
	     literalEq(tagged2SRecord(vs)->getLabel(),AtomPair)) {
    for (int i=0; i < tagged2SRecord(vs)->getWidth(); i++) {
      TaggedRef arg_rest;
      OZ_Return status = 
	vs_length(tagged2SRecord(vs)->getArg(i), &arg_rest, len);

      if (status == SUSPEND) {
	*rest = vs_suspend(tagged2SRecord(vs), i, arg_rest);
	return SUSPEND;
      } else if (status==FAILED) {
	return FAILED;
      }
    }
    return PROCEED;
  } else {
    return FAILED;
  }
}


OZ_C_proc_begin(BIvsLength,3) {
  TaggedRef rest = makeTaggedNULL();
  int len = smallIntValue(deref(OZ_args[1]));
  OZ_Return status = vs_length(OZ_args[0], &rest, &len);
  if (status == SUSPEND) {
    OZ_args[0] = rest;
    OZ_args[1] = makeTaggedSmallInt(len);
    return SUSPEND;
  } else if (status == FAILED) {
    TypeErrorT(0, "Virtual String");
  } else {
    return OZ_unify(OZ_args[2], makeTaggedSmallInt(len));
  }
} OZ_C_proc_end

OZ_C_proc_begin(BIvsIs,2) {
  TaggedRef rest = makeTaggedNULL();
  OZ_Return status = vs_check(OZ_args[0], &rest);
  if (status == SUSPEND) {
    OZ_args[0] = rest;
    return SUSPEND;
  }
  return OZ_unify(OZ_args[1], (status == PROCEED) ? NameTrue : NameFalse);
} OZ_C_proc_end


// ---------------------------------------------------------------------
// Chunk
// ---------------------------------------------------------------------

OZ_C_proc_begin(BInewChunk,2)
{
  OZ_Term val = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  if (OZ_isVariable(val)) OZ_suspendOn(val);
  val=deref(val);
  if (!isRecord(val)) TypeErrorT(0,"Record");

  return OZ_unify(out,OZ_newChunk(val));
}
OZ_C_proc_end

OZ_C_proc_begin(BIchunkArity,2)
{
  OZ_Term ch =  OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  DEREF(ch, chPtr, chTag);

  switch(chTag) {
  case UVAR:
  case SVAR:
  case CVAR:
    OZ_suspendOn(makeTaggedRef(chPtr));

  case OZCONST: 
    if (!isChunk(ch)) TypeErrorT(0,"Chunk");
    //
    switch (tagged2Const(ch)->getType()) {
    case Co_Object:
      return OZ_unify(out,tagged2Object(ch)->getArityList());
    case Co_Chunk:
      return OZ_unify(out,tagged2SChunk(ch)->getArityList());
    default:
      // no features
      return OZ_unify(out,nil());
    }

  default:
    TypeErrorT(0,"Chunk");
  }
}
OZ_C_proc_end

OZ_C_proc_begin(BIchunkWidth, 2)
{
  OZ_Term ch =  OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  DEREF(ch, chPtr, chTag);

  switch(chTag) {
  case UVAR:
  case SVAR:
  case CVAR:
    OZ_suspendOn(makeTaggedRef(chPtr));

  case OZCONST:
    if (!isChunk(ch)) TypeErrorT(0,"Chunk");
    //
    switch (tagged2Const(ch)->getType()) {
    case Co_Object:
      return
	OZ_unify(out, makeTaggedSmallInt (tagged2Object(ch)->getWidth ()));
    case Co_Chunk:
      return
	OZ_unify(out, makeTaggedSmallInt (tagged2SChunk(ch)->getWidth ()));
    default:
      // no features
      return OZ_unify(out,makeTaggedSmallInt (0));
    }

  default:
    TypeErrorT(0,"Chunk");
  }
}
OZ_C_proc_end

OZ_C_proc_begin(BIrecordWidth, 2)
{
  OZ_Term arg = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  DEREF(arg, argPtr, argTag);

  switch (argTag) {
  case CVAR:
    switch (tagged2CVar(arg)->getType ()) {
    case OFSVariable:
      {
	GenOFSVariable *ofsVar = tagged2GenOFSVar(arg);
	return (OZ_unify (out, makeTaggedSmallInt(ofsVar->getWidth ())));
      }

    default:
      TypeErrorT(0, "Record");
    }

  case SRECORD:
    return (OZ_unify (out, makeTaggedSmallInt(tagged2SRecord(arg)->getWidth ())));

  default:
    TypeErrorT(0, "Record");
  }
}
OZ_C_proc_end

/* ---------------------------------------------------------------------
 * Threads
 * --------------------------------------------------------------------- */

#define OZ_declareThreadArg(ARG,VAR)		\
 Thread *VAR;					\
 OZ_nonvarArg(ARG);				\
 if (! OZ_isThread(OZ_getCArg(ARG))) {		\
   return OZ_typeError(ARG,"Thread");		\
 } else {					\
   VAR = tagged2Thread(deref(OZ_getCArg(ARG)));	\
 }

OZ_C_proc_begin(BIthreadThis,1)
{
  OZ_declareArg(0,out);

  return OZ_unify(out, makeTaggedConst(am.currentThread));
}
OZ_C_proc_end

/*
 * change priority of a thread
 *  if my priority is lowered, then preempt me
 *  if priority of other thread become higher than mine, then preempt me
 */
OZ_C_proc_begin(BIthreadSetPriority,2)
{
  OZ_declareThreadArg(0,th);
  OZ_declareNonvarArg(1,atom_prio);
  int prio;

  atom_prio = deref(atom_prio);

  if (!isAtom(atom_prio)) 
    goto type_goof;
    
  if (literalEq(atom_prio, AtomLow)) {
    prio = LOW_PRIORITY;
  } else if (literalEq(atom_prio, AtomMedium)) {
    prio = MID_PRIORITY;
  } else if (literalEq(atom_prio, AtomHigh)) {
    prio = HI_PRIORITY;
  } else {
  type_goof:
    TypeErrorT(1,"Atom [low medium high]");
  }

  if (th->isDeadThread()) return PROCEED;

  int oldPrio = th->getPriority();
  th->setPriority(prio);

  if (am.currentThread == th) {
    if (prio <= oldPrio) {
      am.setSFlag(ThreadSwitch);
      return BI_PREEMPT;
    }
  } else {
    if (th->isRunnable()) {
      am.rescheduleThread(th);
    }
    if (prio > am.currentThread->getPriority()) {
      return BI_PREEMPT;
    }
  }

  return PROCEED;
}
OZ_C_proc_end 

OZ_C_proc_begin(BIthreadGetPriority,2)
{
  OZ_declareThreadArg(0,th);
  OZ_declareArg(1,out);
  TaggedRef prio;
  
  switch (th->getPriority()) {
  case LOW_PRIORITY: prio = AtomLow;    break;
  case MID_PRIORITY: prio = AtomMedium; break;
  case HI_PRIORITY:  prio = AtomHigh;   break;
  default: prio = AtomHigh; Assert(0);
  }
  return OZ_unify(prio,out);
}
OZ_C_proc_end 

OZ_C_proc_begin(BIthreadID,2)
{
  OZ_declareThreadArg(0,th);
  OZ_declareArg(1,out);
  
  return OZ_unifyInt(out, th->getID());
}
OZ_C_proc_end 

OZ_C_proc_begin(BIthreadIs,2)
{
  OZ_declareNonvarArg(0,th);
  OZ_declareArg(1,out);

  return OZ_unify(out,OZ_isThread(th)?NameTrue:NameFalse);
}
OZ_C_proc_end 

/*
 * raise exception on thread
 */
OZ_C_proc_proto(BIraise);

OZ_C_proc_begin(BIthreadRaise,2)
{
  OZ_declareThreadArg(0,th);
  OZ_declareNonvarArg(1,E);

  if (th->isDeadThread()) return PROCEED;

  if (am.currentThread == th) {
    return OZ_raise(E);
  }

  RefsArray args=allocateRefsArray(1, NO);
  args[0]=E;
  
  th->pushCFunCont (BIraise, args, 1, OK);
  
  if (th->isSuspended()) {
    th->suspThreadToRunnable();
    am.scheduleThread(th);
  }
  return PROCEED;
}
OZ_C_proc_end 

/*
 * terminate a thread
 *   in deep guard == failed
 */
OZ_C_proc_begin(BIthreadTerminate,1)
{
  OZ_declareThreadArg(0,th);

  if (th->isDeadThread()) return PROCEED;

  Bool isLocal=th->terminate();

  if (am.currentThread == th) {
    if (isLocal) return FAILED;
    return BI_TERMINATE;
  }

  if (isLocal) th->getBoard()->setFailed();
  if (!th->isRunnable()) {
    am.scheduleThread(th);
  }
  return PROCEED;
}
OZ_C_proc_end 

/*
 * suspend a thread
 *   is done lazy: when the thread becomes running the stop flag is tested
 */
OZ_C_proc_begin(BIthreadSuspend,1)
{
  OZ_declareThreadArg(0,th);
  
  th->stop();
  if (th == am.currentThread) {
    return BI_PREEMPT;
  }
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BIthreadResume,1)
{
  OZ_declareThreadArg(0,th);

  th->cont();

  if (th->isDeadThread()) return PROCEED;

  if (th->isRunnable() && !am.isScheduled(th)) {
    am.scheduleThread(th);
  }

  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BIthreadIsSuspended,2)
{
  OZ_declareThreadArg(0,th);
  OZ_declareArg(1,out);

  return OZ_unify(out,th->stopped()?NameTrue:NameFalse);
}
OZ_C_proc_end 

OZ_C_proc_begin(BIthreadState,2)
{
  OZ_declareThreadArg(0,th);
  OZ_declareArg(1,out);

  if (th->isDeadThread()) {
    return OZ_unifyAtom(out,"terminated");
  }
  if (th->isRunnable()) {
    /* terminated but not yet removed from scheduler */
    if (th!=am.currentThread && th->isEmpty() && th!=am.rootThread)
	return OZ_unifyAtom(out,"terminated");
    return OZ_unifyAtom(out,"runnable");
  }
  return OZ_unifyAtom(out,"blocked");
}
OZ_C_proc_end 

OZ_C_proc_begin(BIthreadGetName,2)
{
  OZ_declareThreadArg(0,th);
  OZ_declareArg(1,out);

  return OZ_unify(out, th->getName());
}
OZ_C_proc_end 

OZ_C_proc_begin(BIthreadSetName,2)
{
  OZ_declareThreadArg(0,th);
  OZ_declareArg(1,in);
  th->setName(in);
  return PROCEED;
}
OZ_C_proc_end

#ifdef LINKEDTHREADS
 OZ_C_proc_begin(BIthreadParent,2)
 {
   OZ_declareThreadArg(0,th);
   OZ_declareArg(1,out);
   return OZ_unify(out, th->getParent());
 }
 OZ_C_proc_end

 OZ_C_proc_begin(BIthreadChildren,2)
 {
   OZ_declareThreadArg(0,th);
   OZ_declareArg(1,out);
   return OZ_unify(out, th->getChildren());
 }
 OZ_C_proc_end
#endif

OZ_C_proc_begin(BIthreadPreempt,1)
{
  OZ_declareThreadArg(0,th);
  
  if (th == am.currentThread) {
    return BI_PREEMPT;
  }
  return PROCEED;
}
OZ_C_proc_end

// ---------------------------------------------------------------------
// NAMES
// ---------------------------------------------------------------------

OZ_C_proc_begin(BInewName,1)
{
  OZ_Term out = OZ_getCArg(0);
  return OZ_unify(out,OZ_newName());
}
OZ_C_proc_end 

// ---------------------------------------------------------------------
// term type
// ---------------------------------------------------------------------

OZ_Return BItermTypeInline(TaggedRef term, TaggedRef &out)
{
  out = OZ_termType(term);
  if (OZ_eq(out,OZ_atom("variable"))) {
    return SUSPEND;
  }
  return PROCEED;
}

DECLAREBI_USEINLINEFUN1(BItermType,BItermTypeInline)

OZ_Return BIstatusInline(TaggedRef term, TaggedRef &out) {
  DEREF(term, _1, tag);

  switch (tag) {
  case UVAR: 
  case SVAR: 
    out = AtomFree; break;
  case CVAR: {
    SRecord *t = SRecord::newSRecord(AtomKinded, 1);
    switch (tagged2CVar(term)->getType()) {
    case FDVariable:
    case BoolVariable:
      t->setArg(0, AtomInt); break;
    case OFSVariable:
      t->setArg(0, AtomRecord); break;
    default:
      t->setArg(0, AtomOther); break;
    }
    out = makeTaggedSRecord(t);
    break;
  }
  default: {
    SRecord *t = SRecord::newSRecord(AtomDet, 1);
    t->setArg(0, OZ_termType(term));
    out = makeTaggedSRecord(t);
  }
  }
  return PROCEED;
}

DECLAREBI_USEINLINEFUN1(BIstatus,BIstatusInline)


// ---------------------------------------------------------------------
// Builtins ==, \=, ==B and \=B
// ---------------------------------------------------------------------

inline OZ_Return eqeqWrapper(TaggedRef Ain, TaggedRef Bin)
{
  TaggedRef A = Ain, B = Bin;
  DEREF(A,aPtr,tagA); DEREF(B,bPtr,tagB);

  /* Really fast test for equality */
  if (tagA != tagB) {
    if (isAnyVar(A) || isAnyVar(B)) goto dontknow;
    return FAILED;
  }

  if (isSmallInt(tagA)) return smallIntEq(A,B) ? PROCEED : FAILED;
  if (isBigInt(tagA))   return bigIntEq(A,B)   ? PROCEED : FAILED;
  if (isFloat(tagA))    return floatEq(A,B)    ? PROCEED : FAILED;

  if (isLiteral(tagA))  return literalEq(A,B)  ? PROCEED : FAILED;

  if (A == B && !isAnyVar(A)) return PROCEED;
  
 dontknow:
  am.trail.pushMark();
  Bool ret = am.unify(Ain,Bin,NO);
  if (ret == NO) {
    am.reduceTrailOnFail();
    return FAILED;
  }

  if (am.trail.isEmptyChunk()) {
    am.trail.popMark();
    return PROCEED;
  }

  am.reduceTrailOnShallow(NULL);

  return SUSPEND;
}


OZ_C_proc_begin(BIeq,2)
{
  TaggedRef A = OZ_getCArg(0);
  TaggedRef B = OZ_getCArg(1);
  return eqeqWrapper(A,B);
}
OZ_C_proc_end


OZ_C_proc_begin(BIneq,2)
{
  TaggedRef A = OZ_getCArg(0);
  TaggedRef B = OZ_getCArg(1);
  return eqeqWrapper(A,B);
}
OZ_C_proc_end



OZ_Return neqInline(TaggedRef A, TaggedRef B, TaggedRef &out);
OZ_Return eqeqInline(TaggedRef A, TaggedRef B, TaggedRef &out);


OZ_C_proc_begin(BIneqB,3)
{
  OZ_Term help;
  OZ_Return ret=neqInline(OZ_getCArg(0),OZ_getCArg(1),help);
  return ret==PROCEED ? OZ_unify(help,OZ_getCArg(2)) : ret;
}
OZ_C_proc_end

OZ_C_proc_begin(BIeqB,3)
{
  OZ_Term help;
  OZ_Return ret=eqeqInline(OZ_getCArg(0),OZ_getCArg(1),help);
  return ret==PROCEED ? OZ_unify(help,OZ_getCArg(2)): ret;
}
OZ_C_proc_end


OZ_Return eqeqInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  switch(eqeqWrapper(A,B)) {
  case PROCEED:
    out = NameTrue;
    return PROCEED;
  case FAILED:
    out = NameFalse;
    return PROCEED;
  case SUSPEND:
  default:
    return SUSPEND;
  }
}


OZ_Return neqInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  switch(eqeqWrapper(A,B)) {
  case PROCEED:
    out = NameFalse;
    return PROCEED;
  case FAILED:
    out = NameTrue;
    return PROCEED;
  case SUSPEND:
  default:
    return SUSPEND;
  }
}

// ---------------------------------------------------------------------
// String
// ---------------------------------------------------------------------

OZ_C_proc_begin(BIisString,2)
{
  OZ_Term in=OZ_getCArg(0);
  OZ_Term out=OZ_getCArg(1);

  OZ_Term var;
  if (!OZ_isString(in,&var)) {
    if (var == 0) return OZ_unify(out,NameFalse);
    OZ_suspendOn(var);
  }
  return OZ_unify(out,NameTrue);
}
OZ_C_proc_end

#endif /* BUILTINS2 */

#ifdef BUILTINS1


// ---------------------------------------------------------------------
// Char
// ---------------------------------------------------------------------

#define FirstCharArg \
 TaggedRef tc = OZ_getCArg(0);      \
 int i;				    \
 { DEREF(tc, tc_ptr, tc_tag);       \
 if (isAnyVar(tc_tag)) {            \
   am.addSuspendVarList(tc_ptr);    \
   return SUSPEND;                  \
 }                                  \
 if (!isSmallInt(tc)) {             \
   return OZ_typeError(1,"Char");   \
 } else {			    \
   i = smallIntValue(tc);	    \
   if ((i < 0) || (i > 255)) {	    \
     return OZ_typeError(1,"Char"); \
   }				    \
 } }

#define TestChar(TEST)                                            \
  FirstCharArg;                                                   \
  return OZ_unify(OZ_getCArg(1), TEST (i) ? NameTrue : NameFalse);

OZ_C_proc_begin(BIcharIs,2) {
 OZ_declareNonvarArg(0,c);
 OZ_declareArg(1,out);
 c = deref(c);
 if (!isSmallInt(c)) return OZ_unify(out,NameFalse);
 int i = smallIntValue(c);
 return OZ_unify(out,(i >=0 && i <= 255) ? NameTrue : NameFalse);
} OZ_C_proc_end

OZ_C_proc_begin(BIcharIsAlNum,2) { TestChar(isalnum); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsAlpha,2) { TestChar(isalpha); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsCntrl,2) { TestChar(iscntrl); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsDigit,2) { TestChar(isdigit); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsGraph,2) { TestChar(isgraph); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsLower,2) { TestChar(islower); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsPrint,2) { TestChar(isprint); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsPunct,2) { TestChar(ispunct); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsSpace,2) { TestChar(isspace); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsUpper,2) { TestChar(isupper); } OZ_C_proc_end

OZ_C_proc_begin(BIcharIsXDigit,2) {TestChar(isxdigit);} OZ_C_proc_end

OZ_C_proc_begin(BIcharToLower,2) {
  FirstCharArg;
  return OZ_unifyInt(OZ_getCArg(1), tolower(i));
} OZ_C_proc_end

OZ_C_proc_begin(BIcharToUpper,2) {
  FirstCharArg;
  return OZ_unifyInt(OZ_getCArg(1), toupper(i));
} OZ_C_proc_end

OZ_C_proc_begin(BIcharToAtom,2) {
  FirstCharArg;
  if (i) {
     char s[2]; s[0]= (char) i; s[1]='\0';
     return OZ_unify(OZ_getCArg(1), makeTaggedAtom(s));
  }
  return OZ_unify(OZ_getCArg(1), AtomEmpty);
} OZ_C_proc_end

OZ_C_proc_begin(BIcharType,2) {
  FirstCharArg;
  TaggedRef type;
  if (isupper(i))      type = AtomUpper; 
  else if (islower(i)) type = AtomLower;
  else if (isdigit(i)) type = AtomDigit;
  else if (isspace(i)) type = AtomCharSpace;
  else if (ispunct(i)) type = AtomPunct;
  else                 type = AtomOther;
  return OZ_unify(OZ_getCArg(1), type);
} OZ_C_proc_end


/********************************************************************
 * Records
 ******************************************************************** */

OZ_Return BIadjoinInline(TaggedRef t0, TaggedRef t1, TaggedRef &out)
{
  DEREF(t0,_0,tag0);
  DEREF(t1,_1,tag1);

  switch (tag0) {
  case LITERAL:
    switch (tag1) {
    case SRECORD:
    case LITERAL:
      out = t1;
      return PROCEED;
    case UVAR:
    case SVAR:
      return SUSPEND;
    case CVAR:
      switch (tagged2CVar(t1)->getType()) {
      case FDVariable:
      case BoolVariable:
          TypeErrorT(1,"Record");
      default:
          return SUSPEND; 
      }
    default:
      TypeErrorT(1,"Record");
    }
  case LTUPLE:
  case SRECORD:
    {
      SRecord *rec= makeRecord(t0);
      switch (tag1) {
      case LITERAL:
	{
	  SRecord *newrec = rec->replaceLabel(t1);
	  out = newrec->normalize();
	  return PROCEED;
	}
      case SRECORD:
      case LTUPLE:
	{
	  out = rec->adjoin(makeRecord(t1));
	  return PROCEED;
	}
      case UVAR:
      case SVAR:
	return SUSPEND;
      case CVAR:
        switch (tagged2CVar(t1)->getType()) {
        case FDVariable:
        case BoolVariable:
            TypeErrorT(1,"Record");
        default:
            return SUSPEND;
        }
      default:
	TypeErrorT(1,"Record");
      }
    }
  case UVAR:
  case SVAR:
  case CVAR:
    if (tag0==CVAR) {
        switch (tagged2CVar(t0)->getType()) {
        case FDVariable:
        case BoolVariable:
	  TypeErrorT(0,"Record");
	default:
	  break;
        }
    }
    switch (tag1) {
    case UVAR:
    case SVAR:
    case SRECORD:
    case LTUPLE:
    case LITERAL:
      return SUSPEND;
    case CVAR:
      switch (tagged2CVar(t1)->getType()) {
      case FDVariable:
      case BoolVariable:
          TypeErrorT(1,"Record");
      default:
	return SUSPEND;
      }
    default:
      TypeErrorT(1,"Record");
    }
  default:
    TypeErrorT(0,"Record");
  }
} 

DECLAREBI_USEINLINEFUN2(BIadjoin,BIadjoinInline)

OZ_C_proc_begin(BIadjoinAt,4)
{
  OZ_Term rec = OZ_getCArg(0);
  OZ_Term fea = OZ_getCArg(1);
  OZ_Term value = OZ_getCArg(2);
  OZ_Term out = OZ_getCArg(3);

  DEREF(rec,recPtr,tag0);
  DEREF(fea,feaPtr,tag1);
  
  switch (tag0) {
  case LITERAL:
    if (isFeature(fea)) {
      SRecord *newrec = SRecord::newSRecord(rec,aritytable.find(cons(fea,nil())));
      newrec->setArg(0,value);
      return OZ_unify(out, makeTaggedSRecord(newrec));
    }
    if (isNotCVar(fea)) {
      OZ_suspendOn(makeTaggedRef(feaPtr));
    }
    if (isCVar(fea)) {
      if (tagged2CVar(fea)->getType()!=OFSVariable ||
          tagged2GenOFSVar(fea)->getWidth()>0)
	TypeErrorT(1,"Feature");
      OZ_suspendOn(makeTaggedRef(feaPtr));;
    }
    TypeErrorT(1,"Feature");

  case LTUPLE:
  case SRECORD:
    {
      SRecord *rec1 = makeRecord(rec);
      if (isAnyVar(tag1)) {
	OZ_suspendOn(makeTaggedRef(feaPtr));
      }
      if (!isFeature(tag1)) {
	TypeErrorT(1,"Feature");
      }
      return OZ_unify(out,rec1->adjoinAt(fea,value));
    }

  case UVAR:
  case SVAR:
  case CVAR:
    if (tag0==CVAR && tagged2CVar(rec)->getType()!=OFSVariable)
        TypeErrorT(0,"Record");
    if (isFeature(fea) || isNotCVar(fea)) {
      OZ_suspendOn(makeTaggedRef(recPtr));
    }
    if (isCVar(fea)) {
      if (tagged2CVar(fea)->getType()!=OFSVariable ||
          tagged2GenOFSVar(fea)->getWidth()>0)
	TypeErrorT(1,"Feature");
      OZ_suspendOn(makeTaggedRef(recPtr));
    }
    TypeErrorT(1,"Feature");

  default:
    TypeErrorT(0,"Record");
  }
}
OZ_C_proc_end

TaggedRef getArity(TaggedRef list)
{
  TaggedRef arity;
  TaggedRef *next=&arity;
loop:
  DEREF(list,listPtr,_l1);
  if (isLTuple(list)) {
    TaggedRef pair = head(list);
    DEREF(pair,pairPtr,_e1);
    if (isAnyVar(pair)) return makeTaggedRef(pairPtr);
    if (!OZ_isPair2(pair)) goto bomb;

    TaggedRef fea = tagged2SRecord(pair)->getArg(0);
    DEREF(fea,feaPtr,_f1);

    if (isAnyVar(fea)) return makeTaggedRef(feaPtr);
    if (!isFeature(fea)) goto bomb;

    LTuple *lt=new LTuple();
    *next=makeTaggedLTuple(lt);
    lt->setHead(fea);
    next=lt->getRefTail();
    
    list = tail(list);
    goto loop;
  }

  if (isAnyVar(list)) return makeTaggedRef(listPtr);
  if (isNil(list)) {
    *next=nil();
    return arity;
  }

bomb:
  return makeTaggedNULL(); // FAIL
}

/* common subroutine for builtins adjoinList and record:
   recordFlag=OK: 'adjoinList' allows records as first arg
              N0: 'makeRecord' only allows literals */

OZ_Return adjoinPropListInline(TaggedRef t0, TaggedRef list, TaggedRef &out,
			   Bool recordFlag)
{
  TaggedRef arity=getArity(list);
  if (arity == makeTaggedNULL()) {
    TypeErrorT(1,"list(Feature#Value)");
  }
  DEREF(t0,t0Ptr,tag0);
  if (isRef(arity)) { // must suspend
    out=arity;
    switch (tag0) {
    case UVAR:
    case SVAR:
    case LITERAL:
      return SUSPEND;
    case SRECORD:
    case LTUPLE:
      if (recordFlag) {
	return SUSPEND;
      }
      goto typeError0;
    case CVAR:
      if (tagged2CVar(t0)->getType()!=OFSVariable)
          goto typeError0;
      if (recordFlag) {
	return SUSPEND;
      }
      goto typeError0;
    default:
      goto typeError0;
    }
  }
  
  if (isNil(arity)) { // adjoin nothing
    switch (tag0) {
    case SRECORD:
    case LTUPLE:
      if (recordFlag) {
	out = t0;
	return PROCEED;
      }
      goto typeError0;
    case LITERAL:
      out = t0;
      return PROCEED;
    case UVAR:
    case SVAR:
      out=makeTaggedRef(t0Ptr);
      return SUSPEND;
    case CVAR:
      if (tagged2CVar(t0)->getType()!=OFSVariable)
          goto typeError0;
      out=makeTaggedRef(t0Ptr);
      return SUSPEND;
    default:
      goto typeError0;
    }
  }

  switch (tag0) {
  case LITERAL:
    {
      int len=length(arity);
      arity = sortlist(arity,len);
      len=length(arity); // NOTE: duplicates may be removed
      SRecord *newrec = SRecord::newSRecord(t0,aritytable.find(arity));
      newrec->setFeatures(list);
      out = newrec->normalize();
      return PROCEED;
    }
  case SRECORD:
  case LTUPLE:
    if (recordFlag) {
      SRecord *rec1 = makeRecord(t0);
      out = rec1->adjoinList(arity,list);
      return PROCEED;
    }
    goto typeError0;
  case UVAR:
  case SVAR:
    out=makeTaggedRef(t0Ptr);
    return SUSPEND;
  case CVAR:
    if (tagged2CVar(t0)->getType()!=OFSVariable)
        goto typeError0;
    out=makeTaggedRef(t0Ptr);
    return SUSPEND;
  default:
    goto typeError0;
  }

 typeError0:
  if (recordFlag) {
    TypeErrorT(0,"Record");
  } else {
    TypeErrorT(0,"Literal");
  }
}

OZ_Return adjoinPropList(TaggedRef t0, TaggedRef list, TaggedRef &out,
		     Bool recordFlag) 
{
  return adjoinPropListInline(t0,list,out,recordFlag);
}


OZ_C_proc_begin(BIadjoinList,3)
{
  OZ_Term help;

  OZ_Return state = adjoinPropListInline(OZ_getCArg(0),OZ_getCArg(1),help,OK);
  switch (state) {
  case SUSPEND:
    OZ_suspendOn(help);
  case PROCEED:
    return(OZ_unify(help,OZ_getCArg(2)));
  default:
    return state;
  }
}
OZ_C_proc_end


OZ_C_proc_begin(BImakeRecord,3)
{
  OZ_Term help;

  OZ_Return state = adjoinPropListInline(OZ_getCArg(0),OZ_getCArg(1),help,NO);
  switch (state) {
  case SUSPEND:
    OZ_suspendOn(help);
    return PROCEED;
  case PROCEED:
    return(OZ_unify(help,OZ_getCArg(2)));
  default:
    return state;
  }
}
OZ_C_proc_end


OZ_Return BIarityInline(TaggedRef term, TaggedRef &out)
{
  DEREF(term,termPtr,tag);

  out = getArityList(term);
  if (out) return PROCEED;
  if (isNotCVar(tag)) return SUSPEND;
  if (isCVar(tag)) {
    if (tagged2CVar(term)->getType()!=OFSVariable)
      TypeErrorT(0,"Record");
    return SUSPEND;
  }
  TypeErrorT(0,"Record");
}

DECLAREBI_USEINLINEFUN1(BIarity,BIarityInline)


#endif /* BUILTINS1 */


/* -----------------------------------------------------------------------
   Numbers
   ----------------------------------------------------------------------- */

#ifdef BUILTINS1

static OZ_Return bombArith(char *type)
{
  TypeErrorT(-1,type);
}

#define suspendTest(A,B,test,type)			\
  if (isAnyVar(A)) {					\
    if (isAnyVar(B) || test(B)) { return SUSPEND; }	\
    return bombArith(type);				\
  } 							\
  if (isAnyVar(B)) {					\
    if (isNumber(A)) { return SUSPEND; }		\
  }							\
  return bombArith(type);


static OZ_Return suspendOnNumbers(TaggedRef A, TaggedRef B) 
{
  suspendTest(A,B,isNumber,"Number");
}

inline Bool isNumOrAtom(TaggedRef t)
{
  return isNumber(t) || isAtom(t);
}

static OZ_Return suspendOnNumbersAndAtoms(TaggedRef A, TaggedRef B) 
{
  suspendTest(A,B,isNumOrAtom,"Number or Atom");
}

static OZ_Return suspendOnFloats(TaggedRef A, TaggedRef B) 
{
  suspendTest(A,B,isFloat,"Float");
}


static OZ_Return suspendOnInts(TaggedRef A, TaggedRef B) 
{
  suspendTest(A,B,isInt,"Int");
}

#undef suspendTest





/* -----------------------------------
   Z = X op Y
   ----------------------------------- */

// Float x Float -> Float
OZ_Return BIfdivInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);
  if (isFloat(tagA) && isFloat(tagB)) {
    out = makeTaggedFloat(floatValue(A) / floatValue(B));
    return PROCEED;
  }
  return suspendOnFloats(A,B);
}


// Int x Int -> Int
#define BIGOP(op)							      \
  if (tagA == BIGINT) {							      \
    if (tagB == BIGINT) {						      \
      out = tagged2BigInt(A)->op(tagged2BigInt(B));			      \
      return PROCEED;							      \
    }									      \
    if (tagB == SMALLINT) {						      \
      BigInt *b = new BigInt(smallIntValue(B));				      \
      out = tagged2BigInt(A)->op(b);					      \
      b->dispose();							      \
      return PROCEED;							      \
    }									      \
  }									      \
  if (tagB == BIGINT) {							      \
    if (tagA == SMALLINT) {						      \
      BigInt *a = new BigInt(smallIntValue(A));				      \
      out = a->op(tagged2BigInt(B));					      \
      a->dispose();							      \
      return PROCEED;							      \
    }									      \
  }

// Integer x Integer -> Integer
OZ_Return BIdivInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if (tagB == SMALLINT && smallIntValue(B) == 0) {
    if (tagA == SMALLINT || tagA == BIGINT) {
      return am.raise(E_ERROR,E_KERNEL,"div0",1,A);
    } else {
      return bombArith("Int");
    }
  }

  if ( (tagA == SMALLINT) && (tagB == SMALLINT)) {
    out = newSmallInt(smallIntValue(A) / smallIntValue(B));
    return PROCEED;
  }
  BIGOP(div);
  return suspendOnInts(A,B);
}

// Integer x Integer -> Integer
OZ_Return BImodInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if ((tagB == SMALLINT && smallIntValue(B) == 0)) {
    if (tagA == SMALLINT || tagA == BIGINT) {
      return am.raise(E_ERROR,E_KERNEL,"mod0",1,A);
    } else {
      return bombArith("Int");
    }
  }
  
  if ( (tagA == SMALLINT) && (tagB == SMALLINT)) {
    out = newSmallInt(smallIntValue(A) % smallIntValue(B));
    return PROCEED;
  }

  BIGOP(mod);
  return suspendOnInts(A,B);
}


/* Division is slow on RISC (at least SPARC)
 *  --> first make a simpler test for no overflow
 */

inline
int multOverflow(int a, int b)
{
  int absa = ozabs(a);
  int absb = ozabs(b);
  const int bits = (sizeof(TaggedRef)*8-tagSize)/2 - 1;

  if (!((absa|absb)>>bits)) /* if none of the 13 MSB in neither a nor b are set */
    return NO;
  return ((b!=0) && (absa >= OzMaxInt / absb));
}


OZ_Return BImultInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if (tagA == SMALLINT && tagB == SMALLINT) {
    int valA = smallIntValue(A);
    int valB = smallIntValue(B);
    if ( multOverflow(valA,valB) ) {
      BigInt *a = new BigInt(valA);
      BigInt *b = new BigInt(valB);
      out = a->mul(b);
      a->dispose();
      b->dispose();
      return PROCEED;
    } else {
      out = newSmallInt(valA*valB);
      return PROCEED;
    }
  }
  
  if (isFloat(tagA) && isFloat(tagB)) {
    out = makeTaggedFloat(floatValue(A) * floatValue(B));
    return PROCEED;
  }
  
  BIGOP(mul);
  return suspendOnNumbers(A,B);
}


OZ_Return BIminusInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if ( (tagA == SMALLINT) && (tagB == SMALLINT) ) {
    out = makeInt(smallIntValue(A) - smallIntValue(B));
    return PROCEED;
  } 

  if (isFloat(tagA) && isFloat(tagB)) {
    out = makeTaggedFloat(floatValue(A) - floatValue(B));
    return PROCEED;
  }

  BIGOP(sub);
  return suspendOnNumbers(A,B);
}

OZ_Return BIplusInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if ( (tagA == SMALLINT) && (tagB == SMALLINT) ) {
    out = makeInt(smallIntValue(A) + smallIntValue(B));
    return PROCEED;
  } 

  if (isFloat(tagA) && isFloat(tagB)) {
    out = makeTaggedFloat(floatValue(A) + floatValue(B));
    return PROCEED;
  }

  BIGOP(add);
  return suspendOnNumbers(A,B);
}
#undef BIGOP

/* -----------------------------------
   Z = op X
   ----------------------------------- */

// unary minus: Number -> Number
OZ_Return BIuminusInline(TaggedRef A, TaggedRef &out)
{
  DEREF(A,_1,tagA);

  switch(tagA) {

  case OZFLOAT:
    out = makeTaggedFloat(-floatValue(A));
    return PROCEED;

  case SMALLINT:
    out = newSmallInt(-smallIntValue(A));
    return PROCEED;

  case BIGINT:
    out = tagged2BigInt(A)->neg();
    return PROCEED;

  case UVAR:
  case SVAR:
    return SUSPEND;

  default:
    TypeErrorT(0,"Int");
  }
}

OZ_Return BIabsInline(TaggedRef A, TaggedRef &out)
{
  DEREF(A,_1,tagA);

  if (isSmallInt(tagA)) {
    int i = smallIntValue(A);
    out = (i >= 0) ? A : newSmallInt(-i);
    return PROCEED;
  }

  if (isFloat(tagA)) {
    double f = floatValue(A);
    out = (f >= 0.0) ? A : makeTaggedFloat(fabs(f));
    return PROCEED;
  }

  if (isBigInt(tagA)) {
    BigInt *b = tagged2BigInt(A);
    out = (b->cmp(0l) >= 0) ? A : b->neg();
    return PROCEED;
  }

  if (isAnyVar(tagA)){
    return SUSPEND;
  }

  TypeErrorT(0,"Number");
}

// add1(X) --> X+1
OZ_Return BIadd1Inline(TaggedRef A, TaggedRef &out)
{
  DEREF(A,_1,tagA);

  if (isSmallInt(tagA)) {
    /* INTDEP */
    int res = (int)A + (1<<tagSize);
    out = (int)A >= res ? makeInt(smallIntValue(A)+1) : res;
    return PROCEED;
  }

  if (isBigInt(tagA)) {
    return BIplusInline(A,newSmallInt(1),out);
  }
  
  if (isAnyVar(tagA)) {
    return SUSPEND;
  }

  TypeErrorT(0,"Int");
}

// sub1(X) --> X-1
OZ_Return BIsub1Inline(TaggedRef A, TaggedRef &out)
{
  DEREF(A,_1,tagA);

  if (isSmallInt(tagA)) {
    /* INTDEP */
    int res = (int)A - (1<<tagSize);
    out = (int)A <= res ? makeInt(smallIntValue(A)-1) : res;
    return PROCEED;
  }

  if (isBigInt(tagA)) {
    return BIminusInline(A,newSmallInt(1),out);
  }

  if (isAnyVar(tagA)) {
    return SUSPEND;
  }

  TypeErrorT(0,"Int");
}


/* -----------------------------------
   X test Y
   ----------------------------------- */

OZ_Return bigintLess(BigInt *A, BigInt *B)
{
  return (A->cmp(B) < 0 ? PROCEED : FAILED);
}


OZ_Return bigintLe(BigInt *A, BigInt *B)
{
  return (A->cmp(B) <= 0 ? PROCEED : FAILED);
}


OZ_Return bigtest(TaggedRef A, TaggedRef B,
		  OZ_Return (*test)(BigInt*, BigInt*))
{
  if (isBigInt(A)) {
    if (isBigInt(B)) {
      return test(tagged2BigInt(A),tagged2BigInt(B));
    }
    if (isSmallInt(B)) {
      BigInt *b = new BigInt(smallIntValue(B));
      OZ_Return res = test(tagged2BigInt(A),b);
      b->dispose();
      return res;
    }
  }
  if (isBigInt(B)) {
    if (isSmallInt(A)) {
      BigInt *a = new BigInt(smallIntValue(A));
      OZ_Return res = test(a,tagged2BigInt(B));
      a->dispose();
      return res;
    }
  }
  if (isAnyVar(A) || isAnyVar(B)) 
    return SUSPEND;

  TypeErrorT(-1,"Float x Float or Int x Int or Atom x Atom");
}




OZ_Return BIminInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA); 
  DEREF(B,_2,tagB);

  if (tagA == tagB) {
    switch(tagA) {
    case SMALLINT: out = (smallIntLess(A,B) ? A : B);             return PROCEED;
    case OZFLOAT:  out = (floatValue(A) < floatValue(B)) ? A : B; return PROCEED;
    case LITERAL:
      if (isAtom(A) && isAtom(B)) {
	out = (strcmp(tagged2Literal(A)->getPrintName(),
		      tagged2Literal(B)->getPrintName()) < 0)
	  ? A : B;
	return PROCEED;
      }
      TypeErrorT(-1,"Comparable");
      
    default: break;
    }
  }

  OZ_Return ret = bigtest(A,B,bigintLess);
  switch (ret) {
  case PROCEED: out = A; return PROCEED;
  case FAILED:  out = B; return PROCEED;
  case RAISE:   return RAISE;
  default:      break;
  }

  return suspendOnNumbersAndAtoms(A,B);
}


/* code adapted from min */
OZ_Return BImaxInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA); 
  DEREF(B,_2,tagB);

  if (tagA == tagB) {
    switch(tagA) {
    case SMALLINT: out = (smallIntLess(A,B) ? B : A);             return PROCEED;
    case OZFLOAT:  out = (floatValue(A) < floatValue(B)) ? B : A; return PROCEED;
    case LITERAL:
      if (isAtom(A) && isAtom(B)) {
	out = (strcmp(tagged2Literal(A)->getPrintName(),
		      tagged2Literal(B)->getPrintName()) < 0)
	  ? B : A;
	return PROCEED;
      }
      TypeErrorT(-1,"Comparable");

    default: break;
    }
  }

  OZ_Return ret = bigtest(A,B,bigintLess);
  switch (ret) {
  case PROCEED: out = B; return PROCEED;
  case FAILED:  out = A; return PROCEED;
  case RAISE:   return RAISE;
  default:      break;
  }

  return suspendOnNumbersAndAtoms(A,B);
}


OZ_Return BIlessInline(TaggedRef A, TaggedRef B)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if (tagA == tagB) {
    if (tagA == SMALLINT) {
      return (smallIntLess(A,B) ? PROCEED : FAILED);
    }

    if (isFloat(tagA)) {
      return (floatValue(A) < floatValue(B)) ? PROCEED : FAILED;
    }

    if (tagA == LITERAL) {
      if (isAtom(A) && isAtom(B)) {
        return (strcmp(tagged2Literal(A)->getPrintName(),
                       tagged2Literal(B)->getPrintName()) < 0)
          ? PROCEED : FAILED;
      }
      TypeErrorT(-1,"Comparable");
    }
  }

  OZ_Return ret = bigtest(A,B,bigintLess); 
  if (ret!=SUSPEND) 
    return ret;

  return suspendOnNumbersAndAtoms(A,B);
}



OZ_Return BInumeqInline(TaggedRef A, TaggedRef B)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if (tagA == tagB) {
    if (isSmallInt(tagA)) if (smallIntEq(A,B)) goto proceed; goto failed;
    if (isFloat(tagA))    if (floatEq(A,B))    goto proceed; goto failed;
    if (isBigInt(tagA))   if (bigIntEq(A,B))   goto proceed; goto failed;
  }

  return suspendOnNumbers(A,B);

 failed:
  return FAILED;
  
 proceed:
  return PROCEED;
}


OZ_Return BInumeqInlineFun(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  switch (BInumeqInline(A,B)) {
  case PROCEED:
    out = NameTrue;
    return PROCEED;
  case FAILED:
    out = NameFalse;
    return PROCEED;
  case SUSPEND:
  default:
    return SUSPEND;
  }
}

OZ_Return BInumneqInline(TaggedRef A, TaggedRef B)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if (tagA == tagB) {
    if (isSmallInt(tagA)) if(smallIntEq(A,B)) goto failed; goto proceed;
    if (isFloat(tagA))    if(floatEq(A,B))    goto failed; goto proceed;
    if (isBigInt(tagA))   if(bigIntEq(A,B))   goto failed; goto proceed;
  }

  return suspendOnNumbers(A,B);

 failed:
  return FAILED;
  
 proceed:
  return PROCEED;
}


OZ_Return BInumneqInlineFun(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  OZ_Return ret = BInumneqInline(A,B);
  switch (ret) {
  case PROCEED: out = NameTrue;  return PROCEED;
  case FAILED:  out = NameFalse; return PROCEED;
  default:      return ret;
  }
}

OZ_Return BIlessInlineFun(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  OZ_Return ret = BIlessInline(A,B);
  switch (ret) {
  case PROCEED: out = NameTrue;  return PROCEED;
  case FAILED:  out = NameFalse; return PROCEED;
  default:      return ret;
  }
}

OZ_Return BIgreatInline(TaggedRef A, TaggedRef B)
{
  return BIlessInline(B,A);
}

OZ_Return BIgreatInlineFun(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  return BIlessInlineFun(B,A,out);
}


OZ_Return BIleInline(TaggedRef A, TaggedRef B)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if (tagA == tagB) {

    if (tagA == SMALLINT) {
      return (smallIntLE(A,B) ? PROCEED : FAILED);
    }

    if (isFloat(tagA)) {
      return (floatValue(A) <= floatValue(B)) ? PROCEED : FAILED;
    }

    if (tagA == LITERAL) {
      if (isAtom(A) && isAtom(B)) {
	return (strcmp(tagged2Literal(A)->getPrintName(),
		       tagged2Literal(B)->getPrintName()) <= 0)
	  ? PROCEED : FAILED;
      }
      TypeErrorT(-1,"Comparable");
    }

  }

  OZ_Return ret = bigtest(A,B,bigintLe); 
  if (ret!=SUSPEND) 
    return ret;

  return suspendOnNumbersAndAtoms(A,B);
}


OZ_Return BIleInlineFun(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  OZ_Return ret = BIleInline(A,B);
  switch (ret) {
  case PROCEED: out = NameTrue;  return PROCEED;
  case FAILED:  out = NameFalse; return PROCEED;
  default:      return ret;
  }
}



OZ_Return BIgeInlineFun(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  return BIleInlineFun(B,A,out);
}


OZ_Return BIgeInline(TaggedRef A, TaggedRef B)
{
  return BIleInline(B,A);
}

/* -----------------------------------
   X = conv(Y)
   ----------------------------------- */


OZ_Return BIintToFloatInline(TaggedRef A, TaggedRef &out)
{
  DEREF(A,_1,_2);
  if (isSmallInt(A)) {
    out = makeTaggedFloat((double)smallIntValue(A));
    return PROCEED;
  }
  if (isBigInt(A)) {
    char *s = toC(A);
    out = OZ_CStringToFloat(s);
    return PROCEED;
  }

  if (OZ_isVariable(A)) {
    return SUSPEND;
  }

  TypeErrorT(0,"Int");
}

/* mm2: I don't know if this is efficient, but it's only called,
   when rounding "xx.5" */
inline
Bool ozisodd(double ff)
{
  double m = ff/2;
  return m != floor(m);
}

/* ozround -- round float to int as required by IEEE Standard */
inline
double ozround(double in) {
  double ff = floor(in);
  double diff = in-ff;
  if (diff > 0.5 || (diff == 0.5 && ozisodd(ff))) {
    ff += 1;
  }
  return ff;
}

OZ_Return BIfloatToIntInline(TaggedRef A, TaggedRef &out) {
  A=deref(A);

  if (isAnyVar(A))
    return SUSPEND;

  if (isFloat(A)) {
    double ff = ozround(floatValue(A));
    if (ff > INT_MAX || ff < INT_MIN) {
      OZ_warning("float to int: truncated to signed 32 Bit\n");
    }
    out = makeInt((int) ff);
    return PROCEED;
  }

  TypeErrorT(-1,"Float");
}

OZ_C_proc_begin(BIfloatToString, 2)
{
  OZ_nonvarArg(0);

  OZ_Term in = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  if (OZ_isFloat(in)) {
    char *s = toC(in);
    OZ_Return ret = OZ_unify(out,OZ_string(s));
    return ret;
  }
  TypeErrorT(0,"Float");
}
OZ_C_proc_end

OZ_C_proc_begin(BIstringToFloat, 2)
{
  OZ_declareProperStringArg(0,str);
  OZ_declareArg(1,out);

  char *end = OZ_parseFloat(str);
  if (!end || *end != 0) {
    return am.raise(E_ERROR,E_KERNEL,"stringNoFloat",1,OZ_getCArg(0));
  }
  OZ_Return ret = OZ_unify(out,OZ_CStringToFloat(str));
  return ret;
}
OZ_C_proc_end

OZ_C_proc_begin(BIstringIsFloat, 2)
{
  OZ_declareProperStringArg(0,str);
  OZ_declareArg(1,out);

  if (!str) return OZ_unify(out,NameFalse);

  char *end = OZ_parseFloat(str);

  if (!end || *end != 0) {
    return OZ_unify(out,NameFalse);
  }

  return OZ_unify(out,NameTrue);
}
OZ_C_proc_end

OZ_C_proc_begin(BIstringIsAtom, 2) {
  OZ_Term in = OZ_getCArg(0);
  OZ_Term out= OZ_getCArg(1);

  OZ_Term var;
  if (!OZ_isProperString(in,&var)) {
    if (var == 0) return OZ_unify(out,NameFalse);
    OZ_suspendOn(var);
  }
  return OZ_unify(out,NameTrue);
}
OZ_C_proc_end

OZ_C_proc_begin(BIstringToInt, 2)
{
  OZ_declareProperStringArg(0,str);
  OZ_declareArg(1,out);

  if (!str) return am.raise(E_ERROR,E_KERNEL,"stringNoInt",1,OZ_getCArg(0));


  char *end = OZ_parseInt(str);
  if (!end || *end != 0) {
    return am.raise(E_ERROR,E_KERNEL,"stringNoInt",1,OZ_getCArg(0));
  }
  OZ_Return ret = OZ_unify(out,OZ_CStringToInt(str));
  return ret;
}
OZ_C_proc_end

OZ_C_proc_begin(BIstringIsInt, 2)
{
  OZ_declareProperStringArg(0,str);
  OZ_declareArg(1,out);

  if (!str) return OZ_unify(out,NameFalse);

  char *end = OZ_parseInt(str);

  if (!end || *end != 0) {
    return OZ_unify(out,NameFalse);
  }

  return OZ_unify(out,NameTrue);
}
OZ_C_proc_end

OZ_C_proc_begin(BIintToString, 2)
{
  OZ_nonvarArg(0);

  OZ_Term in = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  if (OZ_isInt(in)) {
    return OZ_unify(out,OZ_string(toC(in)));
  }
  TypeErrorT(0,"Int");
}
OZ_C_proc_end
#endif

/* -----------------------------------
   type X
   ----------------------------------- */
#ifdef BUILTINS2

OZ_Return BIisFloatInline(TaggedRef num)
{
  DEREF(num,_,tag);

  if (isAnyVar(tag)) {
    return SUSPEND;
  }

  return isFloat(tag) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisFloat,BIisFloatInline)
DECLAREBOOLFUN1(BIisFloatB,BIisFloatBInline,BIisFloatInline)

OZ_Return BIisIntInline(TaggedRef num)
{
  DEREF(num,_,tag);

  if (isAnyVar(tag)) {
    return SUSPEND;
  }

  return isInt(tag) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisInt,BIisIntInline)
DECLAREBOOLFUN1(BIisIntB,BIisIntBInline,BIisIntInline)



OZ_Return BIisNumberInline(TaggedRef num)
{
  DEREF(num,_,tag);

  if (isAnyVar(tag)) {
    return SUSPEND;
  }

  return isNumber(tag) ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisNumber,BIisNumberInline)
DECLAREBOOLFUN1(BIisNumberB,BIisNumberBInline,BIisNumberInline)


#endif


#ifdef BUILTINS1

/* -----------------------------------------------------------------------
   misc. floating point functions
   ----------------------------------------------------------------------- */


#define FLOATFUN(Fun,BIName,InlineName)			\
OZ_Return InlineName(TaggedRef AA, TaggedRef &out)	\
{							\
  DEREF(AA,_,tag);					\
							\
  if (isAnyVar(tag)) {					\
    return SUSPEND;					\
  }							\
							\
  if (isFloat(tag)) {					\
    out = makeTaggedFloat(Fun(floatValue(AA)));		\
    return PROCEED;					\
  }							\
  TypeErrorT(0,"Float");				\
}							\
DECLAREBI_USEINLINEFUN1(BIName,InlineName)


FLOATFUN(exp, BIexp, BIinlineExp)
FLOATFUN(log, BIlog, BIinlineLog)
FLOATFUN(sqrt,BIsqrt,BIinlineSqrt) 
FLOATFUN(sin, BIsin, BIinlineSin) 
FLOATFUN(asin,BIasin,BIinlineAsin) 
FLOATFUN(cos, BIcos, BIinlineCos) 
FLOATFUN(acos,BIacos,BIinlineAcos) 
FLOATFUN(tan, BItan, BIinlineTan) 
FLOATFUN(atan,BIatan,BIinlineAtan) 
FLOATFUN(ceil,BIceil,BIinlineCeil)
FLOATFUN(floor,BIfloor,BIinlineFloor)
FLOATFUN(ozround, BIround, BIinlineRound)
#undef FLOATFUN


OZ_Return BIfPowInline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if (isFloat(tagA) && isFloat(tagB)) {
    out = makeTaggedFloat(pow(floatValue(A),floatValue(B)));
    return PROCEED;
  }
  return suspendOnFloats(A,B);
}

OZ_Return BIatan2Inline(TaggedRef A, TaggedRef B, TaggedRef &out)
{
  DEREF(A,_1,tagA);
  DEREF(B,_2,tagB);

  if (isFloat(tagA) && isFloat(tagB)) {
    out = makeTaggedFloat(atan2(floatValue(A),floatValue(B)));
    return PROCEED;
  }
  return suspendOnFloats(A,B);
}


/* -----------------------------------
   EXCEPTION HANDLERS
   ----------------------------------- */

static void invalid_handler(int /* sig */, 
		     int /* code */, 
		     struct sigcontext* /* scp */, 
		     char* /* addr */) {
  OZ_warning("signal: arithmethic exception: invalid argument");
}

static void overflow_handler(int /* sig */, 
		      int /* code */, 
		      struct sigcontext* /* scp */, 
		      char* /* addr */) {
  OZ_warning("signal: arithmethic exception: overflow");
}

static void underflow_handler(int /* sig */, 
		       int /* code */, 
		       struct sigcontext* /* scp */, 
		       char* /* addr */) {
  OZ_warning("signal: arithmethic exception: underflow");
}

static void divison_handler(int /* sig */, 
		     int /* code */, 
		     struct sigcontext* /* scp */, 
		     char* /* addr */) {
  OZ_warning("signal: arithmethic exception: divison by zero");
}


/* -----------------------------------
   make non inline versions
   ----------------------------------- */

DECLAREBI_USEINLINEREL2(BIless,BIlessInline)
DECLAREBI_USEINLINEREL2(BIle,BIleInline)
DECLAREBI_USEINLINEREL2(BIgreat,BIgreatInline)
DECLAREBI_USEINLINEREL2(BIge,BIgeInline)
DECLAREBI_USEINLINEREL2(BInumeq,BInumeqInline)
DECLAREBI_USEINLINEREL2(BInumneq,BInumneqInline)

DECLAREBI_USEINLINEFUN2(BIplus,BIplusInline)
DECLAREBI_USEINLINEFUN2(BIminus,BIminusInline)

DECLAREBI_USEINLINEFUN2(BImult,BImultInline)
DECLAREBI_USEINLINEFUN2(BIdiv,BIdivInline)
DECLAREBI_USEINLINEFUN2(BIfdiv,BIfdivInline)
DECLAREBI_USEINLINEFUN2(BImod,BImodInline)

DECLAREBI_USEINLINEFUN2(BIfPow,BIfPowInline)
DECLAREBI_USEINLINEFUN2(BIatan2,BIatan2Inline)

DECLAREBI_USEINLINEFUN2(BImax,BImaxInline)
DECLAREBI_USEINLINEFUN2(BImin,BIminInline)

DECLAREBI_USEINLINEFUN2(BIlessFun,BIlessInlineFun)
DECLAREBI_USEINLINEFUN2(BIleFun,BIleInlineFun)
DECLAREBI_USEINLINEFUN2(BIgreatFun,BIgreatInlineFun)
DECLAREBI_USEINLINEFUN2(BIgeFun,BIgeInlineFun)
DECLAREBI_USEINLINEFUN2(BInumeqFun,BInumeqInlineFun)
DECLAREBI_USEINLINEFUN2(BInumneqFun,BInumneqInlineFun)

DECLAREBI_USEINLINEFUN1(BIintToFloat,BIintToFloatInline)
DECLAREBI_USEINLINEFUN1(BIfloatToInt,BIfloatToIntInline)
DECLAREBI_USEINLINEFUN1(BIuminus,BIuminusInline)
DECLAREBI_USEINLINEFUN1(BIabs,BIabsInline)
DECLAREBI_USEINLINEFUN1(BIadd1,BIadd1Inline)
DECLAREBI_USEINLINEFUN1(BIsub1,BIsub1Inline)

// ---------------------------------------------------------------------
// Ports
// ---------------------------------------------------------------------

OZ_C_proc_begin(BInewPort,2)
{
  OZ_declareArg(0,val);
  OZ_declareArg(1,out);

  OZ_Term ret = OZ_newPort(val);
  return OZ_unify(out,ret);
}
OZ_C_proc_end

OZ_Return sendPort(OZ_Term prt, OZ_Term val)
{
  Assert(isPort(prt));

  Port *port  = tagged2Port(prt);
  TertType tt = port->getTertType();

  CheckLocalBoard(port,"port");

  if(tt==Te_Proxy) {
    int ret=remoteSend((PortProxy*) port,val);  
    return ret;
  } 
  LTuple *lt = new LTuple(val,am.currentUVarPrototype);
    
  OZ_Term old = ((PortWithStream*)port)->exchangeStream(lt->getTail());
    
  if (OZ_unify(makeTaggedLTuple(lt),old)!=PROCEED) {
    OZ_fail("OZ_send failed\n");
  }
  return PROCEED;
}

OZ_C_proc_begin(BIsendPort,2)
{
  OZ_declareNonvarArg(0,prt);
  OZ_declareArg(1,msg);

  prt = deref(prt);

  if (!isPort(prt)) {
    TypeErrorT(0,"Port");
  }

  return sendPort(prt,msg);
}
OZ_C_proc_end

OZ_Return closePort(OZ_Term prt)
{
  Assert(isPort(prt));

  Port *port  = tagged2Port(prt);
  TertType tt = port->getTertType();

  CheckLocalBoard(port,"port");

  if(tt==Te_Proxy) {
    int ret=remoteClose((PortProxy*) port);
    return ret;
  } 
  OZ_Term old = ((PortWithStream*)port)->exchangeStream(nil());
    
  if (OZ_unify(nil(),old)!=PROCEED) {
    return am.raise(E_SYSTEM,E_KERNEL,"portClosed",1,prt);	\
  }
  return PROCEED;
}

OZ_C_proc_begin(BIclosePort,1)
{
  OZ_declareNonvarArg(0,prt);

  prt = deref(prt);

  if (!isPort(prt)) {
    TypeErrorT(0,"Port");
  }

  return closePort(prt);
}
OZ_C_proc_end

// ---------------------------------------------------------------------
// Locks
// ---------------------------------------------------------------------

OZ_C_proc_begin(BInewLock,1)
{
  OZ_declareArg(0,out);

  OZ_Term ret = makeTaggedConst(new OzLock(am.currentBoard));

  return OZ_unify(out,ret);
}
OZ_C_proc_end


OZ_C_proc_begin(BIlockLock,1)
{
  OZ_declareNonvarArg(0,lock);

  lock = deref(lock);

  if (!isLock(lock)) {
    TypeErrorT(0,"Lock");
  }

  OzLock *lck = tagged2Lock(lock);
  if (!am.isToplevel()) {
    if (am.currentBoard != lck->getBoard()) {
      return am.raise(E_ERROR,E_KERNEL,"globalState",1,OZ_atom("lock"));
    }
  }

  TaggedRef *aux = lck->lock(am.currentThread);
  if (aux==NULL)
    return PROCEED;

  OZ_suspendOn(*aux);
}
OZ_C_proc_end


OZ_C_proc_begin(BIunlockLock,1)
{
  OZ_declareNonvarArg(0,lock);

  lock = deref(lock);

  if (!isLock(lock)) {
    TypeErrorT(0,"Lock");
  }

  tagged2Lock(lock)->unlock();

  return PROCEED;
}
OZ_C_proc_end


// ---------------------------------------------------------------------
// Cell
// ---------------------------------------------------------------------

OZ_C_proc_begin(BInewCell,2)
{
  OZ_Term val = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  return OZ_unify(out,OZ_newCell(val));
}
OZ_C_proc_end


OZ_Return BIexchangeCellInline(TaggedRef c, TaggedRef inState, TaggedRef &outState)
{
  NONVAR(c,rec);
  outState = makeTaggedRef(newTaggedUVar(am.currentBoard));

  if (!isCell(rec)) {
    TypeErrorT(0,"Cell");
  }
  Cell *cell = tagged2Cell(rec);

  CheckLocalBoard(cell,"cell");

  TaggedRef old = cell->exchangeValue(outState);
  return OZ_unify(old,inState);
}

/* we do not use here DECLAREBI_USEINLINEFUN2,
 * since we only want to suspend on the first argument
 */
OZ_C_proc_begin(BIexchangeCell,3)
{			
  OZ_Term help;
  OZ_Term cell = OZ_getCArg(0);
  OZ_Term inState = OZ_getCArg(1);
  OZ_Term outState = OZ_getCArg(2);
  OZ_Return state=BIexchangeCellInline(cell,inState,help);
  switch (state) {
  case SUSPEND:
    OZ_suspendOn(cell);
  case PROCEED:
    return OZ_unify(help,outState);
  default:
    return state;
  }
}
OZ_C_proc_end


OZ_Return BIaccessCellInline(TaggedRef c, TaggedRef &out)
{
  NONVAR(c,rec);

  if (!isCell(rec)) {
    TypeErrorT(0,"Cell");
  }
  Cell *cell = tagged2Cell(rec);

  out = cell->getValue();
  return PROCEED;
}

DECLAREBI_USEINLINEFUN1(BIaccessCell,BIaccessCellInline)

OZ_Return BIassignCellInline(TaggedRef c, TaggedRef in)
{
  NONVAR(c,rec);

  if (!isCell(rec)) {
    TypeErrorT(0,"Cell");
  }
  Cell *cell = tagged2Cell(rec);

  CheckLocalBoard(cell,"cell");

  cell->setValue(in);
  return PROCEED;
}

DECLAREBI_USEINLINEREL2(BIassignCell,BIassignCellInline)

/********************************************************************
 * Arrays
 ******************************************************************** */

OZ_C_proc_begin(BIarrayNew,4)
{
  OZ_declareIntArg(0,low);
  OZ_declareIntArg(1,high);
  OZ_Term initvalue = OZ_getCArg(2);
  OZ_Term out       = OZ_getCArg(3);

  low = deref(OZ_getCArg(0));
  high = deref(OZ_getCArg(1));

  if (!isSmallInt(low)) {
    TypeErrorT(0,"smallInteger");
  }
  if (!isSmallInt(high)) {
    TypeErrorT(1,"smallInteger");
  }

  int ilow  = smallIntValue(low);
  int ihigh = smallIntValue(high);

  return OZ_unify(makeTaggedConst(new OzArray(am.currentBoard,ilow,ihigh,initvalue)),out);
}
OZ_C_proc_end


OZ_Return isArrayInline(TaggedRef t, TaggedRef &out)
{
  NONVAR( t, term );
  out = isArray(term) ? NameTrue : NameFalse;
  return PROCEED;
}

DECLAREBI_USEINLINEFUN1(BIisArray,isArrayInline)

OZ_Return arrayLowInline(TaggedRef t, TaggedRef &out)
{
  NONVAR( t, term );
  if (!isArray(term)) {
    TypeErrorT(0,"Array");
  }
  out = makeInt(tagged2Array(term)->getLow());
  return PROCEED;
}
DECLAREBI_USEINLINEFUN1(BIarrayLow,arrayLowInline)

OZ_Return arrayHighInline(TaggedRef t, TaggedRef &out)
{
  NONVAR( t, term );
  if (!isArray(term)) {
    TypeErrorT(0,"Array");
  }
  out = makeInt(tagged2Array(term)->getHigh());
  return PROCEED;
}

DECLAREBI_USEINLINEFUN1(BIarrayHigh,arrayHighInline)

OZ_Return arrayGetInline(TaggedRef t, TaggedRef i, TaggedRef &out)
{
  NONVAR( t, array );
  NONVAR( i, index );

  if (!isArray(array)) {
    TypeErrorT(0,"Array");
  }

  if (!isSmallInt(index)) {
    TypeErrorT(1,"smallInteger");
  }

  OzArray *ar = tagged2Array(array);
  out = ar->getArg(smallIntValue(index));
  if (out) return PROCEED;
  return am.raise(E_ERROR,E_KERNEL,"array",2,array,index);
}
DECLAREBI_USEINLINEFUN2(BIarrayGet,arrayGetInline)

OZ_Return arrayPutInline(TaggedRef t, TaggedRef i, TaggedRef value)
{
  NONVAR( t, array );
  NONVAR( i, index );

  if (!isArray(array)) {
    TypeErrorT(0,"Array");
  }

  if (!isSmallInt(index)) {
    TypeErrorT(1,"smallInteger");
  }

  OzArray *ar = tagged2Array(array);
  CheckLocalBoard(ar,"array");
  if (ar->setArg(smallIntValue(index),value)) return PROCEED;

  return am.raise(E_ERROR,E_KERNEL,"array",2,array,index);
}

DECLAREBI_USEINLINEREL3(BIarrayPut,arrayPutInline)


/********************************************************************
 *   Dictionaries
 ******************************************************************** */

OZ_C_proc_begin(BIdictionaryNew,1)
{
  OZ_Term out       = OZ_getCArg(0);

  return OZ_unify(makeTaggedConst(new OzDictionary(am.currentBoard)),out);
}
OZ_C_proc_end


OZ_C_proc_begin(BIdictionaryKeys,2)
{
  OZ_Term d   = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  NONVAR1( d, dict );
  if (!isDictionary(dict)) {
    TypeErrorT(0,"Dictionary");
  }

  return OZ_unify(tagged2Dictionary(dict)->keys(),out);
}
OZ_C_proc_end


OZ_C_proc_begin(BIdictionaryEntries,2)
{
  OZ_Term d   = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  NONVAR1( d, dict );
  if (!isDictionary(dict)) {
    TypeErrorT(0,"Dictionary");
  }

  return OZ_unify(tagged2Dictionary(dict)->pairs(),out);
}
OZ_C_proc_end


OZ_Return isDictionaryInline(TaggedRef t, TaggedRef &out)
{
  NONVAR( t, term);
  out = isDictionary(term) ? NameTrue : NameFalse;
  return PROCEED;
}
DECLAREBI_USEINLINEFUN1(BIisDictionary,isDictionaryInline)


#define GetDictAndKey(d,k,dict,key,checkboard)			\
  NONVAR(d,dictaux);						\
  NONVAR(k,key);						\
  if (!isDictionary(dictaux)) { TypeErrorT(0,"Dictionary"); }	\
  if (!isFeature(key))        { TypeErrorT(1,"feature"); }	\
  OzDictionary *dict = tagged2Dictionary(dictaux);		\
  if (checkboard) { CheckLocalBoard(dict,"dict"); }


OZ_Return dictionaryMemberInline(TaggedRef d, TaggedRef k, TaggedRef &out)
{
  GetDictAndKey(d,k,dict,key,NO);
  out = dict->member(key);
  return PROCEED;
}
DECLAREBI_USEINLINEFUN2(BIdictionaryMember,dictionaryMemberInline)


OZ_Return dictionaryGetInline(TaggedRef d, TaggedRef k, TaggedRef &out)
{
  GetDictAndKey(d,k,dict,key,NO);
  if (dict->getArg(key,out) != PROCEED) {
    return am.raise(E_ERROR,E_KERNEL,"dict",2,d,k);
  }
  return PROCEED;
}
DECLAREBI_USEINLINEFUN2(BIdictionaryGet,dictionaryGetInline)


OZ_Return dictionaryGetIfInline(TaggedRef d, TaggedRef k, TaggedRef deflt, TaggedRef &out)
{
  GetDictAndKey(d,k,dict,key,NO);
  if (dict->getArg(key,out) != PROCEED) {
    out = deflt;
  }
  return PROCEED;
}
DECLAREBI_USEINLINEFUN3(BIdictionaryGetIf,dictionaryGetIfInline)

OZ_Return dictionaryPutInline(TaggedRef d, TaggedRef k, TaggedRef value)
{
  GetDictAndKey(d,k,dict,key,OK);
  dict->setArg(key,value);
  return PROCEED;
}

DECLAREBI_USEINLINEREL3(BIdictionaryPut,dictionaryPutInline)


OZ_Return dictionaryDeepPutInline(TaggedRef d, TaggedRef k, TaggedRef value)
{
  GetDictAndKey(d,k,dict,key,NO);
  dict->setArg(key,value);
  return PROCEED;
}

DECLAREBI_USEINLINEREL3(BIdictionaryDeepPut,dictionaryDeepPutInline)


OZ_Return dictionaryRemoveInline(TaggedRef d, TaggedRef k)
{
  GetDictAndKey(d,k,dict,key,OK);
  dict->remove(key);
  return PROCEED;
}
DECLAREBI_USEINLINEREL2(BIdictionaryRemove,dictionaryRemoveInline)


OZ_C_proc_begin(BIdictionaryToRecord,3)
{
  OZ_Term d = OZ_getCArg(0);
  OZ_Term l = OZ_getCArg(1);
  OZ_Term r = OZ_getCArg(2);

  NONVAR1( d, dict); 
  if (!isDictionary(dict)) {
    TypeErrorT(0,"Dictionary");
  }
  NONVAR1( l, lbl);
  if (!isLiteral(lbl)) {
    TypeErrorT(1,"Literal");
  }
  return OZ_unify(tagged2Dictionary(dict)->toRecord(lbl),r);
}
OZ_C_proc_end


#endif /* BUILTINS1 */

/* -----------------------------------------------------------------
   dynamic link objects files
   ----------------------------------------------------------------- */

#ifdef BUILTINS2

#ifdef WINDOWS
#define PATHSEP ';'
#else
#define PATHSEP ':'
#endif

#ifdef _MSC_VER
#define F_OK 00
#endif

char *expandFileName(char *fileName,char *path) {

  char *ret;

  if (*fileName == '/') {
    if (access(fileName,F_OK) == 0) {
      ret = new char[strlen(fileName)+1];
      strcpy(ret,fileName);
      return ret;
    }
    return NULL;
  }

  if (!path) {
    return NULL;
  }

  char *tmp = new char[strlen(path)+strlen(fileName)+2];
  char *last = tmp;
  while (1) {
    if (*path == PATHSEP || *path == 0) {

      if (last == tmp && *path == 0) { // path empty ??
	return NULL;
      }

      *last++ = '/';
      strcpy(last,fileName);
      if (access(tmp,F_OK) == 0) {
	ret = new char[strlen(tmp)+1];
	strcpy(ret,tmp);
	delete [] tmp;
	return ret;
      }
      last = tmp;
      if (*path == 0) {
	return NULL;
      }
      path++;
    } else {
      *last++ = *path++;
    }
  }
    
}


static void strCat(char toStr[], int &toStringSize, char *fromString)
{
  int i = toStringSize;
  while (*fromString)  {
    toStr[i++] = *fromString;
    fromString++;
  }
  toStr[i] = '\0';
  toStringSize = i;
}


/* arrayFromList(list,array,size):
 *       "list" is a Oz list of atoms.
 *     "array" is an array containing those atoms of max. size "size"
 *     returns NULL on failure
 */

char **arrayFromList(OZ_Term list, char **array, int size)
{
  int i=0;

  while(OZ_isCons(list)) {

    if (i >= size-1) {
      OZ_warning("linkObjectFiles: too many arguments");
      goto bomb;
    }

    OZ_Term hh = head(deref(list));
    if (!OZ_isAtom(hh)) {
      OZ_warning("linkObjectFiles: List with atoms expected");
      goto bomb;
    }
    char *fileName = OZ_atomToC(hh);
    
    char *f = expandFileName(fileName,ozconf.linkPath);

    if (!f) {
      OZ_warning("linkObjectFiles(%s): expand filename failed",fileName);
      goto bomb;    
    }

    array[i++] = f;
    list = OZ_tail(list);
  }

  array[i] = NULL;
  return array;

 bomb:
  while (i>0) {
    i--;
    char *f = array[i];
    delete [] f;
  }
  return NULL;
}

/* linkObjectFiles(+filelist,-handle)
 *    filelist: list of atoms (== object files)
 *    handle:   for future calls of findFunction
 * we do not check for well-typedness, do it in Oz !!
 */

OZ_C_proc_begin(BIlinkObjectFiles,2)
{
  OZ_Term list = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

#ifdef DLOPEN
  const int commandSize = 1000;
  int commandUsed = 0;
  char command[commandSize];
  char *tempfile = tmpnam(NULL);
#ifdef SOLARIS_SPARC
  strCat(command, commandUsed, "ld -G -lsocket -lnsl -lintl -o ");
#endif
#ifdef SUNOS_SPARC
  strCat(command, commandUsed, "ld -o ");
#endif
#ifdef IRIX5_MIPS
  strCat(command, commandUsed, "ld -shared -check_registry /dev/null -o ");
#endif
#ifdef OSF1_ALPHA
  strCat(command, commandUsed, "ld -shared -expect_unresolved \\* -o ");
#endif
#ifdef HPUX_700
  strCat(command, commandUsed, "ld -b -o ");
#endif
#ifdef LINUX_I486
  strCat(command, commandUsed, "ld -shared -o ");
#endif
  strCat(command, commandUsed, tempfile);

  const numOfiles = 100;
  char *ofiles[numOfiles];
  if (arrayFromList(list,ofiles,numOfiles) == NULL) {
    unlink(tempfile);
    goto raise;
  }

  {
    for (int i=0; ofiles[i] != NULL; i++) {
      char *f = ofiles[i];
      if (commandUsed + strlen(f) >= commandSize-1) {
	OZ_warning("linkObjectFiles: too many arguments");
	unlink(tempfile);
	delete [] f;
	goto raise;
      }
      strCat(command, commandUsed, " ");
      strCat(command, commandUsed, f);
      delete [] f;
    }
  }
  
  if (ozconf.showForeignLoad) {
    message("Linking files\n %s\n",command);
  }

  if (osSystem(command) < 0) {
    char buf[1000];
    sprintf(buf,"'%s' failed in linkObjectFiles",command);
    ozpwarning(buf);
    unlink(tempfile);
    goto raise;
  }

#ifdef HPUX_700
  shl_t handle;
  handle = shl_load(tempfile,
                    BIND_IMMEDIATE | BIND_NONFATAL | BIND_NOSTART | BIND_VERBOSE, 0L);

  if (handle == NULL) {
    goto raise;
  }
#else
  void *handle;

  if (!(handle = (void *)dlopen(tempfile, RTLD_NOW ))) {
    OZ_warning("dlopen failed in linkObjectFiles: %s",dlerror());
    unlink(tempfile);
    goto raise;
  }
#endif
  
  unlink(tempfile);
  return OZ_unifyInt(out,ToInt32(handle));

#elif defined(WINDOWS)

  const numOfiles = 100;
  char *ofiles[numOfiles];
  if (arrayFromList(list,ofiles,numOfiles) == NULL ||
      ofiles[0]==NULL ||
      ofiles[1]!=NULL) {
    OZ_warning("linkObjectFiles(%s): can only accept one DLL\n",toC(list));
    goto raise;
  }

  if (ozconf.showForeignLoad) {
    message("Linking files\n %s\n",ofiles[0]);
  }

  {
    void *handle = (void *)LoadLibrary(ofiles[0]);
    if (handle==NULL) {
      OZ_warning("failed in linkObjectFiles: %d",GetLastError());
      goto raise;
    }
    return OZ_unifyInt(out,ToInt32(handle));
  }
#endif
  return PROCEED;

raise:
  return am.raise(E_ERROR,E_KERNEL,"foreign",2,OZ_atom("linkFiles"),list);
}
OZ_C_proc_end



OZ_C_proc_begin(BIdlOpen,2)
{
  OZ_declareVirtualStringArg(0,filename);
  OZ_Term out = OZ_getCArg(1);

  OZ_Term err=NameUnit;
  OZ_Term ret=NameUnit;

  filename = expandFileName(filename,ozconf.linkPath);

  if (!filename) {
    err = OZ_atom("expand filename failed");
    goto raise;
  }

  if (ozconf.showForeignLoad) {
    message("Linking file %s\n",filename);
  }

#ifdef DLOPEN
#ifdef HPUX_700
  {
    shl_t handle;
    handle = shl_load(filename,
		      BIND_IMMEDIATE | BIND_NONFATAL |
		      BIND_NOSTART | BIND_VERBOSE, 0L);

    if (handle == NULL) {
      goto raise;
    }
    ret = OZ_int(ToInt32(handle));
  }
#else
  {
    void *handle=dlopen(filename, RTLD_NOW);

    if (!handle) {
      err=OZ_atom((char *) dlerror());
      goto raise;
    }
    ret = OZ_int(ToInt32(handle));
  }
#endif

#elif defined(WINDOWS)
  {
    void *handle = (void *)LoadLibrary(filename);
    if (!handle) {
      err=OZ_int(GetLastError());
      goto raise;
    }
    ret = OZ_int(ToInt32(handle));
  }
#endif

  return OZ_unify(out,ret);

raise:
  return am.raise(E_ERROR,E_KERNEL,"foreign",3,OZ_atom("dlOpen"),
		  filename,err);
}
OZ_C_proc_end

OZ_C_proc_begin(BIunlinkObjectFile,1)
{
  OZ_declareAtomArg(0,fileName);

#ifdef WINDOWS
  FreeLibrary(GetModuleHandle(fileName));
#endif

  return PROCEED;
}
OZ_C_proc_end


OZ_C_proc_begin(BIdlClose,1)
{
  OZ_declareIntArg(0,handle);


#ifdef DLOPEN
  if (dlclose((void *)handle)) {
    goto raise;
  }
#endif

#ifdef WINDOWS
  FreeLibrary((void *) handle);
#endif

  return PROCEED;

raise:
  return am.raise(E_ERROR,E_KERNEL,"foreign",2,
		  OZ_atom("dlClose"),OZ_int(handle));
}
OZ_C_proc_end

#if defined(DLOPEN)
#define Link(handle,name) dlsym(handle,name)
#elif defined(HPUX_700)
void *ozdlsym(void *handle,char *name)
{
  void *symaddr;

  int symbol = shl_findsym((shl_t*)&handle, name, TYPE_PROCEDURE, &symaddr);
  if (symbol != 0) {
    return NULL;
  }

  return symaddr;
}

#define Link(handle,name) ozdlsym(handle,name)

#elif defined(WINDOWS)

FARPROC winLink(HMODULE handle, char *name)
{
  FARPROC ret = GetProcAddress(handle,name);
  if (ret == NULL) {
    // try prepending a "_"
    char buf[1000];
    sprintf(buf,"_%s",name);
    ret = GetProcAddress(handle,buf);
  }
  return ret;
}

#define Link(handle,name) winLink(handle,name)

#endif

/* findFunction(+fname,+farity,+handle) */
OZ_C_proc_begin(BIfindFunction,3)
{
#if defined(DLOPEN) || defined(HPUX_700) || defined(WINDOWS)
  OZ_declareAtomArg(0,functionName);
  OZ_declareIntArg(1,functionArity);
  OZ_declareIntArg(2,handle);

  // get the function
  OZ_CFun func;
  if ((func = (OZ_CFun) Link((void *) handle,functionName)) == 0) {
    char * a0 = strdup(toC(OZ_getCArg(0)));
    char * a1 = strdup(toC(OZ_getCArg(1)));
    char * a2 = strdup(toC(OZ_getCArg(2)));
    OZ_warning("Cannot find function %s of arity %s with handle %s.", 
	       a0, a1, a2);
    free(a0);
    free(a1); 
    free(a2);
#ifdef WINDOWS
    OZ_warning("error=%d\n",GetLastError());
#endif
    goto raise;
  }
  
  OZ_addBuiltin(functionName,functionArity,*func);
#endif

  return PROCEED;
raise:
  return am.raise(E_ERROR,E_KERNEL,"foreign", 3,
		     OZ_atom("cannotFindFunction"), 
		     OZ_getCArg(0), 
		     OZ_getCArg(1));
}
OZ_C_proc_end


/* ------------------------------------------------------------
 * Shutdown
 * ------------------------------------------------------------ */

OZ_C_proc_begin(BIshutdown,0)
{
  am.exitOz(0);
  return(PROCEED); /* not reached but anyway */
}
OZ_C_proc_end 

/* ------------------------------------------------------------
 * Alarm und Delay
 * ------------------------------------------------------------ */

OZ_C_proc_begin(BIalarm,2) {
  OZ_declareIntArg(0,t);
  OZ_Term out=OZ_getCArg(1);

  if (!am.isToplevel()) {
    return am.raise(E_ERROR,E_KERNEL,"globalState",1,OZ_atom("io"));
  }

  if (t <= 0) 
    return OZ_unify(NameUnit,out);

  am.insertUser(t,cons(NameUnit,out));
  return PROCEED;
}
OZ_C_proc_end


OZ_C_proc_begin(BIdelay,1) {
  OZ_declareIntArg(0,t);

  if (!am.isToplevel()) {
    return am.raise(E_ERROR,E_KERNEL,"globalState",1,OZ_atom("io"));
  }
  
  if (t <= 0)
    return PROCEED;

  TaggedRef var = makeTaggedRef(newTaggedUVar(am.currentBoard));

  am.insertUser(t,cons(NameUnit,var));
  DEREF(var, var_ptr, var_tag);
  
  if (isAnyVar(var_tag)) {
    am.addSuspendVarList(var_ptr);
    OZ_args[0] = makeTaggedSmallInt(-1);
    return SUSPEND;
  }
  return PROCEED;
}
OZ_C_proc_end


/* ------------------------------------------------------------
 * Garbage Collection
 * ------------------------------------------------------------ */

OZ_C_proc_begin(BIgarbageCollection,0)
{
  am.setSFlag(StartGC);

  return BI_PREEMPT;
}
OZ_C_proc_end

/* ------------------------------------------------------------
 * System specials
 * ------------------------------------------------------------ */

OZ_C_proc_begin(BIsystemEq,3) {
  return OZ_unify(OZ_getCArg(2),
		  termEq(OZ_getCArg(0),OZ_getCArg(1)) ? NameTrue : NameFalse);
}
OZ_C_proc_end

/*
 * unify: used by compiler if '\sw -optimize'
 */
OZ_C_proc_begin(BIunify,2)
{
  OZ_Term t0 = OZ_getCArg(0);
  OZ_Term t1 = OZ_getCArg(1);

  return OZ_unify(t0,t1);
}
OZ_C_proc_end

OZ_C_proc_begin(BIfail,VarArity)
{
  return FAILED;
}
OZ_C_proc_end 

/*
 * nop: used as fallback when loading builtins
 */
OZ_C_proc_begin(BInop,VarArity)
{
  warning("nop");
  return PROCEED;
}
OZ_C_proc_end 

// ------------------------------------------------------------------------
// load precompiled file
// ------------------------------------------------------------------------

OZ_C_proc_begin(BIloadFile,1)
{
  TaggedRef term0 = OZ_getCArg(0);
  DEREF(term0,_0,tag0);
  
  if (!isAtom(term0)) {
    TypeErrorT(0,"Atom");
  }

  char *file = tagged2Literal(term0)->getPrintName();
  
  CompStream *fd = CompStream::csopen(file);

  if (fd == NULL) {
    OZ_warning("call: loadFile: cannot open file '%s'",file);
    return am.raise(E_ERROR,E_KERNEL,"loadFile",1,term0);
  }
  
  if (ozconf.showFastLoad) {
    printf("Fast loading file '%s'\n",file);
  }
  
  // begin critical region
  osBlockSignals();
  
  am.loadQuery(fd);
  
  fd->csclose();

  osUnblockSignals();
  // end critical region

  return PROCEED;
}
OZ_C_proc_end


// ------------------------------------------------------------------------
// --- Apply
// ------------------------------------------------------------------------

OZ_C_proc_begin(BIapply,2)
{
  OZ_Term proc = OZ_getCArg(0);
  OZ_Term args = OZ_getCArg(1);

  OZ_Term var;
  if (!OZ_isList(args,&var)) {
    if (var == 0) TypeErrorT(1,"List");
    OZ_suspendOn(var);
  }

  int len = OZ_length(args);
  RefsArray argsArray = allocateY(len);
  for (int i=0; i < len; i++) {
    argsArray[i] = OZ_head(args);
    args=OZ_tail(args);
  }
  Assert(OZ_isNil(args));

  DEREF(proc,procPtr,_2);
  if (isAnyVar(proc)) OZ_suspendOn(makeTaggedRef(procPtr));
  if (!isProcedure(proc) && !isObject(proc)) {
    TypeErrorT(0,"Procedure or Object");
  }
  
  am.pushCall(proc,len,argsArray);
  deallocateY(argsArray);
  return PROCEED;
}
OZ_C_proc_end

// ------------------------------------------------------------------------
// --- special Cell access
// ------------------------------------------------------------------------

OZ_C_proc_begin(BIdeepFeed,2)
{
  OZ_Term c = OZ_getCArg(0);
  OZ_Term val = OZ_getCArg(1);

  NONVAR1(c,rec);
  if (!isCell(rec)) {
    TypeErrorT(0,"Cell");
  }

  Cell *cell1 = tagged2Cell(rec);

  Board *savedNode = am.currentBoard;
  Board *home1 = cell1->getBoard();

  switch (am.installPath(home1)) {
  case INST_FAILED:
  case INST_REJECTED:
    error("deep: install");
  case INST_OK:
    break;
  }

  TaggedRef newVar = makeTaggedRef(newTaggedUVar(am.currentBoard));
  TaggedRef old = cell1->exchangeValue(newVar);
  OZ_Return ret = OZ_unify(old,cons(val,newVar));

  switch (am.installPath(savedNode)) {
  case INST_FAILED:
  case INST_REJECTED:
    error("deep: install back");
  case INST_OK:
    break;
  }

  return ret;
}
OZ_C_proc_end


OZ_C_proc_begin(BIdeepReadCell,2)
{
  OZ_Term c = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);
  NONVAR1(c,rec);
  if (!isCell(rec)) {
    TypeErrorT(0,"Cell");
  }
  Cell *cell1 = tagged2Cell(rec);
  return OZ_unify(out,cell1->getValue());
}
OZ_C_proc_end



/* ---------------------------------------------------------------------
 * Browser: special builtins: getsBound, intToAtom
 * --------------------------------------------------------------------- */

OZ_C_proc_begin(_getsBound_dummy, 0)
{
  return PROCEED;
}
OZ_C_proc_end


OZ_C_proc_begin(BIgetsBound, 1)
{
  OZ_Term v = OZ_getCArg(0);

  DEREF(v, vPtr, vTag);
  
  if (isAnyVar(vTag)){
    Thread *thr =
      (Thread *) OZ_makeSuspendedThread (_getsBound_dummy, NULL, 0);
    SuspList *vcsl = new SuspList(thr, NULL);
    addSuspAnyVar(vPtr, vcsl);
  }

  return PROCEED;
}
OZ_C_proc_end


OZ_C_proc_begin(_getsBound_dummyB, 2)
{
  return (OZ_unify (OZ_getCArg (1), NameTrue));
}
OZ_C_proc_end


OZ_C_proc_begin(BIgetsBoundB, 2)
{
  OZ_Term v = OZ_getCArg(0);

  DEREF(v, vPtr, vTag);
  
  if (isAnyVar(vTag)){
    Thread *thr =
      (Thread *) OZ_makeSuspendedThread (_getsBound_dummyB, OZ_args, OZ_arity);
    SuspList *vcsl = new SuspList (thr, NULL);
    addSuspAnyVar(vPtr, vcsl);
  }

  return (PROCEED);		// no result yet;
}
OZ_C_proc_end

/* ---------------------------------------------------------------------
 * ???
 * --------------------------------------------------------------------- */

OZ_C_proc_begin(BIconstraints,2)
{
  OZ_Term in = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  DEREF(in,inPtr,inTag);
  int len = 0;
  if (isCVar(inTag)) {
    len=tagged2CVar(in)->getSuspListLength();
  } else if (isSVar(inTag)) {
    len = tagged2SVar(in)->getSuspList()->length();
  }
  return OZ_unifyInt(out,len);
}
OZ_C_proc_end

// ---------------------------------------------------------------------------
// linguistic reflection
// ---------------------------------------------------------------------------

OZ_C_proc_begin(BIconnectLingRef,1)
{
  OZ_warning("connectLingRef no longer needed");
  return PROCEED;
}
OZ_C_proc_end


OZ_C_proc_begin(BIgetLingRefFd,1)
{
  return OZ_unifyInt(OZ_getCArg(0),am.compStream->getLingRefFd());
}
OZ_C_proc_end


OZ_C_proc_begin(BIgetLingEof,1)
{
  return OZ_unifyInt(OZ_getCArg(0),am.compStream->getLingEOF());
}
OZ_C_proc_end


// ---------------------------------------------------------------------------
// abstraction table
// ---------------------------------------------------------------------------

OZ_C_proc_begin(BIsetAbstractionTabDefaultEntry,1)
{
  TaggedRef in = OZ_getCArg(0); DEREF(in,_1,_2);
  if (!isAbstraction(in)) {
    TypeErrorT(0,"Abstraction");
  }

  AbstractionEntry::setDefaultEntry(tagged2Abstraction(in));
  return PROCEED;
}
OZ_C_proc_end

/* ---------------------------------------------------------------------
 * System
 * --------------------------------------------------------------------- */

/* print and show are inline,
   because it prevents the compiler from generating different code
   */
OZ_Return printInline(TaggedRef term)
{
  char *s=OZ_toC(term,ozconf.printDepth,ozconf.printWidth);
  fprintf(stdout,"%s",s);
  fflush(stdout);
  return (PROCEED);
}

DECLAREBI_USEINLINEREL1(BIprint,printInline)

OZ_C_proc_begin(BIprintInfo,1) {
  OZ_Term t=OZ_getCArg(0);
  OZ_printVirtualString(t);
  fflush(stdout);
  return PROCEED;
} OZ_C_proc_end

OZ_C_proc_begin(BIprintError,1) {
  OZ_Term t=OZ_getCArg(0);
  // print popup code for opi
  if (!am.isStandalone()) printf("\021");
  OZ_printVirtualString(t);
  fflush(stdout);
  return PROCEED;
} OZ_C_proc_end

OZ_C_proc_begin(BItermToVS,4)
{
  OZ_Term t=OZ_getCArg(0);
  OZ_declareIntArg(1,depth);
  OZ_declareIntArg(2,width);  
  return OZ_unify(OZ_getCArg(3), OZ_string(OZ_toC(t,depth,width)));
}
OZ_C_proc_end

OZ_C_proc_begin(BIgetTermSize,4) {
  OZ_declareArg(0,t);
  OZ_declareIntArg(1,depth);
  OZ_declareIntArg(2,width);
  OZ_declareArg(3,out);

  return OZ_unify(out, OZ_int(OZ_termGetSize(t, depth, width)));
}
OZ_C_proc_end

OZ_Return showInline(TaggedRef term)
{
  printInline(term);
  printf("\n");
  return (PROCEED);
}

DECLAREBI_USEINLINEREL1(BIshow,showInline)

OZ_C_proc_begin(BIprintLong,1)
{
  OZ_Term args0 = OZ_getCArg(0);
  taggedPrintLong(args0);

  return PROCEED;
}
OZ_C_proc_end

// ---------------------------------------------------------------------------
// ???
// ---------------------------------------------------------------------------

TaggedRef Abstraction::DBGgetGlobals() {
  int n = getGSize();
  OZ_Term t = OZ_tuple(OZ_atom("globals"),n);
  for (int i = 0; i < n; i++) {
    OZ_putArg(t,i,gRegs[i]);
  }
  return t;
}

OZ_C_proc_begin(BIgetPrintName,2)
{
  OZ_Term term = OZ_getCArg(0);
  OZ_Term out  = OZ_getCArg(1);

  DEREF(term,termPtr,tag);

  switch (tag) {
  case OZCONST:
    if (isConst(term)) {
      ConstTerm *rec = tagged2Const(term);
      switch (rec->getType()) {
      case Co_Builtin:      return OZ_unify(out, ((BuiltinTabEntry *) rec)->getName());
      case Co_Abstraction:  return OZ_unify(out, ((Abstraction *) rec)->getName());
      case Co_Object:  	    return OZ_unifyAtom(out, ((Object*) rec)->getPrintName());

      case Co_Cell:
      case Co_Dictionary:
      case Co_Array:
      default:    	   return OZ_unifyAtom(out,"_");
      }
    }
    break;

  case UVAR:    return OZ_unifyAtom(out, "_");
  case SVAR:
  case CVAR:    return OZ_unifyAtom(out, VariableNamer::getName(OZ_getCArg(0)));
  case LITERAL: return OZ_unifyAtom(out, tagged2Literal(term)->getPrintName());

  default:      break;
  }

  return OZ_unifyAtom(out, tagged2String(term,ozconf.printDepth));
}
OZ_C_proc_end

TaggedRef SVariable::DBGmakeSuspList()
{
  return suspList->DBGmakeList();
}

TaggedRef SuspList::DBGmakeList() {
  if (this == NULL) {
    return nil();
  }

  Thread *t = getElem();
  Board *b = t->getBoard();
  return cons(makeTaggedConst(b),getNext()->DBGmakeList());
}


//----------------------------------------------------------------------
//  System set and get
//----------------------------------------------------------------------

#define GetRecord \
  SRecord *r = tagged2SRecord(deref(OZ_getCArg(0)));

#define SetTaggedArg(a, t) \
  r->setFeature(a, t);

#define SetIntArg(a, n) \
  r->setFeature(a, makeInt(n));

#define SetBoolArg(a, n) \
  r->setFeature(a, (n) ? NameTrue : NameFalse);

OZ_C_proc_begin(BISystemGetThreads,1) {
  GetRecord;
  SetIntArg(AtomCreated,  ozstat.createdThreads.total);
  SetIntArg(AtomRunnable, am.getRunnableNumber());
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetPriorities,1) {
  GetRecord;
  SetIntArg(AtomHigh,   ozconf.hiMidRatio);
  SetIntArg(AtomMedium, ozconf.midLowRatio);
  return PROCEED;				
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetTime,1) {
  GetRecord;
  unsigned int timeNow = osUserTime();

  SetIntArg(AtomCopy,      ozstat.timeForCopy.total);
  SetIntArg(AtomGC,        ozstat.timeForGC.total);
  SetIntArg(AtomLoad,      ozstat.timeForLoading.total);
  SetIntArg(AtomPropagate, ozstat.timeForPropagation.total);
  SetIntArg(AtomRun,       timeNow-(ozstat.timeForGC.total +
				    ozstat.timeForLoading.total +
				    ozstat.timeForPropagation.total +
				    ozstat.timeForCopy.total));
  SetIntArg(AtomSystem,    osSystemTime());
  SetIntArg(AtomTotal,     osTotalTime());
  SetIntArg(AtomUser,      timeNow);

  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetGC,1) {
  GetRecord;

  SetIntArg(AtomMin,       ozconf.heapMinSize*KB);
  SetIntArg(AtomMax,       ozconf.heapMaxSize*KB);
  SetIntArg(AtomFree,      ozconf.heapFree);
  SetIntArg(AtomTolerance, ozconf.heapTolerance);
  SetBoolArg(AtomOn,       ozconf.gcFlag);
  SetIntArg(AtomThreshold, ozconf.heapThreshold*KB);
  SetIntArg(AtomSize,      getUsedMemory()*KB);
  SetIntArg(AtomActive,    ozstat.gcLastActive*KB);
  
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetPrint,1) {
  GetRecord;

  SetIntArg(AtomDepth, ozconf.printDepth);
  SetIntArg(AtomWidth, ozconf.printWidth);
  
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetFD,1) {
  GetRecord;

  SetIntArg(AtomVariables,   ozstat.fdvarsCreated.total);
  SetIntArg(AtomPropagators, ozstat.propagatorsCreated.total);
  SetIntArg(AtomInvoked,     ozstat.propagatorsInvoked.total);
  SetIntArg(AtomThreshold,   32 * fd_bv_max_high);
  
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetSpaces,1) {
  GetRecord;

  SetIntArg(AtomCommitted, ozstat.solveAlt.total);
  SetIntArg(AtomCloned,    ozstat.solveCloned.total);
  SetIntArg(AtomCreated,   ozstat.solveCreated.total);
  SetIntArg(AtomFailed,    ozstat.solveFailed.total);
  SetIntArg(AtomSucceeded, ozstat.solveSolved.total);
  
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetErrors,1) {
  GetRecord;

  SetBoolArg(AtomLocation, ozconf.errorLocation);
  SetBoolArg(AtomDebug,    ozconf.errorDebug);
  SetBoolArg(AtomHints,    ozconf.errorHints);
  SetIntArg(AtomThread,    ozconf.errorThreadDepth);
  SetIntArg(AtomDepth,     ozconf.errorPrintDepth);
  SetIntArg(AtomWidth,     ozconf.errorPrintWidth);
  
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetMessages,1) {
  GetRecord;

  SetBoolArg(AtomGC,      ozconf.gcVerbosity);
  SetBoolArg(AtomIdle,    ozconf.showIdleMessage);
  SetBoolArg(AtomFeed,    ozconf.showFastLoad);
  SetBoolArg(AtomForeign, ozconf.showForeignLoad);
  
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetMemory,1) {
  GetRecord;

  SetIntArg(AtomAtoms,    ozstat.getAtomMemory());
  SetIntArg(AtomNames,    ozstat.getNameMemory());
  SetIntArg(AtomBuiltins, builtinTab.memRequired());
  SetIntArg(AtomFreelist, getMemoryInFreeList());
  SetIntArg(AtomCode,     CodeArea::totalSize);
  SetIntArg(AtomHeap,     ozstat.heapUsed.total+getUsedMemory());

  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetLimits,1) {
  GetRecord;

  SetTaggedArg(AtomInt, OZ_pair2(makeInt(OzMinInt),
				 makeInt(OzMaxInt)));

  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetArgv,1) {
  TaggedRef out = nil();
  for(int i=ozconf.argC-1; i>=0; i--) {
    out = cons(OZ_atom(ozconf.argV[i]),out);
  }
  return OZ_unify(OZ_getCArg(0),out);
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetStandalone,1) {
  return OZ_unify(OZ_getCArg(0),am.isStandalone() ? NameTrue : NameFalse);
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetHome,1) {
  return OZ_unifyAtom(OZ_getCArg(0),ozconf.ozHome);
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemGetPlatform,1) {
  return OZ_unify(OZ_getCArg(0),
		  OZ_pair2(OZ_atom(ozconf.osname),OZ_atom(ozconf.cpu)));
}
OZ_C_proc_end

#undef GetRecord
#undef SetTaggedArg
#undef SetIntArg
#undef SetBoolArg


#define LookRecord(t) \
  OZ_Term t = OZ_getCArg(0);            \
  DEREF(t, tPtr, tTag);                 \
  if (isAnyVar(tTag))                   \
    OZ_suspendOn(makeTaggedRef(tPtr));  \
  if (isLiteral(tTag))                  \
    return PROCEED;                     \
  if (!isSRecord(t))                    \
    TypeErrorT(0, "Record");            

#define DoPercentFeature(var,term,atom) \
  int     var = -1;                                              \
  { OZ_Term out = tagged2SRecord(term)->getFeature(atom);        \
    if (out) {                                                   \
      DEREF(out, outPtr, outTag);                                \
      if (isAnyVar(outTag)) OZ_suspendOn(makeTaggedRef(outPtr)); \
      if (!isSmallInt(outTag) ||                                 \
	  (var=smallIntValue(out)) < 1 || var > 100)             \
	 TypeErrorT(0, "Int");                                   \
    }                                                            \
  }
          
#define DoNatFeature(var,term,atom) \
  int     var = -1;                                                       \
  { OZ_Term out = tagged2SRecord(term)->getFeature(atom);                 \
    if (out) {                                                            \
      DEREF(out, outPtr, outTag);                                         \
      if (isAnyVar(outTag)) OZ_suspendOn(makeTaggedRef(outPtr));          \
      if (isSmallInt(outTag) && smallIntValue(out)>=0) {                  \
        var = smallIntValue(out);                                         \
      } else if (isBigInt(outTag) && tagged2BigInt(out)->getInt()>=0) {   \
	var = tagged2BigInt(out)->getInt();                               \
      } else {                                                            \
	TypeErrorT(0, "Int");                                             \
      }                                                                   \
    }                                                                     \
  }
          
#define DoBoolFeature(var,term,atom) \
  int     var = -1;                                                       \
  { OZ_Term out = tagged2SRecord(term)->getFeature(atom);                 \
    if (out) {                                                            \
      DEREF(out, outPtr, outTag);                                         \
      if (isAnyVar(outTag)) OZ_suspendOn(makeTaggedRef(outPtr));          \
      if (isLiteral(outTag))                                              \
	if (literalEq(out,NameTrue)) {                                    \
	  var = 1;                                                        \
	} else if (literalEq(out,NameFalse)) {                            \
	  var = 0;                                                        \
	} else {                                                          \
	  TypeErrorT(0, "Bool");                                          \
	}                                                                 \
    }                                                                     \
  }
          
#define SetIfPos(left,right,scale) if (right >= 0) left = right / scale;

OZ_C_proc_begin(BISystemSetPriorities,1) {
  LookRecord(t);

  DoPercentFeature(high,   t, AtomHigh);
  DoPercentFeature(medium, t, AtomMedium);

  SetIfPos(ozconf.hiMidRatio,  high,   1);
  SetIfPos(ozconf.midLowRatio, medium, 1);
  
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemSetGC,1) {
  LookRecord(t);

  DoNatFeature(max_size,      t, AtomMax);
  DoNatFeature(min_size,      t, AtomMin);
  DoPercentFeature(free,      t, AtomFree);
  DoPercentFeature(tolerance, t, AtomTolerance);
  DoBoolFeature(active,       t, AtomOn);

  SetIfPos(ozconf.heapMaxSize,    max_size,  KB);
  SetIfPos(ozconf.heapMinSize,    min_size,  KB);

  SetIfPos(ozconf.heapFree,       free,      1);
  SetIfPos(ozconf.heapTolerance,  tolerance, 1);
  SetIfPos(ozconf.gcFlag,         active,    1);

  if (ozconf.heapMinSize > ozconf.heapMaxSize) 
    ozconf.heapMinSize = ozconf.heapMaxSize;

  if (ozconf.heapMinSize > ozconf.heapThreshold) 
    ozconf.heapThreshold = ozconf.heapMinSize;

  if (ozconf.heapThreshold > ozconf.heapMaxSize) {
    am.setSFlag(StartGC);
    return BI_PREEMPT;
  }

  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemSetPrint,1) {
  LookRecord(t);

  DoNatFeature(width, t, AtomWidth);
  DoNatFeature(depth, t, AtomDepth);

  SetIfPos(ozconf.printWidth, width, 1);
  SetIfPos(ozconf.printDepth, depth, 1);

  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemSetFD,1) {
  LookRecord(t);

  DoNatFeature(threshold, t, AtomThreshold);
  
  reInitFDs(threshold);

  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemSetErrors,1) {
  LookRecord(t);

  DoBoolFeature(location, t, AtomLocation);
  DoBoolFeature(hints,    t, AtomHints);
  DoBoolFeature(debug,    t, AtomDebug);
  DoNatFeature(thread,    t, AtomThread);
  DoNatFeature(width,     t, AtomWidth);
  DoNatFeature(depth,     t, AtomDepth);

  SetIfPos(ozconf.errorThreadDepth, thread,   1);
  SetIfPos(ozconf.errorLocation,    location, 1);
  SetIfPos(ozconf.errorDebug,       debug,    1);
  SetIfPos(ozconf.errorHints,       hints,    1);
  SetIfPos(ozconf.errorPrintWidth,  width,    1);
  SetIfPos(ozconf.errorPrintDepth,  depth,    1);

  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BISystemSetMessages,1) {
  LookRecord(t);

  DoBoolFeature(gc,      t, AtomGC);
  DoBoolFeature(idle,    t, AtomIdle);
  DoBoolFeature(feed,    t, AtomFeed);
  DoBoolFeature(foreign, t, AtomForeign);

  SetIfPos(ozconf.gcVerbosity,     gc,      1);
  SetIfPos(ozconf.showIdleMessage, idle,    1);
  SetIfPos(ozconf.showFastLoad,    feed,    1);
  SetIfPos(ozconf.showForeignLoad, foreign, 1);

  return PROCEED;
}
OZ_C_proc_end


OZ_C_proc_begin(BISystemSetInternal,1) {
  LookRecord(t);

  DoBoolFeature(debugmode,  t, AtomDebug);
  DoBoolFeature(suspension, t, AtomShowSuspension);
  DoBoolFeature(stop,       t, AtomStopOnToplevelFailure);
  DoBoolFeature(cell,       t, AtomCellHack);
  DoNatFeature(stack,       t, AtomStackMaxSize);
  DoNatFeature(debugIP,     t, AtomDebugIP);
  DoNatFeature(debugPerdio, t, AtomDebugPerdio);

  if (debugmode == 0) {
    am.unsetSFlag(DebugMode);
  } else if (debugmode == 1) {
    am.setSFlag(DebugMode);
  }
    
  SetIfPos(ozconf.showSuspension,        suspension, 1);
  SetIfPos(ozconf.stopOnToplevelFailure, stop,       1);
  SetIfPos(ozconf.cellHack,              cell,       1);
  SetIfPos(ozconf.stackMaxSize,          stack,      KB);
  SetIfPos(ozconf.debugIP,               debugIP,    1);
  SetIfPos(ozconf.debugPerdio,           debugPerdio,1);

  return PROCEED;
}
OZ_C_proc_end


#undef LookRecord
#undef DoPercentFeature
#undef DoNatFeature
#undef DoBoolFeature
#undef SetIfPos


// ---------------------------------------------------------------------------

OZ_C_proc_begin(BIonToplevel,1)
{
  OZ_declareArg(0,out);

  return OZ_unify(out,OZ_onToplevel() ? NameTrue : NameFalse);
}
OZ_C_proc_end

OZ_C_proc_begin(BIaddr,2)
{
  OZ_declareArg(0,val);
  OZ_declareArg(1,out);

  DEREF(val,valPtr,valTag);
  if (valPtr) {
    return OZ_unifyInt(out,ToInt32(valPtr));
  }
  return OZ_unifyInt(out,ToInt32(tagValueOf(valTag,val)));
}
OZ_C_proc_end

OZ_C_proc_begin(BIsuspensions,2)
{
  OZ_declareArg(0,val);
  OZ_declareArg(1,out);

  DEREF(val,valPtr,valTag);
  switch (valTag) {
  case UVAR:
    return OZ_unify(out,nil());
  case SVAR:
  case CVAR:
    return OZ_unify(out,tagged2SuspVar(val)->DBGmakeSuspList());
  default:
    return OZ_unify(out,nil());
  }
}
OZ_C_proc_end

OZ_C_proc_begin(BIglobals,2)
{
  OZ_declareNonvarArg(0,proc);
  OZ_declareArg(1,out);

  DEREF(proc,ptr,tag);
  if (isConst(proc)) {
    ConstTerm *cc = tagged2Const(proc);
    switch (cc->getType()) {
    case Co_Builtin:
      return OZ_unify(out,nil());
    case Co_Abstraction:
      return OZ_unify(out, ((Abstraction *) cc)->DBGgetGlobals());
    case Co_Object:
      return OZ_unify(out,
		      ((Object *) cc)->getAbstraction()->DBGgetGlobals());
    default:
      break;
    }
  }
  TypeErrorT(1,"Procedure or Object");
}
OZ_C_proc_end


// ---------------------------------------------------------------------------
// Debugging: set a break
// ---------------------------------------------------------------------------

OZ_C_proc_begin(BIhalt,0)
{
  tracerOn();
  return PROCEED;
}
OZ_C_proc_end

// ---------------------------------------------------------------------------
// Debugging: special builtins for Benni
// ---------------------------------------------------------------------------

OZ_C_proc_begin(BIglobalThreadStream,1)
{
  return OZ_unify(OZ_getCArg(0), am.threadStreamTail);
}
OZ_C_proc_end

OZ_C_proc_begin(BIcurrentThread,1)
{
  return OZ_unify(OZ_getCArg(0),
		  makeTaggedConst(am.currentThread));
}
OZ_C_proc_end

OZ_C_proc_begin(BIsetStepMode,2)
{
  OZ_Term chunk = deref(OZ_getCArg(0));
  char   *onoff = toC(OZ_getCArg(1));
  
  ConstTerm *rec = tagged2Const(chunk);
  Thread *thread = (Thread*) rec;
 
  if (!strcmp(onoff, "on"))
    thread->startStepMode();
  else if (!strcmp(onoff, "off"))
    thread->stopStepMode();
  else warning("setStepMode: invalid second argument: must be 'on' or 'off'");
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BIstopThread,1)
{
  OZ_Term chunk  = deref(OZ_getCArg(0));
  ConstTerm *rec = tagged2Const(chunk);
  Thread *thread = (Thread*) rec;
  
  thread->stop();
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BIcontThread,1)
{
  OZ_Term chunk  = deref(OZ_getCArg(0));
  ConstTerm *rec = tagged2Const(chunk);
  Thread *thread = (Thread*) rec;
  
  thread->cont();
  am.scheduleThread(thread);
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BIqueryDebugState,2)
{
  OZ_Term chunk = deref(OZ_getCArg(0));
  OZ_Term out   = OZ_getCArg(1);
  
  ConstTerm *rec = tagged2Const(chunk);
  Thread *thread = (Thread*) rec;

  return OZ_unify(out, OZ_mkTupleC("debugState",
				   3,
				   thread->isTraced()  ? OZ_atom("Traced")
				                       : OZ_atom("notTraced"),
				   thread->stepMode()  ? OZ_atom("stepON")
				                       : OZ_atom("stepOFF"),
				   thread->stopped()   ? OZ_atom("stopped")
				                       : OZ_atom("running")
				   ));
			      
}
OZ_C_proc_end


/* ------- Builtins to handle toplevel variables in the debugger ---------- */

OZ_C_proc_begin(BItopVarInfo,2) // needs work --BL
{
  OZ_Term in  = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);
  
  char *name = OZ_atomToC(in);
  return OZ_unify(out, nil());
}
OZ_C_proc_end   

OZ_C_proc_begin(BItopVars,2) // needs work --BL
{
  OZ_Term in = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);
  OZ_Term VarList = nil();

  return OZ_unify(out, VarList);
}
OZ_C_proc_end   

OZ_C_proc_begin(BIindex2Tagged,2)
{
  OZ_Term in  = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);

  if (!OZ_isSmallInt(in)) {
    OZ_warning("Invalid index for builtin `index2Tagged'");
    return OZ_unify(out, nil());
  }
  if (OZ_intToC(in) > am.toplevelVarsCount) {
    OZ_warning("Index too big for builtin `index2Tagged'");
    return OZ_unify(out, nil());
  }
  return OZ_unify(out, am.toplevelVars[OZ_intToC(in)]);
}
OZ_C_proc_end   


extern OZ_Term make_time(const struct tm*);  // defined in unix.cc

OZ_C_proc_begin(BItime2localTime,2)
{
  OZ_Term in  = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);
  
  if (!OZ_isInt(in)) {
    OZ_warning("Invalid first argument for builtin `time2localTime'");
    return OZ_unify(out, nil());
  }
  else {
    time_t time = time_t(OZ_intToC(in));
    return OZ_unify(out, make_time(localtime(&time)));
  }
}
OZ_C_proc_end

// ---------------------------------------------------------------------------


OZ_C_proc_begin(BIshowBuiltins,0)
{
  builtinTab.print();
  return(PROCEED);
}
OZ_C_proc_end

OZ_C_proc_begin(BItraceBack, 0)
{
  am.currentThread->printTaskStack(NOCODE);
  return PROCEED;
}
OZ_C_proc_end

// ---------------------------------------------------------------------------

OZ_C_proc_begin(BIforeignFDProps, 1)
{
#ifdef FOREIGNFDPROPS
  return OZ_unify(OZ_args[0], NameTrue);
#else
  return OZ_unify(OZ_args[0], NameFalse);
#endif
}
OZ_C_proc_end

// ---------------------------------------------------------------------------

#ifdef PARSER
OZ_C_proc_proto(ozparser_parse)
OZ_C_proc_proto(ozparser_init)
OZ_C_proc_proto(ozparser_exit)
#else
OZ_C_proc_begin(ozparser_parse,2)
return PROCEED;
OZ_C_proc_end
OZ_C_proc_begin(ozparser_init,0)
return PROCEED;
OZ_C_proc_end
OZ_C_proc_begin(ozparser_exit,0)
return PROCEED;
OZ_C_proc_end
#endif


// ---------------------------------------------------------------------
// OO Stuff
// ---------------------------------------------------------------------

/*
 *	Construct a new SRecord to be a copy of old.
 *	This is the functionality of adjoin(old,newlabel).
 */
OZ_C_proc_begin(BIcopyRecord,2)
{
  OZ_Term in = OZ_getCArg(0);
  OZ_Term out = OZ_getCArg(1);
  NONVAR1(in,rec);

  
  switch (tagTypeOf(rec)) {
  case SRECORD:
    {
      SRecord *rec0 = tagged2SRecord(rec);
      SRecord *rec1 = SRecord::newSRecord(rec0);
      return OZ_unify(out,makeTaggedSRecord(rec1));
    }
  case LITERAL:
    return OZ_unify(out,rec);

  default:
    TypeErrorT(0,"Determined Record");
  }
}
OZ_C_proc_end

#define CheckSelf Assert(am.getSelf()!=NULL);


OZ_Return atInline(TaggedRef fea, TaggedRef &out)
{
  DEREF(fea, _1, feaTag);

  SRecord *rec = am.getSelf()->getState();
  if (rec) {
    if (!isFeature(fea)) {
      if (isAnyVar(fea)) {
	return SUSPEND;
      }
      goto bomb;
    }
    CheckSelf;
    TaggedRef t = rec->getFeature(fea);
    if (t) {
      out = t;
      return PROCEED;
    }
  }

bomb:
  TypeErrorT(1,"(valid) Feature");
}
DECLAREBI_USEINLINEFUN1(BIat,atInline)

OZ_Return assignInline(TaggedRef fea, TaggedRef value)
{
  DEREF(fea, _2, feaTag);

  SRecord *r = am.getSelf()->getState();

  CheckSelf;
  if (!isFeature(fea)) {
    if (isAnyVar(fea)) {
      return SUSPEND;
    }
    TypeErrorT(0,"Feature");
  }

  if (!r) return am.raise(E_ERROR,E_OBJECT,"<-",3,nil(),fea,value);

  if (r->replaceFeature(fea,value) == makeTaggedNULL()) {
    return am.raise(E_ERROR,E_OBJECT,"<-",3,makeTaggedSRecord(r),fea,value);
  }

  return PROCEED;
}

DECLAREBI_USEINLINEREL2(BIassign,assignInline)

OZ_Return ooExchInline(TaggedRef fea, TaggedRef newAttr, TaggedRef &oldAttr)
{
  DEREF(fea, _1, feaTag);

  SRecord *rec = am.getSelf()->getState();
  if (rec) {
    if (!isFeature(feaTag)) {
      if (isAnyVar(feaTag)) {
	return SUSPEND;
      }
      goto bomb;
    }
    CheckSelf;
    TaggedRef aux = rec->getFeature(fea);
    if (aux) {
      oldAttr = aux;
      aux = rec->replaceFeature(fea,newAttr);
      Assert(aux!=makeTaggedNULL());      
      return PROCEED;
    }
  }

bomb:
  TypeErrorT(1,"(valid) Feature");
}
DECLAREBI_USEINLINEFUN2(BIooExch,ooExchInline)

Object *newObject(SRecord *feat, SRecord *st, ObjectClass *cla, 
		  Bool iscl, Board *b)
{
  Bool deep = (b!=am.rootBoard);
  OzLock *lck = cla->supportsLocking() ? new OzLock(am.currentBoard) : (OzLock*) NULL;
  Object *ret = deep
    ? new DeepObject(st,cla,feat,iscl,b,lck)
    : new Object(st,cla,feat,iscl,lck);
  return ret;
}


OZ_C_proc_begin(BImakeClass,9)
{
  OZ_Term fastmeth   = OZ_getCArg(0); { DEREF(fastmeth,_1,_2); }
  OZ_Term printname  = OZ_getCArg(1); { DEREF(printname,_1,_2); }
  OZ_Term slowmeth   = OZ_getCArg(2); { DEREF(slowmeth,_1,_2); }
  OZ_Term send       = OZ_getCArg(3); { DEREF(send,_1,_2); }
  OZ_Term features   = OZ_getCArg(4); { DEREF(features,_1,_2); }
  OZ_Term ufeatures  = OZ_getCArg(5); { DEREF(ufeatures,_1,_2); }
  OZ_Term defmethods = OZ_getCArg(6); { DEREF(defmethods,_1,_2); }
  OZ_Term locking    = OZ_getCArg(7); { DEREF(locking,_1,_2); }
  OZ_Term out        = OZ_getCArg(8);

  SRecord *methods = NULL;

  if (!isDictionary(fastmeth))   { TypeErrorT(0,"dictionary"); }
  if (!isLiteral(printname))     { TypeErrorT(1,"literal"); }
  if (!isDictionary(slowmeth))   { TypeErrorT(2,"dictionary"); }
  if (!isAbstraction(send))      { TypeErrorT(3,"abstraction"); }
  if (!isRecord(features))       { TypeErrorT(4,"record"); }
  if (!isRecord(ufeatures))      { TypeErrorT(5,"record"); }
  if (!isDictionary(defmethods)) { TypeErrorT(6,"dictionary"); }

  SRecord *uf = isSRecord(ufeatures) ? tagged2SRecord(ufeatures) : (SRecord*)NULL;

  ObjectClass *cl = new ObjectClass(tagged2Dictionary(fastmeth),
				    tagged2Literal(printname),
				    tagged2Dictionary(slowmeth),
				    tagged2Abstraction(send),
				    uf,
				    tagged2Dictionary(defmethods),
				    locking==NameTrue);

  Object *reto = newObject(tagged2SRecord(features),
			   NULL, // initState
			   cl,
			   OK,
			   am.currentBoard);
  TaggedRef ret   = makeTaggedConst(reto);
  cl->setOzClass(ret);
  return OZ_unify(out,ret);
}
OZ_C_proc_end


OZ_C_proc_begin(BIsetMethApplHdl,1)
{
  OZ_Term preed = OZ_getCArg(0); DEREF(preed,_1,_2);
  if (! isAbstraction(preed)) {
    TypeErrorT(0,"abstraction");
  }

  if (am.methApplHdl != makeTaggedNULL()) {
    // warning("setMethApplHdl called twice (hint: prelude may not be fed twice)");
  }

  am.methApplHdl = preed;
  return PROCEED;
}
OZ_C_proc_end


OZ_Return BIisObjectInline(TaggedRef t)
{ 
  DEREF(t,_1,_2);
  if (isAnyVar(t)) return SUSPEND;
  if (!isObject(t)) {
    return FAILED;
  }
  Object *obj = (Object *) tagged2Const(t);
  return obj->isClass() ? FAILED : PROCEED;
}

DECLAREBI_USEINLINEREL1(BIisObject,BIisObjectInline)
DECLAREBOOLFUN1(BIisObjectB,BIisObjectBInline,BIisObjectInline)

OZ_Return BIisClassInline(TaggedRef t)
{ 
  DEREF(t,_1,_2);
  if (isAnyVar(t)) return SUSPEND;
  if (!isObject(t)) {
    return FAILED;
  }
  Object *obj = (Object *) tagged2Const(t);
  return obj->isClass() ? PROCEED : FAILED;
}

DECLAREBI_USEINLINEREL1(BIisClass,BIisClassInline)
DECLAREBOOLFUN1(BIisClassB,BIisClassBInline,BIisClassInline)


/* getClass(t) returns class of t, if t is an object
 * otherwise return t!
 */
OZ_Return getClassInline(TaggedRef t, TaggedRef &out)
{ 
  DEREF(t,_,tag);
  if (isAnyVar(tag)) return SUSPEND;
  if (!isObject(t)) {
    out = t;
    return PROCEED;
  }
  out = ((Object *)tagged2Const(t))->getOzClass();
  return PROCEED;
}

DECLAREBI_USEINLINEFUN1(BIgetClass,getClassInline)




inline
TaggedRef cloneObjectRecord(TaggedRef record, Bool cloneAll)
{
  if (isLiteral(record))
    return record;

  Assert(isSRecord(record));

  SRecord *in  = tagged2SRecord(record);
  SRecord *rec = SRecord::newSRecord(in);

  OZ_Term proto = am.currentUVarPrototype;

  for(int i=0; i < in->getWidth(); i++) {
    OZ_Term arg = in->getArg(i);
    if (cloneAll || literalEq(NameOoFreeFlag,deref(arg))) {
      arg = makeTaggedRef(newTaggedUVar(proto));
    }
    rec->setArg(i,arg);
  }

  return makeTaggedSRecord(rec);
}

inline
OZ_Term makeObject(OZ_Term initState, OZ_Term ffeatures, ObjectClass *clas)
{
  Assert(isRecord(initState) && isRecord(ffeatures));

  Object *out = 
    newObject(isSRecord(ffeatures) ? tagged2SRecord(ffeatures) : (SRecord*) NULL,
	      isSRecord(initState) ? tagged2SRecord(initState) : (SRecord*) NULL,
	      clas,
	      NO,
	      am.currentBoard);
  
  //  out->setLocked();
  return makeTaggedConst(out);
}


OZ_Return newObjectInline(TaggedRef cla, TaggedRef &out)
{ 
  { DEREF(cla,_1,_2); }
  if (isAnyVar(cla)) return SUSPEND;
  if (!isObject(cla)) {
    TypeErrorT(0,"Class");
  }

  Object *obj = (Object *)tagged2Const(cla);
  TaggedRef realclass = obj->getOzClass();
  { DEREF(realclass,_1,_2); }
  Assert(isObject(realclass));

  Object *realcl = (Object *)tagged2Const(realclass);
  TaggedRef attr = realcl->getFeature(NameOoAttr);
  { DEREF(attr,_1,_2); }
  if (isAnyVar(attr)) return SUSPEND;
  
  TaggedRef attrclone = cloneObjectRecord(attr,NO);

  TaggedRef freefeat = realcl->getFeature(NameOoFreeFeatR);
  { DEREF(freefeat,_1,_2); }
  Assert(!isAnyVar(freefeat));
  TaggedRef freefeatclone = cloneObjectRecord(freefeat,OK);

  out = makeObject(attrclone, freefeatclone, obj->getClass());

  return PROCEED;
}

DECLAREBI_USEINLINEFUN1(BInewObject,newObjectInline)



OZ_C_proc_begin(BIgetOONames,5)
{
  if (OZ_unify(OZ_getCArg(0),NameOoAttr)       &&
      OZ_unify(OZ_getCArg(1),NameOoFreeFeatR)  &&
      OZ_unify(OZ_getCArg(2),NameOoFreeFlag)   &&
      OZ_unify(OZ_getCArg(3),NameOoDefaultVar) &&
      OZ_unify(OZ_getCArg(4),NameOoRequiredArg))
    return PROCEED;
  return FAILED;
}
OZ_C_proc_end



OZ_C_proc_begin(BIgetSelf,1)
{
  return OZ_unify(makeTaggedConst(am.getSelf()),
		  OZ_getCArg(0));
}
OZ_C_proc_end


OZ_C_proc_begin(BIsetSelf,1)
{
  OZ_Term obj = OZ_getCArg(0);
  obj = deref(obj);
  if (!isObject(obj)) {
    TypeErrorT(0,"Object");
  }
  am.changeSelf(tagged2Object(obj));
  return PROCEED;
}
OZ_C_proc_end


OZ_Return ooGetLockInline(TaggedRef val)
{ 
  OzLock *lock = am.getSelf()->getLock();
  if (lock==NULL)
    return am.raise(E_ERROR,E_OBJECT,"locking",1,makeTaggedConst(am.getSelf()));

  return am.fastUnify(val,makeTaggedConst(lock),OK) ? PROCEED : FAILED;
}
DECLAREBI_USEINLINEREL1(BIooGetLock,ooGetLockInline)


/********************************************************************
 * Exceptions
 ******************************************************************** */

OZ_C_proc_begin(BIsetDefaultExceptionHandler,1)
{
  OZ_declareNonvarArg(0,hdl);
  if (!OZ_isProcedure(hdl)) TypeErrorT(0,"Procedure");

  hdl = deref(hdl);
  if (tagged2Const(hdl)->getArity() != 1) TypeErrorT(0,"Procedure/1");
  am.defaultExceptionHandler = hdl;
  return PROCEED;
}
OZ_C_proc_end

/*
 * the builtin exception handler
 */
OZ_C_proc_begin(BIbiExceptionHandler,1)
{
  OZ_Term arg=OZ_getCArg(0);

  errorHeader();
  message("UNCAUGHT EXCEPTION: %s\n",toC(arg));
  errorTrailer();

  return am.isToplevel() ? PROCEED : FAILED;
}
OZ_C_proc_end

OZ_C_proc_begin(BIraise,1)
{
  OZ_declareArg(0,exc);

  return OZ_raise(exc);
}
OZ_C_proc_end

OZ_C_proc_begin(BIraiseError,1)
{
  OZ_declareArg(0,exc);

  OZ_Term ret = OZ_record(OZ_atom("error"),
			  cons(OZ_int(1),
			       cons(OZ_atom("debug"),nil())));
  OZ_putSubtree(ret,OZ_int(1),exc);
  OZ_putSubtree(ret,OZ_atom("debug"),NameUnit);

  am.exception.info = NameUnit;
  am.exception.value = ret;
  am.exception.debug = TRUE;
  return RAISE;
}
OZ_C_proc_end

#endif /* BUILTINS2 */


/********************************************************************
 * Table of builtins
 ******************************************************************** */


#ifdef BUILTINS2
extern BIspec allSpec1[];
#endif

#ifdef BUILTINS1

BIspec allSpec1[] = {
  {"/",   3, BIfdiv,     (IFOR) BIfdivInline},
  {"*",   3, BImult,     (IFOR) BImultInline},
  {"div", 3, BIdiv,      (IFOR) BIdivInline},
  {"mod", 3, BImod,      (IFOR) BImodInline},
  {"-",   3, BIminus,    (IFOR) BIminusInline},
  {"+",   3, BIplus,     (IFOR) BIplusInline},
  {"Max", 3, BImax,      (IFOR) BImaxInline},
  {"Min", 3, BImin,      (IFOR) BIminInline},
  
  {"<",    3, BIlessFun,   (IFOR) BIlessInlineFun},
  {"=<",   3, BIleFun,     (IFOR) BIleInlineFun},
  {">",    3, BIgreatFun,  (IFOR) BIgreatInlineFun},
  {">=",   3, BIgeFun,     (IFOR) BIgeInlineFun},
  {"=:=",  2, BInumeqFun,  (IFOR) BInumeqInlineFun},
  {"=\\=", 2, BInumneqFun, (IFOR) BInumneqInlineFun},
  
  {"=<Rel",   2, BIle,     (IFOR) BIleInline},
  {"<Rel",    2, BIless,   (IFOR) BIlessInline},
  {">=Rel",   2, BIge,     (IFOR) BIgeInline},
  {">Rel",    2, BIgreat,  (IFOR) BIgreatInline},
  {"=:=Rel",  2, BInumeq,  (IFOR) BInumeqInline},
  {"=\\=Rel", 2, BInumneq, (IFOR) BInumneqInline},
  
  {"~",  2, BIuminus,   (IFOR) BIuminusInline},
  {"+1", 2, BIadd1,     (IFOR) BIadd1Inline},
  {"-1", 2, BIsub1,     (IFOR) BIsub1Inline},
  
  {"Exp",   2, BIexp,   (IFOR) BIinlineExp},
  {"Log",   2, BIlog,   (IFOR) BIinlineLog},
  {"Sqrt",  2, BIsqrt,  (IFOR) BIinlineSqrt},
  {"Sin",   2, BIsin,   (IFOR) BIinlineSin},
  {"Asin",  2, BIasin,  (IFOR) BIinlineAsin},
  {"Cos",   2, BIcos,   (IFOR) BIinlineCos},
  {"Acos",  2, BIacos,  (IFOR) BIinlineAcos},
  {"Tan",   2, BItan,   (IFOR) BIinlineTan},
  {"Atan",  2, BIatan,  (IFOR) BIinlineAtan},
  {"Ceil",  2, BIceil,  (IFOR) BIinlineCeil},
  {"Floor", 2, BIfloor, (IFOR) BIinlineFloor},
  {"Abs",   2, BIabs,   (IFOR) BIabsInline},
  {"Round", 2, BIround, (IFOR) BIinlineRound},
  {"Atan2", 3, BIatan2, (IFOR) BIatan2Inline},

  {"fPow",  3, BIfPow, (IFOR) BIfPowInline},

  /* conversion: float <-> int <-> virtualStrings */
  {"IntToFloat", 2, BIintToFloat,  (IFOR) BIintToFloatInline},
  {"FloatToInt", 2, BIfloatToInt,  (IFOR) BIfloatToIntInline},

  {"IntToString",    2, BIintToString,		0},
  {"FloatToString",  2, BIfloatToString,	0},
  {"StringToInt",    2, BIstringToInt,		0},
  {"StringToFloat",  2, BIstringToFloat,	0},
  {"String.isInt",   2, BIstringIsInt,		0},
  {"String.isFloat", 2, BIstringIsFloat,	0},
  {"String.isAtom",  2, BIstringIsAtom,	        0},

  {"IsArray",    2, BIisArray,   (IFOR) isArrayInline},
  {"NewArray",   4, BIarrayNew,	 0},
  {"Array.high", 2, BIarrayHigh, (IFOR) arrayHighInline},
  {"Array.low",  2, BIarrayLow,  (IFOR) arrayLowInline},
  {"Get",        3, BIarrayGet,  (IFOR) arrayGetInline},
  {"Put",        3, BIarrayPut,  (IFOR) arrayPutInline},

  {"NewDictionary",      1, BIdictionaryNew,	0},
  {"IsDictionary",       2, BIisDictionary,     (IFOR) isDictionaryInline},
  {"Dictionary.get",     3, BIdictionaryGet,    (IFOR) dictionaryGetInline},
  {"Dictionary.condGet", 4, BIdictionaryGetIf,  (IFOR) dictionaryGetIfInline},
  {"Dictionary.put",     3, BIdictionaryPut,    (IFOR) dictionaryPutInline},
  {"Dictionary.deepPut", 3, BIdictionaryDeepPut,(IFOR) dictionaryDeepPutInline},
  {"Dictionary.remove",  2, BIdictionaryRemove, (IFOR) dictionaryRemoveInline},
  {"Dictionary.member",  3, BIdictionaryMember, (IFOR) dictionaryMemberInline},
  {"Dictionary.keys",    2, BIdictionaryKeys,    0},
  {"Dictionary.entries", 2, BIdictionaryEntries, 0},

  {"NewLock",	      1,BInewLock,	 0},
  {"Lock",	      1,BIlockLock,	 0},
  {"Unlock",	      1,BIunlockLock,	 0},

  {"NewPort",	      2,BInewPort,	 0},
  {"Send",	      2,BIsendPort,	 0},
  {"Port.close",      1,BIclosePort,	 0},

  {"NewCell",	      2,BInewCell,	 0},
  {"Exchange",        3,BIexchangeCell, (IFOR) BIexchangeCellInline},
  {"Access",          2,BIaccessCell,   (IFOR) BIaccessCellInline},
  {"Assign",          2,BIassignCell,   (IFOR) BIassignCellInline},

  {"IsChar",        2, BIcharIs,	0},
  {"Char.isAlNum",  2, BIcharIsAlNum,	0},
  {"Char.isAlpha",  2, BIcharIsAlpha,	0},
  {"Char.isCntrl",  2, BIcharIsCntrl,	0},
  {"Char.isDigit",  2, BIcharIsDigit,	0},
  {"Char.isGraph",  2, BIcharIsGraph,	0},
  {"Char.isLower",  2, BIcharIsLower,	0},
  {"Char.isPrint",  2, BIcharIsPrint,	0},
  {"Char.isPunct",  2, BIcharIsPunct,	0},
  {"Char.isSpace",  2, BIcharIsSpace,	0},
  {"Char.isUpper",  2, BIcharIsUpper,	0},
  {"Char.isXDigit", 2, BIcharIsXDigit,	0},
  {"Char.toLower",  2, BIcharToLower,	0},
  {"Char.toUpper",  2, BIcharToUpper,	0},
  {"Char.toAtom",   2, BIcharToAtom,	0},
  {"Char.type",     2, BIcharType,	0},

  {"Adjoin",          3, BIadjoin,           (IFOR) BIadjoinInline},
  {"AdjoinList",      3, BIadjoinList,       0},
  {"record",          3, BImakeRecord,       0},
  {"Arity",           2, BIarity,            (IFOR) BIarityInline},
  {"AdjoinAt",        4, BIadjoinAt,         0},

  {0,0,0,0}
};
#endif

#ifdef BUILTINS2

BIspec allSpec2[] = {
  {"IsNumber",        2, BIisNumberB,    (IFOR) BIisNumberBInline},
  {"IsInt"   ,        2, BIisIntB,       (IFOR) BIisIntBInline},
  {"IsFloat" ,        2, BIisFloatB,     (IFOR) BIisFloatBInline},
  {"IsRecord",        2, BIisRecordB,    (IFOR) isRecordBInline},
  {"IsTuple",         2, BIisTupleB,     (IFOR) isTupleBInline},
  {"IsLiteral",       2, BIisLiteralB,   (IFOR) isLiteralBInline},
  {"IsLock",          2, BIisLockB,      (IFOR) isLockBInline},
  {"IsCell",          2, BIisCellB,      (IFOR) isCellBInline},
  {"IsPort",          2, BIisPortB,      (IFOR) isPortBInline},
  {"IsProcedure",     2, BIisProcedureB, (IFOR) isProcedureBInline},
  {"IsName",          2, BIisNameB,      (IFOR) isNameBInline},
  {"IsAtom",          2, BIisAtomB,      (IFOR) isAtomBInline},
  {"IsBool",          2, BIisBoolB,      (IFOR) isBoolBInline},
  {"IsUnit",          2, BIisUnitB,      (IFOR) isUnitBInline},
  {"IsChunk",         2, BIisChunkB,     (IFOR) isChunkBInline},
  {"IsRecordC",       2, BIisRecordCB,   (IFOR) isRecordCBInline},
  {"IsObject",        2, BIisObjectB,    (IFOR) BIisObjectBInline},
  {"IsClass",         2, BIisClassB,     (IFOR) BIisClassBInline},
  {"IsString",        2, BIisString,     0},
  {"IsVirtualString", 2, BIvsIs,         0},
  {"IsFree",          2, BIisFree,       (IFOR) isFreeInline},
  {"IsKinded",        2, BIisKinded,     (IFOR) isKindedInline},
  {"IsDet",           2, BIisDet,        (IFOR) isDetInline},

  {"isNumberRel",    1, BIisNumber,    (IFOR) BIisNumberInline},
  {"isIntRel"   ,    1, BIisInt,       (IFOR) BIisIntInline},
  {"isFloatRel" ,    1, BIisFloat,     (IFOR) BIisFloatInline},
  {"isRecordRel",    1, BIisRecord,    (IFOR) isRecordInline},
  {"isTupleRel",     1, BIisTuple,     (IFOR) isTupleInline},
  {"isLiteralRel",   1, BIisLiteral,   (IFOR) isLiteralInline},
  {"isCellRel",      1, BIisCell,      (IFOR) isCellInline},
  {"isPortRel",      1, BIisPort,      (IFOR) isPortInline},
  {"isProcedureRel", 1, BIisProcedure, (IFOR) isProcedureInline},
  {"isNameRel",      1, BIisName,      (IFOR) isNameInline},
  {"isAtomRel",      1, BIisAtom,      (IFOR) isAtomInline},
  {"isLockRel",      1, BIisLock,      (IFOR) isLockInline},
  {"isBoolRel",      1, BIisBool,      (IFOR) isBoolInline},
  {"isUnitRel",      1, BIisUnit,      (IFOR) isUnitInline},
  {"isChunkRel",     1, BIisChunk,     (IFOR) isChunkInline},
  {"isRecordCRel",   1, BIisRecordC,   (IFOR) isRecordCInline},
  {"isClassRel",     1, BIisClass,     (IFOR) BIisClassInline},
  {"isObjectRel",    1, BIisObject,    (IFOR) BIisObjectInline},
  {"IsFreeRel",      1, BIisFreeRel,   (IFOR) isFreeRelInline},
  {"IsKindedRel",    1, BIisKindedRel, (IFOR) isKindedRelInline},
  {"IsDetRel",       1, BIisDetRel,    (IFOR) isDetRelInline},

  {"Wait",   1, BIisValue,            (IFOR) isValueInline},
  {"WaitOr", 2, BIwaitOr,             (IFOR) 0},


  {"virtualStringLength",  3, BIvsLength, 0},

  {"getUnit", 1,BIgetUnit,  	   0},

  {"getTrue", 1,BIgetTrue,  	   0},
  {"getFalse",1,BIgetFalse,  	   0},
  {"Not",     2,BInot,             (IFOR) notInline},
  {"And",     3,BIand,             (IFOR) andInline},
  {"Or",      3,BIor,              (IFOR) orInline},

  {"Type.ofValue", 2, BItermType,        (IFOR) BItermTypeInline},
  {"Value.status", 2, BIstatus,       	 (IFOR) BIstatusInline},

  {"ProcedureArity",2,BIprocedureArity,	 (IFOR)procedureArityInline},
  {"MakeTuple",3,BItuple,              (IFOR) tupleInline},
  {"Label",2,BIlabel,                  (IFOR) labelInline},
  {"hasLabel",2,BIhasLabel,            (IFOR) hasLabelInline},

  {"TellRecord",  2, BIrecordTell,	0},
  {"WidthC",      2, BIwidthC,		0},

  {"monitorArity",   3, BImonitorArity,   0},
  {"tellRecordSize", 3, BIsystemTellSize, 0},
  {"recordCIsVarB",  2, BIisRecordCVarB,  0},

  {".",            3,BIdot,              (IFOR) dotInline},
  {"^",            3,BIuparrow,        	 (IFOR) uparrowInline},

  {"HasFeature", 3, BIhasFeatureB,  (IFOR) hasFeatureBInline},
  {"CondSelect", 4, BImatchDefault, (IFOR) matchDefaultInline},
  {"Width",      2, BIwidth,        (IFOR) widthInline},

  {"AtomToString",    2, BIatomToString,	(IFOR) atomToStringInline},
  {"StringToAtom",    2, BIstringToAtom,	0},

  {"NewChunk",	      2,BInewChunk,	0},
  {"chunkArity",      2,BIchunkArity,	0},
  {"chunkWidth",      2,BIchunkWidth,	0},
  {"recordWidth",     2,BIrecordWidth,  0},

  {"NewName",         1,BInewName,	0},

  {"==",      3,BIeqB,    (IFOR) eqeqInline},
  {"\\=",     3,BIneqB,   (IFOR) neqInline},
  {"==Rel",   2,BIeq,     0},
  {"\\=Rel",  2,BIneq,    0},

  {"loadFile",       1, BIloadFile,		0},
  {"linkObjectFiles",2, BIlinkObjectFiles,	0},
  {"unlinkObjectFile",1,BIunlinkObjectFile,	0},

  {"dlOpen",2,          BIdlOpen,		0},
  {"dlClose",1,         BIdlClose,		0},

  {"findFunction",   3, BIfindFunction,		0},

  {"Alarm",          2, BIalarm,		0},
  {"Delay",          1, BIdelay,		0},

  {"System.gcDo",    0, BIgarbageCollection,	0},

  {"System.apply",   2, BIapply,		0},

  {"System.eq",      3, BIsystemEq,		0},

  {"=",              2, BIunify,		0},
  {"fail",           VarArity,BIfail,		0},
  {"nop",            VarArity,BInop,		0},

  {"deepReadCell",   2, BIdeepReadCell,		0},
  {"deepFeed",       2, BIdeepFeed,		0},

  {"getsBound",      1, BIgetsBound,		0},
  {"getsBoundB",     2, BIgetsBoundB,		0},

  {"connectLingRef", 1, BIconnectLingRef,	0},
  {"getLingRefFd",   1, BIgetLingRefFd,		0},
  {"getLingEof",     1, BIgetLingEof,		0},
  {"getOzEof",       1, BIgetLingEof,		0},

  {"setAbstractionTabDefaultEntry", 1, BIsetAbstractionTabDefaultEntry, 0},

  {"showBuiltins",0,BIshowBuiltins},

  {"onToplevel",1,BIonToplevel},
  {"addr",2,BIaddr},
  {"suspensions",2,BIsuspensions},
  {"globals",2,BIglobals},

  // Debugging
  {"globalThreadStream",1,BIglobalThreadStream},
  {"currentThread",1,BIcurrentThread},
  {"setStepMode",2,BIsetStepMode},
  {"stopThread",1,BIstopThread},
  {"contThread",1,BIcontThread},
  {"queryDebugState",2,BIqueryDebugState},
  {"Debug.breakpoint",0,BIbreakpoint},

  {"topVarInfo",2,BItopVarInfo},
  {"topVars",2,BItopVars},
  {"index2Tagged",2,BIindex2Tagged},
  {"time2localTime",2,BItime2localTime},

  //

  {"Thread.is",2,BIthreadIs},
  {"Thread.id",2,BIthreadID},
  {"Thread.this",1,BIthreadThis},
  {"Thread.suspend",1,BIthreadSuspend},
  {"Thread.resume",1,BIthreadResume},
  {"Thread.terminate",1,BIthreadTerminate},
  {"Thread.injectException",2,BIthreadRaise},
  {"Thread.preempt",1,BIthreadPreempt},
  {"Thread.setPriority",2,BIthreadSetPriority},
  {"Thread.getPriority",2,BIthreadGetPriority},
  {"Thread.isSuspended",2,BIthreadIsSuspended},
  {"Thread.state",2,BIthreadState},
  {"Thread.getName",2,BIthreadGetName},
  {"Thread.setName",2,BIthreadSetName},

#ifdef LINKEDTHREADS
  {"Thread.parent",2,BIthreadParent},
  {"Thread.children",2,BIthreadChildren},
#endif

  {"printLong",1,BIprintLong},

  {"halt",0,BIhalt},
  {"traceBack",0,BItraceBack},

  {"taskstack",   2, BItaskStack},
  {"location",    2, BIlocation},
  {"getThreadByID",2,BIgetThreadByID},
  {"spy",         1, BIspy},
  {"nospy",       1, BInospy},
  {"traceOn",     0, BItraceOn},
  {"traceOff",    0, BItraceOff},
  {"Debug.displayCode", 2, BIdisplayCode},

  // System functionality
  {"Print",                       1, BIprint,  (IFOR) printInline},
  {"Show",                        1, BIshow,   (IFOR) showInline},
  {"System.printInfo",            1, BIprintInfo},
  {"System.printError",           1, BIprintError},
  {"System.valueToVirtualString", 4, BItermToVS},
  {"System.printName",            2, BIgetPrintName},
  {"System.nbSusps",              2, BIconstraints, 0},

  {"SystemGetThreads",    1, BISystemGetThreads},
  {"SystemGetPriorities", 1, BISystemGetPriorities},
  {"SystemGetTime",       1, BISystemGetTime},
  {"SystemGetGC",         1, BISystemGetGC},
  {"SystemGetPrint",      1, BISystemGetPrint},
  {"SystemGetFD",         1, BISystemGetFD},
  {"SystemGetSpaces",     1, BISystemGetSpaces},
  {"SystemGetErrors",     1, BISystemGetErrors},
  {"SystemGetMessages",   1, BISystemGetMessages},
  {"SystemGetMemory",     1, BISystemGetMemory},
  {"SystemGetLimits",     1, BISystemGetLimits},
  {"SystemGetArgv",       1, BISystemGetArgv},
  {"SystemGetStandalone", 1, BISystemGetStandalone},
  {"SystemGetHome",       1, BISystemGetHome},
  {"SystemGetPlatform",   1, BISystemGetPlatform},

  {"SystemSetPriorities", 1, BISystemSetPriorities},
  {"SystemSetPrint",      1, BISystemSetPrint},
  {"SystemSetFD",         1, BISystemSetFD},
  {"SystemSetGC",         1, BISystemSetGC},
  {"SystemSetErrors",     1, BISystemSetErrors},
  {"SystemSetMessages",   1, BISystemSetMessages},
  {"SystemSetInternal",   1, BISystemSetInternal},

  {"shutdown",       0, BIshutdown,		0},
  {"ozparser_parse",2,ozparser_parse},
  {"ozparser_init",0,ozparser_init},
  {"ozparser_exit",0,ozparser_exit},


  {"getTermSize",4,BIgetTermSize},

  {"dumpThreads",0,BIdumpThreads},
  {"listThreads",1,BIlistThreads},

  {"foreignFDProps", 1, BIforeignFDProps},

  {"@",               2,BIat,                  (IFOR) atInline},
  {"<-",              2,BIassign,              (IFOR) assignInline},
  {"copyRecord",      2,BIcopyRecord,          0},
  {"makeClass",        9,BImakeClass,	       0},
  {"setMethApplHdl",   1,BIsetMethApplHdl,     0},
  {"getClass",         2,BIgetClass, 	       (IFOR) getClassInline},
  {"ooGetLock",        1,BIooGetLock, 	       (IFOR) ooGetLockInline},
  {"newObject",        2,BInewObject, 	       (IFOR) newObjectInline},
  {"getOONames",       5,BIgetOONames, 	       0},
  {"getSelf",          1,BIgetSelf,            0},
  {"setSelf",          1,BIsetSelf,            0},
  {"ooExch",           3,BIooExch,             (IFOR) ooExchInline},

  {"Space.new",           2, BInewSpace,        0},
  {"IsSpace",             2, BIisSpace,         0},
  {"Space.ask",           2, BIaskSpace,        0},
  {"Space.askVerbose",    2, BIaskVerboseSpace, 0},
  {"Space.merge",         2, BImergeSpace,      0},
  {"Space.clone",         2, BIcloneSpace,      0},
  {"Space.commit",        2, BIchooseSpace,     0},
  {"Space.inject",        2, BIinjectSpace,     0},

  {"biExceptionHandler",         1, BIbiExceptionHandler,         0},
  {"setDefaultExceptionHandler", 1, BIsetDefaultExceptionHandler, 0},

  {"raise",      1, BIraise,      0},
  {"raiseError", 1, BIraiseError, 0},

  {0,0,0,0}
};


extern void BIinitFD(void);
#ifdef FSETVAR
extern void BIinitFSet(void);
#endif
#ifdef METAVAR
extern void BIinitMeta(void);
#endif
extern void BIinitAVar(void);
extern void BIinitPerdioVar(void);
extern void BIinitUnix();
extern void BIinitAssembler();
extern void BIinitTclTk();
extern void BIinitPerdio();

BuiltinTabEntry *BIinit()
{
  BuiltinTabEntry *bi = BIadd("builtin",3,BIbuiltin);

  if (!bi)
    return bi;

  BIaddSpec(allSpec1);
  BIaddSpec(allSpec2);

#ifdef ASSEMBLER
  BIinitAssembler();
#endif

  BIinitFD();
#ifdef FSETVAR
  BIinitFSet();
#endif
#ifdef METAVAR
  BIinitMeta();
#endif

  BIinitAVar();
  BIinitPerdioVar();
  BIinitUnix();
  BIinitTclTk();

#ifdef PERDIO
  BIinitPerdio();
#endif

  return bi;
}

#endif /* BUILTINS2 */
