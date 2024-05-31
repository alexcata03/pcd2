#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cjson/cJSON.h>

// Modificați `extract_metadata_xml` și `extract_metadata_json` să nu mai afișeze pe consolă
void extract_metadata_xml(const char *filename) {
    xmlDoc *document = xmlReadFile(filename, NULL, 0);
    if (document == NULL) {
        log_change(filename, "extract", "Failed to parse XML");
        return;
    }

    xmlNode *root = xmlDocGetRootElement(document);
    xmlNode *cur_node = NULL;

    for (cur_node = root->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char *)cur_node->name, "author") == 0) {
                // printf("Author: %s\n", (const char *)xmlNodeGetContent(cur_node));
                log_change(filename, "extract", "Extracted author");
            } else if (strcmp((const char *)cur_node->name, "title") == 0) {
                // printf("Title: %s\n", (const char *)xmlNodeGetContent(cur_node));
                log_change(filename, "extract", "Extracted title");
            } else if (strcmp((const char *)cur_node->name, "description") == 0) {
                // printf("Description: %s\n", (const char *)xmlNodeGetContent(cur_node));
                log_change(filename, "extract", "Extracted description");
            } else if (strcmp((const char *)cur_node->name, "file_size") == 0) {
                // printf("File Size: %s\n", (const char *)xmlNodeGetContent(cur_node));
                log_change(filename, "extract", "Extracted file_size");
            }
        }
    }
    xmlFreeDoc(document);
}

void extract_metadata_json(const char *filename) {
    FILE *file = fopen(filename, "r"); /* Open the file in reading mode */
    if (!file) { /* If the file cannot be opened, print an error message and exit */
        log_change(filename, "extract", "Failed to open JSON file");
        return;
    }

    char buffer[1024]; /* Define the buffer */
    fread(buffer, 1, sizeof(buffer), file); /* Read the data from the file in the buffer */
    fclose(file); /* Close the file */

    cJSON *json = cJSON_Parse(buffer); /* Parse the buffer to a json node */
    if (!json) { /* If the node is null, print an error message and exit */
        log_change(filename, "extract", "Failed to parse JSON");
        return;
    }

    cJSON *author = cJSON_GetObjectItem(json, "author"); /* Get the author from the json node */
    if (cJSON_IsString(author)) {
        // printf("Author: %s\n", author->valuestring);
        log_change(filename, "extract", "Extracted author"); /* Log the author data */
    }

    cJSON *title = cJSON_GetObjectItem(json, "title"); /* Get the title form the json node */
    if (cJSON_IsString(title)) {
        // printf("Title: %s\n", title->valuestring);
        log_change(filename, "extract", "Extracted title"); /* Log the file title */
    }

    cJSON *description = cJSON_GetObjectItem(json, "description"); /* Get the file description */
    if (cJSON_IsString(description)) {
        // printf("Description: %s\n", description->valuestring);
        log_change(filename, "extract", "Extracted description"); /* Log the description data */
    }

    cJSON *file_size = cJSON_GetObjectItem(json, "file_size"); /* Get the file size */
    if (cJSON_IsNumber(file_size)) {
        // printf("File Size: %d\n", file_size->valueint);
        log_change(filename, "extract", "Extracted file_size"); /* Log the file ize */
    }

    cJSON_Delete(json); /* Delete the json node */
}


void log_change(const char *filename, const char *change_type, const char *details) {
    FILE *change_log_file = fopen("changes.log", "a"); /* Open the changes log in appending mode */
    if (change_log_file == NULL) { /* If the file is null, print an error message and exti */
        perror("Could not open change log file");
        return;
    }
    time_t now = time(NULL); /* Start the time counter */
    /* Write the data in th log file */
    fprintf(change_log_file, "[%s] File: %s, Change: %s, Details: %s\n", ctime(&now), filename, change_type, details); 
    fclose(change_log_file); /* Close the data*/
}
