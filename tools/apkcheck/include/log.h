#ifndef APKCHECK_LOG_H
#define APKCHECK_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apkcheck.h"

typedef enum {
    APKCHECK_LOG_TARGET_CONSOLE = 1 << 0,
    APKCHECK_LOG_TARGET_FILE = 1 << 1,
    APKCHECK_LOG_TARGET_SYSLOG = 1 << 2
} apkcheck_log_target_t;

typedef void (*apkcheck_log_callback_t)(apkcheck_log_level_t level, const char *message, void *user_data);

int apkcheck_log_init(const char *log_file, apkcheck_log_level_t min_level, int targets);
void apkcheck_log_shutdown(void);

void apkcheck_log_set_level(apkcheck_log_level_t level);
apkcheck_log_level_t apkcheck_log_get_level(void);

void apkcheck_log_set_callback(apkcheck_log_callback_t callback, void *user_data);

void apkcheck_log(apkcheck_log_level_t level, const char *format, ...);
void apkcheck_log_debug(const char *format, ...);
void apkcheck_log_info(const char *format, ...);
void apkcheck_log_warn(const char *format, ...);
void apkcheck_log_error(const char *format, ...);
void apkcheck_log_fatal(const char *format, ...);

const char *apkcheck_log_level_to_string(apkcheck_log_level_t level);

#ifdef __cplusplus
}
#endif

#endif
