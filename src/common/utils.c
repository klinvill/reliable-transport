//
// Miscellaneous helper functions
//

#include "utils.h"

// returns elapsed time in milliseconds
int elapsed_time(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000 + (end->tv_usec - start->tv_usec) / 1000;
}

int min(int a, int b) {
    return (a < b) ? a : b;
}
