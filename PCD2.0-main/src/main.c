#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "server.h"

int main() {
    pthread_t admin_thread, simple_thread, remote_thread;

    /* Create the execution threads */
    pthread_create(&admin_thread, NULL, admin_client_handler, NULL); /* Handles the admin client connection */
    pthread_create(&simple_thread, NULL, simple_client_handler, NULL); /* Handles the inet client  connection */
    pthread_create(&remote_thread, NULL, remote_client_handler, NULL); /* Handles the remote client connection */

    /* Starts the server */
    start_server();

    /* Waits for threads to finish their execution */
    pthread_join(admin_thread, NULL);
    pthread_join(simple_thread, NULL);
    pthread_join(remote_thread, NULL);

    return 0;
}
