/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/errno.h>

#include "../common/reliable_udp/reliable_udp.h"

#define BUFSIZE 1024
// TODO: what should the recv_timeout be?
#define RECV_TIMEOUT 500000    // in microseconds

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

void handle_response(SocketInfo *sock_info, RudpReceiver *receiver) {
    int n;
    char buf[BUFSIZE];

    n = rudp_recv(buf, BUFSIZE, sock_info, receiver);
    if (n < 0)
        error("ERROR in recvfrom");

    buf[n] = 0;
    printf("%s\n", buf);
    fflush(stdout);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval recv_timeout;
    recv_timeout.tv_usec = RECV_TIMEOUT;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)))
        error("ERROR setting receive timout for socket");
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *) server->h_addr,
          (char *) &serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    serverlen = sizeof(serveraddr);
    SocketInfo sock_info = {.sockfd=sockfd, .addr=(struct sockaddr *) &serveraddr, .addr_len=serverlen};

    RudpSender sender = {.sender_timeout=SENDER_TIMEOUT};
    RudpReceiver receiver = {};

    while (1) {
        /* get a message from the user */
        bzero(buf, BUFSIZE);
        printf("Please enter one of the following messages: \n"
               "\tget <file_name>\n"
               "\tput <file_name>\n"
               "\tdelete <file_name>\n"
               "\tls\n"
               "\texit\n"
               "> "
        );
        fflush(stdout);
        char* result = fgets(buf, BUFSIZE, stdin);
        if (result == NULL) {
            if (ferror(stdin) != 0)
                error("ERROR in fgets");
            else
                // could not read any characters
                continue;
        }

        /* send the command to the server */
        rudp_send(buf, strlen(buf), &sock_info, &sender, &receiver);
        if (n < 0)
            error("ERROR in rudp_send");

        handle_response(&sock_info, &receiver);
    }
}
