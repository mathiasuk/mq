/* 
 * This file is part of mq.
 * mq - src/pslist.h
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

#ifndef PSLIST_H
#define PSLIST_H

#define CHUNK_SIZE	10	/* amount to increase/decrease list's size */

#include "process.h"

typedef struct _PsList PsList;

struct _PsList 
{
	Process ** __list;
	int __len;
};

PsList * pslist_new (void);
int pslist_append (PsList * self, Process * process);
int pslist_get_nps (PsList * self, PsState state, int * list);
Process * pslist_get_ps (PsList * self, int index);
Process * pslist_get_ps_by_pid (PsList * self, pid_t pid);

#endif /* PSLIST_H */
