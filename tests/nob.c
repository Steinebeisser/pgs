#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define BUILD_FOLDER "build/"

typedef struct {
    const char *name;
    const char **defines;
} Test_Config;

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Test_Config configs[] = {
        {
            .name = "default",
            .defines = (const char *[]) {
                NULL
            }
        },
        {
            .name = "buffering_off",
            .defines = (const char *[]) {
                "PGS_LOG_ENABLE_BUFFERING=0",
                NULL
            }
        },
        {
            .name = "logging_disabled",
            .defines = (const char *[]) {
                "PGS_LOG_ENABLED=0",
                NULL
            }
        },
        {
            .name = "no_file_output",
            .defines = (const char *[]) {
                "PGS_LOG_ENABLE_FILE=0",
                NULL
            }
        },
        {
            .name = "no_insta_write",
            .defines = (const char *[]) {
                "PGS_LOG_BUFFER_INSTA_WRITE_TERMINAL=0",
                NULL
            }
        },
        {
            .name = "strip_prefix",
            .defines = (const char *[]) {
                "PGS_LOG_STRIP_PREFIX",
                NULL
            }
        },
        {
            .name = "append_mode",
            .defines = (const char *[]) {
                "PGS_LOG_APPEND=1",
                "PGS_LOG_OVERRIDE=0",
                NULL
            }
        },
        {
            .name = "override_mode",
            .defines = (const char *[]) {
                "PGS_LOG_APPEND=0",
                "PGS_LOG_OVERRIDE=1",
                NULL
            }
        },
        {
            .name = "rotate_mode",
            .defines = (const char *[]) {
                "PGS_LOG_APPEND=0",
                "PGS_LOG_OVERRIDE=0",
                NULL
            }
        },
    };

    if (!mkdir_if_not_exists(BUILD_FOLDER)) {
        fprintf(stderr, "Failed to create build directory\n");
        return 1;
    }

    const char *test_file = "pgs_log_test.c";
    char *test_name = temp_sprintf("pgs_log_test");

    for (size_t i = 0; i < NOB_ARRAY_LEN(configs); ++i) {
        Test_Config *config = &configs[i];
        Nob_Cmd cmd = {0};

        cmd_append(&cmd, "cc");
        cmd_append(&cmd, "-Wall", "-Wextra", "-I..");
        cmd_append(&cmd, "-o", temp_sprintf("%s%s_%s", BUILD_FOLDER, test_name, config->name));
        for (size_t j = 0; config->defines[j] != NULL; ++j) {
            cmd_append(&cmd, "-D", config->defines[j]);
        }
        cmd_append(&cmd, test_file);

        if (!cmd_run(&cmd)) {
            fprintf(stderr, "Failed to compile %s with config %s\n", test_file, config->name);
            return 1;
        }

        cmd.count = 0;
#ifdef _WIN32
        cmd_append(&cmd, temp_sprintf(".\\%s%s_%s.exe", BUILD_FOLDER, test_name, config->name));
#else
        cmd_append(&cmd, temp_sprintf("./%s%s_%s", BUILD_FOLDER, test_name, config->name));
#endif

        if (!cmd_run(&cmd)) {
            fprintf(stderr, "Tests failed for config %s\n", config->name);
            return 1;
        }
    }

    printf("All tests passed!\n");
    return 0;
}
