#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <limits.h>

#include "daemon.h"
#include "logger.h"

#define MAX_EVENTS	64
#define TIMEOUT		1000

Daemon * daemon_new (void)
{
	Daemon * daemon = malloc (sizeof (Daemon));
	if (!daemon) {
		perror ("daemon_new:malloc");
		exit (EXIT_FAILURE);
	}

	daemon->pipe = -1;
	daemon->pipe_path = PIPE_PATH;
//	daemon->log = logger_new (NULL);
	daemon->log = logger_new ("/tmp/mq.log");

	return daemon;
}

void daemon_setup (Daemon * self)
{
	struct epoll_event event;

	/* TODO: check if pipe exists and create it if necessary */

	/* Open the pipe: */
	self->pipe = open (self->pipe_path, O_RDWR);
	if (self->pipe == -1) {
		perror ("daemon_setup:open");
		exit (EXIT_FAILURE);
	}

	/* Create the epoll fd: */
	self->epfd = epoll_create (5);
	if (self->epfd < 0) {
		perror ("epoll_create");
		exit (EXIT_FAILURE);
	}

	/* We want to be notified when there is data to read: */
	event.data.fd = self->pipe;
	event.events = EPOLLIN;
	if (epoll_ctl (self->epfd, EPOLL_CTL_ADD, self->pipe, &event) == -1) {
		perror ("epoll_ctl");
		exit (EXIT_FAILURE);
	}
}

void daemon_run (Daemon * self)
{
	struct epoll_event * events;
	char buf[LINE_MAX], * s;
	int i, nr_events, len;
	 
	logger_info(self->log, "In 'daemon_run'\n");

	events = malloc (sizeof (struct epoll_event) * MAX_EVENTS);
	if (!events) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}

	/* We read one char at a time, stored in 'buf' at position 's',
	 * when a '\n' is read we process the line and reset 's' */
	s = buf;
	while (1) {
		nr_events = epoll_wait (self->epfd, events, MAX_EVENTS, TIMEOUT);
		if (nr_events < 0) {
			perror ("epoll_wait");
			exit (EXIT_FAILURE);
		}

		if (nr_events == 0)
			/* Waited for TIMEOUT ms */
			;

		for (i = 0; i < nr_events ; i++)
		{
			if (events[i].events == EPOLLIN) {
				len = read (events[i].data.fd, s, sizeof(char));
				if (len == -1) { 
					perror("read");
					exit(EXIT_FAILURE);
				} 
				if (len) {
					if (*s == '\n') {
						*s = '\0';
						logger_info(self->log, "Read line: %s\n", buf);
						s = buf;
					} else s++;
				}
			}
		}
	}
	return ;
}
