/*
 * LogDestination.cc
 *
 *  Created on: Aug 15, 2012
 *      Author: changqwa
 */

#include <assert.h>
#include "LogDestination.h"
#include "LogSink.h"

_START_GOOGLE_NAMESPACE_

// Errors do not get logged to email by default.
LogSeverity LogDestination::email_logging_severity_ = 99999;
string LogDestination::addresses_;
string LogDestination::hostname_;
LogDestination* LogDestination::log_destinations_[NUM_SEVERITIES];
vector<LogSink*>* LogDestination::sinks_ = NULL;
Mutex LogDestination::sink_mutex_;

LogDestination::LogDestination(LogSeverity severity,
                               const char* base_filename)
  : fileobject_(severity, base_filename),
    logger_(&fileobject_) {
}

/* LogDestination Static Functions */
const string& LogDestination::hostname() {
  if (hostname_.empty()) {
    hostname_ = GetHostName();
    if (hostname_.empty()) {
      hostname_ = "(unknown)";
    }
  }
  return hostname_;
}

void LogDestination::AddLogSink(LogSink *destination) {
  // Prevent any subtle race conditions by wrapping a mutex lock around
  // all this stuff.
  MutexLock l(&sink_mutex_);
  if (!sinks_)  sinks_ = new vector<LogSink*>;
  sinks_->push_back(destination);
}

void LogDestination::RemoveLogSink(LogSink *destination) {
  // Prevent any subtle race conditions by wrapping a mutex lock around
  // all this stuff.
  MutexLock l(&sink_mutex_);
  // This doesn't keep the sinks in order, but who cares?
  if (sinks_) {
    for (int i = sinks_->size() - 1; i >= 0; i--) {
      if ((*sinks_)[i] == destination) {
        (*sinks_)[i] = (*sinks_)[sinks_->size() - 1];
        sinks_->pop_back();
        break;
      }
    }
  }
}

void LogDestination::MaybeLogToStderr(const LogSeverity severity,
                                             const char* message,
                                             const size_t len) {
  if ((severity >= FLAGS_stderrthreshold) || FLAGS_alsologtostderr) {
    WriteToStderr(message, len);
  }
}

// use_logging controls whether the logging functions LOG/VLOG are used
// to log errors.  It should be set to false when the caller holds the
// log_mutex.
static bool SendEmailInternal(const char*dest, const char *subject,
                              const char*body, bool use_logging) {
  if (dest && *dest) {
    if ( use_logging ) {
//      VLOG(1) << "Trying to send TITLE:" << subject
//              << " BODY:" << body << " to " << dest;
    } else {
      fprintf(stderr, "Trying to send TITLE: %s BODY: %s to %s\n",
              subject, body, dest);
    }

    string cmd =
        FLAGS_logmailer + " -s\"" + subject + "\" " + dest;
    FILE* pipe = popen(cmd.c_str(), "w");
    if (pipe != NULL) {
      // Add the body if we have one
      if (body)
        fwrite(body, sizeof(char), strlen(body), pipe);
      bool ok = pclose(pipe) != -1;
      if ( !ok ) {
        if ( use_logging ) {
//          char buf[100];
//          posix_strerror_r(errno, buf, sizeof(buf));
//          LOG(ERROR) << "Problems sending mail to " << dest << ": " << buf;
        } else {
          char buf[100];
//          posix_strerror_r(errno, buf, sizeof(buf));
          fprintf(stderr, "Problems sending mail to %s: %s\n", dest, buf);
        }
      }
      return ok;
    } else {
      if ( use_logging ) {
//        LOG(ERROR) << "Unable to send mail to " << dest;
      } else {
        fprintf(stderr, "Unable to send mail to %s\n", dest);
      }
    }
  }
  return false;
}

bool SendEmail(const char*dest, const char *subject, const char*body){
  return SendEmailInternal(dest, subject, body, true);
}

void LogDestination::MaybeLogToEmail(const LogSeverity severity,
                                     const char* message,
                                     const size_t len) {
  if (severity >= email_logging_severity_ ||
      severity >= FLAGS_logemaillevel) {
    string to(FLAGS_alsologtoemail);
    if (!addresses_.empty()) {
      if (!to.empty()) {
        to += ",";
      }
      to += addresses_;
    }
    const string subject(string("[LOG] ") + LogSeverityNames[severity] + ": " +
                         glog_internal_namespace_::ProgramInvocationShortName());
    string body(hostname());
    body += "\n\n";
    body.append(message, len);

    // should NOT use SendEmail().  The caller of this function holds the
    // log_mutex and SendEmail() calls LOG/VLOG which will block trying to
    // acquire the log_mutex object.  Use SendEmailInternal() and set
    // use_logging to false.
    SendEmailInternal(to.c_str(), subject.c_str(), body.c_str(), false);
  }
}

LogDestination* LogDestination::log_destination(LogSeverity severity) {
  assert(severity >=0 && severity < NUM_SEVERITIES);
  if (!log_destinations_[severity]) {
    log_destinations_[severity] = new LogDestination(severity, NULL);
  }
  return log_destinations_[severity];
}

void LogDestination::MaybeLogToLogfile(LogSeverity severity,
                                       time_t timestamp,
                                       const char* message,
                                       size_t len) {
  const bool should_flush = severity > FLAGS_logbuflevel;
  LogDestination* destination = log_destination(severity);
  destination->logger_->Write(should_flush, timestamp, message, len);
}

void LogDestination::LogToAllLogfiles(LogSeverity severity,
                                      time_t timestamp,
                                      const char* message,
                                      size_t len) {

  if ( FLAGS_logtostderr )            // global flag: never log to file
    WriteToStderr(message, len);
  else
    for (int i = severity; i >= 0; --i)
      LogDestination::MaybeLogToLogfile(i, timestamp, message, len);
}

void LogDestination::LogToSinks(LogSeverity severity,
                                const char *full_filename,
                                const char *base_filename,
                                int line,
                                const struct ::tm* tm_time,
                                const char* message,
                                size_t message_len) {
  ReaderMutexLock l(&sink_mutex_);
  if (sinks_) {
    for (int i = sinks_->size() - 1; i >= 0; i--) {
      (*sinks_)[i]->send(severity, full_filename, base_filename,
                         line, tm_time, message, message_len);
    }
  }
}

void LogDestination::WaitForSinks() {
  ReaderMutexLock l(&sink_mutex_);
  if (sinks_) {
    for (int i = sinks_->size() - 1; i >= 0; i--) {
      (*sinks_)[i]->WaitTillSent();
    }
  }
}

_END_GOOGLE_NAMESPACE_

