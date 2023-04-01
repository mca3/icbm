#include <stdarg.h>

enum {
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,

	LOG_LAST
};

extern int log_fd;
extern int log_level;

void vlogf(const int level, const char *fmt, va_list ap);
void infof(const char *fmt, ...);
void warnf(const char *fmt, ...);
void errorf(const char *fmt, ...);
void debugf(const char *fmt, ...);
