CC = gcc
CFLAGS = -Wall -Iinclude -I/usr/include/libxml2 -I/home/alex/cJSON
LIBS = -lxml2 -lcjson -lpthread

SRC = src/main.c src/admin_client.c src/simple_client.c src/remote_client.c src/metadata.c src/server.c src/threadpool.c
OBJ = $(SRC:.c=.o)

all: server

server: $(OBJ)
	$(CC) -o server $(OBJ) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) server
