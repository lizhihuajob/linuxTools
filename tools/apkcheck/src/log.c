#include "log.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <syslog.h>

static FILE *log_file = NULL;
static apkcheck_log_level_t min_log_level = APKCHECK_LOG_LEVEL_INFO;
static int log_targets = APKCHECK_LOG_TARGET_CONSOLE;
static apkcheck_log_callback_t log_callback = NULL;
static void *log_callback_user_data = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *log_level_names[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

static int log_level_to_syslog[] = {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERR,
    LOG_CRIT
};

int apkcheck_log_init(const char *log_path, apkcheck_log_level_t min_level, int targets) {
    pthread_mutex_lock(&log_mutex);
    
    min_log_level = min_level;
    log_targets = targets;
    
    if (log_targets & APKCHECK_LOG_TARGET_SYSLOG) {
        openlog("apkcheck", LOG_PID | LOG_CONS, LOG_USER);
    }
    
    if ((log_targets & APKCHECK_LOG_TARGET_FILE) && log_path != NULL) {
        log_file = fopen(log_path, "a");
        if (log_file == NULL) {
            pthread_mutex_unlock(&log_mutex);
            return APKCHECK_ERROR_PERMISSION_DENIED;
        }
    }
    
    pthread_mutex_unlock(&log_mutex);
    return APKCHECK_SUCCESS;
}

void apkcheck_log_shutdown(void) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_targets & APKCHECK_LOG_TARGET_SYSLOG) {
        closelog();
    }
    
    if (log_file != NULL) {
        fflush(log_file);
        fclose(log_file);
        log_file = NULL;
    }
    
    log_callback = NULL;
    log_callback_user_data = NULL;
    
    pthread_mutex_unlock(&log_mutex);
}

void apkcheck_log_set_level(apkcheck_log_level_t level) {
    pthread_mutex_lock(&log_mutex);
    min_log_level = level;
    pthread_mutex_unlock(&log_mutex);
}

apkcheck_log_level_t apkcheck_log_get_level(void) {
    return min_log_level;
}

void apkcheck_log_set_callback(apkcheck_log_callback_t callback, void *user_data) {
    pthread_mutex_lock(&log_mutex);
    log_callback = callback;
    log_callback_user_data = user_data;
    pthread_mutex_unlock(&log_mutex);
}

const char *apkcheck_log_level_to_string(apkcheck_log_level_t level) {
    if (level < APKCHECK_LOG_LEVEL_DEBUG || level > APKCHECK_LOG_LEVEL_FATAL) {
        return "UNKNOWN";
    }
    return log_level_names[level];
}

void apkcheck_log(apkcheck_log_level_t level, const char *format, ...) {
    if (level < min_log_level) return;
    if (format == NULL) return;
    
    pthread_mutex_lock(&log_mutex);
    
    char time_buf[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info != NULL) {
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        strcpy(time_buf, "????-??-?? ??:??:??");
    }
    
    char message[8192];
    va_list args;
    va_start(args, format);
    int msg_len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    if (msg_len < 0) {
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    
    if (log_callback != NULL) {
        log_callback(level, message, log_callback_user_data);
    }
    
    if (log_targets & APKCHECK_LOG_TARGET_CONSOLE) {
        const char *level_str = log_level_names[level];
        FILE *out = (level >= APKCHECK_LOG_LEVEL_ERROR) ? stderr : stdout;
        fprintf(out, "[%s] [%s] %s\n", time_buf, level_str, message);
        fflush(out);
    }
    
    if ((log_targets & APKCHECK_LOG_TARGET_FILE) && log_file != NULL) {
        const char *level_str = log_level_names[level];
        fprintf(log_file, "[%s] [%s] %s\n", time_buf, level_str, message);
        fflush(log_file);
    }
    
    if (log_targets & APKCHECK_LOG_TARGET_SYSLOG) {
        int syslog_level = log_level_to_syslog[level];
        syslog(syslog_level, "%s", message);
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void apkcheck_log_debug(const char *format, ...) {
    if (APKCHECK_LOG_LEVEL_DEBUG < min_log_level) return;
    
    va_list args;
    va_start(args, format);
    
    char message[8192];
    int msg_len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    if (msg_len >= 0) {
        apkcheck_log(APKCHECK_LOG_LEVEL_DEBUG, "%s", message);
    }
}

void apkcheck_log_info(const char *format, ...) {
    if (APKCHECK_LOG_LEVEL_INFO < min_log_level) return;
    
    va_list args;
    va_start(args, format);
    
    char message[8192];
    int msg_len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    if (msg_len >= 0) {
        apkcheck_log(APKCHECK_LOG_LEVEL_INFO, "%s", message);
    }
}

void apkcheck_log_warn(const char *format, ...) {
    if (APKCHECK_LOG_LEVEL_WARN < min_log_level) return;
    
    va_list args;
    va_start(args, format);
    
    char message[8192];
    int msg_len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    if (msg_len >= 0) {
        apkcheck_log(APKCHECK_LOG_LEVEL_WARN, "%s", message);
    }
}

void apkcheck_log_error(const char *format, ...) {
    if (APKCHECK_LOG_LEVEL_ERROR < min_log_level) return;
    
    va_list args;
    va_start(args, format);
    
    char message[8192];
    int msg_len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    if (msg_len >= 0) {
        apkcheck_log(APKCHECK_LOG_LEVEL_ERROR, "%s", message);
    }
}

void apkcheck_log_fatal(const char *format, ...) {
    if (APKCHECK_LOG_LEVEL_FATAL < min_log_level) return;
    
    va_list args;
    va_start(args, format);
    
    char message[8192];
    int msg_len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    if (msg_len >= 0) {
        apkcheck_log(APKCHECK_LOG_LEVEL_FATAL, "%s", message);
    }
}
