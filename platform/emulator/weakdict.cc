#include "weakdict.hh"
#include "var_readonly.hh"
#include "atoms.hh"
#include "tagged.hh"
#include "am.hh"

// apparently we have to be really careful not to perform a deref
// during gc
inline int _oz_isWeakDictionary(OZ_Term t)
{
  return oz_isExtension(t) &&
    tagged2Extension(t)->getIdV()==OZ_E_WEAKDICTIONARY;
}

inline WeakDictionary* _tagged2WeakDictionary(OZ_Term t)
{
  Assert(_oz_isWeakDictionary(t));
  return (WeakDictionary*) tagged2Extension(t);
}

inline int oz_isWeakDictionary(OZ_Term t)
{
  return _oz_isWeakDictionary(OZ_deref(t));
}

inline WeakDictionary* tagged2WeakDictionary(OZ_Term t)
{
  return _tagged2WeakDictionary(OZ_deref(t));
}

inline bool WeakDictionary::isEmpty()
{
  return (table==0 || table->numelem==0);
}

// GARBAGE COLLECTION SUPPORT

// this is a list of currently known weak dictionaries
// it is computed anew at each GC by copying.  creating
// a new weak dictionary also adds it to this list.

static OZ_Term weakList = 0;
// raph: weakList == 0 means that the module has not been initialized

void weakdict_init(void) {
  weakList = AtomNil;
}

WeakDictionary::WeakDictionary(DynamicTable*t, OZ_Term s)
  : OZ_Extension(), table(t), stream(s)
{
  weakList = oz_cons(OZ_extension(this), weakList);
}

WeakDictionary::WeakDictionary(OZ_Term srm)
  : OZ_Extension(), stream(srm)
{
  table = DynamicTable::newDynamicTable(DictDefaultSize);
  weakList = oz_cons(OZ_extension(this), weakList);
}


// at the very start of GC, we must save the list of currently
// known weak dictionaries.  We can then process this list at
// any point prior to finalization provided we also check for
// GC marks.  That list is also used to detect weak dictionaries
// that are revived by the finalization mechanism.

static OZ_Term previousWeakList = 0;
// raph: before initialization, weakList == previousWeakList must hold

// kost@ : close down in the presence of distribution: we want to dump
// all the locally accessible entities (so that all the proxies could
// be removed). The simplest strategy is to discard weak dictionaries
// (their streams cannot be processed anyway).
void gDropWeakDictionaries()
{
  weakList = AtomNil;
}

void gCollectWeakDictionariesInit()
{
  if (weakList == 0) return;
  previousWeakList = weakList;
  weakList = AtomNil;
}

// All previously known dictionaries that
//	(1) have a live board
//	(2) have a stream
//	(3) are not empty
// must be preserved because they must still be given a chance to
// eventually finalize their content.

void gCollectWeakDictionariesPreserve()
{
  if (weakList == 0) return;
  // we simply go through the list of previously known
  // dictionaries and collect those that have not yet been
  // collected and that verify the 3 conditions above

  for (;previousWeakList!=AtomNil;
       previousWeakList=oz_tail(previousWeakList))
    {
      OZ_Term t = oz_head(previousWeakList);
      if (tagged2Const(t)->cacIsMarked())
	continue;
      WeakDictionary*d = _tagged2WeakDictionary(t);
      if (extension2Const(d)->getBoardInternal()->cacIsAlive() &&
	  d->stream!=0 &&
	  !d->isEmpty())
	// gc has the side effect of entering the copy into
	// weakList, thus recording its existence for the
	// next time
	oz_gCollectTerm(t,t);
    }

  // From now on, previousWeakList refers to the first weak dictionary
  // in the list of which the contents has been finalized.
  Assert(previousWeakList == AtomNil);
}

bool gCollectWeakDictionariesHasMore() {
  // Are there dictionaries in the list that have not been finalized yet?
  return weakList != previousWeakList;
}

void gCollectWeakDictionariesContent()
{
  if (weakList == 0) return;
  // This is called after the 1st gc phase has completed.
  // all data reachable outside of weak dictionaries has
  // been marked and copied.  Entries in a weak dictionary
  // now point either to marked values (that have already
  // been copied to the new heap) or to unmarked values
  // (which are no longer reachable from outside).
  //
  // Note: The dictionaries that have not been finalized yet, are the
  // ones between weakList and previousWeakList.
  Assert(weakList != previousWeakList);
  for (OZ_Term curr = weakList; curr != previousWeakList; curr=oz_tail(curr))
    _tagged2WeakDictionary(oz_head(curr))->weakGC();
  previousWeakList = weakList;
  // Revived dictionaries will appear in front of previousWeakList.
}

OZ_Extension* WeakDictionary::gCollectV()
{
  // copy to the new heap and enter into known list
  // WeakDictionary's gcRecurseV does nothing.
  // the real work is done in gCollectWeakDictionariesContent().
  return new WeakDictionary(table,stream);
}

OZ_Extension* WeakDictionary::sCloneV() {
  return gCollectV();
}

// EXTENSION INTERFACE

OZ_Term WeakDictionary::printV(int depth)
{
  return oz_pair2(oz_atom("<WeakDictionary n="),
		  oz_pair2(oz_int(table->numelem),
			   oz_atom(">")));
}

inline OZ_Boolean WeakDictionary::get(OZ_Term key,OZ_Term& val)
{
  return ((val = table->lookup(oz_deref(key)))!=0);	// 0 if not found
}

extern int oz_raise(OZ_Term cat, OZ_Term key, const char *label, int arity, ...);

OZ_Return WeakDictionary::getFeatureV(OZ_Term f,OZ_Term& v)
{
  if (!OZ_isFeature(f)) { OZ_typeError(1,"feature"); }
  if (get(f,v)) {
    return PROCEED;
  } else {
    return oz_raise(E_ERROR,E_KERNEL,"WeakDictionary.get",2,
		    makeTaggedExtension(this),f);
  }
}

void WeakDictionary::put(OZ_Term key,OZ_Term val)
{
  if (table->fullTest()) resizeDynamicTable(table);
  if (!table->add(key,val)) {
    resizeDynamicTable(table);
    table->add(key,val);
  }
}

// kost@ + raph : OptVar's should not be put in weak dictionaries.
// OptVar's are not marked by the garbage collector, and are therefore
// always considered as garbage.  This macro turns v into a SimpleVar
// if necessary.
#define NonOptVar(v)							\
  TaggedRef w = v;							\
  DEREF(w,w_ptr);							\
  if (oz_isOptVar(w)) { (void) oz_getNonOptVar(w_ptr); }

OZ_Return WeakDictionary::putFeatureV(OZ_Term f,OZ_Term  v)
{
  if (!OZ_isFeature(f)) { OZ_typeError(1,"feature"); }
  if (!isLocal()) return oz_raise(E_ERROR,E_KERNEL,"globalState",1,
				  oz_atom("weakDictionary"));
  NonOptVar(v);   // bug fix for OptVar's
  put(f,v);
  return PROCEED;
}

// BUILTINS

OZ_BI_define(weakdict_new,0,2)
{
  OZ_Term srm = oz_newReadOnly(oz_currentBoard());
  WeakDictionary* wd = new WeakDictionary(srm);
  OZ_out(0) = srm;
  OZ_out(1) = OZ_extension(wd);
  return PROCEED;
}
OZ_BI_end

#define OZ_declareWeakDict(ARG,VAR) \
OZ_declareType(ARG,VAR,WeakDictionary*,"weakDictionary",\
	oz_isWeakDictionary,tagged2WeakDictionary)

#define NO_COERCE(X) X

#define OZ_declareFeature(ARG,VAR) \
OZ_declareType(ARG,VAR,OZ_Term,"feature",OZ_isFeature,NO_COERCE)

OZ_BI_define(weakdict_get,2,1)
{
  OZ_declareWeakDict(0,d);
  OZ_declareFeature(1,k);
  OZ_Term v;
  if (!d->get(k,v))
    return oz_raise(E_SYSTEM,E_KERNEL,"weakDictionary",2,OZ_in(0),OZ_in(1));
  OZ_RETURN(v);
}
OZ_BI_end

OZ_BI_define(weakdict_condGet,3,1)
{
  OZ_declareWeakDict(0,d);
  OZ_declareFeature(1,k);
  OZ_Term v;
  if (!d->get(k,v)) OZ_RETURN(OZ_in(2));
  OZ_RETURN(v);
}
OZ_BI_end

OZ_BI_define(weakdict_put,3,0)
{
  OZ_declareWeakDict(0,d);
  if (!d->isLocal()) return oz_raise(E_ERROR,E_KERNEL,"globalState",1,
				     oz_atom("weakDictionary"));
  OZ_declareFeature(1,k);
  OZ_declareTerm(2,v);
  NonOptVar(v);   // bug fix for OptVar's
  d->put(oz_deref(k),v);
  return PROCEED;
}
OZ_BI_end

// {WeakDictionary.exchange D LI ?OldVal NewVal}
OZ_BI_define(weakdict_exchange,4,0)
{
  OZ_declareWeakDict(0,d);
  if (!d->isLocal()) return oz_raise(E_ERROR,E_KERNEL,"globalState",1,
				     oz_atom("weakDictionary"));
  OZ_declareFeature(1,k);
  OZ_declareTerm(3,new_val);
  OZ_Term old;
  // read the old value
  if (!d->get(k,old))
    return oz_raise(E_SYSTEM,E_KERNEL,"weakDictionary",2,OZ_in(0),OZ_in(1));
  // put the new value
  NonOptVar(new_val);   // bug fix for OptVar's
  d->put(oz_deref(k),new_val);
  // unify the old value with third argument
  am.prepareCall(BI_Unify, RefsArray::make(OZ_in(2), old));
  return BI_REPLACEBICALL;
}
OZ_BI_end

// {WeakDictionary.condExchange D LI DefVal ?OldVal NewVal}
OZ_BI_define(weakdict_condExchange,5,0)
{
  OZ_declareWeakDict(0,d);
  if (!d->isLocal()) return oz_raise(E_ERROR,E_KERNEL,"globalState",1,
				     oz_atom("weakDictionary"));
  OZ_declareFeature(1,k);
  OZ_declareTerm(2,def_val);
  OZ_declareTerm(4,new_val);
  OZ_Term old;
  // read the old value, or take def_val if not present
  if (!d->get(k,old)) old = def_val;
  // put the new value
  NonOptVar(new_val);   // bug fix for OptVar's
  d->put(oz_deref(k),new_val);
  // unify the old value with fourth argument
  am.prepareCall(BI_Unify, RefsArray::make(OZ_in(3), old));
  return BI_REPLACEBICALL;
}
OZ_BI_end

OZ_BI_define(weakdict_is,1,1)
{
  OZ_declareDetTerm(0,t);
  OZ_RETURN_BOOL(oz_isWeakDictionary(t));
}
OZ_BI_end

void WeakDictionary::close()
{
  if (stream) {
    DEREF(stream,ptr);
    oz_bindReadOnly(ptr,oz_nil());
    stream=0;
  }
}

OZ_BI_define(weakdict_close,0,0)
{
  OZ_declareWeakDict(0,d);
  if (!d->isLocal())
    return oz_raise(E_ERROR,E_KERNEL,"globalState",1,oz_atom("weakDictionary"));
  d->close();
  return PROCEED;
}
OZ_BI_end

inline OZ_Term WeakDictionary::getKeys()
{
  return (table)?table->getKeys():AtomNil;
}

OZ_BI_define(weakdict_keys,1,1)
{
  OZ_declareWeakDict(0,d);
  OZ_RETURN(d->getKeys());
}
OZ_BI_end

inline OZ_Term WeakDictionary::getPairs()
{
  return (table)?table->getPairs():AtomNil;
}

OZ_BI_define(weakdict_entries,1,1)
{
  OZ_declareWeakDict(0,d);
  OZ_RETURN(d->getPairs());
}
OZ_BI_end

inline OZ_Term WeakDictionary::getItems()
{
  return (table)?table->getItems():AtomNil;
}

OZ_BI_define(weakdict_items,1,1)
{
  OZ_declareWeakDict(0,d);
  OZ_RETURN(d->getItems());
}
OZ_BI_end

OZ_BI_define(weakdict_isempty,1,1)
{
  OZ_declareWeakDict(0,d);
  OZ_RETURN_BOOL(d->isEmpty());
}
OZ_BI_end

inline OZ_Term WeakDictionary::toRecord(OZ_Term label)
{
  return (table)?table->toRecord(label):label;
}

#define OZ_declareLiteral(ARG,VAR)

OZ_BI_define(weakdict_torecord,2,1)
{
  OZ_expectType(0,"Literal",OZ_isLiteral);
  OZ_Term label = OZ_in(0);
  OZ_declareWeakDict(1,d);
  OZ_RETURN(d->toRecord(label));
}
OZ_BI_end

inline void WeakDictionary::remove(OZ_Term key)
{
  if (table) {
    DynamicTable *aux = table->remove(key);
    if (aux!=table) {
      table->dispose();
      table=aux;
    }
  }
}

OZ_BI_define(weakdict_remove,2,0)
{
  OZ_declareWeakDict(0,d);
  if (!d->isLocal())
    return oz_raise(E_ERROR,E_KERNEL,"globalState",1,oz_atom("weakDictionary"));
  OZ_declareFeature(1,key);
  d->remove(key);
  return PROCEED;
}
OZ_BI_end

inline void WeakDictionary::remove_all()
{
  if (table) {
    table->dispose();
    table = DynamicTable::newDynamicTable(DictDefaultSize);
  }
}

OZ_BI_define(weakdict_remove_all,1,0)
{
  OZ_declareWeakDict(0,d);
  if (!d->isLocal())
    return oz_raise(E_ERROR,E_KERNEL,"globalState",1,oz_atom("weakDictionary"));
  d->remove_all();
  return PROCEED;
}
OZ_BI_end

inline bool WeakDictionary::member(OZ_Term key)
{
  return (table->lookup(key) != makeTaggedNULL());
}

OZ_BI_define(weakdict_member,2,1)
{
  OZ_declareWeakDict(0,d);
  OZ_declareFeature(1,key);
  OZ_RETURN_BOOL(d->member(key));
}
OZ_BI_end
