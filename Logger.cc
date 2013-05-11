/*
 * Logger.cc
 *
 *  Created on: Aug 15, 2012
 *      Author: changqwa
 */
#include "Logger.h"
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h> // for errno
#include <iomanip>
#include <vector>
#include <strstream>
#include "utilities.h"
#include "LogDestination.h"

using std::vector;
using std::setw;

#define PATH_SEPARATOR '/'

DEFINE_int32(logbufsecs, 30,
             "Buffer log messages for at most this many seconds");

DEFINE_int32(max_log_size, 1800,
             "approx. maximum log file size (in MB). A value of 0 will "
             "be silently overridden to 1.");

DEFINE_bool(stop_logging_if_full_disk, false,
            "Stop attempting to log to disk if the disk is full.");

DEFINE_string(log_link, "", "Put additional links to the log "
              "files in this directory");

_START_GOOGLE_NAMESPACE_

// Safely get max_log_size, overriding to 1 if it somehow gets defined as 0
static int32 MaxLogSize() {
  return (FLAGS_max_log_size > 0 ? FLAGS_max_log_size : 1);
}

static vector<string>* logging_directories_list;
// Globally disable log writing (if disk is full)
static bool stop_writing = false;

static void GetTempDirectories(vector<string>* list) {
  list->clear();

  // Make sure we don't surprise anyone who's expecting a '/'
  const char candidate[] = "/tmp/";
  list->push_back(candidate);
}

const vector<string>& GetLoggingDirectories() {
  // Not strictly thread-safe but we're called early in InitGoogle().
  if (logging_directories_list == NULL) {
    logging_directories_list = new vector<string>;

    if (!FLAGS_log_dir.empty()) {
      // A dir was specified, we should use it
      logging_directories_list->push_back(FLAGS_log_dir.c_str());
    } else {
      GetTempDirectories(logging_directories_list);
      logging_directories_list->push_back("./");
    }
  }
  return *logging_directories_list;
}

LogFileObject::LogFileObject(LogSeverity severity,
                             const char* base_filename)
  : base_filename_selected_(base_filename != NULL),
    base_filename_((base_filename != NULL) ? base_filename : ""),
    symlink_basename_(glog_internal_namespace_::ProgramInvocationShortName()),
    filename_extension_(),
    file_(NULL),
    severity_(severity),
    bytes_since_flush_(0),
    file_length_(0),
    rollover_attempt_(kRolloverAttemptFrequency-1),
    next_flush_time_(0) {
  assert(severity >= 0);
  assert(severity < NUM_SEVERITIES);
}

LogFileObject::~LogFileObject() {
  MutexLock l(&lock_);
  if (file_ != NULL) {
    fclose(file_);
    file_ = NULL;
  }
}

void LogFileObject::SetBasename(const char* basename) {
  MutexLock l(&lock_);
  base_filename_selected_ = true;
  if (base_filename_ != basename) {
    // Get rid of old log file since we are changing names
    if (file_ != NULL) {
      fclose(file_);
      file_ = NULL;
      rollover_attempt_ = kRolloverAttemptFrequency-1;
    }
    base_filename_ = basename;
  }
}

void LogFileObject::SetExtension(const char* ext) {
  MutexLock l(&lock_);
  if (filename_extension_ != ext) {
    // Get rid of old log file since we are changing names
    if (file_ != NULL) {
      fclose(file_);
      file_ = NULL;
      rollover_attempt_ = kRolloverAttemptFrequency-1;
    }
    filename_extension_ = ext;
  }
}

void LogFileObject::SetSymlinkBasename(const char* symlink_basename) {
  MutexLock l(&lock_);
  symlink_basename_ = symlink_basename;
}

void LogFileObject::Flush() {
  MutexLock l(&lock_);
  FlushUnlocked();
}

void LogFileObject::FlushUnlocked(){
  if (file_ != NULL) {
    fflush(file_);
    bytes_since_flush_ = 0;
  }
  // Figure out when we are due for another flush.
  const int64 next = (FLAGS_logbufsecs
                      * static_cast<int64>(1000000));  // in usec
  next_flush_time_ = CycleClock_Now() + next;
}

bool LogFileObject::CreateLogfile(const char* time_pid_string) {
  string string_filename = base_filename_+filename_extension_+
                           time_pid_string;
  const char* filename = string_filename.c_str();
  // Make sure the file doesn't exist.
  // File can be read by all, and can be written by user and group.
  int fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0664);
  if (fd == -1) return false;
  // Mark the file close-on-exec. We don't really care if this fails
  fcntl(fd, F_SETFD, FD_CLOEXEC);

  file_ = fdopen(fd, "a");  // Make a FILE*.
  if (file_ == NULL) {  // Man, we're screwed!
    close(fd);
    unlink(filename);  // Erase the half-baked evidence: an unusable log file
    return false;
  }

  // We try to create a symlink called <program_name>.<severity>,
  // which is easier to use.  (Every time we create a new logfile,
  // we destroy the old symlink and create a new one, so it always
  // points to the latest logfile.)  If it fails, we're sad but it's
  // no error.
  if (!symlink_basename_.empty()) {
    // take directory from filename
    const char* slash = strrchr(filename, PATH_SEPARATOR);
    const string linkname =
      symlink_basename_ + '.' + LogSeverityNames[severity_];
    string linkpath;
    if ( slash ) linkpath = string(filename, slash-filename+1);  // get dirname
    linkpath += linkname;
    unlink(linkpath.c_str());                    // delete old one if it exists

    // Make the symlink be relative (in the same dir) so that if the
    // entire log directory gets relocated the link is still valid.
    const char *linkdest = slash ? (slash + 1) : filename;
    if (symlink(linkdest, linkpath.c_str()) != 0) {
      // silently ignore failures
    }

    // Make an additional link to the log file in a place specified by
    // FLAGS_log_link, if indicated
    if (!FLAGS_log_link.empty()) {
      linkpath = FLAGS_log_link + "/" + linkname;
      unlink(linkpath.c_str());                  // delete old one if it exists
      if (symlink(filename, linkpath.c_str()) != 0) {
        // silently ignore failures
      }
    }
  }
  return true;  // Everything worked
}

void LogFileObject::Write(bool force_flush,
                          time_t timestamp,
                          const char* message,
                          int message_len) {
  MutexLock l(&lock_);

  // We don't log if the base_name_ is "" (which means "don't write")
  if (base_filename_selected_ && base_filename_.empty()) {
    return;
  }

  if (static_cast<int>(file_length_ >> 20) >= MaxLogSize() ||
      PidHasChanged()) {
    if (file_ != NULL) fclose(file_);
    file_ = NULL;
    file_length_ = bytes_since_flush_ = 0;
    rollover_attempt_ = kRolloverAttemptFrequency-1;
  }

  // If there's no destination file, make one before outputting
  if (file_ == NULL) {
    // Try to rollover the log file every 32 log messages.  The only time
    // this could matter would be when we have trouble creating the log
    // file.  If that happens, we'll lose lots of log messages, of course!
    if (++rollover_attempt_ != kRolloverAttemptFrequency) return;
    rollover_attempt_ = 0;

    struct ::tm tm_time;
    localtime_r(&timestamp, &tm_time);

    // The logfile's filename will have the date/time & pid in it
    char time_pid_string[256];  // More than enough chars for time, pid, \0
    std::ostrstream time_pid_stream(time_pid_string, sizeof(time_pid_string));
    time_pid_stream.fill('0');
    time_pid_stream << 1900+tm_time.tm_year
        << setw(2) << 1+tm_time.tm_mon
        << setw(2) << tm_time.tm_mday
        << '-'
        << setw(2) << tm_time.tm_hour
        << setw(2) << tm_time.tm_min
        << setw(2) << tm_time.tm_sec
        << '.'
        << GetMainThreadPid()
        << '\0';

    if (base_filename_selected_) {
      if (!CreateLogfile(time_pid_string)) {
        perror("Could not create log file");
        fprintf(stderr, "COULD NOT CREATE LOGFILE '%s'!\n", time_pid_string);
        return;
      }
    } else {
      // If no base filename for logs of this severity has been set, use a
      // default base filename of
      // "<program name>.<hostname>.<user name>.log.<severity level>.".  So
      // logfiles will have names like
      // webserver.examplehost.root.log.INFO.19990817-150000.4354, where
      // 19990817 is a date (1999 August 17), 150000 is a time (15:00:00),
      // and 4354 is the pid of the logging process.  The date & time reflect
      // when the file was created for output.
      //
      // Where does the file get put?  Successively try the directories
      // "/tmp", and "."
      string stripped_filename(
          glog_internal_namespace_::ProgramInvocationShortName());
      string hostname = GetHostName();
      string uidname = MyUserName();
      // We should not call CHECK() here because this function can be
      // called after holding on to log_mutex. We don't want to
      // attempt to hold on to the same mutex, and get into a
      // deadlock. Simply use a name like invalid-user.
      if (uidname.empty()) uidname = "invalid-user";

      stripped_filename = stripped_filename+'.'+hostname+'.'
                          +uidname+".log."
                          +LogSeverityNames[severity_]+'.';
      // We're going to (potentially) try to put logs in several different dirs
      const vector<string> & log_dirs = GetLoggingDirectories();

      // Go through the list of dirs, and try to create the log file in each
      // until we succeed or run out of options
      bool success = false;
      for (vector<string>::const_iterator dir = log_dirs.begin();
           dir != log_dirs.end();
           ++dir) {
        base_filename_ = *dir + "/" + stripped_filename;
        if ( CreateLogfile(time_pid_string) ) {
          success = true;
          break;
        }
      }
      // If we never succeeded, we have to give up
      if ( success == false ) {
        perror("Could not create logging file");
        fprintf(stderr, "COULD NOT CREATE A LOGGINGFILE %s!", time_pid_string);
        return;
      }
    }

    // Write a header message into the log file
    char file_header_string[512];  // Enough chars for time and binary info
    std::ostrstream file_header_stream(file_header_string,
                                  sizeof(file_header_string));
    file_header_stream.fill('0');
    file_header_stream << "Log file created at: "
                       << 1900+tm_time.tm_year << '/'
                       << setw(2) << 1+tm_time.tm_mon << '/'
                       << setw(2) << tm_time.tm_mday
                       << ' '
                       << setw(2) << tm_time.tm_hour << ':'
                       << setw(2) << tm_time.tm_min << ':'
                       << setw(2) << tm_time.tm_sec << '\n'
                       << "Running on machine: "
                       << LogDestination::hostname() << '\n'
                       << "Log line format: [IWEF]mmdd hh:mm:ss.uuuuuu "
                       << "threadid file:line] msg" << '\n'
                       << '\0';
    int header_len = strlen(file_header_string);
    fwrite(file_header_string, 1, header_len, file_);
    file_length_ += header_len;
    bytes_since_flush_ += header_len;
  }

  // Write to LOG file
  if ( !stop_writing ) {
    // fwrite() doesn't return an error when the disk is full, for
    // messages that are less than 4096 bytes. When the disk is full,
    // it returns the message length for messages that are less than
    // 4096 bytes. fwrite() returns 4096 for message lengths that are
    // greater than 4096, thereby indicating an error.
    errno = 0;
    fwrite(message, 1, message_len, file_);
    if ( FLAGS_stop_logging_if_full_disk &&
         errno == ENOSPC ) {  // disk full, stop writing to disk
      stop_writing = true;  // until the disk is
      return;
    } else {
      file_length_ += message_len;
      bytes_since_flush_ += message_len;
    }
  } else {
    if ( CycleClock_Now() >= next_flush_time_ )
      stop_writing = false;  // check to see if disk has free space.
    return;  // no need to flush
  }

  // See important msgs *now*.  Also, flush logs at least every 10^6 chars,
  // or every "FLAGS_logbufsecs" seconds.
  if ( force_flush ||
       (bytes_since_flush_ >= 1000000) ||
       (CycleClock_Now() >= next_flush_time_) ) {
    FlushUnlocked();
  }
}
}// end namespace asb
