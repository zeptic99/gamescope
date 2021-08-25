#pragma once

#ifdef __GNUC__
#define ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define ATTRIB_PRINTF(start, end)
#endif

enum LogPriority {
	LOG_ERROR = 1,
	LOG_INFO,
	LOG_DEBUG,
};

class LogScope {
	const char *name;
	void vprintf(enum LogPriority priority, const char *fmt, va_list args) ATTRIB_PRINTF(3, 0);
	void vlogf(enum LogPriority priority, const char *fmt, va_list args) ATTRIB_PRINTF(3, 0);

public:
	LogScope(const char *name);
	void errorf(const char *fmt, ...) ATTRIB_PRINTF(2, 3);
	void infof(const char *fmt, ...) ATTRIB_PRINTF(2, 3);
	void debugf(const char *fmt, ...) ATTRIB_PRINTF(2, 3);

	void errorf_errno(const char *fmt, ...) ATTRIB_PRINTF(2, 3);
};
