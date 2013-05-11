/*
 * file_capture.cc
 *
 *  Created on: Dec 13, 2012
 *      Author: changqwa
 */

#include "file_capture.h"
#include <iostream>
#include <strstream>
#include <string>
#include <unistd.h>
#include <fcntl.h>

using std::string;

namespace {
  //Dir we use for unittest temp files
  const string FLAGS_test_tmpdir = "/home/changqwa";
}

class TestOutStream : public std::ostrstream {
public:
  static const int kBufferSize = 1024;
  TestOutStream() : std::ostrstream(buffer_, kBufferSize){}
  std::ostream &stream() { return *this;}
  ~TestOutStream() { std::cerr << this->str() << std::endl; abort();}
private:
  char buffer_[kBufferSize];
};

#define CHECK(condition) \
  if(!(condition)) TestOutStream().stream() << "Check fail " #condition " "

// ----------------------------------------------------------------------
// Golden file functions
// ----------------------------------------------------------------------

class CapturedStream {
public:
  CapturedStream(int fd, const string & filename) :
    fd_(fd),
    uncaptured_fd_(-1),
    filename_(filename) {
    Capture();
  }

  ~CapturedStream() {
    if (uncaptured_fd_ != -1) {
      CHECK(close(uncaptured_fd_) != -1);
    }
  }

  // Start redirecting output to a file
  void Capture() {
    // Keep original stream for later
    CHECK(uncaptured_fd_ == -1) << ", Stream " << fd_ << " already captured!";
    uncaptured_fd_ = dup(fd_);
    CHECK(uncaptured_fd_ != -1);

    // Open file to save stream to
    int cap_fd = open(filename_.c_str(),
                      O_CREAT | O_TRUNC | O_WRONLY,
                      S_IRUSR | S_IWUSR);
    CHECK(cap_fd != -1);

    // Send stdout/stderr to this file
    fflush(NULL);
    CHECK(dup2(cap_fd, fd_) != -1);
    CHECK(close(cap_fd) != -1);
  }

  // Remove output redirection
  void StopCapture() {
    // Restore original stream
    if (uncaptured_fd_ != -1) {
      fflush(NULL);
      CHECK(dup2(uncaptured_fd_, fd_) != -1);
    }
  }

  const string & filename() const { return filename_; }

private:
  int fd_;             // file descriptor being captured
  int uncaptured_fd_;  // where the stream was originally being sent to
  string filename_;    // file where stream is being saved
};

static CapturedStream * s_captured_streams[STDERR_FILENO+1];

// Redirect a file descriptor to a file.
//   fd       - Should be STDOUT_FILENO or STDERR_FILENO
//   filename - File where output should be stored
static inline void CaptureTestOutput(int fd, const string & filename) {
//  CHECK((fd == STDOUT_FILENO) || (fd == STDERR_FILENO));
//  CHECK(s_captured_streams[fd] == NULL);
  s_captured_streams[fd] = new CapturedStream(fd, filename);
}

void CaptureTestStderr() {
  CaptureTestOutput(STDERR_FILENO, FLAGS_test_tmpdir + "/captured.err");
}

// Return the size (in bytes) of a file
static inline size_t GetFileSize(FILE * file) {
  fseek(file, 0, SEEK_END);
  return static_cast<size_t>(ftell(file));
}

// Read the entire content of a file as a string
static inline string ReadEntireFile(FILE * file) {
  const size_t file_size = GetFileSize(file);
  char * const buffer = new char[file_size];

  size_t bytes_last_read = 0;  // # of bytes read in the last fread()
  size_t bytes_read = 0;       // # of bytes read so far

  fseek(file, 0, SEEK_SET);

  // Keep reading the file until we cannot read further or the
  // pre-determined file size is reached.
  do {
    bytes_last_read = fread(buffer+bytes_read, 1, file_size-bytes_read, file);
    bytes_read += bytes_last_read;
  } while (bytes_last_read > 0 && bytes_read < file_size);

  const string content = string(buffer, buffer+bytes_read);
  delete[] buffer;

  return content;
}

// Get the captured stdout (when fd is STDOUT_FILENO) or stderr (when
// fd is STDERR_FILENO) as a string
static inline string GetCapturedTestOutput(int fd) {
  CHECK(fd == STDOUT_FILENO || fd == STDERR_FILENO);
  CapturedStream * const cap = s_captured_streams[fd];
  CHECK(cap)
    << ": did you forget CaptureTestStdout() or CaptureTestStderr()?";

  // Make sure everything is flushed.
  cap->StopCapture();

  // Read the captured file.
  FILE * const file = fopen(cap->filename().c_str(), "r");
  const string content = ReadEntireFile(file);
  fclose(file);

  delete cap;
  s_captured_streams[fd] = NULL;

  return content;
}

// Get the captured stderr of a test as a string.
string GetCapturedTestStderr() {
  return GetCapturedTestOutput(STDERR_FILENO);
}
