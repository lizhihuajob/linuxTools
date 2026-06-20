#include "cli.h"
#include "apkcheck.h"
#include "log.h"
#include "utils.h"
#include "crypto.h"
#include "jks.h"
#include "sign.h"
#include "verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

static struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"verbose", no_argument, NULL, 'V'},
    {"output", required_argument, NULL, 'o'},
    {"keystore", required_argument, NULL, 'k'},
    {"alias", required_argument, NULL, 'a'},
    {"storepass", required_argument, NULL, 'p'},
    {"keypass", required_argument, NULL, 'P'},
    {"keysize", required_argument, NULL, 'b'},
    {"validity", required_argument, NULL, 'd'},
    {"cn", required_argument, NULL, 'c'},
    {"ou", required_argument, NULL, 'u'},
    {"organization", required_argument, NULL, 'O'},
    {"locality", required_argument, NULL, 'L'},
    {"state", required_argument, NULL, 'S'},
    {"country", required_argument, NULL, 'C'},
    {"sigalg", required_argument, NULL, 'g'},
    {"digestalg", required_argument, NULL, 'D'},
    {"no-align-check", no_argument, NULL, 'n'},
    {"force", no_argument, NULL, 'f'},
    {"log-file", required_argument, NULL, 'l'},
    {0, 0, 0, 0}
};

void apkcheck_cli_config_init(apkcheck_cli_config_t *config) {
    if (config == NULL) return;
    
    memset(config, 0, sizeof(apkcheck_cli_config_t));
    config->log_level = APKCHECK_LOG_LEVEL_INFO;
    config->keygen.key_size = 2048;
    config->keygen.validity_days = 365 * 25;
}

void apkcheck_cli_config_cleanup(apkcheck_cli_config_t *config) {
    if (config == NULL) return;
    
    apkcheck_memzero(config->keygen.storepass, sizeof(config->keygen.storepass));
    apkcheck_memzero(config->keygen.keypass, sizeof(config->keygen.keypass));
    apkcheck_memzero(config->sign.storepass, sizeof(config->sign.storepass));
    apkcheck_memzero(config->sign.keypass, sizeof(config->sign.keypass));
    apkcheck_memzero(config->list_keys.storepass, sizeof(config->list_keys.storepass));
}

const char *apkcheck_get_sig_alg_name(apkcheck_sig_alg_t alg) {
    switch (alg) {
        case APKCHECK_SIG_ALG_SHA1withRSA:
            return "SHA1withRSA";
        case APKCHECK_SIG_ALG_SHA256withRSA:
            return "SHA256withRSA";
        case APKCHECK_SIG_ALG_SHA512withRSA:
            return "SHA512withRSA";
        default:
            return "Unknown";
    }
}

const char *apkcheck_get_digest_alg_name(apkcheck_digest_alg_t alg) {
    switch (alg) {
        case APKCHECK_DIGEST_MD5:
            return "MD5";
        case APKCHECK_DIGEST_SHA1:
            return "SHA1";
        case APKCHECK_DIGEST_SHA256:
            return "SHA-256";
        case APKCHECK_DIGEST_SHA512:
            return "SHA-512";
        default:
            return "Unknown";
    }
}

apkcheck_sig_alg_t apkcheck_parse_sig_alg(const char *name) {
    if (name == NULL) return APKCHECK_SIG_ALG_SHA256withRSA;
    
    char lower[256];
    apkcheck_safe_strcpy(lower, sizeof(lower), name);
    apkcheck_to_lower(lower);
    
    if (strstr(lower, "sha1") != NULL) {
        return APKCHECK_SIG_ALG_SHA1withRSA;
    } else if (strstr(lower, "sha512") != NULL) {
        return APKCHECK_SIG_ALG_SHA512withRSA;
    }
    
    return APKCHECK_SIG_ALG_SHA256withRSA;
}

apkcheck_digest_alg_t apkcheck_parse_digest_alg(const char *name) {
    if (name == NULL) return APKCHECK_DIGEST_SHA256;
    
    char lower[256];
    apkcheck_safe_strcpy(lower, sizeof(lower), name);
    apkcheck_to_lower(lower);
    
    if (strstr(lower, "md5") != NULL) {
        return APKCHECK_DIGEST_MD5;
    } else if (strstr(lower, "sha1") != NULL) {
        return APKCHECK_DIGEST_SHA1;
    } else if (strstr(lower, "sha512") != NULL) {
        return APKCHECK_DIGEST_SHA512;
    }
    
    return APKCHECK_DIGEST_SHA256;
}

static void print_option_help(const char *opt, const char *desc) {
    printf("  %-25s %s\n", opt, desc);
}

void apkcheck_cli_print_help(void) {
    printf("APKCheck - APK Signing Tool v%s\n", APKCHECK_VERSION);
    printf("A complete APK signing tool written in C\n\n");
    
    printf("Usage: apkcheck <command> [options]\n\n");
    
    printf("Commands:\n");
    printf("  keygen                   Generate a new JKS keystore\n");
    printf("  sign                     Sign an APK file\n");
    printf("  verify                   Verify APK signatures\n");
    printf("  list-keys                List keys in a keystore\n");
    printf("  help                     Show this help message\n");
    printf("  version                  Show version information\n\n");
    
    printf("Global Options:\n");
    print_option_help("-h, --help", "Show help message");
    print_option_help("-v, --version", "Show version information");
    print_option_help("-V, --verbose", "Enable verbose output");
    print_option_help("-l, --log-file FILE", "Log output to FILE");
    printf("\n");
    
    printf("Keygen Options (for 'keygen' command):\n");
    print_option_help("-k, --keystore FILE", "Path to keystore file (required)");
    print_option_help("-a, --alias NAME", "Key alias (default: 'mykey')");
    print_option_help("-p, --storepass PASS", "Keystore password (required)");
    print_option_help("-P, --keypass PASS", "Key password (same as storepass if not set)");
    print_option_help("-b, --keysize BITS", "Key size in bits (default: 2048)");
    print_option_help("-d, --validity DAYS", "Validity in days (default: 9125)");
    print_option_help("-c, --cn NAME", "Common Name");
    print_option_help("-u, --ou NAME", "Organizational Unit");
    print_option_help("-O, --organization NAME", "Organization");
    print_option_help("-L, --locality NAME", "Locality/City");
    print_option_help("-S, --state NAME", "State/Province");
    print_option_help("-C, --country CODE", "Country code (2 letters)");
    print_option_help("-g, --sigalg ALG", "Signature algorithm (SHA1withRSA, SHA256withRSA, SHA512withRSA)");
    printf("\n");
    
    printf("Sign Options (for 'sign' command):\n");
    print_option_help("-k, --keystore FILE", "Path to keystore file (required)");
    print_option_help("-a, --alias NAME", "Key alias (required)");
    print_option_help("-p, --storepass PASS", "Keystore password (required)");
    print_option_help("-P, --keypass PASS", "Key password (required)");
    print_option_help("-o, --output FILE", "Output signed APK path");
    print_option_help("-g, --sigalg ALG", "Signature algorithm (SHA1withRSA, SHA256withRSA, SHA512withRSA)");
    print_option_help("-n, --no-align-check", "Skip ZIP alignment verification");
    print_option_help("-f, --force", "Force overwrite output file");
    printf("\n");
    
    printf("Verify Options (for 'verify' command):\n");
    print_option_help("-V, --verbose", "Show detailed certificate information");
    printf("\n");
    
    printf("List Keys Options (for 'list-keys' command):\n");
    print_option_help("-k, --keystore FILE", "Path to keystore file (required)");
    print_option_help("-p, --storepass PASS", "Keystore password (required)");
    printf("\n");
    
    printf("Examples:\n");
    printf("  apkcheck keygen -k my.keystore -a mykey -p password -C US\n");
    printf("  apkcheck sign -k my.keystore -a mykey -p password -P password -o signed.apk app.apk\n");
    printf("  apkcheck verify signed.apk\n");
    printf("  apkcheck list-keys -k my.keystore -p password\n\n");
}

void apkcheck_cli_print_version(void) {
    printf("APKCheck v%s\n", APKCHECK_VERSION);
    printf("Copyright (c) 2024 APKCheck Contributors\n");
    printf("\n");
    printf("Built with:\n");
    printf("  - libzip\n");
    printf("  - OpenSSL\n");
    printf("\n");
}

void apkcheck_cli_print_usage(void) {
    printf("Usage: apkcheck <command> [options]\n");
    printf("Try 'apkcheck help' for more information.\n");
}

int apkcheck_cli_parse_args(int argc, char *argv[], apkcheck_cli_config_t *config) {
    if (argc < 2 || argv == NULL || config == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    apkcheck_cli_config_init(config);
    
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        config->command = CMD_HELP;
        return APKCHECK_SUCCESS;
    }
    
    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        config->command = CMD_VERSION;
        return APKCHECK_SUCCESS;
    }
    
    if (strcmp(argv[1], "keygen") == 0) {
        config->command = CMD_KEYGEN;
    } else if (strcmp(argv[1], "sign") == 0) {
        config->command = CMD_SIGN;
    } else if (strcmp(argv[1], "verify") == 0) {
        config->command = CMD_VERIFY;
    } else if (strcmp(argv[1], "list-keys") == 0) {
        config->command = CMD_LIST_KEYS;
    } else {
        apkcheck_log_error("Unknown command: %s", argv[1]);
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    int opt_index = 0;
    int c;
    
    optind = 2;
    
    while ((c = getopt_long(argc, argv, "hvVo:k:a:p:P:b:d:c:u:O:L:S:C:g:D:nfl:",
                              long_options, &opt_index)) != -1) {
        switch (c) {
            case 'h':
                config->command = CMD_HELP;
                break;
            case 'v':
                config->command = CMD_VERSION;
                break;
            case 'V':
                config->verbose = true;
                config->log_level = APKCHECK_LOG_LEVEL_DEBUG;
                break;
            case 'o':
                apkcheck_safe_strcpy(config->sign.output_path, sizeof(config->sign.output_path), optarg);
                break;
            case 'k':
                apkcheck_safe_strcpy(config->keygen.keystore_path, sizeof(config->keygen.keystore_path), optarg);
                apkcheck_safe_strcpy(config->sign.keystore_path, sizeof(config->sign.keystore_path), optarg);
                apkcheck_safe_strcpy(config->list_keys.keystore_path, sizeof(config->list_keys.keystore_path), optarg);
                break;
            case 'a':
                apkcheck_safe_strcpy(config->keygen.alias, sizeof(config->keygen.alias), optarg);
                apkcheck_safe_strcpy(config->sign.alias, sizeof(config->sign.alias), optarg);
                break;
            case 'p':
                apkcheck_safe_strcpy(config->keygen.storepass, sizeof(config->keygen.storepass), optarg);
                apkcheck_safe_strcpy(config->sign.storepass, sizeof(config->sign.storepass), optarg);
                apkcheck_safe_strcpy(config->list_keys.storepass, sizeof(config->list_keys.storepass), optarg);
                break;
            case 'P':
                apkcheck_safe_strcpy(config->keygen.keypass, sizeof(config->keygen.keypass), optarg);
                apkcheck_safe_strcpy(config->sign.keypass, sizeof(config->sign.keypass), optarg);
                break;
            case 'b':
                config->keygen.key_size = atoi(optarg);
                break;
            case 'd':
                config->keygen.validity_days = atoi(optarg);
                break;
            case 'c':
                apkcheck_safe_strcpy(config->keygen.common_name, sizeof(config->keygen.common_name), optarg);
                break;
            case 'u':
                apkcheck_safe_strcpy(config->keygen.organizational_unit, sizeof(config->keygen.organizational_unit), optarg);
                break;
            case 'O':
                apkcheck_safe_strcpy(config->keygen.organization, sizeof(config->keygen.organization), optarg);
                break;
            case 'L':
                apkcheck_safe_strcpy(config->keygen.city, sizeof(config->keygen.city), optarg);
                break;
            case 'S':
                apkcheck_safe_strcpy(config->keygen.state, sizeof(config->keygen.state), optarg);
                break;
            case 'C':
                apkcheck_safe_strcpy(config->keygen.country, sizeof(config->keygen.country), optarg);
                break;
            case 'g':
                apkcheck_safe_strcpy(config->keygen.sig_alg_name, sizeof(config->keygen.sig_alg_name), optarg);
                apkcheck_safe_strcpy(config->sign.sig_alg_name, sizeof(config->sign.sig_alg_name), optarg);
                break;
            case 'D':
                apkcheck_safe_strcpy(config->keygen.digest_alg_name, sizeof(config->keygen.digest_alg_name), optarg);
                break;
            case 'n':
                config->no_alignment_check = true;
                break;
            case 'f':
                config->force = true;
                break;
            case 'l':
                apkcheck_safe_strcpy(config->log_file, sizeof(config->log_file), optarg);
                break;
            case '?':
                return APKCHECK_ERROR_INVALID_PARAM;
            default:
                break;
        }
    }
    
    if (config->command == CMD_SIGN && optind < argc) {
        apkcheck_safe_strcpy(config->sign.apk_path, sizeof(config->sign.apk_path), argv[optind]);
    }
    
    if (config->command == CMD_VERIFY && optind < argc) {
        apkcheck_safe_strcpy(config->verify.apk_path, sizeof(config->verify.apk_path), argv[optind]);
        if (config->verbose) {
            config->verify.show_cert_details = true;
        }
    }
    
    if (config->keygen.alias[0] == '\0') {
        apkcheck_safe_strcpy(config->keygen.alias, sizeof(config->keygen.alias), "mykey");
    }
    
    if (config->keygen.keypass[0] == '\0' && config->keygen.storepass[0] != '\0') {
        apkcheck_safe_strcpy(config->keygen.keypass, sizeof(config->keygen.keypass), config->keygen.storepass);
    }
    
    return APKCHECK_SUCCESS;
}

static int cmd_keygen(const apkcheck_cli_config_t *config) {
    if (config->keygen.keystore_path[0] == '\0') {
        apkcheck_log_error("Keystore path is required (use -k/--keystore)");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (config->keygen.storepass[0] == '\0') {
        apkcheck_log_error("Keystore password is required (use -p/--storepass)");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (apkcheck_file_exists(config->keygen.keystore_path) && !config->force) {
        apkcheck_log_error("Keystore file already exists: %s", config->keygen.keystore_path);
        apkcheck_log_error("Use -f/--force to overwrite");
        return APKCHECK_ERROR_PERMISSION_DENIED;
    }
    
    apkcheck_keygen_config_t keygen_config;
    memset(&keygen_config, 0, sizeof(keygen_config));
    
    apkcheck_safe_strcpy(keygen_config.alias, sizeof(keygen_config.alias), config->keygen.alias);
    apkcheck_safe_strcpy(keygen_config.storepass, sizeof(keygen_config.storepass), config->keygen.storepass);
    apkcheck_safe_strcpy(keygen_config.keypass, sizeof(keygen_config.keypass), config->keygen.keypass);
    apkcheck_safe_strcpy(keygen_config.common_name, sizeof(keygen_config.common_name), config->keygen.common_name);
    apkcheck_safe_strcpy(keygen_config.organizational_unit, sizeof(keygen_config.organizational_unit), config->keygen.organizational_unit);
    apkcheck_safe_strcpy(keygen_config.organization, sizeof(keygen_config.organization), config->keygen.organization);
    apkcheck_safe_strcpy(keygen_config.city, sizeof(keygen_config.city), config->keygen.city);
    apkcheck_safe_strcpy(keygen_config.state, sizeof(keygen_config.state), config->keygen.state);
    apkcheck_safe_strcpy(keygen_config.country, sizeof(keygen_config.country), config->keygen.country);
    
    keygen_config.validity_days = config->keygen.validity_days;
    keygen_config.key_size = config->keygen.key_size;
    keygen_config.sig_alg = apkcheck_parse_sig_alg(config->keygen.sig_alg_name);
    
    if (keygen_config.common_name[0] == '\0') {
        apkcheck_safe_strcpy(keygen_config.common_name, sizeof(keygen_config.common_name), "APKCheck");
    }
    
    apkcheck_log_info("Generating keystore: %s", config->keygen.keystore_path);
    apkcheck_log_info("  Alias: %s", keygen_config.alias);
    apkcheck_log_info("  Key size: %d bits", keygen_config.key_size);
    apkcheck_log_info("  Validity: %d days", keygen_config.validity_days);
    apkcheck_log_info("  Signature algorithm: %s", apkcheck_get_sig_alg_name(keygen_config.sig_alg));
    
    int result = jks_generate_keystore(config->keygen.keystore_path,
                                         config->keygen.storepass,
                                         &keygen_config);
    
    if (result == APKCHECK_SUCCESS) {
        apkcheck_log_info("Keystore generated successfully: %s", config->keygen.keystore_path);
    } else {
        apkcheck_log_error("Failed to generate keystore: %s", apkcheck_strerror(result));
    }
    
    return result;
}

static int cmd_sign(const apkcheck_cli_config_t *config) {
    if (config->sign.keystore_path[0] == '\0') {
        apkcheck_log_error("Keystore path is required (use -k/--keystore)");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (config->sign.alias[0] == '\0') {
        apkcheck_log_error("Key alias is required (use -a/--alias)");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (config->sign.storepass[0] == '\0') {
        apkcheck_log_error("Keystore password is required (use -p/--storepass)");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (config->sign.keypass[0] == '\0') {
        apkcheck_log_error("Key password is required (use -P/--keypass)");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (config->sign.apk_path[0] == '\0') {
        apkcheck_log_error("APK path is required");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (!config->force && config->sign.output_path[0] != '\0' &&
        apkcheck_file_exists(config->sign.output_path)) {
        apkcheck_log_error("Output file already exists: %s", config->sign.output_path);
        apkcheck_log_error("Use -f/--force to overwrite");
        return APKCHECK_ERROR_PERMISSION_DENIED;
    }
    
    char output_path[APKCHECK_MAX_PATH_LEN];
    if (config->sign.output_path[0] == '\0') {
        const char *ext = apkcheck_file_extension(config->sign.apk_path);
        if (ext != NULL && strcmp(ext, "apk") == 0) {
            char *dir = apkcheck_dirname(config->sign.apk_path);
            char *base = apkcheck_basename(config->sign.apk_path);
            
            size_t base_len = strlen(base);
            char *base_no_ext = (char *)malloc(base_len + 1);
            if (base_no_ext != NULL) {
                strncpy(base_no_ext, base, base_len - 4);
                base_no_ext[base_len - 4] = '\0';
            }
            
            apkcheck_path_join(output_path, sizeof(output_path), dir,
                                (base_no_ext != NULL) ? base_no_ext : "signed");
            apkcheck_safe_strcat(output_path, sizeof(output_path), "-signed.apk");
            
            free(dir);
            free(base);
            free(base_no_ext);
        } else {
            apkcheck_safe_strcpy(output_path, sizeof(output_path), config->sign.apk_path);
            apkcheck_safe_strcat(output_path, sizeof(output_path), ".signed");
        }
    } else {
        apkcheck_safe_strcpy(output_path, sizeof(output_path), config->sign.output_path);
    }
    
    if (!config->no_alignment_check) {
        apkcheck_log_info("Checking APK alignment...");
        bool aligned;
        if (apkcheck_verify_apk_alignment(config->sign.apk_path, &aligned) == APKCHECK_SUCCESS) {
            if (!aligned) {
                apkcheck_log_warn("APK is not properly aligned. This may cause issues on some devices.");
                apkcheck_log_warn("Use -n/--no-align-check to skip this check.");
            } else {
                apkcheck_log_info("APK alignment: OK");
            }
        }
    }
    
    apkcheck_sign_config_t sign_config;
    memset(&sign_config, 0, sizeof(sign_config));
    
    apkcheck_safe_strcpy(sign_config.keystore_path, sizeof(sign_config.keystore_path),
                          config->sign.keystore_path);
    apkcheck_safe_strcpy(sign_config.alias, sizeof(sign_config.alias), config->sign.alias);
    apkcheck_safe_strcpy(sign_config.storepass, sizeof(sign_config.storepass), config->sign.storepass);
    apkcheck_safe_strcpy(sign_config.keypass, sizeof(sign_config.keypass), config->sign.keypass);
    sign_config.sig_alg = apkcheck_parse_sig_alg(config->sign.sig_alg_name);
    
    apkcheck_log_info("Signing APK: %s", config->sign.apk_path);
    apkcheck_log_info("  Output: %s", output_path);
    apkcheck_log_info("  Keystore: %s", sign_config.keystore_path);
    apkcheck_log_info("  Alias: %s", sign_config.alias);
    apkcheck_log_info("  Signature algorithm: %s", apkcheck_get_sig_alg_name(sign_config.sig_alg));
    
    int result = apkcheck_sign_v1(config->sign.apk_path, output_path, &sign_config);
    
    if (result != APKCHECK_SUCCESS) {
        apkcheck_log_error("Failed to sign APK: %s", apkcheck_strerror(result));
    }
    
    return result;
}

static int cmd_verify(const apkcheck_cli_config_t *config) {
    if (config->verify.apk_path[0] == '\0') {
        apkcheck_log_error("APK path is required");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    apkcheck_verify_result_t result;
    int ret = apkcheck_verify_apk(config->verify.apk_path, &result);
    
    if (ret == APKCHECK_SUCCESS) {
        apkcheck_print_signature_info(&result);
    }
    
    return ret;
}

static int cmd_list_keys(const apkcheck_cli_config_t *config) {
    if (config->list_keys.keystore_path[0] == '\0') {
        apkcheck_log_error("Keystore path is required (use -k/--keystore)");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    if (config->list_keys.storepass[0] == '\0') {
        apkcheck_log_error("Keystore password is required (use -p/--storepass)");
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    jks_keystore_t *ks = NULL;
    int result = jks_keystore_load(&ks, config->list_keys.keystore_path,
                                      config->list_keys.storepass);
    
    if (result != APKCHECK_SUCCESS) {
        apkcheck_log_error("Failed to load keystore: %s", apkcheck_strerror(result));
        return result;
    }
    
    printf("\n");
    printf("========================================\n");
    printf("Keystore Contents: %s\n", config->list_keys.keystore_path);
    printf("========================================\n");
    printf("\n");
    printf("Total entries: %d\n\n", ks->entry_count);
    
    for (int i = 0; i < ks->entry_count; i++) {
        if (ks->entries[i] == NULL) continue;
        
        printf("----------------------------------------\n");
        printf("Entry %d:\n", i + 1);
        printf("  Alias: %s\n", ks->entries[i]->alias);
        printf("  Type: %s\n",
               (ks->entries[i]->type == JKS_ENTRY_TYPE_PRIVATE_KEY) ? "PrivateKeyEntry" :
               (ks->entries[i]->type == JKS_ENTRY_TYPE_TRUSTED_CERT) ? "TrustedCertEntry" : "Unknown");
        
        char time_buf[64];
        apkcheck_format_time(ks->entries[i]->creation_date, time_buf, sizeof(time_buf),
                              "%Y-%m-%d %H:%M:%S");
        printf("  Created: %s\n", time_buf);
        
        if (ks->entries[i]->type == JKS_ENTRY_TYPE_PRIVATE_KEY) {
            jks_private_key_entry_t *pk = (jks_private_key_entry_t *)ks->entries[i];
            printf("  Certificate chain length: %d\n", pk->cert_chain_len);
            
            if (pk->cert_chain != NULL && pk->cert_chain_len > 0) {
                apkcheck_cert_info_t cert_info;
                apkcheck_cert_get_info(pk->cert_chain[0], &cert_info);
                
                printf("\n  Certificate Details:\n");
                printf("    Subject: %s\n", cert_info.subject);
                printf("    Issuer: %s\n", cert_info.issuer);
                printf("    Serial: %s\n", cert_info.serial_number);
                printf("    Valid: %s - %s\n", cert_info.valid_from, cert_info.valid_to);
                
                char sha1_hex[64], sha256_hex[128];
                apkcheck_hex_encode(cert_info.fingerprint_sha1, 20, sha1_hex, sizeof(sha1_hex));
                apkcheck_hex_encode(cert_info.fingerprint_sha256, 32, sha256_hex, sizeof(sha256_hex));
                
                printf("\n    Fingerprints:\n");
                printf("      SHA1: %s\n", sha1_hex);
                printf("      SHA256: %s\n", sha256_hex);
            }
        }
        printf("\n");
    }
    
    jks_keystore_destroy(ks);
    return APKCHECK_SUCCESS;
}

int apkcheck_cli_execute(const apkcheck_cli_config_t *config) {
    if (config == NULL) {
        return APKCHECK_ERROR_INVALID_PARAM;
    }
    
    int log_targets = APKCHECK_LOG_TARGET_CONSOLE;
    if (config->log_file[0] != '\0') {
        log_targets |= APKCHECK_LOG_TARGET_FILE;
    }
    
    apkcheck_log_init(config->log_file[0] ? config->log_file : NULL,
                      config->log_level, log_targets);
    
    apkcheck_crypto_init();
    
    int result = APKCHECK_SUCCESS;
    
    switch (config->command) {
        case CMD_HELP:
            apkcheck_cli_print_help();
            break;
        case CMD_VERSION:
            apkcheck_cli_print_version();
            break;
        case CMD_KEYGEN:
            result = cmd_keygen(config);
            break;
        case CMD_SIGN:
            result = cmd_sign(config);
            break;
        case CMD_VERIFY:
            result = cmd_verify(config);
            break;
        case CMD_LIST_KEYS:
            result = cmd_list_keys(config);
            break;
        default:
            apkcheck_cli_print_usage();
            result = APKCHECK_ERROR_INVALID_PARAM;
            break;
    }
    
    apkcheck_crypto_cleanup();
    apkcheck_log_shutdown();
    
    return result;
}
