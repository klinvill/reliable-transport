all: client server

client: src/client/udp_client.c
	mkdir -p out/client
	gcc src/client/udp_client.c -o out/client/client

server: src/server/udp_server.c
	mkdir -p out/server
	gcc src/server/udp_server.c -o out/server/server

.c.o: src/common/utils.c
	mkdir -p out/common
	gcc -c src/common/utils.c -o out/common/utils.o

test: all
	pytest

unit_tests: test_utils
	./out/tests/common/test_utils

test_utils: .c.o
	mkdir -p out/tests/common
	gcc -lcheck -o out/tests/common/test_utils tests/common/test_utils.c out/common/utils.o

clean:
	rm -r out/*
