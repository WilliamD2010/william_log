/*
 * LogDestination.h
 *
 *  Created on: Aug 15, 2012
 *      Author: changqwa
 */

#ifndef LOGDESTINATION_H_
#define LOGDESTINATION_H_
#include <string>
#include <vector>
#include "logging.h"
#include "mutex.h"
#include "utilities.h"
#include "Logger.h"

using std::string;
using std::vector;

_START_GOOGLE_NAMESPACE_

class LogSink;

class LogDestination {
public:
  friend class LogMessage;
  static const string& hostname();

  static void AddLogSink(LogSink *destination);
  static void RemoveLogSink(LogSink *destination);

private:
  LogDestination(LogSeverity severity, const char* base_filename);
  ~LogDestination() { }

  // Take a log message of a particular severity and log it to stderr
  // if it's of a high enough severity to deserve it.
  static void MaybeLogToStderr(LogSeverity severity,
                               const char* message,
                               size_t len);

  // Take a log message of a particular severity and log it to email
  // if it's of a high enough severity to deserve it.
  static void MaybeLogToEmail(const LogSeverity severity,
                              const char* message,
                              const size_t len);

  // Take a log message of a particular severity and log it to a file
  // if the base filename is not "" (which means "don't log to me")
  static void MaybeLogToLogfile(LogSeverity severity,
                                time_t timestamp,
                                const char* message,
                                size_t len);

  // Take a log message of a particular severity and log it to the file
  // for that severity and also for all files with severity less than
  // this severity.
  static void LogToAllLogfiles(LogSeverity severity,
                               time_t timestamp,
                               const char* message,
                               size_t len);


  //Send logging info to all registered sinks.
  static void LogToSinks(LogSeverity severity,
                         const char *full_filename,
                         const char *base_filename,
                         int line,
                         const tm *tm_time,
                         const char *message,
                         size_t message_len);

  // Wait for all registered sinks via WaitTillSent
  static void WaitForSinks();

  static LogDestination* log_destination(LogSeverity severity);

  LogFileObject fileobject_;
  base::Logger* logger_;      // Either &fileobject_, or wrapper around it
  static LogDestination* log_destinations_[NUM_SEVERITIES];
  static LogSeverity email_logging_severity_;
  static string addresses_;
  static string hostname_;

  // arbitrary global logging destinations.
  static vector<LogSink*>* sinks_;

  // Protect the vector sinks_,
  // but not the LogSink objects its elements reference.
  static Mutex sink_mutex_;

  // Disallow
  LogDestination(const LogDestination&);
  LogDestination& operator=(const LogDestination&);
};

_END_GOOGLE_NAMESPACE_

#endif /* LOGDESTINATION_H_ */
