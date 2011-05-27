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

#endif /* PSLIST_H */
