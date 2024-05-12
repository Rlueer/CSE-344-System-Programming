all: server client

server: server_side/server.c
	@gcc $^ -o server_side/server.out -lrt

client: client_side/client.c
	@gcc $^ -o client_side/client.out -lrt
