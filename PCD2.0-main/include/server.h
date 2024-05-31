#ifndef SERVER_H
#define SERVER_H

#include <time.h>

void *admin_client_handler(void *arg);
void *simple_client_handler(void *arg);
void *remote_client_handler(void *arg);

void extract_metadata_xml(const char *filename);
void extract_metadata_json(const char *filename);

void start_server();
int authenticate_client(const char *username, const char *password);
const char* get_role(const char *username);

void log_activity(const char *message);
void log_request(const char *client_info, const char *request);

void monitor_server();
void update_connection_count(int delta);

void client_handler(void *arg);

// Structură pentru istoricul modificărilor
typedef struct {
    char filename[256];
    char change_type[50];
    time_t timestamp;
    char details[1024];
} ChangeLog;

void log_change(const char *filename, const char *change_type, const char *details);

#endif // SERVER_H
