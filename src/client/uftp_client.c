//
// Simple reliable file transfer client over UDP
//
// This client uses RUDP (Reliable UDP) and KFTP (Kirby's File Transfer Protocol) to provide this functionality. This
// work was done as a homework assignment for a networking class.
//
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

// Delimiters to use when extracting commands and arguments from user-supplied input
#define DELIMITERS " \n\t\r\v\f"

// TODO: standardize error codes between client and server
#define PARSE_ERROR (-2)

/* 
 * error - wrapper for perror
 */
// TODO: rename to fatal_error, exit with -1, and ensure that this is only called in the case of fatal errors
void error(char *msg) {
    perror(msg);
    exit(0);
}


// Receives a RUDP response from the server, then prints out that response.
//
// A limitation of this function is that it will only receive one RUDP message, so the size of messages this can handle
// is limited to the size of the message buffer.
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


// Handles `ls` command
int do_ls(SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char* command = "ls";

    // not currently implemented over KFTP, instead send using RUDP
    // TODO: the limitation to use RUDP effectively means that the listed files must fit into a single message buffer.
    //  should implement ls through KFTP so that larger results can be returned
    int n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    return handle_response(socket_info, receiver);
}


// Handles `exit` command
void do_exit(SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char* command = "exit";

    // not currently implemented over KFTP, instead send using RUDP
    int n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    // expect a message back from the server that it is exiting gracefully
    n = handle_response(socket_info, receiver);
    if (n < 0)
        error("ERROR in handle_response");

    // we terminate the client since the server should also have terminated
    exit(0);
}


// Handles `get` command
int do_get(char* filename, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char command[BUFSIZE] = {};
    int n = snprintf(command, BUFSIZE, "get %s", filename);
    if (n >= BUFSIZE)
        error("ERROR in sprintf");

    // the initial get command (that notifies the server that it should send a file) is currently not
    // implemented using KFTP, so instead we just send it using RUDP
    n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    FILE* fetched_file = fopen(filename, "w");
    if (fetched_file == NULL)
        error("ERROR opening file to write to");
    int result = kftp_recv_file(fetched_file, socket_info, receiver);
    fclose(fetched_file);

    if (result < 0)
        error("ERROR while downloading file");

    printf("Downloaded file: %s\n", filename);
    return result;
}


// Handles `put` command
int do_put(char* filename, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char command[BUFSIZE] = {};
    int n = snprintf(command, BUFSIZE, "put %s", filename);
    if (n >= BUFSIZE)
        error("ERROR in sprintf");

    // the initial put command (that notifies the server that it should prepare to receive a file) is currently not
    // implemented using KFTP, so instead we just send it using RUDP
    n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    FILE* file = fopen(filename, "r");
    if (file == NULL)
        error("ERROR opening file to send");
    int result = kftp_send_file(file, socket_info, sender, receiver);
    fclose(file);

    if (result < 0)
        error("ERROR while sending file");

    // TODO: should we require the server to send back a message stating that the file was successfully received?
    printf("Sent file: %s\n", filename);
    return result;
}


// Handles `delete` command.
int do_delete(char* filename, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char command[BUFSIZE] = {};
    int n = snprintf(command, BUFSIZE, "delete %s", filename);
    if (n >= BUFSIZE || n == 0)
        error("ERROR in sprintf");

    // delete is currently not implemented using KFTP, so instead we just send the command using RUDP
    n = rudp_send(command, strlen(command), socket_info, sender, receiver);
    if (n < 0)
        error("ERROR in rudp_send");

    // we expect the server to send back a response when the file is successfully deleted
    return handle_response(socket_info, receiver);
}


// Executes the proper processing based on the given command.
//
// This function uses strtok which will mutate the message argument.
int process_command(char *message, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    // TODO: unify command parsing with server implementation
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
            // does not return since do_exit terminates the process
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

    // unrecognized command
    return PARSE_ERROR;
}


// Handles running the users command and returns the result. This function also handles checking for any straggling
// messages that need to be ack'd before returning.
int run_command(char *command, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    // strtok is used to parse the strings and is destructive
    // TODO: should implement a separate command parsing function that is shared between client and server
    char command_copy[BUFSIZE] = {};
    strcpy(command_copy, command);

    int result = process_command(command, socket_info, sender, receiver);

    // TODO: should unify error handling with server when possible
    if (result == PARSE_ERROR) {
        printf("Invalid command: %s\n", command_copy);
        return 0;
    }
    else if (result < 0)
        error("ERROR in process_command");

    // acks to server can be lost, so it's possible to successfully finish a task without the server's
    // knowledge. Here we check to make sure there are no outstanding acks before considering the command complete
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
    memset((char *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    memcpy((char *) &serveraddr.sin_addr.s_addr, (char *) server->h_addr_list[0], server->h_length);
    serveraddr.sin_port = htons(portno);

    serverlen = sizeof(serveraddr);
    SocketInfo sock_info = {.sockfd=sockfd, .addr=(struct sockaddr *) &serveraddr, .addr_len=serverlen};

    RudpSender sender = {.sender_timeout=SENDER_TIMEOUT, .message_timeout=INITIAL_TIMEOUT};
    RudpReceiver receiver = {};

    // client loops to remain interactive, only terminates in the case of a fatal error or exit command
    while (1) {
        // get the next command from the user
        memset(buf, 0, BUFSIZE);
        printf("Please enter one of the following messages: \n"
               "\tget <file_name>\n"
               "\tput <file_name>\n"
               "\tdelete <file_name>\n"
               "\tls\n"
               "\texit\n"
               "> "
        );
        // fflush() calls are needed for the end-to-end tests that monitor the stdout of the client process they run
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
