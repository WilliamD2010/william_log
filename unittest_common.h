/*
 * unittest_common.h
 *
 *  Created on: Nov 13, 2012
 *      Author: changqwa
 */

#ifndef UNITTEST_COMMON_H_
#define UNITTEST_COMMON_H_

#include "logging.h"

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&); \
  TypeName & operator = (const TypeName&)

template <typename T>
class FlagSaver {
public:
  FlagSaver(T&flag) :flag_(flag),default_value_(flag){}
  ~FlagSaver() {
    flag_ = default_value_;
  }

private:
  DISALLOW_COPY_AND_ASSIGN(FlagSaver);
  T &flag_;
  const T default_value_;
};

class CommonFlagsSaver {
public:
  CommonFlagsSaver() :
    FLAGS_logtostderr_(FLAGS_logtostderr),
    FLAGS_stderrthreshold_(FLAGS_stderrthreshold),
    FLAGS_alsologtostderr_(FLAGS_alsologtostderr),
    FLAGS_v_(FLAGS_v) {}

private:
  DISALLOW_COPY_AND_ASSIGN(CommonFlagsSaver);
  FlagSaver<bool> FLAGS_logtostderr_;
  FlagSaver<google::int32> FLAGS_stderrthreshold_;
  FlagSaver<bool> FLAGS_alsologtostderr_;
  FlagSaver<google::int32> FLAGS_v_;
};

#endif /* UNITTEST_COMMON_H_ */
