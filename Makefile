all: client server

client: udp_client.c
	gcc udp_client.c -o out/client

server: udp_server.c
	gcc udp_server.c -o out/server

.c.o: utils.c
	gcc -c utils.c -o out/utils.o

test: all
	pytest

unit_tests: test_utils
	./out/test_utils

test_utils: .c.o
	gcc -lcheck -o out/test_utils test_utils.c out/utils.o

clean:
	rm -r out/*
