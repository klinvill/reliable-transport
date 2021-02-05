all: client server

client: src/client/udp_client.c .c.o
	mkdir -p out/client
	gcc src/client/udp_client.c -o out/client/client out/common/reliable_udp/reliable_udp.o out/common/reliable_udp/serde.o out/common/utils.o out/common/kftp/kftp.o out/common/kftp/kftp_serde.o

server: src/server/udp_server.c .c.o
	mkdir -p out/server
	gcc src/server/udp_server.c -o out/server/server out/common/reliable_udp/reliable_udp.o out/common/reliable_udp/serde.o out/common/utils.o out/common/kftp/kftp.o out/common/kftp/kftp_serde.o

.c.o: src/common/utils.c src/common/reliable_udp/serde.c src/common/reliable_udp/reliable_udp.c src/common/kftp/kftp.c
	mkdir -p out/common/reliable_udp out/common/kftp
	gcc -c src/common/utils.c -o out/common/utils.o
	gcc -c src/common/reliable_udp/serde.c -o out/common/reliable_udp/serde.o
	gcc -c src/common/reliable_udp/reliable_udp.c -o out/common/reliable_udp/reliable_udp.o
	gcc -c src/common/kftp/kftp_serde.c -o out/common/kftp/kftp_serde.o
	gcc -c src/common/kftp/kftp.c -o out/common/kftp/kftp.o

test: all unit_tests end_to_end_tests

end_to_end_tests:
	pytest

unit_tests: test_utils test_reliable_udp test_kftp
	./out/tests/common/test_utils
	./out/tests/common/reliable_udp/test_serde
	DYLD_INSERT_LIBRARIES=./out/tests/mocks/mocks.dylib DYLD_FORCE_FLAT_NAMESPACE=1 lldb ./out/tests/common/reliable_udp/test_reliable_udp -o run -o quit
	DYLD_INSERT_LIBRARIES=./out/tests/mocks/reliable_udp_mocks.dylib:./out/tests/mocks/mocks.dylib DYLD_FORCE_FLAT_NAMESPACE=1 lldb ./out/tests/common/kftp/test_kftp -o run -o quit

test_utils: .c.o
	mkdir -p out/tests/common
	gcc -lcheck -o out/tests/common/test_utils tests/common/test_utils.c out/common/utils.o

test_reliable_udp: .c.o mocks
	mkdir -p out/tests/common/reliable_udp
	gcc -lcheck -o out/tests/common/reliable_udp/test_serde tests/common/reliable_udp/test_serde.c out/common/reliable_udp/serde.o
	gcc -lcmocka -o out/tests/common/reliable_udp/test_reliable_udp tests/common/reliable_udp/test_reliable_udp.c out/common/reliable_udp/reliable_udp.o out/common/reliable_udp/serde.o out/common/utils.o out/tests/mocks/mocks.dylib

test_kftp: .c.o mocks
	mkdir -p out/tests/common/kftp
	gcc -lcmocka -o out/tests/common/kftp/test_kftp tests/common/kftp/test_kftp.c out/common/kftp/kftp.o out/common/kftp/kftp_serde.o out/common/reliable_udp/serde.o out/common/utils.o out/tests/mocks/mocks.dylib out/tests/mocks/reliable_udp_mocks.dylib

mocks: tests/mocks/mocks.c tests/mocks/reliable_udp_mocks.c
	mkdir -p out/tests/mocks
	gcc -shared -fPIC -lcmocka tests/mocks/mocks.c -o out/tests/mocks/mocks.dylib
	gcc -shared -fPIC -lcmocka tests/mocks/reliable_udp_mocks.c -o out/tests/mocks/reliable_udp_mocks.dylib

clean:
	rm -r out/*
