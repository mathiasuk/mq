#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>


#include "logger.h"

#define DEBUGGING	1

/* Private methods: */
static void __logger (Logger * self, const char * prefix, char * fmt, va_list list);

/* 
 * Create and initialise the logger
 * args:   path to the log file
 * return: void
 */
Logger * logger_new (const char * path)
{
	Logger * logger = malloc (sizeof (Logger));
	if (!logger) {
		perror ("logger_new:malloc");
		exit (EXIT_FAILURE);
	}

	/* Set the path: */
	logger->path = path;

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

/*
 * Close the logging file
 * args:   Logger
 * return: void
 */
void logger_close (Logger * self)
{
	if (fclose (self->stream))
		logger_log (self, CRITICAL, "logger_close:fclose");
}

/* 
 * Print a message to the log file,
 * if level is CRITICAL then also prints last errnum message
 * args:   Logger, log level, printf style strings and args
 * return: void
 */
void logger_log (Logger * self, LogLevel level, char * fmt, ...)
{
	if (self == NULL)
		return ;

	/* TODO: add \n by default */

	va_list list;
	char * level_str;
	char * s_err;

	switch (level)
	{
		case INFO:
			level_str = "INFO: ";
			break;
		case WARNING:
			level_str = "WARNING: ";
			break;
		case CRITICAL:
			s_err = strerror(errno);
			level_str = malloc (snprintf (NULL, 0, "%s: (%s), ", "CRITICAL",
						                  s_err) + 1);
			if (!level_str) {
				perror ("logger_log:malloc");
				exit (EXIT_FAILURE);
			}
			sprintf (level_str, "%s: (%s), ", "CRITICAL", s_err);
			break;
		case DEBUG:
			/* Debug messages are only displayed if DEBUGGING is on: */
			if (!DEBUGGING)
				return;
			level_str = "DEBUG: ";
			break;
		default:
			level_str = "";
	}

	/* prepare list for va_arg */
	va_start (list, fmt);

	__logger (self, level_str, fmt, list);

	if (level == CRITICAL)
		exit (EXIT_FAILURE);
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
