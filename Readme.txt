Project: Multi-Client Messaging Server
Authors:

    GROUP PROJECT 2 students

Description

This project implements a multi-client messaging server in C. The server can handle multiple clients simultaneously using threads and allows clients to send and receive messages. Clients can execute various commands such as echo, rand, time, list, nick, send, rcv, and quit.
Features

    Multi-threaded Server: Handles multiple clients using threads.
    Client Nicknames: Clients can set and use nicknames.
    Messaging: Clients can send and receive messages.
    Command Execution: Supports commands like echo, rand, time, list, nick, send, rcv, and quit.
    Mutex Lock: Ensures thread safety with a mutex lock for shared resources.

Files
    client_telenet.c
    server.c

Setup

    Compile the Server:
        gcc -c server.c -o serveur


Run the Server:
    ./server <port_number>

Commands

    echo <message>: Echoes the message back to the client.
    rand [<n>]: Returns a random number. If n is provided, returns a random number between 0 and n-1.
    time: Returns the current server time.
    list: Lists all connected clients.
    nick <nickname>: Sets the client's nickname.
    send <nickname> <message>: Sends a message to the specified client.
    rcv: Receives the next message in the client's mailbox.
    quit: Disconnects the client from the server.

Implementation Details
Global Variables

    int nr_clients: Tracks the number of connected clients.
    pthread_mutex_t lock: Mutex lock to protect shared resources.
    client_data *first, *last: Pointers to the first and last clients in the list.

Structures

    msg: Represents a message.
    mbox: Represents a mailbox containing messages.
    client_data: Stores client information and its mailbox.

Functions

    init_clients(): Initializes the client counter and client list.
    alloc_client(int sock): Allocates memory for a new client.
    free_client(client_data* cli): Frees memory allocated for a client.
    worker(void* arg): Handles client communication.
    client_arrived(int sock): Handles new client connections.
    listen_port(int port_num): Listens for incoming client connections.
    get_question(int sock, char *buf, int *plen, char* question): Reads a message from the client.
    eval_quest(char *question, char *resp, client_data *cli): Evaluates a client command.
    write_full(int fd, char *buf, int len): Sends a complete message to the client.
    do_rand (char* args, char* resp): Handles the rand command.
    do_echo (char* args, char* resp): Handles the echo command.
    do_quit (char* args, char* resp): Handles the quit command.
    do_time (char* args, char* resp): Handles the time command.
    do_list(char* args, char* resp): Handles the list command.
    create_msg(char* author, char* contents): Creates a new message.
    init_mbox(mbox* box): Initializes a mailbox.
    put(mbox* box, msg* mess): Adds a message to the mailbox.
    destroy_msg(msg* mess): Frees memory allocated for a message.
    get(mbox* box): Retrieves a message from the mailbox.
    send_mess(mbox* box, char* author, char* contents): Sends a message.
    show_msg(msg* mess, char* resp): Displays a message.
    recieve_mess(mbox* box , char* resp): Receives a message.
    valid_nick(char* resp, char* nick): Validates a nickname.
    search_client(char* nick): Searches for a client by nickname.
    do_nick(char* args, char* resp, client_data *cli): Handles the nick command.
    do_send(char* args, char* resp, client_data *receiver, client_data *sender): Handles the send command.
    do_rcv(char* args, char* resp, client_data *cli): Handles the rcv command.


Client-Server Communication Program

This program demonstrates a simple client-server communication using sockets in C. The client connects to a server, sends messages, and receives responses.

Compilation
     gcc -c client_telnet.c -o client

usage
    client localhost <port_number>
    //same port number as the one on which the server is running
