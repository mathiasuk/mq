CC=gcc -Wall -g -lefence
#CC=gcc -Wall -g
SOURCES = mq_utils.c mq_daemon.c
OBJS    = ${SOURCES:.c=.o}

mq: main.c ${OBJS}
	$(CC) -o mq main.c ${OBJS}

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	rm -f mq *.o

