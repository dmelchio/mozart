/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: mehl
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  implementation of threads and queues of threads with priorities
  ------------------------------------------------------------------------
*/

#if defined(__GNUC__) && !defined(NOPRAGMA)
#pragma implementation "thread.hh"
#endif

/* some random comments on threads:

   threads are 'switch'ed, if
     - 'timed out' by alarm clock
     - 'finish'ed
     - 'displace'd by higher priority
     - 'freeze'd

   need for thread switching is signaled by 'ThreadSwitch' S-Flag

   "thread queue"
     - has HEAD and TAIL
     - HEAD has always the highest priority and is the next to run

   "current thread"
     - the currently running thread

   */

#include "types.hh"

#include "actor.hh"
#include "alarm.hh"
#include "am.hh"
#include "io.hh"
#include "suspensi.hh"
#include "taskstk.hh"
#include "thread.hh"
#include "verbose.hh"

#ifdef OUTLINE
#define inline
#include "thread.icc"
#undef inline
#endif

// --------------------------------------------------------------------------


/* class Thread
   static variables
     Head: pointer to the head of the thread queue
     Tail: pointer to the tail of the thread queue
     am.currentThread: pointer to the current thread
     am.rootThread: pointer to the root thread
     am.conf.timeSlice: the overall time slice of a thread in msec
     am.conf.defaultPriority: the user priority
     am.conf.systemPriority: the system priority
   member data
     next: the next thread in the thread queue
     prev: the prev thread in the thread queue
     priority: the thread priority
               -MININT ... +MAXINT
	       low     ... high priority
     taskStack: the stack of elaboration tasks
     */


// head and tail of the thread queue
Thread *Thread::Head;
Thread *Thread::Tail;
Thread *Thread::FreeList;

/*
 * Toplevel is a queue of toplevel queries, which must be executed
 * sequentially.
 *   see Thread::checkToplevel()  called when disposing a thread
 *       Thread::addToplevel()    called when a new query arrives
 */
class Toplevel {
public:
  ProgramCounter pc;
  Toplevel *next;
  Toplevel(ProgramCounter p, Toplevel *nxt) : pc(p), next(nxt) {}
};

Toplevel *Thread::ToplevelQueue;

void Thread::Init()
{
  Head     = (Thread *) NULL;
  Tail     = (Thread *) NULL;
  FreeList = (Thread *) NULL;
  ToplevelQueue = (Toplevel *) NULL;
  am.currentThread = (Thread *) NULL;
  am.rootThread = new Thread();
  am.rootThread->init(am.conf.defaultPriority,am.rootBoard);
}

/*
 * the mode is 3, if all tasks on the thread have seq mode
 */
void Thread::switchMode(int newMode) {
  Assert((mode&1) != newMode);
  Assert(SEQMODE==1 && PARMODE==0);
  if (taskStack.isEmpty()) {
    if (newMode == SEQMODE) {
      mode=ALLSEQMODE;
    } else {
      mode=newMode;
    }
  } else {
    taskStack.pushMode(mode&1);
    mode=newMode;
  }
}

/*
 * The Toplevel
 */
void Thread::checkToplevel()
{
  if (ToplevelQueue) {
    Verbose((VERB_THREAD,"Thread::checkToplevel: pushNext: 0x%x\n",this));
    Toplevel **last;
    for (last = &ToplevelQueue;
	 ((*last)->next) != NULL;
	 last = &((*last)->next)) {
    }
    pushToplevel((*last)->pc);
    *last = (Toplevel *) NULL;
  }
}

void Thread::addToplevel(ProgramCounter pc)
{
  if (taskStack.isEmpty()) {
    Verbose((VERB_THREAD,"Thread::addToplevel: push\n"));
    pushToplevel(pc);
  } else {
    Verbose((VERB_THREAD,"Thread::addToplevel: delay\n"));
    ToplevelQueue = new Toplevel(pc,ToplevelQueue);
  }
}

void Thread::pushToplevel(ProgramCounter pc)
{
  am.rootBoard->incSuspCount();
  taskStack.pushCont(am.rootBoard,pc,am.toplevelVars);
  if (this!=am.currentThread && !this->isScheduled()) {
    schedule();
  }
}

/* for gdb debugging: cannot access static member data */
Thread *Thread::GetHead()
{
  return Head;
}

/* for gdb debugging: cannot access static member data */
Thread *Thread::GetTail()
{
  return Tail;
}


void Thread::ScheduleSuspCont(SuspContinuation *c, Bool wasExtSusp)
{
  Thread *t = newThread(c->getPriority(),c->getBoard());
  if (am.currentSolveBoard != (Board *) NULL || wasExtSusp == OK) {
    Board *nb = c->getBoard ();
    am.incSolveThreads (nb);
    t->setNotificationBoard (nb);
  }
  t->taskStack.pushCont(c->getBoard(),c->getPC(),c->getY(),c->getG(),
			c->getX(),c->getXSize(), NO);
  t->schedule();
}

void Thread::ScheduleSuspCCont(CFuncContinuation *c, Bool wasExtSusp,
			       Suspension *s)
{
  Thread *t = newThread(c->getPriority(),c->getBoard());
  if (am.currentSolveBoard != (Board *) NULL || wasExtSusp == OK) {
    Board *nb = c->getBoard();
    am.incSolveThreads(nb);
    t->setNotificationBoard(nb);
  }
  t->taskStack.pushCFunCont(c->getBoard(),c->getCFunc(),s,c->getX(),c->getXSize(),NO);
  t->schedule();
}


// create a new thread to reduce a solve actor; 
void Thread::ScheduleSolve (Board *b)
{
  DebugCheck ((b->isCommitted () == OK || b->isSolve () == NO),
	      error ("no solve board in Thread::ScheduleSolve ()"));
  // message("Thread::ScheduleSolve (@0x%x)\n", (void *) b->getActor ());
#ifdef NEWCOUNTER
  if (b->isNervousSolve()) {
    return;
  }
#endif

  Thread *t = newThread(b->getActor()->getPriority(),NULL);
#ifdef NEWCOUNTER
  t->taskStack.pushNervousSolve(b);
  b->setNervousSolve();
#else
  Board *nb = (b->getParentBoard ())->getSolveBoard ();
  am.incSolveThreads (nb);
  t->setNotificationBoard (nb);
  t->taskStack.pushNervous(b);
  b->setNervous();
#endif
  t->schedule();
}

#ifndef NEWCOUNTER
// create a new thread after wakeup (nervous)
void Thread::ScheduleWakeup(Board *b, Bool wasExtSusp)
{
  Thread *t = newThread(b->getActor()->getPriority(),b);
  if (am.currentSolveBoard != (Board *) NULL || wasExtSusp == OK) {
    am.incSolveThreads (b);
    t->setNotificationBoard(b);
  }
  t->taskStack.pushNervous(b);
  b->setNervous();
  t->schedule();
}
#endif
