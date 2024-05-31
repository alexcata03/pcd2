#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

// Function to read the entire file into a string
char *read_file(const char *filename) {
    FILE *file = fopen(filename, "rb"); /* Opent the file in read binary mode */
    if (!file) { /* If the file could not be opened, print an error message and exit */
        printf("Could not open file: %s\n", filename);
        return NULL;
    }
    fseek(file, 0, SEEK_END); /* Get the size of the file*/
    long length = ftell(file); 
    fseek(file, 0, SEEK_SET); /* Reset the position in the file to the beginning of it */
    char *content = (char *)malloc(length + 1); /* Allocate the memory for the buffer */
    if (!content) { /* If the memory could not be allocated, print an error message, close the file and exit */
        fclose(file);
        printf("Memory allocation failed\n");
        return NULL;
    }
    fread(content, 1, length, file); /* Read the data from the file in the buffer */
    content[length] = '\0'; /* Set the ending of the buffer string */
    fclose(file); /* Close the file */
    return content; /* Return the content */
}

// Function to search cJSON object recursively based on JSON path
void search_json(cJSON *json, const char *path) {
    cJSON *current = json; /* Current cJSON node */

    // Tokenize the JSON path
    char *token = strtok((char *)path, ".");
    while (token != NULL) { 
        // Check if the token contains array indexing
        char *index_str = strchr(token, '[');
        if (index_str != NULL) {
            // Separate array index from the rest of the token
            *index_str = '\0'; // Terminate token at '['
            int index = atoi(index_str + 1); // Extract array index
            cJSON *array = cJSON_GetObjectItemCaseSensitive(current, token); /* Add the current token*/
            if (array == NULL || !cJSON_IsArray(array)) { /* If the array is null, print an error message and exit */
                printf("Path not found: %s\n", path);
                return;
            }
            current = cJSON_GetArrayItem(array, index); /* Get the current cJSON node */
        } else {
            // Handle regular object key
            current = cJSON_GetObjectItemCaseSensitive(current, token);
        }

        if (current == NULL) { /* If the node is null, print an error message and exit */
            printf("Path not found: %s\n", path);
            return;
        }
        token = strtok(NULL, "."); /* Mode to the next token */
    }

    // Print the result
    char *result = cJSON_Print(current); 
    printf("Result: %s\n", result); /* Print the cJSON node */
    free(result); /* Free the memory allocated to the `result` */
}

int main(int argc, char **argv) {
    if (argc != 3) { /* 3 arguments must be sent from the command line */
        printf("Usage: %s <file_path> <json_path>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1]; /* Get the file path */
    const char *json_path = argv[2]; /* Get the json file path */

    char *json_string = read_file(file_path); /* Read the data from the file path */
    if (!json_string) {
        return 1;
    }

    cJSON *json = cJSON_Parse(json_string); /* Parse the json data */
    free(json_string); /* Free the memroy allocated */

    if (json == NULL) { /* If the data is null, print an error message and exit */
        printf("Error parsing JSON\n");
        return 1;
    }

    search_json(json, json_path); /* Search for the json file */

    cJSON_Delete(json); /* Delete the json node */
    return 0;
}
/*
gcc -o json_search jpath.c -I/home/alex/cJSON -L/home/alex/cJSON -lcjson
./json_search numefisier.json store.book[0](numerotarea incepe de la 0, se parcurge cu .)
*/