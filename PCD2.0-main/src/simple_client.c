#include <stdio.h>
#include <unistd.h>
#include "server.h"

void *simple_client_handler(void *arg) {
    int count = 0;
    while (count < 1) {
       // printf("Simple client handler running\n");
        extract_metadata_json("example.json");
        sleep(10);
        count++;
    }
    return NULL;
}