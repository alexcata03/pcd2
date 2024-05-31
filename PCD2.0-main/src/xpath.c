#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // Include this header for strlen
#include <libxml/parser.h>
#include <libxml/xpath.h>

// Function to read the entire file into a string
char *read_file(const char *filename) {
    FILE *file = fopen(filename, "rb"); /* Open the file in reading binary mode */
    if (!file) { /* If the file could not be oppened, print an error message and exit */
        printf("Could not open file: %s\n", filename);
        return NULL;
    }
    fseek(file, 0, SEEK_END); /* Get the file size */
    long length = ftell(file);
    fseek(file, 0, SEEK_SET); /* Reset the position in the file */
    char *content = (char *)malloc(length + 1); /* Allocate the memory for the buffer */
    if (!content) { /* If the buffer is null, print an error message and exit */
        fclose(file);
        printf("Memory allocation failed\n");
        return NULL;
    }
    fread(content, 1, length, file); /* Read the data from the file in the buffer */
    content[length] = '\0'; /* Mark the ending of the buffer string */
    fclose(file); /* Close the file */
    return content; /* Return the buffer */
}

// Function to perform XPath search
void search_xpath(xmlDocPtr doc, const char *xpathExpr) {
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc); /* Get the context path of the xml document */
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar *)xpathExpr, xpathCtx);

    if (xpathObj == NULL) { /* If the object is null, print an error message and exit */
        printf("Error in XPath expression: %s\n", xpathExpr);
        xmlXPathFreeContext(xpathCtx);
        return;
    }

    xmlNodeSetPtr nodes = xpathObj->nodesetval;
    if (nodes->nodeNr == 0) { /* If no data is in the file, print an error message and exit */
        printf("No results found for XPath: %s\n", xpathExpr);
    } else {
        for (int i = 0; i < nodes->nodeNr; ++i) { /* Iterrate through the node */
            xmlChar *content = xmlNodeGetContent(nodes->nodeTab[i]);
            printf("Result: %s\n", content); /* Print the data */
            xmlFree(content); /* Free the memeory allocated to the xml characters */
        }
    }

    xmlXPathFreeObject(xpathObj); /* Free the object*/
    xmlXPathFreeContext(xpathCtx); /* Free the content */
}

int main(int argc, char **argv) {
    if (argc != 3) { /* Expecting 3 elements from the command line */
        printf("Usage: %s <file_path> <xpath>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1]; /* Get the file path from the second element from the command line */
    const char *xpath = argv[2]; /* Get the xpaht from the command line */

    char *xml_string = read_file(file_path); /* Read the file from the file path */
    if (!xml_string) { /* If the data resulted is empty exit */
        return 1;
    }

    xmlDocPtr doc = xmlReadMemory(xml_string, strlen(xml_string), "noname.xml", NULL, 0); /* Read the xml*/
    free(xml_string); /* Free the memory */

    if (doc == NULL) { /* If the document is null, print an error message and exit */
        printf("Error parsing XML\n");
        return 1;
    }

    search_xpath(doc, xpath); /* Search for the data in the document */

    xmlFreeDoc(doc); /* Free the document */
    xmlCleanupParser(); /* Clean the parser */
    return 0; /* Exit */
}
/*
gcc -o xml_search xpath.c `xml2-config --cflags --libs`
./xml_search numefisier.xml /store/chesti[1](numerotarea incepe de la 1 parcurgi cu /)
*/