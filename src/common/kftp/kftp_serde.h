//
// Created by Kirby Linvill on 1/29/21.
//

#ifndef UDP_KFTP_SERDE_H
#define UDP_KFTP_SERDE_H

#include "kftp.h"

int serialize_kftp_header(KftpHeader* header, char* buffer, int buffer_len);
int deserialize_kftp_header(char* buffer, int buffer_len, KftpHeader* header);

#endif //UDP_KFTP_SERDE_H
