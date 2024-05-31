#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // Include this header for strlen
#include <libxml/parser.h>
#include <libxml/xpath.h>

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

// Function to perform XPath search
void search_xpath(xmlDocPtr doc, const char *xpathExpr) {
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar *)xpathExpr, xpathCtx);

    if (xpathObj == NULL) {
        printf("Error in XPath expression: %s\n", xpathExpr);
        xmlXPathFreeContext(xpathCtx);
        return;
    }

    xmlNodeSetPtr nodes = xpathObj->nodesetval;
    if (nodes->nodeNr == 0) {
        printf("No results found for XPath: %s\n", xpathExpr);
    } else {
        for (int i = 0; i < nodes->nodeNr; ++i) {
            xmlChar *content = xmlNodeGetContent(nodes->nodeTab[i]);
            printf("Result: %s\n", content);
            xmlFree(content);
        }
    }

    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <file_path> <xpath>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    const char *xpath = argv[2];

    char *xml_string = read_file(file_path);
    if (!xml_string) {
        return 1;
    }

    xmlDocPtr doc = xmlReadMemory(xml_string, strlen(xml_string), "noname.xml", NULL, 0);
    free(xml_string);

    if (doc == NULL) {
        printf("Error parsing XML\n");
        return 1;
    }

    search_xpath(doc, xpath);

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}
/*
gcc -o xml_search xpath.c `xml2-config --cflags --libs`
./xml_search numefisier.xml /store/chesti[1](numerotarea incepe de la 1 parcurgi cu /)
*/