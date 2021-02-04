import asyncio
import subprocess
import pytest
import socket
import multiprocessing
import time
from typing import Generator


address = "localhost"
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


class ResponseDetails:
    def __init__(self, lines: int):
        self.lines = lines

@pytest.fixture
def echo_server():
    p = multiprocessing.Process(target=run_echo_server)
    p.start()
    time.sleep(0.2)
    yield
    p.terminate()
    p.join(kill_server_timeout)


@pytest.fixture
def big_response_server() -> Generator[ResponseDetails, None, None]:
    with open(big_response_file, "rb") as f:
        lines = len(f.readlines())

    p = multiprocessing.Process(target=run_big_response_server)
    p.start()
    time.sleep(0.2)
    yield ResponseDetails(lines)
    p.terminate()
    p.join(kill_server_timeout)


def run_echo_server():
    with open("test_log.txt", "w") as f:
        f.write("Opening socket...\n")
        f.flush()
        with socket.socket(type=socket.SOCK_DGRAM) as sock:
            f.write("Binding socket...\n")
            f.flush()
            sock.bind((address, port))
            f.write("Receiving data...\n")
            f.flush()
            while True:
                (data, sender) = sock.recvfrom(server_bufsize)
                f.write(f"Received: {data}\n")
                f.flush()
                sock.sendto(data, sender)
                f.write(f"Sent: {data}\n")
                f.flush()


def run_big_response_server():
    with open(big_response_file, "rb") as f:
        response = f.read()

    chunk_size = 1024

    with socket.socket(type=socket.SOCK_DGRAM) as sock:
        sock.bind((address, port))
        while True:
            (data, sender) = sock.recvfrom(server_bufsize)

            for i in range(int((len(response) - 1) / chunk_size + 1)):
                sock.sendto(response[i * chunk_size:(i+1) * chunk_size], sender)


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

