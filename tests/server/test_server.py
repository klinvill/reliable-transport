import pytest
import subprocess
import socket
import time
from typing import Generator
from pathlib import Path

address = "127.0.0.1"
port = 8080
resources_filepath = Path("tests/resources/")


class RudpHeader:
    def __init__(self, seq_num: int, ack_num: int, data_size: int):
        self.seq_num = seq_num
        self.ack_num = ack_num
        self.data_size = data_size

    def serialize(self) -> bytes:
        return (self.seq_num.to_bytes(4, "big", signed=True)
                + self.ack_num.to_bytes(4, "big", signed=True)
                + self.data_size.to_bytes(4, "big", signed=True)
                )

    @staticmethod
    def deserialize(data: bytes) -> "RudpHeader":
        assert len(data) >= 12
        return RudpHeader(int.from_bytes(data[0:4], "big", signed=True),
                          int.from_bytes(data[4:8], "big", signed=True),
                          int.from_bytes(data[8:12], "big", signed=True))


class RudpMessage:
    def __init__(self, header: RudpHeader, data: bytes):
        self.header = header
        self.data = data
        assert header.data_size == len(data)

    def serialize(self) -> bytes:
        return self.header.serialize() + self.data

    @staticmethod
    def deserialize(data: bytes) -> "RudpMessage":
        header = RudpHeader.deserialize(data)
        assert header.data_size == len(data[12:])
        return RudpMessage(header, data[12:])


class KftpHeader:
    def __init__(self, data_size: int):
        self.data_size = data_size

    def serialize(self) -> bytes:
        return self.data_size.to_bytes(4, "big", signed=True)

    @staticmethod
    def deserialize(data: bytes) -> "KftpHeader":
        assert len(data) >= 4
        return KftpHeader(int.from_bytes(data[0:4], "big", signed=True))


class RudpSender:
    BUFSIZE = 1024

    def __init__(self, sock: socket.socket, last_ack: int = 0):
        self.sock = sock
        self.last_ack = last_ack

    def send(self, data: bytes):
        message = RudpMessage(RudpHeader(self.last_ack + 1, 0, len(data)), data)
        self.sock.sendto(message.serialize(), (address, port))

        acked = False
        while not acked:
            (data, addr) = self.sock.recvfrom(self.BUFSIZE)
            if addr == (address, port):
                recv_message = RudpMessage.deserialize(data)
                if recv_message.header.ack_num == self.last_ack + 1:
                    self.last_ack += 1
                    acked = True


class RudpReceiver:
    BUFSIZE = 1024

    def __init__(self, sock: socket.socket, last_received: int = 0):
        self.sock = sock
        self.last_received = last_received

    def receive(self) -> bytes:
        while True:
            try:
                (data, addr) = self.sock.recvfrom(self.BUFSIZE)
            except socket.timeout:
                return b''
            if addr == (address, port):
                recv_message = RudpMessage.deserialize(data)
                if recv_message.header.seq_num == self.last_received + 1:
                    self.last_received += 1
                    self.send_ack(recv_message.header.seq_num)
                    return recv_message.data

    def send_ack(self, ack_num: int):
        message = RudpMessage(RudpHeader(0, ack_num, 0), b'')
        self.sock.sendto(message.serialize(), (address, port))


class KftpReceiver:
    def __init__(self, receiver: RudpReceiver):
        self.receiver = receiver

    def receive(self) -> bytes:
        first_message = self.receiver.receive()
        header = KftpHeader.deserialize(first_message)
        file_data = first_message[4:]

        while len(file_data) < header.data_size:
            next_message = self.receiver.receive()
            file_data += next_message

        return file_data


class Client:
    BUFSIZE = 1024
    SOCKET_TIMEOUT = 0.1    # in seconds

    def __init__(self, sock: socket.socket):
        sock.settimeout(self.SOCKET_TIMEOUT)
        self.sock = sock
        self.sender = RudpSender(self.sock)
        self.receiver = RudpReceiver(self.sock)

    def get(self, filename: str) -> bytes:
        self.send(f"get {filename}".encode())
        return KftpReceiver(self.receiver).receive()

    def put(self, filename: str) -> bytes:
        return self.send_and_receive(f"put {filename}".encode())

    def delete(self, filename: str) -> bytes:
        return self.send_and_receive(f"delete {filename}".encode())

    def ls(self) -> bytes:
        return self.send_and_receive(b"ls")

    def exit(self) -> bytes:
        return self.send_and_receive(b"exit")

    def send(self, data: bytes):
        self.sender.send(data)

    def receive(self) -> bytes:
        return self.receiver.receive()

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

    def test_put_not_implemented(self, client: Client):
        filepath = resources_filepath.joinpath("test.txt")
        response = client.put(filepath)
        expected_response = self.not_implemented_message_format.format(command=f"put {filepath}").encode()
        assert response == expected_response

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
