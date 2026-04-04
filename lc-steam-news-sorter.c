#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NOTICES 5000

// Usage: ./lc-steam-news-sorter lcsn-MASTER.txt full_steamlist.txt
// Compile: gcc -O3 lc-steam-news-sorter.c -o lc-steam-news-sorter

typedef struct {
    char actual_name[256];
    char steam_name[256];
} Override;

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
    char *title_normalized;
    char *raw_text;
} SteamPost;

Notice *notices;
int notice_count = 0;

SteamPost *steam_posts;
int steam_count = 0;

Override *overrides;
int override_count = 0;

int g_is_input_ascending = 0;

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

void load_overrides(const char* filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("No manual_overrides.csv found. Proceeding without overrides.\n");
        return;
    }
    char line[1024];
    while(fgets(line, sizeof(line), fp)) {
        if(line[0] == '\n' || line[0] == '\r') continue;
        char* delim = strstr(line, "^¬^");
        if(delim) {
            *delim = '\0'; 
            char* steam_name = delim + strlen("^¬^");
            
            char* nl = strchr(steam_name, '\n'); if(nl) *nl = '\0';
            nl = strchr(steam_name, '\r'); if(nl) *nl = '\0';
            
            strncpy(overrides[override_count].actual_name, line, 255);
            strncpy(overrides[override_count].steam_name, steam_name, 255);
            override_count++;
        }
    }
    fclose(fp);
    printf("Loaded %d manual overrides.\n", override_count);
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
        if (isalnum((unsigned char)src[i])) dest[j++] = tolower((unsigned char)src[i]);
    }
    dest[j] = '\0';
}

int parse_steam_date(const char* block, int* y, int* m, int* d) {
    *y = 0; *m = 0; *d = 0;
    const char* months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    const char* short_months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    for(int i=0; i<12; i++) {
        const char* ptr = custom_stristr(block, months[i]);
        int len = 0;
        
        if (ptr) {
            len = strlen(months[i]);
        } else {
            ptr = custom_stristr(block, short_months[i]);
            if (ptr) len = strlen(short_months[i]);
        }
        
        if (ptr) {
            *m = i + 1;
            ptr += len;
            while(*ptr == ' ' || *ptr == '.') ptr++; 
            
            int parsed_d = 0, parsed_y = 0;
            if (sscanf(ptr, "%d , %d", &parsed_d, &parsed_y) == 2 ||
                sscanf(ptr, "%d, %d", &parsed_d, &parsed_y) == 2) {
                *d = parsed_d; *y = parsed_y; return 1;
            } else if (sscanf(ptr, "%d", &parsed_d) == 1) {
                *d = parsed_d; return 1;
            }
        }
    }
    return 0;
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
                strncmp(after_d, "rd", 2)==0 || strncmp(after_d, "th", 2)==0) after_d += 2;
            if (sscanf(after_d, ", %4d", &y) == 1) {
                 if (y >= 2023 && d >= 1 && d <= 31 && (m = parse_month(m_str))) { *year = y; *month = m; *day = d; return; }
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
    if (g_is_input_ascending) return n2->original_id - n1->original_id;
    return n1->original_id - n2->original_id;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("\nUsage: %s <input_notices.txt> <steam_full_list.txt>\n", argv[0]);
        return 1;
    }

    notices = calloc(MAX_NOTICES, sizeof(Notice));
    steam_posts = calloc(MAX_NOTICES, sizeof(SteamPost));
    overrides = calloc(1000, sizeof(Override));
    
    if (!notices || !steam_posts || !overrides) return 1;

    load_overrides("manual_overrides.csv");

    char *buffer = load_file(argv[1]);
    char *steam_buf = load_file(argv[2]);
    if (!buffer || !steam_buf) return 1;

    int current_year = 2026; 
    char* yr_ptr = strstr(steam_buf, "202");
    if (yr_ptr) {
        int tmp = 0;
        if (sscanf(yr_ptr, "%4d", &tmp) == 1 && tmp >= 2023 && tmp <= 2030) current_year = tmp;
    }

    char* p = steam_buf;
    char* s_next;
    while ((s_next = strstr(p, "Share"))) {
        *s_next = '\0'; 
        char* blk_yr = strstr(p, "202");
        if (blk_yr) {
            int tmp = 0;
            if (sscanf(blk_yr, "%4d", &tmp) == 1 && tmp >= 2023 && tmp <= 2030) current_year = tmp;
        }

        steam_posts[steam_count].title_normalized = malloc(512);
        steam_posts[steam_count].title_normalized[0] = '\0';
        
        char temp_p[2048] = {0};
        strncpy(temp_p, p, 2047);
        char *line = strtok(temp_p, "\n");
        int line_idx = 0;
        int posted_idx = -1;
        int date_idx = -1;
        int ran_idx = -1;
        char date_line[256] = {0}; 
        
        while(line) {
            char *r = strchr(line, '\r'); if (r) *r = '\0';
            char *start = line; while(*start == ' ' || *start == '\t') start++;
            
            if (strcmp(start, "Posted") == 0) {
                posted_idx = line_idx;
            } else if (posted_idx != -1 && line_idx == posted_idx + 1) {
                strncpy(date_line, start, 255); 
                date_idx = line_idx;
            } else if (date_idx != -1 && line_idx == date_idx + 1) {
                if (strcmp(start, "Ran") == 0) {
                    ran_idx = line_idx;
                } else {
                    normalize_text(steam_posts[steam_count].title_normalized, start, 512);
                    break;
                }
            } else if (ran_idx != -1 && line_idx == ran_idx + 2) {
                normalize_text(steam_posts[steam_count].title_normalized, start, 512);
                break;
            }
            line_idx++; line = strtok(NULL, "\n");
        }

        if (date_line[0] == '\0') strncpy(date_line, p, 255);

        int y = 0, m = 0, d = 0;
        if (parse_steam_date(date_line, &y, &m, &d)) {
            if (y == 0) y = current_year;
            else current_year = y; 
        }

        steam_posts[steam_count].y = y;
        steam_posts[steam_count].m = m;
        steam_posts[steam_count].d = d;
        steam_posts[steam_count].raw_text = strdup(p); 
        steam_posts[steam_count].normalized = malloc(strlen(p) + 1);
        normalize_text(steam_posts[steam_count].normalized, p, strlen(p) + 1);
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

            while (body_start < body_end && isspace((unsigned char)*body_start)) body_start++;
            char *actual_end = body_end - 1;
            while (actual_end >= body_start && isspace((unsigned char)*actual_end)) actual_end--;

            long body_len = actual_end - body_start + 1;
            if (body_len > 0 && notice_count < MAX_NOTICES) {
                char *raw_body = malloc(body_len + 1);
                strncpy(raw_body, body_start, body_len);
                raw_body[body_len] = '\0';

                notices[notice_count].original_id = notice_count;
                notices[notice_count].parsed_label_num = label_num;
                notices[notice_count].cleaned_content = clean_spacing(raw_body);
                notices[notice_count].chrono_score = -1.0;
                notice_count++;
                free(raw_body);
            }
        }
        current = next;
    }

    for (int i = 0; i < notice_count; i++) {
        char title[512] = {0};
        const char *nl = strchr(notices[i].cleaned_content, '\n');
        if (nl) {
            int len = nl - notices[i].cleaned_content;
            if (len > 511) len = 511;
            strncpy(title, notices[i].cleaned_content, len);
        } else {
            strncpy(title, notices[i].cleaned_content, 511);
        }
        
        char clean_title[512] = {0};
        int ct = 0;
        for (int k = 0; title[k] && ct < 511; k++) {
            if (title[k] != '\'' && title[k] != '"' && (unsigned char)title[k] != 0xE2 && (unsigned char)title[k] != 0x80 && (unsigned char)title[k] != 0x98 && (unsigned char)title[k] != 0x99) {
                clean_title[ct++] = title[k];
            }
        }
        clean_title[ct] = '\0';

        int best_match = -1;
        
        for (int o = 0; o < override_count; o++) {
            if (custom_stristr(clean_title, overrides[o].actual_name) != NULL || custom_stristr(title, overrides[o].actual_name) != NULL) {
                for (int s = 0; s < steam_count; s++) {
                    if (custom_stristr(steam_posts[s].raw_text, overrides[o].steam_name) != NULL) {
                        best_match = s;
                        printf("Manual Override Applied: '%s'\n", title);
                        break;
                    }
                }
                if (best_match != -1) break;
            }
        }

        if (best_match == -1) {
            char notice_norm_150[151] = {0};
            normalize_text(notice_norm_150, notices[i].cleaned_content, 150);
            char title_norm[256] = {0};
            normalize_text(title_norm, title, 256);
            
            int best_len_diff = 999999;
            for (int j = 0; j < steam_count; j++) {
                int is_match = 0;
                int len_diff = 999999;
                
                if (strlen(title_norm) > 8 && steam_posts[j].title_normalized[0] != '\0' && strstr(steam_posts[j].title_normalized, title_norm)) {
                    is_match = 1;
                    len_diff = abs((int)strlen(steam_posts[j].title_normalized) - (int)strlen(title_norm));
                }
                else if (strlen(title_norm) > 8 && strstr(steam_posts[j].normalized, title_norm)) {
                    is_match = 1;
                    len_diff = 5000; 
                } 
                else if (strlen(notice_norm_150) > 20 && strstr(steam_posts[j].normalized, notice_norm_150)) {
                    is_match = 1;
                    len_diff = 10000; 
                }
                
                if (is_match && len_diff < best_len_diff) {
                    best_len_diff = len_diff;
                    best_match = j;
                }
            }
        }
        
        if (best_match >= 0 && steam_posts[best_match].y > 0) {
            notices[i].y = steam_posts[best_match].y;
            notices[i].m = steam_posts[best_match].m;
            notices[i].d = steam_posts[best_match].d;
            notices[i].chrono_score = (notices[i].y * 10000.0) + (notices[i].m * 100.0) + notices[i].d + (1.0 - ((double)best_match / 10000.0));
        } else {
            extract_date_fallback(notices[i].cleaned_content, &notices[i].y, &notices[i].m, &notices[i].d);
            if (notices[i].m > 0 && notices[i].y == 0) notices[i].y = 2024; 
            if (notices[i].m > 0) {
                notices[i].chrono_score = notices[i].y * 10000.0 + notices[i].m * 100.0 + notices[i].d;
            }
        }
    }

    int first_valid = -1, last_valid = -1;
    for (int i = 0; i < notice_count; i++) {
        if (notices[i].chrono_score >= 0.0) {
            if (first_valid == -1) first_valid = i;
            last_valid = i;
        }
    }
    if (first_valid != -1 && last_valid != -1 && first_valid != last_valid) {
        g_is_input_ascending = (notices[first_valid].chrono_score < notices[last_valid].chrono_score);
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
    for (int i = 0; i < notice_count; i++) {
        notices[i].new_label = notice_count - i;
        fprintf(out, "Writing Sample %d:\n", notices[i].new_label);
        fprintf(out, "%s\n\n\n", notices[i].cleaned_content ? notices[i].cleaned_content : "");
    }
    fclose(out);
    
    FILE *dates_out = fopen("lcsn-dates.tsv", "w");
    for (int i = 0; i < notice_count; i++) {
        int out_y = notices[i].y;
        int out_m = notices[i].m;
        int out_d = notices[i].d;
        
        if (out_y == 0 || out_m == 0 || out_d == 0) {
            long score = (long)notices[i].chrono_score;
            out_y = score / 10000;
            out_m = (score % 10000) / 100;
            out_d = score % 100;
        }
        
        if (out_y > 0 && out_m > 0 && out_d > 0) {
            fprintf(dates_out, "%d\t%04d-%02d-%02d\n", notices[i].new_label, out_y, out_m, out_d);
        }
    }
    fclose(dates_out);

    free(notices);
    for(int i=0; i<steam_count; i++) {
        if (steam_posts[i].title_normalized) free(steam_posts[i].title_normalized);
        if (steam_posts[i].normalized) free(steam_posts[i].normalized);
        if (steam_posts[i].raw_text) free(steam_posts[i].raw_text);
    }
    free(steam_posts);
    free(overrides);

    printf("\nSuccess! Generated 'lcsn-sorted_notices.txt' and sidecar 'lcsn-dates.tsv'.\n");
    return 0;
}