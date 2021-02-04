import socket
from typing import Tuple

from tests.e2e_utils.socket_utils import Socket


class RudpHeader:
    SIZE = 12

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
    BUFSIZE = 1024
    DATASIZE = BUFSIZE - RudpHeader.SIZE

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


class RudpReceiver:
    def __init__(self, sock: Socket, last_received: int = 0):
        self.sock = sock
        self.last_received = last_received

    def receive_from(self) -> Tuple[bytes, Tuple[str, int]]:
        while True:
            try:
                (data, addr) = self.sock.recvfrom(RudpMessage.BUFSIZE)
            except socket.timeout:
                return b'', ('', 0)

            recv_message = RudpMessage.deserialize(data)
            print(f"Received message with seq header: {recv_message.header.seq_num}, looking for: {self.last_received+1}")
            if recv_message.header.seq_num == self.last_received + 1:
                self.last_received += 1
                self.send_ack(recv_message.header.seq_num, addr)
                return recv_message.data, addr
            # ack previously received messages in case the ack hasn't yet been received by the sender
            elif recv_message.header.seq_num == self.last_received:
                self.send_ack(recv_message.header.seq_num, addr)

    def send_ack(self, ack_num: int, addr: Tuple[str, int]):
        message = RudpMessage(RudpHeader(0, ack_num, 0), b'')
        print(f"Sending ack: {ack_num} to: {addr}")
        self.sock.sendto(message.serialize(), addr)


class RudpSender:
    timeout_retries = 5

    def __init__(self, sock: Socket, receiver: RudpReceiver, last_ack: int = 0):
        self.sock = sock
        self.receiver = receiver
        self.last_ack = last_ack

    def send_to(self, data: bytes, to_addr: Tuple[str, int]):
        message = RudpMessage(RudpHeader(self.last_ack + 1, 0, len(data)), data)

        counter = 0
        acked = False
        while not acked:
            self.sock.sendto(message.serialize(), to_addr)
            try:
                (recv_data, addr) = self.sock.recvfrom(RudpMessage.BUFSIZE)
            except socket.timeout as err:
                if counter < self.timeout_retries:
                    counter += 1
                    print("Timeout while waiting for ACK, retrying...")
                    continue
                else:
                    raise err
            print(f"Checked for ack: {recv_data}")
            if addr == to_addr:
                recv_message = RudpMessage.deserialize(recv_data)
                if recv_message.header.ack_num == self.last_ack + 1:
                    print(f"Acked: {recv_message.header.ack_num}")
                    self.last_ack += 1
                    acked = True
                # ack previously received messages in case the ack hasn't yet been received
                elif recv_message.header.seq_num == self.receiver.last_received:
                    self.receiver.send_ack(recv_message.header.seq_num, addr)
