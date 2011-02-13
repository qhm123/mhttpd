CC =		gcc
CFLAGS =	-O
LDFLAGS =	-s

all:		mhttpd

tmhttpd:	mhttpd.o
	${CC} ${CFLAGS} mhttpd.o ${LDFLAGS} -o mhttpd

tmhttpd.o:	mhttpd.c
	${CC} ${CFLAGS} -c mhttpd.c

clean:
	rm -f mhttpd *.o core core.* *.core