from typing import Tuple

from tests.e2e_utils.rudp_utils import RudpMessage, RudpReceiver, RudpSender


class KftpHeader:
    SIZE = 4

    def __init__(self, data_size: int):
        self.data_size = data_size

    def serialize(self) -> bytes:
        return self.data_size.to_bytes(4, "big", signed=True)

    @staticmethod
    def deserialize(data: bytes) -> "KftpHeader":
        assert len(data) >= 4
        return KftpHeader(int.from_bytes(data[0:4], "big", signed=True))


class KftpSender:
    def __init__(self, sender: RudpSender):
        self.sender = sender

    def send_to(self, file_data: bytes, addr: Tuple[str, int]):
        header = KftpHeader(len(file_data))
        serialized_header = header.serialize()

        if len(file_data) + len(serialized_header) > RudpMessage.DATASIZE:
            offset = RudpMessage.DATASIZE - len(serialized_header)
            first_message = serialized_header + file_data[:offset]
            self.sender.send_to(first_message, addr)

            while offset < len(file_data):
                next_message_size = min(len(file_data) - offset, RudpMessage.DATASIZE)
                next_message = file_data[offset:offset+next_message_size]
                self.sender.send_to(next_message, addr)
                offset += next_message_size

        else:
            self.sender.send_to(serialized_header + file_data, addr)


class KftpReceiver:
    def __init__(self, receiver: RudpReceiver):
        self.receiver = receiver

    def receive_from(self) -> Tuple[bytes, Tuple[str, int]]:
        first_message, first_addr = self.receiver.receive_from()
        header = KftpHeader.deserialize(first_message)
        file_data = first_message[4:]

        while len(file_data) < header.data_size:
            next_message, next_addr = self.receiver.receive_from()
            if first_addr == next_addr:
                file_data += next_message

        return file_data, first_addr
