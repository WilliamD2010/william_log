/*
 * LogMessage.cc
 *
 *  Created on: Jul 17, 2012
 *      Author: changqwa
 */
#include "LogMessage.h"
#include <stdio.h>
#include <iomanip>
#include <iostream>
#include <vector>
#include "raw_logging.h"
#include "mutex.h"
#include "LogDestination.h"
#include "LogSink.h"

#ifdef HAVE_STACKTRACE
#include "stacktrace.h"
#endif

using std::setw;
using std::string;
using std::setfill;
using std::min;

// Use below macro as a thread annotation.
#define EXCLUSIVE_LOCKS_REQUIRED(mu)

_START_GOOGLE_NAMESPACE_

// A mutex that allows only one thread to log at a time, to keep things from
// getting jumbled.  Some other very uncommon logging operations (like
// changing the destination file for log messages of a given severity) also
// lock this mutex.  Please be sure that anybody who might possibly need to
// lock it does so.
static Mutex log_mutex;

// Since multiple threads may call LOG(FATAL), and we want to preserve
// the data from the first call, we allocate two sets of space.  One
// for exclusive use by the first thread, and one for shared use by
// all other threads.
static Mutex fatal_msg_lock;
static CrashReason crash_reason;

// Has the user called SetExitOnDFatal(true)?
static bool exit_on_dfatal = true;

static bool fatal_msg_exclusive = true;
static char fatal_msg_buf_exclusive[LogMessage::kMaxLogMessageLen+1];
static char fatal_msg_buf_shared[LogMessage::kMaxLogMessageLen+1];
static LogMessage::LogStream fatal_msg_stream_exclusive(
    fatal_msg_buf_exclusive, LogMessage::kMaxLogMessageLen, 0);
static LogMessage::LogStream fatal_msg_stream_shared(
    fatal_msg_buf_shared, LogMessage::kMaxLogMessageLen, 0);
LogMessage::LogMessageData LogMessage::fatal_msg_data_exclusive_;
LogMessage::LogMessageData LogMessage::fatal_msg_data_shared_;

// Number of messages sent at each severity. Under log_mutex.
int64 LogMessage::num_messages_[NUM_SEVERITIES] = {0, 0, 0, 0};

// An arbitrary limit on the length of a single log message.
// To do this makes the streaming be more efficient.
const size_t LogMessage::kMaxLogMessageLen;

LogMessage::LogMessageData::~LogMessageData() {
  delete[] buf_;
  delete stream_allocated_;
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity, int ctr,
                       SendMethod send_method) {
  Init(file, line, severity, send_method);
  data_->stream_->set_ctr(ctr);
}

LogMessage::LogMessage(const char *file, int line) {
  Init(file, line, GLOG_INFO, &LogMessage::SendToLog);
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity) {
  Init(file, line, severity, &LogMessage::SendToLog);
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity, LogSink* sink,
                       bool also_send_to_log) {
  Init(file, line, severity, also_send_to_log ? &LogMessage::SendToSinkAndLog :
                                                &LogMessage::SendToSink);
  data_->sink_ = sink;  // override Init()'s setting to NULL
}

LogMessage::LogMessage(const char *file, int line, LogSeverity severity,
                       std::vector<std::string> *outvec) {
  Init(file, line, severity, &LogMessage::SaveOrSendToLog);
  data_->outvec_ = outvec;
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       std::string *message) {
  Init(file, line, severity, &LogMessage::WriteToStringAndLog);
  data_->message_ = message;
}

void LogMessage::Init(const char *file, int line, LogSeverity severity,
                      void (LogMessage::*send_method)()) {
  allocated_ = NULL;
  if (severity != GLOG_FATAL) {
    allocated_ = new LogMessageData();
    data_ = allocated_;
    data_->buf_ = new char[kMaxLogMessageLen+1];
    data_->message_text_ = data_->buf_;
    data_->stream_allocated_ =
        new LogStream(data_->message_text_, kMaxLogMessageLen, 0);
    data_->stream_ = data_->stream_allocated_;
    data_->first_fatal_ = false;
  } else {
    MutexLock l(&fatal_msg_lock);
    if (fatal_msg_exclusive) {
      fatal_msg_exclusive = false;
      data_ = &fatal_msg_data_exclusive_;
      data_->message_text_ = fatal_msg_buf_exclusive;
      data_->stream_ = &fatal_msg_stream_exclusive;
      data_->first_fatal_ = true;
    } else {
      data_ = &fatal_msg_data_shared_;
      data_->message_text_ = fatal_msg_buf_shared;
      data_->stream_ = &fatal_msg_stream_shared;
      data_->first_fatal_ = false;
    }
    data_->stream_allocated_ = NULL;
  }
  stream().fill('0');
  data_->severity_ = severity;
  data_->line_ = line;
  data_->send_method_ = send_method;
  WallTime now = WallTime_Now();
  data_->timestamp_ = static_cast<time_t>(now);
  localtime_r(&data_->timestamp_, &data_->tm_time_);
  int usecs = static_cast<int>((now - data_->timestamp_) * 1000000);
  data_->basename_ = const_basename(file);
  data_->fullname_ = file;
  data_->has_been_flushed_ = false;
  if (FLAGS_log_prefix) {
    stream() << LogSeverityNames[severity][0]
             << setw(2) << 1 + data_->tm_time_.tm_mon
             << setw(2) << data_->tm_time_.tm_mday
             << ' '
             << setw(2) << data_->tm_time_.tm_hour  << ':'
             << setw(2) << data_->tm_time_.tm_min   << ':'
             << setw(2) << data_->tm_time_.tm_sec   << "."
             << setw(6) << usecs
             << ' '
             << setfill(' ') << setw(5)
             << static_cast<unsigned int>(GetTID()) << setfill('0')
             << ' '
             << data_->basename_ << ':' << data_->line_ << "] ";
  }
  data_->num_prefix_chars_ = data_->stream_->pcount();
  if (!FLAGS_log_backtrace_at.empty()) {
    char fileline[128];
    snprintf(fileline, sizeof(fileline), "%s:%d", data_->basename_, line);
    if (!strcmp(FLAGS_log_backtrace_at.c_str(), fileline)) {
      string stacktrace;
      DumpStackTraceToString(&stacktrace);
      stream() << " (stacktrace:\n" << stacktrace << ") ";
    }
  }
}

LogMessage::~LogMessage() {
  Flush();
  delete allocated_;
}

void LogMessage::Flush() {
  if (data_->has_been_flushed_ || data_->severity_ < FLAGS_minloglevel) return;
  data_->num_chars_to_log_ = data_->stream_->pcount();
  // Do we need to add a \n to the end of this message?
  const bool append_newline =
      (data_->message_text_[data_->num_chars_to_log_-1] != '\n');
  if (append_newline) {
    stream() << "\n";
    data_->num_chars_to_log_ = data_->stream_->pcount();
  }
  LogTraceWithMutexLock();

  WaitForSink();

  data_->has_been_flushed_ = true;
}

// Copy of first FATAL log message so that we can print it out again
// after all the stack traces.  To preserve legacy behavior, we don't
// use fatal_msg_buf_exclusive.
static time_t fatal_time;
static char fatal_message[256];

void LogMessage::LogTraceWithMutexLock() {
  MutexLock l(&log_mutex);
  (this->*(data_->send_method_))();
  ++num_messages_[static_cast<int>(data_->severity_)];
}

// callers must hold the log_mutex
void LogMessage::SendToLog() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  static bool already_warned_before_initthread = false;

  log_mutex.AssertHeld();

  if (!already_warned_before_initthread && !IsGoogleLoggingInitialized()) {
    const char w[] = "WARNING: Logging before InitGoogleLogging() is "
                     "written to STDERR\n";
    WriteToStderr(w, strlen(w));
    already_warned_before_initthread = true;
  }
  // global flag: never log to file if set. Also,
  // don't log to a file if we haven't retrieved program name.
  if (FLAGS_logtostderr || !IsGoogleLoggingInitialized()) {
    WriteToStderr(data_->message_text_, data_->num_chars_to_log_);
    LogDestination::LogToSinks(data_->severity_, data_->fullname_,
                               data_->basename_, data_->line_,
                               &data_->tm_time_,
                               data_->message_text_
                               + data_->num_prefix_chars_,
                               data_->num_chars_to_log_
                               - data_->num_prefix_chars_ - 1);
  } else {
    // log this message to all log files of severity <= severity_
    LogDestination::LogToAllLogfiles(data_->severity_, data_->timestamp_,
                                     data_->message_text_,
                                     data_->num_chars_to_log_);

    LogDestination::MaybeLogToStderr(data_->severity_, data_->message_text_,
                                     data_->num_chars_to_log_);
    LogDestination::MaybeLogToEmail(data_->severity_, data_->message_text_,
                                    data_->num_chars_to_log_);
    LogDestination::LogToSinks(data_->severity_,
                               data_->fullname_, data_->basename_,
                               data_->line_, &data_->tm_time_,
                               data_->message_text_ + data_->num_prefix_chars_,
                               (data_->num_chars_to_log_
                                - data_->num_prefix_chars_ - 1));
  }

  // If we log a FATAL message, flush all the log destinations, then toss
  // a signal for others to catch. We leave the logs in a state that
  // someone else can use them (as long as they flush afterwards)
  if (data_->severity_ == GLOG_FATAL && exit_on_dfatal) {
    if (data_->first_fatal_) {
      // Store crash information so that it is accessible from within signal
      // handlers that may be invoked later.
      RecordCrashReason(&crash_reason);

      // Store shortened fatal message for other logs and GWQ status
      const int copy = min<int>(data_->num_chars_to_log_,
                                sizeof(fatal_message)-1);
      memcpy(fatal_message, data_->message_text_, copy);
      fatal_message[copy] = '\0';
      fatal_time = data_->timestamp_;
    }

    if (!FLAGS_logtostderr) {
      for (int i = 0; i < NUM_SEVERITIES; ++i) {
        if ( LogDestination::log_destinations_[i] )
          LogDestination::log_destinations_[i]->logger_->Write(true, 0, "", 0);
      }
    }

    // release the lock that our caller (directly or indirectly)
    // LogMessage::~LogMessage() grabbed so that signal handlers
    // can use the logging facility. Alternately, we could add
    // an entire unsafe logging interface to bypass locking
    // for signal handlers but this seems simpler.
    log_mutex.Unlock();

    WaitForSink();

    Fail();

    // below code is just for unit test
    fatal_msg_stream_shared.seekp(std::ios::beg);
    log_mutex.Lock(); // still keep the mutex lock
  }
}

std::string LogMessage::ExtractMessage() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  RAW_DCHECK(data_->num_chars_to_log_ > 0 &&
                 data_->message_text_[data_->num_chars_to_log_-1] == '\n', "");
  // Omit prefix of message and trailing newline when writing to message_.
  const char *start = data_->message_text_ + data_->num_prefix_chars_;
  int len = data_->num_chars_to_log_ - data_->num_prefix_chars_ - 1;
  return std::string(start, len);
}

void LogMessage::WriteToStringAndLog() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  if (data_->message_ != NULL) {
    data_->message_->assign(ExtractMessage());
  }
  SendToLog();
}

void LogMessage::SaveOrSendToLog() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  if (data_->outvec_ != NULL) {
    data_->outvec_->push_back(ExtractMessage());
  } else {
    SendToLog();
  }
}

void LogMessage::WaitForSink() {
  const bool send_to_sink = (data_->send_method_ == &LogMessage::SendToSink)
      || (data_->send_method_ == &LogMessage::SendToSinkAndLog);
  if (send_to_sink && data_->sink_ != NULL) {
    data_->sink_->WaitTillSent();
  }

  LogDestination::WaitForSinks();
}

void LogMessage::RecordCrashReason(
    glog_internal_namespace_::CrashReason* reason) {
  reason->filename = fatal_msg_data_exclusive_.fullname_;
  reason->line_number = fatal_msg_data_exclusive_.line_;
  reason->message = fatal_msg_buf_exclusive +
                    fatal_msg_data_exclusive_.num_prefix_chars_;
#ifdef HAVE_STACKTRACE
  // Retrieve the stack trace, omitting the logging frames that got us here.
  reason->depth = GOOGLE_NAMESPACE::GetStackTrace(reason->stack, ARRAYSIZE(reason->stack), 4);
#else
  reason->depth = 0;
#endif
}

void LogMessage::Fail() {
  Crash();
}

// L >= log_mutex (callers must hold the log_mutex).
void LogMessage::SendToSink() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  if (data_->sink_ != NULL) {
    RAW_DCHECK(data_->num_chars_to_log_ > 0 &&
               data_->message_text_[data_->num_chars_to_log_-1] == '\n', "");
    data_->sink_->send(data_->severity_, data_->fullname_, data_->basename_,
                       data_->line_, &data_->tm_time_,
                       data_->message_text_ + data_->num_prefix_chars_,
                       (data_->num_chars_to_log_ -
                        data_->num_prefix_chars_ - 1));
  }
}

// L >= log_mutex (callers must hold the log_mutex).
void LogMessage::SendToSinkAndLog() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  SendToSink();
  SendToLog();
}

// L < log_mutex.  Acquires and releases mutex_.
int64 LogMessage::num_messages(int severity) {
  MutexLock l(&log_mutex);
  return num_messages_[severity];
}

// Output the COUNTER value.
// This is only valid if ostream is a LogStream
std::ostream & operator << (std::ostream &os, PRIVATE_LogStream_Counter) {
  LogMessage::LogStream &log_os = dynamic_cast<LogMessage::LogStream&>(os);
  os << log_os.ctr();
  return os;
}

_END_GOOGLE_NAMESPACE_
