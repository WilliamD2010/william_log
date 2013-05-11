/*
 * Logger.h
 *
 *  Created on: Aug 15, 2012
 *      Author: changqwa
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <string>
#include "logging.h"
#include "mutex.h"

using std::string;

// A Logger is the interface used by logging modules to emit entries
// to a log.  A typical implementation will dump formatted data to a
// sequence of files.  We also provide interfaces that will forward
// the data to another thread so that the invoker never blocks.
// Implementations should be thread-safe since the logging system
// will write to them from multiple threads.
_START_GOOGLE_NAMESPACE_

namespace base {
class Logger {
 public:
  virtual ~Logger(){}

  // Writes "message[0,message_len-1]" corresponding to an event that
  // occurred at "timestamp".  If "force_flush" is true, the log file
  // is flushed immediately.
  //
  // The input message has already been formatted as deemed
  // appropriate by the higher level logging facility.  For example,
  // textual log messages already contain timestamps, and the
  // file:linenumber header.
  virtual void Write(bool force_flush,
                     time_t timestamp,
                     const char* message,
                     int message_len) = 0;

  // Flush any buffered messages
  virtual void Flush() = 0;

  // Get the current LOG file size.
  // The returned value is approximate since some
  // logged data may not have been flushed to disk yet.
  virtual uint32 LogSize() = 0;
};

// Get the logger for the specified severity level.  The logger
// remains the property of the logging module and should not be
// deleted by the caller.  Thread-safe.
extern Logger* GetLogger(LogSeverity level);

// Set the logger for the specified severity level.  The logger
// becomes the property of the logging module and should not
// be deleted by the caller.  Thread-safe.
extern void SetLogger(LogSeverity level, Logger* logger);
}// end namespace base

// Encapsulates all file-system related state
class LogFileObject : public base::Logger {
public:
  LogFileObject(LogSeverity severity, const char* base_filename);
  ~LogFileObject();

  virtual void Write(bool force_flush, // Should we force a flush here?
                     time_t timestamp,  // Timestamp for this entry
                     const char* message,
                     int message_len);

  // Configuration options
  void SetBasename(const char* basename);
  void SetExtension(const char* ext);
  void SetSymlinkBasename(const char* symlink_basename);

  // Normal flushing routine
  virtual void Flush();

  // It is the actual file length for the system loggers,
  // i.e., INFO, ERROR, etc.
  virtual uint32 LogSize() {
    MutexLock l(&lock_);
    return file_length_;
  }

  // Internal flush routine.  Exposed so that FlushLogFilesUnsafe()
  // can avoid grabbing a lock.  Usually Flush() calls it after
  // acquiring lock_.
  void FlushUnlocked();

 private:
  static const uint32 kRolloverAttemptFrequency = 0x20;

  Mutex lock_;
  bool base_filename_selected_;
  string base_filename_;
  string symlink_basename_;
  string filename_extension_;     // option users can specify (eg to add port#)
  FILE* file_;
  LogSeverity severity_;
  uint32 bytes_since_flush_;
  uint32 file_length_;
  unsigned int rollover_attempt_;
  int64 next_flush_time_;         // cycle count at which to flush log

  // Actually create a logfile using the value of base_filename_ and the
  // supplied argument time_pid_string
  // REQUIRES: lock_ is held
  bool CreateLogfile(const char* time_pid_string);
};

_END_GOOGLE_NAMESPACE_

#endif /* LOGGER_H_ */
