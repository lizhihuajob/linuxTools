#ifndef APKCHECK_UTILS_H
#define APKCHECK_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apkcheck.h"
#include <stdio.h>
#include <time.h>

int apkcheck_file_exists(const char *path);
int apkcheck_file_readable(const char *path);
int apkcheck_file_writable(const char *path);
int apkcheck_directory_exists(const char *path);

size_t apkcheck_file_size(const char *path);

int apkcheck_read_file(const char *path, apkcheck_buffer_t *buf);
int apkcheck_write_file(const char *path, const uint8_t *data, size_t len);

int apkcheck_copy_file(const char *src, const char *dst);

char *apkcheck_basename(const char *path);
char *apkcheck_dirname(const char *path);
const char *apkcheck_file_extension(const char *path);

int apkcheck_path_join(char *result, size_t max_len, const char *dir, const char *file);

int apkcheck_mkdir_p(const char *path);

void apkcheck_hex_encode(const uint8_t *input, size_t input_len, char *output, size_t output_len);
int apkcheck_hex_decode(const char *input, uint8_t *output, size_t output_len);

void apkcheck_base64_encode(const uint8_t *input, size_t input_len, char *output, size_t output_len);
int apkcheck_base64_decode(const char *input, uint8_t *output, size_t output_len);

size_t apkcheck_base64_encoded_len(size_t input_len);
size_t apkcheck_base64_decoded_len(const char *input);

int apkcheck_get_current_time_str(char *buf, size_t max_len);
int apkcheck_format_time(time_t t, char *buf, size_t max_len, const char *format);

int apkcheck_safe_printf(char *buf, size_t max_len, const char *format, ...);
int apkcheck_safe_strcpy(char *dst, size_t dst_size, const char *src);
int apkcheck_safe_strcat(char *dst, size_t dst_size, const char *src);

char *apkcheck_strdup_safe(const char *str);

int apkcheck_memcmp_constant(const uint8_t *a, const uint8_t *b, size_t len);

void apkcheck_to_lower(char *str);
void apkcheck_to_upper(char *str);

int apkcheck_str_ends_with(const char *str, const char *suffix);
int apkcheck_str_starts_with(const char *str, const char *prefix);

char *apkcheck_str_replace(const char *str, const char *old_str, const char *new_str);
void apkcheck_str_free(char *str);

int apkcheck_random_bytes(uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
