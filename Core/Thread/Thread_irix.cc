/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/


/*
 *  Thread_irix.cc: Irix implementation of the Thread library
 *
 *  Written by:
 *   Author: Steve Parker
 *   Department of Computer Science
 *   University of Utah
 *   Date: June 1997
 *
 *  Copyright (C) 1997 SCI Group
 */

/*
 * User signals:
 *    SIGQUIT: Tells all of the processes to quit
 */

// This is brutal, but effective.  We want this file to have access
// to the private members of Thread, without having to explicitly
// declare friendships in the class.
#define private public
#define protected public
#include <Core/Thread/Thread.h>
#undef private
#undef protected
#include <Core/Thread/AtomicCounter.h>
#include <Core/Thread/Barrier.h>
#include <Core/Thread/ConditionVariable.h>
#include <Core/Thread/Mutex.h>
#include <Core/Thread/Semaphore.h>
#include <Core/Thread/ThreadError.h>
#include <Core/Thread/ThreadGroup.h>
#include <Core/Thread/Time.h>
#include <Core/Thread/WorkQueue.h>
#include "Thread_unix.h"
#include <abi_mutex.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <ulocks.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/signal.h>
#include <sys/sysmp.h>
#include <sys/syssgi.h>
#include <sys/time.h>
#include <sys/types.h>
extern "C" {
#include <sys/pmo.h>
#include <fetchop.h>
}

/*
 * The irix implementation uses the default version of CrowdMonitor
 * and RecursiveMutex.  It provides native implementations of
 * AtomicCounter (using the Origin fetch&op counters if available),
 * Barrier, Mutex, and Semaphore.
 *
 */

#include "CrowdMonitor_default.cc"
#include "RecursiveMutex_default.cc"


extern "C" int __ateachexit(void(*)());

#define MAXBSTACK 10
#define MAXTHREADS 4000

#define N_POOLMUTEX 301

namespace SCIRun {

struct Thread_private {
  Thread* thread;
  int pid;
  caddr_t sp;
  caddr_t stackbot;
  size_t stacklen;
  usema_t* startup;
  usema_t* done;
  usema_t* delete_ready;
  Thread::ThreadState state;
  int bstacksize;
  const char* blockstack[MAXBSTACK];
};

static Thread_private* idle_main;
static Thread_private* idle[MAXTHREADS];
static int nidle;
static Thread_private* active[MAXTHREADS];
static int nactive;
static bool initialized;
static usema_t* schedlock;
static usptr_t* arena;
static atomic_reservoir_t reservoir;
static bool use_fetchop=true;

static usptr_t* main_sema;
static int devzero_fd;
static int main_pid;
static bool exiting;
static int exit_code;
static bool aborting;
static int nprocessors;
static usema_t* control_c_sema;

static void install_signal_handlers();

struct ThreadLocalMemory {
  Thread* current_thread;
};
ThreadLocalMemory* thread_local;

static
void
lock_scheduler()
{
  if(!initialized)
    Thread::initialize();
  if(uspsema(schedlock) == -1)
    throw ThreadError(std::string("lock_scheduler failed")
		      +strerror(errno));
}

static
void
unlock_scheduler()
{
  if(usvsema(schedlock) == -1)
    throw ThreadError(std::string("unlock_scheduler failed")
		      +strerror(errno));
}

void
Thread_shutdown(Thread* thread, bool actually_exit)
{
  Thread_private* priv=thread->priv_;
  int pid=getpid();

  if(usvsema(priv->done) == -1)
    throw ThreadError(std::string("usvema failed on priv->done")
		      +strerror(errno));

  // Wait to be deleted...
  if(priv->delete_ready){
    if(uspsema(priv->delete_ready) == -1)
      throw ThreadError(std::string("uspsema failed on priv->delete_ready")
			+strerror(errno));
  }
  // Allow this thread to run anywhere...
  if(thread->cpu_ != -1)
    thread->migrate(-1);

  delete thread;

  priv->thread=0;
  thread_local->current_thread=0;
  lock_scheduler();
  /* Remove it from the active queue */
  int i;
  for(i=0;i<nactive;i++){
    if(active[i]==priv)
      break;
  }
  for(i++;i<nactive;i++){
    active[i-1]=active[i];
  }
  nactive--;
  if(priv->pid != main_pid){
    if(!actually_exit){
      idle[nidle]=priv;
      nidle++;
    }
  } else {
    idle_main=priv;
  }
  unlock_scheduler();
  Thread::checkExit();
  if(pid == main_pid){
    priv->state=Thread::PROGRAM_EXIT;
    if(uspsema(main_sema) == -1)
      throw ThreadError(std::string("uspsema failed on main_sema")
			+strerror(errno));
  }
  if(actually_exit)
    _exit(0);
}

/*
 * This is the ateachexit handler
 */
static
void
Thread_exit()
{
  if(exiting)
    return;
  Thread* self=Thread::self();
  Thread_shutdown(self, false);
}

static
void
wait_shutdown()
{
  long n;
  if((n=prctl(PR_GETNSHARE)) > 1){
    fprintf(stderr, "Waiting for %d threads to shut down: ", (int)(n-1));
    sginap(10);
    int delay=10;
    while((n=prctl(PR_GETNSHARE)) > 1){
      fprintf(stderr, "%d...", (int)(n-1));
      sginap(delay);
      delay+=10;
    }
    fprintf(stderr, "done\n");
  }
}

void
Thread::allow_sgi_OpenGL_page0_sillyness()
{
#if 0
  if(mprotect(0, getpagesize(), PROT_READ|PROT_WRITE) != 0){
    fprintf(stderr, "\007\007!!! Strange error re-mapping page 0 - tell Steve this number: %d\n", errno);
  }
#endif
}

void
Thread::disallow_sgi_OpenGL_page0_sillyness()
{
  if(mprotect(0, getpagesize(), PROT_NONE) == -1){
    fprintf(stderr, "\007\007!!! Strange error protecting page 0 - tell Steve this number: %d\n", errno);
  }
}

/*
 * Intialize threads for irix
 */
void
Thread::initialize()
{
  if(initialized)
    return;
  // disallow_sgi_OpenGL_page0_sillyness();
  usconfig(CONF_ARENATYPE, US_SHAREDONLY);
  usconfig(CONF_INITSIZE, 30*1024*1024);
  usconfig(CONF_INITUSERS, (unsigned int)140);
  arena=usinit("/dev/zero");
  if(!arena)
    throw ThreadError(std::string("Error calling usinit: ")
		      +strerror(errno));

  // Fetchop init prints out a really annoying message.  This
  // ugly code is to make it not do that
  // failed to create fetchopable area error
  int tmpfd=dup(2);
  close(2);
  int nullfd=open("/dev/null", O_WRONLY);
  if(nullfd != 2)
    throw ThreadError("Wrong FD returned from open");
  reservoir=atomic_alloc_reservoir(USE_DEFAULT_PM, 500, NULL);
  close(2);
  int newfd=dup(tmpfd);
  if(newfd != 2)
    throw ThreadError("Wrong FD returned from dup!");
  close(tmpfd);

  if(!reservoir){
    use_fetchop=false;
  } else {
    use_fetchop=true;
  }

  devzero_fd=open("/dev/zero", O_RDWR);
  if(devzero_fd == -1)
    throw ThreadError(std::string("Error opening /dev/zero: ")
		      +strerror(errno));

  schedlock=usnewsema(arena, 1);
  main_sema=usnewsema(arena, 0);
  nprocessors=Thread::numProcessors();

  control_c_sema=usnewsema(arena, 1);
  if(!control_c_sema)
    throw ThreadError(std::string("Error creating semaphore")
		      +strerror(errno));

  main_pid=getpid();
  /*
   * The functionality relies on SGI's __ateachexit function.  If
   * that ever goes away, then we will probably have to write our
   * own exit.
   */
  __ateachexit(Thread_exit);

  thread_local=(ThreadLocalMemory*)mmap(0, sizeof(ThreadLocalMemory),
					PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_LOCAL,
					devzero_fd, 0);
  /*
   * We have to say that we are initialized here, because the
   * following calls can create synchronization primitives that
   * will rely only on the above initialization.
   */
  initialized=true;

  ThreadGroup::s_default_group=new ThreadGroup("default group", 0);
  Thread* mainthread=new Thread(ThreadGroup::s_default_group, "main");
  mainthread->priv_=new Thread_private;
  mainthread->priv_->pid=main_pid;
  mainthread->priv_->thread=mainthread;
  mainthread->priv_->state=RUNNING;
  mainthread->priv_->bstacksize=0;
  mainthread->priv_->done=usnewsema(arena, 0);
  mainthread->priv_->delete_ready=0;
  lock_scheduler();
  active[nactive]=mainthread->priv_;
  nactive++;
  unlock_scheduler();

  thread_local->current_thread=mainthread;
  if(!getenv("THREAD_NO_CATCH_SIGNALS") && !getenv("DEBUGGER_SHELL"))
    install_signal_handlers();
}

static
void
run_threads(void* priv_v, size_t)
{
  Thread_private* priv=(Thread_private*)priv_v;
  if(!getenv("THREAD_NO_CATCH_SIGNALS"))
    install_signal_handlers();
  for(;;){
    /* Wait to be started... */
    priv->state=Thread::IDLE;
    if(uspsema(priv->startup) == -1)
      throw ThreadError(std::string("uspsema failed on priv->startup")
			+strerror(errno));
    thread_local->current_thread=priv->thread;
    priv->state=Thread::RUNNING;
    priv->thread->run_body();
    priv->state=Thread::SHUTDOWN;
    Thread_shutdown(priv->thread, false);
    priv->state=Thread::IDLE;
  }
}

void
Thread::exit()
{
  Thread* self=Thread::self();
  Thread_shutdown(self, true);
}

void
Thread::checkExit()
{
  lock_scheduler();
  int done=true;
  for(int i=0;i<nactive;i++){
    Thread_private* p=active[i];
    if(!p->thread->isDaemon()){
      done=false;
      break;
    }
  }
  unlock_scheduler();

  if(done)
    Thread::exitAll(0);
}

void
Thread::exitAll(int code)
{
  exiting=true;
  exit_code=code;

  if(nactive == 0)
    ::exit(code);

  // We want to do this:
  //   kill(0, SIGQUIT);
  // but that has the unfortunate side-effect of sending a signal
  // to things like par, perfex, and other things that somehow end
  // up in the same share group.  So we have to kill each process
  // independently...
  pid_t me=getpid();
  for(int i=0;i<nidle;i++){
    if(idle[i]->pid != me)
      kill(idle[i]->pid, SIGQUIT);
  }
  for(int i=0;i<nactive;i++){
    if(active[i]->pid != me)
      kill(active[i]->pid, SIGQUIT);
  }
  if(idle_main && idle_main->pid != me)
    kill(idle_main->pid, SIGQUIT);
  kill(me, SIGQUIT);
  fprintf(stderr, "Should not reach this point!\n");
}

/*
 * Startup the given thread...
 */
void
Thread::os_start(bool stopped)
{
  /* See if there is a thread waiting around ... */
  if(!initialized){
    Thread::initialize();
  }
  lock_scheduler();
  if(nidle){
    nidle--;
    priv_=idle[nidle];
  } else {
    priv_=new Thread_private;
    priv_->stacklen=stacksize_;
    priv_->stackbot=(caddr_t)mmap(0, priv_->stacklen, PROT_READ|PROT_WRITE,
				   MAP_PRIVATE, devzero_fd, 0);
    priv_->sp=priv_->stackbot+priv_->stacklen-1;
    if((long)priv_->sp == -1)
      throw ThreadError(std::string("Not enough memory for thread stack")
			+strerror(errno));
    priv_->startup=usnewsema(arena, 0);
    priv_->done=usnewsema(arena, 0);
    priv_->delete_ready=usnewsema(arena, 0);
    priv_->state=STARTUP;
    priv_->bstacksize=0;
    priv_->pid=sprocsp(run_threads, PR_SALL, priv_, priv_->sp, priv_->stacklen);
    if(priv_->pid == -1)
      throw ThreadError(std::string("Cannot start new thread")
			+strerror(errno));
  }
  priv_->thread=this;
  active[nactive]=priv_;
  nactive++;
  unlock_scheduler();
  if(stopped){
    if(blockproc(priv_->pid) != 0)
      throw ThreadError(std::string("blockproc returned error: ")
			+strerror(errno));
  }
  /* The thread is waiting to be started, release it... */
  if(usvsema(priv_->startup) == -1)
    throw ThreadError(std::string("usvsema failed on priv_->startup")
		      +strerror(errno));
}

void
Thread::stop()
{
  if(blockproc(priv_->pid) != 0)
    throw ThreadError(std::string("blockproc returned error: ")
		      +strerror(errno));
}

void
Thread::resume()
{
  if(unblockproc(priv_->pid) != 0)
    throw ThreadError(std::string("unblockproc returned error: ")
		      +strerror(errno));
}

void
Thread::detach()
{
  if(detached_)
    throw ThreadError(std::string("Thread detached when already detached"));
  if(usvsema(priv_->delete_ready) == -1)
    throw ThreadError(std::string("usvsema returned error: ")
		      +strerror(errno));
  detached_=true;
}

void
Thread::join()
{
  Thread* us=Thread::self();
  int os=push_bstack(us->priv_, JOINING, threadname_);
  if(uspsema(priv_->done) == -1)
    throw ThreadError(std::string("uspsema returned error: ")
		      +strerror(errno));

  pop_bstack(us->priv_, os);
  detach();
}

void
ThreadGroup::gangSchedule()
{
  // This doesn't actually do anything on IRIX because it causes more
  // problems than it solves
}

/*
 * Thread block stack manipulation
 */
int
Thread::push_bstack(Thread_private* p, Thread::ThreadState state,
		    const char* name)
{
  int oldstate=p->state;
  p->state=state;
  p->blockstack[p->bstacksize]=name;
  p->bstacksize++;
  if(p->bstacksize>=MAXBSTACK)
    throw ThreadError("Blockstack Overflow!\n");
  return oldstate;
}

void
Thread::pop_bstack(Thread_private* p, int oldstate)
{
  p->bstacksize--;
  if(p->bstacksize < 0)
    throw ThreadError("Blockstack Underflow!\n");
  p->state=(Thread::ThreadState)oldstate;
}

int
Thread::get_thread_id()
{
  return priv_->pid;
}

/*
 * Signal handling cruft
 */
void
Thread::print_threads()
{
  FILE* fp=stderr;
  for(int i=0;i<nactive;i++){
    Thread_private* p=active[i];
    const char* tname=p->thread?p->thread->getThreadName():"???";
    fprintf(fp, " %d: %s (", p->pid, tname);
    if(p->thread){
      if(p->thread->isDaemon())
	fprintf(fp, "daemon, ");
      if(p->thread->isDetached())
	fprintf(fp, "detached, ");
    }
    fprintf(fp, "state=%s", Thread::getStateString(p->state));
    for(int i=0;i<p->bstacksize;i++){
      fprintf(fp, ", %s", p->blockstack[i]);
    }
    fprintf(fp, ")\n");
  }
  for(int i=0;i<nidle;i++){
    Thread_private* p=idle[i];
    fprintf(fp, " %d: Idle worker\n", p->pid);
  }
  if(idle_main){
    fprintf(fp, " %d: Completed main thread\n", idle_main->pid);
  }
}

/*
 * Handle sigquit - usually sent by control-C
 */
static
void
handle_quit(int sig, int /* code */, sigcontext_t*)
{
  if(exiting){
    if(getpid() == main_pid){
      wait_shutdown();
    }
    exit(exit_code);
  }
  // Try to acquire a lock.  If we can't, then assume that somebody
  // else already caught the signal...
  Thread* self=Thread::self();
  if(self==0)
    return; // This is an idle thread...
  if(sig == SIGINT){
    int st=uscpsema(control_c_sema);
    if(st==-1)
      throw ThreadError(std::string("uscpsema failed")
			+strerror(errno));
    
    if(st == 0){
      // This will wait until the other thread is done
      // handling the interrupt
      uspsema(control_c_sema);
      usvsema(control_c_sema);
      return;
    }
    // Otherwise, we handle the interrupt
  }

  const char* tname=self?self->getThreadName():"main?";

  // Kill all of the threads...
  char* signam=Core_Thread_signal_name(sig, 0);
  int pid=getpid();
  fprintf(stderr, "Thread \"%s\"(pid %d) caught signal %s\n", tname, pid, signam);
  if(sig==SIGINT){
    // Print out the thread states...
    Thread::niceAbort();
    usvsema(control_c_sema);
  } else {
    exiting=true;
    exit_code=1;
    exit(1);
  }
}

/*
 * Handle an abort signal - like segv, bus error, etc.
 */
static
void
handle_abort_signals(int sig, int /* code */, sigcontext_t* context)
{
  if(aborting)
    exit(0);
  struct sigaction action;
  sigemptyset(&action.sa_mask);
  action.sa_handler=SIG_DFL;
  action.sa_flags=0;
  if(sigaction(sig, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction (SIG_DFL) failed")
		      +strerror(errno));

  Thread* self=Thread::self();
  const char* tname=self?self->getThreadName():"idle or main";
#if defined(_LONGLONG)
  caddr_t addr=(caddr_t)context->sc_badvaddr;
#else
  caddr_t addr=(caddr_t)context->sc_badvaddr.lo32;
#endif
  char* signam=Core_Thread_signal_name(sig, addr);
  fprintf(stderr, "%c%c%cThread \"%s\"(pid %d) caught signal %s\n", 7,7,7,tname, getpid(), signam);
  Thread::niceAbort();

  action.sa_handler=(SIG_PF)handle_abort_signals;
  action.sa_flags=0;
  if(sigaction(sig, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction (restore) failed")
		      +strerror(errno));
}

/*
 * Setup signals for the current thread
 */
static
void
install_signal_handlers()
{
  struct sigaction action;
  sigemptyset(&action.sa_mask);
  action.sa_flags=0;

  action.sa_handler=(SIG_PF)handle_abort_signals;
  if(sigaction(SIGILL, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction(SIGILL) failed")
		      +strerror(errno));
  if(sigaction(SIGABRT, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction(SIGABRT) failed")
		      +strerror(errno));
  if(sigaction(SIGTRAP, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction(SIGTRAP) failed")
		      +strerror(errno));
  if(sigaction(SIGBUS, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction(SIGBUS) failed")
		      +strerror(errno));
  if(sigaction(SIGSEGV, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction(SIGSEGV) failed")
		      +strerror(errno));
  if(sigaction(SIGFPE, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction(SIGFPE) failed")
		      +strerror(errno));

  action.sa_handler=(SIG_PF)handle_quit;
  if(sigaction(SIGQUIT, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction(SIGQUIT) failed")
		      +strerror(errno));
  if(sigaction(SIGINT, &action, NULL) == -1)
    throw ThreadError(std::string("sigaction(SIGINT) failed")
		      +strerror(errno));
}

/*
 * Return the current thread...
 */
Thread*
Thread::self()
{
  return thread_local->current_thread;
}


int
Thread::numProcessors()
{
  static int nproc=-1;
  if(nproc==-1){
    char* np=getenv("SCI_NPROCESSORS");
    if(np){
      nproc = atoi(np);
      if(nproc<=0)
	nproc=1;
    } else {
      nproc = (int)sysmp(MP_NAPROCS);
    }
  }
  return nproc;
}

/*
 * Yield the CPU
 */
void
Thread::yield()
{
  sginap(0);
}

/*
 * Migrate the thread to a CPU.
 */
void
Thread::migrate(int proc)
{
  if(proc==-1){
    if(sysmp(MP_RUNANYWHERE_PID, priv_->pid) == -1)
      throw ThreadError(std::string("sysmp(MP_RUNANYWHERE_PID) failed")
			+strerror(errno));
  } else {
    if(sysmp(MP_MUSTRUN_PID, proc, priv_->pid) == -1)
      throw ThreadError(std::string("sysmp(_MP_MUSTRUN_PID) failed")
			+strerror(errno));
  }
  cpu_=proc;
}

/*
 * Mutex implementation
 */
Mutex::Mutex(const char* name)
  : name_(name)
{
#ifdef BROKEN
  if(!initialized){
    Thread::initialize();
  }
  priv_=(Mutex_private*)usnewlock(arena);
  if(!priv_)
    throw ThreadError(std::string("usnewlock failed")
		      +strerror(errno));
#else
  if(init_lock((abilock_t*)&priv_) != 0)
    throw ThreadError(std::string("init_lock failed"));
#endif
}

Mutex::~Mutex()
{
#ifdef BROKEN
  usfreelock((ulock_t)priv_, arena);
#else
#endif
}

void
Mutex::lock()
{
  Thread* t=Thread::self();
  int os;
  Thread_private* p=0;
  if(t){
    p=t->priv_;
    os=Thread::push_bstack(p, Thread::BLOCK_MUTEX, name_);
  }
#ifdef BROKEN
  if(ussetlock((ulock_t)priv_) == -1)
    throw ThreadError(std::string("ussetlock failed")
		      +strerror(errno));
#else
  spin_lock((abilock_t*)&priv_);
  if(t)
    Thread::pop_bstack(p, os);
#endif
}

bool
Mutex::tryLock()
{
#ifdef BROKEN
  int st=uscsetlock((ulock_t)priv_, 100);
  if(st==-1)
    throw ThreadError(std::string("uscsetlock failed")
		      +strerror(errno));
  return st!=0;
#else
  if(acquire_lock((abilock_t*)&priv_) == 0)
    return true;
  else
    return false;
#endif
}

void
Mutex::unlock()
{
#ifdef BROKEN
  if(usunsetlock((ulock_t)priv_) == -1)
    throw ThreadError(std::string("usunsetlock failed")
		      +strerror(errno));
#else
  if(release_lock((abilock_t*)&priv_) != 0)
    throw ThreadError(std::string("release_lock failed"));
#endif
}

/*
 * Semaphore implementation
 */

Semaphore::Semaphore(const char* name, int count)
  : name_(name)
{
  if(!initialized){
    Thread::initialize();
  }
  priv_=(Semaphore_private*)usnewsema(arena, count);
  if(!priv_)
    throw ThreadError(std::string("usnewsema failed")
		      +strerror(errno));
}

Semaphore::~Semaphore()
{
  if(priv_)
    usfreesema((usema_t*)priv_, arena);
}

void
Semaphore::down(int count)
{
  Thread_private* p=Thread::self()->priv_;
  int oldstate=Thread::push_bstack(p, Thread::BLOCK_SEMAPHORE, name_);
  for(int i=0;i<count;i++){
    if(uspsema((usema_t*)priv_) == -1)
      throw ThreadError(std::string("uspsema failed")
			+strerror(errno));
  }
  Thread::pop_bstack(p, oldstate);
}

bool
Semaphore::tryDown()
{
  int stry=uscpsema((usema_t*)priv_);
  if(stry == -1)
    throw ThreadError(std::string("uscpsema failed")
		      +strerror(errno));

  return stry;
}

void
Semaphore::up(int count)
{
  for(int i=0;i<count;i++){
    if(usvsema((usema_t*)priv_) == -1)
      throw ThreadError(std::string("usvsema failed")
			+strerror(errno));
  }
}

struct Barrier_private {
  Barrier_private();

  // These variables used only for single processor implementation
  Mutex mutex;
  ConditionVariable cond0;
  ConditionVariable cond1;
  int cc;
  int nwait;

  // Only for MP, non fetchop implementation
  barrier_t* barrier;

  // Only for fetchop implementation
  atomic_var_t* pvar;
  char pad[128];
  volatile int flag;  // We want this on it's own cache line
  char pad2[128];
};

Barrier_private::Barrier_private()
  : cond0("Barrier condition 0"), cond1("Barrier condition 1"),
    mutex("Barrier lock"), nwait(0), cc(0)
{
  if(nprocessors > 1){
    if(use_fetchop){
      flag=0;
      pvar=atomic_alloc_variable(reservoir, 0);
      //	    fprintf(stderr, "***Alloc: %p\n", pvar);
      if(!pvar)
	throw ThreadError(std::string("fetchop_alloc failed")
			  +strerror(errno));

      storeop_store(pvar, 0);
    } else {
      // Use normal SGI barrier
      barrier=new_barrier(arena);
    }
  }
}   

Barrier::Barrier(const char* name)
  : name_(name)
{
  if(!initialized){
    Thread::initialize();
  }
  priv_=new Barrier_private;
}

Barrier::~Barrier()
{
  if (nprocessors > 1) {
    if(use_fetchop){
      //	    fprintf(stderr, "***Alloc free: %p\n", priv_->pvar);
      //	    atomic_free_variable(reservoir, priv_->pvar);
    } else {
      free_barrier(priv_->barrier);
    }
  }
  delete priv_;
}

void
Barrier::wait(int n)
{
  Thread_private* p=Thread::self()->priv_;
  int oldstate=Thread::push_bstack(p, Thread::BLOCK_BARRIER, name_);
  if(nprocessors > 1){
    if(use_fetchop){
      int gen=priv_->flag;
      atomic_var_t val=atomic_fetch_and_increment(priv_->pvar);
      if(val == n-1){
	storeop_store(priv_->pvar, 0);
	priv_->flag++;
      }
      while(priv_->flag==gen)
	/* spin */ ;
    } else {
      barrier(priv_->barrier, n);
    }
  } else {
    priv_->mutex.lock();
    ConditionVariable& cond=priv_->cc?priv_->cond0:priv_->cond1;
    priv_->nwait++;
    if(priv_->nwait == n){
      // Wake everybody up...
      priv_->nwait=0;
      priv_->cc=1-priv_->cc;
      cond.conditionBroadcast();
    } else {
      cond.wait(priv_->mutex);
    }
    priv_->mutex.unlock();
  }
  Thread::pop_bstack(p, oldstate);
}

struct AtomicCounter_private {
  AtomicCounter_private();

  // These variables used only for non fectchop implementation
  Mutex lock;
  int value;
};

AtomicCounter_private::AtomicCounter_private()
  : lock("AtomicCounter lock")
{
}

AtomicCounter::AtomicCounter(const char* name)
  : name_(name)
{
  if(!initialized){
    Thread::initialize();
  }
  if(use_fetchop){
    priv_=(AtomicCounter_private*)atomic_alloc_variable(reservoir, 0);
    // 	fprintf(stderr, "***Alloc atomcounter: %p\n", priv_);
    if(!priv_)
      throw ThreadError(std::string("fetchop_alloc failed")
			+strerror(errno));
  } else {
    priv_=new AtomicCounter_private;
    priv_->value=0;
  }
}

AtomicCounter::AtomicCounter(const char* name, int value)
  : name_(name)
{
  if(!initialized){
    Thread::initialize();
  }
  if(use_fetchop){
    priv_=(AtomicCounter_private*)atomic_alloc_variable(reservoir, 0);
    // 	fprintf(stderr, "***Alloc atomcounter: %p\n", priv_);
    if(!priv_)
      throw ThreadError(std::string("fetchop_alloc failed")
			+strerror(errno));
    atomic_store((atomic_var_t*)priv_, value);
  } else {
    priv_=new AtomicCounter_private;
    priv_->value=value;
  }
}

AtomicCounter::~AtomicCounter()
{
  if(use_fetchop){
    // 	fprintf(stderr, "***Alloc free: %p\n", priv_);
    //	atomic_free_variable(reservoir, (atomic_var_t*)priv_);
  } else {
    delete priv_;
  }
}

AtomicCounter::operator int() const
{
  if(use_fetchop){
    return (int)atomic_load((atomic_var_t*)priv_);
  } else {
    return priv_->value;
  }
}

int
AtomicCounter::operator++()
{
  if(use_fetchop){
    // We do not use the couldBlock/couldBlockDone pairs here because
    // they are so fast (microsecond), and never block...
    return (int)atomic_fetch_and_increment((atomic_var_t*)priv_)+1;
  } else {
    int oldstate=Thread::couldBlock(name_);
    priv_->lock.lock();
    int ret=++priv_->value;
    priv_->lock.unlock();
    Thread::couldBlockDone(oldstate);
    return ret;
  }
}

int
AtomicCounter::operator++(int)
{
  if(use_fetchop){
    return (int)atomic_fetch_and_increment((atomic_var_t*)priv_);
  } else {
    int oldstate=Thread::couldBlock(name_);
    priv_->lock.lock();
    int ret=priv_->value++;
    priv_->lock.unlock();
    Thread::couldBlockDone(oldstate);
    return ret;
  }
}

int
AtomicCounter::operator--()
{
  if(use_fetchop){
    return (int)atomic_fetch_and_decrement((atomic_var_t*)priv_)-1;
  } else {
    int oldstate=Thread::couldBlock(name_);
    priv_->lock.lock();
    int ret=--priv_->value;	
    priv_->lock.unlock();
    Thread::couldBlockDone(oldstate);
    return ret;
  }
}

int
AtomicCounter::operator--(int)
{
  if(use_fetchop){
    return (int)atomic_fetch_and_increment((atomic_var_t*)priv_);
  } else {
    int oldstate=Thread::couldBlock(name_);
    priv_->lock.lock();
    int ret=priv_->value--;
    priv_->lock.unlock();
    Thread::couldBlockDone(oldstate);
    return ret;
  }
}

void
AtomicCounter::set(int v)
{
  if(use_fetchop){
    atomic_store((atomic_var_t*)priv_, v);
  } else {
    int oldstate=Thread::couldBlock(name_);
    priv_->lock.lock();
    priv_->value=v;
    priv_->lock.unlock();
    Thread::couldBlockDone(oldstate);
  }
}

struct ConditionVariable_private {
  int num_waiters;
  usema_t* semaphore;
  bool pollsema;
};

ConditionVariable::ConditionVariable(const char* name)
  : name_(name)
{
  priv_=new ConditionVariable_private();
  priv_->num_waiters=0;
  priv_->pollsema=false;
  priv_->semaphore=usnewsema(arena, 0);
  if(!priv_->semaphore)
    throw ThreadError(std::string("usnewsema failed")
		      +strerror(errno));
}

ConditionVariable::~ConditionVariable()
{
  if(priv_->semaphore){
    if(priv_->pollsema)
      usfreepollsema(priv_->semaphore, arena);
    else
      usfreesema(priv_->semaphore, arena);
  }
  delete priv_;
}

void
ConditionVariable::wait(Mutex& m)
{
  if(priv_->pollsema){
    if(!timedWait(m, 0)){
      throw ThreadError("timedWait with infinite timeout didn't wait");
    }
  } else {
    priv_->num_waiters++;
    m.unlock();
    Thread_private* p=Thread::self()->priv_;
    int oldstate=Thread::push_bstack(p, Thread::BLOCK_CONDITIONVARIABLE, name_);
    // Block until woken up by signal or broadcast
    if(uspsema(priv_->semaphore) == -1)
      throw ThreadError(std::string("uspsema failed")
			+strerror(errno));
    Thread::couldBlockDone(oldstate);
    m.lock();
  }
}

/*
 * pollable semaphores are strange beasts.  This code isn't meant
 * to be understood.
 */
bool
ConditionVariable::timedWait(Mutex& m, const struct timespec* abstime)
{
  if(!priv_->pollsema){
    // Convert to a pollable semaphore...
    if(priv_->num_waiters){
      // There would be much code for this case, so I hope it
      // never happens.
      throw ThreadError("Cannot convert to timedWait/pollable semaphore if regular wait is in effect");
    } else {
      usfreesema(priv_->semaphore, arena);
      priv_->pollsema=true;
      priv_->semaphore=usnewpollsema(arena, 0);
      if(!priv_->semaphore)
	throw ThreadError(std::string("usnewpollsema failed")
			  +strerror(errno));
    }
  }

  priv_->num_waiters++;

  /*
   * Open the file descriptor before we unlock the mutex because
   * it is required for the conditionSignal to call usvsema.
   */
  int pollfd = usopenpollsema(priv_->semaphore, S_IRWXU);
  if(pollfd == -1)
    throw ThreadError(std::string("usopenpollsema failed")
		      +strerror(errno));

  m.unlock();

  Thread_private* p=Thread::self()->priv_;
  int oldstate=Thread::push_bstack(p, Thread::BLOCK_CONDITIONVARIABLE, name_);

  // Block until woken up by signal or broadcast or timed out
  bool success;

  int ss = uspsema(priv_->semaphore);
  if(ss == -1){
    throw ThreadError(std::string("uspsema failed")
		      +strerror(errno));
  } else if(ss == 0){
    for(;;){
      struct timeval timeout;
      struct timeval* timeoutp;
      if(abstime){
	struct timeval now;
	if(gettimeofday(&now, 0) != 0)
	  throw ThreadError(std::string("gettimeofday failed")
			    +strerror(errno));
	/* Compute the difference. */
	timeout.tv_sec = abstime->tv_sec - now.tv_sec;
	timeout.tv_usec = abstime->tv_nsec/1000 - now.tv_usec;
	if(timeout.tv_usec < 0){
	  long secs = (-timeout.tv_usec)/1000000+1;
	  timeout.tv_sec-=secs;
	  timeout.tv_usec+=secs*1000000;
	}
	if(timeout.tv_sec < 0){
	  timeout.tv_sec=0;
	  timeout.tv_usec=0;
	}
	timeoutp=&timeout;
      } else {
	timeoutp=0;
      }

      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(pollfd, &fds);
      int s = select(pollfd+1, &fds, 0, 0, timeoutp);
      if(s == -1){
	if(errno == EINTR)
	  continue;
	throw ThreadError(std::string("select failed")
			  +strerror(errno));
      } else if(s == 0){
	// Timed out...
	success = false;
	break;
      } else {
	// Got it
	success=true;
	break;
      }
    }
  } else {
    // Semaphore available...
    success=true;
  }

  // Lock now so that the usvsema below will not interfere
  // with a poorly timed signal
  m.lock();
  if(!success){
    // v the semaphore so that the next p will work
    if(usvsema(priv_->semaphore) == -1)
      throw ThreadError(std::string("usvsema failed")
			+strerror(errno));
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pollfd, &fds);
    timeval zerotimeout;
    zerotimeout.tv_sec = 0;
    zerotimeout.tv_usec = 0;
    int s = select(pollfd+1, &fds, 0, 0, &zerotimeout);
    if(s == -1){
      throw ThreadError(std::string("select failed")
			+strerror(errno));
    } else if(s == 0){
      // Timed out...
      throw ThreadError("ConditionVariable timeout recover failed");
    } else {
      // Got it
    }
  }
  if(usclosepollsema(priv_->semaphore) != 0)
    throw ThreadError(std::string("usclosepollsema failed")
		      +strerror(errno));
  Thread::couldBlockDone(oldstate);
  return success;
}

void
ConditionVariable::conditionSignal()
{
  if(priv_->num_waiters > 0){
    priv_->num_waiters--;
    if(usvsema(priv_->semaphore) == -1)
      throw ThreadError(std::string("usvsema failed")
			+strerror(errno));
  }
}

void
ConditionVariable::conditionBroadcast()
{
  while(priv_->num_waiters > 0){
    priv_->num_waiters--;
    if(usvsema(priv_->semaphore) == -1)
      throw ThreadError(std::string("usvsema failed")
			+strerror(errno));
  }
}

} // End namespace SCIRun
