#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "server.h"

int main() {
    pthread_t admin_thread, simple_thread, remote_thread;

    // Crearea firelor de execuție
    pthread_create(&admin_thread, NULL, admin_client_handler, NULL);
    pthread_create(&simple_thread, NULL, simple_client_handler, NULL);
    pthread_create(&remote_thread, NULL, remote_client_handler, NULL);

    // Pornirea serverului
    start_server();

    // Așteptarea terminării firelor de execuție
    pthread_join(admin_thread, NULL);
    pthread_join(simple_thread, NULL);
    pthread_join(remote_thread, NULL);

    return 0;
}
