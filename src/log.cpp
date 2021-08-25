#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <string>

#include "log.hpp"

LogScope::LogScope(const char *name) {
	this->name = name;
}

void LogScope::vprintf(enum LogPriority priority, const char *fmt, va_list args) {
	fprintf(stderr, "%s: ", this->name);
	vfprintf(stderr, fmt, args);
}

void LogScope::vlogf(enum LogPriority priority, const char *fmt, va_list args) {
	fprintf(stderr, "%s: ", this->name);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
}

void LogScope::errorf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	this->vlogf(LOG_ERROR, fmt, args);
	va_end(args);
}

void LogScope::infof(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	this->vlogf(LOG_INFO, fmt, args);
	va_end(args);
}

void LogScope::debugf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	this->vlogf(LOG_DEBUG, fmt, args);
	va_end(args);
}

void LogScope::errorf_errno(const char *fmt, ...) {
	const char *err = strerror(errno);

	va_list args;
	va_start(args, fmt);
	this->vprintf(LOG_ERROR, fmt, args);
	va_end(args);

	fprintf(stderr, ": %s\n", err);
}
