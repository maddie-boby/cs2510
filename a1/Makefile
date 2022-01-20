CC := gcc
CFLAGS = -std=gnu11 -g -W -Wall -pthread
INCLUDE = -I.
# LDFLAGS = 


%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

server_objs = server.o \
	jobqueue.o \
	linked_list.o \
	message.o \
	message_net.o

client_objs = client.o \
	message.o \
	message_net.o

all: server client

server: $(server_objs) $(server_libs)
	$(CC) $(CFLAGS) $(INCLUDE) -o server $(server_objs) $(server_libs) $(LDFLAGS)

client: $(client_objs) $(client_libs)
	$(CC) $(CFLAGS) $(INCLUDE) -o client $(client_objs) $(client_libs) $(LDFLAGS)

clean:
	rm *.o server client