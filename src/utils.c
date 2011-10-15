/* 
 * This file is part of mq.
 * mq - src/utils.c
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "utils.h"

/*
 * Behaves like malloc but memory is set to 0
 * args:   size of memory to allocate
 * return: point to memory
 */
void * malloc0 (size_t size)
{
	if (size == 0)
		return NULL;

	/* calloc set memory to 0 by default */
	return calloc (size, 1);
}

/*
 * Behaves like sprintf but automatically allocates a buffer
 * args:   format string, arguments
 * return: pointer to string on success, NULL on failure
 */
char * msprintf (char * fmt, ...)
{
	va_list ap, aq;
	char * str;
	int len, i;


	va_start (ap, fmt);
	va_copy (aq, ap);

	/* Check how long should the string be */
	len = vsnprintf (NULL, 0, fmt, aq);

	/* Try to allocate enough memory */
	str = malloc (len + 1);
	str[len] = '\0';
	if (str == NULL)
		return NULL;

	i = vsnprintf (str, len + 1, fmt, ap);

	va_end (aq);
	va_end (ap);

	if (i == -1)
		return NULL;

	return str;
}
