#ifndef APKCHECK_H
#define APKCHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define APKCHECK_VERSION "1.0.0"

#define APKCHECK_MAX_PATH_LEN 4096
#define APKCHECK_MAX_PASSWORD_LEN 256
#define APKCHECK_MAX_ALIAS_LEN 256
#define APKCHECK_MAX_KEY_SIZE 4096

typedef enum {
    APKCHECK_SUCCESS = 0,
    APKCHECK_ERROR_INVALID_PARAM = -1,
    APKCHECK_ERROR_FILE_NOT_FOUND = -2,
    APKCHECK_ERROR_PERMISSION_DENIED = -3,
    APKCHECK_ERROR_IO = -4,
    APKCHECK_ERROR_MEMORY = -5,
    APKCHECK_ERROR_INVALID_FORMAT = -6,
    APKCHECK_ERROR_CRYPTO = -7,
    APKCHECK_ERROR_SIGNATURE = -8,
    APKCHECK_ERROR_ZIP = -9,
    APKCHECK_ERROR_JKS = -10,
    APKCHECK_ERROR_TIMEOUT = -11,
    APKCHECK_ERROR_UNKNOWN = -99
} apkcheck_error_t;

typedef enum {
    APKCHECK_LOG_LEVEL_DEBUG = 0,
    APKCHECK_LOG_LEVEL_INFO = 1,
    APKCHECK_LOG_LEVEL_WARN = 2,
    APKCHECK_LOG_LEVEL_ERROR = 3,
    APKCHECK_LOG_LEVEL_FATAL = 4
} apkcheck_log_level_t;

typedef enum {
    APKCHECK_DIGEST_MD5 = 0,
    APKCHECK_DIGEST_SHA1 = 1,
    APKCHECK_DIGEST_SHA256 = 2,
    APKCHECK_DIGEST_SHA512 = 3
} apkcheck_digest_alg_t;

typedef enum {
    APKCHECK_SIG_ALG_SHA1withRSA = 0,
    APKCHECK_SIG_ALG_SHA256withRSA = 1,
    APKCHECK_SIG_ALG_SHA512withRSA = 2
} apkcheck_sig_alg_t;

typedef struct {
    uint8_t *data;
    size_t len;
    size_t capacity;
} apkcheck_buffer_t;

typedef struct {
    char alias[APKCHECK_MAX_ALIAS_LEN];
    char storepass[APKCHECK_MAX_PASSWORD_LEN];
    char keypass[APKCHECK_MAX_PASSWORD_LEN];
    char common_name[256];
    char organizational_unit[256];
    char organization[256];
    char city[128];
    char state[128];
    char country[16];
    int validity_days;
    int key_size;
    apkcheck_sig_alg_t sig_alg;
} apkcheck_keygen_config_t;

typedef struct {
    char keystore_path[APKCHECK_MAX_PATH_LEN];
    char alias[APKCHECK_MAX_ALIAS_LEN];
    char storepass[APKCHECK_MAX_PASSWORD_LEN];
    char keypass[APKCHECK_MAX_PASSWORD_LEN];
    apkcheck_sig_alg_t sig_alg;
} apkcheck_sign_config_t;

typedef struct {
    char apk_path[APKCHECK_MAX_PATH_LEN];
    char output_path[APKCHECK_MAX_PATH_LEN];
    bool verify_alignment;
    apkcheck_sign_config_t sign_config;
} apkcheck_apk_config_t;

typedef struct {
    char issuer[1024];
    char subject[1024];
    char serial_number[64];
    char valid_from[64];
    char valid_to[64];
    char signature_algorithm[128];
    apkcheck_sig_alg_t sig_alg;
    uint8_t fingerprint_sha1[20];
    uint8_t fingerprint_sha256[32];
    bool is_valid;
} apkcheck_cert_info_t;

typedef struct {
    bool has_v1_signature;
    bool has_v2_signature;
    bool has_v3_signature;
    bool v1_verified;
    bool v2_verified;
    bool v3_verified;
    apkcheck_cert_info_t cert_info;
    char apk_path[APKCHECK_MAX_PATH_LEN];
} apkcheck_verify_result_t;

apkcheck_buffer_t *apkcheck_buffer_create(size_t initial_capacity);
void apkcheck_buffer_destroy(apkcheck_buffer_t *buf);
int apkcheck_buffer_append(apkcheck_buffer_t *buf, const uint8_t *data, size_t len);
int apkcheck_buffer_append_string(apkcheck_buffer_t *buf, const char *str);
int apkcheck_buffer_resize(apkcheck_buffer_t *buf, size_t new_size);
void apkcheck_buffer_clear(apkcheck_buffer_t *buf);

void apkcheck_memzero(void *ptr, size_t len);
void apkcheck_free_sensitive(void *ptr, size_t len);

const char *apkcheck_strerror(apkcheck_error_t code);

#ifdef __cplusplus
}
#endif

#endif
