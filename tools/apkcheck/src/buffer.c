#include "apkcheck.h"
#include "log.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

apkcheck_buffer_t *apkcheck_buffer_create(size_t initial_capacity) {
    apkcheck_buffer_t *buf = (apkcheck_buffer_t *)malloc(sizeof(apkcheck_buffer_t));
    if (buf == NULL) {
        apkcheck_log_error("Failed to allocate buffer structure");
        return NULL;
    }
    
    if (initial_capacity == 0) {
        initial_capacity = 1024;
    }
    
    buf->data = (uint8_t *)malloc(initial_capacity);
    if (buf->data == NULL) {
        free(buf);
        apkcheck_log_error("Failed to allocate buffer data");
        return NULL;
    }
    
    buf->len = 0;
    buf->capacity = initial_capacity;
    return buf;
}

void apkcheck_buffer_destroy(apkcheck_buffer_t *buf) {
    if (buf == NULL) return;
    
    if (buf->data != NULL) {
        apkcheck_memzero(buf->data, buf->capacity);
        free(buf->data);
    }
    
    apkcheck_memzero(buf, sizeof(apkcheck_buffer_t));
    free(buf);
}

int apkcheck_buffer_append(apkcheck_buffer_t *buf, const uint8_t *data, size_t len) {
    if (buf == NULL || data == NULL || len == 0) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    size_t required = buf->len + len;
    if (required > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        while (new_capacity < required) {
            new_capacity *= 2;
        }
        
        uint8_t *new_data = (uint8_t *)realloc(buf->data, new_capacity);
        if (new_data == NULL) {
            apkcheck_log_error("Failed to reallocate buffer");
            return APKCHECK_ERROR_MEMORY;
        }
        
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
    
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    
    return APKCHECK_SUCCESS;
}

int apkcheck_buffer_append_string(apkcheck_buffer_t *buf, const char *str) {
    if (buf == NULL || str == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    size_t len = strlen(str);
    return apkcheck_buffer_append(buf, (const uint8_t *)str, len);
}

int apkcheck_buffer_resize(apkcheck_buffer_t *buf, size_t new_size) {
    if (buf == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (new_size > buf->capacity) {
        uint8_t *new_data = (uint8_t *)realloc(buf->data, new_size);
        if (new_data == NULL) {
            apkcheck_log_error("Failed to reallocate buffer");
            return APKCHECK_ERROR_MEMORY;
        }
        
        if (buf->len < new_size) {
            memset(new_data + buf->len, 0, new_size - buf->len);
        }
        
        buf->data = new_data;
        buf->capacity = new_size;
    }
    
    if (new_size > buf->len) {
        memset(buf->data + buf->len, 0, new_size - buf->len);
    }
    
    buf->len = new_size;
    return APKCHECK_SUCCESS;
}

void apkcheck_buffer_clear(apkcheck_buffer_t *buf) {
    if (buf == NULL || buf->data == NULL) return;
    apkcheck_memzero(buf->data, buf->len);
    buf->len = 0;
}

void apkcheck_memzero(void *ptr, size_t len) {
    if (ptr == NULL || len == 0) return;
    
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) {
        *p++ = 0;
    }
}

void apkcheck_free_sensitive(void *ptr, size_t len) {
    if (ptr == NULL) return;
    apkcheck_memzero(ptr, len);
    free(ptr);
}

const char *apkcheck_strerror(apkcheck_error_t code) {
    switch (code) {
        case APKCHECK_SUCCESS:
            return "Success";
        case APKCHECK_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case APKCHECK_ERROR_FILE_NOT_FOUND:
            return "File not found";
        case APKCHECK_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case APKCHECK_ERROR_IO:
            return "I/O error";
        case APKCHECK_ERROR_MEMORY:
            return "Memory allocation failed";
        case APKCHECK_ERROR_INVALID_FORMAT:
            return "Invalid file format";
        case APKCHECK_ERROR_CRYPTO:
            return "Cryptographic error";
        case APKCHECK_ERROR_SIGNATURE:
            return "Signature verification failed";
        case APKCHECK_ERROR_ZIP:
            return "ZIP operation error";
        case APKCHECK_ERROR_JKS:
            return "JKS keystore error";
        case APKCHECK_ERROR_TIMEOUT:
            return "Operation timed out";
        case APKCHECK_ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}
