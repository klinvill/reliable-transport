//
// Created by Kirby Linvill on 1/29/21.
//

#include "kftp.h"

#include "kftp_serde.h"
#include "../reliable_udp/reliable_udp.h"
#include "../utils.h"

#include <stdio.h>
#include <assert.h>


int kftp_send_file(FILE* read_fp, SocketInfo* to, RudpSender* sender) {
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

    status = rudp_send(rudp_buffer, serialized+read_bytes, to, sender);
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

        status = rudp_send(rudp_buffer, read_bytes, to, sender);
        // TODO: error handling
        if (status < 0)
            return status;

        remaining_bytes -= read_bytes;
    }

    assert(remaining_bytes == 0);
    return 0;
}
