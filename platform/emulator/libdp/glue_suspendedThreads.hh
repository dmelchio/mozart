/*
 *  Authors:
 *    Zacharias El Banna, 2002
 *    Erik Klintskog, 2002
 * 
 *  Contributors:
 *    Raphael Collet (raph@info.ucl.ac.be)
 * 
 *  Copyright:
 *    Zacharias El Banna, 2002
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

#ifndef __GLUE_SUSPENDEDTHREADS_HH
#define __GLUE_SUSPENDEDTHREADS_HH

#ifdef INTERFACE  
#pragma interface
#endif

#include "value.hh"
#include "glue_mediators.hh"
#include "glue_tables.hh"
#include "dss_classes.hh"


/*
  A SuspendedOperation object is a "thread mediator" for the DSS, and
  is in charge of resuming a distributed operation in the emulator.
  It is given as a thread mediator to a DssThreadId, and is called
  back when the DSS operation has completed.  It completes the
  operation in the emulator, and releases the DssThreadId.

  A distributed operation is often initiated by an Oz thread.
  However, for the sake of simplicity, we do not mix DssThreadIds with
  Oz threads.  An Oz thread is always resumed by means of a control
  variable.  The corresponding SuspendedOperation binds the control
  variable, and releases the DssThreadId.

  Note.  A new SuspendedOperation automatically makes itself the
  thread mediator of the current DssThreadId, and forces the renewal
  of the current DssThreadId.

 */



// return the current DssThreadId
DssThreadId* currentThreadId();

// Those ones are called automatically by SuspendedOperations...
// set the ThreadMediator of the current DssThreadId (which is renewed)
void setCurrentThreadMediator(ThreadMediator* tm);
// release a DssThreadId after usage
void releaseThreadId(DssThreadId* tid);



// garbage collect SuspendedOperation objects, and DssThreadIds too
void gCollectSuspendedOperations();



// generic class SuspendedOperation
class SuspendedOperation : public ThreadMediator {
  friend void gCollectSuspendedOperations();

protected:
  TaggedRef ctlVar;           // control var for thread synchronization
  Mediator* mediator;         // entity mediator
  DssThreadId* threadId;      // which DssThreadId is mediated
  SuspendedOperation *next;   // used for garbage collection

  // suspend the current thread on the control variable
  void suspend();
  
  // resume the suspended thread (synchronize ctlVar)
  void resume();
  void resumeRaise(TaggedRef exc);
  void resumeUnify(TaggedRef a, TaggedRef b);
  void resumeApply(TaggedRef p, TaggedRef args);

public:
  SuspendedOperation(Mediator*);
  Mediator* getMediator() { return mediator; }
  bool gc();        // returns TRUE and collect stuff if needed
  
  // inherited from ThreadMediator
  virtual void resumeDoLocal() = 0; 
  virtual void resumeRemoteDone(PstInContainerInterface* pstin) = 0;
  virtual void resumeFailed();

  // returns TRUE and collect stuff if needed
  virtual bool gCollect() = 0;
};


// dummy suspended operation (nothing to do for resume)
class SuspendedDummy : public SuspendedOperation {
public:
  SuspendedDummy();
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface* pstin);
  bool gCollect();
};


// suspended cell operations
class SuspendedCellOp: public SuspendedOperation {
private:
  OperationTag op;
  TaggedRef    args[1];
  TaggedRef    result;
public:
  SuspendedCellOp(Mediator*, OperationTag, TaggedRef*, TaggedRef*);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface*);
  bool gCollect();
};



// suspended lock operations
class SuspendedLockTake: public SuspendedOperation {
private:
  TaggedRef ozthread;
public:
  SuspendedLockTake(Mediator*, TaggedRef);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface* pstin);
  bool gCollect();
};

class SuspendedLockRelease: public SuspendedOperation {
private:
  TaggedRef ozthread;
public:
  // Note: we do not suspend the requesting thread!
  SuspendedLockRelease(Mediator*, TaggedRef);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface* pstin);
  bool gCollect();
};


// suspended array operations
class SuspendedArrayOp: public SuspendedOperation {
private:
  OperationTag op;
  TaggedRef    args[2];
  TaggedRef    result;
public:
  SuspendedArrayOp(Mediator*, OperationTag, TaggedRef*, TaggedRef*);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface*);
  bool gCollect();
};


// suspended dictionary operations
class SuspendedDictionaryOp: public SuspendedOperation {
private:
  OperationTag op;
  TaggedRef    args[3];
  TaggedRef    result;
public:
  SuspendedDictionaryOp(Mediator*, OperationTag, TaggedRef*, TaggedRef*);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface*);
  bool gCollect();
};


// suspended object operations (on features)
class SuspendedObjectOp: public SuspendedOperation {
private:
  OperationTag op;
  TaggedRef    args[2];
  TaggedRef    result;
public:
  SuspendedObjectOp(Mediator*, OperationTag, TaggedRef*, TaggedRef*);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface*);
  bool gCollect();
};

// suspended object state operations
class SuspendedObjectStateOp: public SuspendedOperation {
private:
  OperationTag op;
  TaggedRef    args[2];
  TaggedRef    result;
public:
  SuspendedObjectStateOp(Mediator*, OperationTag, TaggedRef*, TaggedRef*);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface*);
  bool gCollect();
};


// suspended chunk operations
class SuspendedChunkOp: public SuspendedOperation {
private:
  OperationTag op;
  TaggedRef    args[2];
  TaggedRef    result;
public:
  SuspendedChunkOp(Mediator*, OperationTag, TaggedRef*, TaggedRef*);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface*);
  bool gCollect();
};


// generic dot operation for distributed chunks, dictionaries, etc.
class SuspendedGenericDot: public SuspendedOperation {
private:
  OZ_Term key;
  OZ_Term result;     // a variable
public:
  SuspendedGenericDot(Mediator*, OZ_Term, OZ_Term);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface* pstin);
  bool gCollect();
};


// generic call operation for distributed procedures and objects
class SuspendedCall : public SuspendedOperation {
private:
  OZ_Term args;     // list of arguments
public:
  SuspendedCall(Mediator*, OZ_Term);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface* pstin);
  bool gCollect();
};


// class operations
class SuspendedClassGet : public SuspendedOperation {
public:
  SuspendedClassGet(Mediator*);
  void resumeDoLocal();
  void resumeRemoteDone(PstInContainerInterface* pstin);
  bool gCollect();
};


#endif 
