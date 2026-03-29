#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 8192
#define NOTICE_LIMIT 1500000L

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buffer;

static void buffer_init(Buffer *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void buffer_free(Buffer *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void buffer_clear(Buffer *b) {
    b->len = 0;
    if (b->data) {
        b->data[0] = '\0';
    }
}

static void buffer_reserve(Buffer *b, size_t needed) {
    if (needed <= b->cap) return;

    size_t new_cap = (b->cap == 0) ? 8192 : b->cap;
    while (new_cap < needed) {
        new_cap *= 2;
    }

    char *new_data = realloc(b->data, new_cap);
    if (!new_data) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }

    b->data = new_data;
    b->cap = new_cap;
}

static void buffer_append(Buffer *b, const char *s) {
    size_t add = strlen(s);
    buffer_reserve(b, b->len + add + 1);
    memcpy(b->data + b->len, s, add);
    b->len += add;
    b->data[b->len] = '\0';
}

static void trim_newline(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static char *ltrim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

static int is_alpha_str(const char *s) {
    if (*s == '\0') return 0;
    while (*s) {
        if (!isalpha((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_digit_str(const char *s) {
    if (*s == '\0') return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_notice_header_line(const char *line) {
    char buf[MAX_LINE];
    char w1[256], w2[256], num[256], extra[256];

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    trim_newline(buf);

    char *s = ltrim(buf);
    rtrim(s);

    if (*s == '\0') return 0;

    size_t len = strlen(s);
    if (len < 2) return 0;
    if (s[len - 1] != ':') return 0;

    s[len - 1] = '\0';
    rtrim(s);

    int count = sscanf(s, "%255s %255s %255s %255s", w1, w2, num, extra);
    if (count != 3) return 0;

    if (!is_alpha_str(w1)) return 0;
    if (!is_alpha_str(w2)) return 0;
    if (!is_digit_str(num)) return 0;

    return 1;
}

static void write_output_file(const Buffer *chunk, int index) {
    char filename[512];
    snprintf(filename, sizeof(filename), "/Users/malik/Documents/Limbus Company News Archive/Other Files/Google Doc Text Files/lcsn-%d.txt", index);

    FILE *out = fopen(filename, "w");
    if (!out) {
        fprintf(stderr, "Could not create output file: %s\n", filename);
        exit(1);
    }

    if (chunk->len > 0) {
        fwrite(chunk->data, 1, chunk->len, out);
    }

    fclose(out);
    printf("Wrote %s (%zu chars)\n", filename, chunk->len);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s notices.txt\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("Could not open input file");
        return 1;
    }

    Buffer current_notice;
    Buffer current_chunk;

    buffer_init(&current_notice);
    buffer_init(&current_chunk);

    char line[MAX_LINE];
    int file_index = 1;
    int have_notice = 0;

    while (fgets(line, sizeof(line), fp)) {
        int is_header = is_notice_header_line(line);

        if (is_header) {
            if (have_notice) {
                if (current_chunk.len > 0 &&
                    current_chunk.len + current_notice.len > NOTICE_LIMIT) {
                    write_output_file(&current_chunk, file_index++);
                    buffer_clear(&current_chunk);
                }

                buffer_append(&current_chunk, current_notice.data);
                buffer_clear(&current_notice);
            }

            have_notice = 1;
        }

        if (have_notice) {
            buffer_append(&current_notice, line);
        }
    }

    if (have_notice && current_notice.len > 0) {
        if (current_chunk.len > 0 &&
            current_chunk.len + current_notice.len > NOTICE_LIMIT) {
            write_output_file(&current_chunk, file_index++);
            buffer_clear(&current_chunk);
        }

        buffer_append(&current_chunk, current_notice.data);
    }

    if (current_chunk.len > 0) {
        write_output_file(&current_chunk, file_index++);
    }

    fclose(fp);
    buffer_free(&current_notice);
    buffer_free(&current_chunk);

    return 0;
}