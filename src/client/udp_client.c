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
#include "../common/kftp/kftp.h"

#define BUFSIZE 1024
#define DELIMITERS " \n\t\r\v\f"

#define PARSE_ERROR (-2)

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int handle_response(SocketInfo *sock_info, RudpReceiver *receiver) {
    int n;
    char buf[BUFSIZE];

    n = rudp_recv(buf, BUFSIZE, sock_info, receiver);
    if (n < 0)
        error("ERROR in recvfrom");

    buf[n] = 0;
    printf("%s\n", buf);
    fflush(stdout);

    return n;
}

int do_ls(SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char* command = "ls";

    int n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    return handle_response(socket_info, receiver);
}

void do_exit(SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char* command = "exit";

    int n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    n = handle_response(socket_info, receiver);
    if (n < 0)
        error("ERROR in handle_response");

    exit(0);
}

int do_get(char* filename, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char command[BUFSIZE] = {};
    int n = snprintf(command, BUFSIZE, "get %s", filename);
    if (n >= BUFSIZE)
        error("ERROR in sprintf");

    n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    FILE* fetched_file = fopen(filename, "w");
    int result = kftp_recv_file(fetched_file, socket_info, receiver);
    fclose(fetched_file);

    if (result < 0)
        error("ERROR while downloading file");

    printf("Downloaded file: %s\n", filename);
    return result;
}

int do_put(char* filename, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char command[BUFSIZE] = {};
    int n = snprintf(command, BUFSIZE, "put %s", filename);
    if (n >= BUFSIZE)
        error("ERROR in sprintf");

    n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    FILE* file = fopen(filename, "r");
    int result = kftp_send_file(file, socket_info, sender, receiver);
    fclose(file);

    if (result < 0)
        error("ERROR while sending file");

    printf("Sent file: %s\n", filename);
    return result;
}

int do_delete(char* filename, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char command[BUFSIZE] = {};
    int n = snprintf(command, BUFSIZE, "delete %s", filename);
    if (n >= BUFSIZE || n == 0)
        error("ERROR in sprintf");

    n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    return handle_response(socket_info, receiver);
}


int process_command(char *message, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char *first_token = strtok(message, DELIMITERS);
    if (!first_token) return PARSE_ERROR;

    char *second_token = strtok(NULL, DELIMITERS);

    // single arg commands
    if (strcmp(first_token, "ls") == 0 || strcmp(first_token, "exit") == 0) {
        // only one argument allowed
        if (second_token) return PARSE_ERROR;

        if (strcmp(first_token, "ls") == 0)
            return do_ls(socket_info, sender, receiver);
        else if (strcmp(first_token, "exit") == 0)
            do_exit(socket_info, sender, receiver);
    }

    // double arg commands
    {
        if (!second_token) return PARSE_ERROR;

        // there are no commands that take 3 arguments
        if (strtok(NULL, DELIMITERS)) return PARSE_ERROR;

        if (strcmp(first_token, "get") == 0)
            return do_get(second_token, socket_info, sender, receiver);
        else if (strcmp(first_token, "put") == 0)
            return do_put(second_token, socket_info, sender, receiver);
        else if (strcmp(first_token, "delete") == 0)
            return do_delete(second_token, socket_info, sender, receiver);
    }

    return PARSE_ERROR;
}


int run_command(char *message, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    int result = process_command(message, socket_info, sender, receiver);

    if (result < 0)
        error("ERROR in process_command");

    // acks to server can be lost, so it's possible to successfully finish a task without the server's
    // knowledge. Here ee check to make sure there are no outstanding acks before considering the command complete
    char ack_buff[BUFSIZE] = {};
    int status = rudp_check_acks(ack_buff, BUFSIZE, socket_info, receiver);
    if (status < 0)
        error("ERROR in rudp_check_acks");

    return result;
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

    RudpSender sender = {.sender_timeout=SENDER_TIMEOUT, .message_timeout=INITIAL_TIMEOUT};
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

        int status = run_command(buf, &sock_info, &sender, &receiver);
        if (status < 0)
            error("ERROR in run_command");
    }
}
