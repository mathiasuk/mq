#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef struct _Logger Logger;

struct _Logger 
{
	const char * path;
	FILE * stream;
};

Logger * logger_new (const char * path);
void logger_close (Logger * self);

void logger_info (Logger * self, char * fmt, ...);
void logger_warn (Logger * self, char * fmt, ...);
void logger_crit (Logger * self, char * fmt, ...);
void logger_debug (Logger * self, char * fmt, ...);

#endif /* LOGGER_H */
