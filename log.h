#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>

// Debug macro - if DEBUG is not defined, pdebug() does nothing
#ifdef DEBUG
#define ldebug(fmt, ...) fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__)
#else
#define ldebug(fmt, ...) ((void)0)
#endif

#define linfo(fmt, ...) fprintf(stderr, "INFO: " fmt "\n", ##__VA_ARGS__)
#define lerror(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)

#endif /* _LOG_H_ */
