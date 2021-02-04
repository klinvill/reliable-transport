import asyncio
import subprocess
import pytest
import socket
import multiprocessing
import time
from typing import Generator, Tuple

from tests.e2e_utils.socket_utils import Socket
from tests.e2e_utils.rudp_utils import RudpReceiver, RudpSender

address = "127.0.0.1"
port = 8080
server_bufsize = 1024
kill_server_timeout = 0.5     # in seconds

big_response_file = "./tests/resources/foo1"


class Client:
    read_timeout = 1

    def __init__(self, proc: asyncio.subprocess.Process):
        self.proc = proc

    @classmethod
    async def create(cls) -> "Client":
        proc = await asyncio.create_subprocess_exec("./out/client/client", address, str(port),
                                                    stdin=subprocess.PIPE,
                                                    stdout=subprocess.PIPE,
                                                    stderr=subprocess.PIPE)
        return Client(proc)

    async def expect_prompt(self):
        expected_prompt_lines = [
            b'Please enter one of the following messages: \n',
            b'\tget <file_name>\n',
            b'\tput <file_name>\n',
            b'\tdelete <file_name>\n',
            b'\tls\n',
            b'\texit\n',
        ]
        prompt_marker = b"> "

        for i in range(len(expected_prompt_lines)):
            line = await asyncio.wait_for(self.proc.stdout.readline(), timeout=self.read_timeout)
            assert line == expected_prompt_lines[i]
        marker = await asyncio.wait_for(self.proc.stdout.readexactly(len(prompt_marker)), timeout=self.read_timeout)
        assert marker == prompt_marker

    async def check_errors(self):
        try:
            err = await asyncio.wait_for(self.proc.stderr.readline(), timeout=self.read_timeout)
            print(f"Errors: {err}")
        except asyncio.TimeoutError:
            pass

    async def close(self):
        if self.proc.returncode is None:
            self.proc.kill()
            await asyncio.wait_for(self.proc.wait(), timeout=self.read_timeout)
        self.proc = None

    async def send_input(self, message: bytes):
        self.proc.stdin.write(message)
        await self.proc.stdin.drain()

    async def readline(self) -> bytes:
        return await asyncio.wait_for(self.proc.stdout.readline(), timeout=self.read_timeout)

    async def readlines(self, count: int) -> bytes:
        result = b''
        for i in range(count):
            result += await asyncio.wait_for(self.proc.stdout.readline(), timeout=self.read_timeout)
        return result


@pytest.fixture
async def client() -> Generator[Client, None, None]:
    c = await Client.create()
    yield c
    await c.close()


class Server:
    def __init__(self, sock: Socket):
        self.sock = sock
        self.receiver = RudpReceiver(self.sock)
        self.sender = RudpSender(self.sock, self.receiver)

    def send_to(self, message: bytes, addr: Tuple[str, int]):
        self.sender.send_to(message, addr)

    def receive_from(self) -> Tuple[bytes, Tuple[str, int]]:
        return self.receiver.receive_from()


class EchoServer(Server, multiprocessing.Process):
    def __init__(self, sock: Socket):
        Server.__init__(self, sock)
        multiprocessing.Process.__init__(self)

    def run(self):
        with open("test_log.txt", "w") as f:
            f.write("Starting server...\n")
            while True:
                data, addr = self.receive_from()
                f.write(f"Received: {data}\n")
                f.flush()
                self.send_to(data, addr)
                f.write(f"Sent: {data}\n")
                f.flush()


class BigResponseServer(Server, multiprocessing.Process):
    chunk_size = 1024

    def __init__(self, sock: Socket):
        Server.__init__(self, sock)
        multiprocessing.Process.__init__(self)

        with open(big_response_file, "rb") as f:
            self.response = f.read()

        self.lines = self.response.count(b'\n')

    def run(self):
        while True:
            data, sender = self.receive_from()
            for i in range(int((len(self.response) - 1) / self.chunk_size + 1)):
                self.send_to(self.response[i * self.chunk_size:(i+1) * self.chunk_size], sender)


class ResponseDetails:
    def __init__(self, lines: int):
        self.lines = lines


@pytest.fixture
def echo_server():
    with socket.socket(type=socket.SOCK_DGRAM) as sock:
        sock.bind((address, port))

        server = EchoServer(Socket(sock))
        server.start()
        time.sleep(0.2)
        yield
        server.terminate()
        server.join(kill_server_timeout)


@pytest.fixture
def big_response_server() -> Generator[ResponseDetails, None, None]:
    with socket.socket(type=socket.SOCK_DGRAM) as sock:
        sock.bind((address, port))

        server = BigResponseServer(Socket(sock))
        server.start()
        time.sleep(0.2)
        yield ResponseDetails(server.lines)
        server.terminate()
        server.join(kill_server_timeout)


class TestClient:
    @pytest.mark.asyncio
    async def test_client_retrieves_response(self, client: Client, echo_server: None):
        command = b"foo\n"
        expected_response = command

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.readline()
        await client.check_errors()

        assert response == expected_response

    @pytest.mark.xfail(reason="Should make sure client and server communication across multiple messages is done through kftp")
    @pytest.mark.asyncio
    async def test_client_retrieves_large_response(self, client: Client, big_response_server: ResponseDetails):
        command = b"foo\n"
        with open(big_response_file, "rb") as f:
            expected_response = f.read()

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.readlines(big_response_server.lines)
        await client.check_errors()

        assert response == expected_response
