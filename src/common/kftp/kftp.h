//
// Created by Kirby Linvill on 1/29/21.
//

#ifndef UDP_KFTP_H
#define UDP_KFTP_H

#include <stdio.h>

#include "../reliable_udp/types.h"


#define KFTP_HEADER_SIZE 4


typedef struct {
    int data_size; // size of data in bytes
} KftpHeader;

typedef struct {
    KftpHeader header;
    char* data;
} KftpMessage;

int kftp_send_file(FILE* read_fp, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver);
int kftp_recv_file(FILE* write_fp, SocketInfo* from, RudpReceiver * receiver);

#endif //UDP_KFTP_H
