/*
 * vlog_is_on_unittest.cc
 *
 *  Created on: Nov 13, 2012
 *      Author: changqwa
 */
#include "gtest/gtest.h"
#include "unittest_common.h"
#include <string>
#include "vlog_is_on.h"
#include "logging.h"
#include <iostream>

namespace google {
extern void ResetVModule();
extern int number_of_vmodule();
}

using std::string;
using namespace std;
using namespace google;


class VLOG_IS_ON_Test: public testing::Test {
public:
  VLOG_IS_ON_Test():vmodule(FLAGS_vmodule), v(FLAGS_v) {}
  void SetUp() {
    ResetVModule();
  }

protected:
  const FlagSaver<string>  vmodule;
  const FlagSaver<int32>  v;
};

TEST_F(VLOG_IS_ON_Test, FLAGS_vmodule_initializer) {
  FLAGS_vmodule = "log=1,lo*=2,l?g=3";
  VLOG_IS_ON(0);

  ASSERT_EQ(3,  number_of_vmodule());
}

class Module_Map_Test: public VLOG_IS_ON_Test { };
TEST_F(Module_Map_Test, entire_module_log_level) {
  SetVLOGLevel("vlog_is_on_unittest", 1);

  ASSERT_EQ(1,  number_of_vmodule());
  ASSERT_EQ(true,  VLOG_IS_ON(0));
  ASSERT_EQ(true,  VLOG_IS_ON(1));
  ASSERT_EQ(false, VLOG_IS_ON(2));
}

TEST_F(Module_Map_Test, interrogation_module_level) {
  SetVLOGLevel("vlog_is_o?_unittest", 2);

  ASSERT_EQ(1,  number_of_vmodule());
  ASSERT_EQ(true,  VLOG_IS_ON(1));
  ASSERT_EQ(true,  VLOG_IS_ON(2));
  ASSERT_EQ(false,  VLOG_IS_ON(3));
}

TEST_F(Module_Map_Test, star_module_level) {
  SetVLOGLevel("vlog*_unittest", 3);

  ASSERT_EQ(1,  number_of_vmodule());
  ASSERT_EQ(true,  VLOG_IS_ON(2));
  ASSERT_EQ(true,  VLOG_IS_ON(3));
  ASSERT_EQ(false,  VLOG_IS_ON(4));
}

class VLOG_Level_Setting_Test: public VLOG_IS_ON_Test { };
TEST_F(VLOG_Level_Setting_Test, overwrite_vmodule) {
  const int kType = 3;
  char *vmodule[kType] = {"vlog_is_on_unittest", "vlog_is_o?_unittest", "vlog*_unittest" };
  for (int index = 0; index < kType; index++) {
    SetVLOGLevel(vmodule[index], index);
  }
  ASSERT_EQ(3,  number_of_vmodule());

  for (int index = kType-1; index > -1; index--) {
     SetVLOGLevel(vmodule[index], index);
  }
  ASSERT_EQ(3,  number_of_vmodule());
}

TEST_F(VLOG_Level_Setting_Test, interrogation_already_been_maped_ignore) {
  SetVLOGLevel("vlog_is_o?_unittest", 2);
  SetVLOGLevel("vlog_is_on_unittest", 1);

  ASSERT_EQ(1,  number_of_vmodule());
}

TEST_F(VLOG_Level_Setting_Test, star_alread_been_maped_ignore) {
  SetVLOGLevel("vlog*_unittest", 3);
  SetVLOGLevel("vlog_is_o?_unittest", 2);
  SetVLOGLevel("vlog_is_on_unittest", 1);

  ASSERT_EQ(1,  number_of_vmodule());
}
