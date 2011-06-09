/* 
 * This file is part of mq.
 * mq - src/client.c
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
#include <stdio.h>
#include <limits.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/stat.h>

#include "client.h"
#include "daemon.h"
#include "utils.h"

/* Private methods */
static char * _client_get_next_arg (Client * self);
static int _client_parse_opt (Client * self);
static int _client_daemon_running (Client * self);
static void _client_send_command (Client * self);
static int _client_recv_message (Client * self);

/* 
 * Create and initialise the Cleint
 * args:   void
 * return: Client object or NULL on error
 */
Client * client_new (void)
{
	Client * client = malloc0 (sizeof (Client));
	char * home;

	/* Build the default pidfile's path */
	if ((home = getenv ("HOME")) == NULL)
		return NULL;
	client->_pid_path = msprintf ("%s/%s", home, PID_FILENAME);
	if (client->_pid_path == NULL)
		return NULL;

	/* Build the default socket's path */
	client->_sock_path = msprintf ("%s/%s", home, SOCK_FILENAME);
	if (client->_sock_path == NULL)
		return NULL;

	/* Build the default logfile's path */
	client->_log_path = msprintf ("%s/%s", home, LOG_FILENAME);
	if (client->_log_path == NULL)
		return NULL;

	client->_argc = 0;
	client->_argv = NULL;
	client->_sock = -1;
	client->_ncpus = 0;
	client->_arg_index = 0;

	return client;
}

/* 
 * Parse the given arguments
 * args:   Client, argc, argv
 * return: 0 on sucess, 1 on failure
 */
int client_parse_args (Client * self, int argc, char ** argv)
{
	char * arg;

	self->_argc = argc;
	self->_argv = argv;

	/* We're only interested in finding options (ie: args starting with 
	 * '-' or '--'), if we find anything else we can assume that it should
	 * be sent to the daemon */

	/* Loop until the first non-option argument */
	while ((arg = _client_get_next_arg (self)) != NULL && arg[0] == '-')
	{
		if (_client_parse_opt (self))
			/* TODO: handle opt error */
			return 1;
			;
	}

	return 0;
}

/*
 * Run the client
 * args:   Client
 * return: void
 */
void client_run (Client * self)
{
	extern Daemon * d;
    int len, ret;
    struct sockaddr_un remote;

	/* Check if a daemon is already running, otherwise start one */
	if (_client_daemon_running (self) == 0) 
	{
		printf ("Starting daemon...");
		d = daemon_new (self->_sock_path, self->_pid_path, self->_log_path);
		if (d == NULL) {
			perror ("client_run:daemon_new");
			exit (EXIT_FAILURE);
		}

		/* Run the daemon */
		daemon_run (d);	

		printf (" done\n");
	}


	/* Connect to the socket */
	if ((self->_sock = socket (AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror ("socket");
		exit (EXIT_FAILURE);
	}

    remote.sun_family = AF_UNIX;
    strcpy (remote.sun_path, self->_sock_path);
    len = strlen (remote.sun_path) + sizeof (remote.sun_family);
    if (connect (self->_sock, (struct sockaddr *) &remote, len) == -1) {
        perror ("connect");
        exit (EXIT_FAILURE);
    }

	/* Check if there any args left on the command line */
	if (self->_arg_index > 0 &&
		self->_arg_index < self->_argc &&
		self->_argv[self->_arg_index] != NULL)
	{
		_client_send_command (self);
		ret = _client_recv_message (self);
		close (self->_sock);
		exit (ret);
	}
	else
		printf ("No command provided, try: mq help\n");

	close (self->_sock);

    exit (EXIT_SUCCESS);
}


/* Private methods */

/* 
 * Return the next argument
 * args:   Client, pointer to int to store the index of the argument (can be
 *		   NULL)
 * return: Next argument or NULL 
 */
static char * _client_get_next_arg (Client * self)
{
	self->_arg_index++;	/* Look for the next arg, this means we skip argv[0] */

	if (self->_arg_index >= self->_argc)
		return NULL;

	return self->_argv[self->_arg_index];
}

/*
 * Parse the option at _argv[Client->_arg_index]
 * args:   Client, index of option in Client->_argv
 * return: 0 on success, 1 on error
 */
static int _client_parse_opt (Client * self)
{
	char * arg, * narg;

	/* Check that the given index is within range */
	if (self->_arg_index < 0 || self->_arg_index > self->_argc)
		return 1;

	arg = self->_argv[self->_arg_index];

	if (strcmp (arg, "-s") == 0)		/* -s SOCK_PATH */
	{
		/* Get the next argument */
		narg = _client_get_next_arg (self);

		/* Check that it is an option parameter (ie: doesn't start with '-') */
		if (narg == NULL || narg[0] == '-') {
			printf ("Expected: -s SOCK_PATH\n");
			return 1;
		} else {
			free (self->_sock_path);
			self->_sock_path = narg;
			printf ("Set _sock_path to %s\n", self->_sock_path);
		}
	}
	else if (strcmp (arg, "-p") == 0)	/* -p PIDFILE_PATH */
	{
		/* Get the next argument */
		narg = _client_get_next_arg (self);

		/* Check that it is an option parameter (ie: doesn't start with '-') */
		if (narg == NULL || narg[0] == '-') {
			printf ("Expected: -p PIDFILE_PATH\n");
			return 1;
		} else {
			free (self->_pid_path);
			self->_pid_path = narg;
			printf ("Set _pid_path to %s\n", self->_pid_path);
		}
	}
	else if (strcmp (arg, "-l") == 0)	/* -l LOGFILE_PATH */
	{
		/* Get the next argument */
		narg = _client_get_next_arg (self);

		/* Check that it is an option parameter (ie: doesn't start with '-') */
		if (narg == NULL || narg[0] == '-') {
			printf ("Expected: -l LOGFILE_PATH\n");
			return 1;
		} else {
			free (self->_log_path);
			self->_log_path = narg;
			printf ("Set _log_path to %s\n", self->_log_path);
		}
	}
	else if (strcmp (arg, "-n") == 0)	/* -n NUMBER_CPUS */
	{
		/* Get the next argument */
		narg = _client_get_next_arg (self);

		/* Check that it is an option parameter (ie: doesn't start with '-') */
		if (narg == NULL || narg[0] == '-') {
			printf ("Expected: -n NUMBER_CPUS\n");
			return 1;
		} else {
			self->_ncpus = atol(narg);
			printf ("Set _ncpus to %ld\n", self->_ncpus);
		}
	}

	return 0;
}

/* 
 * Check if the daemon is running
 * args:   Client
 * return: 1 if Daemon is running, else 0
 */
static int _client_daemon_running (Client * self)
{
	FILE * f;
	char buf[LINE_MAX];
	size_t len;
	struct stat s;
	char * proc_path;

	/* Try to open the PID file */
	f = fopen (self->_pid_path, "r");
	if (f == NULL) {
		if (errno == ENOENT)	/* PID file doesn't exist */
			return 0;
		perror ("_client_daemon_running:fopen");
		exit (EXIT_FAILURE);
	}

	/* Read the PID from the PID file */
	len = fread (buf, sizeof (char), LINE_MAX, f);
	if (len == 0) {
		if (feof (f)) {		/* Note: feof doesn't set errno */
			printf ("PID file '%s' is empty!", self->_pid_path);
			exit (EXIT_FAILURE);
		}
		perror ("_client_daemon_running:fread");
		exit (EXIT_FAILURE);
	}
	buf[len - 1] = '\0';	/* - 1 removes the '\n' */

	/* Construct /proc path */
	proc_path = msprintf ("/proc/%s", buf);
	if (proc_path == NULL) {
		perror ("_client_daemon_running:msprintf");
		exit (EXIT_FAILURE);
	}

	/* Check if PID is running (ie: /proc/PID exists) */
	if (stat (proc_path, &s) == -1) {
		if (errno == ENOENT) {
			/* PID file exists but PID is not running */
			printf ("%s exists but Daemon is not running, Please check log file %s\n", self->_pid_path, self->_log_path);

			return 0;
		}
		perror ("_client_daemon_running:stat");
		exit (EXIT_FAILURE);
	}

	return 1;
}

/*
 * Send the command (starting at _argv[_arg_index]) to the * Daemon
 * args:   Client
 * return: void
 */
static void _client_send_command (Client * self)
{
	int i;
	char * arg;

	/* Send each argument separated by '\0' */
	for (i = self->_arg_index; i < self->_argc; i++) {
		arg = self->_argv[i];
        if (send (self->_sock, arg, strlen (arg) + 1, 0) == -1) {
            perror ("_client_send_command:send");
            exit (EXIT_FAILURE);
        }
	}

	/* Send an EOL */
	*arg = '\n';
	if (send (self->_sock, arg, 1, 0) == -1) {
		perror ("_client_send_command:send");
		exit (EXIT_FAILURE);
	}
}

/*
 * Receive a message from the Daemon
 * args:   Client
 * return: EXIT_SUCESS, EXIT_FAILURE
 */
static int _client_recv_message (Client * self)
{
	MessageType type;
	char buf[LINE_MAX];
	int len, fd, ret;

	for (;;) 
	{
		/* Read the line prefix */
		len = recv (self->_sock, &type, sizeof (type), 0);
		if (len == -1) {
			perror ("_client_recv_command:recv");
			exit (EXIT_FAILURE);
		}
		if (len == 0) {
			printf ("Expected MessageType not received\n");
			exit (EXIT_FAILURE);
		}

		if (type == OK || type == KO)
			break;

		/* Read a line from the socket */
		for (len = 0; len < LINE_MAX - 1; len++)	/* LINE_MAX - 1 to squeeze a '\0' */
		{
			ret = recv (self->_sock, &buf[len], sizeof (char), 0);
			if (len == -1) {
				perror ("_client_recv_command:recv");
				exit (EXIT_FAILURE);
			}

			if (buf[len] == '\n')
				break;
		}

		/* Print the line on stdout/err */
		if (type == OUT)
			fd = 1;
		else
			fd = 2;
		write (fd, buf, len + 1);
	}


	if (type == OK)
		return (EXIT_SUCCESS);
	else
		return (EXIT_FAILURE);
}
