#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NOTICES 10000

typedef struct {
    char *content;
} Notice;

char* load_file(const char* filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buffer = malloc(size + 1);
    if (!buffer) { fclose(fp); return NULL; }

    fread(buffer, 1, size, fp);
    buffer[size] = '\0';
    fclose(fp);
    return buffer;
}

int parse_notices(char *buffer, Notice *notices) {
    int count = 0;

    char *current = strstr(buffer, "Writing Sample ");
    while (current && count < MAX_NOTICES) {

        char *next = strstr(current + 1, "Writing Sample ");
        char *end = next ? next : buffer + strlen(buffer);

        long len = end - current;

        char *block = malloc(len + 1);
        if (!block) break;

        strncpy(block, current, len);
        block[len] = '\0';

        notices[count].content = block;
        count++;

        current = next;
    }

    return count;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <input.txt> <output.txt>\n", argv[0]);
        return 1;
    }

    char *buffer = load_file(argv[1]);
    if (!buffer) {
        printf("Error: Could not load input file.\n");
        return 1;
    }

    Notice notices[MAX_NOTICES];
    int count = parse_notices(buffer, notices);

    if (count == 0) {
        printf("No notices found.\n");
        free(buffer);
        return 1;
    }

    FILE *out = fopen(argv[2], "wb");
    if (!out) {
        printf("Error: Could not open output file.\n");
        free(buffer);
        return 1;
    }

    for (int i = count - 1, new_label = 1; i >= 0; i--, new_label++) {

        char *body = strchr(notices[i].content, ':');
        if (!body) body = notices[i].content;
        else body++;

        fprintf(out, "Writing Sample %d:%s", new_label, body);

        if (body[strlen(body)-1] != '\n') {
            fprintf(out, "\n");
        }
        fprintf(out, "\n");
    }

    fclose(out);

    for (int i = 0; i < count; i++) {
        free(notices[i].content);
    }
    free(buffer);

    printf("Success! Reversed %d notices into '%s'\n", count, argv[2]);
    return 0;
}