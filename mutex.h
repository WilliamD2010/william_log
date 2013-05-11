/*
 * mutex.h
 *
 *  Created on: Jul 20, 2012
 *      Author: changqwa
 */

// TRICKY IMPLEMENTATION NOTE:
// This class is designed to be safe to use during
// dynamic-initialization -- that is, by global constructors that are
// run before main() starts.  The issue in this case is that
// dynamic-initialization happens in an unpredictable order, and it
// could be that someone else's dynamic initializer could call a
// function that tries to acquire this mutex -- but that all happens
// before this mutex's constructor has run.  (This can happen even if
// the mutex and the function that uses the mutex are in the same .cc
// file.)  Basically, because Mutex does non-trivial work in its
// constructor, it's not, in the naive implementation, safe to use
// before dynamic initialization has run on it.
//
// The solution used here is to pair the actual mutex primitive with a
// bool that is set to true when the mutex is dynamically initialized.
// (Before that it's false.)  Then we modify all mutex routines to
// look at the bool, and not try to lock/unlock until the bool makes
// it to true (which happens after the Mutex constructor has run.)
//
// This works because before main() starts -- particularly, during
// dynamic initialization -- there are no threads, so a) it's ok that
// the mutex operations are a no-op, since we don't need locking then
// anyway; and b) we can be quite confident our bool won't change
// state between a call to Lock() and a call to Unlock() (that would
// require a global constructor in one translation unit to call Lock()
// and another global constructor in another translation unit to call
// Unlock() later, which is pretty perverse).
//
// That said, it's tricky, and can conceivably fail; it's safest to
// avoid trying to acquire a mutex in a global constructor, if you
// can.  One way it can fail is that a really smart compiler might
// initialize the bool to true at static-initialization time (too
// early) rather than at dynamic-initialization time.  To discourage
// that, we set is_safe_ to true in code (not the constructor
// colon-initializer) and set it to true via a function that always
// evaluates to true, but that the compiler can't know always
// evaluates to true.  This should be good enough.

#ifndef MUTEX_H_
#define MUTEX_H_

#include <pthread.h>
typedef pthread_rwlock_t MutexType;

#define MUTEX_NAMESPACE glog_internal_namespace_

namespace MUTEX_NAMESPACE {

class Mutex {
 public:
  // Create a Mutex that is not held by anybody.  This constructor is
  // typically used for Mutexes allocated on the heap or the stack.
  // See below for a recommendation for constructing global Mutex
  // objects.
  inline Mutex();
  // Destructor
  inline ~Mutex();

  inline void Lock();    // Block if needed until free then acquire exclusively
  inline void Unlock();  // Release a lock acquired via Lock()
  // Note that on systems that don't support read-write locks, these may
  // be implemented as synonyms to Lock() and Unlock().  So you can use
  // these for efficiency, but don't use them anyplace where being able
  // to do shared reads is necessary to avoid deadlock.
  inline void ReaderLock();   // Block until free or shared then acquire a share
  inline void ReaderUnlock(); // Release a read share of this Mutex
  inline void WriterLock() { Lock(); }     // Acquire an exclusive lock
  inline void WriterUnlock() { Unlock(); } // Release a lock from WriterLock()

  // Do nothing, help to express that the lock is still locked.
  inline void AssertHeld() {}

private:
  MutexType mutex_;
  // We want to make sure that the compiler sets is_safe_ to true only
  // when we tell it to, and never makes assumptions is_safe_ is
  // always true.  volatile is the most reliable way to do that.
  volatile bool is_safe_;

  inline void SetIsSafe() { is_safe_ = true; }

  // Catch the error of writing Mutex when intending MutexLock.
  Mutex(Mutex* /*ignored*/) {}
  // Disallow "evil" constructors
  Mutex(const Mutex&);
  void operator=(const Mutex&);
};

#define SAFE_PTHREAD(fncall)  do {   /* run fncall if is_safe_ is true */  \
  if (is_safe_ && fncall(&mutex_) != 0) abort();                           \
} while (0)

Mutex::Mutex() {
  SetIsSafe();
  if (is_safe_ && pthread_rwlock_init(&mutex_, NULL) != 0) abort();
}
Mutex::~Mutex()            { SAFE_PTHREAD(pthread_rwlock_destroy); }
void Mutex::Lock()         { SAFE_PTHREAD(pthread_rwlock_wrlock); }
void Mutex::Unlock()       { SAFE_PTHREAD(pthread_rwlock_unlock); }
void Mutex::ReaderLock()   { SAFE_PTHREAD(pthread_rwlock_rdlock); }
void Mutex::ReaderUnlock() { SAFE_PTHREAD(pthread_rwlock_unlock); }
#undef SAFE_PTHREAD

// --------------------------------------------------------------------------
// Some helper classes

// MutexLock(mu) acquires mu when constructed and releases it when destroyed.
class MutexLock {
public:
  explicit MutexLock(Mutex *mu) : mu_(mu) { mu_->Lock(); }
  ~MutexLock() { mu_->Unlock(); }

private:
  Mutex * const mu_;
  // Disallow "evil" constructors
  MutexLock(const MutexLock&);
  void operator=(const MutexLock&);
};

// ReaderMutexLock and WriterMutexLock do the same, for rwlocks
class ReaderMutexLock {
public:
  explicit ReaderMutexLock(Mutex *mu) : mu_(mu) { mu_->ReaderLock(); }
  ~ReaderMutexLock() { mu_->ReaderUnlock(); }

private:
  Mutex * const mu_;
  // Disallow "evil" constructors
  ReaderMutexLock(const ReaderMutexLock&);
  void operator=(const ReaderMutexLock&);
};

class WriterMutexLock {
public:
  explicit WriterMutexLock(Mutex *mu) : mu_(mu) { mu_->WriterLock(); }
  ~WriterMutexLock() { mu_->WriterUnlock(); }

private:
  Mutex * const mu_;
  // Disallow "evil" constructors
  WriterMutexLock(const WriterMutexLock&);
  void operator=(const WriterMutexLock&);
};

#ifndef COMPILE_ASSERT
template <bool>
struct CompileAssert {};
#define COMPILE_ASSERT(expr, msg) \
  typedef CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]
#endif

// Catch bug where variable name is omitted, e.g. MutexLock (&mu);
#define MutexLock(x) COMPILE_ASSERT(0, mutex_lock_decl_missing_var_name)
#define ReaderMutexLock(x) COMPILE_ASSERT(0, rmutex_lock_decl_missing_var_name)
#define WriterMutexLock(x) COMPILE_ASSERT(0, wmutex_lock_decl_missing_var_name)

}; // namespace MUTEX_NAMESPACE

using namespace MUTEX_NAMESPACE;

#endif /* MUTEX_H_ */
