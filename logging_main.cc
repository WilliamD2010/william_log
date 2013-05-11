/*
 * logging_main.cc
 *
 *  Created on: Jul 4, 2012
 *      Author: changqwa
 */

#include "gtest/gtest.h"
#include "logging.h"
//#include "LogMessage.h"
//#include "raw_logging.h"
#include "utilities.h"

class LogEnvironment : public testing::Environment
{
public:
  virtual void SetUp() {
    InstallUnitTestFailureFunction();
  }
  virtual void TearDown(){
    InstallProductFailureFunction();
  }
};

int main(int argc,char *argv[]) {
  testing::AddGlobalTestEnvironment(new LogEnvironment);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
