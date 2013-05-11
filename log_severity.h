/*
 * log_severity.h
 *
 *  Created on: Nov 10, 2012
 *      Author: changqwa
 */

#ifndef LOG_SEVERITY_H_
#define LOG_SEVERITY_H_

typedef int LogSeverity;
const int GLOG_INFO = 0, GLOG_WARNING = 1, GLOG_ERROR = 2, GLOG_FATAL = 3,
    NUM_SEVERITIES = 4;

const int INFO = GLOG_INFO, WARNING = GLOG_WARNING,
    ERROR = GLOG_ERROR, FATAL = GLOG_FATAL;

// DFATAL is FATAL in debug mode, ERROR in normal mode
#ifdef NDEBUG
#define DFATAL_LEVEL ERROR
#else
#define DFATAL_LEVEL FATAL
#endif

#ifdef NDEBUG
enum { DEBUG_MODE = 0 };
#define IF_DEBUG_MODE(x)
#else
enum { DEBUG_MODE = 1 };
#define IF_DEBUG_MODE(x) x
#endif

extern const char* const LogSeverityNames[NUM_SEVERITIES];

#endif /* LOG_SEVERITY_H_ */
