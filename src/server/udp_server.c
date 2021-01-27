/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define MAX_FILES 100

#define DELIMITERS " \n\t\r\v\f"

#define PARSE_ERROR -1
#define NOT_IMPLEMENTED_ERROR -2


/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}

typedef struct {
    int sockfd;
    struct sockaddr* clientaddr;
    socklen_t clientlen;
} SocketInfo;

typedef struct {
    int count;
    char** files;
} Filenames;

void cleanup_filenames(Filenames* filenames) {
    for (int i=0; i < filenames->count; i++) {
        free(filenames->files[i]);
    }
    free(filenames->files);
}

// TODO: do we need to only return files?
// Need to cleanup the allocated memory Filenames when done
Filenames ls_files(char* directory) {
    char** files = malloc(sizeof (char*) * MAX_FILES);
    int i = 0;

    DIR* dir = opendir(directory);

    if (dir == NULL)
        error("Could not open directory");

    struct dirent* entry;
    struct stat entry_info;

    while((entry = readdir(dir)) != NULL) {
        lstat(entry->d_name, &entry_info);
        // only return files
        if (S_ISREG(entry_info.st_mode)) {
            if (i == MAX_FILES)
                error("Too many files in directory to return them all");

            size_t filename_size = entry->d_namlen + 1;
            char* filename = malloc(sizeof(char) * filename_size);
            strncpy(filename, entry->d_name, filename_size-1);
            filename[filename_size-1] = 0;
            files[i] = filename;
            i++;
        }
    }

    return (Filenames) {.count=i, .files=files};
}

int do_send(char* message, const SocketInfo* socket_info) {
    int status = sendto(socket_info->sockfd, message, strlen(message), 0, socket_info->clientaddr,
                        socket_info->clientlen);
    if (status < 0)
        error("ERROR in sendto");

    return status;
}

void send_error(int errno, char* command, const SocketInfo* socket_info) {
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

    do_send(err_buff, socket_info);
}

// Commands, prefixed with do_ to avoid name collisions (e.g. with exit())
int do_get(char* filename, SocketInfo* socket_info) {
    FILE* f = fopen(filename, "r");
    if(f == NULL)
        error("Could not open file for reading");

    char buffer[BUFSIZE] = {0,};
    int ret_code = 0;
    size_t bytes_read = 0;

    while((bytes_read = fread(buffer, sizeof(char), BUFSIZE-1, f)) > 0) {
        buffer[bytes_read] = 0;
        ret_code = do_send(buffer, socket_info);
        // TODO: what should I return for ret_code? Right now it's just tracking the last number of bytes read
        if (ret_code < 0)
            break;
    }

    return ret_code;
}
int do_put(char* filename) { return NOT_IMPLEMENTED_ERROR; }

int do_delete(char* filename, SocketInfo* socket_info) {
    // According to given spec, we should do nothing if the file does not exist
    if (unlink(filename) == 0)
        return do_send("Deleted file\n", socket_info);

    return 0;
}

int do_ls(SocketInfo* socket_info) {
    Filenames filenames = ls_files(".");
    char message[BUFSIZE] = {0,};

    for (int i=0; i < filenames.count; i++) {
        char* filename = filenames.files[i];
        // we check for 2 characters extra to hold a newline character and terminating null character
        if (strlen(filename) + strlen(message) + 2 < BUFSIZE) {
            strcat(message, filename);
            strcat(message, "\n");
        } else {
            error("The filenames are too large to all fit into the buffer");
        }
    }
    int ret_code = do_send(message, socket_info);

    // ls_files() allocates memory that needs to be freed
    cleanup_filenames(&filenames);

    return ret_code;
}

// does not return
void do_exit(SocketInfo* socket_info) {
    char* exit_message = "Exiting gracefully";
    do_send(exit_message, socket_info);

    close(socket_info->sockfd);
    exit(0);
}

int process_message(char* message, SocketInfo* socket_info) {
    char* first_token = strtok(message, DELIMITERS);
    if(!first_token) return PARSE_ERROR;

    char* second_token = strtok(NULL, DELIMITERS);

    // single arg commands
    if(strcmp(first_token, "ls") == 0 || strcmp(first_token, "exit") == 0) {
        // only one argument allowed
        if(second_token) return PARSE_ERROR;

        if(strcmp(first_token, "ls") == 0)
            return do_ls(socket_info);
        else if (strcmp(first_token, "exit") == 0)
            do_exit(socket_info);
    }

    // double arg commands
    {
        if(!second_token) return PARSE_ERROR;

        // there are no commands that take 3 arguments
        if (strtok(NULL, DELIMITERS)) return PARSE_ERROR;

        if(strcmp(first_token, "get") == 0)
            return do_get(second_token, socket_info);
        else if (strcmp(first_token, "put") == 0)
            return do_put(second_token);
        else if (strcmp(first_token, "delete") == 0)
            return do_delete(second_token, socket_info);
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
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  clientlen = sizeof(clientaddr);
  SocketInfo socket_info = {sockfd, (struct sockaddr *) &clientaddr, clientlen};

  /* 
   * main loop: wait for a datagram, then echo it
   */
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    // TODO: should we instead receive BUFSIZE-1 since we generally treat the buffer as a string?
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
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
    strncpy(original_command, buf, BUFSIZE-1);
    int status = process_message(buf, &socket_info);
    if (status < 0) {
        send_error(status, original_command, &socket_info);
    }
  }
}
