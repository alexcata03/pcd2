#include <stdio.h>
#include <unistd.h>
#include "server.h"

void *admin_client_handler(void *arg) {
    int count = 0;
    while (count < 1) {
        //printf("Admin client handler running\n");
        sleep(10);
        count++;
    }
    return NULL;
}
