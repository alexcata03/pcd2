#include <stdio.h>
#include <unistd.h>
#include "server.h"

void *remote_client_handler(void *arg) {
    int count = 0;
    while (count < 1) { 
        //printf("Remote client handler running\n");
        sleep(10);
        count++;
    }
    return NULL;
}
