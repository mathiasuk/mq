CC=gcc -Wall -g
SOURCES = client.c daemon.c logger.c process.c pslist.c
OBJS    = ${SOURCES:.c=.o}

mq: main.c ${OBJS}
	$(CC) -o mq main.c ${OBJS}

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	rm -f mq *.o client
