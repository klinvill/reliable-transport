/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#include "../common/reliable_udp/reliable_udp.h"
#include "../common/kftp/kftp.h"

#define BUFSIZE 1024
#define MAX_FILES 100

#define DELIMITERS " \n\t\r\v\f"

#define PARSE_ERROR (-2)
#define NOT_IMPLEMENTED_ERROR (-3)


/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}


typedef struct {
    int count;
    char **files;
} Filenames;

void cleanup_filenames(Filenames *filenames) {
    for (int i = 0; i < filenames->count; i++) {
        free(filenames->files[i]);
    }
    free(filenames->files);
}

// TODO: do we need to only return files?
// Need to cleanup the allocated memory Filenames when done
Filenames ls_files(char *directory) {
    char **files = malloc(sizeof(char *) * MAX_FILES);
    int i = 0;

    DIR *dir = opendir(directory);

    if (dir == NULL)
        error("Could not open directory");

    struct dirent *entry;
    struct stat entry_info;

    while ((entry = readdir(dir)) != NULL) {
        lstat(entry->d_name, &entry_info);
        // only return files
        if (S_ISREG(entry_info.st_mode)) {
            if (i == MAX_FILES)
                error("Too many files in directory to return them all");

            size_t filename_size = entry->d_namlen + 1;
            char *filename = malloc(sizeof(char) * filename_size);
            strncpy(filename, entry->d_name, filename_size - 1);
            filename[filename_size - 1] = 0;
            files[i] = filename;
            i++;
        }
    }

    return (Filenames) {.count=i, .files=files};
}

// TODO: which parameters (for all the functions) should be const?
int do_send(char *message, SocketInfo *socket_info, RudpSender *sender) {
    int status = rudp_send(message, strlen(message), socket_info, sender);
    if (status < 0)
        error("ERROR in sendto");

    return status;
}

void send_error(int errno, char *command, SocketInfo *socket_info, RudpSender *sender) {
    char err_buff[BUFSIZE] = {0,};

    switch (errno) {
        case PARSE_ERROR :
            snprintf(err_buff, BUFSIZE, "Invalid command: %s", command);
            break;
        case NOT_IMPLEMENTED_ERROR :
            snprintf(err_buff, BUFSIZE, "Command not yet implemented: %s", command);
            break;
        default:
            snprintf(err_buff, BUFSIZE, "Unrecognized error code: %d", errno);
    }

    do_send(err_buff, socket_info, sender);
}

// Commands, prefixed with do_ to avoid name collisions (e.g. with exit())
int do_get(char *filename, SocketInfo *socket_info, RudpSender *sender) {
    FILE *f = fopen(filename, "r");
    if (f == NULL)
        error("Could not open file for reading");

    int result = kftp_send_file(f, socket_info, sender);
    fclose(f);
    return result;
}

int do_put(char *filename, SocketInfo *socket_info, RudpReceiver *receiver) {
    FILE *f = fopen(filename, "w");
    if (f == NULL)
        error("Could not open file for reading");

    int result = kftp_recv_file(f, socket_info, receiver);
    fclose(f);
    return result;
}

int do_delete(char *filename, SocketInfo *socket_info, RudpSender *sender) {
    // According to given spec, we should do nothing if the file does not exist
    if (unlink(filename) == 0)
        return do_send("Deleted file\n", socket_info, sender);

    return 0;
}

int do_ls(SocketInfo *socket_info, RudpSender *sender) {
    Filenames filenames = ls_files(".");
    char message[BUFSIZE] = {0,};

    for (int i = 0; i < filenames.count; i++) {
        char *filename = filenames.files[i];
        // we check for 2 characters extra to hold a newline character and terminating null character
        if (strlen(filename) + strlen(message) + 2 < BUFSIZE) {
            strcat(message, filename);
            strcat(message, "\n");
        } else {
            error("The filenames are too large to all fit into the buffer");
        }
    }
    int ret_code = do_send(message, socket_info, sender);

    // ls_files() allocates memory that needs to be freed
    cleanup_filenames(&filenames);

    return ret_code;
}

// does not return
void do_exit(SocketInfo *socket_info, RudpSender *sender) {
    char *exit_message = "Exiting gracefully";
    do_send(exit_message, socket_info, sender);

    close(socket_info->sockfd);
    exit(0);
}

int process_message(char *message, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char *first_token = strtok(message, DELIMITERS);
    if (!first_token) return PARSE_ERROR;

    char *second_token = strtok(NULL, DELIMITERS);

    // single arg commands
    if (strcmp(first_token, "ls") == 0 || strcmp(first_token, "exit") == 0) {
        // only one argument allowed
        if (second_token) return PARSE_ERROR;

        if (strcmp(first_token, "ls") == 0)
            return do_ls(socket_info, sender);
        else if (strcmp(first_token, "exit") == 0)
            do_exit(socket_info, sender);
    }

    // double arg commands
    {
        if (!second_token) return PARSE_ERROR;

        // there are no commands that take 3 arguments
        if (strtok(NULL, DELIMITERS)) return PARSE_ERROR;

        if (strcmp(first_token, "get") == 0)
            return do_get(second_token, socket_info, sender);
        else if (strcmp(first_token, "put") == 0)
            return do_put(second_token, socket_info, receiver);
        else if (strcmp(first_token, "delete") == 0)
            return do_delete(second_token, socket_info, sender);
    }

    return PARSE_ERROR;
}

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    socklen_t clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp; /* client host info */
    char buf[BUFSIZE]; /* message buf */
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */

    /*
     * check command line arguments
     */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *) &optval, sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    clientlen = sizeof(clientaddr);
    SocketInfo client_socket_info = {sockfd, (struct sockaddr *) &clientaddr, clientlen};

    RudpReceiver receiver = {};
    RudpSender sender = {};

    /*
     * main loop: wait for a datagram, then echo it
     */
    while (1) {

        /*
         * recvfrom: receive a UDP datagram from a client
         */
        bzero(buf, BUFSIZE);
        // TODO: should we instead receive BUFSIZE-1 since we generally treat the buffer as a string?
        n = rudp_recv(buf, BUFSIZE, &client_socket_info, &receiver);

        if (n < 0)
            error("ERROR in rudp_recv");


        // add zero to end of buffer since we treat it as a string
        if (n < BUFSIZE && n > 0)
            buf[n] = 0;
        else
            buf[BUFSIZE - 1] = 0;

        /*
         * gethostbyaddr: determine who sent the datagram
         */
        hostp = gethostbyaddr((const char *) &clientaddr.sin_addr.s_addr,
                              sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL)
            error("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
        printf("server received datagram from %s (%s)\n",
               hostp->h_name, hostaddrp);
        printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);

        char original_command[BUFSIZE] = {0,};
        strncpy(original_command, buf, BUFSIZE - 1);
        int status = process_message(buf, &client_socket_info, &sender, &receiver);
        if (status < 0) {
            send_error(status, original_command, &client_socket_info, &sender);
        }
    }
}
