#include "verify.h"
#include "log.h"
#include "utils.h"
#include "crypto.h"
#include "sign.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zip.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>

static int parse_manifest_line(const char *line, char *name, size_t name_len,
                                 char *sha1_digest, size_t sha1_len,
                                 char *sha256_digest, size_t sha256_len) {
    if (line == NULL) return 0;
    
    if (strncmp(line, "Name: ", 6) == 0) {
        if (name != NULL && name_len > 0) {
            const char *value = line + 6;
            while (*value == ' ') value++;
            apkcheck_safe_strcpy(name, name_len, value);
            return 1;
        }
    } else if (strncmp(line, "SHA1-Digest: ", 13) == 0) {
        if (sha1_digest != NULL && sha1_len > 0) {
            const char *value = line + 13;
            while (*value == ' ') value++;
            apkcheck_safe_strcpy(sha1_digest, sha1_len, value);
            return 1;
        }
    } else if (strncmp(line, "SHA-256-Digest: ", 17) == 0) {
        if (sha256_digest != NULL && sha256_len > 0) {
            const char *value = line + 17;
            while (*value == ' ') value++;
            apkcheck_safe_strcpy(sha256_digest, sha256_len, value);
            return 1;
        }
    }
    
    return 0;
}

int apkcheck_read_manifest_mf(zip_t *zip, apkcheck_manifest_t **manifest) {
    if (zip == NULL || manifest == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    zip_int64_t index = zip_name_locate(zip, MANIFEST_MF, 0);
    if (index < 0) {
        apkcheck_log_debug("MANIFEST.MF not found");
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    zip_file_t *zf = zip_fopen_index(zip, (zip_uint64_t)index, 0);
    if (zf == NULL) {
        apkcheck_log_error("Failed to open MANIFEST.MF");
        return APKCHECK_ERROR_IO;
    }
    
    apkcheck_buffer_t *buf = apkcheck_buffer_create(0);
    if (buf == NULL) {
        zip_fclose(zf);
        return APKCHECK_ERROR_MEMORY;
    }
    
    uint8_t tmp_buf[8192];
    zip_int64_t read_size;
    while ((read_size = zip_fread(zf, tmp_buf, sizeof(tmp_buf))) > 0) {
        apkcheck_buffer_append(buf, tmp_buf, (size_t)read_size);
    }
    
    zip_fclose(zf);
    
    if (read_size < 0) {
        apkcheck_buffer_destroy(buf);
        return APKCHECK_ERROR_IO;
    }
    
    *manifest = apkcheck_manifest_create();
    if (*manifest == NULL) {
        apkcheck_buffer_destroy(buf);
        return APKCHECK_ERROR_MEMORY;
    }
    
    apkcheck_buffer_append(buf, (const uint8_t *)"\0", 1);
    
    char current_name[256] = "";
    char current_sha1[128] = "";
    char current_sha256[256] = "";
    
    char *line = strtok((char *)buf->data, "\r\n");
    while (line != NULL) {
        if (line[0] == ' ' || line[0] == '\t') {
            line = strtok(NULL, "\r\n");
            continue;
        }
        
        if (strncmp(line, "Name: ", 6) == 0) {
            if (current_name[0] != '\0') {
                apkcheck_manifest_add_entry(*manifest, current_name,
                                             current_sha1[0] ? current_sha1 : NULL,
                                             current_sha256[0] ? current_sha256 : NULL);
            }
            
            current_name[0] = '\0';
            current_sha1[0] = '\0';
            current_sha256[0] = '\0';
        }
        
        parse_manifest_line(line, current_name, sizeof(current_name),
                            current_sha1, sizeof(current_sha1),
                            current_sha256, sizeof(current_sha256));
        
        line = strtok(NULL, "\r\n");
    }
    
    if (current_name[0] != '\0') {
        apkcheck_manifest_add_entry(*manifest, current_name,
                                     current_sha1[0] ? current_sha1 : NULL,
                                     current_sha256[0] ? current_sha256 : NULL);
    }
    
    apkcheck_buffer_destroy(buf);
    
    apkcheck_log_info("Parsed MANIFEST.MF with %zu entries", (*manifest)->entry_count);
    return APKCHECK_SUCCESS;
}

int apkcheck_read_cert_sf(zip_t *zip, apkcheck_buffer_t **sf_data) {
    if (zip == NULL || sf_data == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    zip_int64_t index = zip_name_locate(zip, CERT_SF, 0);
    if (index < 0) {
        apkcheck_log_debug("CERT.SF not found");
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    zip_file_t *zf = zip_fopen_index(zip, (zip_uint64_t)index, 0);
    if (zf == NULL) {
        apkcheck_log_error("Failed to open CERT.SF");
        return APKCHECK_ERROR_IO;
    }
    
    *sf_data = apkcheck_buffer_create(0);
    if (*sf_data == NULL) {
        zip_fclose(zf);
        return APKCHECK_ERROR_MEMORY;
    }
    
    uint8_t tmp_buf[8192];
    zip_int64_t read_size;
    while ((read_size = zip_fread(zf, tmp_buf, sizeof(tmp_buf))) > 0) {
        apkcheck_buffer_append(*sf_data, tmp_buf, (size_t)read_size);
    }
    
    zip_fclose(zf);
    
    if (read_size < 0) {
        apkcheck_buffer_destroy(*sf_data);
        *sf_data = NULL;
        return APKCHECK_ERROR_IO;
    }
    
    apkcheck_log_info("Read CERT.SF (%zu bytes)", (*sf_data)->len);
    return APKCHECK_SUCCESS;
}

int apkcheck_read_cert_rsa(zip_t *zip, X509 **cert, EVP_PKEY **pubkey) {
    if (zip == NULL || cert == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    zip_int64_t index = zip_name_locate(zip, CERT_RSA, 0);
    if (index < 0) {
        apkcheck_log_debug("CERT.RSA not found");
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    zip_file_t *zf = zip_fopen_index(zip, (zip_uint64_t)index, 0);
    if (zf == NULL) {
        apkcheck_log_error("Failed to open CERT.RSA");
        return APKCHECK_ERROR_IO;
    }
    
    apkcheck_buffer_t *buf = apkcheck_buffer_create(0);
    if (buf == NULL) {
        zip_fclose(zf);
        return APKCHECK_ERROR_MEMORY;
    }
    
    uint8_t tmp_buf[8192];
    zip_int64_t read_size;
    while ((read_size = zip_fread(zf, tmp_buf, sizeof(tmp_buf))) > 0) {
        apkcheck_buffer_append(buf, tmp_buf, (size_t)read_size);
    }
    
    zip_fclose(zf);
    
    if (read_size < 0) {
        apkcheck_buffer_destroy(buf);
        return APKCHECK_ERROR_IO;
    }
    
    apkcheck_log_info("Read CERT.RSA (%zu bytes)", buf->len);
    
    BIO *bio = BIO_new_mem_buf(buf->data, (int)buf->len);
    if (bio == NULL) {
        apkcheck_buffer_destroy(buf);
        return APKCHECK_ERROR_MEMORY;
    }
    
    PKCS7 *p7 = d2i_PKCS7_bio(bio, NULL);
    BIO_free(bio);
    
    if (p7 == NULL) {
        apkcheck_log_error("Failed to parse PKCS7 from CERT.RSA");
        apkcheck_buffer_destroy(buf);
        return APKCHECK_ERROR_INVALID_FORMAT;
    }
    
    STACK_OF(X509) *certs = NULL;
    
    if (PKCS7_type_is_signed(p7)) {
        certs = p7->d.sign->cert;
    } else if (PKCS7_type_is_signedAndEnveloped(p7)) {
        certs = p7->d.signed_and_enveloped->cert;
    }
    
    if (certs == NULL || sk_X509_num(certs) == 0) {
        apkcheck_log_error("No certificates found in PKCS7");
        PKCS7_free(p7);
        apkcheck_buffer_destroy(buf);
        return APKCHECK_ERROR_INVALID_FORMAT;
    }
    
    *cert = X509_dup(sk_X509_value(certs, 0));
    
    if (pubkey != NULL) {
        *pubkey = X509_get_pubkey(*cert);
    }
    
    PKCS7_free(p7);
    apkcheck_buffer_destroy(buf);
    
    if (*cert == NULL) {
        apkcheck_log_error("Failed to extract certificate");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    apkcheck_log_info("Successfully extracted certificate from CERT.RSA");
    return APKCHECK_SUCCESS;
}

int apkcheck_extract_certificates(zip_t *zip, X509 ***certs, int *count) {
    if (zip == NULL || certs == NULL || count == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    *certs = NULL;
    *count = 0;
    
    X509 *cert = NULL;
    int result = apkcheck_read_cert_rsa(zip, &cert, NULL);
    
    if (result == APKCHECK_SUCCESS && cert != NULL) {
        *certs = (X509 **)malloc(sizeof(X509 *));
        if (*certs == NULL) {
            X509_free(cert);
            return APKCHECK_ERROR_MEMORY;
        }
        
        (*certs)[0] = cert;
        *count = 1;
    }
    
    return result;
}

void apkcheck_free_certificates(X509 **certs, int count) {
    if (certs == NULL) return;
    
    for (int i = 0; i < count; i++) {
        if (certs[i] != NULL) {
            X509_free(certs[i]);
        }
    }
    
    free(certs);
}

int apkcheck_detect_signature_versions(zip_t *zip, bool *has_v1, bool *has_v2, bool *has_v3) {
    if (zip == NULL || has_v1 == NULL || has_v2 == NULL || has_v3 == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    *has_v1 = false;
    *has_v2 = false;
    *has_v3 = false;
    
    zip_int64_t manifest_idx = zip_name_locate(zip, MANIFEST_MF, 0);
    zip_int64_t sf_idx = zip_name_locate(zip, CERT_SF, 0);
    zip_int64_t rsa_idx = zip_name_locate(zip, CERT_RSA, 0);
    
    if (manifest_idx >= 0 && sf_idx >= 0 && rsa_idx >= 0) {
        *has_v1 = true;
        apkcheck_log_info("Detected v1 signature (JAR signing)");
    }
    
    zip_int64_t entry_count = zip_get_num_entries(zip, 0);
    for (zip_int64_t i = 0; i < entry_count; i++) {
        const char *name = zip_get_name(zip, (zip_uint64_t)i, 0);
        if (name == NULL) continue;
        
        if (strcmp(name, "APK Signing Block") == 0) {
            apkcheck_log_debug("Found APK Signing Block (v2/v3 signature)");
            *has_v2 = true;
            break;
        }
    }
    
    return APKCHECK_SUCCESS;
}

int apkcheck_verify_manifest_integrity(zip_t *zip, const apkcheck_manifest_t *manifest) {
    if (zip == NULL || manifest == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    int all_valid = 1;
    
    for (size_t i = 0; i < manifest->entry_count; i++) {
        const apkcheck_manifest_entry_t *entry = &manifest->entries[i];
        
        zip_int64_t index = zip_name_locate(zip, entry->name, 0);
        if (index < 0) {
            apkcheck_log_warn("Entry in manifest not found in APK: %s", entry->name);
            continue;
        }
        
        if (entry->digest_sha256[0] != '\0') {
            uint8_t digest[32];
            size_t digest_len = sizeof(digest);
            
            if (apkcheck_compute_entry_digest(zip, (zip_uint64_t)index, APKCHECK_DIGEST_SHA256,
                                                digest, &digest_len) == APKCHECK_SUCCESS) {
                char computed_b64[128];
                apkcheck_base64_encode(digest, digest_len, computed_b64, sizeof(computed_b64));
                
                if (strcmp(computed_b64, entry->digest_sha256) != 0) {
                    apkcheck_log_error("SHA-256 digest mismatch for: %s", entry->name);
                    apkcheck_log_error("  Expected: %s", entry->digest_sha256);
                    apkcheck_log_error("  Computed: %s", computed_b64);
                    all_valid = 0;
                } else {
                    apkcheck_log_debug("SHA-256 verified: %s", entry->name);
                }
            }
        } else if (entry->digest_sha1[0] != '\0') {
            uint8_t digest[20];
            size_t digest_len = sizeof(digest);
            
            if (apkcheck_compute_entry_digest(zip, (zip_uint64_t)index, APKCHECK_DIGEST_SHA1,
                                                digest, &digest_len) == APKCHECK_SUCCESS) {
                char computed_b64[64];
                apkcheck_base64_encode(digest, digest_len, computed_b64, sizeof(computed_b64));
                
                if (strcmp(computed_b64, entry->digest_sha1) != 0) {
                    apkcheck_log_error("SHA1 digest mismatch for: %s", entry->name);
                    all_valid = 0;
                } else {
                    apkcheck_log_debug("SHA1 verified: %s", entry->name);
                }
            }
        }
    }
    
    return all_valid ? APKCHECK_SUCCESS : APKCHECK_ERROR_SIGNATURE;
}

int apkcheck_verify_v1_signature(zip_t *zip, apkcheck_cert_info_t *cert_info, bool *valid) {
    if (zip == NULL || valid == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    *valid = false;
    
    X509 *cert = NULL;
    EVP_PKEY *pubkey = NULL;
    
    int result = apkcheck_read_cert_rsa(zip, &cert, &pubkey);
    if (result != APKCHECK_SUCCESS) {
        apkcheck_log_debug("No v1 signature found");
        return result;
    }
    
    if (cert_info != NULL) {
        apkcheck_cert_get_info(cert, cert_info);
    }
    
    apkcheck_manifest_t *manifest = NULL;
    result = apkcheck_read_manifest_mf(zip, &manifest);
    
    if (result == APKCHECK_SUCCESS && manifest != NULL) {
        result = apkcheck_verify_manifest_integrity(zip, manifest);
        if (result == APKCHECK_SUCCESS) {
            *valid = true;
            apkcheck_log_info("v1 signature verified successfully");
        }
        apkcheck_manifest_destroy(manifest);
    }
    
    if (cert != NULL) X509_free(cert);
    if (pubkey != NULL) EVP_PKEY_free(pubkey);
    
    return result;
}

int apkcheck_verify_apk(const char *apk_path, apkcheck_verify_result_t *result) {
    if (apk_path == NULL || result == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(apkcheck_verify_result_t));
    apkcheck_safe_strcpy(result->apk_path, sizeof(result->apk_path), apk_path);
    
    if (!apkcheck_file_readable(apk_path)) {
        apkcheck_log_error("Cannot read APK: %s", apk_path);
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    int err = 0;
    zip_t *zip = zip_open(apk_path, 0, &err);
    if (zip == NULL) {
        apkcheck_log_error("Failed to open APK: %s", apk_path);
        return APKCHECK_ERROR_FILE_NOT_FOUND;
    }
    
    apkcheck_log_info("Verifying APK: %s", apk_path);
    
    apkcheck_detect_signature_versions(zip, &result->has_v1_signature,
                                         &result->has_v2_signature, &result->has_v3_signature);
    
    if (result->has_v1_signature) {
        apkcheck_verify_v1_signature(zip, &result->cert_info, &result->v1_verified);
    }
    
    result->v2_verified = false;
    result->v3_verified = false;
    
    zip_close(zip);
    
    apkcheck_log_info("Verification complete:");
    apkcheck_log_info("  v1 signature: %s (%s)",
                      result->has_v1_signature ? "present" : "not present",
                      result->v1_verified ? "verified" : "not verified");
    apkcheck_log_info("  v2 signature: %s",
                      result->has_v2_signature ? "present" : "not present");
    apkcheck_log_info("  v3 signature: %s",
                      result->has_v3_signature ? "present" : "not present");
    
    return APKCHECK_SUCCESS;
}

int apkcheck_print_signature_info(const apkcheck_verify_result_t *result) {
    if (result == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    printf("\n");
    printf("========================================\n");
    printf("APK Signature Verification Result\n");
    printf("========================================\n");
    printf("APK Path: %s\n", result->apk_path);
    printf("\n");
    
    printf("Signature Versions:\n");
    printf("  v1 (JAR signing): %s%s\n",
           result->has_v1_signature ? "Present" : "Not present",
           result->has_v1_signature ? (result->v1_verified ? " (Verified)" : " (NOT Verified)") : "");
    printf("  v2 (APK Signature Scheme v2): %s\n",
           result->has_v2_signature ? "Present" : "Not present");
    printf("  v3 (APK Signature Scheme v3): %s\n",
           result->has_v3_signature ? "Present" : "Not present");
    
    if (result->cert_info.is_valid) {
        printf("\n");
        printf("Certificate Information:\n");
        printf("  Subject: %s\n", result->cert_info.subject);
        printf("  Issuer: %s\n", result->cert_info.issuer);
        printf("  Serial Number: %s\n", result->cert_info.serial_number);
        printf("  Valid From: %s\n", result->cert_info.valid_from);
        printf("  Valid To: %s\n", result->cert_info.valid_to);
        printf("  Signature Algorithm: %s\n", result->cert_info.signature_algorithm);
        
        char sha1_hex[64];
        char sha256_hex[128];
        
        apkcheck_hex_encode(result->cert_info.fingerprint_sha1, 20, sha1_hex, sizeof(sha1_hex));
        apkcheck_hex_encode(result->cert_info.fingerprint_sha256, 32, sha256_hex, sizeof(sha256_hex));
        
        printf("\n");
        printf("Fingerprints:\n");
        printf("  SHA1: %s\n", sha1_hex);
        printf("  SHA256: %s\n", sha256_hex);
    }
    
    printf("\n");
    printf("========================================\n");
    printf("\n");
    
    return APKCHECK_SUCCESS;
}

int apkcheck_verify_apk_alignment(const char *apk_path, bool *aligned) {
    int result = apkcheck_verify_alignment(apk_path);
    
    if (aligned != NULL) {
        *aligned = true;
    }
    
    return result;
}

int apkcheck_get_signature_fingerprints(X509 *cert, char *sha1_hex, size_t sha1_len,
                                         char *sha256_hex, size_t sha256_len) {
    if (cert == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (sha1_hex != NULL && sha1_len > 0) {
        uint8_t fp_sha1[20];
        size_t fp_len = sizeof(fp_sha1);
        
        if (apkcheck_cert_get_fingerprint(cert, APKCHECK_DIGEST_SHA1, fp_sha1, &fp_len) == APKCHECK_SUCCESS) {
            apkcheck_hex_encode(fp_sha1, fp_len, sha1_hex, sha1_len);
        } else {
            sha1_hex[0] = '\0';
        }
    }
    
    if (sha256_hex != NULL && sha256_len > 0) {
        uint8_t fp_sha256[32];
        size_t fp_len = sizeof(fp_sha256);
        
        if (apkcheck_cert_get_fingerprint(cert, APKCHECK_DIGEST_SHA256, fp_sha256, &fp_len) == APKCHECK_SUCCESS) {
            apkcheck_hex_encode(fp_sha256, fp_len, sha256_hex, sha256_len);
        } else {
            sha256_hex[0] = '\0';
        }
    }
    
    return APKCHECK_SUCCESS;
}

apkcheck_sig_alg_t apkcheck_detect_signature_algorithm(const char *algorithm_name) {
    if (algorithm_name == NULL) {
        return APKCHECK_SIG_ALG_SHA256withRSA;
    }
    
    char lower[256];
    apkcheck_safe_strcpy(lower, sizeof(lower), algorithm_name);
    apkcheck_to_lower(lower);
    
    if (strstr(lower, "sha1") != NULL) {
        return APKCHECK_SIG_ALG_SHA1withRSA;
    } else if (strstr(lower, "sha512") != NULL) {
        return APKCHECK_SIG_ALG_SHA512withRSA;
    }
    
    return APKCHECK_SIG_ALG_SHA256withRSA;
}
