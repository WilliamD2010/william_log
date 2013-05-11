/*
 * logging.h
 *
 *  Created on: Jul 2, 2012
 *      Author: changqwa
 */

#ifndef LOGGING_H_
#define LOGGING_H_

#include <string>   //for string type
#include <strstream> // ostrstream
#include "config.h"
#include "basic_type.h"
#include "vlog_is_on.h"

_START_GOOGLE_NAMESPACE_
#include "log_severity.h"
_END_GOOGLE_NAMESPACE_

#define DECLARE_VARIABLE(type, name, type_name)                                    \
  namespace FLAG_namespace_do_not_use_directly_use_DECLARE_##type_name##_instead { \
  extern type FLAGS_##name;                                                        \
  }                                                                                \
  using FLAG_namespace_do_not_use_directly_use_DECLARE_##type_name##_instead::FLAGS_##name

// bool specialization
#define DECLARE_bool(name) DECLARE_VARIABLE (bool, name, bool)

// int32 specialization
#define DECLARE_int32(name) DECLARE_VARIABLE (GOOGLE_NAMESPACE::int32, name, int32)

// Special case for string, because we have to specify the namespace
// std::string, which doesn't play nicely with our FLAG__namespace hackery.
#define DECLARE_string(name) DECLARE_VARIABLE (std::string, name, string)

// Set whether log messages go to stderr instead of logfiles
DECLARE_bool(logtostderr);

// Set whether log messages go to stderr in addition to logfiles.
DECLARE_bool(alsologtostderr);

// Set whether the log prefix should be prepended to each line of output.
DECLARE_bool(log_prefix);

// Log messages at a level >= this flag are automatically sent to
// stderr in addition to log files.
DECLARE_int32(stderrthreshold);

// Log suppression level: messages logged at a lower level than this
// are suppressed.
DECLARE_int32(minloglevel);

// If specified, logfiles are written into this directory instead of the
// default logging directory.
DECLARE_string(log_dir);

// Show all VLOG(m) messages for m less or equal the value of this flag.
// Overridable by --vmodule.
DECLARE_int32(v);  // in vlog_is_on.cc

// per-module verbose level.
// Argument is a comma-separated list of <module name>=<log level>.
// <module name> is a glob pattern, matched against the filename base
// (that is, name ignoring .cc/.h./-inl.h).
// <log level> overrides any value given by --v.
DECLARE_string(vmodule); //in vlog_is_on.cc

// If specified, Emit a backtrace when logging at file:linenum.
// Default null
DECLARE_string(log_backtrace_at);

// Buffer log messages logged at this level or lower
// -1 means don't buffer; 0 means buffer INFO only
// Default 0
DECLARE_int32(logbuflevel);

// log messages go to these email addresses in addition to logfiles
// Default null
DECLARE_string(alsologtoemail);

// Email log messages logged at this level or higher
// 0 means email all; 3 means email FATAL only
// Default 999
DECLARE_int32(logemaillevel);

// Mailer used to send logging email
// Default /bin/mail
DECLARE_string(logmailer);

#define DEFINE_VARIABLE(type, name, value, meaning, type_name) \
  namespace FLAG_namespace_do_not_use_directly_use_DECLARE_##type_name##_instead {  \
  type FLAGS_##name(value);                                                         \
  }                                                                                 \
  using FLAG_namespace_do_not_use_directly_use_DECLARE_##type_name##_instead::FLAGS_##name

#define DEFINE_bool(name, value, meaning) \
  DEFINE_VARIABLE(bool, name, value, meaning, bool)

#define DEFINE_int32(name, value, meaning) \
  DEFINE_VARIABLE(GOOGLE_NAMESPACE::int32, name, value, meaning, int32)

#define DEFINE_string(name, value, meaning) \
  DEFINE_VARIABLE(std::string, name, value, meaning, string)

#define COMPACT_GOOGLE_LOG_INFO GOOGLE_NAMESPACE::LogMessage(__FILE__, __LINE__)
#define COMPACT_GOOGLE_LOG_WARNING    \
  GOOGLE_NAMESPACE::LogMessage(__FILE__, __LINE__, GOOGLE_NAMESPACE::GLOG_WARNING)
#define COMPACT_GOOGLE_LOG_ERROR      \
  GOOGLE_NAMESPACE::LogMessage(__FILE__, __LINE__, GOOGLE_NAMESPACE::GLOG_ERROR)
#define COMPACT_GOOGLE_LOG_FATAL      \
  GOOGLE_NAMESPACE::LogMessage(__FILE__, __LINE__, GOOGLE_NAMESPACE::GLOG_FATAL)

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token GOOGLE_LOG_INFO.  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define LOG(severity) COMPACT_GOOGLE_LOG_ ## severity.stream()

_START_GOOGLE_NAMESPACE_

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify {
public:
  LogMessageVoidify() {}
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator &(std::ostream &) {}
};

#define LOG_IF(severity, condition) \
  !(condition) ? (void) 0 : GOOGLE_NAMESPACE::LogMessageVoidify() & LOG(severity)
#define LOG_ASSERT(condition) \
  LOG_IF(FATAL, !(condition)) << "Assert failed: " #condition

// Use macro expansion to create, for each use of LOG_EVERY_N(), static
// variables with the __LINE__ expansion as part of the variable name.
#define LOG_EVERY_N_VARNAME_CONCAT(base, line) base ## line
#define LOG_EVERY_N_VARNAME(base, line) LOG_EVERY_N_VARNAME_CONCAT(base, line)

#define LOG_OCCURRENCES LOG_EVERY_N_VARNAME(occurrences_, __LINE__)
#define LOG_OCCURRENCES_MOD_N LOG_EVERY_N_VARNAME(occurrences_mod_n_, __LINE__)

#define SOME_KIND_OF_LOG_FIRST_N(severity, n, what_to_do)   \
  static int LOG_OCCURRENCES = 0; \
  if (LOG_OCCURRENCES < n) \
    ++LOG_OCCURRENCES, google::LogMessage(__FILE__, __LINE__, \
        google::GLOG_##severity, LOG_OCCURRENCES, &what_to_do).stream()

// n must be a integral number,
// if is float, compile error will occur
// if n > 0, n is a valid number, dump trace as we expect,
// if n <= 0, n is not illegal number, do nothing.
#define SOME_KIND_OF_LOG_EVERY_N(severity, n, what_to_do)    \
  static int LOG_OCCURRENCES = 0, LOG_OCCURRENCES_MOD_N = 0; \
    ++LOG_OCCURRENCES; \
    if ((n) > 0 &&   \
        (LOG_OCCURRENCES_MOD_N = (LOG_OCCURRENCES_MOD_N + 1)%(n)) == (1%(n))) \
        google::LogMessage(__FILE__, __LINE__, \
            google::GLOG_##severity, LOG_OCCURRENCES, &what_to_do).stream()

#define SOME_KIND_OF_LOG_IF_EVERY_N(severity, condition, n, what_to_do) \
  static int LOG_OCCURRENCES = 0, LOG_OCCURRENCES_MOD_N = 0; \
  ++LOG_OCCURRENCES; \
  if ((n) > 0 && condition &&   \
      (LOG_OCCURRENCES_MOD_N = (LOG_OCCURRENCES_MOD_N + 1)%(n)) == (1%(n))) \
      google::LogMessage(__FILE__, __LINE__, \
          google::GLOG_##severity, LOG_OCCURRENCES, &what_to_do).stream()

#define LOG_FIRST_N(severity, n) \
  SOME_KIND_OF_LOG_FIRST_N(severity, (n), google::LogMessage::SendToLog)

#define LOG_EVERY_N(severity, n) \
  SOME_KIND_OF_LOG_EVERY_N(severity, (n), google::LogMessage::SendToLog)

#define LOG_IF_EVERY_N(severity, condition, n) \
  SOME_KIND_OF_LOG_IF_EVERY_N(severity, condition, (n), \
                              google::LogMessage::SendToLog)


// Log only in verbose mode.
#define VLOG(verboselevel) LOG_IF(INFO, VLOG_IS_ON(verboselevel))

#define VLOG_IF(verboselevel, condition) \
  LOG_IF(INFO, (condition) && VLOG_IS_ON(verboselevel))

#define VLOG_EVERY_N(verboselevel, n) \
  LOG_IF_EVERY_N(INFO, VLOG_IS_ON(verboselevel), n)

#define VLOG_IF_EVERY_N(verboselevel, condition, n) \
  LOG_IF_EVERY_N(INFO, (condition) && VLOG_IS_ON(verboselevel), n)

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by NDEBUG, so the check will be executed regardless of
// compilation mode.  Therefore, it is safe to do things like:
//    CHECK(fp->Write(x) == 4)
#define CHECK(condition) \
  LOG_IF(FATAL, !(condition)) << "Check failed: " #condition " "

// This is a dummy class to define the following operator.
struct DummyClassToBeDefineOperator {};
_END_GOOGLE_NAMESPACE_

// Define global operator<< to declare using ::operator<<.
// This declaration will allow us to use CHECK macros for user
// defined classes which have operator<< (e.g., stl_logging.h).
//
// The function doesn't have function body, if it is used,
// compile error will occurs.
inline std::ostream & operator << (
    std::ostream &os, const google::DummyClassToBeDefineOperator&);

_START_GOOGLE_NAMESPACE_

// Build the error message string.
// We cannot use stl_logging if compiler doesn't
// support using expression for operator <<.
template<class t1, class t2>
std::string MakeCheckOpString(const t1& v1, const t2& v2, const char* names) {
  using ::operator<<;
  std::strstream ss;
  ss << names << " (" << v1 << " vs. " << v2 << ")";
  std::string compare_result = std::string(ss.str(), ss.pcount());
  delete []ss.str();
  return compare_result;
}

// A container for a string which can be evaluated to a bool -
class CheckOpString {
public:
  CheckOpString() : result(false) { check_op_str = ""; }
  CheckOpString(std::string check_str) : result(true) {
    check_op_str = "Check failed: ";
    check_op_str += check_str;
    check_op_str += " ";
  }
  operator bool() const { return result; }
  std::string str() { return check_op_str; }

private:
  const bool result;
  std::string check_op_str;
};

// Helper functions for CHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler
// will not instantiate the template version of the function on values of
// unnamed enum type - see comment below. //changqwa
#define DEFINE_CHECK_OP_IMPL(name, op) \
  template <class t1, class t2> \
  inline CheckOpString Check##name##Impl(const t1& v1, const t2& v2, \
                                       const char* names) { \
    if (v1 op v2) return CheckOpString(); \
    else return MakeCheckOpString(v1, v2, names); \
  } \
  inline CheckOpString Check##name##Impl(int v1, int v2, const char* names) { \
    return Check##name##Impl<int, int>(v1, v2, names); \
  }

// Use _EQ, _NE, _LE, etc. in case the file including base/logging.h
// provides its own #defines for the simpler names EQ, NE, LE, etc.
// This happens if, for example, those are used as token names in a
// yacc grammar.
DEFINE_CHECK_OP_IMPL(_EQ, ==)
DEFINE_CHECK_OP_IMPL(_NE, !=)
DEFINE_CHECK_OP_IMPL(_LE, <=)
DEFINE_CHECK_OP_IMPL(_LT, < )
DEFINE_CHECK_OP_IMPL(_GE, >=)
DEFINE_CHECK_OP_IMPL(_GT, > )
#undef DEFINE_CHECK_OP_IMPL

// Function is overloaded for integral types to allow static const
// integrals declared in classes and not defined to be used as arguments to
// CHECK* macros. It's not encouraged though.
//
// Still not sustain static const enumerate declared in classes and not
// defined in to be used as arguments to CHECK* macros directly. if we still
// want to check these values, explicit cast them to interval values.
template <typename T>
inline const T &GetReferenceableValue(const T &val) { return val; }
inline char           GetReferenceableValue(char           val) { return val; }
inline unsigned char  GetReferenceableValue(unsigned char  val) { return val; }
inline signed char    GetReferenceableValue(signed char    val) { return val; }
inline short          GetReferenceableValue(short          val) { return val; }
inline unsigned short GetReferenceableValue(unsigned short val) { return val; }
inline int            GetReferenceableValue(int            val) { return val; }
inline unsigned int   GetReferenceableValue(unsigned int   val) { return val; }
inline long           GetReferenceableValue(long           val) { return val; }
inline unsigned long  GetReferenceableValue(unsigned long  val) { return val; }
inline long long      GetReferenceableValue(long long      val) { return val; }
inline unsigned long long GetReferenceableValue(unsigned long long val) {
  return val;
}

#define CHECK_OP_LOG(name, op, val1, val2)              \
  if (google::CheckOpString _result =                   \
      google::Check##name##Impl(                        \
      google::GetReferenceableValue(val1),              \
      google::GetReferenceableValue(val2),              \
      #val1 " " #op " " #val2))                         \
      LOG(FATAL) << _result.str()

// Equality/Inequality checks - compare two values, and log a FATAL message
// including the two values when the result is not as expected.  The values
// must have operator<<(ostream, ...) defined.
//
// You may append to the error message like so:
//   CHECK_NE(1, 2) << ": The world must be ending!";
//
// We are very careful to ensure that each argument is evaluated exactly
// once, and that anything which is legal to pass as a function argument is
// legal here.  In particular, the arguments may be temporary expressions
// which will end up being destroyed at the end of the apparent statement,
// for example:
//   CHECK_EQ(string("abc")[1], 'b');
//
// WARNING: These don't compile correctly if one of the arguments is a pointer
// and the other is NULL. To work around this, simply static_cast NULL to the
// type of the desired pointer.

#define CHECK_EQ(val1, val2) CHECK_OP_LOG(_EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) CHECK_OP_LOG(_NE, !=, val1, val2)
#define CHECK_LE(val1, val2) CHECK_OP_LOG(_LE, <=, val1, val2)
#define CHECK_LT(val1, val2) CHECK_OP_LOG(_LT, < , val1, val2)
#define CHECK_GE(val1, val2) CHECK_OP_LOG(_GE, >=, val1, val2)
#define CHECK_GT(val1, val2) CHECK_OP_LOG(_GT, > , val1, val2)

// Helper functions for string comparisons.
// To avoid bloat, the definitions are in logging.cc.
#define DECLARE_CHECK_STROP_IMPL(func, expected) \
    CheckOpString Check##func##expected##Impl( \
        const char* s1, const char* s2, const char* names)
DECLARE_CHECK_STROP_IMPL(strcmp, true);
DECLARE_CHECK_STROP_IMPL(strcmp, false);
#undef DECLARE_CHECK_STROP_IMPL

// Helper macro for string comparisons.
// Don't use this macro directly in your code, use CHECK_STREQ et al below.
#define CHECK_STROP(func, op, expected, s1, s2) \
  if (google::CheckOpString _result = \
         google::Check##func##expected##Impl((s1), (s2), \
                                     #s1 " " #op " " #s2)) \
    LOG(FATAL) << _result.str()

// String (char*) equality/inequality checks.
// CASE versions are case-insensitive.
//
// Note that "s1" and "s2" may be temporary strings which are destroyed
// by the compiler at the end of the current "full expression"
// (e.g. CHECK_STREQ(Foo().c_str(), Bar().c_str())).

#define CHECK_STREQ(s1, s2) CHECK_STROP(strcmp, ==, true, s1, s2)
#define CHECK_STRNE(s1, s2) CHECK_STROP(strcmp, !=, false, s1, s2)

#define CHECK_NEAR(v1, v2, margin) \
  do {                             \
    CHECK_GE(v1, v2 - margin);     \
    CHECK_LE(v1, v2 + margin);     \
  } while(0)

// If a non-NULL string pointer is given, we write this message to that string.
// We then do normal LOG(severity) logging as well.
// This is useful for capturing messages and storing them somewhere more
// specific than the global log of the process.
// Argument types:
//   LogSeverity severity;
//   string* message;
// The cast is to disambiguate NULL arguments.
// NOTE: LOG(severity) expands to LogMessage().stream() for the specified
// severity.
#define LOG_TO_STRING(severity, message) \
  LOG_TO_STRING_##severity(static_cast<string *>(message)).stream()

// If a non-NULL pointer is given, we push the message onto the end
// of a vector of strings; otherwise, we report it with LOG(severity).
// This is handy for capturing messages and perhaps passing them back
// to the caller, rather than reporting them immediately.
// Argument types:
//   LogSeverity severity;
//   vector<string> *outvec;
// The cast is to disambiguate NULL arguments.
#define LOG_STRING(severity, outvec) \
  LOG_TO_STRING_##severity(static_cast<vector<string>*>(outvec)).stream()

#define LOG_TO_STRING_INFO(message) GOOGLE_NAMESPACE::LogMessage( \
  __FILE__, __LINE__, GOOGLE_NAMESPACE::GLOG_INFO, message)

#define LOG_TO_STRING_WARNING(message) GOOGLE_NAMESPACE::LogMessage( \
  __FILE__, __LINE__, GOOGLE_NAMESPACE::GLOG_WARNING, message)

#define LOG_TO_STRING_ERROR(message) GOOGLE_NAMESPACE::LogMessage( \
      __FILE__, __LINE__, GOOGLE_NAMESPACE::GLOG_ERROR, message)

#define LOG_TO_STRING_FATAL(message) GOOGLE_NAMESPACE::LogMessage( \
      __FILE__, __LINE__, GOOGLE_NAMESPACE::GLOG_FATAL, message)

class LogSink;  // defined below

// If a non-NULL sink pointer is given, we push this message to that sink.
// For LOG_TO_SINK we then do normal LOG(severity) logging as well.
// This is useful for capturing messages and passing/storing them
// somewhere more specific than the global log of the process.
// Argument types:
//   LogSink* sink;
//   LogSeverity severity;
// The cast is to disambiguate NULL arguments.
#define LOG_TO_SINK(sink, severity) \
  google::LogMessage(                                    \
      __FILE__, __LINE__,                                \
      google::GLOG_ ## severity,                         \
      static_cast<google::LogSink*>(sink), true).stream()

#define LOG_TO_SINK_BUT_NOT_TO_LOGFILE(sink, severity)   \
  google::LogMessage(                                    \
      __FILE__, __LINE__,                                \
      google::GLOG_ ## severity,                         \
      static_cast<google::LogSink*>(sink), false).stream()

extern const char *const LogSeverityNames[NUM_SEVERITIES];
void InitGoogleLogging(const char *argv0);

_END_GOOGLE_NAMESPACE_

#define WILLIAM_TRACE(x) std::cout << "Wang Changqian Trace: "<< #x << " = " << x << std::endl

#endif /* LOGGING_H_ */
