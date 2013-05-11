/*
 *  config.h
 *
 *  Created on: Sep 23, 2012
 *      Author: changqwa
 */

#ifndef GOOGLE_CONFIG_H_
#define GOOGLE_CONFIG_H_

/* Namespace for Google classes */
#define GOOGLE_NAMESPACE google

/* Puts following code inside the Google namespace */
#define _START_GOOGLE_NAMESPACE_ namespace google {

/* Stops putting the code inside the Google namespace */
#define _END_GOOGLE_NAMESPACE_ }

/* define if your compiler has __attribute__ */
#define HAVE___ATTRIBUTE__ 1

/* Compiler has the symbol string */
#define HAVE_SYMBOLIZE

#define HAVE_STACKTRACE

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <syscall.h> header file. */
#define HAVE_SYSCALL_H 1

#endif /* GOOGLE_CONFIG_H_ */
