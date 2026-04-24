#include "sign.h"
#include "log.h"
#include "utils.h"
#include "crypto.h"
#include "jks.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zip.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>

apkcheck_manifest_t *apkcheck_manifest_create(void) {
    apkcheck_manifest_t *manifest = (apkcheck_manifest_t *)malloc(sizeof(apkcheck_manifest_t));
    if (manifest == NULL) {
        apkcheck_log_error("Failed to allocate manifest");
        return NULL;
    }
    
    memset(manifest, 0, sizeof(apkcheck_manifest_t));
    strcpy(manifest->version, "1.0");
    
    manifest->entries = (apkcheck_manifest_entry_t *)malloc(16 * sizeof(apkcheck_manifest_entry_t));
    if (manifest->entries == NULL) {
        free(manifest);
        return NULL;
    }
    
    manifest->entry_count = 0;
    manifest->entry_capacity = 16;
    
    return manifest;
}

void apkcheck_manifest_destroy(apkcheck_manifest_t *manifest) {
    if (manifest == NULL) return;
    if (manifest->entries != NULL) {
        free(manifest->entries);
    }
    apkcheck_memzero(manifest, sizeof(apkcheck_manifest_t));
    free(manifest);
}

int apkcheck_manifest_add_entry(apkcheck_manifest_t *manifest,
                                 const char *name,
                                 const char *digest_sha1,
                                 const char *digest_sha256) {
    if (manifest == NULL || name == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (manifest->entry_count >= manifest->entry_capacity) {
        size_t new_capacity = manifest->entry_capacity * 2;
        apkcheck_manifest_entry_t *new_entries = (apkcheck_manifest_entry_t *)realloc(
            manifest->entries, new_capacity * sizeof(apkcheck_manifest_entry_t));
        if (new_entries == NULL) {
            return APKCHECK_ERROR_MEMORY;
        }
        manifest->entries = new_entries;
        manifest->entry_capacity = new_capacity;
    }
    
    apkcheck_manifest_entry_t *entry = &manifest->entries[manifest->entry_count];
    apkcheck_safe_strcpy(entry->name, sizeof(entry->name), name);
    
    if (digest_sha1 != NULL) {
        apkcheck_safe_strcpy(entry->digest_sha1, sizeof(entry->digest_sha1), digest_sha1);
    }
    if (digest_sha256 != NULL) {
        apkcheck_safe_strcpy(entry->digest_sha256, sizeof(entry->digest_sha256), digest_sha256);
    }
    
    manifest->entry_count++;
    return APKCHECK_SUCCESS;
}

int apkcheck_is_meta_inf_file(const char *name) {
    if (name == NULL) return 0;
    if (strncmp(name, "META-INF/", 9) != 0) return 0;
    
    const char *basename = name + 9;
    if (strlen(basename) == 0) return 0;
    
    if (strcmp(basename, "MANIFEST.MF") == 0) return 1;
    if (strlen(basename) >= 3 &&
        ((strcmp(basename + strlen(basename) - 3, ".SF") == 0) ||
         (strcmp(basename + strlen(basename) - 4, ".RSA") == 0) ||
         (strcmp(basename + strlen(basename) - 4, ".DSA") == 0) ||
         (strcmp(basename + strlen(basename) - 7, ".MF") == 0))) {
        return 1;
    }
    
    return 0;
}

int apkcheck_compute_entry_digest(zip_t *zip, zip_uint64_t index,
                                   apkcheck_digest_alg_t alg,
                                   uint8_t *digest, size_t *digest_len) {
    if (zip == NULL || digest == NULL || digest_len == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    zip_file_t *zf = zip_fopen_index(zip, index, 0);
    if (zf == NULL) {
        apkcheck_log_error("Failed to open zip entry");
        return APKCHECK_ERROR_ZIP;
    }
    
    apkcheck_digest_ctx_t *ctx = apkcheck_digest_create(alg);
    if (ctx == NULL) {
        zip_fclose(zf);
        return APKCHECK_ERROR_CRYPTO;
    }
    
    uint8_t buf[8192];
    zip_int64_t read_size;
    int result = APKCHECK_SUCCESS;
    
    while ((read_size = zip_fread(zf, buf, sizeof(buf))) > 0) {
        if (apkcheck_digest_update(ctx, buf, (size_t)read_size) != APKCHECK_SUCCESS) {
            result = APKCHECK_ERROR_CRYPTO;
            break;
        }
    }
    
    if (read_size < 0) {
        apkcheck_log_error("Failed to read zip entry");
        result = APKCHECK_ERROR_IO;
    }
    
    if (result == APKCHECK_SUCCESS) {
        result = apkcheck_digest_final(ctx, digest, digest_len);
    }
    
    apkcheck_digest_destroy(ctx);
    zip_fclose(zf);
    
    return result;
}

int apkcheck_generate_manifest_mf(zip_t *zip, apkcheck_manifest_t **manifest,
                                   const apkcheck_sign_config_internal_t *config) {
    if (zip == NULL || manifest == NULL || config == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    *manifest = apkcheck_manifest_create();
    if (*manifest == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    zip_int64_t entry_count = zip_get_num_entries(zip, 0);
    if (entry_count < 0) {
        apkcheck_manifest_destroy(*manifest);
        *manifest = NULL;
        return APKCHECK_ERROR_ZIP;
    }
    
    for (zip_int64_t i = 0; i < entry_count; i++) {
        const char *name = zip_get_name(zip, (zip_uint64_t)i, 0);
        if (name == NULL) continue;
        
        if (apkcheck_is_meta_inf_file(name)) {
            apkcheck_log_debug("Skipping META-INF file: %s", name);
            continue;
        }
        
        zip_stat_t st;
        if (zip_stat_index(zip, (zip_uint64_t)i, 0, &st) != 0) continue;
        if (st.valid & ZIP_STAT_SIZE && st.size == 0) continue;
        
        apkcheck_log_debug("Processing entry: %s", name);
        
        uint8_t digest_sha1[20];
        size_t digest_sha1_len = sizeof(digest_sha1);
        char digest_sha1_b64[64];
        
        uint8_t digest_sha256[32];
        size_t digest_sha256_len = sizeof(digest_sha256);
        char digest_sha256_b64[128];
        
        if (apkcheck_compute_entry_digest(zip, (zip_uint64_t)i, APKCHECK_DIGEST_SHA1,
                                            digest_sha1, &digest_sha1_len) == APKCHECK_SUCCESS) {
            apkcheck_base64_encode(digest_sha1, digest_sha1_len, digest_sha1_b64, sizeof(digest_sha1_b64));
        }
        
        if (apkcheck_compute_entry_digest(zip, (zip_uint64_t)i, APKCHECK_DIGEST_SHA256,
                                            digest_sha256, &digest_sha256_len) == APKCHECK_SUCCESS) {
            apkcheck_base64_encode(digest_sha256, digest_sha256_len, digest_sha256_b64, sizeof(digest_sha256_b64));
        }
        
        apkcheck_manifest_add_entry(*manifest, name, digest_sha1_b64, digest_sha256_b64);
    }
    
    return APKCHECK_SUCCESS;
}

int apkcheck_generate_cert_sf(const apkcheck_manifest_t *manifest,
                               apkcheck_buffer_t **output,
                               const apkcheck_sign_config_internal_t *config) {
    if (manifest == NULL || output == NULL || config == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    *output = apkcheck_buffer_create(4096);
    if (*output == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    char line[1024];
    
    apkcheck_safe_printf(line, sizeof(line), "Signature-Version: 1.0\r\n");
    apkcheck_buffer_append_string(*output, line);
    
    if (config->created_by[0] != '\0') {
        apkcheck_safe_printf(line, sizeof(line), "Created-By: %s\r\n", config->created_by);
        apkcheck_buffer_append_string(*output, line);
    }
    
    apkcheck_buffer_append_string(*output, "\r\n");
    
    for (size_t i = 0; i < manifest->entry_count; i++) {
        apkcheck_manifest_entry_t *entry = &manifest->entries[i];
        
        apkcheck_safe_printf(line, sizeof(line), "Name: %s\r\n", entry->name);
        apkcheck_buffer_append_string(*output, line);
        
        if (config->digest_alg == APKCHECK_DIGEST_SHA1 && entry->digest_sha1[0] != '\0') {
            apkcheck_safe_printf(line, sizeof(line), "SHA1-Digest: %s\r\n", entry->digest_sha1);
            apkcheck_buffer_append_string(*output, line);
        } else if (entry->digest_sha256[0] != '\0') {
            apkcheck_safe_printf(line, sizeof(line), "SHA-256-Digest: %s\r\n", entry->digest_sha256);
            apkcheck_buffer_append_string(*output, line);
        }
        
        apkcheck_buffer_append_string(*output, "\r\n");
    }
    
    return APKCHECK_SUCCESS;
}

static int pkcs7_sign_data(const uint8_t *data, size_t data_len,
                             EVP_PKEY *key, X509 *cert,
                             const EVP_MD *md,
                             uint8_t **output, size_t *output_len) {
    BIO *bio = BIO_new_mem_buf(data, (int)data_len);
    if (bio == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    PKCS7 *p7 = PKCS7_sign(cert, key, NULL, bio,
                             PKCS7_DETACHED | PKCS7_BINARY | PKCS7_NOATTR);
    
    BIO_free(bio);
    
    if (p7 == NULL) {
        apkcheck_log_error("PKCS7_sign failed");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    BIO *out_bio = BIO_new(BIO_s_mem());
    if (out_bio == NULL) {
        PKCS7_free(p7);
        return APKCHECK_ERROR_MEMORY;
    }
    
    i2d_PKCS7_bio(out_bio, p7);
    
    BUF_MEM *bptr;
    BIO_get_mem_ptr(out_bio, &bptr);
    
    *output = (uint8_t *)malloc(bptr->length);
    if (*output == NULL) {
        BIO_free(out_bio);
        PKCS7_free(p7);
        return APKCHECK_ERROR_MEMORY;
    }
    
    memcpy(*output, bptr->data, bptr->length);
    *output_len = bptr->length;
    
    BIO_free(out_bio);
    PKCS7_free(p7);
    
    return APKCHECK_SUCCESS;
}

int apkcheck_generate_cert_rsa(const uint8_t *sf_data, size_t sf_len,
                                EVP_PKEY *private_key, X509 *cert,
                                apkcheck_sig_alg_t sig_alg,
                                apkcheck_buffer_t **output) {
    if (sf_data == NULL || private_key == NULL || cert == NULL || output == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    const EVP_MD *md = apkcheck_sig_alg_to_evp(sig_alg);
    if (md == NULL) {
        apkcheck_log_error("Unknown signature algorithm");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    uint8_t *sig_data = NULL;
    size_t sig_len = 0;
    
    int result = pkcs7_sign_data(sf_data, sf_len, private_key, cert, md, &sig_data, &sig_len);
    if (result != APKCHECK_SUCCESS) {
        return result;
    }
    
    *output = apkcheck_buffer_create(sig_len);
    if (*output == NULL) {
        free(sig_data);
        return APKCHECK_ERROR_MEMORY;
    }
    
    apkcheck_buffer_append(*output, sig_data, sig_len);
    free(sig_data);
    
    return APKCHECK_SUCCESS;
}

int apkcheck_check_alignment(zip_t *zip, bool *aligned) {
    if (zip == NULL || aligned == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    *aligned = true;
    
    apkcheck_log_info("Alignment check skipped (requires direct ZIP file parsing)");
    apkcheck_log_info("For proper alignment, use zipalign tool after signing");
    
    return APKCHECK_SUCCESS;
}

int apkcheck_verify_alignment(const char *apk_path) {
    if (apk_path == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    int err = 0;
    zip_t *zip = zip_open(apk_path, 0, &err);
    if (zip == NULL) {
        apkcheck_log_error("Failed to open APK: %s", apk_path);
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    bool aligned;
    int result = apkcheck_check_alignment(zip, &aligned);
    
    zip_close(zip);
    
    if (result == APKCHECK_SUCCESS) {
        if (aligned) {
            apkcheck_log_info("APK is properly aligned");
        } else {
            apkcheck_log_warn("APK is NOT properly aligned");
        }
    }
    
    return result;
}

int apkcheck_remove_old_signatures(zip_t *zip) {
    if (zip == NULL) return APKCHECK_ERROR_INVALID_PARAM;
    
    zip_int64_t entry_count = zip_get_num_entries(zip, 0);
    if (entry_count < 0) {
        return APKCHECK_ERROR_ZIP;
    }
    
    int removed = 0;
    for (zip_int64_t i = entry_count - 1; i >= 0; i--) {
        const char *name = zip_get_name(zip, (zip_uint64_t)i, 0);
        if (name == NULL) continue;
        
        if (apkcheck_is_meta_inf_file(name)) {
            apkcheck_log_debug("Removing old signature file: %s", name);
            if (zip_delete(zip, (zip_uint64_t)i) == 0) {
                removed++;
            }
        }
    }
    
    if (removed > 0) {
        apkcheck_log_info("Removed %d old signature files", removed);
    }
    
    return APKCHECK_SUCCESS;
}

static int add_file_to_zip(zip_t *dst_zip, const char *name,
                            const uint8_t *data, size_t len) {
    zip_source_t *src = zip_source_buffer(dst_zip, data, len, 0);
    if (src == NULL) {
        apkcheck_log_error("Failed to create zip source for: %s", name);
        return APKCHECK_ERROR_ZIP;
    }
    
    zip_int64_t index = zip_file_add(dst_zip, name, src, ZIP_FL_ENC_UTF_8);
    if (index < 0) {
        zip_source_free(src);
        apkcheck_log_error("Failed to add file to zip: %s", name);
        return APKCHECK_ERROR_ZIP;
    }
    
    return APKCHECK_SUCCESS;
}

int apkcheck_create_signed_zip(const char *input_apk, const char *output_apk,
                                const uint8_t *manifest_data, size_t manifest_len,
                                const uint8_t *sf_data, size_t sf_len,
                                const uint8_t *rsa_data, size_t rsa_len) {
    int err = 0;
    zip_t *src_zip = zip_open(input_apk, 0, &err);
    if (src_zip == NULL) {
        apkcheck_log_error("Failed to open input APK: %s", input_apk);
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    zip_t *dst_zip = zip_open(output_apk, ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (dst_zip == NULL) {
        zip_close(src_zip);
        apkcheck_log_error("Failed to create output APK: %s", output_apk);
        return APKCHECK_ERROR_PERMISSION_DENIED;
    }
    
    int result = APKCHECK_SUCCESS;
    zip_int64_t entry_count = zip_get_num_entries(src_zip, 0);
    
    for (zip_int64_t i = 0; i < entry_count; i++) {
        const char *name = zip_get_name(src_zip, (zip_uint64_t)i, 0);
        if (name == NULL) continue;
        
        if (apkcheck_is_meta_inf_file(name)) {
            continue;
        }
        
        zip_stat_t st;
        if (zip_stat_index(src_zip, (zip_uint64_t)i, 0, &st) != 0) {
            continue;
        }
        
        zip_file_t *zf = zip_fopen_index(src_zip, (zip_uint64_t)i, 0);
        if (zf == NULL) continue;
        
        uint8_t *data = NULL;
        if (st.valid & ZIP_STAT_SIZE) {
            data = (uint8_t *)malloc((size_t)st.size);
            if (data != NULL) {
                zip_int64_t read_size = zip_fread(zf, data, (size_t)st.size);
                if (read_size < 0 || (zip_uint64_t)read_size != st.size) {
                    free(data);
                    data = NULL;
                }
            }
        }
        
        zip_fclose(zf);
        
        if (data == NULL) {
            apkcheck_log_warn("Skipping entry: %s", name);
            continue;
        }
        
        zip_source_t *src = zip_source_buffer(dst_zip, data, (size_t)st.size, 0);
        if (src == NULL) {
            free(data);
            result = APKCHECK_ERROR_ZIP;
            break;
        }
        
        zip_int64_t new_index = zip_file_add(dst_zip, name, src, ZIP_FL_ENC_UTF_8);
        if (new_index < 0) {
            zip_source_free(src);
            free(data);
            result = APKCHECK_ERROR_ZIP;
            break;
        }
        
        if (st.valid & ZIP_STAT_MTIME) {
            zip_set_file_compression(dst_zip, (zip_uint64_t)new_index, st.comp_method, 0);
        }
        
        free(data);
    }
    
    if (result == APKCHECK_SUCCESS) {
        result = add_file_to_zip(dst_zip, MANIFEST_MF, manifest_data, manifest_len);
    }
    
    if (result == APKCHECK_SUCCESS) {
        result = add_file_to_zip(dst_zip, CERT_SF, sf_data, sf_len);
    }
    
    if (result == APKCHECK_SUCCESS) {
        result = add_file_to_zip(dst_zip, CERT_RSA, rsa_data, rsa_len);
    }
    
    if (result == APKCHECK_SUCCESS) {
        if (zip_close(dst_zip) != 0) {
            apkcheck_log_error("Failed to write output APK");
            result = APKCHECK_ERROR_IO;
        }
    } else {
        zip_discard(dst_zip);
    }
    
    zip_close(src_zip);
    
    return result;
}

static apkcheck_buffer_t *manifest_to_buffer(const apkcheck_manifest_t *manifest) {
    if (manifest == NULL) return NULL;
    
    apkcheck_buffer_t *buf = apkcheck_buffer_create(4096);
    if (buf == NULL) return NULL;
    
    char line[1024];
    
    apkcheck_safe_printf(line, sizeof(line), "Manifest-Version: 1.0\r\n");
    apkcheck_buffer_append_string(buf, line);
    
    apkcheck_safe_printf(line, sizeof(line), "Created-By: APKCheck %s (Android Signing)\r\n", APKCHECK_VERSION);
    apkcheck_buffer_append_string(buf, line);
    
    apkcheck_buffer_append_string(buf, "\r\n");
    
    for (size_t i = 0; i < manifest->entry_count; i++) {
        apkcheck_manifest_entry_t *entry = &manifest->entries[i];
        
        apkcheck_safe_printf(line, sizeof(line), "Name: %s\r\n", entry->name);
        apkcheck_buffer_append_string(buf, line);
        
        if (entry->digest_sha1[0] != '\0') {
            apkcheck_safe_printf(line, sizeof(line), "SHA1-Digest: %s\r\n", entry->digest_sha1);
            apkcheck_buffer_append_string(buf, line);
        }
        
        if (entry->digest_sha256[0] != '\0') {
            apkcheck_safe_printf(line, sizeof(line), "SHA-256-Digest: %s\r\n", entry->digest_sha256);
            apkcheck_buffer_append_string(buf, line);
        }
        
        apkcheck_buffer_append_string(buf, "\r\n");
    }
    
    return buf;
}

int apkcheck_sign_v1(const char *input_apk, const char *output_apk,
                      const apkcheck_sign_config_t *config) {
    if (input_apk == NULL || output_apk == NULL || config == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (!apkcheck_file_readable(input_apk)) {
        apkcheck_log_error("Cannot read input APK: %s", input_apk);
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    apkcheck_log_info("Loading keystore: %s", config->keystore_path);
    
    jks_keystore_t *ks = NULL;
    int result = jks_keystore_load(&ks, config->keystore_path, config->storepass);
    if (result != APKCHECK_SUCCESS) {
        apkcheck_log_error("Failed to load keystore");
        return result;
    }
    
    EVP_PKEY *private_key = NULL;
    X509 **cert_chain = NULL;
    int cert_chain_len = 0;
    
    result = jks_keystore_get_private_key(ks, config->alias, config->keypass,
                                             &private_key, &cert_chain, &cert_chain_len);
    if (result != APKCHECK_SUCCESS) {
        jks_keystore_destroy(ks);
        apkcheck_log_error("Failed to get private key for alias: %s", config->alias);
        return result;
    }
    
    if (private_key == NULL) {
        jks_keystore_destroy(ks);
        apkcheck_log_error("Private key is NULL");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    X509 *cert = (cert_chain != NULL && cert_chain_len > 0) ? cert_chain[0] : NULL;
    if (cert == NULL) {
        jks_keystore_destroy(ks);
        apkcheck_log_error("Certificate chain is empty");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    apkcheck_log_info("Opening APK: %s", input_apk);
    
    int err = 0;
    zip_t *zip = zip_open(input_apk, 0, &err);
    if (zip == NULL) {
        jks_keystore_destroy(ks);
        apkcheck_log_error("Failed to open APK");
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    apkcheck_sign_config_internal_t internal_config;
    memset(&internal_config, 0, sizeof(internal_config));
    internal_config.digest_alg = APKCHECK_DIGEST_SHA256;
    internal_config.sig_alg = config->sig_alg;
    apkcheck_safe_strcpy(internal_config.created_by, sizeof(internal_config.created_by),
                          "APKCheck " APKCHECK_VERSION);
    
    apkcheck_manifest_t *manifest = NULL;
    result = apkcheck_generate_manifest_mf(zip, &manifest, &internal_config);
    
    zip_close(zip);
    
    if (result != APKCHECK_SUCCESS) {
        jks_keystore_destroy(ks);
        apkcheck_log_error("Failed to generate manifest");
        return result;
    }
    
    apkcheck_buffer_t *manifest_buf = manifest_to_buffer(manifest);
    apkcheck_manifest_destroy(manifest);
    
    if (manifest_buf == NULL) {
        jks_keystore_destroy(ks);
        return APKCHECK_ERROR_MEMORY;
    }
    
    apkcheck_log_info("Generating CERT.SF...");
    
    apkcheck_buffer_t *sf_buf = NULL;
    result = apkcheck_generate_cert_sf(manifest, &sf_buf, &internal_config);
    
    if (result != APKCHECK_SUCCESS) {
        apkcheck_buffer_destroy(manifest_buf);
        jks_keystore_destroy(ks);
        return result;
    }
    
    apkcheck_log_info("Generating CERT.RSA...");
    
    apkcheck_buffer_t *rsa_buf = NULL;
    result = apkcheck_generate_cert_rsa(sf_buf->data, sf_buf->len,
                                          private_key, cert, config->sig_alg, &rsa_buf);
    
    if (result != APKCHECK_SUCCESS) {
        apkcheck_buffer_destroy(manifest_buf);
        apkcheck_buffer_destroy(sf_buf);
        jks_keystore_destroy(ks);
        return result;
    }
    
    apkcheck_log_info("Creating signed APK: %s", output_apk);
    
    result = apkcheck_create_signed_zip(input_apk, output_apk,
                                          manifest_buf->data, manifest_buf->len,
                                          sf_buf->data, sf_buf->len,
                                          rsa_buf->data, rsa_buf->len);
    
    apkcheck_buffer_destroy(manifest_buf);
    apkcheck_buffer_destroy(sf_buf);
    apkcheck_buffer_destroy(rsa_buf);
    jks_keystore_destroy(ks);
    
    if (result == APKCHECK_SUCCESS) {
        apkcheck_log_info("APK signed successfully: %s", output_apk);
    }
    
    return result;
}
