/*
 * LogMessage.h
 *
 *  Created on: Jul 17, 2012
 *      Author: changqwa
 */

#ifndef LOGMESSAGE_H_
#define LOGMESSAGE_H_

#include <time.h>
#include <strstream>
#include <vector>
#include "logging.h"
#include "utilities.h"

_START_GOOGLE_NAMESPACE_

class LogSink;

class LogMessage{
public:
  // LogStream inherit from non-DLL-exported class (std::ostrstream)
  class LogStream : public std::ostrstream {
  public :
    LogStream(char *buf, int len, int ctr_in) :
      std::ostrstream(buf, len), ctr_(ctr_in) {
    }

    int ctr() const {return ctr_; }
    void set_ctr(int ctr_in) { ctr_ = ctr_in; }
  private:
    int ctr_; // Counter hack
  };

  typedef void (LogMessage::*SendMethod)();

  // Used for LOG_FIRST_N LOG_EVERY_N & LOG_IF_EVERY_N
  LogMessage(const char* file, int line, LogSeverity severity, int ctr,
             SendMethod send_method);

  // Used for LOG(INFO): Implied are:
  // severity = INFO, ctr = 0, send_method = &LogMessage::SendToLog.
  LogMessage(const char *file, int line);

  // Used for LOG(severity) where severity != INFO.  Implied
  // are: ctr = 0, send_method = &LogMessage::SendToLog
  LogMessage(const char *file, int line, LogSeverity severity);

  // Constructor to log this message to a specified sink (if not NULL).
  // Implied are: ctr = 0, send_method = &LogMessage::SendToSinkAndLog if
  // also_send_to_log is true, send_method = &LogMessage::SendToSink otherwise.
  LogMessage(const char* file, int line, LogSeverity severity, LogSink* sink,
             bool also_send_to_log);

  // Constructor where we also give a vector<string> pointer
  // for storing the messages (if the pointer is not NULL).
  // Implied are: ctr = 0, send_method = &LogMessage::SaveOrSendToLog.
  LogMessage(const char *file, int line, LogSeverity severity,
             std::vector<std::string> *outvec);

  // Constructor where we also give a string pointer for storing the
  // message (if the pointer is not NULL).  Implied are: ctr = 0,
  // send_method = &LogMessage::WriteToStringAndLog.
  LogMessage(const char *file, int line, LogSeverity severity,
             std::string *message);

  ~LogMessage();

  // Flush a buffered message to the sink set in the constructor.  Always
  // called by the destructor, it may also be called from elsewhere if
  // needed.  Only the first call is actioned; any later ones are ignored.
  void Flush();

  // An arbitrary limit on the length of a single log message.
  // To do this makes the streaming be more efficient.
  static const size_t kMaxLogMessageLen = 30000;

  // Theses should not be called directly outside of logging.*,
  // only passed as SendMethod arguments to other LogMessage methods:
  void SendToLog();  // Actually dispatch to the logs

  // Call abort() or similar to perform LOG(FATAL) crash.
  static void Fail();

  std::ostream &stream() { return *(data_->stream_);}

  // Must be called without the log_mutex held.  (L < log_mutex)
  static int64 num_messages(int severity);

private:
  void Init(const char *file, int line, LogSeverity severity,
            void (LogMessage::*send_method)());

  // Fully internal SendMethod cases:
  void SendToSinkAndLog(); // Send to sink if provided and dispatch to the logs
  void SendToSink(); // Send to sink if provided and dispatch to the logs
  void SaveOrSendToLog(); // Save to string vector if provided, else to logs
  // Write to string if provided and dispatch to the logs.
  void WriteToStringAndLog();

  // Extract the appended message, and omit the prefix
  std::string ExtractMessage();

  // Used to fill in crash information during LOG(FATAL) failures.
  void RecordCrashReason(glog_internal_namespace_::CrashReason* reason);

  //Counts of messages sent at each priority:
  static int64 num_messages_[NUM_SEVERITIES]; // under log_mutex

  // Prevent any subtle race conditions by wrapping a mutex lock around
  // the actual logging action per trace.
  void LogTraceWithMutexLock();

  // Wait for the registered sink  in "data" via WaitTillSent
  void WaitForSink();

  struct LogMessageData {
    LogMessageData() {}
    char *buf_;
    char *message_text_;   // Complete message text (points to selected buffer)
    LogStream *stream_allocated_;
    LogStream *stream_;
    LogSeverity severity_; // LogMessage level
    int line_;             // line number where logging call is.
    void (LogMessage::*send_method_)(); //Call this in destructor to send
    union {  // At most one of these is used: union to keep the size low.
      LogSink* sink_;             // NULL or sink to send message to
      std::vector<std::string>* outvec_; // NULL or vector to push message onto
      std::string* message_;             // NULL or string to write message into
    };
    time_t timestamp_;            // Seconds since the Epoch.
    tm tm_time_;                  // year, month, day, hour, minute, second
    const char *basename_;        // basename of file that called LOG
    const char *fullname_;        // fullname of file that called LOG
    size_t num_prefix_chars_;     // number of chars of prefix in this message
    size_t num_chars_to_log_;     // number of chars of msg to send to log
    bool has_been_flushed_;       // false => data has not been flushed
    bool first_fatal_;            // true => this was first fatal msg
    ~LogMessageData();
  private:
    LogMessageData(const LogMessageData&);
    void operator=(const LogMessageData&);
  };

  static LogMessageData fatal_msg_data_exclusive_;
  static LogMessageData fatal_msg_data_shared_;

  LogMessageData *allocated_; //identify LogMessageData allocated state
  LogMessageData *data_;

  // disallow copy and assign
  LogMessage(const LogMessage&);
  void operator=(const LogMessage&);
};

enum PRIVATE_LogStream_Counter {COUNTER};

// Allow folks to put a counter in the LOG_EVERY_X()'end messages. This
// only works if ostream is a LogStream. If the ostream is not a
// LogStream, you'll get an bad_cast at runtime.
std::ostream & operator << (std::ostream &os, PRIVATE_LogStream_Counter);

_END_GOOGLE_NAMESPACE_

#endif /* LOGMESSAGE_H_ */
