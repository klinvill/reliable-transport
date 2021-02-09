# File transfer over UDP

## Background
This repo contains the code for a client and server to reliably transfer files over UDP. In particular, this
implementation provides in-order delivery and retries. It was written as a homework assignment for a Network Systems
class.

The client and server are built on top of two protocols I created for the assignment, RUDP and KFTP. 

### RUDP (Reliable UDP)
RUDP provides reliable transmission of a single message using a stop-and-wait approach where only one message is sent at
a time, and no other messages are sent until that messsage is acknowledged. A sliding window approach would be more
efficient, but a stop-and-wait approach was sufficient for this assignment. 

### KFTP (Kirby's File Transfer Protocol)
KFTP provides file download and upload functionality on top of RUDP. Ideally KFTP should also implement the other
commands supported by the client (ls, delete, exit), however this repo instead just implements those commands using
RUDP to stay closer to the homework instructions (that the client and server should send the commands as a string).

## Code layout
The general directory structure is:
```text
src
├── client
├── common
│   ├── kftp
│   └── reliable_udp
└── server
tests
├── client
├── common
│   ├── kftp
│   └── reliable_udp
├── e2e_utils
├── mocks
├── resources
└── server
out
├── client
├── common
│   ├── kftp
│   └── reliable_udp
├── server
└── tests
```

The three top level directories correspond to the source code (src), unit and end-to-end tests (tests), and build
products (out). The files used by both the client and server reside under the common directory. Most importantly, this
includes the KFTP and RUDP implementations. When the executables are built, they will reside in `out/client/client` and 
`out/server/server`. The unit tests are written in C and produce executables of their own under out/tests. The
end-to-end tests are run in python using pytest.

## Building the code

To build the executables, navigate to the base directory and run `make`. You can cleanup the generated artifacts by
running `make clean`.

## Running the client and server

Once the executables have been generated, you can run the server using the command `out/server/server <port>`. This will
start a server listening on the specified port on all interfaces.

You can run the client using the command `out/client/client <server_host> <server_port>` which will start a client that
will send commands to the specified server host and port.

### Client commands

Once you run the client, it will prompt you to enter one of five different (case-sensitive) commands. The commands are:
- `get <filename>` -- download the specified file from the server
- `put <filename>` -- upload the specified file to the server
- `delete <filename>` -- delete the specified file from the server
- `ls` -- print the names of the files (ignores directories) in the server's local directory
- `exit` -- instruct the server to exit, then close the client

## Notable limitations
There are many limitations for this system (being created for a homework assignment). Some of the more notable
limitations include:

- RUDP does not provide a connection establishment or teardown. The consequence of this is that the server really only
    works well with one client since the sequence number and ack counters are never reset. Similarly, a client should 
    only be used to contact at most one server.

## Running the tests

The tests can be collectively built and run using the command `make test`. The python tests rely on pytest and
pytest-asyncio which can be installed using the command `pip install -r requirements.txt`. The unit tests require the
check and cmocka libraries which can be installed through your package manager (`sudo apt-get install check` and
`sudo apt-get install libcmocka-dev` on ubuntu).
