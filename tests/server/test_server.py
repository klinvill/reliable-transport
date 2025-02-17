import pytest
import subprocess
import socket
import time
from typing import Generator
from pathlib import Path

from tests.e2e_utils.socket_utils import Socket, UnreliableSocket
from tests.e2e_utils.rudp_utils import RudpReceiver, RudpSender
from tests.e2e_utils.kftp_utils import KftpReceiver, KftpSender

address = "127.0.0.1"
port = 8080
resources_filepath = Path("tests/resources/")


class Client:
    SOCKET_TIMEOUT = 0.5    # in seconds

    def __init__(self, sock: Socket):
        sock.sock.settimeout(self.SOCKET_TIMEOUT)
        self.sock = sock
        self.receiver = RudpReceiver(self.sock)
        self.sender = RudpSender(self.sock, self.receiver)

    def get(self, filename: str) -> bytes:
        self.send(f"get {filename}".encode())
        data, _ = KftpReceiver(self.receiver).receive_from()
        return data

    def put(self, filename: str, data: bytes):
        self.send(f"put {filename}".encode())
        return KftpSender(self.sender).send_to(data, (address, port))

    def delete(self, filename: str) -> bytes:
        return self.send_and_receive(f"delete {filename}".encode())

    def ls(self) -> bytes:
        return self.send_and_receive(b"ls")

    def exit(self) -> bytes:
        return self.send_and_receive(b"exit")

    def send(self, data: bytes):
        self.sender.send_to(data, (address, port))

    def receive(self) -> bytes:
        data, _ = self.receiver.receive_from()
        return data

    def send_and_receive(self, data: bytes) -> bytes:
        self.send(data)
        return self.receive()


@pytest.fixture(params=["reliable", "unreliable"])
def client(request) -> Client:
    if request.param == "reliable":
        cls = Socket
    elif request.param == "unreliable":
        cls = UnreliableSocket
    else:
        raise NotImplementedError("Only reliable and unreliable sockets currently supported")

    with socket.socket(type=socket.SOCK_DGRAM) as sock:
        yield Client(cls(sock))


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
    with subprocess.Popen(["./out/server/server", str(port)]) as proc:
        # TODO: should have the server print a message when listening, and then yield after that instead of sleeping
        #  1 second
        time.sleep(1)
        yield proc
        proc.kill()


class TestResponses:
    not_implemented_message_format = "Command not yet implemented: {command}"
    invalid_command_message_format = "Invalid command: {command}"
    delete_message = b"Deleted file\n"


@pytest.mark.usefixtures("killable_server")
class TestServerNonExiting(TestResponses):
    def test_get(self, client: Client):
        filepath = resources_filepath.joinpath("foo1")
        response = client.get(filepath)

        with open(filepath, "rb") as f:
            file_contents = f.read()
        expected_response = file_contents
        assert response == expected_response

    def test_get_sample_files(self, client: Client):
        """Tests the sample files provided for this homework assignment"""
        sample_files = ["foo1", "foo2", "foo3"]
        for file in sample_files:
            filepath = resources_filepath.joinpath(file)
            response = client.get(filepath)

            with open(filepath, "rb") as f:
                file_contents = f.read()
            expected_response = file_contents
            assert response == expected_response

    def test_put(self, client: Client):
        test_contents = b"Hello world!\nGoodbye...\n"
        filepath = resources_filepath.joinpath("test.txt")
        client.put(filepath, test_contents)

        with open(filepath, "rb") as f:
            file_contents = f.read()

        assert file_contents == test_contents

        Path(filepath).unlink()

    def test_put_sample_files(self, client: Client):
        """Tests the sample files provided for this homework assignment"""
        sample_files = ["foo1", "foo2", "foo3"]
        test_prefix = "test_"
        for file in sample_files:
            input_filepath = resources_filepath.joinpath(file)
            with open(input_filepath, "rb") as f:
                test_contents = f.read()
            output_filepath = resources_filepath.joinpath(f"{test_prefix}{file}")
            client.put(output_filepath, test_contents)

            with open(output_filepath, "rb") as f:
                file_contents = f.read()

            assert file_contents == test_contents

            Path(output_filepath).unlink()

    def test_delete(self, client: Client):
        filepath = resources_filepath.joinpath("test.txt")
        with open(filepath, "w") as f:
            f.write("Test case, soon to be deleted\n")
        assert Path(filepath).is_file()

        response = client.delete(filepath)
        expected_response = self.delete_message
        assert response == expected_response
        assert not Path(filepath).is_file()

    def test_delete_nonexistent_file_does_nothing(self, client: Client):
        filepath = resources_filepath.joinpath("test.txt")
        # test file should not exist
        assert not Path(filepath).is_file()

        response = client.delete(filepath)
        expected_response = b""
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
