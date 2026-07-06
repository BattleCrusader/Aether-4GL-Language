#include "codegen/codegen_internal.h"

/* Create directory (mkdir -p equivalent) */
int mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

#define INITIAL_CAP 262144

/* Process a raw string literal token value into actual bytes.
 * Strips surrounding quotes, processes escape sequences.
 * Returns the number of bytes written to out_buf (max max_len). */
int process_string_literal(StringView raw, char *out_buf, int max_len) {
    int di = 0;
    int i = 0;
    if (i < (int)raw.len && raw.data[i] == '"') i++;
    while (i < (int)raw.len && di < max_len) {
        char c = raw.data[i];
        if (c == '"') break;
        if (c == '\\' && i + 1 < (int)raw.len) {
            i++;
            switch (raw.data[i]) {
                case 'n':  out_buf[di++] = '\n'; break;
                case 'r':  out_buf[di++] = '\r'; break;
                case 't':  out_buf[di++] = '\t'; break;
                case '\\': out_buf[di++] = '\\'; break;
                case '"':  out_buf[di++] = '"';  break;
                case 'x': {
                    if (i + 2 < (int)raw.len && isxdigit((unsigned char)raw.data[i+1]) && isxdigit((unsigned char)raw.data[i+2])) {
                        char hex[3] = {raw.data[i+1], raw.data[i+2], 0};
                        out_buf[di++] = (char)strtoul(hex, NULL, 16);
                        i += 2;
                    }
                    break;
                }
                default:   out_buf[di++] = c; break;
            }
        } else {
            out_buf[di++] = c;
        }
        i++;
    }
    return di;
}
