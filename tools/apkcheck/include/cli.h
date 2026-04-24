#ifndef APKCHECK_CLI_H
#define APKCHECK_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apkcheck.h"

typedef enum {
    CMD_NONE = 0,
    CMD_HELP,
    CMD_VERSION,
    CMD_KEYGEN,
    CMD_SIGN,
    CMD_VERIFY,
    CMD_LIST_KEYS
} apkcheck_command_t;

typedef enum {
    OPT_NONE = 0,
    OPT_HELP = 'h',
    OPT_VERSION = 'v',
    OPT_VERBOSE = 'V',
    OPT_OUTPUT = 'o',
    OPT_KEYSTORE = 'k',
    OPT_ALIAS = 'a',
    OPT_STOREPASS = 'p',
    OPT_KEYPASS = 'P',
    OPT_KEYSIZE = 'b',
    OPT_VALIDITY = 'd',
    OPT_CN = 'c',
    OPT_OU = 'u',
    OPT_O = 'O',
    OPT_L = 'L',
    OPT_S = 'S',
    OPT_C = 'C',
    OPT_SIG_ALG = 'g',
    OPT_DIGEST_ALG = 'D',
    OPT_NO_ALIGN = 'n',
    OPT_FORCE = 'f',
    OPT_LOG_FILE = 'l'
} apkcheck_option_t;

typedef struct {
    apkcheck_command_t command;
    bool verbose;
    bool force;
    bool no_alignment_check;
    char log_file[APKCHECK_MAX_PATH_LEN];
    apkcheck_log_level_t log_level;
    
    struct {
        char keystore_path[APKCHECK_MAX_PATH_LEN];
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
        char sig_alg_name[64];
        char digest_alg_name[64];
    } keygen;
    
    struct {
        char apk_path[APKCHECK_MAX_PATH_LEN];
        char output_path[APKCHECK_MAX_PATH_LEN];
        char keystore_path[APKCHECK_MAX_PATH_LEN];
        char alias[APKCHECK_MAX_ALIAS_LEN];
        char storepass[APKCHECK_MAX_PASSWORD_LEN];
        char keypass[APKCHECK_MAX_PASSWORD_LEN];
        char sig_alg_name[64];
    } sign;
    
    struct {
        char apk_path[APKCHECK_MAX_PATH_LEN];
        bool show_cert_details;
    } verify;
    
    struct {
        char keystore_path[APKCHECK_MAX_PATH_LEN];
        char storepass[APKCHECK_MAX_PASSWORD_LEN];
    } list_keys;
    
} apkcheck_cli_config_t;

int apkcheck_cli_parse_args(int argc, char *argv[], apkcheck_cli_config_t *config);
void apkcheck_cli_config_init(apkcheck_cli_config_t *config);
void apkcheck_cli_config_cleanup(apkcheck_cli_config_t *config);

int apkcheck_cli_execute(const apkcheck_cli_config_t *config);

void apkcheck_cli_print_help(void);
void apkcheck_cli_print_version(void);
void apkcheck_cli_print_usage(void);

const char *apkcheck_get_sig_alg_name(apkcheck_sig_alg_t alg);
const char *apkcheck_get_digest_alg_name(apkcheck_digest_alg_t alg);

apkcheck_sig_alg_t apkcheck_parse_sig_alg(const char *name);
apkcheck_digest_alg_t apkcheck_parse_digest_alg(const char *name);

#ifdef __cplusplus
}
#endif

#endif
