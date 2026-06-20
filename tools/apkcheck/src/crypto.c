#include "crypto.h"
#include "log.h"
#include "utils.h"
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>
#include <string.h>

int apkcheck_crypto_init(void) {
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
    OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS, NULL);
    OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);
    
    if (RAND_status() != 1) {
        unsigned char seed[32];
        if (apkcheck_random_bytes(seed, sizeof(seed)) == APKCHECK_SUCCESS) {
            RAND_seed(seed, sizeof(seed));
            apkcheck_memzero(seed, sizeof(seed));
        }
    }
    
    return APKCHECK_SUCCESS;
}

void apkcheck_crypto_cleanup(void) {
    OPENSSL_cleanup();
}

apkcheck_digest_ctx_t *apkcheck_digest_create(apkcheck_digest_alg_t alg) {
    apkcheck_digest_ctx_t *ctx = (apkcheck_digest_ctx_t *)malloc(sizeof(apkcheck_digest_ctx_t));
    if (ctx == NULL) {
        apkcheck_log_error("Failed to allocate digest context");
        return NULL;
    }
    
    ctx->alg = alg;
    ctx->ctx = EVP_MD_CTX_new();
    
    if (ctx->ctx == NULL) {
        free(ctx);
        apkcheck_log_error("Failed to create EVP_MD_CTX");
        return NULL;
    }
    
    const EVP_MD *md = apkcheck_digest_alg_to_evp(alg);
    if (md == NULL) {
        EVP_MD_CTX_free(ctx->ctx);
        free(ctx);
        apkcheck_log_error("Unknown digest algorithm");
        return NULL;
    }
    
    if (EVP_DigestInit_ex(ctx->ctx, md, NULL) != 1) {
        EVP_MD_CTX_free(ctx->ctx);
        free(ctx);
        apkcheck_log_error("Failed to initialize digest");
        return NULL;
    }
    
    return ctx;
}

void apkcheck_digest_destroy(apkcheck_digest_ctx_t *ctx) {
    if (ctx == NULL) return;
    if (ctx->ctx != NULL) {
        EVP_MD_CTX_free(ctx->ctx);
    }
    apkcheck_memzero(ctx, sizeof(apkcheck_digest_ctx_t));
    free(ctx);
}

int apkcheck_digest_update(apkcheck_digest_ctx_t *ctx, const uint8_t *data, size_t len) {
    if (ctx == NULL || ctx->ctx == NULL || data == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (len == 0) return APKCHECK_SUCCESS;
    
    if (EVP_DigestUpdate(ctx->ctx, data, len) != 1) {
        apkcheck_log_error("Digest update failed");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    return APKCHECK_SUCCESS;
}

int apkcheck_digest_final(apkcheck_digest_ctx_t *ctx, uint8_t *output, size_t *output_len) {
    if (ctx == NULL || ctx->ctx == NULL || output == NULL || output_len == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    unsigned int md_len;
    if (EVP_DigestFinal_ex(ctx->ctx, output, &md_len) != 1) {
        apkcheck_log_error("Digest final failed");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    *output_len = (size_t)md_len;
    return APKCHECK_SUCCESS;
}

int apkcheck_digest_buffer(apkcheck_digest_alg_t alg, const uint8_t *input, size_t input_len,
                           uint8_t *output, size_t *output_len) {
    if (input == NULL || output == NULL || output_len == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    const EVP_MD *md = apkcheck_digest_alg_to_evp(alg);
    if (md == NULL) {
        apkcheck_log_error("Unknown digest algorithm");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        apkcheck_log_error("Failed to create digest context");
        return APKCHECK_ERROR_MEMORY;
    }
    
    int result = APKCHECK_SUCCESS;
    unsigned int md_len;
    
    if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
        EVP_DigestUpdate(ctx, input, input_len) != 1 ||
        EVP_DigestFinal_ex(ctx, output, &md_len) != 1) {
        apkcheck_log_error("Digest operation failed");
        result = APKCHECK_ERROR_CRYPTO;
    } else {
        *output_len = (size_t)md_len;
    }
    
    EVP_MD_CTX_free(ctx);
    return result;
}

size_t apkcheck_digest_size(apkcheck_digest_alg_t alg) {
    const EVP_MD *md = apkcheck_digest_alg_to_evp(alg);
    if (md == NULL) return 0;
    return (size_t)EVP_MD_size(md);
}

apkcheck_sign_ctx_t *apkcheck_sign_create(apkcheck_sig_alg_t alg, EVP_PKEY *private_key) {
    if (private_key == NULL) {
        apkcheck_log_error("Private key is NULL");
        return NULL;
    }
    
    apkcheck_sign_ctx_t *ctx = (apkcheck_sign_ctx_t *)malloc(sizeof(apkcheck_sign_ctx_t));
    if (ctx == NULL) {
        apkcheck_log_error("Failed to allocate signature context");
        return NULL;
    }
    
    ctx->alg = alg;
    ctx->key = private_key;
    ctx->ctx = EVP_MD_CTX_new();
    
    if (ctx->ctx == NULL) {
        free(ctx);
        apkcheck_log_error("Failed to create EVP_MD_CTX");
        return NULL;
    }
    
    const EVP_MD *md = apkcheck_sig_alg_to_evp(alg);
    if (md == NULL) {
        EVP_MD_CTX_free(ctx->ctx);
        free(ctx);
        apkcheck_log_error("Unknown signature algorithm");
        return NULL;
    }
    
    if (EVP_DigestSignInit(ctx->ctx, NULL, md, NULL, private_key) != 1) {
        EVP_MD_CTX_free(ctx->ctx);
        free(ctx);
        apkcheck_log_error("Failed to initialize signature");
        return NULL;
    }
    
    return ctx;
}

void apkcheck_sign_destroy(apkcheck_sign_ctx_t *ctx) {
    if (ctx == NULL) return;
    if (ctx->ctx != NULL) {
        EVP_MD_CTX_free(ctx->ctx);
    }
    apkcheck_memzero(ctx, sizeof(apkcheck_sign_ctx_t));
    free(ctx);
}

int apkcheck_sign_update(apkcheck_sign_ctx_t *ctx, const uint8_t *data, size_t len) {
    if (ctx == NULL || ctx->ctx == NULL || data == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (len == 0) return APKCHECK_SUCCESS;
    
    if (EVP_DigestSignUpdate(ctx->ctx, data, len) != 1) {
        apkcheck_log_error("Signature update failed");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    return APKCHECK_SUCCESS;
}

int apkcheck_sign_final(apkcheck_sign_ctx_t *ctx, uint8_t *signature, size_t *sig_len) {
    if (ctx == NULL || ctx->ctx == NULL || signature == NULL || sig_len == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    size_t len = *sig_len;
    if (EVP_DigestSignFinal(ctx->ctx, signature, &len) != 1) {
        apkcheck_log_error("Signature final failed");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    *sig_len = len;
    return APKCHECK_SUCCESS;
}

int apkcheck_sign_buffer(apkcheck_sig_alg_t alg, EVP_PKEY *private_key,
                         const uint8_t *input, size_t input_len,
                         uint8_t *signature, size_t *sig_len) {
    if (private_key == NULL || input == NULL || signature == NULL || sig_len == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    const EVP_MD *md = apkcheck_sig_alg_to_evp(alg);
    if (md == NULL) {
        apkcheck_log_error("Unknown signature algorithm");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        apkcheck_log_error("Failed to create signature context");
        return APKCHECK_ERROR_MEMORY;
    }
    
    int result = APKCHECK_SUCCESS;
    size_t len = *sig_len;
    
    if (EVP_DigestSignInit(ctx, NULL, md, NULL, private_key) != 1 ||
        EVP_DigestSignUpdate(ctx, input, input_len) != 1 ||
        EVP_DigestSignFinal(ctx, signature, &len) != 1) {
        apkcheck_log_error("Signature operation failed");
        result = APKCHECK_ERROR_CRYPTO;
    } else {
        *sig_len = len;
    }
    
    EVP_MD_CTX_free(ctx);
    return result;
}

int apkcheck_verify_signature(apkcheck_sig_alg_t alg, X509 *cert,
                               const uint8_t *data, size_t data_len,
                               const uint8_t *signature, size_t sig_len) {
    if (cert == NULL || data == NULL || signature == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    EVP_PKEY *pubkey = X509_get_pubkey(cert);
    if (pubkey == NULL) {
        apkcheck_log_error("Failed to get public key from certificate");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    const EVP_MD *md = apkcheck_sig_alg_to_evp(alg);
    if (md == NULL) {
        EVP_PKEY_free(pubkey);
        apkcheck_log_error("Unknown signature algorithm");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        EVP_PKEY_free(pubkey);
        apkcheck_log_error("Failed to create verify context");
        return APKCHECK_ERROR_MEMORY;
    }
    
    int result = APKCHECK_ERROR_SIGNATURE;
    
    if (EVP_DigestVerifyInit(ctx, NULL, md, NULL, pubkey) == 1 &&
        EVP_DigestVerifyUpdate(ctx, data, data_len) == 1 &&
        EVP_DigestVerifyFinal(ctx, signature, sig_len) == 1) {
        result = APKCHECK_SUCCESS;
    } else {
        apkcheck_log_debug("Signature verification failed");
    }
    
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pubkey);
    
    return result;
}

EVP_PKEY *apkcheck_generate_rsa_key(int bits) {
    EVP_PKEY *pkey = EVP_PKEY_new();
    if (pkey == NULL) {
        apkcheck_log_error("Failed to create EVP_PKEY");
        return NULL;
    }
    
    BIGNUM *e = BN_new();
    if (e == NULL) {
        EVP_PKEY_free(pkey);
        apkcheck_log_error("Failed to create BIGNUM");
        return NULL;
    }
    
    BN_set_word(e, RSA_F4);
    
    RSA *rsa = RSA_new();
    if (rsa == NULL || RSA_generate_key_ex(rsa, bits, e, NULL) != 1) {
        BN_free(e);
        if (rsa != NULL) RSA_free(rsa);
        EVP_PKEY_free(pkey);
        apkcheck_log_error("Failed to generate RSA key");
        return NULL;
    }
    
    BN_free(e);
    
    if (EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
        RSA_free(rsa);
        EVP_PKEY_free(pkey);
        apkcheck_log_error("Failed to assign RSA key to EVP_PKEY");
        return NULL;
    }
    
    return pkey;
}

void apkcheck_key_free(EVP_PKEY *key) {
    if (key != NULL) {
        EVP_PKEY_free(key);
    }
}

static int add_ext(X509 *cert, int nid, char *value) {
    X509_EXTENSION *ex = NULL;
    X509V3_CTX ctx;
    
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
    ex = X509V3_EXT_conf_nid(NULL, &ctx, nid, value);
    
    if (!ex) {
        apkcheck_log_error("Failed to create extension %d", nid);
        return 0;
    }
    
    X509_add_ext(cert, ex, -1);
    X509_EXTENSION_free(ex);
    return 1;
}

X509 *apkcheck_generate_self_signed_cert(EVP_PKEY *key, const apkcheck_keygen_config_t *config) {
    if (key == NULL || config == NULL) {
        apkcheck_log_error("Invalid parameters for certificate generation");
        return NULL;
    }
    
    X509 *cert = X509_new();
    if (cert == NULL) {
        apkcheck_log_error("Failed to create X509 certificate");
        return NULL;
    }
    
    if (X509_set_version(cert, 2) != 1) {
        X509_free(cert);
        apkcheck_log_error("Failed to set certificate version");
        return NULL;
    }
    
    BIGNUM *serial = BN_new();
    if (serial == NULL) {
        X509_free(cert);
        apkcheck_log_error("Failed to create serial number");
        return NULL;
    }
    
    unsigned char serial_bytes[20];
    if (apkcheck_random_bytes(serial_bytes, sizeof(serial_bytes)) != APKCHECK_SUCCESS) {
        BN_free(serial);
        X509_free(cert);
        apkcheck_log_error("Failed to generate serial number");
        return NULL;
    }
    
    BN_bin2bn(serial_bytes, sizeof(serial_bytes), serial);
    if (X509_set_serialNumber(cert, serial) != 1) {
        BN_free(serial);
        X509_free(cert);
        apkcheck_log_error("Failed to set serial number");
        return NULL;
    }
    BN_free(serial);
    
    X509_gmtime_adj(X509_getm_notBefore(cert), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert), (long)config->validity_days * 24 * 3600);
    
    if (X509_set_pubkey(cert, key) != 1) {
        X509_free(cert);
        apkcheck_log_error("Failed to set public key");
        return NULL;
    }
    
    X509_NAME *name = X509_get_subject_name(cert);
    
    if (config->common_name[0] != '\0') {
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                    (unsigned char *)config->common_name, -1, -1, 0);
    }
    if (config->organizational_unit[0] != '\0') {
        X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC,
                                    (unsigned char *)config->organizational_unit, -1, -1, 0);
    }
    if (config->organization[0] != '\0') {
        X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                                    (unsigned char *)config->organization, -1, -1, 0);
    }
    if (config->city[0] != '\0') {
        X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC,
                                    (unsigned char *)config->city, -1, -1, 0);
    }
    if (config->state[0] != '\0') {
        X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC,
                                    (unsigned char *)config->state, -1, -1, 0);
    }
    if (config->country[0] != '\0') {
        X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC,
                                    (unsigned char *)config->country, -1, -1, 0);
    }
    
    if (X509_set_issuer_name(cert, name) != 1) {
        X509_free(cert);
        apkcheck_log_error("Failed to set issuer name");
        return NULL;
    }
    
    add_ext(cert, NID_basic_constraints, "critical,CA:TRUE");
    add_ext(cert, NID_key_usage, "critical,digitalSignature,keyCertSign");
    add_ext(cert, NID_subject_key_identifier, "hash");
    
    const EVP_MD *md = apkcheck_sig_alg_to_evp(config->sig_alg);
    if (md == NULL) {
        X509_free(cert);
        apkcheck_log_error("Unknown signature algorithm");
        return NULL;
    }
    
    if (X509_sign(cert, key, md) == 0) {
        X509_free(cert);
        apkcheck_log_error("Failed to sign certificate");
        return NULL;
    }
    
    return cert;
}

void apkcheck_cert_free(X509 *cert) {
    if (cert != NULL) {
        X509_free(cert);
    }
}

int apkcheck_cert_get_fingerprint(X509 *cert, apkcheck_digest_alg_t alg,
                                   uint8_t *fingerprint, size_t *fp_len) {
    if (cert == NULL || fingerprint == NULL || fp_len == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    const EVP_MD *md = apkcheck_digest_alg_to_evp(alg);
    if (md == NULL) {
        apkcheck_log_error("Unknown digest algorithm");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    unsigned char cert_der[8192];
    unsigned char *p = cert_der;
    int der_len = i2d_X509(cert, &p);
    
    if (der_len < 0) {
        apkcheck_log_error("Failed to encode certificate to DER");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    unsigned int md_len;
    if (EVP_Digest(cert_der, (size_t)der_len, fingerprint, &md_len, md, NULL) != 1) {
        apkcheck_log_error("Failed to compute fingerprint");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    *fp_len = (size_t)md_len;
    return APKCHECK_SUCCESS;
}

int apkcheck_cert_get_info(X509 *cert, apkcheck_cert_info_t *info) {
    if (cert == NULL || info == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    memset(info, 0, sizeof(apkcheck_cert_info_t));
    
    BIO *bio = BIO_new(BIO_s_mem());
    if (bio == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    X509_NAME_oneline(X509_get_issuer_name(cert), info->issuer, sizeof(info->issuer) - 1);
    X509_NAME_oneline(X509_get_subject_name(cert), info->subject, sizeof(info->subject) - 1);
    
    ASN1_INTEGER *serial = X509_get_serialNumber(cert);
    BIGNUM *bn = ASN1_INTEGER_to_BN(serial, NULL);
    if (bn != NULL) {
        char *serial_hex = BN_bn2hex(bn);
        if (serial_hex != NULL) {
            apkcheck_safe_strcpy(info->serial_number, sizeof(info->serial_number), serial_hex);
            OPENSSL_free(serial_hex);
        }
        BN_free(bn);
    }
    
    ASN1_TIME *not_before = X509_getm_notBefore(cert);
    ASN1_TIME *not_after = X509_getm_notAfter(cert);
    
    BIO_printf(bio, "");
    ASN1_TIME_print(bio, not_before);
    (void)BIO_gets(bio, info->valid_from, (int)sizeof(info->valid_from) - 1);
    
    BIO_reset(bio);
    ASN1_TIME_print(bio, not_after);
    (void)BIO_gets(bio, info->valid_to, (int)sizeof(info->valid_to) - 1);
    
    int sig_nid = X509_get_signature_nid(cert);
    const char *sig_name = OBJ_nid2ln(sig_nid);
    if (sig_name != NULL) {
        apkcheck_safe_strcpy(info->signature_algorithm, sizeof(info->signature_algorithm), sig_name);
    }
    
    size_t fp_len;
    apkcheck_cert_get_fingerprint(cert, APKCHECK_DIGEST_SHA1, info->fingerprint_sha1, &fp_len);
    apkcheck_cert_get_fingerprint(cert, APKCHECK_DIGEST_SHA256, info->fingerprint_sha256, &fp_len);
    
    info->is_valid = 1;
    info->sig_alg = APKCHECK_SIG_ALG_SHA256withRSA;
    
    BIO_free(bio);
    return APKCHECK_SUCCESS;
}

int apkcheck_pkcs8_encrypt(EVP_PKEY *key, const char *password,
                            uint8_t **output, size_t *output_len) {
    if (key == NULL || password == NULL || output == NULL || output_len == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    BIO *bio = BIO_new(BIO_s_mem());
    if (bio == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    const EVP_CIPHER *cipher = EVP_aes_256_cbc();
    
    if (PEM_write_bio_PKCS8PrivateKey(bio, key, cipher,
                                        (char *)password, (int)strlen(password),
                                        NULL, NULL) != 1) {
        BIO_free(bio);
        apkcheck_log_error("Failed to encrypt private key");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    BUF_MEM *bptr;
    BIO_get_mem_ptr(bio, &bptr);
    
    *output = (uint8_t *)malloc(bptr->length);
    if (*output == NULL) {
        BIO_free(bio);
        return APKCHECK_ERROR_MEMORY;
    }
    
    memcpy(*output, bptr->data, bptr->length);
    *output_len = bptr->length;
    
    BIO_free(bio);
    return APKCHECK_SUCCESS;
}

EVP_PKEY *apkcheck_pkcs8_decrypt(const uint8_t *input, size_t input_len,
                                   const char *password) {
    if (input == NULL || password == NULL) {
        return NULL;
    }
    
    BIO *bio = BIO_new_mem_buf(input, (int)input_len);
    if (bio == NULL) {
        return NULL;
    }
    
    EVP_PKEY *key = PEM_read_bio_PrivateKey(bio, NULL, NULL, (void *)password);
    
    BIO_free(bio);
    
    if (key == NULL) {
        apkcheck_log_debug("Failed to decrypt private key (wrong password?)");
    }
    
    return key;
}

int apkcheck_pkcs12_create(EVP_PKEY *key, X509 *cert, const char *password,
                            const char *friendly_name, uint8_t **output, size_t *output_len) {
    if (key == NULL || cert == NULL || password == NULL || output == NULL || output_len == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    PKCS12 *p12 = PKCS12_create((char *)password,
                                  (char *)(friendly_name ? friendly_name : "apksigner"),
                                  key, cert, NULL, 0, 0, 0, 0, 0);
    
    if (p12 == NULL) {
        apkcheck_log_error("Failed to create PKCS12");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    BIO *bio = BIO_new(BIO_s_mem());
    if (bio == NULL) {
        PKCS12_free(p12);
        return APKCHECK_ERROR_MEMORY;
    }
    
    if (i2d_PKCS12_bio(bio, p12) != 1) {
        BIO_free(bio);
        PKCS12_free(p12);
        apkcheck_log_error("Failed to encode PKCS12");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    BUF_MEM *bptr;
    BIO_get_mem_ptr(bio, &bptr);
    
    *output = (uint8_t *)malloc(bptr->length);
    if (*output == NULL) {
        BIO_free(bio);
        PKCS12_free(p12);
        return APKCHECK_ERROR_MEMORY;
    }
    
    memcpy(*output, bptr->data, bptr->length);
    *output_len = bptr->length;
    
    BIO_free(bio);
    PKCS12_free(p12);
    
    return APKCHECK_SUCCESS;
}

int apkcheck_pkcs12_parse(const uint8_t *input, size_t input_len, const char *password,
                           EVP_PKEY **key, X509 **cert) {
    if (input == NULL || password == NULL || key == NULL || cert == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    BIO *bio = BIO_new_mem_buf(input, (int)input_len);
    if (bio == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    PKCS12 *p12 = d2i_PKCS12_bio(bio, NULL);
    BIO_free(bio);
    
    if (p12 == NULL) {
        apkcheck_log_error("Failed to parse PKCS12");
        return APKCHECK_ERROR_INVALID_FORMAT;
    }
    
    STACK_OF(X509) *ca = NULL;
    int result = APKCHECK_SUCCESS;
    
    if (PKCS12_parse(p12, password, key, cert, &ca) != 1) {
        apkcheck_log_error("Failed to decrypt PKCS12 (wrong password?)");
        result = APKCHECK_ERROR_CRYPTO;
    }
    
    sk_X509_pop_free(ca, X509_free);
    PKCS12_free(p12);
    
    return result;
}

const EVP_MD *apkcheck_digest_alg_to_evp(apkcheck_digest_alg_t alg) {
    switch (alg) {
        case APKCHECK_DIGEST_MD5:
            return EVP_md5();
        case APKCHECK_DIGEST_SHA1:
            return EVP_sha1();
        case APKCHECK_DIGEST_SHA256:
            return EVP_sha256();
        case APKCHECK_DIGEST_SHA512:
            return EVP_sha512();
        default:
            return NULL;
    }
}

const EVP_MD *apkcheck_sig_alg_to_evp(apkcheck_sig_alg_t alg) {
    switch (alg) {
        case APKCHECK_SIG_ALG_SHA1withRSA:
            return EVP_sha1();
        case APKCHECK_SIG_ALG_SHA256withRSA:
            return EVP_sha256();
        case APKCHECK_SIG_ALG_SHA512withRSA:
            return EVP_sha512();
        default:
            return NULL;
    }
}
