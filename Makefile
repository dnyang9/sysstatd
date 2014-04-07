SION = 1

CC = gcc
CFLAGS = -Wall -Werror -pthread

SHARED_OBJS = csapp.o json.o rio.o
OBJS = $(SHARED_OBJS) server.o

csapp.o: csapp.c csapp.h
server.o: server.c server.h
json.o: json.c json.h
rio.o: rio.c rio.h

all: $(OBJS)
	$(CC) $(CFLAGS) -o sysstatd $(OBJS)

clean:
	rm -f *- *.o sysstatd


