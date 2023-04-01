#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "log.h"

// Done out of laziness...
#define LOGFN(NAME, LEVEL) void \
NAME(const char *fmt, ...) \
{ \
	va_list ap; \
	va_start(ap, fmt); \
	vlogf(LEVEL, fmt, ap); \
	va_end(ap); \
}

int log_fd = STDOUT_FILENO;
int log_level = LOG_DEBUG;
int log_color = 1;

static const char *lognames[LOG_LAST] = {
	[LOG_ERROR] = "ERROR",
	[LOG_WARN] = "WARN",
	[LOG_INFO] = "INFO",
	[LOG_DEBUG] = "DEBUG"
};

static const char *logfmt[LOG_LAST] = {
	[LOG_ERROR] = "\x1b[1;91m",
	[LOG_WARN] = "\x1b[1;93m",
	[LOG_INFO] = "\x1b[1;94m",
	[LOG_DEBUG] = "\x1b[1;95m",
};

static void
write_current_time(char *buf, size_t n)
{
	// XXX: I would like microsecond precision to three or so decimal
	// places.
	time_t t;
	struct tm *tmp;

	t = time(NULL);
	tmp = localtime(&t);
	if (!tmp) {
		// Shouldn't happen, will ignore.
		*buf = 0;
		return;
	}

	strftime(buf, n, "%F %H:%M:%S", tmp);
}

/* vlogf logs a message with a specified level.
 *
 * You likely do not want to use this.
 * See the helper functions infof, warnf, errorf, and debugf.
 */
void
vlogf(const int level, const char *fmt, va_list ap)
{
	const char *lname;
	const char *lfmt;
	const char *lfmtr;
	char time_buf[32] = {0};
	char buf[512] = {0};
	char prefix_buf[64] = {0};

	if (level > log_level)
		return;

	assert(level < LOG_LAST);

	lname = lognames[level];
	lfmt = log_color ? logfmt[level] : "";
	lfmtr = log_color ? "\x1b[0m" : "";

	write_current_time(time_buf, sizeof(time_buf));

	snprintf(prefix_buf, sizeof(prefix_buf), "[%s] %s%s%s", time_buf, lfmt, lname, lfmtr);
	vsnprintf(buf, sizeof(buf), fmt, ap);	

	dprintf(log_fd, "%s %s\n", prefix_buf, buf);
}

/* infof logs a message to the logger at the info level. */
LOGFN(infof, LOG_INFO)

/* warnf logs a message to the logger at the warning level. */
LOGFN(warnf, LOG_WARN)

/* errorf logs a message to the logger at the error level. */
LOGFN(errorf, LOG_ERROR)

/* debugf logs a message to the logger at the debug level. */
LOGFN(debugf, LOG_DEBUG)
