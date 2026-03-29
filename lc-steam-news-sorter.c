#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NOTICES 5000
#define MAX_WORDS 2000
#define MAX_WORD_LEN 64

// Usage: ./lc-steam-news-sorter lcsn-MASTER.txt full_steamlist.txt
// Compile: gcc -O3 lc-steam-news-sorter.c -o lc-steam-news-sorter  

typedef struct {
    int original_id;
    int parsed_label_num;
    char *cleaned_content;
    unsigned long content_hash;
    
    double chrono_score; 
    int has_steam_match;
    int steam_index;
    
    int y, m, d;
    int new_label;
} Notice;

typedef struct {
    int y, m, d;
    char *normalized;
    char raw_snippet[65];
    char **words;
    int word_count;
} SteamPost;

Notice notices[MAX_NOTICES];
int notice_count = 0;

SteamPost steam_posts[MAX_NOTICES];
int steam_count = 0;

int g_is_input_ascending = 0;

unsigned long djb2_hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

int cmp_str(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

const char* custom_stristr(const char* haystack, const char* needle) {
    if (!*needle) return haystack;
    for (const char* h = haystack; *h; h++) {
        const char* h_sub = h;
        const char* n_sub = needle;
        while (*h_sub && *n_sub && tolower((unsigned char)*h_sub) == tolower((unsigned char)*n_sub)) {
            h_sub++; n_sub++;
        }
        if (!*n_sub) return h;
    }
    return NULL;
}

char* load_file(const char* filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size <= 0) { fclose(fp); return NULL; }
    rewind(fp);
    char *buffer = malloc(size + 1);
    if (!buffer) { fclose(fp); return NULL; }
    size_t bytes = fread(buffer, 1, size, fp);
    buffer[bytes] = '\0';
    fclose(fp);
    return buffer;
}

int parse_month(const char* s) {
    char low[4] = {0};
    for(int i = 0; i < 3 && s[i]; i++) low[i] = tolower((unsigned char)s[i]);
    if (!strcmp(low, "jan")) return 1; if (!strcmp(low, "feb")) return 2;
    if (!strcmp(low, "mar")) return 3; if (!strcmp(low, "apr")) return 4;
    if (!strcmp(low, "may")) return 5; if (!strcmp(low, "jun")) return 6;
    if (!strcmp(low, "jul")) return 7; if (!strcmp(low, "aug")) return 8;
    if (!strcmp(low, "sep")) return 9; if (!strcmp(low, "oct")) return 10;
    if (!strcmp(low, "nov")) return 11;if (!strcmp(low, "dec")) return 12;
    return 0;
}

void normalize_text(char *dest, const char *src, int max_len) {
    int j = 0;
    for (int i = 0; src[i] && j < max_len - 1; i++) {
        if (isalnum((unsigned char)src[i])) {
            dest[j++] = tolower((unsigned char)src[i]);
        }
    }
    dest[j] = '\0';
}

void extract_unique_words(const char* text, char*** words_out, int* count_out) {
    char** words = malloc(MAX_WORDS * sizeof(char*));
    int count = 0;
    const char* p = text;
    char buf[MAX_WORD_LEN];
    int i = 0;
    
    while (*p) {
        if (isalnum((unsigned char)*p)) {
            if (i < MAX_WORD_LEN - 1) buf[i++] = tolower((unsigned char)*p);
        } else {
            if (i > 2) { 
                buf[i] = '\0';
                int dup = 0;
                for (int j = 0; j < count; j++) {
                    if (strcmp(words[j], buf) == 0) { dup = 1; break; }
                }
                if (!dup && count < MAX_WORDS) {
                    words[count] = strdup(buf);
                    count++;
                }
            }
            i = 0;
        }
        p++;
    }
    if (i > 2) { 
        buf[i] = '\0';
        int dup = 0;
        for (int j = 0; j < count; j++) {
            if (strcmp(words[j], buf) == 0) { dup = 1; break; }
        }
        if (!dup && count < MAX_WORDS) {
            words[count] = strdup(buf);
            count++;
        }
    }
    
    qsort(words, count, sizeof(char*), cmp_str);
    
    *words_out = words;
    *count_out = count;
}

int parse_steam_date(const char* block, int* y, int* m, int* d) {
    *y = 0; *m = 0; *d = 0;
    const char* months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    for(int i=0; i<12; i++) {
        const char* ptr = custom_stristr(block, months[i]);
        if (ptr) {
            *m = i + 1;
            int parsed_d = 0, parsed_y = 0;
            if (sscanf(ptr + strlen(months[i]), " %d , %d", &parsed_d, &parsed_y) == 2 ||
                sscanf(ptr + strlen(months[i]), " %d, %d", &parsed_d, &parsed_y) == 2) {
                *d = parsed_d;
                *y = parsed_y;
                return 1;
            } else if (sscanf(ptr + strlen(months[i]), " %d", &parsed_d) == 1) {
                *d = parsed_d;
                return 1;
            }
        }
    }
    return 0;
}

void get_snippet(const char* text, char* out, int max_len) {
    if (!text) { out[0] = '\0'; return; }
    int i = 0, j = 0;
    while (text[i] && isspace((unsigned char)text[i])) i++;

    while (text[i] && j < max_len - 1) {
        if (text[i] != '\n' && text[i] != '\r') out[j++] = text[i];
        else if (j > 0 && out[j-1] != ' ') out[j++] = ' ';
        i++;
    }
    out[j] = '\0';
}

void extract_date_fallback(const char* text, int* year, int* month, int* day) {
    *year = 0; *month = 0; *day = 0;
    int last_seen_year = 0;
    
    char buf[401] = {0};
    strncpy(buf, text, 400); 
    const char* p = buf;
    
    while (*p) {
        int y = 0, m = 0, d = 0;
        char m_str[32] = {0};

        if (sscanf(p, "%4d", &y) == 1 && y >= 2023 && y <= 2030) last_seen_year = y;

        if (sscanf(p, "%4d. %2d. %2d", &y, &m, &d) == 3 || sscanf(p, "%4d.%2d.%2d", &y, &m, &d) == 3) {
            if (y >= 2023 && m >= 1 && m <= 12 && d >= 1 && d <= 31) { *year = y; *month = m; *day = d; return; }
        }
        if (sscanf(p, "%2d/%2d/%4d", &m, &d, &y) == 3) {
            if (y >= 2023 && m >= 1 && m <= 12 && d >= 1 && d <= 31) { *year = y; *month = m; *day = d; return; }
        }

        int offset = 0;
        if (sscanf(p, "%15[a-zA-Z.] %d%n", m_str, &d, &offset) == 2) {
            char *after_d = (char*)p + offset;
            if (strncmp(after_d, "st", 2)==0 || strncmp(after_d, "nd", 2)==0 || 
                strncmp(after_d, "rd", 2)==0 || strncmp(after_d, "th", 2)==0) {
                after_d += 2;
            }
            if (sscanf(after_d, ", %4d", &y) == 1) {
                 if (y >= 2023 && d >= 1 && d <= 31 && (m = parse_month(m_str))) { 
                     *year = y; *month = m; *day = d; return; 
                 }
            }
        }

        if (sscanf(p, "%4d, %15[a-zA-Z.] %d", &y, m_str, &d) == 3) {
            if (y >= 2023 && d >= 1 && d <= 31 && (m = parse_month(m_str))) { *year = y; *month = m; *day = d; return; }
        }

        if ((sscanf(p, "%15[a-zA-Z.] %d", m_str, &d) == 2 && (m = parse_month(m_str))) || 
             sscanf(p, "%2d.%2d(%*[^)])", &m, &d) == 2) {
            if (m >= 1 && m <= 12 && d >= 1 && d <= 31) {
                if (*month == 0) { *year = last_seen_year; *month = m; *day = d; }
            }
        }
        p++;
    }
}

char* clean_spacing(const char* original) {
    char* clean = malloc(strlen(original) + 1);
    if (!clean) return NULL;
    int i = 0, j = 0;
    
    while (original[i]) {
        if (original[i] == '\n' || original[i] == '\r') {
            int nl_count = 0;
            int temp_i = i;
            
            while (original[temp_i] == '\n' || original[temp_i] == '\r' || original[temp_i] == ' ' || original[temp_i] == '\t') {
                if (original[temp_i] == '\n') nl_count++;
                temp_i++;
            }
            
            if (nl_count == 0) nl_count = 1; 
            int out_nl = (nl_count >= 2) ? 2 : 1;
            for(int k = 0; k < out_nl; k++) clean[j++] = '\n';
            
            temp_i = i;
            int last_nl_pos = -1;
            while (original[temp_i]) {
                if (original[temp_i] == '\n') last_nl_pos = temp_i;
                else if (original[temp_i] != '\r' && original[temp_i] != ' ' && original[temp_i] != '\t') break;
                temp_i++;
            }
            
            if (last_nl_pos != -1) i = last_nl_pos + 1;
            else while (original[i] == '\r') i++;
            
        } else {
            clean[j++] = original[i++];
        }
    }
    clean[j] = '\0';
    return clean;
}

int compare_notices(const void* a, const void* b) {
    Notice* n1 = (Notice*)a;
    Notice* n2 = (Notice*)b;
    
    if (n1->chrono_score > n2->chrono_score) return -1;
    if (n1->chrono_score < n2->chrono_score) return 1;
    
    if (g_is_input_ascending) {
        return n2->original_id - n1->original_id;
    } else {
        return n1->original_id - n2->original_id;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("\nUsage: %s <input_notices.txt> <steam_full_list.txt>\n", argv[0]);
        printf("Error: The Steam dump file is required for accurate date metadata assignment.\n\n");
        return 1;
    }

    const char *input_filename = argv[1];
    char *buffer = load_file(input_filename);
    if (!buffer) {
        printf("Error: Could not load master input file '%s'.\n", input_filename);
        return 1;
    }
    
    char *steam_buf = load_file(argv[2]);
    if (!steam_buf) {
        printf("Error: Could not load Steam list file '%s'.\n", argv[2]);
        free(buffer);
        return 1;
    }

    printf("Dataset and Steam Metadata loaded. Parsing and generating date index...\n");

    int current_year = 2026; 
    char* yr_ptr = strstr(steam_buf, "202");
    if (yr_ptr) {
        int tmp = 0;
        if (sscanf(yr_ptr, "%4d", &tmp) == 1 && tmp >= 2023 && tmp <= 2030) {
            current_year = tmp;
        }
    }

    char* p = steam_buf;
    char* s_next;
    while ((s_next = strstr(p, "Share"))) {
        *s_next = '\0'; 

        char* blk_yr = strstr(p, "202");
        if (blk_yr) {
            int tmp = 0;
            if (sscanf(blk_yr, "%4d", &tmp) == 1 && tmp >= 2023 && tmp <= 2030) {
                current_year = tmp;
            }
        }

        int y = 0, m = 0, d = 0;
        if (parse_steam_date(p, &y, &m, &d)) {
            if (y == 0) y = current_year;
            else current_year = y; 
        }

        steam_posts[steam_count].y = y;
        steam_posts[steam_count].m = m;
        steam_posts[steam_count].d = d;
        
        steam_posts[steam_count].normalized = malloc(strlen(p) + 1);
        normalize_text(steam_posts[steam_count].normalized, p, strlen(p) + 1);
        
        get_snippet(p, steam_posts[steam_count].raw_snippet, 60);
        
        // Extract & Sort Steam Post words
        extract_unique_words(p, &steam_posts[steam_count].words, &steam_posts[steam_count].word_count);

        steam_count++;
        
        *s_next = 'S'; 
        p = s_next + 5;
    }
    
    char *current = strstr(buffer, "Writing Sample ");
    while (current) {
        char *next = current + 15;
        while ((next = strstr(next, "Writing Sample "))) {
            int dummy;
            if (sscanf(next, "Writing Sample %d:", &dummy) == 1) break;
            next += 15;
        }

        int label_num = 0;
        if (sscanf(current, "Writing Sample %d:", &label_num) >= 1) {
            char *body_start = strchr(current, ':');
            if (!body_start || (next && body_start > next)) body_start = current + 15;
            else body_start++;
            
            char *body_end = next ? next : current + strlen(current);

            while (body_start < body_end && (isspace((unsigned char)*body_start) || *body_start == '\'' || *body_start == '"' || strncmp(body_start, "\xE2\x80\x98", 3) == 0 || strncmp(body_start, "\xE2\x80\x9C", 3) == 0)) {
                if (*body_start == '\'' || *body_start == '"') body_start++;
                else if (strncmp(body_start, "\xE2\x80\x98", 3) == 0 || strncmp(body_start, "\xE2\x80\x9C", 3) == 0) body_start += 3;
                else body_start++;
            }

            char *actual_end = body_end - 1;
            while (actual_end >= body_start && (isspace((unsigned char)*actual_end) || *actual_end == '\'' || *actual_end == '"' || (actual_end - 2 >= body_start && (strncmp(actual_end - 2, "\xE2\x80\x99", 3) == 0 || strncmp(actual_end - 2, "\xE2\x80\x9D", 3) == 0)))) {
                if (*actual_end == '\'' || *actual_end == '"') actual_end--;
                else if (actual_end - 2 >= body_start && (strncmp(actual_end - 2, "\xE2\x80\x99", 3) == 0 || strncmp(actual_end - 2, "\xE2\x80\x9D", 3) == 0)) actual_end -= 3;
                else actual_end--;
            }

            long body_len = actual_end - body_start + 1;
            if (body_len <= 0) { 
                current = next;
                continue;
            }

            char *raw_body = malloc(body_len + 1);
            if (raw_body) {
                strncpy(raw_body, body_start, body_len);
                raw_body[body_len] = '\0';

                char* new_cleaned_content = clean_spacing(raw_body);
                unsigned long new_hash = djb2_hash(new_cleaned_content);
                int is_duplicate = 0;
                
                // Optimized Duplicate Check (Integer Hash First)
                for (int j = 0; j < notice_count; j++) {
                    if (notices[j].content_hash == new_hash && strcmp(new_cleaned_content, notices[j].cleaned_content) == 0) {
                        is_duplicate = 1;
                        break;
                    }
                }

                if (is_duplicate) {
                    printf("  -> Duplicate notice (Original Label: %d) found and ignored.\n", label_num);
                    free(new_cleaned_content);
                } else if (notice_count < MAX_NOTICES) {
                    notices[notice_count].original_id = notice_count;
                    notices[notice_count].parsed_label_num = label_num;
                    notices[notice_count].cleaned_content = new_cleaned_content;
                    notices[notice_count].content_hash = new_hash;
                    notices[notice_count].chrono_score = -1.0;
                    notice_count++;
                }
                free(raw_body);
            }
        }
        current = next;
    }

    int steam_match_count = 0;
    int native_match_count = 0;
    int omitted_count = 0;

    for (int i = 0; i < notice_count; i++) {
        char title[512] = {0};
        const char *nl = strchr(notices[i].cleaned_content, '\n');
        if (nl) {
            int len = nl - notices[i].cleaned_content;
            if (len > 511) len = 511;
            strncpy(title, notices[i].cleaned_content, len);
            title[len] = '\0';
        } else {
            strncpy(title, notices[i].cleaned_content, 511);
        }
        
        char notice_norm[1024] = {0};
        normalize_text(notice_norm, notices[i].cleaned_content, 1024);
        char notice_norm_150[151] = {0};
        strncpy(notice_norm_150, notice_norm, 150);
        
        char title_norm[256] = {0};
        normalize_text(title_norm, title, 256);
        
        char** notice_words;
        int notice_word_count;
        extract_unique_words(notices[i].cleaned_content, &notice_words, &notice_word_count);
        
        int best_match = -1;
        int best_overlap = 0;
        int best_is_direct = 0;

        for (int j = 0; j < steam_count; j++) {
            int is_direct = 0;
            if (strlen(title_norm) > 8 && strstr(steam_posts[j].normalized, title_norm)) {
                is_direct = 1;
            } else if (strlen(notice_norm_150) > 20 && strstr(steam_posts[j].normalized, notice_norm_150)) {
                is_direct = 1;
            }
            
            int overlap = 0;
            int ptr1 = 0, ptr2 = 0;
            while(ptr1 < notice_word_count && ptr2 < steam_posts[j].word_count) {
                int cmp = strcmp(notice_words[ptr1], steam_posts[j].words[ptr2]);
                if (cmp == 0) {
                    overlap++;
                    ptr1++;
                    ptr2++;
                } else if (cmp < 0) {
                    ptr1++;
                } else {
                    ptr2++;
                }
            }
            
            int score = overlap;
            if (is_direct) score += 1000; 
            
            if (score > best_overlap) {
                best_overlap = score;
                best_match = j;
                best_is_direct = is_direct;
            }
        }
        
        int accepted = 0;
        if (best_match != -1) {
            int pure_overlap = best_is_direct ? (best_overlap - 1000) : best_overlap;
            if (best_is_direct) accepted = 1;
            else if (pure_overlap >= 12) accepted = 1;
            else if (notice_word_count > 0 && pure_overlap >= 5 && pure_overlap >= (int)(notice_word_count * 0.4)) accepted = 1;
        }
        
        if (accepted && steam_posts[best_match].y > 0) {
            notices[i].has_steam_match = 1;
            notices[i].steam_index = best_match;
            notices[i].y = steam_posts[best_match].y;
            notices[i].m = steam_posts[best_match].m;
            notices[i].d = steam_posts[best_match].d;
            
            notices[i].chrono_score = (notices[i].y * 10000.0) + (notices[i].m * 100.0) + notices[i].d + (1.0 - ((double)best_match / 10000.0));
            steam_match_count++;
        } else {
            notices[i].has_steam_match = 0;
            extract_date_fallback(notices[i].cleaned_content, &notices[i].y, &notices[i].m, &notices[i].d);
            if (notices[i].m > 0 && notices[i].y == 0) notices[i].y = 2024; 
            if (notices[i].m > 0) {
                notices[i].chrono_score = notices[i].y * 10000.0 + notices[i].m * 100.0 + notices[i].d;
                native_match_count++;
            } else {
                omitted_count++;
            }
        }
        
        for(int w=0; w<notice_word_count; w++) free(notice_words[w]);
        free(notice_words);
    }

    int first_valid = -1, last_valid = -1;
    for (int i = 0; i < notice_count; i++) {
        if (notices[i].chrono_score >= 0.0) {
            if (first_valid == -1) first_valid = i;
            last_valid = i;
        }
    }
    
    if (first_valid != -1 && last_valid != -1 && first_valid != last_valid) {
        if (notices[first_valid].chrono_score < notices[last_valid].chrono_score) g_is_input_ascending = 1;
        else g_is_input_ascending = 0;
    }

    double top_fallback = g_is_input_ascending ? 20230101.0 : 20300101.0;
    double bottom_fallback = g_is_input_ascending ? 20300101.0 : 20230101.0;

    int start = 0;
    while (start < notice_count) {
        if (notices[start].chrono_score < 0.0) {
            int end = start;
            while (end < notice_count && notices[end].chrono_score < 0.0) end++;
            
            double val1 = (start > 0) ? notices[start-1].chrono_score : top_fallback; 
            double val2 = (end < notice_count) ? notices[end].chrono_score : bottom_fallback;
            
            for (int k = start; k < end; k++) {
                notices[k].chrono_score = val1 + (val2 - val1) * (k - start + 1) / (end - start + 1);
            }
            start = end;
        } else {
            start++;
        }
    }

    qsort(notices, notice_count, sizeof(Notice), compare_notices);

    FILE *out = fopen("lcsn-sorted_notices.txt", "wb");
    FILE *log = fopen("lcsn-sort_log.txt", "wb");
    
    fprintf(log, "=================================================\n");
    fprintf(log, "LIMBUS COMPANY: STEAM NEWS SORTING LOG\n");
    fprintf(log, "Mode: O(N) Two-Pointer Hybrid Matcher\n");
    fprintf(log, "Input Format Detected: %s\n", g_is_input_ascending ? "Ascending (Oldest -> Newest)" : "Descending (Newest -> Oldest)");
    fprintf(log, "Total Steam Posts Parsed: %d\n", steam_count);
    fprintf(log, "Total Unique Notices Processed: %d\n", notice_count);
    fprintf(log, "-------------------------------------------------\n");
    fprintf(log, "  Dates assigned via Steam Match: %d\n", steam_match_count);
    fprintf(log, "  Dates assigned via Native Text Fallback: %d\n", native_match_count);
    fprintf(log, "  Notices Sandwiched (No Date): %d\n", omitted_count);
    fprintf(log, "=================================================\n\n");

    for (int i = 0; i < notice_count; i++) {
        notices[i].new_label = notice_count - i;

        fprintf(out, "Writing Sample %d:\n", notices[i].new_label);
        fprintf(out, "‘%s’\n\n\n", notices[i].cleaned_content ? notices[i].cleaned_content : "");
        
        char snippet[60];
        get_snippet(notices[i].cleaned_content, snippet, sizeof(snippet));
        
        fprintf(log, "[Old #%03d -> New #%03d] ", notices[i].parsed_label_num, notices[i].new_label);
        
        if (notices[i].has_steam_match) {
            fprintf(log, "Match: Steam ID %03d | ", notices[i].steam_index);
        } else {
            if (notices[i].y > 0) fprintf(log, "Date Fallback: %04d-%02d-%02d | ", notices[i].y, notices[i].m, notices[i].d);
            else fprintf(log, "Sandwiched: ****-**-** | ");
        }
        fprintf(log, "Preview: %s...\n", snippet);
    }

    fclose(out);
    fclose(log);
    
    printf("\nSuccess! Generated 'lcsn-sorted_notices.txt' and 'lcsn-sort_log.txt'.\n");
    printf("Detected Input Format: %s\n", g_is_input_ascending ? "Ascending" : "Descending");
    printf(" - Dates from Steam Metadata: %d\n", steam_match_count);
    printf(" - Dates from Native Text Fallback: %d\n", native_match_count);
    if (omitted_count > 0) printf(" - Notices with NO date found: %d\n", omitted_count);

    for(int i = 0; i < notice_count; i++) free(notices[i].cleaned_content);
    for(int i = 0; i < steam_count; i++) {
        free(steam_posts[i].normalized);
        for(int j=0; j < steam_posts[i].word_count; j++) free(steam_posts[i].words[j]);
        free(steam_posts[i].words);
    }

    free(buffer);
    free(steam_buf);

    return 0;
}