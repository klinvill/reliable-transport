import asyncio
import subprocess
import pytest
import socket
import multiprocessing
from typing import Generator, Tuple
from pathlib import Path

from tests.e2e_utils.socket_utils import Socket
from tests.e2e_utils.rudp_utils import RudpReceiver, RudpSender
from tests.e2e_utils.kftp_utils import KftpReceiver, KftpSender

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


def filepath_to_test_filepath(filepath: Path) -> Path:
    return filepath.parent.joinpath(f"test_{filepath.name}")


class Server(multiprocessing.Process):
    mock_ls_response = b'.git\nfoo\nbar\n'
    mock_exit_response = b'Exiting gracefully\n'
    mock_file_contents = b"Hello world!\nGoodbye...\n"
    sample_files = ["foo1", "foo2", "foo3"]

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
            self.handle_put(command)
        elif command_type == b"delete":
            self.handle_delete(command, from_addr)
        elif command_type == b"ls":
            self.handle_ls(from_addr)
        elif command_type == b"exit":
            self.handle_exit(from_addr)

    def handle_get(self, command: bytes, from_addr: Tuple[str, int]):
        contents = self.mock_file_contents
        filename = command.split()[1].decode()
        for file in self.sample_files:
            if filename == str(resources_filepath.joinpath(f"test_{file}")):
                with open(resources_filepath.joinpath(file), "rb") as f:
                    contents = f.read()
                break

        return KftpSender(self.sender).send_to(contents, from_addr)

    def handle_put(self, command: bytes):
        filename = command.split()[1].decode()
        test_filepath = filepath_to_test_filepath(Path(filename))
        data, _ = KftpReceiver(self.receiver).receive_from()
        with test_filepath.open("wb") as f:
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
    @pytest.mark.asyncio
    async def test_get(self, client: Client, server: None):
        test_file = "test.txt"
        test_filepath = resources_filepath.joinpath(test_file)
        command = f"get {test_filepath}\n".encode()
        expected_contents = Server.mock_file_contents
        expected_response = f"Downloaded file: {test_filepath}\n".encode()

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.read_available_lines()
        await client.check_errors()

        with open(test_filepath, "rb") as f:
            contents = f.read()

        assert response.rstrip() == expected_response.rstrip()
        assert contents == expected_contents

        test_filepath.unlink()

    @pytest.mark.asyncio
    @pytest.mark.parametrize("file", Server.sample_files)
    async def test_get_sample_files(self, client: Client, server: None, file: str):
        """Tests the sample files provided for this homework assignment"""
        filepath = resources_filepath.joinpath(file)
        test_file = f"test_{file}"
        test_filepath = resources_filepath.joinpath(test_file)
        command = f"get {test_filepath}\n".encode()

        with open(filepath, "rb") as f:
            expected_contents = f.read()
        expected_response = f"Downloaded file: {test_filepath}\n".encode()

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.read_available_lines()
        await client.check_errors()

        with open(test_filepath, "rb") as f:
            contents = f.read()

        assert response.rstrip() == expected_response.rstrip()
        assert contents == expected_contents

        test_filepath.unlink()

    @pytest.mark.asyncio
    async def test_put(self, client: Client, server: None):
        input_file = "foo1"
        input_filepath = resources_filepath.joinpath(input_file)
        test_filepath = filepath_to_test_filepath(input_filepath)
        command = f"put {input_filepath}\n".encode()

        with open(input_filepath, "rb") as f:
            expected_contents = f.read()
        expected_response = f"Sent file: {input_filepath}\n".encode()

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.read_available_lines()
        await client.check_errors()

        with open(test_filepath, "rb") as f:
            test_contents = f.read()

        assert response.rstrip() == expected_response.rstrip()
        assert test_contents == expected_contents

        test_filepath.unlink()

    @pytest.mark.asyncio
    @pytest.mark.parametrize("file", Server.sample_files)
    async def test_put_sample_files(self, client: Client, server: None, file: str):
        input_filepath = resources_filepath.joinpath(file)
        test_filepath = filepath_to_test_filepath(input_filepath)
        command = f"put {input_filepath}\n".encode()

        with open(input_filepath, "rb") as f:
            expected_contents = f.read()
        expected_response = f"Sent file: {input_filepath}\n".encode()

        await client.expect_prompt()

        await client.send_input(command)
        response = await client.read_available_lines()
        await client.check_errors()

        with open(test_filepath, "rb") as f:
            test_contents = f.read()

        assert response.rstrip() == expected_response.rstrip()
        assert test_contents == expected_contents

        test_filepath.unlink()

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

        # The exit message should cause the client to exit as well
        assert client.proc.returncode == 0
        assert response.rstrip() == expected_response.rstrip()
