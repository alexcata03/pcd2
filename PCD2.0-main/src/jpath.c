#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

// Function to read the entire file into a string
char *read_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Could not open file: %s\n", filename);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *content = (char *)malloc(length + 1);
    if (!content) {
        fclose(file);
        printf("Memory allocation failed\n");
        return NULL;
    }
    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);
    return content;
}

// Function to search cJSON object recursively based on JSON path
void search_json(cJSON *json, const char *path) {
    cJSON *current = json;

    // Tokenize the JSON path
    char *token = strtok((char *)path, ".");
    while (token != NULL) {
        // Check if the token contains array indexing
        char *index_str = strchr(token, '[');
        if (index_str != NULL) {
            // Separate array index from the rest of the token
            *index_str = '\0'; // Terminate token at '['
            int index = atoi(index_str + 1); // Extract array index
            cJSON *array = cJSON_GetObjectItemCaseSensitive(current, token);
            if (array == NULL || !cJSON_IsArray(array)) {
                printf("Path not found: %s\n", path);
                return;
            }
            current = cJSON_GetArrayItem(array, index);
        } else {
            // Handle regular object key
            current = cJSON_GetObjectItemCaseSensitive(current, token);
        }

        if (current == NULL) {
            printf("Path not found: %s\n", path);
            return;
        }
        token = strtok(NULL, ".");
    }

    // Print the result
    char *result = cJSON_Print(current);
    printf("Result: %s\n", result);
    free(result);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <file_path> <json_path>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    const char *json_path = argv[2];

    char *json_string = read_file(file_path);
    if (!json_string) {
        return 1;
    }

    cJSON *json = cJSON_Parse(json_string);
    free(json_string);

    if (json == NULL) {
        printf("Error parsing JSON\n");
        return 1;
    }

    search_json(json, json_path);

    cJSON_Delete(json);
    return 0;
}
/*
gcc -o json_search jpath.c -I/home/alex/cJSON -L/home/alex/cJSON -lcjson
./json_search numefisier.json store.book[0](numerotarea incepe de la 0, se parcurge cu .)
*/