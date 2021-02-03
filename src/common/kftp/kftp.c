//
// Created by Kirby Linvill on 1/29/21.
//

#include "kftp.h"

#include "kftp_serde.h"
#include "../reliable_udp/reliable_udp.h"
#include "../utils.h"

#include <stdio.h>
#include <assert.h>


int kftp_send_file(FILE* read_fp, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver) {
    int status = fseek(read_fp, 0, SEEK_END);
    // TODO: error handling;
    if (status < 0)
        return status;
    long file_size = ftell(read_fp);
    // TODO: error handling;
    if (file_size < 0)
        return file_size;
    status = fseek(read_fp, 0, SEEK_SET);
    // TODO: error handling;
    if (status < 0)
        return status;

    KftpHeader header = {.data_size=file_size};

    int rudp_size_limit = MAX_DATA_SIZE;
    char rudp_buffer[MAX_DATA_SIZE] = {};

    // only the first packet needs to serialize anything since the rest is just read directly from the file
    int serialized = serialize_kftp_header(&header, rudp_buffer, rudp_size_limit);
    int first_packet_remaining_size = rudp_size_limit - serialized;
    assert(first_packet_remaining_size >= 0);
    size_t read_bytes = fread(&rudp_buffer[serialized], sizeof(char), first_packet_remaining_size, read_fp);

    if (read_bytes != first_packet_remaining_size) {
        if(feof(read_fp) == 0) {
            assert(file_size == read_bytes);
        } else
            // TODO: error handling
            return -1;
    }

    size_t remaining_bytes = file_size - read_bytes;

    status = rudp_send(rudp_buffer, serialized+read_bytes, to, sender, receiver);
    // TODO: error handling
    if (status < 0)
        return status;

    while (remaining_bytes > 0) {
        size_t bytes_to_read = min(rudp_size_limit, remaining_bytes);
        read_bytes = fread(rudp_buffer, sizeof(char), bytes_to_read, read_fp);

        if (read_bytes != bytes_to_read) {
            // We don't need to check EOF here since we have already determined where EOF should be. Therefore, the
            // number of read bytes should only differ from the bytes we told the file to read if an error occurred or
            // if the number of bytes in the file changed since we first calculated the size, in which case we want to
            // abort anyway.
            // TODO: error handling
            return -1;
        }

        status = rudp_send(rudp_buffer, read_bytes, to, sender, receiver);
        // TODO: error handling
        if (status < 0)
            return status;

        remaining_bytes -= read_bytes;
    }

    assert(remaining_bytes == 0);
    return 0;
}

int kftp_recv_file(FILE* write_fp, SocketInfo* from, RudpReceiver * receiver) {
    char rudp_buffer[MAX_PAYLOAD_SIZE] = {};

    int received_bytes = rudp_recv(rudp_buffer, MAX_PAYLOAD_SIZE, from, receiver);
    // TODO: error handling
    if (received_bytes < 0)
        return received_bytes;

    KftpHeader header = {};
    int deserialized = deserialize_kftp_header(rudp_buffer, MAX_DATA_SIZE, &header);
    // TODO: error handling
    if (deserialized != KFTP_HEADER_SIZE)
        return -1;

    int received_data_bytes = received_bytes - deserialized;
    assert(received_data_bytes > 0);
    int remaining_bytes = header.data_size - received_data_bytes;

    size_t written_chunk_size = fwrite(&rudp_buffer[deserialized], sizeof(char), received_data_bytes, write_fp);
    // TODO: error handling
    if (written_chunk_size != received_data_bytes)
        return -1;

    while(remaining_bytes > 0) {
        received_bytes = rudp_recv(rudp_buffer, MAX_PAYLOAD_SIZE, from, receiver);
        assert(received_bytes > 0);
        written_chunk_size = fwrite(rudp_buffer, sizeof(char), received_bytes, write_fp);
        // TODO: error handling
        if (written_chunk_size != received_bytes)
            return -1;
        remaining_bytes -= received_bytes;
    }

    return 0;
}
