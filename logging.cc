/*
 * logging.cc
 *
 *  Created on: Jul 4, 2012
 *      Author: changqwa
 */

#include "logging.h"
#include "utilities.h"
#include <sstream>

DEFINE_bool(logtostderr, false,
            "log messages go to stderr instead of logfiles");

DEFINE_bool(alsologtostderr, false,
            "log messages go to stderr in addition to logfiles");

DEFINE_bool(log_prefix, true,
            "Prepend the log prefix to the start of each log line");

DEFINE_int32(stderrthreshold,
             GOOGLE_NAMESPACE::GLOG_ERROR,
             "log messages at or above this level are copied to stderr in "
             "addition to logfiles.  This flag obsoletes --alsologtostderr.");

DEFINE_int32(minloglevel, 0, "Messages logged at a lower level than this don't "
             "actually get logged anywhere");

DEFINE_string(log_dir, "/localdisk/changqwa/log",
              "If specified, log files are written into this directory "
              "instead of the default logging directory.");

DEFINE_string(log_backtrace_at, "",
              "Emit a backtrace when logging at file:linenum.");

DEFINE_int32(logbuflevel, 0,
             "Buffer log messages logged at this level or lower"
             " (-1 means don't buffer; 0 means buffer INFO only;"
             " ...)");

DEFINE_string(alsologtoemail, "",
              "log messages go to these email addresses "
              "in addition to logfiles");

DEFINE_int32(logemaillevel, 999,
             "Email log messages logged at this level or higher"
             " (0 means email all; 3 means email FATAL only;"
             " ...)");

DEFINE_string(logmailer, "/bin/mail",
              "Mailer used to send logging email");

////Has the user called SetExitOnDFatal(true)?
//static bool exit_on_dfatal = true;

_START_GOOGLE_NAMESPACE_

const char *const LogSeverityNames[NUM_SEVERITIES] = {
  "INFO", "WARNING", "ERROR", "FATAL"};

void InitGoogleLogging(const char *argv0) {
  glog_internal_namespace_::InitGoogleLoggingUtilities(argv0);
}

#define DEFINE_CHECK_STROP_IMPL(name, func, expected)                   \
  CheckOpString Check##func##expected##Impl(const char* s1, const char* s2,   \
                                            const char* names) {        \
    bool equal = s1 == s2 || (s1 && s2 && !func(s1, s2));               \
    if (equal == expected) return CheckOpString();                                 \
    else {                                                              \
      std::ostringstream os;                                                     \
      if (!s1) s1 = "";                                                 \
      if (!s2) s2 = "";                                                 \
      os << #name " failed: " << names << " (" << s1 << " vs. " << s2 << ")"; \
      return os.str();                                              \
    }                                                                   \
  }
DEFINE_CHECK_STROP_IMPL(CHECK_STREQ, strcmp, true)
DEFINE_CHECK_STROP_IMPL(CHECK_STRNE, strcmp, false)
#undef DEFINE_CHECK_STROP_IMPL

_END_GOOGLE_NAMESPACE_
