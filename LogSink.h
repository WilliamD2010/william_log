/*
 * LogSink.h
 *
 *  Created on: Aug 15, 2012
 *      Author: changqwa
 */

#ifndef LOGSINK_H_
#define LOGSINK_H_

#include <time.h>
#include <string>
#include "config.h"
#include "log_severity.h"

_START_GOOGLE_NAMESPACE_
//
// Used to send logs to some other kind of destination
// Users should subclass LogSink and override send to do whatever they want.
// Implementations must be thread-safe because a shared instance will
// be called from whichever thread ran the LOG(XXX) line.
class LogSink {
public:
  virtual ~LogSink() { }

  // Sink's logging logic (message_len is such as to exclude '\n' at the end).
  // This method can't use LOG() or CHECK() as logging system mutex(s) are held
  // during this call.
  virtual void send(LogSeverity severity, const char* full_filename,
                    const char* base_filename, int line,
                    const struct ::tm* tm_time,
                    const char* message, size_t message_len) = 0;

  // Redefine this to implement waiting for
  // the sink's logging logic to complete.
  // It will be called after each send() returns,
  // but before that LogMessage exits or crashes.
  // By default this function does nothing.
  // Using this function one can implement complex logic for send()
  // that itself involves logging; and do all this w/o causing deadlocks and
  // inconsistent rearrangement of log messages.
  // E.g. if a LogSink has thread-specific actions, the send() method
  // can simply add the message to a queue and wake up another thread that
  // handles real logging while itself making some LOG() calls;
  // WaitTillSent() can be implemented to wait for that logic to complete.
  // See our unittest for an example.
  virtual void WaitTillSent() { /* noop default */ }

  // Returns the normal text output of the log message.
  // Can be useful to implement send().
  static std::string ToString(LogSeverity severity, const char* file, int line,
                              const struct ::tm* tm_time,
                              const char* message, size_t message_len);
};

_END_GOOGLE_NAMESPACE_

#endif /* LOGSINK_H_ */
