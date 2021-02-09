//
// Server for simple reliable file transfer over UDP
//
// Usage: server <port>
//
// This server uses RUDP (Reliable UDP) and KFTP (Kirby's File Transfer Protocol) to provide this functionality. This
// work was done as a homework assignment for a networking class.
//
// Limitations:
//  - The server is single-threaded
//  - The server only expects at most one connection (it never resets tracked sequence numbers)
//
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

// max number of files that can be listed using the ls command
// TODO: if we implement ls through KFTP to support an arbitrary response size, do we still need this MAX_FILES limit?
#define MAX_FILES 100

// Delimiters to use when extracting commands and arguments from user-supplied input
// TODO: should unify client and server command parsing
#define DELIMITERS " \n\t\r\v\f"

// TODO: standardize error codes between client and server
#define PARSE_ERROR (-2)
#define NOT_IMPLEMENTED_ERROR (-3)


// wrapper around perror for errors that should cause the program to terminate with a negative return code
void fatal_error(char *msg) {
    perror(msg);
    exit(-1);
}


// Collection of filenames, created by the ls_files() function
typedef struct {
    int count;
    char **files;
} Filenames;


// Cleans up the dynamically allocated memory for Filenames.
//
// This memory is typically allocated in the ls_files() function.
void cleanup_filenames(Filenames *filenames) {
    for (int i = 0; i < filenames->count; i++) {
        free(filenames->files[i]);
    }
    free(filenames->files);
}


// TODO: do we need to only return files?
// Returns the files in a given directory up to MAX_FILES number of files.
//
// This function dynamically allocates the memory required to hold the names of all the files. This memory should be
// cleaned up using the cleanup_filenames() function.
Filenames ls_files(char *directory) {
    char **files = malloc(sizeof(char *) * MAX_FILES);
    int i = 0;

    DIR *dir = opendir(directory);

    if (dir == NULL) {
        perror("Could not open directory");
        free(files);
        return (Filenames) {.count=0, .files=NULL};
    }

    struct dirent *entry;
    struct stat entry_info;

    while ((entry = readdir(dir)) != NULL) {
        stat(entry->d_name, &entry_info);
        // only return files
        if (S_ISREG(entry_info.st_mode)) {
            if (i == MAX_FILES) {
                fprintf(stderr, "ERROR in ls_files: Too many files in directory to return them all\n");
                break;
            }

            size_t filename_size = strlen(entry->d_name) + 1;
            char *filename = malloc(sizeof(char) * filename_size);
            strncpy(filename, entry->d_name, filename_size - 1);
            // makes sure the filename is null terminated
            filename[filename_size - 1] = 0;
            files[i] = filename;
            i++;
        }
    }

    return (Filenames) {.count=i, .files=files};
}

// TODO: which parameters (for all the functions) should be const?
// Sends a message to the client
//
// The message is expected to be a string
int do_send(char *message, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    int status = rudp_send(message, strlen(message), socket_info, sender, receiver);
    if (status < 0) {
        perror("ERROR in rudp_send");
        return status;
    }

    return status;
}


// Sends an error message back to the client
void send_error(int errno, char *command, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
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

    do_send(err_buff, socket_info, sender, receiver);
}


// Handles `get` command, that transfers a file from the server to the client
int do_get(char *filename, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        perror("Could not open file for reading");
        return -1;
    }

    int result = kftp_send_file(f, socket_info, sender, receiver);
    fclose(f);
    return result;
}


// Handles `put` command, that transfers a file from the client to the server
int do_put(char *filename, SocketInfo *socket_info, RudpReceiver *receiver) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        perror("Could not open file for reading");
        return -1;
    }

    int result = kftp_recv_file(f, socket_info, receiver);
    fclose(f);
    return result;
}


// Handles `delete` command, that deletes a file from the server
int do_delete(char *filename, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    // According to given spec, we should do nothing if the file does not exist
    if (unlink(filename) == 0)
        return do_send("Deleted file\n", socket_info, sender, receiver);

    return 0;
}


// Handles `ls` command, that lists files in the current directory on the server
//
// Sends the list of files in the current directory back to the client, separated by a newline character
int do_ls(SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    Filenames filenames = ls_files(".");
    char message[BUFSIZE] = {0,};

    if (filenames.files == NULL) {
        perror("ERROR in ls_files");
        return -1;
    }

    for (int i = 0; i < filenames.count; i++) {
        char *filename = filenames.files[i];
        // we check for 2 characters extra to hold a newline character and terminating null character
        if (strlen(filename) + strlen(message) + 2 < BUFSIZE) {
            strcat(message, filename);
            strcat(message, "\n");
        } else {
            fprintf(stderr, "ERROR: the filenames from ls are too large to all fit into the buffer\n");
            return -1;
        }
    }
    int ret_code = do_send(message, socket_info, sender, receiver);

    // ls_files() allocates memory that needs to be freed
    cleanup_filenames(&filenames);

    return ret_code;
}


// Handles `exit` command. Does not return.
void do_exit(SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    char *exit_message = "Exiting gracefully";
    do_send(exit_message, socket_info, sender, receiver);

    close(socket_info->sockfd);
    exit(0);
}


// Executes the proper processing based on the given command.
//
// This function uses strtok which will mutate the message argument.
int process_message(char *message, SocketInfo *socket_info, RudpSender *sender, RudpReceiver *receiver) {
    // TODO: unify command parsing with client implementation
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
            return do_put(second_token, socket_info, receiver);
        else if (strcmp(first_token, "delete") == 0)
            return do_delete(second_token, socket_info, sender, receiver);
    }

    // unrecognized command
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
        fatal_error("ERROR opening socket");

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
    memset((char *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr,
             sizeof(serveraddr)) < 0)
        fatal_error("ERROR on binding");

    clientlen = sizeof(clientaddr);
    SocketInfo client_socket_info = {sockfd, (struct sockaddr *) &clientaddr, clientlen};

    RudpReceiver receiver = {};
    RudpSender sender = {.sender_timeout=SENDER_TIMEOUT, .message_timeout=INITIAL_TIMEOUT};

    /*
     * main loop: wait for a datagram, then echo it
     */
    while (1) {

        // receive a command from the client
        memset(buf, 0, BUFSIZE);
        // we receive BUFSIZE-1 bytes instead of BUFSIZE since we treat the buffer as a string that needs to be
        // null-terminated
        n = rudp_recv(buf, BUFSIZE-1, &client_socket_info, &receiver);

        if (n < 0) {
            perror("ERROR in rudp_recv");
            continue;
        }


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
        if (hostp == NULL) {
            perror("ERROR on gethostbyaddr");
            continue;
        }
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL) {
            perror("ERROR on inet_ntoa\n");
            continue;
        }
        printf("server received datagram from %s (%s)\n",
               hostp->h_name, hostaddrp);
        printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);

        // we keep a copy of the original command since our requirements state "For any other commands, the server
        // should simply repeat the command back to the client with no modification, stating that the given command was
        // not understood"
        char original_command[BUFSIZE] = {0,};
        strncpy(original_command, buf, BUFSIZE - 1);
        int status = process_message(buf, &client_socket_info, &sender, &receiver);
        if (status < 0) {
            // send error message back to the client
            send_error(status, original_command, &client_socket_info, &sender, &receiver);
        }
    }
}
