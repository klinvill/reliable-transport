all: client server

client: udp_client.c
	gcc udp_client.c -o out/client

server: udp_server.c
	gcc udp_server.c -o out/server

clean:
	rm out/client out/server
