#ifndef PROCESS_H
#define PROCESS_H

typedef enum {
	WAITING,
	RUNNING,
	STOPPED,
	DONE,
} PsState;

typedef struct _Process Process;

struct _Process 
{
	const char * command;
	PsState state;
};

Process * process_new (const char * command);
char * process_print (Process * self);

#endif /* PROCESS_H */
