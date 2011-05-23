#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "logger.h"

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

void logger_info (Logger * self, char * fmt, ...)
{
	va_list list;

	/* prepare list for va_arg */
	va_start (list, fmt);

	__logger(self, "INFO", fmt, list);
}

/* Private methods: */
static void __logger (Logger * self, const char * prefix, char * fmt, va_list list)
{
	char * fmt_prefix;

	/* Add prefix to fmt if necessary: */
	if (prefix) {
		fmt_prefix = malloc (snprintf (NULL, 0, "%s %s", fmt, prefix) + 1);
		sprintf(fmt_prefix, "%s %s", fmt, prefix);
		if (!fmt_prefix) {
			perror ("logger:malloc");
			exit (EXIT_FAILURE);
		}
		if (sprintf (fmt_prefix, "%s: %s", prefix, fmt) < 0) {
			perror ("logger:sprintf");
			exit (EXIT_FAILURE);
		}
	}
	else
		fmt_prefix = fmt;

	if (vfprintf (self->stream, fmt_prefix, list) < 0) {
		perror ("logger_info:vfprintf");
		exit (EXIT_FAILURE);
	}
}
