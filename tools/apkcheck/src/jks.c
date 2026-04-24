#include "jks.h"
#include "log.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <arpa/inet.h>

static uint32_t read_uint32(const uint8_t **ptr) {
    uint32_t val = ntohl(*(uint32_t *)(*ptr));
    *ptr += 4;
    return val;
}

static uint16_t read_uint16(const uint8_t **ptr) {
    uint16_t val = ntohs(*(uint16_t *)(*ptr));
    *ptr += 2;
    return val;
}

static uint64_t read_uint64(const uint8_t **ptr) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val = (val << 8) | (*ptr)[i];
    }
    *ptr += 8;
    return val;
}

static void write_uint32(uint8_t **ptr, uint32_t val) {
    *(uint32_t *)(*ptr) = htonl(val);
    *ptr += 4;
}

static void write_uint16(uint8_t **ptr, uint16_t val) {
    *(uint16_t *)(*ptr) = htons(val);
    *ptr += 2;
}

static void write_uint64(uint8_t **ptr, uint64_t val) {
    for (int i = 7; i >= 0; i--) {
        (*ptr)[i] = (uint8_t)(val & 0xFF);
        val >>= 8;
    }
    *ptr += 8;
}

static void write_utf8(uint8_t **ptr, const char *str) {
    size_t len = strlen(str);
    write_uint16(ptr, (uint16_t)len);
    if (len > 0) {
        memcpy(*ptr, str, len);
        *ptr += len;
    }
}

static int read_utf8(const uint8_t **ptr, const uint8_t *end, char *buf, size_t buf_len) {
    if (*ptr + 2 > end) return -1;
    uint16_t len = read_uint16(ptr);
    
    if (*ptr + len > end) return -1;
    if ((size_t)len >= buf_len) return -1;
    
    memcpy(buf, *ptr, len);
    buf[len] = '\0';
    *ptr += len;
    return 0;
}

jks_keystore_t *jks_keystore_create(const char *password) {
    jks_keystore_t *ks = (jks_keystore_t *)malloc(sizeof(jks_keystore_t));
    if (ks == NULL) {
        apkcheck_log_error("Failed to allocate keystore");
        return NULL;
    }
    
    memset(ks, 0, sizeof(jks_keystore_t));
    ks->magic = JKS_MAGIC;
    ks->version = JKS_VERSION;
    
    if (password != NULL) {
        apkcheck_safe_strcpy(ks->password, sizeof(ks->password), password);
    }
    
    return ks;
}

void jks_keystore_destroy(jks_keystore_t *ks) {
    if (ks == NULL) return;
    
    for (int i = 0; i < ks->entry_count; i++) {
        if (ks->entries[i] != NULL) {
            if (ks->entries[i]->type == JKS_ENTRY_TYPE_PRIVATE_KEY) {
                jks_private_key_entry_t *pk_entry = (jks_private_key_entry_t *)ks->entries[i];
                if (pk_entry->private_key != NULL) {
                    EVP_PKEY_free(pk_entry->private_key);
                }
                if (pk_entry->cert_chain != NULL) {
                    for (int j = 0; j < pk_entry->cert_chain_len; j++) {
                        if (pk_entry->cert_chain[j] != NULL) {
                            X509_free(pk_entry->cert_chain[j]);
                        }
                    }
                    free(pk_entry->cert_chain);
                }
            } else if (ks->entries[i]->type == JKS_ENTRY_TYPE_TRUSTED_CERT) {
                jks_trusted_cert_entry_t *tc_entry = (jks_trusted_cert_entry_t *)ks->entries[i];
                if (tc_entry->cert != NULL) {
                    X509_free(tc_entry->cert);
                }
            }
            free(ks->entries[i]);
        }
    }
    
    free(ks->entries);
    apkcheck_memzero(ks->password, sizeof(ks->password));
    apkcheck_memzero(ks, sizeof(jks_keystore_t));
    free(ks);
}

static int jks_read_private_key_entry(const uint8_t **ptr, const uint8_t *end,
                                        jks_private_key_entry_t *entry) {
    uint32_t entry_len = read_uint32(ptr);
    if (*ptr + entry_len > end) return -1;
    
    const uint8_t *entry_end = *ptr + entry_len;
    
    uint32_t protected_privkey_len = read_uint32(ptr);
    if (*ptr + protected_privkey_len > entry_end) return -1;
    
    apkcheck_log_debug("Private key entry: protected key len = %u", protected_privkey_len);
    
    uint8_t *protected_key = (uint8_t *)malloc(protected_privkey_len);
    if (protected_key == NULL) return -1;
    memcpy(protected_key, *ptr, protected_privkey_len);
    *ptr += protected_privkey_len;
    
    entry->private_key = jks_decrypt_private_key(protected_key, protected_privkey_len, 
                                                    entry->header.alias);
    free(protected_key);
    
    if (entry->private_key == NULL) {
        apkcheck_log_error("Failed to decrypt private key");
        return -1;
    }
    
    uint32_t cert_chain_len = read_uint32(ptr);
    apkcheck_log_debug("Certificate chain length: %u", cert_chain_len);
    
    if (cert_chain_len > 0) {
        entry->cert_chain = (X509 **)malloc(cert_chain_len * sizeof(X509 *));
        if (entry->cert_chain == NULL) return -1;
        entry->cert_chain_len = (int)cert_chain_len;
        
        for (uint32_t i = 0; i < cert_chain_len; i++) {
            char cert_type[256];
            if (read_utf8(ptr, entry_end, cert_type, sizeof(cert_type)) != 0) return -1;
            
            uint32_t cert_data_len = read_uint32(ptr);
            if (*ptr + cert_data_len > entry_end) return -1;
            
            const uint8_t *cert_data = *ptr;
            *ptr += cert_data_len;
            
            entry->cert_chain[i] = d2i_X509(NULL, &cert_data, (long)cert_data_len);
            if (entry->cert_chain[i] == NULL) {
                apkcheck_log_error("Failed to parse certificate %u", i);
            }
        }
    }
    
    return 0;
}

static int jks_read_trusted_cert_entry(const uint8_t **ptr, const uint8_t *end,
                                         jks_trusted_cert_entry_t *entry) {
    if (read_utf8(ptr, end, entry->cert_type, sizeof(entry->cert_type)) != 0) return -1;
    
    uint32_t cert_data_len = read_uint32(ptr);
    if (*ptr + cert_data_len > end) return -1;
    
    const uint8_t *cert_data = *ptr;
    *ptr += cert_data_len;
    
    entry->cert = d2i_X509(NULL, &cert_data, (long)cert_data_len);
    if (entry->cert == NULL) {
        apkcheck_log_error("Failed to parse trusted certificate");
        return -1;
    }
    
    return 0;
}

int jks_keystore_load(jks_keystore_t **ks, const char *path, const char *password) {
    if (ks == NULL || path == NULL || password == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    apkcheck_buffer_t *buf = apkcheck_buffer_create(0);
    if (buf == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    int result = apkcheck_read_file(path, buf);
    if (result != APKCHECK_SUCCESS) {
        apkcheck_buffer_destroy(buf);
        return result;
    }
    
    const uint8_t *ptr = buf->data;
    const uint8_t *end = buf->data + buf->len;
    
    *ks = jks_keystore_create(password);
    if (*ks == NULL) {
        apkcheck_buffer_destroy(buf);
        return APKCHECK_ERROR_MEMORY;
    }
    
    (*ks)->magic = read_uint32(&ptr);
    (*ks)->version = read_uint32(&ptr);
    
    apkcheck_log_info("JKS keystore: magic=0x%08X, version=%u", (*ks)->magic, (*ks)->version);
    
    if ((*ks)->magic != JKS_MAGIC || ((*ks)->version != 1 && (*ks)->version != 2)) {
        jks_keystore_destroy(*ks);
        *ks = NULL;
        apkcheck_buffer_destroy(buf);
        apkcheck_log_error("Invalid JKS magic or version");
        return APKCHECK_ERROR_INVALID_FORMAT;
    }
    
    (*ks)->entry_count = (int)read_uint32(&ptr);
    apkcheck_log_info("Keystore has %d entries", (*ks)->entry_count);
    
    if ((*ks)->entry_count > 0) {
        (*ks)->entries = (jks_entry_header_t **)malloc(
            (size_t)(*ks)->entry_count * sizeof(jks_entry_header_t *));
        if ((*ks)->entries == NULL) {
            jks_keystore_destroy(*ks);
            *ks = NULL;
            apkcheck_buffer_destroy(buf);
            return APKCHECK_ERROR_MEMORY;
        }
        memset((*ks)->entries, 0, (size_t)(*ks)->entry_count * sizeof(jks_entry_header_t *));
        
        for (int i = 0; i < (*ks)->entry_count; i++) {
            if (ptr >= end) break;
            
            int type = (int)read_uint32(&ptr);
            char alias[256];
            
            if (read_utf8(&ptr, end, alias, sizeof(alias)) != 0) break;
            
            uint64_t timestamp = read_uint64(&ptr);
            
            apkcheck_log_info("Entry %d: type=%d, alias=%s", i, type, alias);
            
            if (type == JKS_ENTRY_TYPE_PRIVATE_KEY) {
                jks_private_key_entry_t *entry = (jks_private_key_entry_t *)malloc(
                    sizeof(jks_private_key_entry_t));
                if (entry == NULL) break;
                
                memset(entry, 0, sizeof(jks_private_key_entry_t));
                entry->header.type = JKS_ENTRY_TYPE_PRIVATE_KEY;
                apkcheck_safe_strcpy(entry->header.alias, sizeof(entry->header.alias), alias);
                entry->header.creation_date = (time_t)(timestamp / 1000);
                
                if (jks_read_private_key_entry(&ptr, end, entry) == 0) {
                    (*ks)->entries[i] = &entry->header;
                } else {
                    free(entry);
                    apkcheck_log_warn("Failed to read private key entry");
                }
            } else if (type == JKS_ENTRY_TYPE_TRUSTED_CERT) {
                jks_trusted_cert_entry_t *entry = (jks_trusted_cert_entry_t *)malloc(
                    sizeof(jks_trusted_cert_entry_t));
                if (entry == NULL) break;
                
                memset(entry, 0, sizeof(jks_trusted_cert_entry_t));
                entry->header.type = JKS_ENTRY_TYPE_TRUSTED_CERT;
                apkcheck_safe_strcpy(entry->header.alias, sizeof(entry->header.alias), alias);
                entry->header.creation_date = (time_t)(timestamp / 1000);
                
                if (jks_read_trusted_cert_entry(&ptr, end, entry) == 0) {
                    (*ks)->entries[i] = &entry->header;
                } else {
                    free(entry);
                    apkcheck_log_warn("Failed to read trusted cert entry");
                }
            }
        }
    }
    
    if ((*ks)->version >= 2) {
        size_t mac_data_len = (size_t)(ptr - buf->data);
        if (ptr + 20 <= end) {
            uint8_t stored_mac[20];
            memcpy(stored_mac, ptr, 20);
            
            uint8_t computed_mac[20];
            size_t mac_len = 20;
            
            if (jks_compute_mac(buf->data, mac_data_len, password, computed_mac, &mac_len) == 0) {
                if (memcmp(stored_mac, computed_mac, 20) != 0) {
                    apkcheck_log_warn("Keystore MAC verification failed (wrong password?)");
                } else {
                    apkcheck_log_info("Keystore MAC verified successfully");
                }
            }
        }
    }
    
    apkcheck_buffer_destroy(buf);
    return APKCHECK_SUCCESS;
}

int jks_compute_mac(const uint8_t *data, size_t len, const char *password,
                      uint8_t *mac, size_t *mac_len) {
    if (data == NULL || password == NULL || mac == NULL || mac_len == NULL) {
        return -1;
    }
    
    size_t pass_len = strlen(password);
    size_t unicode_pass_len = pass_len * 2;
    uint8_t *unicode_pass = (uint8_t *)malloc(unicode_pass_len);
    
    for (size_t i = 0; i < pass_len; i++) {
        unicode_pass[i * 2] = 0;
        unicode_pass[i * 2 + 1] = (uint8_t)password[i];
    }
    
    const char *magic = "Mighty Aphrodite";
    size_t magic_len = strlen(magic);
    
    size_t total_len = unicode_pass_len + magic_len + len;
    uint8_t *mac_data = (uint8_t *)malloc(total_len);
    
    size_t offset = 0;
    memcpy(mac_data + offset, unicode_pass, unicode_pass_len);
    offset += unicode_pass_len;
    memcpy(mac_data + offset, magic, magic_len);
    offset += magic_len;
    memcpy(mac_data + offset, data, len);
    
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int md_len = 0;
    int result = -1;
    
    if (ctx != NULL) {
        if (EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) == 1 &&
            EVP_DigestUpdate(ctx, mac_data, total_len) == 1 &&
            EVP_DigestFinal_ex(ctx, mac, &md_len) == 1) {
            *mac_len = md_len;
            result = 0;
        }
        EVP_MD_CTX_free(ctx);
    }
    
    apkcheck_memzero(unicode_pass, unicode_pass_len);
    free(unicode_pass);
    free(mac_data);
    
    return result;
}

int jks_encrypt_private_key(EVP_PKEY *key, const char *password,
                               uint8_t **output, size_t *output_len) {
    if (key == NULL || password == NULL || output == NULL || output_len == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    unsigned char iv[8];
    if (apkcheck_random_bytes(iv, sizeof(iv)) != APKCHECK_SUCCESS) {
        return APKCHECK_ERROR_CRYPTO;
    }
    
    size_t pass_len = strlen(password);
    uint8_t key_bytes[20];
    uint8_t *pass_utf16 = (uint8_t *)malloc(pass_len * 2);
    
    for (size_t i = 0; i < pass_len; i++) {
        pass_utf16[i * 2] = 0;
        pass_utf16[i * 2 + 1] = (uint8_t)password[i];
    }
    
    uint8_t xor_key[20];
    memset(xor_key, 0, sizeof(xor_key));
    
    uint8_t current_hash[20];
    uint8_t hash_input[256];
    
    for (int round = 0; round < 20; round++) {
        size_t input_len = 0;
        for (int i = 0; i < 5; i++) {
            hash_input[input_len++] = (uint8_t)(round / 10 + '0');
        }
        memcpy(hash_input + input_len, xor_key, 20);
        input_len += 20;
        memcpy(hash_input + input_len, pass_utf16, pass_len * 2);
        input_len += pass_len * 2;
        memcpy(hash_input + input_len, iv, 8);
        input_len += 8;
        
        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        unsigned int md_len;
        if (EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) == 1 &&
            EVP_DigestUpdate(ctx, hash_input, input_len) == 1 &&
            EVP_DigestFinal_ex(ctx, current_hash, &md_len) == 1) {
            memcpy(xor_key, current_hash, 20);
        }
        EVP_MD_CTX_free(ctx);
    }
    
    memcpy(key_bytes, xor_key, 20);
    
    BIO *bio = BIO_new(BIO_s_mem());
    if (bio == NULL) {
        free(pass_utf16);
        return APKCHECK_ERROR_MEMORY;
    }
    
    i2d_PrivateKey_bio(bio, key);
    
    BUF_MEM *bptr;
    BIO_get_mem_ptr(bio, &bptr);
    
    size_t key_der_len = bptr->length;
    uint8_t *key_der = (uint8_t *)malloc(key_der_len);
    memcpy(key_der, bptr->data, key_der_len);
    BIO_free(bio);
    
    uint8_t padded_key[1024];
    size_t padded_len = ((key_der_len + 7) / 8) * 8;
    
    memcpy(padded_key, key_der, key_der_len);
    for (size_t i = key_der_len; i < padded_len; i++) {
        padded_key[i] = (uint8_t)(padded_len - key_der_len);
    }
    
    free(key_der);
    
    uint8_t *encrypted = (uint8_t *)malloc(8 + padded_len);
    if (encrypted == NULL) {
        free(pass_utf16);
        return APKCHECK_ERROR_MEMORY;
    }
    
    memcpy(encrypted, iv, 8);
    
    int key_idx = 0;
    for (size_t i = 0; i < padded_len; i++) {
        encrypted[8 + i] = padded_key[i] ^ key_bytes[key_idx % 20];
        key_idx++;
    }
    
    apkcheck_memzero(pass_utf16, pass_len * 2);
    free(pass_utf16);
    apkcheck_memzero(key_bytes, sizeof(key_bytes));
    
    *output = encrypted;
    *output_len = 8 + padded_len;
    
    return APKCHECK_SUCCESS;
}

EVP_PKEY *jks_decrypt_private_key(const uint8_t *input, size_t input_len,
                                    const char *password) {
    if (input == NULL || input_len < 8 || password == NULL) {
        return NULL;
    }
    
    const uint8_t *iv = input;
    const uint8_t *encrypted_key = input + 8;
    size_t encrypted_len = input_len - 8;
    
    size_t pass_len = strlen(password);
    uint8_t *pass_utf16 = (uint8_t *)malloc(pass_len * 2);
    
    for (size_t i = 0; i < pass_len; i++) {
        pass_utf16[i * 2] = 0;
        pass_utf16[i * 2 + 1] = (uint8_t)password[i];
    }
    
    uint8_t xor_key[20];
    memset(xor_key, 0, sizeof(xor_key));
    
    uint8_t current_hash[20];
    uint8_t hash_input[256];
    
    for (int round = 0; round < 20; round++) {
        size_t input_len_hash = 0;
        for (int i = 0; i < 5; i++) {
            hash_input[input_len_hash++] = (uint8_t)(round / 10 + '0');
        }
        memcpy(hash_input + input_len_hash, xor_key, 20);
        input_len_hash += 20;
        memcpy(hash_input + input_len_hash, pass_utf16, pass_len * 2);
        input_len_hash += pass_len * 2;
        memcpy(hash_input + input_len_hash, iv, 8);
        input_len_hash += 8;
        
        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        unsigned int md_len;
        if (EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) == 1 &&
            EVP_DigestUpdate(ctx, hash_input, input_len_hash) == 1 &&
            EVP_DigestFinal_ex(ctx, current_hash, &md_len) == 1) {
            memcpy(xor_key, current_hash, 20);
        }
        EVP_MD_CTX_free(ctx);
    }
    
    uint8_t key_bytes[20];
    memcpy(key_bytes, xor_key, 20);
    
    uint8_t *decrypted = (uint8_t *)malloc(encrypted_len);
    if (decrypted == NULL) {
        free(pass_utf16);
        return NULL;
    }
    
    int key_idx = 0;
    for (size_t i = 0; i < encrypted_len; i++) {
        decrypted[i] = encrypted_key[i] ^ key_bytes[key_idx % 20];
        key_idx++;
    }
    
    if (encrypted_len > 0) {
        uint8_t pad_len = decrypted[encrypted_len - 1];
        if (pad_len > 0 && pad_len <= 8) {
            bool valid_pad = true;
            for (size_t i = encrypted_len - pad_len; i < encrypted_len; i++) {
                if (decrypted[i] != pad_len) {
                    valid_pad = false;
                    break;
                }
            }
            if (valid_pad) {
                encrypted_len -= pad_len;
            }
        }
    }
    
    const uint8_t *ptr = decrypted;
    EVP_PKEY *key = d2i_AutoPrivateKey(NULL, &ptr, (long)encrypted_len);
    
    apkcheck_memzero(pass_utf16, pass_len * 2);
    free(pass_utf16);
    apkcheck_memzero(key_bytes, sizeof(key_bytes));
    apkcheck_memzero(decrypted, encrypted_len);
    free(decrypted);
    
    return key;
}

static size_t jks_calculate_entry_size(jks_entry_header_t *entry) {
    if (entry->type == JKS_ENTRY_TYPE_PRIVATE_KEY) {
        jks_private_key_entry_t *pk = (jks_private_key_entry_t *)entry;
        size_t size = 0;
        
        BIO *bio = BIO_new(BIO_s_mem());
        i2d_PrivateKey_bio(bio, pk->private_key);
        BUF_MEM *bptr;
        BIO_get_mem_ptr(bio, &bptr);
        size_t key_der_len = bptr->length;
        BIO_free(bio);
        
        size += 4;
        size_t padded_key_len = ((key_der_len + 7) / 8) * 8;
        size += 4 + 8 + padded_key_len;
        
        size += 4;
        for (int i = 0; i < pk->cert_chain_len; i++) {
            size += 2 + strlen("X.509");
            size += 4;
            
            bio = BIO_new(BIO_s_mem());
            i2d_X509_bio(bio, pk->cert_chain[i]);
            BIO_get_mem_ptr(bio, &bptr);
            size += bptr->length;
            BIO_free(bio);
        }
        
        return size;
    } else if (entry->type == JKS_ENTRY_TYPE_TRUSTED_CERT) {
        jks_trusted_cert_entry_t *tc = (jks_trusted_cert_entry_t *)entry;
        size_t size = 0;
        
        size += 2 + strlen(tc->cert_type);
        size += 4;
        
        BIO *bio = BIO_new(BIO_s_mem());
        i2d_X509_bio(bio, tc->cert);
        BUF_MEM *bptr;
        BIO_get_mem_ptr(bio, &bptr);
        size += bptr->length;
        BIO_free(bio);
        
        return size;
    }
    
    return 0;
}

static void jks_write_entry(uint8_t **ptr, jks_entry_header_t *entry) {
    write_uint32(ptr, (uint32_t)entry->type);
    write_utf8(ptr, entry->alias);
    write_uint64(ptr, (uint64_t)entry->creation_date * 1000);
    
    if (entry->type == JKS_ENTRY_TYPE_PRIVATE_KEY) {
        jks_private_key_entry_t *pk = (jks_private_key_entry_t *)entry;
        
        size_t entry_size = jks_calculate_entry_size(entry);
        write_uint32(ptr, (uint32_t)(entry_size - 4));
        
        uint8_t *protected_key = NULL;
        size_t protected_key_len = 0;
        
        if (jks_encrypt_private_key(pk->private_key, entry->alias,
                                     &protected_key, &protected_key_len) == APKCHECK_SUCCESS) {
            write_uint32(ptr, (uint32_t)protected_key_len);
            memcpy(*ptr, protected_key, protected_key_len);
            *ptr += protected_key_len;
            free(protected_key);
        }
        
        write_uint32(ptr, (uint32_t)pk->cert_chain_len);
        for (int i = 0; i < pk->cert_chain_len; i++) {
            write_utf8(ptr, "X.509");
            
            BIO *bio = BIO_new(BIO_s_mem());
            i2d_X509_bio(bio, pk->cert_chain[i]);
            BUF_MEM *bptr;
            BIO_get_mem_ptr(bio, &bptr);
            
            write_uint32(ptr, (uint32_t)bptr->length);
            memcpy(*ptr, bptr->data, bptr->length);
            *ptr += bptr->length;
            
            BIO_free(bio);
        }
    } else if (entry->type == JKS_ENTRY_TYPE_TRUSTED_CERT) {
        jks_trusted_cert_entry_t *tc = (jks_trusted_cert_entry_t *)entry;
        
        write_utf8(ptr, tc->cert_type);
        
        BIO *bio = BIO_new(BIO_s_mem());
        i2d_X509_bio(bio, tc->cert);
        BUF_MEM *bptr;
        BIO_get_mem_ptr(bio, &bptr);
        
        write_uint32(ptr, (uint32_t)bptr->length);
        memcpy(*ptr, bptr->data, bptr->length);
        *ptr += bptr->length;
        
        BIO_free(bio);
    }
}

int jks_keystore_save(jks_keystore_t *ks, const char *path) {
    if (ks == NULL || path == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    size_t total_size = 4 + 4 + 4;
    
    for (int i = 0; i < ks->entry_count; i++) {
        if (ks->entries[i] != NULL) {
            total_size += 4;
            total_size += 2 + strlen(ks->entries[i]->alias);
            total_size += 8;
            total_size += jks_calculate_entry_size(ks->entries[i]);
        }
    }
    
    total_size += 20;
    
    apkcheck_buffer_t *buf = apkcheck_buffer_create(total_size);
    if (buf == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    uint8_t *ptr = buf->data;
    
    write_uint32(&ptr, JKS_MAGIC);
    write_uint32(&ptr, (uint32_t)JKS_VERSION);
    write_uint32(&ptr, (uint32_t)ks->entry_count);
    
    for (int i = 0; i < ks->entry_count; i++) {
        if (ks->entries[i] != NULL) {
            jks_write_entry(&ptr, ks->entries[i]);
        }
    }
    
    size_t mac_data_len = (size_t)(ptr - buf->data);
    
    uint8_t mac[20];
    size_t mac_len = 20;
    
    if (jks_compute_mac(buf->data, mac_data_len, ks->password, mac, &mac_len) == 0) {
        memcpy(ptr, mac, mac_len);
        ptr += mac_len;
    }
    
    buf->len = (size_t)(ptr - buf->data);
    
    int result = apkcheck_write_file(path, buf->data, buf->len);
    apkcheck_buffer_destroy(buf);
    
    if (result == APKCHECK_SUCCESS) {
        apkcheck_log_info("Keystore saved to: %s", path);
    }
    
    return result;
}

int jks_keystore_add_private_key(jks_keystore_t *ks, const char *alias,
                                     EVP_PKEY *key, const char *key_password,
                                     X509 **cert_chain, int cert_chain_len) {
    if (ks == NULL || alias == NULL || key == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    jks_private_key_entry_t *entry = (jks_private_key_entry_t *)malloc(
        sizeof(jks_private_key_entry_t));
    if (entry == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    memset(entry, 0, sizeof(jks_private_key_entry_t));
    entry->header.type = JKS_ENTRY_TYPE_PRIVATE_KEY;
    apkcheck_safe_strcpy(entry->header.alias, sizeof(entry->header.alias), alias);
    entry->header.creation_date = time(NULL);
    
    entry->private_key = key;
    
    if (cert_chain != NULL && cert_chain_len > 0) {
        entry->cert_chain = (X509 **)malloc((size_t)cert_chain_len * sizeof(X509 *));
        if (entry->cert_chain == NULL) {
            free(entry);
            return APKCHECK_ERROR_MEMORY;
        }
        
        entry->cert_chain_len = cert_chain_len;
        memcpy(entry->cert_chain, cert_chain, (size_t)cert_chain_len * sizeof(X509 *));
    }
    
    jks_entry_header_t **new_entries = (jks_entry_header_t **)realloc(
        ks->entries, (size_t)(ks->entry_count + 1) * sizeof(jks_entry_header_t *));
    if (new_entries == NULL) {
        free(entry->cert_chain);
        free(entry);
        return APKCHECK_ERROR_MEMORY;
    }
    
    ks->entries = new_entries;
    ks->entries[ks->entry_count] = &entry->header;
    ks->entry_count++;
    
    apkcheck_log_info("Added private key entry: %s", alias);
    return APKCHECK_SUCCESS;
}

int jks_generate_keystore(const char *path, const char *store_password,
                         const apkcheck_keygen_config_t *config) {
    if (path == NULL || store_password == NULL || config == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    apkcheck_log_info("Generating RSA key pair (%d bits)...", config->key_size);
    
    EVP_PKEY *key = apkcheck_generate_rsa_key(config->key_size);
    if (key == NULL) {
        apkcheck_log_error("Failed to generate RSA key");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    apkcheck_log_info("Generating self-signed certificate...");
    
    X509 *cert = apkcheck_generate_self_signed_cert(key, config);
    if (cert == NULL) {
        EVP_PKEY_free(key);
        apkcheck_log_error("Failed to generate certificate");
        return APKCHECK_ERROR_CRYPTO;
    }
    
    jks_keystore_t *ks = jks_keystore_create(store_password);
    if (ks == NULL) {
        X509_free(cert);
        EVP_PKEY_free(key);
        return APKCHECK_ERROR_MEMORY;
    }
    
    X509 *cert_chain[] = {cert};
    
    int result = jks_keystore_add_private_key(ks, config->alias, key,
                                                 config->keypass, cert_chain, 1);
    if (result == APKCHECK_SUCCESS) {
        result = jks_keystore_save(ks, path);
    }
    
    jks_keystore_destroy(ks);
    
    return result;
}

int jks_keystore_contains_alias(jks_keystore_t *ks, const char *alias) {
    if (ks == NULL || alias == NULL) return 0;
    
    for (int i = 0; i < ks->entry_count; i++) {
        if (ks->entries[i] != NULL &&
            strcmp(ks->entries[i]->alias, alias) == 0) {
            return 1;
        }
    }
    return 0;
}

int jks_keystore_list_aliases(jks_keystore_t *ks, char ***aliases, int *count) {
    if (ks == NULL || aliases == NULL || count == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    *count = ks->entry_count;
    if (*count == 0) {
        *aliases = NULL;
        return APKCHECK_SUCCESS;
    }
    
    *aliases = (char **)malloc((size_t)*count * sizeof(char *));
    if (*aliases == NULL) {
        return APKCHECK_ERROR_MEMORY;
    }
    
    for (int i = 0; i < *count; i++) {
        if (ks->entries[i] != NULL) {
            (*aliases)[i] = apkcheck_strdup_safe(ks->entries[i]->alias);
        } else {
            (*aliases)[i] = NULL;
        }
    }
    
    return APKCHECK_SUCCESS;
}

int jks_keystore_get_private_key(jks_keystore_t *ks, const char *alias,
                                   const char *key_password,
                                   EVP_PKEY **key, X509 ***cert_chain,
                                   int *cert_chain_len) {
    if (ks == NULL || alias == NULL || key == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    for (int i = 0; i < ks->entry_count; i++) {
        if (ks->entries[i] != NULL &&
            ks->entries[i]->type == JKS_ENTRY_TYPE_PRIVATE_KEY &&
            strcmp(ks->entries[i]->alias, alias) == 0) {
            
            jks_private_key_entry_t *entry = (jks_private_key_entry_t *)ks->entries[i];
            *key = entry->private_key;
            
            if (cert_chain != NULL && cert_chain_len != NULL) {
                *cert_chain = entry->cert_chain;
                *cert_chain_len = entry->cert_chain_len;
            }
            
            return APKCHECK_SUCCESS;
        }
    }
    
    apkcheck_log_error("Alias not found: %s", alias);
    return APKCHECK_ERROR_FILE_NOT_FOUND;
}
