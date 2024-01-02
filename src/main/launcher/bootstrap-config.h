#ifndef LAUNCHER_BOOTSTRAP_CONFIG_H
#define LAUNCHER_BOOTSTRAP_CONFIG_H

#include <stdbool.h>
#include <windows.h>

#include "imports/avs.h"

// should be enough for a while
#define DEFAULT_FILE_MAX 16

struct bootstrap_startup_config {
    struct bootstrap_default_file_config {
        struct bootstrap_default_file {
            char src[64];
            char dst[64];
        } file[DEFAULT_FILE_MAX];
    } default_file;

    struct bootstrap_boot_config {
        char config_file[64];
        uint32_t avs_heap_size;
        uint32_t std_heap_size;
        char launch_config_file[64];
        char mount_table_selector[16];
        bool watcher_enable;
        bool timemachine_enable;
    } boot;

    struct bootstrap_log_config {
        char level[8];
        char name[64];
        char file[64];
        uint32_t bufsz;
        uint16_t output_delay_ms;
        bool enable_console;
        bool enable_sci;
        bool enable_net;
        bool enable_file;
        bool rotate;
        bool append;
        uint16_t count;
    } log;

    struct bootstrap_minidump_config {
        uint8_t count;
        bool continue_;
        bool log;
        uint8_t type;
        char path[64];
        uint32_t symbufsz;
        char search_path[64];
    } minidump;

    struct bootstrap_module_config {
        char file[64];
        char load_type[64];
        struct property* app_config;
    } module;

    struct bootstrap_dlm_config {
        char digest[16];
        uint32_t size;
        uint32_t ift_table;
        uint32_t ift_insert;
        uint32_t ift_remove;
    };
    
    struct bootstrap_dlm_config dlm_ntdll;

    struct bootstrap_shield_config {
        bool enable;
        bool verbose;
        bool use_loadlibrary;
        char logger[64];
        uint32_t sleep_min;
        uint32_t sleep_blur;
        uint32_t tick_sleep;
        uint32_t tick_error;
        uint8_t overwork_threshold;
        uint32_t overwork_delay;
        uint32_t pause_delay;
        char whitelist_file[64];
        char unlimited_key[10];
        uint16_t killer_port;
    } shield;

    struct bootstrap_dongle_config {
        char license_cn[32];
        char account_cn[32];
        char driver_dll[16];
        bool disable_gc;
    } dongle;

    struct bootstrap_drm_config {
        char dll[64];
        char device[64];
        char mount[64];
        char fstype[64];
        char options[64];
    } drm;

    struct bootstrap_lte_config {
        bool enable;
        char config_file[64];
        char unlimited_key[10];
    } lte;

    struct bootstrap_ssl_config {
        char options[64];
    } ssl;
    
    struct bootstrap_esign_config {
        bool enable;
    } esign;

    struct bootstrap_eamuse_config {
        bool enable;
        bool sync;
        bool enable_model;
        char config_file[64];
        bool updatecert_enable;
        uint32_t updatecert_interval;
    } eamuse;
};

struct bootstrap_config {
    char release_code[16];
    struct bootstrap_startup_config startup;
};

void bootstrap_config_init(struct bootstrap_config *config);

void bootstrap_config_load(
    struct property *property,
    const char *profile,
    struct bootstrap_config *config);

#endif /* LAUNCHER_BOOTSTRAP_CONFIG_H */
