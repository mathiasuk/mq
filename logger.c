#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "logger.h"

#define DEBUG	1

/* Private methods: */
static void __logger (Logger * self, const char * prefix, char * fmt, va_list list);

Logger * logger_new (const char * path)
{
	Logger * logger = malloc (sizeof (Logger));
	if (!logger) {
		perror ("logger_new:malloc");
		exit (EXIT_FAILURE);
	}

	/* Set the path (or use the default: */
	if (path)
		logger->path = path;
	else
		logger->path = LOGGER_PATH;

	/* Open the log file: */
	logger->stream = fopen (path, "a");
	if (logger->stream == NULL) {
		perror ("logger_new:fopen");
		exit (EXIT_FAILURE);
	}

	/* Set the buffering to "line buffered": */
	setlinebuf (logger->stream);
		
	return logger;
}

void logger_close (Logger * self)
{
	if (fclose (self->stream)) {
		perror ("logger_close:fclose");
		exit (EXIT_FAILURE);
	}
}

/* Print an info message to the log file: */
void logger_info (Logger * self, char * fmt, ...)
{
	va_list list;

	/* prepare list for va_arg */
	va_start (list, fmt);

	__logger(self, "INFO: ", fmt, list);
}

/* Print a warning message to the log file: */
void logger_warn (Logger * self, char * fmt, ...)
{
	va_list list;

	/* prepare list for va_arg */
	va_start (list, fmt);

	__logger(self, "WARN: ", fmt, list);
}

/* Print a critical message to the log file: */
void logger_crit (Logger * self, char * fmt, ...)
{
	va_list list;

	/* prepare list for va_arg */
	va_start (list, fmt);

	__logger(self, "CRIT: ", fmt, list);
}

/* Print a debug message to the log file if debug is enabled: */
void logger_debug (Logger * self, char * fmt, ...)
{
	va_list list;

	if (!DEBUG)
		return ;

	/* prepare list for va_arg */
	va_start (list, fmt);

	__logger(self, "DEBUG: ", fmt, list);
}

/* Private methods: */

static void __logger (Logger * self, const char * prefix, char * fmt, va_list list)
{
	time_t t;
	struct tm * tm;
	char * fmt_full;
	char time_str[200];

	/* Get the local time as a string: */
	t = time (NULL);
	tm = localtime (&t);
	if (!tm) {
		perror ("__logger:localtime");
		exit (EXIT_FAILURE);
	}
	if (strftime (time_str, sizeof (time_str), "%b %e %H:%M:%S ", tm) == 0) {
		perror ("__logger:strftime");
		exit (EXIT_FAILURE);
	}

	/* Prepend timestamp (and prefix if necessary) to fmt: */
	fmt_full = malloc (snprintf (NULL, 0, "%s%s%s", time_str, prefix, fmt) + 1);
	if (!fmt_full) {
		perror ("__logger:malloc");
		exit (EXIT_FAILURE);
	}
	if (sprintf (fmt_full, "%s%s%s", time_str, prefix, fmt) < 0) {
		perror ("__logger:sprintf");
		exit (EXIT_FAILURE);
	}

	if (vfprintf (self->stream, fmt_full, list) < 0) {
		perror ("__logger_info:vfprintf");
		exit (EXIT_FAILURE);
	}
}
