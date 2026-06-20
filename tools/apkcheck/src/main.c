#include "apkcheck.h"
#include "cli.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        apkcheck_cli_print_usage();
        return 1;
    }
    
    apkcheck_cli_config_t config;
    apkcheck_cli_config_init(&config);
    
    int result = apkcheck_cli_parse_args(argc, argv, &config);
    
    if (result == APKCHECK_SUCCESS) {
        result = apkcheck_cli_execute(&config);
    }
    
    apkcheck_cli_config_cleanup(&config);
    
    if (result != APKCHECK_SUCCESS && result != APKCHECK_ERROR_INVALID_PARAM) {
        fprintf(stderr, "Error: %s\n", apkcheck_strerror(result));
        return 1;
    }
    
    if (result == APKCHECK_ERROR_INVALID_PARAM && config.command == CMD_NONE) {
        return 1;
    }
    
    return 0;
}
