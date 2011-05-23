CC=gcc -Wall -g -lefence
SOURCES = daemon.c logger.c
OBJS    = ${SOURCES:.c=.o}

mq: main.c ${OBJS}
	$(CC) -o mq main.c ${OBJS}

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	rm -f mq *.o

