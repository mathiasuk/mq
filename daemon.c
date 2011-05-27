#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include "daemon.h"
#include "logger.h"
#include "process.h"

#define MAX_EVENTS	64
#define TIMEOUT		1000

/* Private methods */
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

	/* Build the pipe's path */
	if (pipe_path)
		daemon->__pipe_path = pipe_path;
	else {
		if ((home = getenv ("HOME")) == NULL)
			return NULL;
		daemon->__pipe_path = malloc (snprintf (NULL, 0, "%s/%s", home, 
					                          PIPE_FILENAME) + 1);
		if (daemon->__pipe_path == NULL)
			return NULL;

		sprintf (daemon->__pipe_path, "%s/%s", home, PIPE_FILENAME);
	}

	daemon->__pipe = -1;

	/* Build the log file's path */
	if (log_path)
		daemon->__log_path = log_path;
	else {
		if ((home = getenv ("HOME")) == NULL)
			return NULL;

		daemon->__log_path = malloc(snprintf (NULL, 0, "%s/%s", home, LOG_FILENAME) + 1);
		if (daemon->__log_path == NULL)
			return NULL;

		sprintf (daemon->__log_path, "%s/%s", home, LOG_FILENAME);
	}

	daemon->__log = NULL;

	daemon->__ncpus = 0;

	daemon->__pslist = pslist_new();
	if (daemon->__pslist == NULL) {
		perror ("daemon_new:pslist_new");
		exit (EXIT_FAILURE);
	}

	return daemon;
}

/* 
 * Setup the daemon (open pipe, setup Logger, find number of CPUS)
 * args:   Daemon
 * return: void
 */
void daemon_setup (Daemon * self)
{
	struct epoll_event event;
	pid_t pid;
	struct stat buf;

	/* Setup the logging */
	self->__log = logger_new (self->__log_path);

	/* Find out the number of CPUs */
	self->__ncpus = sysconf (_SC_NPROCESSORS_ONLN);
	if (self->__ncpus < 1)
		logger_log (self->__log, CRITICAL, "daemon_setup: Can't find the number of CPUs");
	logger_log (self->__log, DEBUG, "Found %d CPU(s)", self->__ncpus);

	/* Check if pipe_path exists and is a named pipe */
	if (stat (self->__pipe_path, &buf) == -1)
		logger_log (self->__log, CRITICAL, "daemon_setup:stat: '%s'", self->__pipe_path);
	if (!S_ISFIFO (buf.st_mode))
		logger_log (self->__log, CRITICAL, "'%s' is not a named pipe", self->__pipe_path);

	/* Open the pipe */
	self->__pipe = open (self->__pipe_path, O_RDWR);
	if (self->__pipe == -1)
		logger_log (self->__log, CRITICAL, "daemon_setup:open");

	/* Create the epoll fd */
	self->__epfd = epoll_create (5);
	if (self->__epfd < 0)
		logger_log (self->__log, CRITICAL, "daemon_setup:epoll_create");

	/* We want to be notified when there is data to read */
	event.data.fd = self->__pipe;
	event.events = EPOLLIN;
	if (epoll_ctl (self->__epfd, EPOLL_CTL_ADD, self->__pipe, &event) == -1)
		logger_log (self->__log, CRITICAL, "daemon_setup:epoll_ctl");

	/* Daemonize */

	/* Create new process */
	pid = fork ();
	if (pid == -1)
		logger_log (self->__log, CRITICAL, "daemon_setup:fork");
	else if (pid != 0)
		exit (EXIT_SUCCESS);

	/* Create new session and process group */
	if (setsid () == -1)
		logger_log (self->__log, CRITICAL, "daemon_setup:setsid");

	/* Set the working directory to the root directory */
	if (chdir ("/") == -1)
		logger_log (self->__log, CRITICAL, "daemon_setup:chdir");

	/* Close stdin, stdout, stderr */
	close (0);
	close (1);
	close (2);

	/* redirect stdin, stdout, stderr to /dev/null */
	open ("/dev/null", O_RDWR);		/* stdin */
	dup (0);						/* stdout */
	dup (0);						/* stderr */

	logger_log (self->__log, INFO, "Daemon started with pid: %d", getpid ());
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
		logger_log (self->__log, CRITICAL, "daemon_run:malloc");

	/* We read one char at a time, stored in 'buf' at position 's',
	 * when a '\n' is read we process the line and reset 's' */
	s = buf;
	while (1) {
		nr_events = epoll_wait (self->__epfd, events, MAX_EVENTS, TIMEOUT);
		if (nr_events < 0)
			logger_log (self->__log, CRITICAL, "daemon_run:epoll_wait");

		if (nr_events == 0)
			/* Waited for TIMEOUT ms */
			;

		for (i = 0; i < nr_events ; i++)
		{
			if (events[i].events == EPOLLIN) {
				len = read (events[i].data.fd, s, sizeof (char));
				if (len == -1)
					logger_log (self->__log, CRITICAL, "daemon_run:read");
				if (len) {
					if (*s == '\n') {
						*s = '\0';
						logger_log (self->__log, DEBUG, "Read line: '%s'", buf);
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

	logger_log (self->__log, INFO, "Shutting daemon down");

	close (self->__pipe);
	logger_close (self->__log);

	exit (EXIT_SUCCESS);
}


/* Private methods */

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

	/* Strip heading whitespaces */
	while (*line == ' ')
		line++;

	/* Strip trailing whitespaces */
	while (line[strlen (line)-1] == ' ')
		line[strlen (line)-1] = '\0';

	if (!strlen (line))
		return ;

	/* We duplicate the 'line' string as strtok modifies it */
	if ((line_d = strdup (line)) == NULL)
		logger_log (self->__log, CRITICAL, "__daemon_parse_line:strdup:");

	/* Parse the action (first word of the line) */
	action = strtok (line_d, " ");

	/* TODO: split each action in its own private method */

	if (strcmp (action, "add") == 0) {
		/* Check for extra arguments */
		if (strtok (NULL, " ")) {
			/* Create Process: 
			 * "line + strlen(action) + 1" skips the "add " from the line*/
			p = process_new(line + strlen(action) + 1);
			if (p == NULL)
				logger_log (self->__log, CRITICAL, 
						    "__daemon_parse_line:process_new");
			s = process_str(p);
			if (s == NULL)
				logger_log (self->__log, CRITICAL,
							"__daemon_parse_line:process_print");
			logger_log (self->__log, DEBUG, "Created Process: '%s'", s);
			free (s);
			/* Add process to pslist */
			if (pslist_append(self->__pslist, p))
				logger_log (self->__log, CRITICAL,
						    "__daemon_parse_line:pslit_append");
			/* FIXME: remove this */
			process_run(p);
			logger_log (self->__log, DEBUG, "Running Process: '%s'",
					    process_str (p));
			/* End of FIXME */
		} else {
			logger_log (self->__log, WARNING, "Missing command for add: '%s'", 
					    line);
		}
	} else if (strcmp (action, "exit") == 0) {
		daemon_stop (self);
	} else if (strcmp (action, "debug") == 0) {
		logger_set_debugging (self->__log, 1);
	} else if (strcmp (action, "nodebug") == 0) {
		logger_set_debugging (self->__log, 0);
	} else {
		logger_log (self->__log, WARNING, "Unknown action: '%s'", line);
	}
}
