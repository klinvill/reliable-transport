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

#define BUFSIZE 1024

#define DELIMITERS " "

#define PARSE_ERROR -1
#define NOT_IMPLEMENTED_ERROR -2

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

void do_send(char* message, int sockfd, struct sockaddr* clientaddr, int clientlen) {
    int status = sendto(sockfd, message, strlen(message), 0, clientaddr, clientlen);
    if (status < 0)
        error("ERROR in sendto");
}

void send_error(int errno, char* command, int sockfd, struct sockaddr* clientaddr, int clientlen) {
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

    do_send(err_buff, sockfd, clientaddr, clientlen);
}

// Commands, prefixed with do_ to avoid name collisions (e.g. with exit())
int do_get(char* filename) { return NOT_IMPLEMENTED_ERROR; };
int do_put(char* filename) { return NOT_IMPLEMENTED_ERROR; };
int do_delete(char* filename) { return NOT_IMPLEMENTED_ERROR; };
int do_ls() { return NOT_IMPLEMENTED_ERROR; };
int do_exit() { return NOT_IMPLEMENTED_ERROR; };

int process_message(char* message) {
    char* first_token = strtok(message, DELIMITERS);
    if(!first_token) return PARSE_ERROR;

    char* second_token = strtok(NULL, DELIMITERS);

    // single arg commands
    if(strcmp(first_token, "ls") == 0 || strcmp(first_token, "exit") == 0) {
        // only one argument allowed
        if(second_token) return PARSE_ERROR;

        if(strcmp(first_token, "ls") == 0)
            return do_ls();
        else if (strcmp(first_token, "exit") == 0)
            return do_exit();
    }

    // double arg commands
    {
        if(!second_token) return PARSE_ERROR;

        // there are no commands that take 3 arguments
        if (strtok(NULL, DELIMITERS)) return PARSE_ERROR;

        if(strcmp(first_token, "get") == 0)
            return do_get(second_token);
        else if (strcmp(first_token, "put") == 0)
            return do_put(second_token);
        else if (strcmp(first_token, "delete") == 0)
            return do_delete(second_token);
    }

    return PARSE_ERROR;
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
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

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
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
    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);

    char original_command[BUFSIZE] = {0,};
    strncpy(original_command, buf, BUFSIZE-1);
    int status = process_message(buf);
    if (status < 0) {
        send_error(status, original_command, sockfd, (struct sockaddr *) &clientaddr, clientlen);
    }
  }
}
