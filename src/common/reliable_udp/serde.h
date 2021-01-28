//
// Created by Kirby Linvill on 1/27/21.
//

#ifndef UDP_RELIABLE_UDP_SERDE_H
#define UDP_RELIABLE_UDP_SERDE_H

#include "types.h"

int serialize(RudpMessage* message, char* buffer, int buffer_len);
int deserialize(char* buffer, int buffer_len, RudpMessage* message);

// methods intended primarily for internal use
int serialize_header(RudpHeader* header, char* buffer, int buffer_len);
int serialize_int(int number, char* buffer, int buffer_len);

int deserialize_header(char* buffer, int buffer_len, RudpHeader* header);
int deserialize_int(char* buffer, int buffer_len, int* value);

#endif //UDP_RELIABLE_UDP_SERDE_H
