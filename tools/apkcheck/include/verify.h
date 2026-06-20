#ifndef APKCHECK_VERIFY_H
#define APKCHECK_VERIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apkcheck.h"
#include "crypto.h"
#include <zip.h>

typedef enum {
    APKCHECK_VERIFY_V1 = 1,
    APKCHECK_VERIFY_V2 = 2,
    APKCHECK_VERIFY_V3 = 3
} apkcheck_verify_version_t;

typedef struct {
    bool has_v1;
    bool has_v2;
    bool has_v3;
    bool v1_valid;
    bool v2_valid;
    bool v3_valid;
    apkcheck_cert_info_t cert_info;
} apkcheck_apk_signature_info_t;

int apkcheck_verify_apk(const char *apk_path, apkcheck_verify_result_t *result);

int apkcheck_verify_v1_signature(zip_t *zip, apkcheck_cert_info_t *cert_info, bool *valid);

int apkcheck_detect_signature_versions(zip_t *zip, bool *has_v1, bool *has_v2, bool *has_v3);

int apkcheck_extract_certificates(zip_t *zip, X509 ***certs, int *count);
void apkcheck_free_certificates(X509 **certs, int count);

int apkcheck_read_manifest_mf(zip_t *zip, apkcheck_manifest_t **manifest);
int apkcheck_read_cert_sf(zip_t *zip, apkcheck_buffer_t **sf_data);
int apkcheck_read_cert_rsa(zip_t *zip, X509 **cert, EVP_PKEY **pubkey);

int apkcheck_verify_manifest_integrity(zip_t *zip, const apkcheck_manifest_t *manifest);

int apkcheck_verify_signature_block(const uint8_t *sf_data, size_t sf_len,
                                      const uint8_t *signature, size_t sig_len,
                                      X509 *cert, apkcheck_sig_alg_t sig_alg);

int apkcheck_print_signature_info(const apkcheck_verify_result_t *result);

int apkcheck_verify_apk_alignment(const char *apk_path, bool *aligned);

int apkcheck_get_signature_fingerprints(X509 *cert, char *sha1_hex, size_t sha1_len,
                                         char *sha256_hex, size_t sha256_len);

int apkcheck_extract_public_key(X509 *cert, EVP_PKEY **pubkey);

apkcheck_sig_alg_t apkcheck_detect_signature_algorithm(const char *algorithm_name);

#ifdef __cplusplus
}
#endif

#endif
