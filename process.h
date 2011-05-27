#ifndef PROCESS_H
#define PROCESS_H

#include <unistd.h>

typedef enum {
	ANY,		/* Matches any other state, used to get list of processes */
	WAITING,
	RUNNING,
	STOPPED,
	KILLED,
	DONE,
} PsState;

typedef struct _Process Process;

struct _Process 
{
	const char * __command;
	PsState __state;
	pid_t __pid;
};

Process * process_new (const char * command);
char * process_str (Process * self);
int process_run (Process * self);

PsState process_get_state (Process * self);
pid_t process_get_pid (Process * self);

#endif /* PROCESS_H */
