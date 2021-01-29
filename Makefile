all: client server

client: src/client/udp_client.c
	mkdir -p out/client
	gcc src/client/udp_client.c -o out/client/client

server: src/server/udp_server.c
	mkdir -p out/server
	gcc src/server/udp_server.c -o out/server/server

.c.o: src/common/utils.c src/common/reliable_udp/serde.c src/common/reliable_udp/reliable_udp.c
	mkdir -p out/common/reliable_udp
	gcc -c src/common/utils.c -o out/common/utils.o
	gcc -c src/common/reliable_udp/serde.c -o out/common/reliable_udp/serde.o
	gcc -c src/common/reliable_udp/reliable_udp.c -o out/common/reliable_udp/reliable_udp.o

test: all unit_tests end_to_end_tests

end_to_end_tests:
	pytest

unit_tests: test_utils test_reliable_udp
	./out/tests/common/test_utils
	./out/tests/common/reliable_udp/test_serde
	DYLD_INSERT_LIBRARIES=./out/tests/mocks/mocks.dylib DYLD_FORCE_FLAT_NAMESPACE=1 ./out/tests/common/reliable_udp/test_reliable_udp

test_utils: .c.o
	mkdir -p out/tests/common
	gcc -lcheck -o out/tests/common/test_utils tests/common/test_utils.c out/common/utils.o

test_reliable_udp: .c.o mocks
	mkdir -p out/tests/common/reliable_udp
	gcc -lcheck -o out/tests/common/reliable_udp/test_serde tests/common/reliable_udp/test_serde.c out/common/reliable_udp/serde.o
	gcc -lcmocka -o out/tests/common/reliable_udp/test_reliable_udp tests/common/reliable_udp/test_reliable_udp.c out/common/reliable_udp/reliable_udp.o out/common/reliable_udp/serde.o out/common/utils.o out/tests/mocks/mocks.dylib

mocks: tests/mocks/mocks.c
	mkdir -p out/tests/mocks
	gcc -shared -fPIC -lcmocka tests/mocks/mocks.c -o out/tests/mocks/mocks.dylib

clean:
	rm -r out/*
