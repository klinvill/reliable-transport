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

typedef struct {
    int sockfd;
    struct sockaddr* serveraddr;
    socklen_t serverlen;
} SocketInfo;

void handle_response(SocketInfo* sock_info) {
    int n = 0;
    char buf[BUFSIZE];

    bool did_recv_any = false;

    while((n = recvfrom(sock_info->sockfd, buf, BUFSIZE, 0, sock_info->serveraddr, &sock_info->serverlen)) > 0) {
        did_recv_any = true;
        buf[n] = 0;
        printf("%s", buf);
    }

    if (n < 0 && errno != EWOULDBLOCK)
        error("ERROR in recvfrom");

    printf("\n");

    if (!did_recv_any)
        error("Timeout before receiving any data");
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
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
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
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    serverlen = sizeof(serveraddr);
    SocketInfo sock_info = {.sockfd=sockfd, .serveraddr=(struct sockaddr*) &serveraddr, .serverlen=serverlen};

    while(1) {
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
        fgets(buf, BUFSIZE, stdin);

        /* send the message to the server */
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*) &serveraddr, serverlen);
        if (n < 0)
            error("ERROR in sendto");

        handle_response(&sock_info);
    }
}
