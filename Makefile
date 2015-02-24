
CC = gcc
LIBS= -lresolv -lsocket -lnsl -lpthread\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\

FLAGS = -g -O2
CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib
PROGS = server client 

all: ${PROGS}

server : server.o get_ifi_info_plus.o get_subnetaddr.o file_queue.o buffer_queue.o
	${CC} ${CFLAGS} -o $@ server.o get_ifi_info_plus.o get_subnetaddr.o file_queue.o buffer_queue.o ${LIBS}
client: client.o get_ifi_info_plus.o get_subnetaddr.o buffer_queue.o
	${CC} ${CFLAGS} -o $@ client.o get_ifi_info_plus.o get_subnetaddr.o file_queue.o buffer_queue.o ${LIBS}
get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c ${LIBS}
get_subnetaddr.o: get_subnetaddr.c
	${CC} ${CFLAGS} -c get_subnetaddr.c 
file_queue.o: file_queue.c
	${CC} ${CFLAGS} -c file_queue.c
buffer_queue.o: buffer_queue.c
	${CC} ${CFGLAG} -c buffer_queue.c 



