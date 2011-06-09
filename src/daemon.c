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
#include "utils.h"

#define MAX_EVENTS	64
#define TIMEOUT		1000
#define BACKLOG		5		/* backlog for listen () */
#define FORK		1

/* Private methods */
static int _daemon_daemonize (Daemon * self);
static MessageType _daemon_parse_line (Daemon * self, char * line,
									   int len, char ** message);
static void _daemon_block_signals (Daemon * self);
static void _daemon_unblock_signals (Daemon * self);
static int _daemon_read_socket (Daemon * self, int sock);
static MessageType _daemon_action_list (Daemon * self, char ** message);
static MessageType _daemon_action_move (Daemon * self, char ** argv, char ** message);

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
	Daemon * daemon = malloc0 (sizeof (Daemon));
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

	/* Initialise PsList */
	daemon->_pslist = pslist_new ();
	if (daemon->_pslist == NULL) {
		perror ("daemon_new:pslist_new");
		exit (EXIT_FAILURE);
	}

	/* Initialise list of Messages */
	daemon->_mlist = messagelist_new ();
	if (daemon->_mlist == NULL) {
		perror ("daemon_new:list_new");
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
	Message * message;

	/* Daemonize */
	if (_daemon_daemonize (self))
		return ;	/* In Client */
	 
	events = malloc0 (sizeof (struct epoll_event) * MAX_EVENTS);
	if (events == NULL)
		logger_log (self->_log, CRITICAL, "daemon_run:malloc0");

	/* Main loop */
	for (;;) {
		n_events = epoll_wait (self->_epfd, events, MAX_EVENTS, TIMEOUT);

		if (n_events < 0) {
			if (errno == EINTR) /* call was interrupted by a signal handler */
				continue ;
			logger_log (self->_log, CRITICAL, "daemon_run:epoll_wait");
		}

		if (n_events == 0) {
			/* Waited for TIMEOUT ms */
		}

		/* Loop on all events */
		for (i = 0; i < n_events; i++) {
			if (events[i].data.fd == self->_sock) 
			{
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
					logger_log (self->_log, CRITICAL, "daemon_run:epoll_ctl (2)");
			} 
			else 
			{
				/* Handle the existing socket */

				/* Write to socket if it's ready */
				if (events[i].events & EPOLLOUT) 
				{
					/* Look for any Message for the current socket */
					message = messagelist_get_message_by_sock (self->_mlist,
															   events[i].data.fd);
					if (message == NULL)
					{
						sleep (3600);
						logger_log (self->_log, CRITICAL,
									"Can't find message for socket %d, this shouln't happen",
									events[i].data.fd);
					}

					/* Send the message */
					if (message_send (message))
						logger_log (self->_log, CRITICAL, "daemon_run:message_send");
					logger_log (self->_log, DEBUG, "Sent message to socket (%d)",
								events[i].data.fd);

					/* Remove message from the MessageList */
					if (messagelist_remove (self->_mlist, message))
						logger_log (self->_log, CRITICAL,
									"daemon_run:_daemon_read_socket");

					message_del (message);

					/* We can now close the socket */
					/* This should not be necessary according to epoll(7)
					 * but removing it causes "Bad file descriptor" errors */
					if (epoll_ctl (self->_epfd, EPOLL_CTL_DEL, 
						events[i].data.fd, NULL) == -1)
						logger_log (self->_log, CRITICAL, "daemon_run:epoll_ctl");

					if (close (events[i].data.fd) == -1)
						logger_log (self->_log, CRITICAL, "daemon_run:close");
				}

				/* Read from socket if it's ready */
				if (events[i].events & EPOLLIN) 
				{
					if (_daemon_read_socket(self, events[i].data.fd))
						logger_log (self->_log, CRITICAL,
									"daemon_run:_daemon_read_socket");
				}

				/* Close socket if Client closed it */
				if (events[i].events & EPOLLHUP)
				{
					/* This should not be necessary according to epoll(7)
					 * but removing it causes "Bad file descriptor" errors */
					if (epoll_ctl (self->_epfd, EPOLL_CTL_DEL, 
						events[i].data.fd, NULL) == -1)
						logger_log (self->_log, CRITICAL, "daemon_run:epoll_ctl");

					logger_log (self->_log, DEBUG,
							"Client closed socket (%d)", events[i].data.fd);
					if (close (events[i].data.fd) == -1)
						logger_log (self->_log, CRITICAL, "daemon_run:close");
				}
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
	if (close (self->_sock) == -1)
		logger_log (self->_log, CRITICAL, "daemon_run:close");

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
	char * s;
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

	/* Get the number of processes waiting */
	n_waiting = pslist_get_nps (self->_pslist, WAITING, NULL);


	/* We only check this as the following malloc(0) would
	 * fail when running with -lefence, FIXME: this is not the case with
	 * malloc0 */
	if (n_waiting == 0)
	{
		/* No processes are currently waiting */
		/* Unblock signals */
		_daemon_unblock_signals (self);
		return ;
	}

	/* Get the list of processes waiting */
	l_waiting  = malloc0 (n_waiting * sizeof (int));
	if (l_waiting == NULL) {
		logger_log (self->_log, WARNING, "daemon_run_processes:malloc0");
		/* Unblock signals */
		_daemon_unblock_signals (self);
		return ;
	}

	pslist_get_nps (self->_pslist, WAITING, l_waiting);

	/* FIXME: string from process_str should be freed */
	/* Start as many Processes as we have free CPUs */
	for (i = 0; (i < n_waiting) && (n_running <= self->_ncpus); i++)
	{
		p = pslist_get_ps (self->_pslist, l_waiting[i]);
		if (process_run (p) == 0) {
			s = process_str (p);
			logger_log (self->_log, DEBUG, "Running Process (%d): '%s'", p->uid, s);
			free (s);
			n_running++;
		} else {
			s = process_str (p);
			logger_log (self->_log, WARNING, "Failed to run Process: '%s'", s);
			free (s);
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
 * args:   Daemon, line to parse, length of the line, pointer to a buffer for 
 *		   message to send back to client (buffer must be freed after use)
 * return: MessageType
 */
static MessageType _daemon_parse_line (Daemon * self, char * line,
									   int len, char ** message)
{
	char * action, * s;
	Process * p;
	int argc = 0, i;
	char ** argv;

	/* Set the default return message to NULL */
	*message = NULL;

	/* We expect line to be of the form:
	 * "arg1\0arg2\0arg3\0\n" */

	if (line == NULL)
		return KO;

	/* Strip heading whitespaces */
	while (*line == ' ')
		line++;

	/* Count the number of arguments (ie: number of '\0') */
	for (i = 0; line[i] != '\n'; i++)
		if (line[i] == '\0')
			argc++;

	/* Convert the line to an array of char *, we add one slot to
	 * NULL terminate the array */
	argv = malloc0 (sizeof (char *) * (argc + 1));
	if (!argv)
		logger_log (self->_log, CRITICAL, "_daemon_parse_line:malloc0");
	for (i = 0; i < argc; i++) {
		argv[i] = strdup (line);
		/* Move the the next argument in line */
		line += strlen(line) + 1;
	}
	/* NULL terminate the array (for execp) */
	argv[argc] = NULL;

	/* Parse the action (first argument), this should always exist */
	action = argv[0];

	/* Move to the next argument */
	argv++;

	/* TODO: split each action in its own private method */

	if (strcmp (action, "add") == 0) {
		/* Check for extra arguments */
		if (*argv != NULL)
		{
			/* Create Process:  */
			p = process_new (argv);
			if (p == NULL)
				logger_log (self->_log, CRITICAL, 
						    "_daemon_parse_line:process_new");
			s = process_str (p);
			if (s == NULL)
				logger_log (self->_log, CRITICAL,
							"_daemon_parse_line:process_str");
			/* Add process to pslist */
			if (pslist_append (self->_pslist, p))
				logger_log (self->_log, CRITICAL,
						    "_daemon_parse_line:pslit_append");
			logger_log (self->_log, DEBUG, "Added Process to queue: '%s'", s);
			free (s);

			/* Start processes if any CPUs are available */
			daemon_run_processes (self);

			return OK;
		} else {
			logger_log (self->_log, WARNING, "Expected: 'add COMMAND'");
			*message = strdup ("Missing command for add\n");
		}
	} else if (strcmp (action, "list") == 0 || strcmp (action, "ls") == 0) {
		return _daemon_action_list (self, message);
	} else if (strcmp (action, "move") == 0 || strcmp (action, "mv") == 0) {
		return _daemon_action_move (self, argv, message);
	} else if (strcmp (action, "exit") == 0) {
		daemon_stop (self);
		/* FIXME: OK is never returned as daemon is stopped ... */
		return OK;
	} else if (strcmp (action, "debug") == 0) {
		logger_set_debugging (self->_log, 1);
		return OK;
	} else if (strcmp (action, "nodebug") == 0) {
		logger_set_debugging (self->_log, 0);
		return OK;
	} else {
		logger_log (self->_log, WARNING, "Unknown action: '%s'", line);
	}
	return KO;
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
		logger_log (self->_log, CRITICAL, "_daemon_unblock_signals:sigprocmask");
	if (sigprocmask (SIG_UNBLOCK, &self->_blk_term, NULL) == -1)
		logger_log (self->_log, CRITICAL, "_daemon_unblock_signals:sigprocmask");
}

/* 
 * Read from the given socket
 * args:   Daemon, socket
 * return: 0 on success, 1 on error
 */
static int _daemon_read_socket (Daemon * self, int sock)
{
	char buf[LINE_MAX];
	MessageType type;
	struct epoll_event event;
	int len;
	char * message_content = NULL;
	Message * message = NULL;;

	len = recv (sock, buf, LINE_MAX, 0);

	if (len < 0)
		return 1;

	/* Other side has closed the socket */
	if (len == 0) {
		if (close(sock) == -1)
			return 0;
	}

	/* Parse the received line */
	type = _daemon_parse_line (self, buf, len, &message_content);

	/* Create new return message */
	message = message_new (type, message_content, sock);
	if (message == NULL)
		logger_log (self->_log, CRITICAL, "_daemon_read_socket:message_new");

	/* Add message to message queue */
	if (messagelist_append (self->_mlist, message))	
		logger_log (self->_log, CRITICAL, "_daemon_read_socket:messagelist_append");

	/* Update the epoll event for _sock to be notified 
	 * when it's ready to write */
	event.data.fd = sock;
	event.events = EPOLLOUT;
	if (epoll_ctl (self->_epfd, EPOLL_CTL_MOD, sock, &event) == -1)
		logger_log (self->_log, CRITICAL, "daemon_setup:epoll_ctl");

	return 0;
}

/* 
 * Build list of all processes as string
 * args:   Daemon, pointer to string
 * return: MessageType
 */
static MessageType _daemon_action_list (Daemon * self, char ** message)
{
	Process * p = NULL;
	char * current, * header, * s;
	int len, i;
	size_t slen;

	/* Get the number of Processes in the list */
	len = list_len (self->_pslist);

	if (len == 0)
		return OK;

	/* Allocate a string long enough to fit the process 
	 * string for all processes */
	*message = malloc0 ((STR_MAX_LEN + 1) * (len + 1));	/* STR_MAX_LEN + 1 to fit '\n'
													     * len + 1 to fit headers */
	if (*message == NULL)
		logger_log (self->_log, CRITICAL, "_daemon_action_list:malloc0");

	/* Initialise the current position in the string */
	current = *message;

	/* Add header */
	header = "UID STAT EXIT CMD";
	if (snprintf (current, STR_MAX_LEN, "%s\n", header) == -1)
		logger_log (self->_log, CRITICAL, "_daemon_action_list:snprintf");
	/* Increment the current pointer */
	current += strlen (header) + 1;		/* + 1 for '\n' */

	for (i = 0; i < len; i++)
	{
		/* Get the process i */
		p = pslist_get_ps (self->_pslist, i);
		if (p == NULL)
			logger_log (self->_log, CRITICAL, "_daemon_action_list:pslist_get_ps");

		/* Get the string representation of the process */
		s = process_str (p);
		if (s == NULL)
			logger_log (self->_log, CRITICAL, "_daemon_action_list:process_str");

		slen = strlen (s);

		/* Copy the process string in the return string */
		if (snprintf (current, slen + 2, "%s\n", s) == -1)
			logger_log (self->_log, CRITICAL, "_daemon_action_list:snprintf");

		free (s);

		/* Increment the current pointer */
		current += slen + 1;
	}

	return OK;
}

/* 
 * Build list of all processes as string
 * args:   Daemon, pointer to string
 * return: MessageType
 */
static MessageType _daemon_action_move (Daemon * self, char ** argv, char ** message)
{
	char * src = NULL, * dst = NULL;
	int src_i, dst_i;

	/* Check for the required extra arguments */
	if (*argv != NULL)
	{
		src = *argv;

		/* Move to the next argument */
		argv++;
		dst = *argv;
	}
	if (src == NULL || dst == NULL) {
		logger_log (self->_log, WARNING, "Expected: 'mv UID DEST'");
		*message = strdup ("Expected: 'mv UID DEST'\n");
		return KO;
	}

	/* TODO: check if src contains a range (eg : 2-4) */

	/* Convert the arguments to int */
	errno = 0;
	src_i = strtol (src, NULL, 10);
	if (errno != 0) {
		*message = strdup ("Expected: 'mv UID DEST'\n");
		return KO;
	}
	dst_i = strtol (dst, NULL, 10);
	if (errno != 0) {
		*message = strdup ("Expected: 'mv UID DEST'\n");
		return KO;
	}

	/* Get the index of the process based on its uid */
	src_i = pslist_get_uid_index (self->_pslist, src_i);

	logger_log (self->_log, DEBUG, "Moving processes: %d to %d", src_i, dst_i);

	/* Move the processes */
	if (pslist_move_items (self->_pslist, src_i, 1, dst_i))
		logger_log (self->_log, CRITICAL, "_daemon_action_move:pslist_move_items");

	return OK;
}

/* Signal handlers */
void sigchld_handler (int signum, siginfo_t * siginfo, void * ptr)
{
	extern Daemon * d;
	logger_log (d->_log, DEBUG, "caught SIGCHLD");
	daemon_wait_process (d, siginfo);
	daemon_run_processes (d);
}

void sigterm_handler (int signum)
{
	extern Daemon * d;
	logger_log (d->_log, DEBUG, "caught SIGTERM");
	daemon_stop (d);
}
