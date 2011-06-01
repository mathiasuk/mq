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
#include <stdio.h>

#include "daemon.h"
#include "logger.h"
#include "process.h"

#define MAX_EVENTS	64
#define TIMEOUT		1000
#define BACKLOG		5		/* backlog for listen () */
#define FORK		1

/* Private methods */
static int _daemon_daemonize (Daemon * self);
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
Daemon * daemon_new (char * sock_path, char * pid_path, char * log_path)
{
	Daemon * daemon = malloc (sizeof (Daemon));
	struct epoll_event event;
	int len;

	if (!daemon)
		return NULL;

	/* Build the socket's path */
	if (sock_path == NULL || strlen (sock_path) == 0)
		return NULL;
	daemon->_sock_path = sock_path;

	/* Build the pidfile's path */
	if (pid_path == NULL || strlen (pid_path) == 0)
		return NULL;
	daemon->_pid_path = pid_path;

	/* Build the logfile's path */
	if (log_path == NULL || strlen (log_path) == 0)
		return NULL;
	daemon->_log_path = log_path;

	/* Setup the logging */
	daemon->_log = logger_new (daemon->_log_path);

	/* Find out the number of CPUs */
	daemon->_ncpus = sysconf (_SC_NPROCESSORS_ONLN);
	if (daemon->_ncpus < 1)
		logger_log (daemon->_log, CRITICAL,
					"daemon_setup: Can't find the number of CPUs");
	logger_log (daemon->_log, DEBUG, "Found %d CPU(s)", daemon->_ncpus);

	/* Unlink the socket's path to prevent EINVAL if the file already exist */
    if (unlink (daemon->_sock_path) == -1) {
		/* Don't report error if the path didn't exist */
		if (errno != ENOENT)
			logger_log (daemon->_log, CRITICAL, "daemon_setup:unlink");
	}

	/* Open the socket */
	if ((daemon->_sock = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
		logger_log (daemon->_log, CRITICAL, "daemon_setup:socket");

	/* Bind the socket */
	daemon->_slocal.sun_family = AF_UNIX;
    strcpy (daemon->_slocal.sun_path, daemon->_sock_path);
    len = strlen (daemon->_slocal.sun_path) + sizeof (daemon->_slocal.sun_family);
    if (bind (daemon->_sock, (struct sockaddr *) &daemon->_slocal, len) == -1)
		logger_log (daemon->_log, CRITICAL, "daemon_setup:bind: %s",
					daemon->_sock_path);

	/* Listen on the socket */
	if (listen (daemon->_sock, BACKLOG) == -1)
		logger_log (daemon->_log, CRITICAL, "daemon_setup:listen");

	/* Create the epoll fd */
	daemon->_epfd = epoll_create (5);
	if (daemon->_epfd < 0)
		logger_log (daemon->_log, CRITICAL, "daemon_setup:epoll_create");

	/* We want to be notified when there is data to read */
	event.data.fd = daemon->_sock;
	event.events = EPOLLIN;
	if (epoll_ctl (daemon->_epfd, EPOLL_CTL_ADD, daemon->_sock, &event) == -1)
		logger_log (daemon->_log, CRITICAL, "daemon_setup:epoll_ctl");

	daemon->_pslist = pslist_new ();
	if (daemon->_pslist == NULL) {
		perror ("daemon_new:pslist_new");
		exit (EXIT_FAILURE);
	}

	return daemon;
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

	/* Daemonize */
	if (_daemon_daemonize (self))
		return ;	/* In Client */
	 
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

	/* Close the socket */
	close (self->_sock);

	/* Unlink the socket's path */
    if (unlink (self->_sock_path) == -1)
		logger_log (self->_log, CRITICAL, "daemon_stop:unlink '%s'", self->_sock_path);

	/* Unlink the pid file */
    if (unlink (self->_pid_path) == -1)
		logger_log (self->_log, CRITICAL, "daemon_stop:unlink '%s'", self->_pid_path);

	/* Close the logger */
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
 * Daemonize (forks, etc.)
 * args:   Daemon
 * return: 0 in Daemon, 1 in Client (ie: parent process)
 */
static int _daemon_daemonize (Daemon * self)
{
	struct sigaction sigchld_action, sigterm_action;
	FILE * pid_file;
	pid_t pid;

	/* Create new process */
	pid = fork ();
	if (pid == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:fork");
	else if (pid != 0)		/* in Client */
		return 1;

	/* Write current PID to pid file */
	pid_file = fopen (self->_pid_path, "w");
	if (pid_file == NULL)
		logger_log (self->_log, CRITICAL, "daemon_setup:fopen: '%s'", self->_pid_path);
	if (fprintf (pid_file, "%d\n", getpid ()) == 0)
		logger_log (self->_log, CRITICAL, "daemon_setup:fwrite");
	if (fclose (pid_file) != 0)
		logger_log (self->_log, CRITICAL, "daemon_setup:fclose");

	/* Create new session and process group */
	if (setsid () == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:setsid");

	/* Set the working directory to the root directory */
	if (chdir ("/") == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:chdir");

	/* Initialise the signal masks for SIGCHLD */
	if (sigemptyset (&self->_blk_chld) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:sigemptyset");
	if (sigaddset (&self->_blk_chld, SIGCHLD) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:sigaddset");

	/* Initialise the signal masks for SIGTERM */
	if (sigemptyset (&self->_blk_term) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:sigemptyset");
	if (sigaddset (&self->_blk_term, SIGTERM) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:sigaddset");

	/* Attach the signal handler for SIGCHLD */
	sigchld_action.sa_sigaction = sigchld_handler;
	sigemptyset (&sigchld_action.sa_mask);
	/* We use SA_NOCLDWAIT as sa_sigaction will give us all the info regarding
	 * the Process without waiting for it */
	sigchld_action.sa_flags = SA_SIGINFO | SA_NOCLDWAIT;
	if (sigaction (SIGCHLD, &sigchld_action, NULL) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:sigaction");

	/* Attach the signal handler for SIGTERM */
	sigterm_action.sa_handler = sigterm_handler;
	sigemptyset (&sigterm_action.sa_mask);
	sigchld_action.sa_flags = 0;
	if (sigaction (SIGTERM, &sigterm_action, NULL) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:sigaction");

	/* Close stdin, stdout, stderr */
	close (0);
	close (1);
	close (2);

	/* redirect stdin, stdout, stderr to /dev/null */
	open ("/dev/null", O_RDWR);		/* stdin */
	dup (0);						/* stdout */
	dup (0);						/* stderr */

	logger_log (self->_log, INFO, "Daemon started with pid: %d", getpid ());
	logger_log (self->_log, DEBUG, "Set logfile path to: %s", self->_log_path);
	logger_log (self->_log, DEBUG, "Set pidfile path to: %s", self->_pid_path);
	logger_log (self->_log, DEBUG, "Set socket path to: %s", self->_sock_path);

	return 0;
}

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
	if (sigprocmask (SIG_BLOCK, &self->_blk_term, NULL) == -1)
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
	if (sigprocmask (SIG_UNBLOCK, &self->_blk_term, NULL) == -1)
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
	daemon_stop (d);
}
