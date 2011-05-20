#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define MAX_EVENTS	64

int main (void)
{
	int epfd, fd, nr_events, i, len;
	struct epoll_event event, *events;
	char buf[LINE_MAX];

	fd = open ("p", O_RDWR);
	if (fd == -1) {
		perror ("open");
		exit (EXIT_FAILURE);
	}

	epfd = epoll_create (5);
	if (epfd < 0) {
		perror ("epoll_create");
		exit (EXIT_FAILURE);
	}

	event.data.fd = fd;
	event.events = EPOLLIN;
	if (epoll_ctl (epfd, EPOLL_CTL_ADD, fd, &event) == -1) {
		perror ("epoll_ctl");
		exit (EXIT_FAILURE);
	}

	events = malloc (sizeof (struct epoll_event) * MAX_EVENTS);
	if (!events) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}

	while (1) {
		nr_events = epoll_wait (epfd, events, MAX_EVENTS, 1000);
		if (nr_events < 0) {
			perror ("epoll_wait");
			exit (EXIT_FAILURE);
		}

		if (nr_events == 0)
			printf("Waited for 1 sec...\n");

		for (i = 0; i < nr_events ; i++)
		{
			if (events[i].events == EPOLLIN) {
				len = read (fd, buf, LINE_MAX);
				if (len == -1) { perror("read");
					exit(EXIT_FAILURE);
				} else if (len) {
					buf[len] = '\0';
					printf("%d:%d:%s", LINE_MAX, (int) strlen (buf), buf);
				}
			}
		}
	}

	if (close (fd)) {
		perror ("close");
		exit (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}
