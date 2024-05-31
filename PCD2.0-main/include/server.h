#ifndef SERVER_H
#define SERVER_H

#include <time.h>

/* 
    Functions definitions
*/

void *admin_client_handler(void *arg); /* Function that allows one admin to connect to a UNIX socket */
void *simple_client_handler(void *arg); /* Function that allows a inet user to connect to the server  */
void *remote_client_handler(void *arg); /* Function that allows a remote user to connect to the server */

void extract_metadata_xml(const char *filename); /* Function that extracts metadata from an xml file */
void extract_metadata_json(const char *filename); /* Function that extracts metadata from a json file */

void start_server(); /* Function that starts the server */
int authenticate_client(const char *username, const char *password); /* Authentication function for the client */
const char* get_role(const char *username); /* Role checking for the client */

void log_activity(const char *message); /* Function that returns the server log data */
void log_request(const char *client_info, const char *request); /* Function that allows the admin to print the log data*/

void monitor_server();
void update_connection_count(int delta);

void client_handler(void *arg);

/* Structure used for history tracking */
typedef struct {
    char filename[256];
    char change_type[50];
    time_t timestamp;
    char details[1024];
} ChangeLog;

void log_change(const char *filename, const char *change_type, const char *details);

#endif // SERVER_H
