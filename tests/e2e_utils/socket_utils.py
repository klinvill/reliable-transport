import socket
from typing import Tuple


class Socket:
    def __init__(self, sock: socket.socket):
        self.sock = sock

    def sendto(self, data: bytes, s_address: Tuple[str, int]) -> int:
        return self.sock.sendto(data, s_address)

    def recvfrom(self, bufsize: int) -> Tuple[bytes, Tuple[str, int]]:
        return self.sock.recvfrom(bufsize)


class UnreliableSocket(Socket):
    def __init__(self, sock: socket.socket):
        super().__init__(sock)
        self.recv_counter = 0

    def sendto(self, data: bytes, s_address: Tuple[str, int]) -> int:
        # the unreliable socket will periodically flip all the bits in a message to simulate an out-of-order message
        # being received, or the original message being dropped during transmission. This shouldnt cause the parsers to
        # crash since they should only look at the header which will contain sequence/ack numbers that are not expected.
        flipped_data = bytes([d ^ 0xff for d in data])
        print(f"Sending (flipped): {flipped_data}")
        self.sock.sendto(flipped_data, s_address)

        print(f"Sending: {data}")
        return self.sock.sendto(data, s_address)

    def recvfrom(self, bufsize: int) -> Tuple[bytes, Tuple[str, int]]:
        # periodically ignore a message, simulating the case where a response is lost
        if self.recv_counter % 2 == 0:
            (result, from_address) = self.sock.recvfrom(bufsize)
            self.recv_counter += 1
            print(f"Received (ignoring): {(result, from_address)}")

        (result, from_address) = self.sock.recvfrom(bufsize)
        self.recv_counter += 1
        print(f"Received: {(result, from_address)}")
        return (result, from_address)
