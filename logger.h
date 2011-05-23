#ifndef LOGGER_H
#define LOGGER_H

#define LOGGER_PATH "/var/tmp/mq.log"

/* TODO: is LOGGER_PATH working? */

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

#endif /* LOGGER_H */
