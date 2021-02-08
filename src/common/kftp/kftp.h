//
// KFTP (Kirby's File Transfer Protocol) interface
//
// KFTP uses RUDP as the underlying transport
//

#ifndef UDP_KFTP_H
#define UDP_KFTP_H

#include <stdio.h>

#include "../reliable_udp/types.h"


// size of KftpHeader in bytes
#define KFTP_HEADER_SIZE sizeof(KftpHeader)


// TODO: using an int limits the size of a possible file to ~2GB. Should instead use a long datatype for the size
// The KFTP header specifies how large of a message follows. This message is likely fragmented over several RUDP messages
typedef struct {
    int data_size; // size of data in bytes
} KftpHeader;

typedef struct {
    KftpHeader header;
    char* data;
} KftpMessage;

// Reads from the specified `read_fp` and sends the contents to `to` socket over RUDP. The content is read and sent as
// a stream.
//
// Returns 0 on success, and a negative int on failure.
int kftp_send_file(FILE* read_fp, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver);

// Writes the data received from the `from` socket, over RUDP, to the file specified by `write_fp`. The content is
// received and written as a stream.
//
// Returns 0 on success, and a negative int on failure.
int kftp_recv_file(FILE* write_fp, SocketInfo* from, RudpReceiver * receiver);

#endif //UDP_KFTP_H
