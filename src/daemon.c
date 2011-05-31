/* 
 * This file is part of mq.
 * mq - src/daemon.c
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
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>

#include "daemon.h"
#include "logger.h"
#include "process.h"

#define MAX_EVENTS	64
#define TIMEOUT		1000
#define BACKLOG		5		/* backlog for listen () */
#define FORK		0

/* Private methods */
static void _daemon_parse_line (Daemon * self, char * line);
static void _daemon_block_signals (Daemon * self);
static void _daemon_unblock_signals (Daemon * self);
static int _daemon_read_socket (Daemon * self, int sock);

/* Signal handler */
void sigchld_handler (int signum, siginfo_t * siginfo, void * ptr);
void sigterm_handler (int signum);

/* 
 * Create and initialise the Daemon
 * args:   path to socket, path to log file
 * return: Daemon object or NULL on error
 */
Daemon * daemon_new (char * sock_path, char * log_path)
{
	char * home;
	Daemon * daemon = malloc (sizeof (Daemon));

	if (!daemon)
		return NULL;

	/* Build the socket's path */
	if (sock_path)
		daemon->_sock_path = sock_path;
	else {
		if ((home = getenv ("HOME")) == NULL)
			return NULL;
		daemon->_sock_path = malloc (snprintf (NULL, 0, "%s/%s", home, 
					                          SOCK_FILENAME) + 1);
		if (daemon->_sock_path == NULL)
			return NULL;

		sprintf (daemon->_sock_path, "%s/%s", home, SOCK_FILENAME);
	}

	daemon->_sock = -1;

	/* Build the log file's path */
	if (log_path)
		daemon->_log_path = log_path;
	else {
		if ((home = getenv ("HOME")) == NULL)
			return NULL;

		daemon->_log_path = malloc (snprintf (NULL, 0, "%s/%s", home, LOG_FILENAME) + 1);
		if (daemon->_log_path == NULL)
			return NULL;

		sprintf (daemon->_log_path, "%s/%s", home, LOG_FILENAME);
	}

	daemon->_log = NULL;

	daemon->_ncpus = 0;

	daemon->_pslist = pslist_new ();
	if (daemon->_pslist == NULL) {
		perror ("daemon_new:pslist_new");
		exit (EXIT_FAILURE);
	}

	return daemon;
}

/* 
 * Setup the daemon (open socket, setup Logger, find number of CPUS)
 * args:   Daemon
 * return: void
 */
void daemon_setup (Daemon * self)
{
	int len;
	struct epoll_event event;
	struct sigaction sigchld_action;
#if FORK == 1
	pid_t pid;
#endif

	/* Setup the logging */
	self->_log = logger_new (self->_log_path);

	/* Find out the number of CPUs */
	self->_ncpus = sysconf (_SC_NPROCESSORS_ONLN);
	if (self->_ncpus < 1)
		logger_log (self->_log, CRITICAL, "daemon_setup: Can't find the number of CPUs");
	logger_log (self->_log, DEBUG, "Found %d CPU(s)", self->_ncpus);

	/* Unlink the socket's path to prevent EINVAL if the file already exist */
    if (unlink (self->_sock_path) == -1) {
		/* Don't report error if the path didn't exist */
		if (errno != ENOENT)
			logger_log (self->_log, CRITICAL, "daemon_setup:unlink");
	}

	/* Open the socket */
	if ((self->_sock = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:socket");

	/* Bind the socket */
	self->_slocal.sun_family = AF_UNIX;
    strcpy (self->_slocal.sun_path, self->_sock_path);
    len = strlen (self->_slocal.sun_path) + sizeof (self->_slocal.sun_family);
    if (bind (self->_sock, (struct sockaddr *) &self->_slocal, len) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:bind: %s", self->_sock_path);

	/* Listen on the socket */
	if (listen (self->_sock, BACKLOG) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:listen");

	/* Create the epoll fd */
	self->_epfd = epoll_create (5);
	if (self->_epfd < 0)
		logger_log (self->_log, CRITICAL, "daemon_setup:epoll_create");

	/* We want to be notified when there is data to read */
	event.data.fd = self->_sock;
	event.events = EPOLLIN;
	if (epoll_ctl (self->_epfd, EPOLL_CTL_ADD, self->_sock, &event) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:epoll_ctl");

	/* Initialise the signal mask */
	if (sigemptyset (&self->_blk_chld) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:sigemptyset");
	if (sigaddset (&self->_blk_chld, SIGCHLD) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:sigaddset");

	/* Attach the signal handlers */
	sigchld_action.sa_sigaction = sigchld_handler;
	sigemptyset (&sigchld_action.sa_mask);
	/* We use SA_NOCLDWAIT as sa_sigaction will give us all the info regarding
	 * the Process without waiting for it */
	sigchld_action.sa_flags = SA_SIGINFO | SA_NOCLDWAIT;

	if (sigaction (SIGCHLD, &sigchld_action, NULL) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:sigaction");
	if (signal (SIGTERM, sigterm_handler) == SIG_ERR)
		logger_log (self->_log, CRITICAL, "daemon_setup:signal");

	/* Daemonize */

#if FORK == 1
	/* Create new process */
	pid = fork ();
	if (pid == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:fork");
	else if (pid != 0)
		exit (EXIT_SUCCESS);

	/* Create new session and process group */
	if (setsid () == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:setsid");

	/* Set the working directory to the root directory */
	if (chdir ("/") == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:chdir");
#endif

	/* Close stdin, stdout, stderr */
	/* close (0);
	close (1);
	close (2); */

	/* redirect stdin, stdout, stderr to /dev/null */
	open ("/dev/null", O_RDWR);		/* stdin */
	dup (0);						/* stdout */
	dup (0);						/* stderr */

	logger_log (self->_log, INFO, "Daemon started with pid: %d", getpid ());
}

/*
 * Run the daemon
 * args:   Daemon
 * return: void
 */
void daemon_run (Daemon * self)
{
	struct epoll_event * events;
	int i, n_events, sock;
	struct epoll_event event;
	 
	events = malloc (sizeof (struct epoll_event) * MAX_EVENTS);
	if (!events)
		logger_log (self->_log, CRITICAL, "daemon_run:malloc");

	/* Main loop */
	for (;;) {
		n_events = epoll_wait (self->_epfd, events, MAX_EVENTS, TIMEOUT);

		if (n_events < 0) {
			if (errno == EINTR) /* call was interrupted by a signal handler */
				continue ;
			logger_log (self->_log, CRITICAL, "daemon_run:epoll_wait");
		}

		if (n_events == 0)
			/* Waited for TIMEOUT ms */
			;

		/* Loop on all events */
		for (i = 0; i < n_events; i++) {
			if (events[i].data.fd == self->_sock) {
				/* Check that the socket is ready */
				if (!(events[i].events & EPOLLIN))
					logger_log (self->_log, CRITICAL,
								"daemon_run: Unexpected event: %x", events[i].events);

				/* Main socket is ready */
				/* Accept the new connection */
				sock = accept (self->_sock, NULL, NULL);
				if (sock == -1)
					logger_log (self->_log, CRITICAL, "daemon_run:accept");

				/* Add the new socket to our epoll_event */
				event.events = EPOLLIN;
				event.data.fd = sock;
				if (epoll_ctl (self->_epfd, EPOLL_CTL_ADD, sock, &event) == -1)
					logger_log (self->_log, CRITICAL, "daemon_setup:epoll_ctl (2)");
			} else {
				/* Handle the existing socket */
				if (_daemon_read_socket(self, events[i].data.fd))
					logger_log (self->_log, CRITICAL, "daemon_setup:_daemon_read_socket");
			}
		}
	}
}

/* 
 * Stop the daemon
 * args:   Daemon
 * return: void
 */
void daemon_stop (Daemon * self)
{
	/* Block signals */
	_daemon_block_signals (self);

	/* TODO: kill all processes */

	logger_log (self->_log, INFO, "Shutting daemon down");

	close (self->_sock);
	logger_close (self->_log);

	/* Unblock signals */
	_daemon_unblock_signals (self);

	exit (EXIT_SUCCESS);
}

/*
 * Run processes in WAITING state if any CPUs available
 * args:   Daemon
 * return: void
 */
void daemon_run_processes (Daemon * self)
{
	int n_running = 0;		/* number of Processes currently running */
	int n_waiting = 0;		/* number of Processes waiting */
	int * l_waiting = NULL;	/* array of Processes waiting */
	Process * p = NULL;
	int i;

	/* Block signals */
	_daemon_block_signals (self);

	/* Check if the number of running processes is less than 
	 * the number of CPUs available */
	n_running = pslist_get_nps (self->_pslist, RUNNING, NULL);
	if (n_running >= self->_ncpus) {
		/* Unblock signals */
		_daemon_unblock_signals (self);
		return ;
	}

	/* Get the list of processes waiting */
	l_waiting  = malloc (pslist_get_nps (self->_pslist, WAITING, NULL) * 
			             sizeof (int));
	if (l_waiting == NULL) {
		logger_log (self->_log, WARNING, "daemon_run_processes:malloc");
		/* Unblock signals */
		_daemon_unblock_signals (self);
		return ;
	}

	n_waiting = pslist_get_nps (self->_pslist, WAITING, l_waiting);

	/* Start as many Processes as we have free CPUs */
	for (i = 0; (i < n_waiting) && (n_running <= self->_ncpus); i++)
	{
		p = pslist_get_ps (self->_pslist, l_waiting[i]);
		if (process_run (p) == 0) {
			logger_log (self->_log, DEBUG, "Running Process: '%s'",
					process_str (p));
			n_running++;
		} else {
			logger_log (self->_log, WARNING, "Failed to run Process: '%s'",
					process_str (p));
		}

	}

	free (l_waiting);

	/* Unblock signals */
	_daemon_unblock_signals (self);
}

/*
 * Wait for terminated processes 
 * args:   Daemon, siginfo_t from signal
 * return: void
 */
void daemon_wait_process (Daemon * self, siginfo_t * siginfo)
{
	Process * p;

	/* Block signals */
	_daemon_block_signals (self);

	/* Check that we received the expected signal */
	if (siginfo->si_signo != SIGCHLD)
		logger_log (self->_log, CRITICAL, "Expected signal %d, received %d", 
					SIGCHLD, siginfo->si_signo);

	/* Get Process which emitted the signal */
	p = pslist_get_ps_by_pid (self->_pslist, siginfo->si_pid);
	if (p == NULL)
		logger_log (self->_log, CRITICAL,
					"daemon_wait_process:Can't find Process with pid %d", siginfo->si_pid);

	/* "Wait" on the process */
	if (process_wait (p, siginfo))
		logger_log (self->_log, CRITICAL, "daemon_wait_process:process_wait", 
					siginfo->si_pid);

	logger_log (self->_log, DEBUG, "daemon_wait_process:waited on process (%d)", 
			siginfo->si_pid);

	/* Unblock signals */
	_daemon_unblock_signals (self);
}

/* Private methods */

/*
 * Parse line and proceed accordingly
 * args:   Daemon, line to parse
 * return: void
 */
static void _daemon_parse_line (Daemon * self, char * line)
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
		logger_log (self->_log, CRITICAL, "_daemon_parse_line:strdup:");

	/* Parse the action (first word of the line) */
	action = strtok (line_d, " ");

	/* TODO: split each action in its own private method */

	if (strcmp (action, "add") == 0) {
		/* Check for extra arguments */
		if (strtok (NULL, " ")) {
			/* Create Process: 
			 * "line + strlen(action) + 1" skips the "add " from the line*/
			p = process_new (line + strlen (action) + 1);
			if (p == NULL)
				logger_log (self->_log, CRITICAL, 
						    "_daemon_parse_line:process_new");
			s = process_str (p);
			if (s == NULL)
				logger_log (self->_log, CRITICAL,
							"_daemon_parse_line:process_str");
			free (s);
			/* Add process to pslist */
			if (pslist_append (self->_pslist, p))
				logger_log (self->_log, CRITICAL,
						    "_daemon_parse_line:pslit_append");
			logger_log (self->_log, DEBUG, "Added Process to queue: '%s'", s);

			/* Start processes if any CPUs are available */
			daemon_run_processes (self);
		} else {
			logger_log (self->_log, WARNING, "Missing command for add: '%s'", 
					    line);
		}
	} else if (strcmp (action, "exit") == 0) {
		daemon_stop (self);
	} else if (strcmp (action, "debug") == 0) {
		logger_set_debugging (self->_log, 1);
	} else if (strcmp (action, "nodebug") == 0) {
		logger_set_debugging (self->_log, 0);
	} else {
		logger_log (self->_log, WARNING, "Unknown action: '%s'", line);
	}
}

/*
 * Block all signals
 * args:   Daemon
 * return: void
 */
static void _daemon_block_signals (Daemon * self)
{
	if (sigprocmask (SIG_BLOCK, &self->_blk_chld, NULL) == -1)
		logger_log (self->_log, CRITICAL, "_daemon_block_signals:sigprocmask");
}

/*
 * Unblock all signals
 * args:   Daemon
 * return: void
 */
static void _daemon_unblock_signals (Daemon * self)
{
	if (sigprocmask (SIG_UNBLOCK, &self->_blk_chld, NULL) == -1)
		logger_log (self->_log, CRITICAL, "_daemon_block_signals:sigprocmask");
}

/* 
 * Read from the given socket
 * args:   Daemon, socket
 * return: 0 on success, 1 on error
 */
static int _daemon_read_socket (Daemon * self, int sock)
{
	char buf[LINE_MAX];
	int len;

	len = recv (sock, buf, 100, 0);
	if (len < 0)
		return 1;

	/* Other side has closed the socket */
	if (len == 0) {
		if (close(sock) == -1)
			return 1;
	}

	/* Remove the EOL if any */
	if (len > 1 && buf[len - 1] == '\n')
		buf[len - 1] = '\0';
	else
		buf[len] = '\0';

	logger_log (self->_log, DEBUG, "Read line: '%s'", buf);
	_daemon_parse_line (self, buf);	

	return 0;
}


/* Signal handlers */
void sigchld_handler (int signum, siginfo_t * siginfo, void * ptr)
{
	extern Daemon * d;
	daemon_wait_process (d, siginfo);
	daemon_run_processes (d);
}

void sigterm_handler (int signum)
{
	extern Daemon * d;
	/* FIXME: _log should be made private, or a daemon_log method implemented */
	logger_log (d->_log, INFO, "Got signal: %d", signum);
}
