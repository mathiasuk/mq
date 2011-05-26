#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef enum {
	INFO,
	WARNING,
	CRITICAL,
	DEBUG,
} LogLevel;

typedef struct _Logger Logger;

struct _Logger 
{
	const char * path;
	FILE * stream;
};

Logger * logger_new (const char * path);
void logger_close (Logger * self);

void logger_log (Logger * self, LogLevel level, char * fmt, ...);

#endif /* LOGGER_H */
