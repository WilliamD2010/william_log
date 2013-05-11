/*
 * utilities.h
 *
 *  Created on: Jul 17, 2012
 *      Author: changqwa
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#ifdef _LP64
#define __PRIS_PREFIX "z"
#else
#define __PRIS_PREFIX
#endif
// Use these macros after a % in a printf format string
// to get correct 32/64 bit behavior, like this:
// size_t size = records.size();
// printf("%"PRIuS"\n", size);

#define PRIdS __PRIS_PREFIX "d"
#define PRIxS __PRIS_PREFIX "x"
#define PRIuS __PRIS_PREFIX "u"
#define PRIXS __PRIS_PREFIX "X"
#define PRIoS __PRIS_PREFIX "o"

#include <unistd.h>
#include <string>
#include "config.h"
#include "logging.h"

#define STACKTRACE_H "stacktrace_x86-inl.h"

#ifndef ARRAYSIZE
// There is a better way, but this is good enough for our purpose.
# define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

#ifdef HAVE___ATTRIBUTE__
# define ATTRIBUTE_NOINLINE __attribute__ ((noinline))
# define HAVE_ATTRIBUTE_NOINLINE
#else
# define ATTRIBUTE_NOINLINE
#endif

_START_GOOGLE_NAMESPACE_

namespace glog_internal_namespace_ {

const char* ProgramInvocationShortName();
bool IsGoogleLoggingInitialized();
void Crash();
void InstallUnitTestFailureFunction(); // just for unit test
void InstallProductFailureFunction(); // just for product test
void InitGoogleLoggingUtilities(const char *argv0);
std::string GetHostName();
const std::string& MyUserName();
int32 GetMainThreadPid();
bool PidHasChanged();
int64 CycleClock_Now();
typedef double WallTime;
WallTime WallTime_Now();

template <typename T>
inline T CompareAndSwap(T *ptr, T old_val, T new_val) {
  T ret = *ptr;
  if (*ptr == old_val) {
    *ptr = new_val;
  }
  return ret;
}

void DumpStackTraceToString(std::string* stacktrace);

struct CrashReason {
  CrashReason() : filename(0), line_number(0), message(0), depth(0) {}

  const char* filename;
  int line_number;
  const char* message;

  // We'll also store a bit of stack trace context at the time of crash as
  // it may not be available later on.
  void* stack[32];
  int depth;
};
void SetCrashReason(const CrashReason* r);

// Get the part of filepath after the last path separator.
const char *const_basename(const char *filepath);

pid_t GetTID();
void WriteToStderr(const char* message, const size_t len);

} // end namespace glog_internal_namespace_
} // end namespace asb

using namespace GOOGLE_NAMESPACE::glog_internal_namespace_;

#endif /* UTILITIES_H_ */
