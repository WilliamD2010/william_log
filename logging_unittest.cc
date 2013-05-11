/*
 * logging_unittest.cc
 *
 *  Created on: Aug 28, 2012
 *      Author: changqwa
 */

#include "file_capture.h"
#include "gtest/gtest.h"
#include <string>
#include <vector>
#include "logging.h"
#include "LogMessage.h"
#include "raw_logging.h"
#include "LogSink.h"
#include "unittest_common.h"

using std::string;
using std::vector;
using std::endl;
using namespace GOOGLE_NAMESPACE;

class SeverityLogImplementTest: public testing::Test {
protected:
  void SetUp() {
    FLAGS_logtostderr = true;
    FLAGS_stderrthreshold = NUM_SEVERITIES;
  }

private:
  CommonFlagsSaver flags_saver_;
};

TEST_F(SeverityLogImplementTest, NormalTraces) {
  const int kNormalTraceTypes = NUM_SEVERITIES - 1;
  const int64 stream_log_num[kNormalTraceTypes] = {
      LogMessage::num_messages(GLOG_INFO),
      LogMessage::num_messages(GLOG_WARNING),
      LogMessage::num_messages(GLOG_ERROR)};
  const int64 raw_log_num[kNormalTraceTypes] = {raw_num_messages(GLOG_INFO),
      raw_num_messages(GLOG_WARNING), raw_num_messages(GLOG_ERROR)};
  const char *log_info[2*kNormalTraceTypes]  =  {
      "info log", "warning log", "error log",
      "raw info log", "raw warning log", "raw error log"};

  CaptureTestStderr();
  LOG(INFO) << log_info[0];
  LOG(WARNING) << log_info[1];
  LOG(ERROR) << log_info[2];
  RAW_LOG(INFO, "%s", log_info[3]);
  RAW_LOG(WARNING, "%s", log_info[4]);
  RAW_LOG(ERROR, "%s", log_info[5]);
  const string early_stderr = GetCapturedTestStderr();

  for (int severity = 0; severity < 2*kNormalTraceTypes; severity++) {
    ASSERT_NE((int)early_stderr.find(log_info[severity]), -1);
  }
  for (int severity = 0; severity < kNormalTraceTypes; severity++) {
    ASSERT_EQ(LogMessage::num_messages(severity), stream_log_num[severity]+1);
  }
  for (int severity = 0; severity < kNormalTraceTypes; severity++) {
    ASSERT_EQ(raw_num_messages(severity), raw_log_num[severity]+1);
  }
}

TEST_F(SeverityLogImplementTest, Fatal) {
  const char *log_info[2] = {"fatal log", "raw fatal log"};
  const int64 stream_fatal_num = LogMessage::num_messages(GLOG_FATAL);
  const int64 raw_fatal_num = raw_num_messages(GLOG_FATAL);

  CaptureTestStderr();
  LOG(FATAL) << log_info[0];
  RAW_LOG(FATAL, "%s", log_info[1]);
  string early_stderr = GetCapturedTestStderr();

  for (int severity = 0; severity < 2; severity++) {
      ASSERT_NE((int)early_stderr.find(log_info[severity]), -1);
  }
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_num+1);
  ASSERT_EQ(raw_num_messages(GLOG_FATAL), raw_fatal_num+1);
}

class LogWithLevelsTest: public testing::Test {
protected:
  void SetUp() {
    FLAGS_logtostderr = false;
    FLAGS_alsologtostderr = false;
    FLAGS_stderrthreshold = NUM_SEVERITIES;
  }
  static const string LogAllTypeTraces() {
    CaptureTestStderr();
    LOG(INFO) << log_info[0];
    LOG(WARNING) << log_info[1];
    LOG(ERROR) << log_info[2];
    LOG(FATAL) << log_info[3];
    RAW_LOG(INFO, "%s", log_info[4]);
    RAW_LOG(WARNING, "%s", log_info[5]);
    RAW_LOG(ERROR, "%s", log_info[6]);
    RAW_LOG(FATAL, "%s", log_info[7]);
    return GetCapturedTestStderr();
  }

  static const char *log_info[2*NUM_SEVERITIES];
private:
  CommonFlagsSaver flags_saver_;
};
const char *LogWithLevelsTest::log_info[2*NUM_SEVERITIES]  =  {
      "info log", "warning log", "error log", "fatal log",
      "raw info log", "raw warning log", "raw error log", "raw fatal log"};

TEST_F(LogWithLevelsTest, Flags_stderrthreshold) {
  FLAGS_stderrthreshold = GLOG_ERROR;

  const string early_stderr = LogAllTypeTraces();

  for (int severity = 0; severity < FLAGS_stderrthreshold; severity++) {
    ASSERT_EQ((int)early_stderr.find(log_info[severity]), -1);
    ASSERT_EQ((int)early_stderr.find(log_info[severity+NUM_SEVERITIES]), -1);
  }
  for (int severity = FLAGS_stderrthreshold; severity < NUM_SEVERITIES; severity++) {
    ASSERT_NE((int)early_stderr.find(log_info[severity]), -1);
    ASSERT_NE((int)early_stderr.find(log_info[severity+NUM_SEVERITIES]), -1);
  }
}

TEST_F(LogWithLevelsTest, FLAGS_alsologtostderr) {
  FLAGS_alsologtostderr = true;

  const string early_stderr = LogAllTypeTraces();

  for (int severity = 0; severity < NUM_SEVERITIES; severity++) {
    ASSERT_NE((int)early_stderr.find(log_info[severity]), -1);
    ASSERT_NE((int)early_stderr.find(log_info[severity+NUM_SEVERITIES]), -1);
  }
}

namespace google {
extern void ResetVModule();
}

class VLogWithLevelsTest: public LogWithLevelsTest {
public:
  VLogWithLevelsTest(): v(FLAGS_v), vmodule(FLAGS_vmodule) {}
  void SetUp() {
    LogWithLevelsTest::SetUp();
    google::ResetVModule();
  }

private:
  const FlagSaver<int32>  v;
  const FlagSaver<string>  vmodule;
};

TEST_F(VLogWithLevelsTest, FLAGS_v) {
  FLAGS_v = 2;
  const char info[] = "vlog";
  const int64 stream_info_log_num = LogMessage::num_messages(GLOG_INFO);

  VLOG(1) << info << endl;
  VLOG(2) << info << endl;
  ASSERT_EQ(LogMessage::num_messages(GLOG_INFO), stream_info_log_num+2);
  VLOG(3) << info << endl;
  ASSERT_EQ(LogMessage::num_messages(GLOG_INFO), stream_info_log_num+2);
}

TEST_F(VLogWithLevelsTest, FLAGS_vmodule) {
  FLAGS_v = 2;
  FLAGS_vmodule = "logging_unittest=1";
  const char info[] = "vlog";
  const int64 stream_info_log_num = LogMessage::num_messages(GLOG_INFO);

  VLOG(1) << info << endl;
  ASSERT_EQ(LogMessage::num_messages(GLOG_INFO), stream_info_log_num+1);
  VLOG(2) << info << endl;
  ASSERT_EQ(LogMessage::num_messages(GLOG_INFO), stream_info_log_num+1);
}

class ConditionLogTest: public testing::Test {
protected:
  void SetUp() {
    FLAGS_logtostderr = false;
    FLAGS_stderrthreshold = NUM_SEVERITIES;
  }

private:
  CommonFlagsSaver flags_saver_;
};

TEST_F(ConditionLogTest, LOG_IF) {
  int64 stream_info_log_num = LogMessage::num_messages(GLOG_INFO);
  LOG_IF(INFO, true) << "log_if info";
  ASSERT_EQ(LogMessage::num_messages(GLOG_INFO), stream_info_log_num + 1);

  stream_info_log_num = LogMessage::num_messages(GLOG_INFO);
  LOG_IF(INFO, false) << "don't log_if info";
  ASSERT_EQ(LogMessage::num_messages(GLOG_INFO), stream_info_log_num);
}

TEST_F(ConditionLogTest, LOG_ASSERT) {
  int64 stream_fatal_log_num = LogMessage::num_messages(GLOG_FATAL);
  LOG_ASSERT(true) << " LOG_ASSERT(true)";
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num);

  stream_fatal_log_num = LogMessage::num_messages(GLOG_FATAL);
  LOG_ASSERT(false) << " LOG_ASSERT(false)";
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num + 1);
}

TEST_F(ConditionLogTest, LOG_FIRST_N) {
  FLAGS_logtostderr = true;
  const int64 stream_info_log_num = LogMessage::num_messages(GLOG_INFO);
  const string log_every = "Log first 2, iteration ";

  CaptureTestStderr();
  for (int index = 0; index < 4; index++) {
    LOG_FIRST_N(INFO, 2) << log_every << COUNTER << endl;
  }
  string early_stderr = GetCapturedTestStderr();

  ASSERT_EQ(LogMessage::num_messages(GLOG_INFO), stream_info_log_num + 2);
  EXPECT_NE((int)early_stderr.find(log_every + "1"), -1);
  EXPECT_NE((int)early_stderr.find(log_every + "2"), -1);
}

TEST_F(ConditionLogTest, LOG_EVERY_1) {
  FLAGS_logtostderr = true;
  const int64 stream_info_log_num = LogMessage::num_messages(GLOG_INFO);
  const string log_every_1 = "Log every 1, iteration ";
  const string log_if_every_1 = "Log if every 1, iteration ";

  CaptureTestStderr();
  for (int index = 0; index < 2; index++) {
    LOG_EVERY_N(INFO, 1) << log_every_1 << COUNTER << endl;
  }
  for (int index = 0; index < 4; index++) {
    bool condition = index%2;
    LOG_IF_EVERY_N(INFO, condition, 1) << log_if_every_1 << COUNTER << endl;
  }
  string early_stderr = GetCapturedTestStderr();

  ASSERT_EQ(LogMessage::num_messages(GLOG_INFO), stream_info_log_num + 4);
  EXPECT_NE((int)early_stderr.find(log_every_1 + "1"), -1);
  EXPECT_NE((int)early_stderr.find(log_every_1 + "2"), -1);
  EXPECT_NE((int)early_stderr.find(log_if_every_1 + "2"), -1);
  EXPECT_NE((int)early_stderr.find(log_if_every_1 + "4"), -1);
}

TEST_F(ConditionLogTest, LOG_EVERY_N) {
  FLAGS_logtostderr = true;
  const int64 stream_info_log_num = LogMessage::num_messages(GLOG_INFO);
  const string log_every_2 = "Log every 2, iteration ";
  const string log_if_every_2 = "Log if every 2, iteration ";

  CaptureTestStderr();
  for (int index = 0; index < 4; index++) {
    LOG_EVERY_N(INFO, 2) << log_every_2 << COUNTER << endl;
  }
  for (int index = 0; index < 4; index++) {
    bool condition = index%2;
    LOG_IF_EVERY_N(INFO, condition, 2) << log_if_every_2 << COUNTER << endl;
  }
  string early_stderr = GetCapturedTestStderr();

  ASSERT_EQ(LogMessage::num_messages(GLOG_INFO), stream_info_log_num + 3);
  EXPECT_NE((int)early_stderr.find(log_every_2 + "1"), -1);
  EXPECT_NE((int)early_stderr.find(log_every_2 + "3"), -1);
  EXPECT_NE((int)early_stderr.find(log_if_every_2 + "2"), -1);
}

class CheckLogTest: public testing::Test {
protected:
  void SetUp() {
    FLAGS_logtostderr = false;
    FLAGS_stderrthreshold = NUM_SEVERITIES;
  }
  static const int class_static_const_internal = 1;

private:
  CommonFlagsSaver flags_saver_;
};

TEST_F(CheckLogTest, CHECK) {
  const int64 stream_fatal_log_num = LogMessage::num_messages(GLOG_FATAL);
  CHECK(true);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num);
  CHECK(false);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num + 1);
}

TEST_F(CheckLogTest, CHECK_comparision) {
  const int64 stream_fatal_log_num = LogMessage::num_messages(GLOG_FATAL);
  CHECK_EQ(1, 1);
  CHECK_NE(1, 2);
  CHECK_LE(2, 3);
  CHECK_LT(3, 4);
  CHECK_GE(6, 5);
  CHECK_GT(7, 6);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num);

  CHECK_EQ(0, 1);
  CHECK_NE(2, 2);
  CHECK_LE(4, 3);
  CHECK_LT(5, 4);
  CHECK_GE(4, 5);
  CHECK_GT(5, 6);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num+6);
}

TEST_F(CheckLogTest, val_not_defined_just_declared) {
  const int64 stream_fatal_log_num = LogMessage::num_messages(GLOG_FATAL);
  CHECK_EQ(class_static_const_internal, 0);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num+1);
}

TEST_F(CheckLogTest, CHECK_STREQ) {
  const int64 stream_fatal_log_num = LogMessage::num_messages(GLOG_FATAL);
  const char check_str[] = "CHECK_STREQ";
  CHECK_STREQ(check_str, check_str);
  CHECK_STREQ(check_str, "CHECK_STREQ");
  CHECK_STREQ(NULL, NULL);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num);

  CHECK_STREQ(check_str, "William");
  CHECK_STREQ(check_str, NULL);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num+2);
}

TEST_F(CheckLogTest, CHECK_STRNE) {
  const int64 stream_fatal_log_num = LogMessage::num_messages(GLOG_FATAL);
  const char check_str[] = "CHECK_STRNE";
  CHECK_STRNE(check_str, "William");
  CHECK_STRNE(check_str, NULL);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num);

  CHECK_STRNE(check_str, check_str);
  CHECK_STRNE(check_str, "CHECK_STRNE");
  CHECK_STRNE(NULL, NULL);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num+3);
}

TEST_F(CheckLogTest, CHECK_NEAR) {
  const int64 stream_fatal_log_num = LogMessage::num_messages(GLOG_FATAL);
  const float float_val = 2.1;

  CHECK_NEAR(float_val, float_val, 0.001);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num);

  CHECK_NEAR(float_val, float_val+0.1, 0.001);
  CHECK_NEAR(float_val, float_val-0.1, 0.001);
  ASSERT_EQ(LogMessage::num_messages(GLOG_FATAL), stream_fatal_log_num+2);
}

class LogStringTest: public testing::Test {
protected:
  void SetUp() {
    FLAGS_logtostderr = true;
    FLAGS_stderrthreshold = NUM_SEVERITIES;
  }

  static const char *kLogInfo[NUM_SEVERITIES];
private:
  CommonFlagsSaver flags_saver_;
};
const char *LogStringTest::kLogInfo[NUM_SEVERITIES] = {
    "info", "warning", "error", "fatal"};

TEST_F(LogStringTest, LOG_STRING_to_outvec_when_it_exists) {
  const int64 stream_log_num[NUM_SEVERITIES] = {
        LogMessage::num_messages(GLOG_INFO),
        LogMessage::num_messages(GLOG_WARNING),
        LogMessage::num_messages(GLOG_ERROR),
        LogMessage::num_messages(GLOG_FATAL)};

  const char *log_string = "LOG_STRING";
  vector<string> errors;
  CaptureTestStderr();
  LOG_STRING(INFO, &errors) << log_string;
  LOG_STRING(WARNING, &errors) << log_string;
  LOG_STRING(ERROR, &errors) << log_string;
  LOG_STRING(FATAL, &errors) << log_string;
  string early_stderr = GetCapturedTestStderr();

  for (int severity = 0; severity < NUM_SEVERITIES; severity++) {
    ASSERT_EQ(LogMessage::num_messages(severity), stream_log_num[severity]+1);
  }
  ASSERT_EQ((size_t)4, errors.size());
  ASSERT_EQ((int)early_stderr.find(log_string), -1);
}

TEST_F(LogStringTest, LOG_STRING_to_default_when_outvec_is_null) {
  const int64 stream_log_num[NUM_SEVERITIES] = {
          LogMessage::num_messages(GLOG_INFO),
          LogMessage::num_messages(GLOG_WARNING),
          LogMessage::num_messages(GLOG_ERROR),
          LogMessage::num_messages(GLOG_FATAL)};

  vector<string> *no_errors = NULL;
  CaptureTestStderr();
  LOG_STRING(INFO, no_errors) << kLogInfo[GLOG_INFO];
  LOG_STRING(WARNING, no_errors) << kLogInfo[GLOG_WARNING];
  LOG_STRING(ERROR, no_errors) << kLogInfo[GLOG_ERROR];
  LOG_STRING(FATAL, no_errors) << kLogInfo[GLOG_FATAL];
  string early_stderr = GetCapturedTestStderr();

  for (int severity = 0; severity < NUM_SEVERITIES; severity++) {
    ASSERT_EQ(LogMessage::num_messages(severity), stream_log_num[severity]+1);
    ASSERT_NE((int)early_stderr.find(kLogInfo[severity]), -1);
  }
}

TEST_F(LogStringTest, LOG_TO_STRING_to_string_and_default_when_it_exists) {
  const int64 stream_log_num[NUM_SEVERITIES] = {
        LogMessage::num_messages(GLOG_INFO),
        LogMessage::num_messages(GLOG_WARNING),
        LogMessage::num_messages(GLOG_ERROR),
        LogMessage::num_messages(GLOG_FATAL)};

  string error[NUM_SEVERITIES];
  CaptureTestStderr();
  LOG_TO_STRING(INFO, &error[GLOG_INFO]) << kLogInfo[GLOG_INFO];
  LOG_TO_STRING(WARNING, &error[GLOG_WARNING]) << kLogInfo[GLOG_WARNING];
  LOG_TO_STRING(ERROR, &error[GLOG_ERROR]) << kLogInfo[GLOG_ERROR];
  LOG_TO_STRING(FATAL, &error[GLOG_FATAL]) << kLogInfo[GLOG_FATAL];
  string early_stderr = GetCapturedTestStderr();

  for (int severity = 0; severity < NUM_SEVERITIES; severity++) {
    ASSERT_EQ(LogMessage::num_messages(severity), stream_log_num[severity]+1);
    ASSERT_NE((int)early_stderr.find(kLogInfo[severity]), -1);
    ASSERT_NE((int)error[severity].find(kLogInfo[severity]), -1);
  }
}

TEST_F(LogStringTest, LOG_TO_STRING_to_default_when_string_is_null) {
  const int64 stream_log_num[NUM_SEVERITIES] = {
          LogMessage::num_messages(GLOG_INFO),
          LogMessage::num_messages(GLOG_WARNING),
          LogMessage::num_messages(GLOG_ERROR),
          LogMessage::num_messages(GLOG_FATAL)};

  string *no_error = NULL;
  CaptureTestStderr();
  LOG_TO_STRING(INFO, no_error) << kLogInfo[GLOG_INFO];
  LOG_TO_STRING(WARNING, no_error) << kLogInfo[GLOG_WARNING];
  LOG_TO_STRING(ERROR, no_error) << kLogInfo[GLOG_ERROR];
  LOG_TO_STRING(FATAL, no_error) << kLogInfo[GLOG_FATAL];
  string early_stderr = GetCapturedTestStderr();

  for (int severity = 0; severity < NUM_SEVERITIES; severity++) {
    ASSERT_EQ(LogMessage::num_messages(severity), stream_log_num[severity]+1);
    ASSERT_NE((int)early_stderr.find(kLogInfo[severity]), -1);
  }
}

class TestLogSinkImpl : public LogSink {
 public:
  vector<string> errors;
  virtual void send(LogSeverity severity, const char* /* full_filename */,
                    const char* base_filename, int line,
                    const struct tm* tm_time,
                    const char* message, size_t message_len) {
    errors.push_back(
      ToString(severity, base_filename, line, tm_time, message, message_len));
  }
};

class LogSinkTest: public testing::Test {
protected:
  void SetUp() {
    FLAGS_logtostderr = true;
    FLAGS_stderrthreshold = NUM_SEVERITIES;
  }

  static const char *kLogInfo[NUM_SEVERITIES];

private:
  CommonFlagsSaver flags_saver_;
};
const char *LogSinkTest::kLogInfo[NUM_SEVERITIES] = {
    "info", "warning", "error", "fatal"};

TEST_F(LogSinkTest, LOG_TO_SINK_exists) {
  const int64 stream_log_num[NUM_SEVERITIES] = {
      LogMessage::num_messages(GLOG_INFO),
      LogMessage::num_messages(GLOG_WARNING),
      LogMessage::num_messages(GLOG_ERROR),
      LogMessage::num_messages(GLOG_FATAL)};

  TestLogSinkImpl sink;

  CaptureTestStderr();
  LOG_TO_SINK(&sink, INFO) << kLogInfo[GLOG_INFO];
  LOG_TO_SINK(&sink, WARNING) << kLogInfo[GLOG_WARNING];
  LOG_TO_SINK(&sink, ERROR) << kLogInfo[GLOG_ERROR];
  LOG_TO_SINK(&sink, FATAL) << kLogInfo[GLOG_FATAL];
  string early_stderr = GetCapturedTestStderr();

  ASSERT_EQ((size_t)4, sink.errors.size());
  for (int severity = 0; severity < NUM_SEVERITIES; severity++) {
    ASSERT_EQ(LogMessage::num_messages(severity), stream_log_num[severity]+1);
    ASSERT_NE((int)early_stderr.find(kLogInfo[severity]), -1);
  }
}

TEST_F(LogSinkTest, LOG_TO_SINK_null) {
  const int64 stream_log_num[NUM_SEVERITIES] = {
      LogMessage::num_messages(GLOG_INFO),
      LogMessage::num_messages(GLOG_WARNING),
      LogMessage::num_messages(GLOG_ERROR),
      LogMessage::num_messages(GLOG_FATAL)};

  LogSink *no_sink = NULL;

  CaptureTestStderr();
  LOG_TO_SINK(no_sink, INFO) << kLogInfo[GLOG_INFO];
  LOG_TO_SINK(no_sink, WARNING) << kLogInfo[GLOG_WARNING];
  LOG_TO_SINK(no_sink, ERROR) << kLogInfo[GLOG_ERROR];
  LOG_TO_SINK(NULL, FATAL) << kLogInfo[GLOG_FATAL];
  string early_stderr = GetCapturedTestStderr();

  for (int severity = 0; severity < NUM_SEVERITIES; severity++) {
    ASSERT_EQ(LogMessage::num_messages(severity), stream_log_num[severity]+1);
    ASSERT_NE((int)early_stderr.find(kLogInfo[severity]), -1);
  }
}

TEST_F(LogSinkTest, LOG_TO_SINK_BUT_NOT_TO_LOGFILE_exists) {
  const int64 stream_log_num[NUM_SEVERITIES] = {
      LogMessage::num_messages(GLOG_INFO),
      LogMessage::num_messages(GLOG_WARNING),
      LogMessage::num_messages(GLOG_ERROR),
      LogMessage::num_messages(GLOG_FATAL)};
  const char *log_info = "LOG_TO_SINK_BUT_NOT_TO_LOGFILE";

  TestLogSinkImpl sink;

  CaptureTestStderr();
  LOG_TO_SINK_BUT_NOT_TO_LOGFILE(&sink, INFO) << log_info;
  LOG_TO_SINK_BUT_NOT_TO_LOGFILE(&sink, WARNING) << log_info;
  LOG_TO_SINK_BUT_NOT_TO_LOGFILE(&sink, ERROR) << log_info;
  LOG_TO_SINK_BUT_NOT_TO_LOGFILE(&sink, FATAL) << log_info;
  string early_stderr = GetCapturedTestStderr();

  ASSERT_EQ((size_t)4, sink.errors.size());
  ASSERT_EQ((int)early_stderr.find(log_info), -1);
  for (int severity = 0; severity < NUM_SEVERITIES; severity++) {
    ASSERT_EQ(LogMessage::num_messages(severity), stream_log_num[severity]+1);
  }
}

TEST_F(LogSinkTest, LOG_TO_SINK_BUT_NOT_TO_LOGFILE_null) {
  const int64 stream_log_num[NUM_SEVERITIES] = {
      LogMessage::num_messages(GLOG_INFO),
      LogMessage::num_messages(GLOG_WARNING),
      LogMessage::num_messages(GLOG_ERROR),
      LogMessage::num_messages(GLOG_FATAL)};
  const char *log_info = "LOG_TO_SINK_BUT_NOT_TO_LOGFILE";

  LogSink *no_sink = NULL;
  CaptureTestStderr();
  LOG_TO_SINK_BUT_NOT_TO_LOGFILE(no_sink, INFO) << log_info;
  LOG_TO_SINK_BUT_NOT_TO_LOGFILE(no_sink, WARNING) << log_info;
  LOG_TO_SINK_BUT_NOT_TO_LOGFILE(no_sink, ERROR) << log_info;
  LOG_TO_SINK_BUT_NOT_TO_LOGFILE(NULL, FATAL) << log_info;
  string early_stderr = GetCapturedTestStderr();

  ASSERT_EQ((int)early_stderr.find(log_info), -1);
  for (int severity = 0; severity < NUM_SEVERITIES; severity++) {
    ASSERT_EQ(LogMessage::num_messages(severity), stream_log_num[severity]+1);
  }
}

