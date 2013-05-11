/*
 * LogSink.cc
 *
 *  Created on: Jan 3, 2013
 *      Author: changqwa
 */

#include "LogSink.h"
#include <sstream>
#include <iomanip>
#include "utilities.h"

_START_GOOGLE_NAMESPACE_

using std::string;
using std::setw;
using std::setfill;

string LogSink::ToString(LogSeverity severity, const char* file, int line,
                         const struct ::tm* tm_time,
                         const char* message, size_t message_len) {
  std::ostringstream stream(string(message, message_len));
  stream.fill('0');

  // FIXME(jrvb): Updating this to use the correct value for usecs
  // requires changing the signature for both this method and
  // LogSink::send().  This change needs to be done in a separate CL
  // so subclasses of LogSink can be updated at the same time.
  int usecs = 0;

  stream << LogSeverityNames[severity][0]
         << setw(2) << 1+tm_time->tm_mon
         << setw(2) << tm_time->tm_mday
         << ' '
         << setw(2) << tm_time->tm_hour << ':'
         << setw(2) << tm_time->tm_min << ':'
         << setw(2) << tm_time->tm_sec << '.'
         << setw(6) << usecs
         << ' '
         << setfill(' ') << setw(5) << GetTID() << setfill('0')
         << ' '
         << file << ':' << line << "] ";

  stream << string(message, message_len);
  return stream.str();
}

_END_GOOGLE_NAMESPACE_
