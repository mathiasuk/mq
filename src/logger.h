/* 
 * This file is part of mq.
 * mq - src/logger.h
 * Copyright (C) 2011 Mathias Andre <mathias@acronycal.org>
 *
 * mq is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mq.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef enum {
	INFO,
	WARNING,
	CRITICAL,
	DEBUG
} LogLevel;

typedef struct _Logger Logger;

struct _Logger 
{
	const char * path;
	FILE * stream;
	short int debugging;	/* debugging on/off */
};

Logger * logger_new (const char * path);
void logger_close (Logger * self);

void logger_set_debugging (Logger * self, short int debugging);
void logger_log (Logger * self, LogLevel level, char * fmt, ...);

#endif /* LOGGER_H */
