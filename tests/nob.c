#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define BUILD_FOLDER "build/"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};

    const char *test_files[] = {
        "pgs_log_test.c"
    };

    if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    for(size_t i = 0; i < NOB_ARRAY_LEN(test_files); ++i) {
        const char *test_path = test_files[i];
        char *test_name = nob_temp_strdup(test_path);

        char *dot = strrchr(test_name, '.');
        if (dot) *dot = '\0';

        cmd_append(&cmd, "cc");
        cmd_append(&cmd, "-Wall");
        cmd_append(&cmd, "-Wextra");
        cmd_append(&cmd, "-I..");
        cmd_append(&cmd, "-o");
        cmd_append(&cmd, temp_sprintf("%s%s", BUILD_FOLDER, test_name));
        cmd_append(&cmd, test_files[i]);

        if (!cmd_run(&cmd)) {
            fprintf(stderr, "Failed to compile %s\n", test_path);
            return 1;
        }

#ifdef _WIN32
        cmd_append(&cmd, temp_sprintf(".\\%s%s%s", BUILD_FOLDER, test_name, ".exe"));
#else
        cmd_append(&cmd, temp_sprintf("./%s%s", BUILD_FOLDER, test_name));
#endif

        if (!cmd_run(&cmd)) {
            fprintf(stderr, "Failed tests for %s\n", test_path);
            return 1;
        }
    }

}
