//works, DONE

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include <signal.h>

#include "debug.h"
#include "server.h"
#include "client_registry.h"
#include "transaction.h"
#include "store.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "csapp.h"

static void terminate(int status);
CLIENT_REGISTRY *client_registry;
char *input = NULL;

// Function to handle SIGHUP signal
void sighup_handler(int signo) {
    terminate(EXIT_SUCCESS);
}

int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    int pflag = 0;
    int qflag = 0;
    int hflag = 0;
    int portArgcNumber = 0;

    //checks arguments
    for(int i = 0; i < argc; i++){
        if(strcmp(argv[i], "-p") == 0){
            pflag += 1;
            portArgcNumber = i;
        }
        if(strcmp(argv[i], "-q") == 0){
            qflag += 1;
        }
        if(strcmp(argv[i], "-h") == 0){
            hflag += 1;
        }
    }
    // if(argc <)
    if(argc < 3 || pflag != 1 || qflag > 1 || hflag > 1){
        // fprintf(stderr, "no argument");
        exit(EXIT_SUCCESS);
    }


    // Perform required initializations of the client_registry,
    // transaction manager, and object store.
    client_registry = creg_init();
    trans_init();
    store_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function xacto_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    // Set up sigaction for SIGHUP
    struct sigaction sa = {0};
    sa.sa_handler = sighup_handler;
    sa.sa_flags = 0;
    if(sigaction(SIGHUP, &sa, NULL) != 0) exit(EXIT_SUCCESS);

    // Start a server and make some threads
    int server_socket = 0;
    struct sockaddr_storage client_address;
    socklen_t client_address_len;
    pthread_t thread;
    if((server_socket = open_listenfd(argv[portArgcNumber+1])) < 0) exit(EXIT_SUCCESS);

    int *client_socket = NULL;
    while (1) {
        client_address_len = sizeof(struct sockaddr_storage);
        client_socket = Malloc(sizeof(int));
        *client_socket = Accept(server_socket, (SA *)&client_address, &client_address_len);

        // Create a thread to handle the client connection
        Pthread_create(&thread, NULL, xacto_client_service, client_socket);
    }
    terminate(EXIT_SUCCESS);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    debug("7");
    creg_shutdown_all(client_registry);
    debug("6");

    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    debug("4");
    creg_fini(client_registry);
    debug("3");
    trans_fini();
    debug("2");
    store_fini();
    debug("1");

    debug("Xacto server terminating");
    exit(status);
}
