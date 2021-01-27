//
// Created by Kirby Linvill on 1/27/21.
//

#include "utils.h"

int elapsed_time(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000 + (end->tv_usec - start->tv_usec) / 1000;
}
