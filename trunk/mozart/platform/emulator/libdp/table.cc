/*
 *  Authors:
 *    Per Brand (perbrand@sics.se)
 * 
 *  Contributors:
 *    optional, Contributor's name (Contributor's email address)
 * 
 *  Copyright:
 *    Per Brand, 1998
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
 
#if defined(INTERFACE)
#pragma implementation "table.hh"
#endif

#include "base.hh"
#include "table.hh"
#include "value.hh"
#include "var.hh"
#include "var_obj.hh"
#include "msgContainer.hh"
#include "state.hh"
#include "fail.hh"
#include "protocolState.hh"
#include "dpResource.hh"
#include <math.h>
#include "os.hh" 


#define PO_getValue(po) \
((po)->isTertiary() ? makeTaggedConst((po)->getTertiary()) : (po)->getRef())

NewOwnerTable *ownerTable;
BorrowTable *borrowTable;


// from var.cc
void oz_dpvar_localize(TaggedRef *);

void OwnerEntry::localize(int index)
{
  if (isVar()) { 
    if(GET_VAR(this,Manager)->getInfo()==NULL)
      oz_dpvar_localize(getPtr());
    else return;
  } else {
    if(isTertiary() &&
       !localizeTertiary(getTertiary()))
      return;
  } 
  homeRef.removeReference();
  OT->freeOwnerEntry(getOdi());
}

void OwnerEntry::updateReference(DSite* site){
  if (isVar() || isTertiary()){
    MsgContainer *msgC = msgContainerManager->newMsgContainer(site);
    msgC->put_M_BORROW_REF(getValue());
    send(msgC);
  }
  else
    printf("Warning: Updating bound variable!\n");
}





OZ_Term BorrowEntry::extract_info() {
  OZ_Term primCred, secCred;
  OZ_Term na=
    OZ_recordInit(oz_atom("netAddress"),
		  oz_cons(oz_pairA("site", oz_atom(remoteRef.netaddr.site->stringrep_notype())),
			  oz_cons(oz_pairAI("index",(int)remoteRef.netaddr.index), oz_nil())));
  
  return OZ_recordInit(oz_atom("be"),
		       oz_cons(oz_pairAA("type", toC(PO_getValue(this))),
			       oz_cons(oz_pairA("na", na),
				       oz_cons(oz_pairA("dist_gc",remoteRef.extract_info()),
					       oz_nil()))));
}


void BorrowEntry::freeBorrowEntry(){
  Assert(remoteRef.canBeReclaimed());
  if(isVar() && typeOfBorrowVar(this)==VAR_PROXY){
    GET_VAR(this,Proxy)->nowGarbage(this);}
  setFlags(474747);
  if(!isPersistent())
    remoteRef.dropReference();}

void BorrowEntry::gcBorrowRoot(int i) {
  if (isVar()) 
    {
      if(oz_isMark(getRef()))
	{
	  gcPO();
	  return;
	}
      if (tagged2Var(*getPtr())->getSuspList()!=0) 
	{
	  gcPO();
	  return;
	}
      if(!remoteRef.canBeReclaimed()) 
	{
	  gcPO();
	}
      return;
    }
  
  if(isRef())
    {
      gcPO(); 
      return;
    }
  Assert(isTertiary());

  if(getTertiary()->cacIsMarked() || !remoteRef.canBeReclaimed() ||
     isTertiaryPending(getTertiary()))
    {
      gcPO();
    }
  return;
}

void OB_Entry::gcPO() {
  if (isGCMarked()) return;
  makeGCMark();
  Assert(isTertiary() || isRef() || isVar());
  PD((GC,"var/ref/tert found"));
  oz_gCollectTerm(u.tert, u.tert);
}

void BorrowTable::gcBorrowTableRoots()
{
   int size = getSize();
   for(int i=0;i<size;i++) 
     {
       BucketHashNode* o = getBucket(i); 
       while(o){
	 BorrowEntry *b = (BorrowEntry *) o;
	 o = o->getNext();
	 if (!b->isGCMarked())b->gcBorrowRoot(i);
       }
     }
}

void BorrowEntry::gcBorrowUnusedFrame(Tertiary* t) {
  Assert(!(isTertiaryPending(getTertiary())));
  if(t->getType()==Co_Cell){
    CellFrame *cf = (CellFrame*) t;
    if(cf->dumpCandidate()){
      cellLockSendDump(this);}}
  else{
    Assert(t->getType()==Co_Lock);
    LockFrame *lf = (LockFrame*) t;
    if(lf->dumpCandidate()){
      cellLockSendDump(this);}
  }
  gcPO();
}

void BorrowTable::gcBorrowTableUnusedFrames()
{
  int size = getSize();
  for(int i=0;i<size;i++) 
    {
      BucketHashNode* o = getBucket(i); 
      while(o){
	BorrowEntry *b = (BorrowEntry *) o;
	o = o->getNext();
	if((!b->isGCMarked()) && b->isTertiary() && 
	   b->getTertiary()->isFrame()){
	  b->gcBorrowUnusedFrame(b->getTertiary());}}
    }
}

void BorrowTable::gcFrameToProxy(){
   int size = getSize();
   for(int i=0;i<size;i++) 
     {
       BucketHashNode* o = getBucket(i); 
       while(o)
	 {
	   BorrowEntry *b = (BorrowEntry *) o;
	   o = o->getNext();
	   if((b->isTertiary())){
	     Tertiary *t=b->getTertiary();
	     if(t->isFrame()) {
	       if((t->getType()==Co_Cell)
		  && ((CellFrame*)t)->getState()==Cell_Lock_Invalid
		  && ((CellSec*)((CellFrame*)t)->getSec())->getPending()==NULL){
		 ((CellFrame*)t)->convertToProxy();}
	       else{
		 if((t->getType()==Co_Lock)
		    && ((LockFrame*)t)->getState()==Cell_Lock_Invalid){
		   ((LockFrame*)t)->convertToProxy();}}}}
	 }
     }
}


/* OBSERVE - this must done at the end of other gc */
void BorrowTable::gcBorrowTableFinal()
{
   int size = getSize();
   for(int i=0;i<size;i++) 
     {
       BucketHashNode* o = getBucket(i); 
       while(o)
	 {
	   BorrowEntry *b = (BorrowEntry *) o;
	   o = o->getNext();
	   if(b->isVar()) 
	     {
	       if(b->isGCMarked())
		 {
		   b->removeGCMark();
		   b->getSite()->makeGCMarkSite();
		 }
	       else
		 {
		   if(!errorIgnoreVar(b)) maybeUnaskVar(b);
		   borrowTable->maybeFreeBorrowEntry((int)b);
		 }
	     }
	   else 
	     if(b->isTertiary())
	       {
		 Tertiary *t = b->getTertiary();
		 if(b->isGCMarked()) 
		   {
		     b->removeGCMark();
		     b->getSite()->makeGCMarkSite();
		   }
		 else
		   {
		     if(!errorIgnore(t)) maybeUnask(t);
		     Assert(t->isProxy());
		     borrowTable->maybeFreeBorrowEntry((int)b);
		   }
	       }
	     else 
	       if(b->isRef())
		 {
		   Assert(b->isGCMarked());
		   b->removeGCMark();
		   b->getSite()->makeGCMarkSite();
		 }
	 }
     }
}

int BorrowTable::dumpFrames()
{
  int notReady = 0;
  
  //
  int size = getSize();
  for(int i=0;i<size;i++) {
    BucketHashNode* o = getBucket(i); 
    while(o)
      {
	BorrowEntry *be = (BorrowEntry *) o;
	o = o->getNext();
	if (be->isTertiary()){
	  Tertiary *t = be->getTertiary();
	  if (t->isFrame()) {
	    int type = t->getType();
	    int state;
	    
	    if (type == Co_Cell) {
	      state = ((CellFrame *) t)->getState();
	      ((CellFrame *) t)->getCellSec()->dumpPending();
	    } else if (type == Co_Lock) {
	      state = ((LockFrame *) t)->getState();
	      ((LockFrame *) t)->getLockSec()->dumpPending();
	    } else {
	      continue;
	    }
	    
	    switch (state){
	    case Cell_Lock_Invalid:
	      if (type == Co_Lock)
		((CellFrame *) t)->convertToProxy();
	      else
		((LockFrame *) t)->convertToProxy();
	      break;
		 
	    case Cell_Lock_Requested:
	    case Cell_Lock_Valid:
	      cellLockSendDump(be);
	      if (type == Co_Lock)
		((CellFrame *) t)->getCellSec()->markDumpAsk();
	      else
		((LockFrame *) t)->getLockSec()->markDumpAsk();
	      notReady++;		// dumping just begun;
	      break;
	      
	    case Cell_Lock_Valid|Cell_Lock_Dump_Asked:
	       case Cell_Lock_Requested|Cell_Lock_Next:
	    case Cell_Lock_Requested|Cell_Lock_Next|Cell_Lock_Dump_Asked:
	    case Cell_Lock_Requested|Cell_Lock_Dump_Asked:
	      notReady++;		// dumping still in progress;
	      break;
	      
	      // kost@ : optimization: don't do 'ask dump' protocol, but
	      // just break the lock locally and send it away:
	    case Cell_Lock_Valid|Cell_Lock_Next:
	      if(type == Co_Lock) {
		NetAddress *na = be->getNetAddress();
		LockSec *sec = ((LockFrame *) t)->getLockSec();
		sec->markInvalid();
		DSite *toS = sec->getNext();
		lockSendToken(na->site, na->index, toS);
		((LockFrame *) t)->convertToProxy();
	      } else {
		Assert(0);
	      }
	      break;
	      
	    default:
	      Assert(0);
	    }
	  }
	}
      }
  }
  return (notReady);
}

//
void BorrowTable::dumpProxies()
{
  DebugCode(int proxies = 0;);
  DebugCode(int frames = 0;);
  int size = getSize();
  for(int i=0;i<size;i++) {
    BucketHashNode* o = getBucket(i); 
    while(o)
      {
	BorrowEntry *be = (BorrowEntry *) o;
	o = o->getNext();
	if (be->isTertiary()) {
	  Tertiary *t = be->getTertiary();
	  
	  if (t->isProxy()) {
	    maybeFreeBorrowEntry((int) be);
	    DebugCode(proxies++;);
	    continue;
	  }
	  
	  // kost@ : any frames left over are stuck here: it does not
	  // make any sense to reclaim the credit 'cause some credit
	  // will be lost anyway;
	  if (t->isFrame()) {
	    DebugCode(frames++;);
	    continue;
	  }
	}
	else 
	  {
	    if (be->isVar() && oz_isProxyVar(oz_deref(be->getRef()))) 
	      {
		maybeFreeBorrowEntry((int)be);
		DebugCode(proxies++;);
		continue;
	      }
	  }
      }
  }
}

int BorrowTable::notGCMarked() {
  int size = getSize();
  for(int i=0;i<size;i++) {
    BucketHashNode* o = getBucket(i); 
    while(o)
      {
	BorrowEntry *be = (BorrowEntry *) o;
	o = o->getNext();
	if(be->isGCMarked())
	  return FALSE;
	if(be->isTertiary()) {
	  if(be->getTertiary()->cacIsMarked())
	    return FALSE;
	  Assert(BT->bi2borrow(be->getTertiary()->getIndex()) == be);
	  
	}
      }
  }
  return TRUE;
}

/* ********************************************************************** */
/*      BUCKET HASH TABLE                                                 */
/* ********************************************************************** */


const int STEP=5;
const double MAXFULL=0.75; // The max. load of HashTable
const double IDEALFULL=0.40; 
const double MINFULL=0.20; // The min. load of HashTable
const int EXPANSION_FACTOR=2;  // load .375
/* ********************************************************************** */
/*      PRIME NUMBER FUNCTIONS                                            */
/* ********************************************************************** */

inline Bool isMyPrime2(int prime){
  if (prime%2 == 0) {return NO;}
  for(int i=3; i<=sqrt(prime); i+=2) {
    if (prime%i == 0) {return NO;}}
  return OK;}

int nextMyPrime2(int prime){
  if (prime <= STEP) {
    prime = STEP+2;}
  if (prime%2 == 0) {
    prime++;}
  while(!isMyPrime2(prime)) {
    prime += 2;}
  return prime;}




inline void 
BucketHashTable::basic_htAdd(unsigned int pk,BucketHashNode *bhn){
  int index=pk % tableSize;
  BucketHashNode *ptr= table[index];
  bhn->setNext(ptr);
  table[index] = bhn;
}


  
inline void BucketHashTable::rehash(BucketHashNode **old,int size){
  BucketHashNode *tmp,*ptr;
  for(int i=0;i<size;i++){
    ptr = old[i];
    while(ptr) {
      tmp = ptr->next;
      int index = ptr->getPrimKey() % tableSize;
      ptr->next = table[index];
      table[index] = ptr; 
      ptr = tmp;
    }
  }
}
	
void BucketHashTable::calc_percents(){
  top_percent = (int) (MAXFULL * tableSize);
  bottom_percent= (int) (MINFULL * tableSize);
  if(tableSize==minSize) bottom_percent=0;
  Assert(top_percent>bottom_percent);}  

void BucketHashTable::resize(){
  int newSize=nextMyPrime2(tableSize*EXPANSION_FACTOR);
  BucketHashNode **oldtable=table;
  table= (BucketHashNode **) malloc(newSize * sizeof(BucketHashNode*));
  if(table==NULL){
    OZ_error("Memory allocation: could not resize Hash Table");}
  init(0,newSize);
  int oldSize=tableSize;
  tableSize=newSize;
  rehash(oldtable,oldSize);
  calc_percents();
  free(oldtable);}

void BucketHashTable::compactify(){
  printf("We dont compactify BucketHashTables\n");
  /*
  if(counter>=bottom_percent) return;
  if(tableSize==minSize) return;
  Assert(tableSize>=minSize);
  int newSize = nextMyPrime2((int) ( ((double) counter) / IDEALFULL));
  if(newSize<minSize) newSize=minSize;
  Assert(newSize<tableSize);
  GenHashNode *oldtable=table;
  table= (GenHashNode *) malloc(newSize * sizeof(GenHashNode));
  Assert(table!=NULL);
  if(table==NULL){
    return;}
  init(0,newSize);
  int oldSize=tableSize;
  tableSize=newSize;
  rehash(oldtable,oldSize);
  calc_percents();
  free(oldtable);
  */
}

/* ********************************************************************** */
/*      VISIBLE MEMBER FUNCTIONS                                           */
/* ********************************************************************** */

BucketHashTable::BucketHashTable(int s){
  tableSize = nextMyPrime2(s);
  counter = 0;
  calc_percents();
  minSize=tableSize;
  table = (BucketHashNode **) malloc(tableSize * sizeof(BucketHashNode*));
  init(0,tableSize);}


void BucketHashTable::htAdd(unsigned int pk,BucketHashNode *bhn){
  if (counter > top_percent) resize();  
  counter++;
  basic_htAdd(pk,bhn);}



void BucketHashTable::htSubPkEn(unsigned int pk,BucketHashNode *cur){
  unsigned int index=pk % tableSize; 
  counter--;
  BucketHashNode **try0=&table[index];
  while(*try0){
    if ((*try0)== cur){
      *try0=cur->next;
      delete cur;
      break;
    }
    try0 = &((*try0)->next);
  }
}
void* BucketHashTable::htSubPkSk(unsigned int pk,unsigned int sk){
  unsigned int index=pk % tableSize; 
  counter--;
  BucketHashNode *tmp, **try0=&table[index];
  while(*try0){
    if ((*try0)->prim_key==pk && (*try0)->sec_key==sk){
      tmp = *try0;
      *try0=tmp->next;
      return (void*) tmp;
    }
    try0 = &((*try0)->next);
  }
  return NULL;
}

BucketHashNode *BucketHashTable::htFindPk(unsigned int key){
  unsigned int index=key % tableSize; 
  return table[index];}


BucketHashNode *BucketHashTable::htFindPkSk(unsigned int pk,unsigned int sk){
  unsigned int index=pk % tableSize; 
  BucketHashNode *ans = table[index];
  while(ans && (ans->prim_key != pk || ans->sec_key != sk))
    ans = ans->next;
  return  ans;
} 


/*******************************************************************

New Owner Table

 *******************************************************************/

void NewOwnerTable::compactify()
{
  printf("New owner table is not compactified\n");
}
void NewOwnerTable::resize()
{
  printf("We dont have to resize the NewOwnerTable\n");
}

int NewOwnerTable::newOwner(OwnerEntry *&oe, int algs){
  int odi = nxtId++; 
  oe = new OwnerEntry(odi,algs);
  htAdd(hash(odi),oe);
  return (int) oe;}

void NewOwnerTable::freeOwnerEntry(int odi)
{
  (void) htSubPkSk(hash(odi),odi);
  localized ++; 
  return;
}

OZ_Term NewOwnerTable::extract_info(){
  OZ_Term list=oz_nil();
  OZ_Term credit;
  int size = getSize();
  for(int i=0;i<size;i++) {
    BucketHashNode* o = getBucket(i); 
    while(o){
      OwnerEntry *oe = (OwnerEntry *)o; 
      credit=oe->homeRef.extract_info();
      o = o->getNext();
      list=
	oz_cons(OZ_recordInit(oz_atom("oe"),
	        oz_cons(oz_pairAI("odi",oe->homeRef.oti),
		oz_cons(oz_pairAA("type", toC(PO_getValue(oe))),
	        oz_cons(oz_pairA("dist_gc", credit), 
		oz_nil())))), list);
    
    }
  }
  return OZ_recordInit(oz_atom("ot"),
		       oz_cons(oz_pairAI("size", size),
			       oz_cons(oz_pairAI("localized", getLocalized()),
				       oz_cons(oz_pairA("list", list), oz_nil()))));  
}
 

void NewOwnerTable::print(){ printf("print\n");}

void NewOwnerTable::gcOwnerTableRoots()
{
  int size = getSize();
  for(int i=0;i<size;i++) {
    BucketHashNode* o = getBucket(i); 
    while(o){
      OwnerEntry *en = (OwnerEntry *)(o);
      en->gcPO();
      o = o->getNext();
    }
  } 
} 

void NewOwnerTable::gcOwnerTableFinal()
{
  int size = getSize();
  for(int i=0;i<size;i++) {
    BucketHashNode* o = getBucket(i); 
    while(o){
      OwnerEntry *oe = (OwnerEntry*) o;
      o = o->getNext();
      oe->removeGCMark();
      if (oe->isVar()){
	TaggedRef *ptr = oe->getPtr();
	DEREFPTR(v,ptr);
	Assert(oz_isManagerVar(v));
	oe->mkVar(makeTaggedRef(ptr),oe->getFlags());
      }
    }
  }
  // compactify?
}

int NewOwnerTable::notGCMarked() {
  int size = getSize();
  for(int i=0;i<size;i++) {
    BucketHashNode* o = getBucket(i); 
    while(o){
      OwnerEntry *oe = (OwnerEntry *)o;
      o = o->getNext();
      if(oe->isGCMarked()) return FALSE;
      if(oe->isTertiary() && oe->getTertiary()->cacIsMarked()) return FALSE;
    }
  }
  return TRUE;
}

OwnerEntry* NewOwnerTable::odi2entry(int odi)
{
  Assert(odi <= nxtId);
  unsigned int hvalue = hash(odi);
  BucketHashNode *aux = htFindPk(hvalue);
  while(aux && aux->getPrimKey() != odi) aux = aux ->getNext(); 
  return (OwnerEntry *) aux;
}


BorrowEntry *BorrowTable::find(NetAddress *na)
{
  return find(na->index, na->site);
}


BorrowEntry *BorrowTable::find(int index, DSite  *site)
{
  return (BorrowEntry *) htFindPkSk((unsigned int) index,(unsigned int)site); 
}

int BorrowTable::newBorrow(RRinstance *c,DSite * sd,int odi){
  BorrowEntry* be = new BorrowEntry(sd,odi,c);
  htAdd((unsigned int)odi,be);
  return (int) be;
}  

Bool BorrowTable::maybeFreeBorrowEntry(int index){
  BorrowEntry *b = bi2borrow(index);
  if(!b->remoteRef.canBeReclaimed()) {
    if(b->isVar()){
      b->changeToRef();
    }
    return FALSE;
  }
  htSubPkSk(b->getPrimKey(),b->getSecKey());
  b->freeBorrowEntry();  
  delete b;
  return TRUE;
}

OZ_Term BorrowTable::extract_info() {
  OZ_Term ans = oz_nil();
  int size = getSize();
  for(int i=0;i<size;i++) {
    BucketHashNode* o = getBucket(i); 
    while(o){
      BorrowEntry *be = (BorrowEntry *)o;
      o = o->getNext();
      ans = oz_cons(be->extract_info(),ans);
    }
  }
  return ans;
}

void BorrowTable::print()
{
  int size = getSize();
  for(int i=0;i<size;i++) {
    BucketHashNode* o = getBucket(i); 
    if(o){
      printf("E %d",i);
      while(o){
	printf(" %d#%d",o->getPrimKey(),o->getSecKey());
	o = o->getNext();
      }
      
    }
  }
  printf("\n"); 
}
	
	
