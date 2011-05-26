#ifndef PSLIST_H
#define PSLIST_H

#define CHUNK_SIZE	10	/* amount to increase/decrease list's size */

#include "process.h"

typedef struct _PsList PsList;

struct _PsList 
{
	Process ** list;
	int len;
};

PsList * pslist_new (void);
int pslist_append (PsList * self, Process * process);

#endif /* PSLIST_H */
