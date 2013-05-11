/*
 * waitTillSent_unit.cc
 *
 *  Created on: Jan 3, 2013
 *      Author: changqwa
 */
#include "gtest/gtest.h"
#include "file_capture.h"
#include "mutex.h"
#include "logging.h"
#include "LogMessage.h"
#include "LogDestination.h"
#include "raw_logging.h"
#include "LogSink.h"
#include <pthread.h>
#include <vector>
#include <queue>
#include <string>

using std::vector;
using std::queue;
using std::string;
using namespace google;

static inline void SleepForMilliseconds(int t) {
  usleep(t * 1000);
}

class Thread {
 public:
  virtual ~Thread() {}

  void SetJoinable(bool) {}

  void Start() {
    pthread_create(&th_, NULL, &Thread::InvokeThread, this);
  }

  void Join() {
    pthread_join(th_, NULL);
  }

 protected:
  virtual void Run() = 0;

 private:
  // used for pthread_create as the third argument
  static void* InvokeThread(void* self) {
    ((Thread*)self)->Run();
    return NULL;
  }

  pthread_t th_;
};
// TestWaitingLogSink will save messages here
// No lock: Accessed only by TestLogSinkWriter thread
// and after its demise by its creator.
static vector<string> global_messages;

// helper for TestWaitingLogSink below.
// Thread that does the logic of TestWaitingLogSink
// It's free to use LOG() itself.
class TestLogSinkWriter : public Thread {
 public:
  TestLogSinkWriter() : should_exit_(false) {
    SetJoinable(true);
    Start();
  }

  // Just buffer it (can't use LOG() here).
  void Buffer(const string& message) {
    mutex_.Lock();
    RAW_LOG(INFO, "Buffering");
    messages_.push(message);
    mutex_.Unlock();
    RAW_LOG(INFO, "Buffered");
  }

  // Wait for the buffer to clear (can't use LOG() here).
  void Wait() {
    RAW_LOG(INFO, "Waiting");
    mutex_.Lock();
    while (!NoWork()) {
      mutex_.Unlock();
      SleepForMilliseconds(1);
      mutex_.Lock();
    }
    RAW_LOG(INFO, "Waited");
    mutex_.Unlock();
  }

  // Trigger thread exit.
  void Stop() {
    MutexLock l(&mutex_);
    should_exit_ = true;
  }

 private:

  // helpers ---------------

  // For creating a "Condition".
  bool NoWork() { return messages_.empty(); }
  bool HaveWork() { return !messages_.empty() || should_exit_; }

  // Thread body; CAN use LOG() here!
  virtual void Run() {
    while (1) {
      // if not wait till silent, this configure will leave a lot work to do.
      SleepForMilliseconds(10);
      mutex_.Lock();
      while (!HaveWork()) {
        mutex_.Unlock();
        SleepForMilliseconds(1);
        mutex_.Lock();
      }
      if (should_exit_ && messages_.empty()) {
        mutex_.Unlock();
        break;
      }
      // Give the main thread time to log its message,
      // so that we get a reliable log capture to compare to golden file.
      // Same for the other sleep below.
      SleepForMilliseconds(20);
      RAW_LOG(INFO, "Sink got a messages");  // only RAW_LOG under mutex_ here
      string message = messages_.front();
      messages_.pop();
      // Normally this would be some more real/involved logging logic
      // where LOG() usage can't be eliminated,
      // e.g. pushing the message over with an RPC:
      int messages_left = messages_.size();
      mutex_.Unlock();
      SleepForMilliseconds(20);
      // May not use LOG while holding mutex_, because Buffer()
      // acquires mutex_, and Buffer is called from LOG(),
      // which has its own internal mutex:
      // LOG()->LogToSinks()->TestWaitingLogSink::send()->Buffer()
      LOG(INFO) << "Sink is sending out a message: " << message;
      LOG(INFO) << "Have " << messages_left << " left";
      global_messages.push_back(message);
    }
  }

  // data ---------------

  Mutex mutex_;
  bool should_exit_;
  queue<string> messages_;  // messages to be logged
};

// A log sink that exercises WaitTillSent:
// it pushes data to a buffer and wakes up another thread to do the logging
// (that other thread can than use LOG() itself),
class TestWaitingLogSink : public LogSink {
 public:

  TestWaitingLogSink() {
    tid_ = pthread_self();  // for thread-specific behavior
    LogDestination::AddLogSink(this);
  }
  ~TestWaitingLogSink() {
    LogDestination::RemoveLogSink(this);
    writer_.Stop();
    writer_.Join();
  }

  // (re)define LogSink interface

  virtual void send(LogSeverity severity, const char* /* full_filename */,
                    const char* base_filename, int line,
                    const struct tm* tm_time,
                    const char* message, size_t message_len) {
    // Push it to Writer thread if we are the original logging thread.
    // Note: Something like ThreadLocalLogSink is a better choice
    //       to do thread-specific LogSink logic for real.
    if (pthread_equal(tid_, pthread_self())) {
      writer_.Buffer(ToString(severity, base_filename, line,
                              tm_time, message, message_len));
    }
  }
  virtual void WaitTillSent() {
    // Wait for Writer thread if we are the original logging thread.
    if (pthread_equal(tid_, pthread_self()))  writer_.Wait();
  }

 private:

  pthread_t tid_;
  TestLogSinkWriter writer_;
};

TEST(WaitTillSilentTest, WaitingLogSink) {
  FLAGS_logtostderr = true;
  CaptureTestStderr();

  {
     TestWaitingLogSink sink;
     // Sleeps give the sink threads time to do all their work,
     // so that we get a reliable log capture to compare to the golden file.
     const int kLogNumber = 4;
     const char *str_index[4] = { "1", "2", "3", "4" };

     for (int i = 0; i < kLogNumber; i++) {
       LOG_TO_SINK_BUT_NOT_TO_LOGFILE(&sink, INFO) << "Message " << str_index[i];
     }
   }
   string early_stderr = GetCapturedTestStderr();

   ASSERT_NE((int)early_stderr.find("Waiting"), -1);
   ASSERT_NE((int)early_stderr.find("Waited"), -1);
   ASSERT_NE((int)early_stderr.find("Have 0 left"), -1);
   ASSERT_EQ((int)early_stderr.find("Have 1 left"), -1);
   ASSERT_EQ(global_messages.size(), 4UL);
};
