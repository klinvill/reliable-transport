//
// KFTP (Kirby's File Transfer Protocol) implementation
//
// KFTP uses RUDP as the underlying transport
//

#include "kftp.h"

#include "kftp_serde.h"
#include "../reliable_udp/reliable_udp.h"
#include "../utils.h"

#include <stdio.h>
#include <assert.h>


int kftp_send_file(FILE* read_fp, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver) {
    // Retrieves the size of the file to determine how much data will be sent in the KFTP message
    int status = fseek(read_fp, 0, SEEK_END);
    if (status < 0) {
        fprintf(stderr, "ERROR in kftp_send_file: error seeking to end of file\n");
        return status;
    }
    long file_size = ftell(read_fp);
    if (file_size < 0) {
        fprintf(stderr, "ERROR in kftp_send_file: error getting file size\n");
        return file_size;
    }

    // Seek back to the beginning so we can read the full file contents
    status = fseek(read_fp, 0, SEEK_SET);
    if (status < 0) {
        fprintf(stderr, "ERROR in kftp_send_file: error seeking to beginning of file\n");
        return status;
    }

    KftpHeader header = {.data_size=file_size};

    // max amount of data that can fit into an RUDP message
    int rudp_size_limit = MAX_DATA_SIZE;
    char rudp_buffer[MAX_DATA_SIZE] = {};

    // only the first packet needs to serialize anything since the rest is just read directly from the file
    int serialized = serialize_kftp_header(&header, rudp_buffer, rudp_size_limit);
    // Since the first RUDP message includes the KFTP header, it has a reduced capacity for KFTP data
    int first_packet_remaining_size = rudp_size_limit - serialized;
    assert(first_packet_remaining_size >= 0);
    size_t read_bytes = fread(&rudp_buffer[serialized], sizeof(char), first_packet_remaining_size, read_fp);

    if (read_bytes != first_packet_remaining_size) {
        // We should only not fill up the first RUDP message if the file is small enough to fit in the single message.
        // In that case, we should have already read to EOF.
        if(feof(read_fp) == 0) {
            assert(file_size == read_bytes);
        } else {
            fprintf(stderr, "ERROR in kftp_send_file: not able to fill up first RUDP message, yet not at EOF\n");
            return -1;
        }
    }

    size_t remaining_bytes = file_size - read_bytes;

    status = rudp_send(rudp_buffer, serialized+read_bytes, to, sender, receiver);
    if (status < 0) {
        fprintf(stderr, "ERROR in kftp_send_file: error in initial rudp_send\n");
        return status;
    }

    // send out successive file chunks until we've sent the rest of the file
    while (remaining_bytes > 0) {
        fprintf(stderr, "Progress: %lu%%                         \r", 100 - (remaining_bytes * 100 / file_size));
        fflush(stderr);

        size_t bytes_to_read = min(rudp_size_limit, remaining_bytes);
        read_bytes = fread(rudp_buffer, sizeof(char), bytes_to_read, read_fp);

        if (read_bytes != bytes_to_read) {
            // We don't need to check EOF here since we have already determined where EOF should be. Therefore, the
            // number of read bytes should only differ from the bytes we told the file to read if an error occurred or
            // if the number of bytes in the file changed since we first calculated the size, in which case we want to
            // abort anyway.
            fprintf(stderr, "ERROR in kftp_send_file: unable to read expected number of bytes from file\n");
            return -1;
        }

        status = rudp_send(rudp_buffer, read_bytes, to, sender, receiver);
        if (status < 0) {
            fprintf(stderr, "ERROR in kftp_send_file: error in rudp_send\n");
            return status;
        }

        remaining_bytes -= read_bytes;
    }

    assert(remaining_bytes == 0);
    return 0;
}


int kftp_recv_file(FILE* write_fp, SocketInfo* from, RudpReceiver * receiver) {
    char rudp_buffer[MAX_PAYLOAD_SIZE] = {};

    int received_bytes = rudp_recv(rudp_buffer, MAX_PAYLOAD_SIZE, from, receiver);
    if (received_bytes < 0) {
        fprintf(stderr, "ERROR in kftp_recv_file: error in initial rudp_recv\n");
        return received_bytes;
    }

    // The first message we receive contains the header that specifies how large the incoming file is
    KftpHeader header = {};
    int deserialized = deserialize_kftp_header(rudp_buffer, MAX_DATA_SIZE, &header);
    if (deserialized != KFTP_HEADER_SIZE) {
        fprintf(stderr, "ERROR in kftp_recv_file: header deserialization error\n");
        return -1;
    }

    int received_data_bytes = received_bytes - deserialized;
    assert(received_data_bytes > 0);
    int remaining_bytes = header.data_size - received_data_bytes;

    // we write the file as we read it in order to scale to large files without needing increased memory
    size_t written_chunk_size = fwrite(&rudp_buffer[deserialized], sizeof(char), received_data_bytes, write_fp);
    if (written_chunk_size != received_data_bytes) {
        fprintf(stderr, "ERROR in kftp_recv_file: error writing to file\n");
        return -1;
    }

    while(remaining_bytes > 0) {
        fprintf(stderr, "Progress: %d%%                         \r", 100 - (remaining_bytes * 100 / header.data_size));
        fflush(stderr);

        received_bytes = rudp_recv(rudp_buffer, MAX_PAYLOAD_SIZE, from, receiver);
        if (received_bytes <= 0) {
            fprintf(stderr, "ERROR in kftp_recv_file: error in rudp_recv\n");
            return -1;
        }

        written_chunk_size = fwrite(rudp_buffer, sizeof(char), received_bytes, write_fp);
        if (written_chunk_size != received_bytes) {
            fprintf(stderr, "ERROR in kftp_recv_file: Written chunk size (%zu) does not match received_bytes (%d)\n",
                    written_chunk_size, received_bytes);
            return -1;
        }
        remaining_bytes -= received_bytes;
    }

    assert(remaining_bytes == 0);
    return 0;
}
