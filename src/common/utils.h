//
// Created by Kirby Linvill on 1/27/21.
//

#ifndef UDP_UTILS_H
#define UDP_UTILS_H

#include <sys/time.h>

// returns elapsed time in milliseconds
int elapsed_time(struct timeval *start, struct timeval *end);

#endif //UDP_UTILS_H
