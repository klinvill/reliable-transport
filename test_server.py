import pytest
import subprocess
import socket
import time

address = "127.0.0.1"
port = 8080

class Client:
    BUFSIZE = 1024

    def __init__(self, sock: socket.socket):
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
        (data, addr) = self.sock.recvfrom(self.BUFSIZE)
        if addr != (address, port):
            raise RuntimeError(f"Received message from {addr} instead of {(address, port)}")
        return data

    def send_and_receive(self, data: bytes) -> bytes:
        self.send(data)
        return self.receive()


@pytest.fixture
def client() -> Client:
    with socket.socket(type=socket.SOCK_DGRAM) as sock:
        yield Client(sock)


@pytest.fixture(scope="module")
def server():
    with subprocess.Popen(["./out/server", str(port)]) as proc:
        # TODO: should have the server print a message when listening, and then yield after that instead of sleeping
        #  1 second
        time.sleep(1)
        yield None
        proc.kill()


@pytest.mark.usefixtures("server")
class TestServer:
    not_implemented_message_format = "Command not yet implemented: {command}"
    invalid_command_message_format = "Invalid command: {command}"

    def test_get_not_implemented(self, client: Client):
        filename = "test.txt"
        response = client.get(filename)
        expected_response = self.not_implemented_message_format.format(command=f"get {filename}").encode()
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

    def test_ls_not_implemented(self, client: Client):
        response = client.ls()
        expected_response = self.not_implemented_message_format.format(command="ls").encode()
        assert response == expected_response

    def test_exit_not_implemented(self, client: Client):
        response = client.exit()
        expected_response = self.not_implemented_message_format.format(command="exit").encode()
        assert response == expected_response

    def test_invalid_commands_sent_back(self, client: Client):
        command = "foo bar"
        response = client.send_and_receive(command.encode())
        expected_response = self.invalid_command_message_format.format(command=command).encode()
        assert response == expected_response

    @pytest.mark.parametrize("command", ["get", "put", "delete"])
    def test_must_pass_one_argument(self, client: Client, command: str):
        files = "test.txt foo.bar"
        multiple_args_command = f"{command} {files}"
        multi_response = client.send_and_receive(multiple_args_command.encode())
        expected_multi_response = self.invalid_command_message_format.format(command=multiple_args_command).encode()
        assert multi_response == expected_multi_response

        no_arg_response = client.send_and_receive(command.encode())
        no_arg_expected_response = self.invalid_command_message_format.format(command=command).encode()
        assert no_arg_response == no_arg_expected_response

    @pytest.mark.parametrize("command", ["ls", "exit"])
    def test_must_pass_no_arguments(self, client: Client, command: str):
        arg = "foo"
        full_command = f"{command} {arg}"
        response = client.send_and_receive(full_command.encode())
        expected_response = self.invalid_command_message_format.format(command=full_command).encode()
        assert response == expected_response
