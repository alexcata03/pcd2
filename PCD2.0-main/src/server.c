#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <pthread.h>
#include "threadpool.h"
#include <cjson/cJSON.h>
#include <dirent.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include "json.h"
#include <time.h>
#include <asm-generic/socket.h>

#define MAX_BUFFER_LENGTH 1024 
#define PORT 8080
#define BUFFER_SIZE 1024 
#define THREAD_COUNT 5
#define QUEUE_SIZE 10
#define MAX_BLOCKED_USERS 100
#define MAX_CLIENTS 100

pthread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t admin_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t blocked_users_mutex = PTHREAD_MUTEX_INITIALIZER;
int connection_count = 0;
int active_admins = 0;
int client_sockets[MAX_CLIENTS];
char client_usernames[MAX_CLIENTS][50];
int client_count = 0;

char *blocked_users[MAX_BLOCKED_USERS];
int blocked_count = 0;

// Trim newline characters from a string
void trim_newline(char *str) {
    char *pos;
    if ((pos = strchr(str, '\n')) != NULL) *pos = '\0';
    if ((pos = strchr(str, '\r')) != NULL) *pos = '\0';
}
void create_log_file(const char *filename, const char *log_message) {
    char log_filename[MAX_BUFFER_LENGTH];
    char *extension_position = strrchr(filename, '.'); // Find last occurrence of '.'

    // Remove file extension if it exists
    if (extension_position != NULL) {
        *extension_position = '\0'; // Replace '.' with '\0' to truncate the string
    }

    snprintf(log_filename, sizeof(log_filename), "%s.log", filename); // Add .log extension

    FILE *log_file = fopen(log_filename, "a"); /* Oppen the file in appending mode */
    if (log_file == NULL) { /* If the file could not be oppened print error message and exit */
        perror("Could not open log file");
        return;
    }

    time_t now = time(NULL); /* Start the timer */
    fprintf(log_file, "[%s] %s\n", ctime(&now), log_message);
    fclose(log_file); /* Close the file */
}
// Load and validate XML file
int load_and_validate_xml(const char *path, XMLDocument *document) {
    if (!ends_with(path, ".xml")) { /* Check for .xml */
        fprintf(stderr, "Ok\n");
        return 0;
    }

    if (!XMLDocument_load(document, path)) {
        fprintf(stderr, "Succes\n"); 
        return 0;
    }

    return 1;
}

// Convert XML to JSON and save to file
void convert_xml_to_json(const char *xml_path, const char *json_path) {
    XMLDocument document;
    if (!load_and_validate_xml(xml_path, &document)) {
        fprintf(stderr, "Invalid XML file.\n");
        return;
    }

    cJSON *json = XMLDocumentToJSON(&document);
    SaveJSONToFile(json_path, json);

    cJSON_Delete(json);
    XMLDocument_free(&document);

    // Log changes to the JSON file
    char json_log_path[BUFFER_SIZE];
    char *json_extension_position = strrchr(json_path, '.'); // Find last occurrence of '.'
    if (json_extension_position != NULL) {
        *json_extension_position = '\0'; // Remove file extension
    }
    snprintf(json_log_path, sizeof(json_log_path), "%s.log", json_path); // Add .log extension

    FILE *json_log_file = fopen(json_log_path, "a"); /* Open in appending mode */
    if (json_log_file) { /* If the file was opened, write in the file and close it */
        fprintf(json_log_file, "Converted XML file '%s' to JSON file '%s'\n", xml_path, json_path);
        fclose(json_log_file);
    }
}


// Extract metadata from a file and send it to the client
void extract_metadata(const char *filename, int client_socket) {
    char buffer[BUFFER_SIZE];
    FILE *file = fopen(filename, "rb");
    if (file) {
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            send(client_socket, buffer, n, 0);
        }
        fclose(file);
        // Log extraction
        char log_path[BUFFER_SIZE];
        char *extension_position = strrchr(filename, '.'); // Find last occurrence of '.'
        if (extension_position != NULL) {
            *extension_position = '\0'; // Remove file extension
        }
        snprintf(log_path, sizeof(log_path), "%s.log", filename); // Add .log extension

        FILE *log_file = fopen(log_path, "a");
        if (log_file) {
            fprintf(log_file, "Metadata extracted from file '%s'\n", filename);
            fclose(log_file);
        }
    } else {
        send(client_socket, "Failed to open file.\n", strlen("Failed to open file.\n"), 0);
        perror("Failed to open file");
    }
}
//Search json path
void search_and_print_json(const char *filename, const char *json_path, int client_socket) {
    FILE *file = fopen(filename, "rb"); /* Open the file in binary reading mode */
    if (!file) { /* If the file could not be oppened, print an error message, send it to the client and exit */
        char error_msg[] = "Failed to open file.\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
        perror("Failed to open file");
        return;
    }

    // Read the entire file into a string
    fseek(file, 0, SEEK_END);
    long length = ftell(file); /* Get teh file size */
    fseek(file, 0, SEEK_SET);
    char *json_string = (char *)malloc(length + 1); /* Allocate memory to the buffer */
    if (!json_string) { /* If the buffer is null , print an error message and exit */
        fclose(file);
        char error_msg[] = "Memory allocation failed\n";
        send(client_socket, error_msg, strlen(error_msg), 0); /* Send the error message to the client */
        perror("Memory allocation failed");
        return;
    }
    fread(json_string, 1, length, file); /* Read the data from the file into the buffer  */
    fclose(file); /* Close the file*/
    json_string[length] = '\0'; /* Set the ending of the string */

    // Parse JSON
    cJSON *json = cJSON_Parse(json_string);
    free(json_string);
    if (!json) {
        char error_msg[] = "Error parsing JSON\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
        perror("Error parsing JSON");
        return;
    }

    // Search JSON based on the provided path
    cJSON *current = json;
    char *token = strtok((char *)json_path, "."); /* Tokenize the string */
    while (token != NULL) { /* Parse while the token is not null */
        char *index_str = strchr(token, '[');
        if (index_str != NULL) {
            *index_str = '\0'; /* Set the ending of the string */
            int index = atoi(index_str + 1); /* Get teh index */
            cJSON *array = cJSON_GetObjectItemCaseSensitive(current, token);
            if (!array || !cJSON_IsArray(array)) { /* If the array is null, send the error message to the client and exit */
                char error_msg[] = "Path not found\n";
                send(client_socket, error_msg, strlen(error_msg), 0);
                perror("Path not found");
                cJSON_Delete(json);
                return;
            }
            current = cJSON_GetArrayItem(array, index); /* Get the item at the position index from the array */
        } else {
            current = cJSON_GetObjectItemCaseSensitive(current, token); /* Get the object from the token */
        }

        if (!current) { /* IF the buffer is null, send an error message and exit */
            char error_msg[] = "Path not found\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
            perror("Path not found");
            cJSON_Delete(json);
            return;
        }
        token = strtok(NULL, ".");
    }

    // Print the result
    char *result = cJSON_Print(current);
    send(client_socket, result, strlen(result), 0);
    free(result);

    cJSON_Delete(json);
}
// //Search xml path
// void search_and_print_xpath(const char *filename, const char *xpathExpr, int client_socket) {
//     FILE *file = fopen(filename, "rb");
//     if (!file) {
//         char error_msg[] = "Failed to open file.\n";
//         send(client_socket, error_msg, strlen(error_msg), 0);
//         perror("Failed to open file");
//         return;
//     }

//     // Read the entire file into a string
//     fseek(file, 0, SEEK_END);
//     long length = ftell(file);
//     fseek(file, 0, SEEK_SET);
//     char *xml_string = (char *)malloc(length + 1);
//     if (!xml_string) {
//         fclose(file);
//         char error_msg[] = "Memory allocation failed\n";
//         send(client_socket, error_msg, strlen(error_msg), 0);
//         perror("Memory allocation failed");
//         return;
//     }
//     fread(xml_string, 1, length, file);
//     fclose(file);
//     xml_string[length] = '\0';

//     // Parse XML
//     xmlDocPtr doc = xmlReadMemory(xml_string, length, "noname.xml", NULL, 0);
//     free(xml_string);
//     if (doc == NULL) {
//         char error_msg[] = "Error parsing XML\n";
//         send(client_socket, error_msg, strlen(error_msg), 0);
//         perror("Error parsing XML");
//         return;
//     }

//     // Perform XPath search
//     xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
//     xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar *)xpathExpr, xpathCtx);

//     if (xpathObj == NULL) {
//         char error_msg[] = "Error in XPath expression\n";
//         send(client_socket, error_msg, strlen(error_msg), 0);
//         perror("Error in XPath expression");
//         xmlXPathFreeContext(xpathCtx);
//         xmlFreeDoc(doc);
//         xmlCleanupParser();
//         return;
//     }

//     xmlNodeSetPtr nodes = xpathObj->nodesetval;
//     if (nodes->nodeNr == 0) {
//         char error_msg[] = "No results found for XPath\n";
//         send(client_socket, error_msg, strlen(error_msg), 0);
//     } else {
//         for (int i = 0; i < nodes->nodeNr; ++i) {
//             xmlChar *content = xmlNodeGetContent(nodes->nodeTab[i]);
//             send(client_socket, content, strlen(content), 0);
//             xmlFree(content);
//         }
//     }

//     xmlXPathFreeObject(xpathObj);
//     xmlXPathFreeContext(xpathCtx);
//     xmlFreeDoc(doc);
//     xmlCleanupParser();
// }


// Authentication function
int authenticate_client(const char *username, const char *password) {
    if ((strcmp(username, "admin") == 0 && strcmp(password, "adminpass") == 0) ||
        (strcmp(username, "simple") == 0 && strcmp(password, "simplepass") == 0) ||
        (strcmp(username, "remote") == 0 && strcmp(password, "remotepass") == 0)) {
        return 1;
    }
    return 0;
}

// Function to get the role of the user
const char* get_role(const char *username) {
    if (strcmp(username, "admin") == 0) {
        return "admin";
    } else if (strcmp(username, "simple") == 0) {
        return "simple";
    } else if (strcmp(username, "remote") == 0) {
        return "remote";
    }
    return "unknown";
}

// Function to log activity
void log_activity(const char *message) {
    FILE *log_file = fopen("server.log", "a");
    if (log_file == NULL) {
        perror("Could not open server log file");
        return;
    }
    time_t now = time(NULL);
    fprintf(log_file, "[%s] %s\n", ctime(&now), message);
    fclose(log_file);
}

// Function to update connection count
void update_connection_count(int delta) {
    pthread_mutex_lock(&connection_mutex);
    connection_count += delta;
    printf("Current connection count: %d\n", connection_count);
    pthread_mutex_unlock(&connection_mutex);
}

// Function to monitor the server
void monitor_server() {
    while (1) {
        printf("Monitoring server...\n");
        sleep(10);
    }
}


void upload_metadata(const char *filename, const char *metadata) {
    char abs_path[BUFFER_SIZE];
    realpath(filename, abs_path);
    FILE *file = fopen(abs_path, "w");
    if (file) {
        fprintf(file, "%s", metadata);
        fclose(file);
        log_activity("Metadata uploaded and file created.");
    } else {
        perror("Failed to upload metadata");
    }
}

// Function to check if a file exists
int file_exists(const char *filename) {
    char abs_path[BUFFER_SIZE];
    realpath(filename, abs_path);
    FILE *file = fopen(abs_path, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

// Function to edit a file
void edit_file(const char *filename, int client_socket) {
    char abs_path[BUFFER_SIZE];
    realpath(filename, abs_path);

    // Send current content to client
    char buffer[BUFFER_SIZE];
    FILE *file = fopen(abs_path, "r");
    if (file) {
        send(client_socket, "Current content:\n", strlen("Current content:\n"), 0);
        while (fgets(buffer, sizeof(buffer), file)) {
            send(client_socket, buffer, strlen(buffer), 0);
        }
        fclose(file);
    } else {
        send(client_socket, "Failed to open file.\n", strlen("Failed to open file.\n"), 0);
        perror("Failed to open file");
        return;
    }

    send(client_socket, "\nEnter 'add <text>', 'delete <line number>', 'replace <line number> <new text>', or 'save' to save changes.\n", strlen("\nEnter 'add <text>', 'delete <line number>', 'replace <line number> <new text>', or 'save' to save changes.\n"), 0);

    // Start editing loop
    char edit_buffer[BUFFER_SIZE * 10] = {0};
    char current_file_content[BUFFER_SIZE * 10] = {0};
    
    // Read file content for later modifications
    file = fopen(abs_path, "r");
    if (file) {
        while (fgets(buffer, sizeof(buffer), file)) {
            strncat(current_file_content, buffer, sizeof(current_file_content) - strlen(current_file_content) - 1);
        }
        fclose(file);
    } else {
        send(client_socket, "Failed to open file for reading.\n", strlen("Failed to open file for reading.\n"), 0);
        perror("Failed to open file for reading");
        return;
    }

    strcpy(edit_buffer, current_file_content);

    int bytes_read;
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) break;
        buffer[bytes_read] = '\0';
        trim_newline(buffer);

        if (strncmp(buffer, "add ", 4) == 0) {
            // Add text to a new line
            strncat(edit_buffer, buffer + 4, sizeof(edit_buffer) - strlen(edit_buffer) - 1);
            strncat(edit_buffer, "\n", sizeof(edit_buffer) - strlen(edit_buffer) - 1);
        } else if (strncmp(buffer, "delete ", 7) == 0) {
            // Delete specified line
            int line_number = atoi(buffer + 7);
            if (line_number > 0) {
                char *line = strtok(current_file_content, "\n");
                int current_line = 1;
                char new_buffer[BUFFER_SIZE * 10] = {0};
                while (line) {
                    if (current_line != line_number) {
                        strncat(new_buffer, line, sizeof(new_buffer) - strlen(new_buffer) - 1);
                        strncat(new_buffer, "\n", sizeof(new_buffer) - strlen(new_buffer) - 1);
                    }
                    line = strtok(NULL, "\n");
                    current_line++;
                }
                strcpy(edit_buffer, new_buffer);
                strcpy(current_file_content, new_buffer);
            }
        } else if (strncmp(buffer, "replace ", 8) == 0) {
            // Replace text in specified line with new text
            int line_number;
            char new_text[BUFFER_SIZE];
            sscanf(buffer, "replace %d %[^\n]", &line_number, new_text);
            if (line_number > 0) {
                char *line = strtok(current_file_content, "\n");
                int current_line = 1;
                char new_buffer[BUFFER_SIZE * 10] = {0};
                while (line) {
                    if (current_line == line_number) {
                        strncat(new_buffer, new_text, sizeof(new_buffer) - strlen(new_buffer) - 1);
                    } else {
                        strncat(new_buffer, line, sizeof(new_buffer) - strlen(new_buffer) - 1);
                    }
                    strncat(new_buffer, "\n", sizeof(new_buffer) - strlen(new_buffer) - 1);
                    line = strtok(NULL, "\n");
                    current_line++;
                }
                strcpy(edit_buffer, new_buffer);
                strcpy(current_file_content, new_buffer);
            }
        } else if (strcmp(buffer, "save") == 0) {
            // Save new content and exit editing
            file = fopen(abs_path, "w");
            if (file) {
                fwrite(edit_buffer, 1, strlen(edit_buffer), file);
                fclose(file);
                send(client_socket, "File saved and updated.\n", strlen("File saved and updated.\n"), 0);
            } else {
                send(client_socket, "Failed to save file.\n", strlen("Failed to save file.\n"), 0);
                perror("Failed to save file");
            }
            break;
        } else {
            send(client_socket, "Unknown command. Use 'add', 'delete', 'replace', or 'save'.\n", strlen("Unknown command. Use 'add', 'delete', 'replace', or 'save'.\n"), 0);
        }
    }
}

// Handle error messages
void handle_error(const char *abs_path, int client_socket, const char *format) {
    char error_message[BUFFER_SIZE];
    int len;

    // Try to format the error message
    len = snprintf(error_message, sizeof(error_message), format, abs_path, strerror(errno));

    // Check if the message was truncated
    if (len >= sizeof(error_message)) {
        // Calculate remaining space in the buffer
        size_t remaining_space = sizeof(error_message) - strlen(error_message) - 1;

        // Add a truncated message warning
        strncat(error_message, "... (message truncated)\n", remaining_space);
    }

    // Send the error message to the client
    send(client_socket, error_message, strlen(error_message), 0);
}

// Function to delete a file or directory
void delete_file_or_directory(const char *path, int client_socket) {
    char abs_path[BUFFER_SIZE];
    realpath(path, abs_path);

    struct stat st;
    if (stat(abs_path, &st) == -1) {
        handle_error(abs_path, client_socket, "Failed to access %s: %s\n");
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        // If directory, delete recursively
        DIR *d = opendir(abs_path);
        if (d) {
            struct dirent *dir;
            while ((dir = readdir(d)) != NULL) {
                if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                    continue;
                }
                char sub_path[BUFFER_SIZE];
                if (snprintf(sub_path, sizeof(sub_path), "%s/%s", abs_path, dir->d_name) >= sizeof(sub_path)) {
                    handle_error(abs_path, client_socket, "Path too long: %s/%s\n");
                    continue;
                }
                delete_file_or_directory(sub_path, client_socket);
            }
            closedir(d);
            if (rmdir(abs_path) == 0) {
                send(client_socket, "Directory deleted.\n", strlen("Directory deleted.\n"), 0);
                log_activity("Directory deleted.");
            } else {
                handle_error(abs_path, client_socket, "Failed to delete directory %s: %s\n");
            }
        }
    } else {
    // If file, delete it
    if (remove(abs_path) == 0) {
        // Prepare to delete the log file
        char log_filename[MAX_BUFFER_LENGTH + 64];
        char *extension_position = strrchr(abs_path, '.'); // Find last occurrence of '.'

        // Create a copy of the abs_path to manipulate the string safely
        char abs_path_without_ext[MAX_BUFFER_LENGTH];
        strncpy(abs_path_without_ext, abs_path, extension_position - abs_path);
        abs_path_without_ext[extension_position - abs_path] = '\0';

        snprintf(log_filename, sizeof(log_filename), "%s.log", abs_path_without_ext); // Add .log extension

        // Attempt to delete the log file
        if (remove(log_filename) == 0) {
            log_activity("File and corresponding log file deleted.");
            send(client_socket, "File and corresponding log file deleted.\n", strlen("File and corresponding log file deleted.\n"), 0);
        } else {
            log_activity("File deleted, but failed to delete corresponding log file.");
            send(client_socket, "File deleted, but failed to delete corresponding log file.\n", strlen("File deleted, but failed to delete corresponding log file.\n"), 0);
        }
    } else {
        handle_error(abs_path, client_socket, "Failed to delete file %s: %s\n");
    }
}

}

// List files and directories in a directory
void list_directory(const char *dirname, int client_socket) {
    DIR *d;
    struct dirent *dir;
    char buffer[BUFFER_SIZE];

    d = opendir(dirname);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Exclude . and ..
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                snprintf(buffer, sizeof(buffer), "%s\n", dir->d_name);
                send(client_socket, buffer, strlen(buffer), 0);
            }
        }
        closedir(d);
    } else {
        send(client_socket, "Failed to open directory.\n", strlen("Failed to open directory.\n"), 0);
    }
}

// List files and directories in a specific directory
void list_directory_contents(const char *dirname, int client_socket) {
    DIR *d;
    struct dirent *dir;
    char buffer[BUFFER_SIZE];

    d = opendir(dirname);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Exclude . and ..
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                snprintf(buffer, sizeof(buffer), "%s\n", dir->d_name);
                send(client_socket, buffer, strlen(buffer), 0);
            }
        }
        closedir(d);
    } else {
        snprintf(buffer, sizeof(buffer), "Failed to open directory: %s\n", dirname);
        send(client_socket, buffer, strlen(buffer), 0);
    }
}

// Check if user is blocked
bool is_user_blocked(const char *username) {
    pthread_mutex_lock(&blocked_users_mutex);
    for (int i = 0; i < blocked_count; i++) {
        if (strcmp(blocked_users[i], username) == 0) {
            pthread_mutex_unlock(&blocked_users_mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&blocked_users_mutex);
    return false;
}

// Block a user
void block_user(const char *username) {
    if (strcmp(username, "admin") == 0) {
        printf("Attempt to block admin ignored.\n");
        return;
    }
    pthread_mutex_lock(&blocked_users_mutex);
    if (blocked_count < MAX_BLOCKED_USERS) {
        blocked_users[blocked_count++] = strdup(username);
        char log_message[BUFFER_SIZE];
        snprintf(log_message, sizeof(log_message), "User %s has been blocked.", username);
        log_activity(log_message);
        pthread_mutex_unlock(&blocked_users_mutex);

        // Disconnect the blocked user if they are currently connected
        pthread_mutex_lock(&connection_mutex);
        for (int i = 0; i < client_count; i++) {
            if (strcmp(client_usernames[i], username) == 0) {
                int client_socket = client_sockets[i];
                char buffer[BUFFER_SIZE] = "You have been blocked by the administrator. Disconnecting...\n";
                send(client_socket, buffer, strlen(buffer), 0);
                close(client_socket);
                client_sockets[i] = client_sockets[--client_count];
                break;
            }
        }
        pthread_mutex_unlock(&connection_mutex);
    } else {
        pthread_mutex_unlock(&blocked_users_mutex);
    }
}

// Unblock a user
void unblock_user(const char *username) {
    pthread_mutex_lock(&blocked_users_mutex);
    for (int i = 0; i < blocked_count; i++) {
        if (strcmp(blocked_users[i], username) == 0) {
            free(blocked_users[i]);
            blocked_users[i] = blocked_users[--blocked_count];
            char log_message[BUFFER_SIZE];
            snprintf(log_message, sizeof(log_message), "User %s has been unblocked.", username);
            log_activity(log_message);
            pthread_mutex_unlock(&blocked_users_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&blocked_users_mutex);
}

// List connected users
void list_connected_users(int client_socket) {
    char buffer[BUFFER_SIZE];
    pthread_mutex_lock(&connection_mutex);
    for (int i = 0; i < client_count; i++) {
        snprintf(buffer, sizeof(buffer), "User: %s, Socket: %d\n", client_usernames[i], client_sockets[i]);
        send(client_socket, buffer, strlen(buffer), 0);
    }
    pthread_mutex_unlock(&connection_mutex);
}

// Handle client connection
void client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int bytes_read;
    char username[50];

    // Authentication
    send(client_socket, "Username: ", strlen("Username: "), 0);
    bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';
    trim_newline(buffer);
    strncpy(username, buffer, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';

    if (is_user_blocked(username)) {
        send(client_socket, "You are blocked from the server.\n", strlen("You are blocked from the server.\n"), 0);
        close(client_socket);
        return;
    }

    send(client_socket, "Password: ", strlen("Password: "), 0);
    bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';
    trim_newline(buffer);
    char password[50];
    strncpy(password, buffer, sizeof(password) - 1);
    password[sizeof(password) - 1] = '\0';

    if (!authenticate_client(username, password)) {
        send(client_socket, "Authentication failed. Please try again.\n", strlen("Authentication failed. Please try again.\n"), 0);
        close(client_socket);
        return;
    }

    const char *role = get_role(username);
    char response[BUFFER_SIZE];

    pthread_mutex_lock(&connection_mutex);
    client_sockets[client_count] = client_socket;
    strncpy(client_usernames[client_count], username, sizeof(client_usernames[0]) - 1);
    client_count++;
    pthread_mutex_unlock(&connection_mutex);

    if (strcmp(role, "admin") == 0) {
        pthread_mutex_lock(&admin_mutex);
        if (active_admins > 0) {
            pthread_mutex_unlock(&admin_mutex);
            send(client_socket, "An admin is already connected. Only one admin can be connected at a time.\n", strlen("An admin is already connected. Only one admin can be connected at a time.\n"), 0);
            close(client_socket);
            return;
        }
        active_admins++;
        pthread_mutex_unlock(&admin_mutex);

        snprintf(response, sizeof(response), "Hello Admin! You have full access. Type 'list' to list all files and directories, 'view <filename>' to view a file, 'edit <filename>' to edit a file, 'delete <path>' to delete a file or directory, 'block <username>' to block a user, 'unblock <username>' to unblock a user, 'users' to list connected users, 'cd <dirname>' to change directory, or 'exit' to disconnect.\n");
        send(client_socket, response, strlen(response), 0);
        log_activity("Admin user authenticated");

        while (1) {
            send(client_socket, "Option: ", strlen("Option: "), 0);

            memset(buffer, 0, sizeof(buffer));
            bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0) break;
            buffer[bytes_read] = '\0';
            trim_newline(buffer);

            if (strcmp(buffer, "list") == 0) {
                list_directory(".", client_socket);
            } else if (strncmp(buffer, "view ", 5) == 0) {
                char *filename = buffer + 5;
                extract_metadata(filename, client_socket);
            } else if (strncmp(buffer, "edit ", 5) == 0) {
                char *filename = buffer + 5;
                if (!file_exists(filename)) {
                    send(client_socket, "File does not exist. Cannot edit.\n", strlen("File does not exist. Cannot edit.\n"), 0);
                    continue;
                }
                edit_file(filename, client_socket);
            } else if (strncmp(buffer, "delete ", 7) == 0) {
                char *path = buffer + 7;
                delete_file_or_directory(path, client_socket);
            } else if (strncmp(buffer, "block ", 6) == 0) {
                char *user_to_block = buffer + 6;
                block_user(user_to_block);
                send(client_socket, "User blocked.\n", strlen("User blocked.\n"), 0);
            } else if (strncmp(buffer, "unblock ", 8) == 0) {
                char *user_to_unblock = buffer + 8;
                unblock_user(user_to_unblock);
                send(client_socket, "User unblocked.\n", strlen("User unblocked.\n"), 0);
            } else if (strcmp(buffer, "users") == 0) {
                list_connected_users(client_socket);
            } else if (strncmp(buffer, "cd ", 3) == 0) {
                char *dirname = buffer + 3;
                list_directory_contents(dirname, client_socket);
            } else if (strcmp(buffer, "exit") == 0) {
                break;
            } else {
                send(client_socket, "Unknown command\n", strlen("Unknown command\n"), 0);
            }

            // Linie nouă după fiecare execuție de comandă
            send(client_socket, "\n", strlen("\n"), 0);
        }

        pthread_mutex_lock(&admin_mutex);
        active_admins--;
        pthread_mutex_unlock(&admin_mutex);
    } else if (strcmp(role, "simple") == 0) {
        snprintf(response, sizeof(response), "Hello Simple User! You can upload a new metadata file or extract metadata. Type 'upload' to upload a new metadata file, 'extract' to extract metadata. Type 'search' to view things based on json path or 'exit' to disconnect.\n");
        send(client_socket, response, strlen(response), 0);
        log_activity("Simple user authenticated");

        while (1) {
            send(client_socket, "Option: ", strlen("Option: "), 0);

            memset(buffer, 0, sizeof(buffer));
            bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0) break;
            buffer[bytes_read] = '\0';
            trim_newline(buffer);
               
            if (strcmp(buffer, "upload") == 0) {
                send(client_socket, "Enter the path to the XML file:\n", strlen("Enter the path to the XML file:\n"), 0);
                memset(buffer, 0, sizeof(buffer));
                bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
                if (bytes_read <= 0) break;
                buffer[bytes_read] = '\0';
                trim_newline(buffer);

                char xml_path[BUFFER_SIZE];
                strncpy(xml_path, buffer, sizeof(xml_path) - 1);

                send(client_socket, "Enter the name of the output JSON file (without extension):\n", strlen("Enter the name of the output JSON file (without extension):\n"), 0);
                memset(buffer, 0, sizeof(buffer));
                bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
                if (bytes_read <= 0) break;
                buffer[bytes_read] = '\0';
                trim_newline(buffer);

                if (strlen(buffer) > MAX_BUFFER_LENGTH - 5) { // 5 for ".json"
                    send(client_socket, "Filename too long.\n", strlen("Filename too long.\n"), 0);
                    continue;
                }

                char json_filename[MAX_BUFFER_LENGTH];
                if (snprintf(json_filename, sizeof(json_filename), "%s.json", buffer) >= sizeof(json_filename)) {
                    send(client_socket, "Filename too long.\n", strlen("Filename too long.\n"), 0);
                    continue;
                }

                convert_xml_to_json(xml_path, json_filename);

                // Create log file for XML
                char log_filename_xml[BUFFER_SIZE * 2];
                char *xml_filename_without_extension = strrchr(xml_path, '.');
                if (xml_filename_without_extension) {
                    *xml_filename_without_extension = '\0'; // Remove extension
                }
                snprintf(log_filename_xml, sizeof(log_filename_xml), "%s.log", xml_path);
                FILE *log_file_xml = fopen(log_filename_xml, "a");
                if (log_file_xml) {
                    fprintf(log_file_xml, "Uploaded XML file '%s' and created JSON file '%s'\n", xml_path, json_filename);
                    fclose(log_file_xml);
                }

                // Create log file for JSON
                char log_filename_json[BUFFER_SIZE * 2];
                char *json_filename_without_extension = strrchr(json_filename, '.');
                if (json_filename_without_extension) {
                    *json_filename_without_extension = '\0'; // Remove extension
                }
                snprintf(log_filename_json, sizeof(log_filename_json), "%s.log", json_filename);
                FILE *log_file_json = fopen(log_filename_json, "a");
                if (log_file_json) {
                    fprintf(log_file_json, "Created JSON file '%s' from XML '%s'\n", json_filename, xml_path);
                    fclose(log_file_json);
                }

                send(client_socket, "XML file converted to JSON and saved.\n", strlen("XML file converted to JSON and saved.\n"), 0);
            } else if (strcmp(buffer, "extract") == 0) {
                    send(client_socket, "Enter the name of the file (without extension):\n", strlen("Enter the name of the file (without extension):\n"), 0);
                    memset(buffer, 0, sizeof(buffer));
                    bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
                    if (bytes_read <= 0) break;
                    buffer[bytes_read] = '\0';
                    trim_newline(buffer);

                    char xml_filename[MAX_BUFFER_LENGTH];
                    char json_filename[MAX_BUFFER_LENGTH];

                    if (snprintf(xml_filename, sizeof(xml_filename), "%s.xml", buffer) >= sizeof(xml_filename)) {
                        send(client_socket, "Filename too long.\n", strlen("Filename too long.\n"), 0);
                        return;
                    }

                    if (snprintf(json_filename, sizeof(json_filename), "%s.json", buffer) >= sizeof(json_filename)) {
                        send(client_socket, "Filename too long.\n", strlen("Filename too long.\n"), 0);
                        return;
                    }

                    //convert_xml_to_json(xml_filename, json_filename);

                    send(client_socket, "Metadata extracted and saved as JSON. Displaying content:\n", strlen("Metadata extracted and saved as JSON. Displaying content:\n"), 0);
                    
                    // Display the JSON content
                    extract_metadata(json_filename, client_socket);
                }
                else if (strcmp(buffer, "search") == 0) {
    send(client_socket, "Enter the name of the JSON file (without extension):\n", strlen("Enter the name of the JSON file (without extension):\n"), 0);
    memset(buffer, 0, sizeof(buffer));
    bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) break;
    buffer[bytes_read] = '\0';
    trim_newline(buffer);
    char json_filename[MAX_BUFFER_LENGTH];
    snprintf(json_filename, sizeof(json_filename), "%.*s.json", (int)(sizeof(json_filename) - 6), buffer);
    
    send(client_socket, "Enter the full search path:\n", strlen("Enter the full search path:\n"), 0); // Prompt for full search path
    memset(buffer, 0, sizeof(buffer));
    bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) break;
    buffer[bytes_read] = '\0';
    trim_newline(buffer);

    // Send the full search path to the server
    send(client_socket, buffer, strlen(buffer), 0);

    // Search JSON and send result to client
    search_and_print_json(json_filename, buffer, client_socket);

    // Log the search operation
    char log_filename_json[BUFFER_SIZE * 2];
    char *json_filename_without_extension = strrchr(json_filename, '.');
    if (json_filename_without_extension) {
        *json_filename_without_extension = '\0'; // Remove extension
    }
    snprintf(log_filename_json, sizeof(log_filename_json), "%s.log", json_filename); // Use the original filename without extension

    FILE *log_file_json = fopen(log_filename_json, "a");
    if (log_file_json) {
        fprintf(log_file_json, "Searched in '%s' for '%s'\n", json_filename, buffer); // Use the original filename
        fclose(log_file_json);
    }
}
                 else if (strcmp(buffer, "exit") == 0) {
                break;
            } else {
                send(client_socket, "Unknown command\n", strlen("Unknown command\n"), 0);
            }

            // Linie nouă după fiecare execuție de comandă
            send(client_socket, "\n", strlen("\n"), 0);
        }
    } else if (strcmp(role, "remote") == 0) {
        snprintf(response, sizeof(response), "Hello Remote User! You have remote access. Type 'exit' to disconnect.\n");
        send(client_socket, response, strlen(response), 0);
        log_activity("Remote user authenticated");
    } else {
        snprintf(response, sizeof(response), "Hello! Your role is not recognized.\n");
        send(client_socket, response, strlen(response), 0);
        log_activity("Unknown role authenticated");
    }

    close(client_socket);
    update_connection_count(-1);

    pthread_mutex_lock(&connection_mutex);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == client_socket) {
            client_sockets[i] = client_sockets[--client_count];
            break;
        }
    }
    pthread_mutex_unlock(&connection_mutex);
}



void start_server() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Mesaj simplu de start
    printf("====================================================\n");
    printf("=             Server is Starting Up                =\n");
    printf("====================================================\n");

    threadpool_t *pool = threadpool_create(THREAD_COUNT, QUEUE_SIZE);

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, (void *)monitor_server, NULL);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0) {
        update_connection_count(1);
        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;

        pthread_mutex_lock(&connection_mutex);
        client_sockets[client_count++] = new_socket;
        pthread_mutex_unlock(&connection_mutex);

        threadpool_add(pool, client_handler, pclient);
    }

    close(server_fd);
    threadpool_destroy(pool, 0);
    pthread_cancel(monitor_thread);
    pthread_join(monitor_thread, NULL);
}