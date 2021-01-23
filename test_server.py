import pytest
import subprocess
import socket
import time
from typing import Generator
from pathlib import Path

address = "127.0.0.1"
port = 8080

class Client:
    BUFSIZE = 1024
    SOCKET_TIMEOUT = 0.1    # in seconds

    def __init__(self, sock: socket.socket):
        sock.settimeout(self.SOCKET_TIMEOUT)
        self.sock = sock

    def get(self, filename: str) -> bytes:
        return self.send_and_receive(f"get {filename}".encode())

    def put(self, filename: str) -> bytes:
        return self.send_and_receive(f"put {filename}".encode())

    def delete(self, filename: str) -> bytes:
        return self.send_and_receive(f"delete {filename}".encode())

    def ls(self) -> bytes:
        return self.send_and_receive(b"ls")

    def exit(self) -> bytes:
        return self.send_and_receive(b"exit")

    def send(self, data: bytes):
        self.sock.sendto(data, (address, port))

    def receive(self) -> bytes:
        full_data = b''
        while True:
            try:
                (data, addr) = self.sock.recvfrom(self.BUFSIZE)
            except socket.timeout:
                break
            if addr == (address, port):
                full_data += data

        return full_data

    def send_and_receive(self, data: bytes) -> bytes:
        self.send(data)
        return self.receive()


@pytest.fixture
def client() -> Client:
    with socket.socket(type=socket.SOCK_DGRAM) as sock:
        yield Client(sock)


@pytest.fixture(scope="class")
def server() -> Generator[subprocess.Popen, None, None]:
    yield from run_server()


# this server is ok to kill since it will be re-created before each test function
@pytest.fixture
def killable_server() -> Generator[subprocess.Popen, None, None]:
    yield from run_server()


def run_server() -> Generator[subprocess.Popen, None, None]:
    """
    yields the port of the run server
    """
    with subprocess.Popen(["./out/server", str(port)]) as proc:
        # TODO: should have the server print a message when listening, and then yield after that instead of sleeping
        #  1 second
        time.sleep(1)
        yield proc
        proc.kill()

class TestResponses:
    not_implemented_message_format = "Command not yet implemented: {command}"
    invalid_command_message_format = "Invalid command: {command}"


@pytest.mark.usefixtures("server")
class TestServerNonExiting(TestResponses):
    def test_get(self, client: Client):
        filename = "foo1"
        response = client.get(filename)

        with open(filename, "rb") as f:
            file_contents = f.read()
        expected_response = file_contents
        assert response == expected_response

    def test_put_not_implemented(self, client: Client):
        filename = "test.txt"
        response = client.put(filename)
        expected_response = self.not_implemented_message_format.format(command=f"put {filename}").encode()
        assert response == expected_response

    def test_delete_not_implemented(self, client: Client):
        filename = "test.txt"
        response = client.delete(filename)
        expected_response = self.not_implemented_message_format.format(command=f"delete {filename}").encode()
        assert response == expected_response

    def test_ls(self, client: Client):
        response = client.ls()
        response_files = response.strip().split(b"\n")
        local_files = [f.name.encode() for f in Path('.').iterdir() if f.is_file()]
        assert sorted(response_files) == sorted(local_files)

    def test_invalid_commands_sent_back(self, client: Client):
        command = "foo bar"
        response = client.send_and_receive(command.encode())
        expected_response = self.invalid_command_message_format.format(command=command).encode()
        assert response == expected_response

    @pytest.mark.parametrize("command", ["get", "put", "delete"])
    def test_must_pass_one_argument(self, command: str, client: Client):
        files = "test.txt foo.bar"
        multiple_args_command = f"{command} {files}"
        multi_response = client.send_and_receive(multiple_args_command.encode())
        expected_multi_response = self.invalid_command_message_format.format(command=multiple_args_command).encode()
        assert multi_response == expected_multi_response

        no_arg_response = client.send_and_receive(command.encode())
        no_arg_expected_response = self.invalid_command_message_format.format(command=command).encode()
        assert no_arg_response == no_arg_expected_response

    @pytest.mark.parametrize("command", ["ls", "exit"])
    def test_must_pass_no_arguments(self, command: str, client: Client):
        arg = "foo"
        full_command = f"{command} {arg}"
        response = client.send_and_receive(full_command.encode())
        expected_response = self.invalid_command_message_format.format(command=full_command).encode()
        assert response == expected_response

    def test_messages_ignore_newlines(self, client: Client):
        command = "ls\n"
        response = client.send_and_receive(command.encode())
        response_files = response.strip().split(b"\n")
        local_files = [f.name.encode() for f in Path('.').iterdir() if f.is_file()]
        assert sorted(response_files) == sorted(local_files)


class TestServerExiting(TestResponses):
    def test_exit(self, client: Client, killable_server: subprocess.Popen):
        response = client.exit()
        expected_response = b"Exiting gracefully"
        assert response == expected_response

        # server should exit gracefully
        killable_server.wait(1)
        assert killable_server.returncode == 0
