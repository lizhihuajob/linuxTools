#ifndef APKCHECK_CRYPTO_H
#define APKCHECK_CRYPTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apkcheck.h"
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>

typedef struct {
    EVP_PKEY *private_key;
    X509 *certificate;
    char alias[APKCHECK_MAX_ALIAS_LEN];
} apkcheck_key_pair_t;

typedef struct {
    apkcheck_digest_alg_t alg;
    EVP_MD_CTX *ctx;
} apkcheck_digest_ctx_t;

typedef struct {
    apkcheck_sig_alg_t alg;
    EVP_MD_CTX *ctx;
    EVP_PKEY *key;
} apkcheck_sign_ctx_t;

int apkcheck_crypto_init(void);
void apkcheck_crypto_cleanup(void);

apkcheck_digest_ctx_t *apkcheck_digest_create(apkcheck_digest_alg_t alg);
void apkcheck_digest_destroy(apkcheck_digest_ctx_t *ctx);
int apkcheck_digest_update(apkcheck_digest_ctx_t *ctx, const uint8_t *data, size_t len);
int apkcheck_digest_final(apkcheck_digest_ctx_t *ctx, uint8_t *output, size_t *output_len);
int apkcheck_digest_buffer(apkcheck_digest_alg_t alg, const uint8_t *input, size_t input_len,
                           uint8_t *output, size_t *output_len);
size_t apkcheck_digest_size(apkcheck_digest_alg_t alg);

apkcheck_sign_ctx_t *apkcheck_sign_create(apkcheck_sig_alg_t alg, EVP_PKEY *private_key);
void apkcheck_sign_destroy(apkcheck_sign_ctx_t *ctx);
int apkcheck_sign_update(apkcheck_sign_ctx_t *ctx, const uint8_t *data, size_t len);
int apkcheck_sign_final(apkcheck_sign_ctx_t *ctx, uint8_t *signature, size_t *sig_len);
int apkcheck_sign_buffer(apkcheck_sig_alg_t alg, EVP_PKEY *private_key,
                         const uint8_t *input, size_t input_len,
                         uint8_t *signature, size_t *sig_len);

int apkcheck_verify_signature(apkcheck_sig_alg_t alg, X509 *cert,
                               const uint8_t *data, size_t data_len,
                               const uint8_t *signature, size_t sig_len);

EVP_PKEY *apkcheck_generate_rsa_key(int bits);
void apkcheck_key_free(EVP_PKEY *key);

X509 *apkcheck_generate_self_signed_cert(EVP_PKEY *key, const apkcheck_keygen_config_t *config);
void apkcheck_cert_free(X509 *cert);

int apkcheck_cert_get_fingerprint(X509 *cert, apkcheck_digest_alg_t alg,
                                   uint8_t *fingerprint, size_t *fp_len);
int apkcheck_cert_get_info(X509 *cert, apkcheck_cert_info_t *info);

int apkcheck_pkcs8_encrypt(EVP_PKEY *key, const char *password,
                            uint8_t **output, size_t *output_len);
EVP_PKEY *apkcheck_pkcs8_decrypt(const uint8_t *input, size_t input_len,
                                   const char *password);

int apkcheck_pkcs12_create(EVP_PKEY *key, X509 *cert, const char *password,
                            const char *friendly_name, uint8_t **output, size_t *output_len);
int apkcheck_pkcs12_parse(const uint8_t *input, size_t input_len, const char *password,
                           EVP_PKEY **key, X509 **cert);

const EVP_MD *apkcheck_digest_alg_to_evp(apkcheck_digest_alg_t alg);
const EVP_MD *apkcheck_sig_alg_to_evp(apkcheck_sig_alg_t alg);

#ifdef __cplusplus
}
#endif

#endif
