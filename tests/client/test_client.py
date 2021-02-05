import asyncio
import subprocess
import pytest
import socket
import multiprocessing
from typing import Generator, Tuple
from pathlib import Path

from tests.e2e_utils.socket_utils import Socket
from tests.e2e_utils.rudp_utils import RudpReceiver, RudpSender

address = "127.0.0.1"
port = 8080
server_bufsize = 1024
kill_server_timeout = 0.5     # in seconds

resources_filepath = Path("tests/resources/")


class Client:
    read_timeout = 1

    expected_prompt_lines = [
        b'Please enter one of the following messages: \n',
        b'\tget <file_name>\n',
        b'\tput <file_name>\n',
        b'\tdelete <file_name>\n',
        b'\tls\n',
        b'\texit\n',
    ]
    prompt_marker = b"> "

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
        for i in range(len(self.expected_prompt_lines)):
            line = await asyncio.wait_for(self.proc.stdout.readline(), timeout=self.read_timeout)
            assert line == self.expected_prompt_lines[i]
        marker = await asyncio.wait_for(self.proc.stdout.readexactly(len(self.prompt_marker)), timeout=self.read_timeout)
        assert marker == self.prompt_marker

    async def check_errors(self):
        while True:
            try:
                err = await asyncio.wait_for(self.proc.stderr.readline(), timeout=self.read_timeout)
                if err == b'':
                    break
                print(f"Error: {err}")
            except asyncio.TimeoutError:
                break

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

    async def read_available_lines(self, timeout=read_timeout, trim_prompt=True):
        result = b''
        # stop looping if the process has exited
        while self.proc.returncode is None:
            try:
                result += await asyncio.wait_for(self.proc.stdout.readline(), timeout=timeout)
            except asyncio.TimeoutError:
                break

        if trim_prompt:
            result = result.removesuffix(b''.join(self.expected_prompt_lines))

        return result


@pytest.fixture
async def client() -> Generator[Client, None, None]:
    c = await Client.create()
    yield c
    await c.close()


class Server(multiprocessing.Process):
    mock_ls_response = b'.git\nfoo\nbar\n'
    mock_exit_response = b'Exiting gracefully\n'

    @staticmethod
    def mock_delete_response(filename: str) -> bytes:
        return f"Deleted {filename}\n".encode()

    def __init__(self, sock: Socket):
        super().__init__()
        self.sock = sock
        self.receiver = RudpReceiver(self.sock)
        self.sender = RudpSender(self.sock, self.receiver)

    def handle_command(self, command: bytes, from_addr: Tuple[str, int]):
        command_type = command.split()[0]
        if command_type == b"get":
            self.handle_get(command, from_addr)
        elif command_type == b"put":
            self.handle_put(command, from_addr)
        elif command_type == b"delete":
            self.handle_delete(command, from_addr)
        elif command_type == b"ls":
            self.handle_ls(from_addr)
        elif command_type == b"exit":
            self.handle_exit(from_addr)

    def handle_get(self, command: bytes, from_addr: Tuple[str, int]):
        filename = command.split()[1].decode()
        filepath = resources_filepath.joinpath(filename)
        with open(filepath, "rb") as f:
            response = f.read()
        self.send_to(response, from_addr)

    def handle_put(self, command: bytes, from_addr: Tuple[str, int]):
        filename = command.split()[1].decode()
        filepath = resources_filepath.joinpath(filename)
        data, addr = self.receive_from()
        assert addr == from_addr
        with open(filepath, "wb") as f:
            f.write(data)

    def handle_delete(self, command: bytes, from_addr: Tuple[str, int]):
        filename = command.split()[1].decode()
        self.send_to(self.mock_delete_response(filename), from_addr)

    def handle_ls(self, from_addr: Tuple[str, int]):
        self.send_to(self.mock_ls_response, from_addr)

    def handle_exit(self, from_addr: Tuple[str, int]):
        self.send_to(self.mock_exit_response, from_addr)

    def send_to(self, message: bytes, addr: Tuple[str, int]):
        self.sender.send_to(message, addr)

    def receive_from(self) -> Tuple[bytes, Tuple[str, int]]:
        return self.receiver.receive_from()

    def run(self):
        while True:
            command, addr = self.receive_from()
            self.handle_command(command, addr)


@pytest.fixture
def server():
    with socket.socket(type=socket.SOCK_DGRAM) as sock:
        sock.bind((address, port))

        serv = Server(Socket(sock))
        serv.start()
        yield
        serv.terminate()
        serv.join(kill_server_timeout)


class TestClient:
    @pytest.mark.xfail(reason="Need to use kftp to send and receive more data than can fit in a single rudp message")
    @pytest.mark.asyncio
    async def test_get(self, client: Client, server: None):
        test_file = "foo1"
        command = f"get {test_file}\n".encode()
        with open(resources_filepath.joinpath(test_file), "rb") as f:
            expected_response = f.read()

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.read_available_lines()
        await client.check_errors()

        assert response.rstrip() == expected_response.rstrip()

    @pytest.mark.xfail(reason="Need to implement put for client")
    @pytest.mark.asyncio
    async def test_put(self, client: Client, server: None):
        input_file = "foo1"
        test_file = f"test_{input_file}"
        command = f"put {test_file}\n".encode()

        with open(resources_filepath.joinpath(input_file), "rb") as f:
            expected_contents = f.read()

        await client.expect_prompt()

        await client.send_input(command)
        await client.check_errors()

        with open(resources_filepath.joinpath(test_file), "rb") as f:
            test_contents = f.read()

        assert test_contents == expected_contents

    @pytest.mark.asyncio
    async def test_delete(self, client: Client, server: None):
        test_file = "test.txt"
        command = f"delete {test_file}\n".encode()
        expected_response = Server.mock_delete_response(test_file)

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.read_available_lines()
        await client.check_errors()

        assert response.rstrip() == expected_response.rstrip()

    @pytest.mark.asyncio
    async def test_ls(self, client: Client, server: None):
        command = b"ls\n"
        expected_response = Server.mock_ls_response

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.read_available_lines()
        await client.check_errors()

        assert response.rstrip() == expected_response.rstrip()

    @pytest.mark.asyncio
    async def test_exit(self, client: Client, server: None):
        command = b"exit\n"
        expected_response = Server.mock_exit_response

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.read_available_lines()
        await client.check_errors()

        assert response.rstrip() == expected_response.rstrip()
