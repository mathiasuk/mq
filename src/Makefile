CC=gcc -Werror -Wall -Wextra -Wno-unused-parameter -pedantic -g
SOURCES = client.c daemon.c logger.c process.c list.c pslist.c message.c messagelist.c
OBJS    = ${SOURCES:.c=.o}

mq: main.c ${OBJS}
	$(CC) -o mq main.c ${OBJS}

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	rm -f mq *.o client
