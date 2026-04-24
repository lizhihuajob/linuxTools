#ifndef APKCHECK_SIGN_H
#define APKCHECK_SIGN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apkcheck.h"
#include "crypto.h"
#include <zip.h>

#define MANIFEST_MF "META-INF/MANIFEST.MF"
#define CERT_SF "META-INF/CERT.SF"
#define CERT_RSA "META-INF/CERT.RSA"

#define APK_ALIGNMENT 4

typedef struct {
    apkcheck_digest_alg_t digest_alg;
    apkcheck_sig_alg_t sig_alg;
    char created_by[128];
} apkcheck_sign_config_internal_t;

apkcheck_manifest_t *apkcheck_manifest_create(void);
void apkcheck_manifest_destroy(apkcheck_manifest_t *manifest);
int apkcheck_manifest_add_entry(apkcheck_manifest_t *manifest,
                                 const char *name,
                                 const char *digest_sha1,
                                 const char *digest_sha256);

int apkcheck_generate_manifest_mf(zip_t *zip, apkcheck_manifest_t **manifest,
                                   const apkcheck_sign_config_internal_t *config);
int apkcheck_generate_cert_sf(const apkcheck_manifest_t *manifest,
                               apkcheck_buffer_t **output,
                               const apkcheck_sign_config_internal_t *config);
int apkcheck_generate_cert_rsa(const uint8_t *sf_data, size_t sf_len,
                                EVP_PKEY *private_key, X509 *cert,
                                apkcheck_sig_alg_t sig_alg,
                                apkcheck_buffer_t **output);

int apkcheck_compute_entry_digest(zip_t *zip, zip_uint64_t index,
                                   apkcheck_digest_alg_t alg,
                                   uint8_t *digest, size_t *digest_len);

int apkcheck_check_alignment(zip_t *zip, bool *aligned);
int apkcheck_verify_alignment(const char *apk_path);

int apkcheck_sign_v1(const char *input_apk, const char *output_apk,
                      const apkcheck_sign_config_t *config);

int apkcheck_get_zip_entry_name(zip_t *zip, zip_uint64_t index, char *name, size_t max_len);

int apkcheck_is_meta_inf_file(const char *name);

int apkcheck_remove_old_signatures(zip_t *zip);

int apkcheck_create_signed_zip(const char *input_apk, const char *output_apk,
                                const uint8_t *manifest_data, size_t manifest_len,
                                const uint8_t *sf_data, size_t sf_len,
                                const uint8_t *rsa_data, size_t rsa_len);

#ifdef __cplusplus
}
#endif

#endif
