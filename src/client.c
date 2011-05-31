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

#include "client.h"
#include "daemon.h"

/* 
 * Create and initialise the Cleint
 * args:   void
 * return: Client object or NULL on error
 */
Client * client_new (void)
{
	Client * client = malloc (sizeof (Client));
	char * home;

	/* Build the default pidfile's path */
	if ((home = getenv ("HOME")) == NULL)
		return NULL;
	client->_pid_path = malloc (snprintf (NULL, 0, "%s/%s", home, 
										  PID_FILENAME) + 1);
	if (client->_pid_path == NULL)
		return NULL;
	sprintf (client->_pid_path, "%s/%s", home, PID_FILENAME);

	/* Build the default socket's path */
	if ((home = getenv ("HOME")) == NULL)
		return NULL;
	client->_sock_path = malloc (snprintf (NULL, 0, "%s/%s", home, 
										   SOCK_FILENAME) + 1);
	if (client->_sock_path == NULL)
		return NULL;
	sprintf (client->_sock_path, "%s/%s", home, SOCK_FILENAME);

	/* Build the default logfile's path */
	if ((home = getenv ("HOME")) == NULL)
		return NULL;
	client->_log_path = malloc (snprintf (NULL, 0, "%s/%s", home, 
										  LOG_FILENAME) + 1);
	if (client->_log_path == NULL)
		return NULL;
	sprintf (client->_log_path, "%s/%s", home, LOG_FILENAME);

	/* TODO: parse arguments */

	return client;
}

/*
 * Run the client
 * args:   Client
 * return: void
 */
void client_run (Client * self)
{
	extern Daemon * d;
    int sock, len;
    struct sockaddr_un remote;
	char buf[LINE_MAX];


	/* TODO: check if the daemon is already running otherwise start it */
	d = daemon_new (self->_sock_path, self->_pid_path, self->_log_path);
	if (d == NULL) {
		perror ("client_run:daemon_new");
		exit (EXIT_FAILURE);
	}

	if (daemon_setup (d) == 0) {
		/* In daemon*/
		daemon_run (d);	
		exit (EXIT_FAILURE);	/* We should never reach this */
	}

    if ((sock = socket (AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror ("socket");
        exit(EXIT_FAILURE);
    }

    printf ("Trying to connect...\n");

    remote.sun_family = AF_UNIX;
    strcpy (remote.sun_path, self->_sock_path);
    len = strlen (remote.sun_path) + sizeof (remote.sun_family);
    if (connect (sock, (struct sockaddr *) &remote, len) == -1) {
        perror("connect");
        exit(1);
    }

    printf ("Connected.\n");

    while (printf("> "), fgets (buf, 100, stdin), !feof(stdin)) {
        if (send (sock, buf, strlen (buf), 0) == -1) {
            perror ("send");
            exit (EXIT_FAILURE);
        }

		/* if ((t=recv(sock, str, 100, 0)) > 0) { */
			/* str[t] = '\0'; */
			/* printf("echo> %s", str); */
		/* } else { */
			/* if (t < 0) perror("recv"); */
			/* else printf("Server closed connection\n"); */
			/* exit(1); */
		/* } */
    }

    close (sock);

    exit (EXIT_SUCCESS);
}
