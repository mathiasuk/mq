#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "daemon.h"
#include "logger.h"
#include "process.h"

#define MAX_EVENTS	64
#define TIMEOUT		1000

/* Private methods: */
static void __daemon_parse_line (Daemon * self, char * line);

/* 
 * Create and initialise the Daemon
 * args:   path to pipe, path to log file
 * return: Daemon object or NULL on error
 */
Daemon * daemon_new (char * pipe_path, char * log_path)
{
	char * home;
	Daemon * daemon = malloc (sizeof (Daemon));

	if (!daemon)
		return NULL;

	/* Build the pipe's path: */
	if (pipe_path)
		daemon->pipe_path = pipe_path;
	else {
		if ((home = getenv ("HOME")) == NULL)
			return NULL;
		daemon->pipe_path = malloc (snprintf (NULL, 0, "%s/%s", home, 
					                          PIPE_FILENAME) + 1);
		if (daemon->pipe_path == NULL)
			return NULL;

		sprintf (daemon->pipe_path, "%s/%s", home, PIPE_FILENAME);
	}

	daemon->pipe = -1;

	/* Build the log file's path: */
	if (log_path)
		daemon->log_path = log_path;
	else {
		if ((home = getenv ("HOME")) == NULL)
			return NULL;

		daemon->log_path = malloc(snprintf (NULL, 0, "%s/%s", home, LOG_FILENAME) + 1);
		if (daemon->log_path == NULL)
			return NULL;

		sprintf (daemon->log_path, "%s/%s", home, LOG_FILENAME);
	}

	daemon->log = NULL;

	daemon->pslist = pslist_new();
	if (daemon->pslist == NULL) {
		perror ("daemon_new:pslist_new");
		exit (EXIT_FAILURE);
	}

	return daemon;
}

/* 
 * Setup the daemon (open pipe and setup Logger)
 * args:   Daemon
 * return: void
 */
void daemon_setup (Daemon * self)
{
	struct epoll_event event;
	pid_t pid;

	/* Setup the logging: */
	self->log = logger_new (self->log_path);

	/* TODO: check if pipe exists and create it if necessary */

	/* Open the pipe: */
	self->pipe = open (self->pipe_path, O_RDWR);
	if (self->pipe == -1)
		logger_log (self->log, CRITICAL, "daemon_setup:open\n");

	/* Create the epoll fd: */
	self->epfd = epoll_create (5);
	if (self->epfd < 0)
		logger_log (self->log, CRITICAL, "daemon_setup:epoll_create");

	/* We want to be notified when there is data to read: */
	event.data.fd = self->pipe;
	event.events = EPOLLIN;
	if (epoll_ctl (self->epfd, EPOLL_CTL_ADD, self->pipe, &event) == -1)
		logger_log (self->log, CRITICAL, "daemon_setup:epoll_ctl");

	/* Daemonize: */

	/* Create new process */
	pid = fork ();
	if (pid == -1)
		logger_log (self->log, CRITICAL, "daemon_setup:fork");
	else if (pid != 0)
		exit (EXIT_SUCCESS);

	/* Create new session and process group: */
	if (setsid () == -1)
		logger_log (self->log, CRITICAL, "daemon_setup:setsid\n");

	/* Set the working directory to the root directory: */
	if (chdir ("/") == -1)
		logger_log (self->log, CRITICAL, "daemon_setup:chdir\n");

	/* Close stdin, stdout, stderr: */
	close (0);
	close (1);
	close (2);

	/* redirect stdin, stdout, stderr to /dev/null: */
	open ("/dev/null", O_RDWR);		/* stdin */
	dup (0);						/* stdout */
	dup (0);						/* stderr */

	logger_log (self->log, INFO, "Daemon started with pid: %d\n", getpid ());
}

/*
 * Run the daemon
 * args:   Daemon
 * return: void
 */
void daemon_run (Daemon * self)
{
	struct epoll_event * events;
	char buf[LINE_MAX], * s;
	int i, nr_events, len;
	 
	events = malloc (sizeof (struct epoll_event) * MAX_EVENTS);
	if (!events)
		logger_log (self->log, CRITICAL, "daemon_run:malloc");

	/* We read one char at a time, stored in 'buf' at position 's',
	 * when a '\n' is read we process the line and reset 's' */
	s = buf;
	while (1) {
		nr_events = epoll_wait (self->epfd, events, MAX_EVENTS, TIMEOUT);
		if (nr_events < 0)
			logger_log (self->log, CRITICAL, "daemon_run:epoll_wait");

		if (nr_events == 0)
			/* Waited for TIMEOUT ms */
			;

		for (i = 0; i < nr_events ; i++)
		{
			if (events[i].events == EPOLLIN) {
				len = read (events[i].data.fd, s, sizeof (char));
				if (len == -1)
					logger_log (self->log, CRITICAL, "daemon_run:read");
				if (len) {
					if (*s == '\n') {
						*s = '\0';
						logger_log (self->log, DEBUG, "Read line: '%s'\n", buf);
						__daemon_parse_line(self, buf);
						s = buf;
					} else s++;
				}
			}
		}
	}
	return ;
}

/* 
 * Stop the daemon
 * args:   Daemon
 * return: void
 */
void daemon_stop (Daemon * self)
{
	/* TODO: kill all processes */

	logger_log (self->log, INFO, "Shutting daemon down");

	close (self->pipe);
	logger_close (self->log);

	exit (EXIT_SUCCESS);
}


/* Private methods: */

/*
 * Parse line and proceed accordingly
 * args:   Daemon, line to parse
 * return: void
 */
static void __daemon_parse_line (Daemon * self, char * line)
{
	char * action, * line_d, * s;
	Process * p;

	if (line == NULL)
		return ;

	/* Strip heading whitespaces: */
	while (*line == ' ')
		line++;

	/* Strip trailing whitespaces: */
	while (line[strlen (line)-1] == ' ')
		line[strlen (line)-1] = '\0';

	if (!strlen (line))
		return ;

	/* We duplicate the 'line' string as strtok modifies it: */
	if ((line_d = strdup (line)) == NULL)
		logger_log (self->log, CRITICAL, "__daemon_parse_line:strdup:");

	/* Parse the action (first word of the line): */
	action = strtok (line_d, " ");

	/* TODO: split each action in its own private method */

	if (strcmp (action, "add") == 0) {
		/* Check for extra arguments: */
		if (strtok (NULL, " ")) {
			/* Create Process: 
			 * "line + strlen(action) + 1" skips the "add " from the line*/
			p = process_new(line + strlen(action) + 1);
			if (p == NULL)
				logger_log (self->log, CRITICAL, 
						    "__daemon_parse_line:process_new");
			s = process_print(p);
			if (s == NULL)
				logger_log (self->log, CRITICAL,
							"__daemon_parse_line:process_print");
			logger_log (self->log, DEBUG, "Created Process: '%s'\n", s);
			free (s);
			/* Add process to pslist: */
			if (pslist_append(self->pslist, p))
				logger_log (self->log, CRITICAL,
						    "__daemon_parse_line:pslit_append");
			/* FIXME: remove this */
			process_run(p);
			logger_log (self->log, DEBUG, "Running Process: '%d'\n", p->pid);
			/* End of FIXME */
		} else {
			logger_log (self->log, WARNING, "Missing command for add: '%s'\n", 
					    line);
		}
	} else if (strcmp (action, "exit") == 0) {
		daemon_stop (self);
	} else if (strcmp (action, "debug") == 0) {
		logger_set_debugging (self->log, 1);
	} else if (strcmp (action, "nodebug") == 0) {
		logger_set_debugging (self->log, 0);
	} else {
		logger_log (self->log, WARNING, "Unknown action: '%s'\n", line);
	}
}
