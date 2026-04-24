#include "utils.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/random.h>

static const char *HEX_CHARS = "0123456789ABCDEF";
static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const int BASE64_PADDING = '=';

static int base64_char_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -1;
    return -2;
}

int apkcheck_file_exists(const char *path) {
    if (path == NULL) return 0;
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
}

int apkcheck_file_readable(const char *path) {
    if (path == NULL) return 0;
    return (access(path, R_OK) == 0) ? 1 : 0;
}

int apkcheck_file_writable(const char *path) {
    if (path == NULL) return 0;
    if (apkcheck_file_exists(path)) {
        return (access(path, W_OK) == 0) ? 1 : 0;
    }
    char *dir = apkcheck_dirname(path);
    int result = (access(dir, W_OK) == 0) ? 1 : 0;
    free(dir);
    return result;
}

int apkcheck_directory_exists(const char *path) {
    if (path == NULL) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

size_t apkcheck_file_size(const char *path) {
    if (path == NULL) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (size_t)st.st_size;
}

int apkcheck_read_file(const char *path, apkcheck_buffer_t *buf) {
    if (path == NULL || buf == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        apkcheck_log_error("Failed to open file: %s", path);
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(f);
        return APKCHECK_ERROR_IO;
    }
    
    if (apkcheck_buffer_resize(buf, (size_t)file_size) != APKCHECK_SUCCESS) {
        fclose(f);
        return APKCHECK_ERROR_MEMORY;
    }
    
    size_t read_size = fread(buf->data, 1, (size_t)file_size, f);
    fclose(f);
    
    if (read_size != (size_t)file_size) {
        apkcheck_log_error("Failed to read complete file: %s", path);
        return APKCHECK_ERROR_IO;
    }
    
    buf->len = read_size;
    return APKCHECK_SUCCESS;
}

int apkcheck_write_file(const char *path, const uint8_t *data, size_t len) {
    if (path == NULL || data == NULL || len == 0) return APKCHECK_ERROR_INVALID_PARAM;
    
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        apkcheck_log_error("Failed to create file: %s", path);
        return APKCHECK_ERROR_PERMISSION_DENIED;
    }
    
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    if (written != len) {
        apkcheck_log_error("Failed to write complete file: %s", path);
        return APKCHECK_ERROR_IO;
    }
    
    return APKCHECK_SUCCESS;
}

int apkcheck_copy_file(const char *src, const char *dst) {
    if (src == NULL || dst == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    FILE *fs = fopen(src, "rb");
    if (fs == NULL) {
        apkcheck_log_error("Failed to open source file: %s", src);
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    FILE *fd = fopen(dst, "wb");
    if (fd == NULL) {
        fclose(fs);
        apkcheck_log_error("Failed to create destination file: %s", dst);
        return APKCHECK_ERROR_PERMISSION_DENIED;
    }
    
    uint8_t buf[8192];
    size_t read_size;
    int result = APKCHECK_SUCCESS;
    
    while ((read_size = fread(buf, 1, sizeof(buf), fs)) > 0) {
        size_t written = fwrite(buf, 1, read_size, fd);
        if (written != read_size) {
            apkcheck_log_error("Failed to write to destination file");
            result = APKCHECK_ERROR_IO;
            break;
        }
    }
    
    if (ferror(fs)) {
        apkcheck_log_error("Error reading source file");
        result = APKCHECK_ERROR_IO;
    }
    
    fclose(fs);
    fclose(fd);
    
    return result;
}

char *apkcheck_basename(const char *path) {
    if (path == NULL || path[0] == '\0') return strdup("");
    
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return strdup(path);
    }
    return strdup(last_slash + 1);
}

char *apkcheck_dirname(const char *path) {
    if (path == NULL || path[0] == '\0') return strdup(".");
    
    size_t len = strlen(path);
    char *result = (char *)malloc(len + 1);
    if (result == NULL) return NULL;
    
    strcpy(result, path);
    
    while (len > 0 && result[len - 1] == '/') {
        result[--len] = '\0';
    }
    
    if (len == 0) {
        result[0] = '/';
        result[1] = '\0';
        return result;
    }
    
    char *last_slash = strrchr(result, '/');
    if (last_slash == NULL) {
        strcpy(result, ".");
    } else if (last_slash == result) {
        result[1] = '\0';
    } else {
        *last_slash = '\0';
    }
    
    return result;
}

const char *apkcheck_file_extension(const char *path) {
    if (path == NULL) return NULL;
    
    const char *last_dot = strrchr(path, '.');
    const char *last_slash = strrchr(path, '/');
    
    if (last_dot == NULL) return NULL;
    if (last_slash != NULL && last_dot < last_slash) return NULL;
    if (*(last_dot + 1) == '\0') return NULL;
    
    return last_dot + 1;
}

int apkcheck_path_join(char *result, size_t max_len, const char *dir, const char *file) {
    if (result == NULL || max_len == 0 || dir == NULL || file == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    
    if (dir_len == 0) {
        if (file_len >= max_len) return APKCHECK_ERROR_INVALID_PARAM;
        strcpy(result, file);
        return APKCHECK_SUCCESS;
    }
    
    int need_slash = (dir[dir_len - 1] != '/') && (file[0] != '/');
    size_t total_len = dir_len + (need_slash ? 1 : 0) + file_len;
    
    if (total_len >= max_len) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    strcpy(result, dir);
    if (need_slash) strcat(result, "/");
    strcat(result, file);
    
    return APKCHECK_SUCCESS;
}

int apkcheck_mkdir_p(const char *path) {
    if (path == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    char *tmp = strdup(path);
    if (tmp == NULL) return APKCHECK_ERROR_MEMORY;
    
    int result = APKCHECK_SUCCESS;
    char *p = tmp;
    
    while (*p) {
        if (*p == '/' && p != tmp) {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                result = APKCHECK_ERROR_PERMISSION_DENIED;
                break;
            }
            *p = '/';
        }
        p++;
    }
    
    if (result == APKCHECK_SUCCESS && strlen(tmp) > 0) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            result = APKCHECK_ERROR_PERMISSION_DENIED;
        }
    }
    
    free(tmp);
    return result;
}

void apkcheck_hex_encode(const uint8_t *input, size_t input_len, char *output, size_t output_len) {
    if (input == NULL || output == NULL || output_len < input_len * 2 + 1) return;
    
    for (size_t i = 0; i < input_len; i++) {
        output[i * 2] = HEX_CHARS[(input[i] >> 4) & 0x0F];
        output[i * 2 + 1] = HEX_CHARS[input[i] & 0x0F];
    }
    output[input_len * 2] = '\0';
}

int apkcheck_hex_decode(const char *input, uint8_t *output, size_t output_len) {
    if (input == NULL || output == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    size_t input_len = strlen(input);
    if (input_len % 2 != 0) return APKCHECK_ERROR_INVALID_FORMAT;
    
    size_t decoded_len = input_len / 2;
    if (output_len < decoded_len) return APKCHECK_ERROR_INVALID_PARAM;
    
    for (size_t i = 0; i < decoded_len; i++) {
        char high = tolower(input[i * 2]);
        char low = tolower(input[i * 2 + 1]);
        
        uint8_t h_val, l_val;
        
        if (high >= '0' && high <= '9') h_val = high - '0';
        else if (high >= 'a' && high <= 'f') h_val = high - 'a' + 10;
        else return APKCHECK_ERROR_INVALID_FORMAT;
        
        if (low >= '0' && low <= '9') l_val = low - '0';
        else if (low >= 'a' && low <= 'f') l_val = low - 'a' + 10;
        else return APKCHECK_ERROR_INVALID_FORMAT;
        
        output[i] = (h_val << 4) | l_val;
    }
    
    return APKCHECK_SUCCESS;
}

size_t apkcheck_base64_encoded_len(size_t input_len) {
    return ((input_len + 2) / 3) * 4 + 1;
}

void apkcheck_base64_encode(const uint8_t *input, size_t input_len, char *output, size_t output_len) {
    if (input == NULL || output == NULL) return;
    
    size_t required = apkcheck_base64_encoded_len(input_len);
    if (output_len < required) return;
    
    size_t output_idx = 0;
    size_t i = 0;
    
    while (i < input_len) {
        uint8_t byte0 = input[i++];
        uint8_t byte1 = (i < input_len) ? input[i++] : 0;
        uint8_t byte2 = (i < input_len) ? input[i++] : 0;
        
        uint32_t triple = ((uint32_t)byte0 << 16) | ((uint32_t)byte1 << 8) | byte2;
        
        output[output_idx++] = BASE64_CHARS[(triple >> 18) & 0x3F];
        output[output_idx++] = BASE64_CHARS[(triple >> 12) & 0x3F];
        
        if (i <= input_len + 1) {
            output[output_idx++] = BASE64_CHARS[(triple >> 6) & 0x3F];
        } else {
            output[output_idx++] = BASE64_PADDING;
        }
        
        if (i <= input_len) {
            output[output_idx++] = BASE64_CHARS[triple & 0x3F];
        } else {
            output[output_idx++] = BASE64_PADDING;
        }
    }
    
    output[output_idx] = '\0';
}

size_t apkcheck_base64_decoded_len(const char *input) {
    if (input == NULL) return 0;
    
    size_t input_len = strlen(input);
    if (input_len == 0) return 0;
    
    size_t padding = 0;
    while (input_len > 0 && input[input_len - 1] == BASE64_PADDING) {
        padding++;
        input_len--;
    }
    
    return (input_len * 3) / 4 - padding;
}

int apkcheck_base64_decode(const char *input, uint8_t *output, size_t output_len) {
    if (input == NULL || output == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    size_t input_len = strlen(input);
    if (input_len % 4 != 0) return APKCHECK_ERROR_INVALID_FORMAT;
    
    size_t decoded_len = apkcheck_base64_decoded_len(input);
    if (output_len < decoded_len) return APKCHECK_ERROR_INVALID_PARAM;
    
    size_t output_idx = 0;
    size_t i = 0;
    
    while (i < input_len) {
        int val[4];
        int num_valid = 0;
        
        for (int j = 0; j < 4; j++) {
            val[j] = base64_char_value(input[i + j]);
            if (val[j] == -2) return APKCHECK_ERROR_INVALID_FORMAT;
            if (val[j] >= 0) num_valid = j + 1;
        }
        
        if (num_valid >= 2) {
            uint32_t triple = ((uint32_t)val[0] << 18) | ((uint32_t)val[1] << 12);
            if (val[2] >= 0) triple |= ((uint32_t)val[2] << 6);
            if (val[3] >= 0) triple |= val[3];
            
            output[output_idx++] = (triple >> 16) & 0xFF;
            if (val[2] >= 0) output[output_idx++] = (triple >> 8) & 0xFF;
            if (val[3] >= 0) output[output_idx++] = triple & 0xFF;
        }
        
        i += 4;
    }
    
    return APKCHECK_SUCCESS;
}

int apkcheck_get_current_time_str(char *buf, size_t max_len) {
    if (buf == NULL || max_len == 0) return APKCHECK_ERROR_INVALID_PARAM;
    
    time_t now = time(NULL);
    return apkcheck_format_time(now, buf, max_len, "%Y-%m-%d %H:%M:%S");
}

int apkcheck_format_time(time_t t, char *buf, size_t max_len, const char *format) {
    if (buf == NULL || max_len == 0 || format == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    struct tm *tm_info = localtime(&t);
    if (tm_info == NULL) return APKCHECK_ERROR_IO;
    
    if (strftime(buf, max_len, format, tm_info) == 0) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    return APKCHECK_SUCCESS;
}

int apkcheck_safe_printf(char *buf, size_t max_len, const char *format, ...) {
    if (buf == NULL || max_len == 0 || format == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buf, max_len, format, args);
    va_end(args);
    
    if (result < 0 || (size_t)result >= max_len) {
        buf[max_len - 1] = '\0';
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    return APKCHECK_SUCCESS;
}

int apkcheck_safe_strcpy(char *dst, size_t dst_size, const char *src) {
    if (dst == NULL || dst_size == 0 || src == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    size_t src_len = strlen(src);
    if (src_len >= dst_size) {
        memcpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    memcpy(dst, src, src_len + 1);
    return APKCHECK_SUCCESS;
}

int apkcheck_safe_strcat(char *dst, size_t dst_size, const char *src) {
    if (dst == NULL || dst_size == 0 || src == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);
    
    if (dst_len + src_len >= dst_size) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    memcpy(dst + dst_len, src, src_len + 1);
    return APKCHECK_SUCCESS;
}

char *apkcheck_strdup_safe(const char *str) {
    if (str == NULL) return NULL;
    size_t len = strlen(str) + 1;
    char *result = (char *)malloc(len);
    if (result == NULL) return NULL;
    memcpy(result, str, len);
    return result;
}

int apkcheck_memcmp_constant(const uint8_t *a, const uint8_t *b, size_t len) {
    volatile uint8_t result = 0;
    for (size_t i = 0; i < len; i++) {
        result |= a[i] ^ b[i];
    }
    return (result != 0) ? 1 : 0;
}

void apkcheck_to_lower(char *str) {
    if (str == NULL) return;
    for (char *p = str; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
}

void apkcheck_to_upper(char *str) {
    if (str == NULL) return;
    for (char *p = str; *p; p++) {
        *p = (char)toupper((unsigned char)*p);
    }
}

int apkcheck_str_ends_with(const char *str, const char *suffix) {
    if (str == NULL || suffix == NULL) return 0;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return 0;
    
    return (strcmp(str + str_len - suffix_len, suffix) == 0) ? 1 : 0;
}

int apkcheck_str_starts_with(const char *str, const char *prefix) {
    if (str == NULL || prefix == NULL) return 0;
    
    size_t str_len = strlen(str);
    size_t prefix_len = strlen(prefix);
    
    if (prefix_len > str_len) return 0;
    
    return (strncmp(str, prefix, prefix_len) == 0) ? 1 : 0;
}

char *apkcheck_str_replace(const char *str, const char *old_str, const char *new_str) {
    if (str == NULL || old_str == NULL || new_str == NULL) return NULL;
    
    size_t str_len = strlen(str);
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    
    if (old_len == 0) return apkcheck_strdup_safe(str);
    
    int count = 0;
    const char *p = str;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }
    
    if (count == 0) return apkcheck_strdup_safe(str);
    
    size_t result_len = str_len + count * (new_len - old_len);
    char *result = (char *)malloc(result_len + 1);
    if (result == NULL) return NULL;
    
    char *dst = result;
    const char *start = str;
    const char *end;
    
    while ((end = strstr(start, old_str)) != NULL) {
        size_t segment_len = (size_t)(end - start);
        memcpy(dst, start, segment_len);
        dst += segment_len;
        memcpy(dst, new_str, new_len);
        dst += new_len;
        start = end + old_len;
    }
    
    size_t remaining_len = strlen(start);
    memcpy(dst, start, remaining_len + 1);
    
    return result;
}

void apkcheck_str_free(char *str) {
    if (str != NULL) {
        apkcheck_memzero(str, strlen(str));
        free(str);
    }
}

int apkcheck_random_bytes(uint8_t *buf, size_t len) {
    if (buf == NULL || len == 0) return APKCHECK_ERROR_INVALID_PARAM;
    
    ssize_t read_len = getrandom(buf, len, 0);
    if (read_len < 0 || (size_t)read_len != len) {
        unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
        for (size_t i = 0; i < len; i++) {
            buf[i] = (uint8_t)(rand_r(&seed) % 256);
        }
        apkcheck_log_warn("Using fallback random number generator");
    }
    
    return APKCHECK_SUCCESS;
}
