/*
 * utilities.cc
 *
 *  Created on: Jul 17, 2012
 *      Author: changqwa
 */
#include "utilities.h"
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>  // For uname.
#include <time.h>
#include "logging.h"

using std::string;

// The following APIs are all internal.
#ifdef HAVE_STACKTRACE

#include "stacktrace.h"
#include "symbolize.h"

using namespace GOOGLE_NAMESPACE;

DEFINE_bool(symbolize_stacktrace, true,
            "Symbolize the stack trace in the tombstone");

_START_GOOGLE_NAMESPACE_

static const char *g_program_invocation_short_name = NULL;
static pthread_t g_main_thread_id;

typedef void DebugWriter(const char*, void*);

// The %p field width for printf() functions is two characters per byte.
// For some environments, add two extra bytes for the leading "0x".
static const int kPrintfPointerFieldWidth = 2 + 2 * sizeof(void*);

static void DebugWriteToStderr(const char* data, void *) {
  // This one is signal-safe.
  if (write(STDERR_FILENO, data, strlen(data)) < 0) {
    // Ignore errors.
  }
}

static void DebugWriteToString(const char* data, void *arg) {
  reinterpret_cast<string*>(arg)->append(data);
}

#ifdef HAVE_SYMBOLIZE
// Print a program counter and its symbol name.
static void DumpPCAndSymbol(DebugWriter *writerfn, void *arg, void *pc,
                            const char * const prefix) {
  char tmp[1024];
  const char *symbol = "(unknown)";
  // Symbolizes the previous address of pc because pc may be in the
  // next function.  The overrun happens when the function ends with
  // a call to a function annotated noreturn (e.g. CHECK).
  if (Symbolize(reinterpret_cast<char *>(pc) - 1, tmp, sizeof(tmp))) {
      symbol = tmp;
  }
  char buf[1024];
  snprintf(buf, sizeof(buf), "%s@ %*p  %s\n",  // fetch the buf
           prefix, kPrintfPointerFieldWidth, pc, symbol);
  writerfn(buf, arg);
}
#endif

static void DumpPC(DebugWriter *writerfn, void *arg, void *pc,
                   const char * const prefix) {
  char buf[100];
  snprintf(buf, sizeof(buf), "%s@ %*p\n",
           prefix, kPrintfPointerFieldWidth, pc);
  writerfn(buf, arg);
}

// Dump current stack trace as directed by writerfn
static void DumpStackTrace(int skip_count, DebugWriter *writerfn, void *arg) { //changqwa
  // Print stack trace
  void* stack[32];
  int depth = GetStackTrace(stack, ARRAYSIZE(stack), skip_count+1);
  for (int i = 0; i < depth; i++) {
#if defined(HAVE_SYMBOLIZE)
    if (FLAGS_symbolize_stacktrace) {
      DumpPCAndSymbol(writerfn, arg, stack[i], "    ");
    } else {
      DumpPC(writerfn, arg, stack[i], "    ");
    }
#else
    DumpPC(writerfn, arg, stack[i], "    ");
#endif
  }
}

static void DumpStackTraceAndExit() {
  const char* message = "*** Check failure stack trace: ***\n";
  if (write(STDERR_FILENO, message, strlen(message)) < 0) {
    // Ignore errors.
  }

  DumpStackTrace(1, DebugWriteToStderr, NULL);

  // Set the default signal handler for SIGABRT, to avoid invoking our
  // own signal handler installed by InstallFailedSignalHandler().
  struct sigaction sig_action;
  memset(&sig_action, 0, sizeof(sig_action));
  sigemptyset(&sig_action.sa_mask);
  sig_action.sa_handler = SIG_DFL;
  sigaction(SIGABRT, &sig_action, NULL);

  abort();
}

_END_GOOGLE_NAMESPACE_
#endif  // HAVE_STACKTRACE

_START_GOOGLE_NAMESPACE_

namespace glog_internal_namespace_ {

#ifdef HAVE_STACKTRACE
void DumpStackTraceToString(string* stacktrace) {
  DumpStackTrace(1, DebugWriteToString, stacktrace);
}
#endif

// We use an atomic operation to prevent problems with calling CrashReason
// from inside the Mutex implementation (potentially through RAW_CHECK).
static const CrashReason* g_reason = 0;

void SetCrashReason(const CrashReason* r) {
  CompareAndSwap(&g_reason,
                 reinterpret_cast<const CrashReason*>(0),
                 r);
}

const char *ProgramInvocationShortName() {
  return g_program_invocation_short_name;
}

bool IsGoogleLoggingInitialized() {
  return NULL != g_program_invocation_short_name;
}

// Default logging fatal function.
static void logging_fail() {
  abort();
}

typedef void (*logging_fail_func_t)();

logging_fail_func_t g_logging_fail_func = &logging_fail;

void InstallFailureFunction(void (*fail_func)()) {
  g_logging_fail_func = (logging_fail_func_t)fail_func;
}

// This fake logging fatal function is used for unit test
static void fake_logging_fail() {
  // do nothing.
}

// Not to suicide when we use unit tests.
void InstallUnitTestFailureFunction() {
  InstallFailureFunction(fake_logging_fail);
}

// Dump trace and suicide in actual product.
void InstallProductFailureFunction() {
  InstallFailureFunction(DumpStackTraceAndExit);
}

void Crash() {
  g_logging_fail_func();
}

void InitGoogleLoggingUtilities(const char *argv0) {
//TODO(changqwa): Finish the check macro.
//  CHECK(!IsAsbLoggingInitialized())
//      << "You called InitAsbLoggingUtilities() twice!";
  const char *slash = strrchr(argv0, '/');
  //use to initial the execution
  g_program_invocation_short_name = slash ? slash + 1 :argv0;
  g_main_thread_id = pthread_self();

#ifdef HAVE_STACKTRACE
  InstallFailureFunction(&DumpStackTraceAndExit);
#endif
}

std::string GetHostName() {
  struct utsname buf;
  if (0 != uname(&buf)) {
    // ensure null termination on failure
    *buf.nodename = '\0';
  }
  return buf.nodename;
}

static std::string g_my_user_name;
const std::string& MyUserName() {
  return g_my_user_name;
}

static void MyUserNameInitializer() {
  const char* user = getenv("USER");
  if (user != NULL) {
    g_my_user_name = user;
  } else {
    g_my_user_name = "invalid-user";
  }
}

class ModuleInitializer {
 public:
  typedef void (*void_function)(void);
  ModuleInitializer(void_function f) {
    f();
  }
};

namespace {
 ModuleInitializer log(MyUserNameInitializer);
}

int64 CycleClock_Now() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return static_cast<int64>(tv.tv_sec)*1000000 + tv.tv_usec;
}

WallTime WallTime_Now() {
  return CycleClock_Now() * 0.000001;
}

const char *const_basename(const char *filepath) {
  const char *base = strrchr(filepath, '/');
  return base ? (base + 1) : filepath;
}

pid_t GetTID() {
  return getpid();
}

static int32 g_main_thread_pid = getpid();
int32 GetMainThreadPid() {
  return g_main_thread_pid;
}

bool PidHasChanged() {
  int32 pid = getpid();
  if (g_main_thread_pid == pid) {
    return false;
  }
  g_main_thread_pid = pid;
  return true;
}

void WriteToStderr(const char* message, const size_t len) {
  // Avoid using cerr from this module since we may get called during
  // exit code, and cerr may be partially or fully destroyed by then.
  fwrite(message, len, 1, stderr);
}
} //end namespace glog_internal_namespace_

_END_GOOGLE_NAMESPACE_

// Make an implementation of stacktrace compiled.
#ifdef STACKTRACE_H
# include STACKTRACE_H
#endif
