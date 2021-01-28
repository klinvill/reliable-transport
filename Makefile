all: client server

client: src/client/udp_client.c
	mkdir -p out/client
	gcc src/client/udp_client.c -o out/client/client

server: src/server/udp_server.c
	mkdir -p out/server
	gcc src/server/udp_server.c -o out/server/server

.c.o: src/common/utils.c
	mkdir -p out/common/reliable_udp
	gcc -c src/common/utils.c -o out/common/utils.o
	gcc -c src/common/reliable_udp/serde.c -o out/common/reliable_udp/serde.o

test: all unit_tests end_to_end_tests

end_to_end_tests:
	pytest

unit_tests: test_utils test_reliable_udp
	./out/tests/common/test_utils
	./out/tests/common/reliable_udp/test_serde

test_utils: .c.o
	mkdir -p out/tests/common
	gcc -lcheck -o out/tests/common/test_utils tests/common/test_utils.c out/common/utils.o

test_reliable_udp: .c.o
	mkdir -p out/tests/common/reliable_udp
	gcc -lcheck -o out/tests/common/reliable_udp/test_serde tests/common/reliable_udp/test_serde.c out/common/reliable_udp/serde.o

clean:
	rm -r out/*
