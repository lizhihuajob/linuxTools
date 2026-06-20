#ifndef APKCHECK_JKS_H
#define APKCHECK_JKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apkcheck.h"
#include "crypto.h"
#include <openssl/x509.h>

#define JKS_MAGIC 0xFEEDFEED
#define JKS_VERSION 2

#define JKS_ENTRY_TYPE_PRIVATE_KEY 1
#define JKS_ENTRY_TYPE_TRUSTED_CERT 2
#define JKS_ENTRY_TYPE_SECRET_KEY 3

typedef struct {
    int type;
    char alias[APKCHECK_MAX_ALIAS_LEN];
    time_t creation_date;
} jks_entry_header_t;

typedef struct {
    jks_entry_header_t header;
    EVP_PKEY *private_key;
    X509 **cert_chain;
    int cert_chain_len;
} jks_private_key_entry_t;

typedef struct {
    jks_entry_header_t header;
    char cert_type[256];
    X509 *cert;
} jks_trusted_cert_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    int entry_count;
    jks_entry_header_t **entries;
    char password[APKCHECK_MAX_PASSWORD_LEN];
} jks_keystore_t;

jks_keystore_t *jks_keystore_create(const char *password);
void jks_keystore_destroy(jks_keystore_t *ks);

int jks_keystore_load(jks_keystore_t **ks, const char *path, const char *password);
int jks_keystore_save(jks_keystore_t *ks, const char *path);

int jks_keystore_add_private_key(jks_keystore_t *ks, const char *alias,
                                     EVP_PKEY *key, const char *key_password,
                                     X509 **cert_chain, int cert_chain_len);
int jks_keystore_add_trusted_cert(jks_keystore_t *ks, const char *alias,
                                      const char *cert_type, X509 *cert);

int jks_keystore_get_private_key(jks_keystore_t *ks, const char *alias,
                                   const char *key_password,
                                   EVP_PKEY **key, X509 ***cert_chain,
                                   int *cert_chain_len);
int jks_keystore_get_trusted_cert(jks_keystore_t *ks, const char *alias,
                                    X509 **cert);

int jks_keystore_contains_alias(jks_keystore_t *ks, const char *alias);
int jks_keystore_list_aliases(jks_keystore_t *ks, char ***aliases, int *count);
int jks_keystore_delete_entry(jks_keystore_t *ks, const char *alias);

int jks_generate_keystore(const char *path, const char *store_password,
                         const apkcheck_keygen_config_t *config);

int jks_compute_mac(const uint8_t *data, size_t len, const char *password,
                      uint8_t *mac, size_t *mac_len);

int jks_encrypt_private_key(EVP_PKEY *key, const char *password,
                               uint8_t **output, size_t *output_len);
EVP_PKEY *jks_decrypt_private_key(const uint8_t *input, size_t input_len,
                                    const char *password);

#ifdef __cplusplus
}
#endif

#endif
