#define PGS_LOG_IMPLEMENTATION
#include "pgs_log.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        pgs_log_print_error_detail(); \
        return 1; \
    } \
} while (0)

static int test_level_filtering() {
    pgs_log_minimal_log_level = PGS_LOG_WARN;
    ASSERT(PGS_LOG_DEBUG("Should be filtered") == PGS_LOG_OK, "Filtering debug failed (return)");
#if PGS_LOG_ENABLED
    ASSERT(strcmp(pgs_log_get_last_error().message, "Below minimal Log Level") == 0, "Wrong message for filtered debug");
#endif
    ASSERT(PGS_LOG_INFO("Should be filtered") == PGS_LOG_OK, "Filtering info failed (return)");
#if PGS_LOG_ENABLED
    ASSERT(strcmp(pgs_log_get_last_error().message, "Below minimal Log Level") == 0, "Wrong message for filtered info");
#endif
    ASSERT(PGS_LOG_WARN("Warn visible") == PGS_LOG_OK, "Warn not logged");
#if PGS_LOG_ENABLED
    ASSERT(strcmp(pgs_log_get_last_error().message, "Log entry written") == 0, "Warn message not success");
#endif
    pgs_log_minimal_log_level = PGS_LOG_DEBUG;
    return 0;
}

static int test_toggle_disable() {
#if !PGS_LOG_ENABLED
    ASSERT(PGS_LOG_INFO("noop") == PGS_LOG_OK, "Call should still return OK when disabled");
    return 0;
#else
    pgs_log_toggle(false);
    ASSERT(PGS_LOG_INFO("Should not log") == PGS_LOG_OK, "Disable logging return not OK");
    ASSERT(strcmp(pgs_log_get_last_error().message, "Logging disabled") == 0, "Disable message mismatch");
    pgs_log_toggle(true);
    ASSERT(PGS_LOG_INFO("Should log again") == PGS_LOG_OK, "Re-enable logging failed");
    ASSERT(strcmp(pgs_log_get_last_error().message, "Log entry written") == 0, "Enable message mismatch");
    return 0;
#endif
}

static int test_level_strings() {
    ASSERT(strcmp(pgs_log_level_to_string(PGS_LOG_DEBUG), "DEBUG") == 0, "DEBUG str");
    ASSERT(strcmp(pgs_log_level_to_string(PGS_LOG_FATAL), "FATAL") == 0, "FATAL str");
    return 0;
}

static int test_temp_sprintf_ring() {
#if PGS_LOG_TEMP_BUFFERS < 2
    return 0;
#endif
    char *a = pgs_log_temp_sprintf("%s", "one");
    char *b = pgs_log_temp_sprintf("%s", "two");
    ASSERT(strcmp(a, "one") == 0 || strcmp(b, "two") == 0, "Temp buffers basic");
    (void)a; (void)b;
    return 0;
}

#if PGS_LOG_ENABLE_FILE

static FILE *find_log_file(char *fname, int check_incremented) {
    FILE *rf = fopen(fname, "rb");
    if (rf || !check_incremented) {
        return rf;
    }

    char incremented[PGS_LOG_MAX_PATH_LEN];
    char base[PGS_LOG_MAX_PATH_LEN];
    strncpy(base, fname, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    const char *ext = "";
    char *dot = strrchr(base, '.');
    if (dot) {
        *dot = '\0';
        ext = dot + 1;
    }

    for (int n = 1; n < 6 && !rf; ++n) {
        char suffix[64];
        if (*ext) {
            snprintf(suffix, sizeof(suffix), "(%d).%s", n, ext);
        } else {
            snprintf(suffix, sizeof(suffix), "(%d)", n);
        }

        strncpy(incremented, base, sizeof(incremented) - 1);
        incremented[sizeof(incremented) - 1] = '\0';
        size_t len = strlen(incremented);
        strncat(incremented, suffix, sizeof(incremented) - len - 1);

        rf = fopen(incremented, "rb");
        if (rf) {
            strncpy(fname, incremented, PGS_LOG_MAX_PATH_LEN - 1);
            fname[PGS_LOG_MAX_PATH_LEN - 1] = '\0';
        }
    }
    return rf;
}

static int test_file_creation_and_flush() {
    ASSERT(PGS_LOG_INFO("File creation test") == PGS_LOG_OK, "Initial file log failed");
    ASSERT(pgs_log_flush() == PGS_LOG_OK, "Flush failed");
    return 0;
}

static int test_file_modes() {
#if !PGS_LOG_ENABLED
    return 0;
#endif
    time_t t = time(NULL);
    struct tm *ti = localtime(&t);
    char fname[PGS_LOG_MAX_PATH_LEN];
    strftime(fname, sizeof(fname), PGS_LOG_PATH, ti);

    remove(fname);

    ASSERT(PGS_LOG_INFO("first entry") == PGS_LOG_OK, "First entry failed");
    ASSERT(pgs_log_flush() == PGS_LOG_OK, "Flush after first entry failed");

    long size_after_first = 0;
    FILE *rf = find_log_file(fname,
#if !PGS_LOG_APPEND && !PGS_LOG_OVERRIDE
                             1
#else
                             0
#endif
                             );
    ASSERT(rf, "File not created");
    fseek(rf, 0, SEEK_END);
    size_after_first = ftell(rf);
    fclose(rf);
    ASSERT(size_after_first > 0, "File empty after first write");

#if PGS_LOG_APPEND && !PGS_LOG_OVERRIDE
    ASSERT(PGS_LOG_INFO("second entry") == PGS_LOG_OK, "Second append entry failed");
    ASSERT(pgs_log_flush() == PGS_LOG_OK, "Flush after second append failed");
    long size_after_second = 0;
    rf = fopen(fname, "rb");
    ASSERT(rf, "File vanished");
    fseek(rf, 0, SEEK_END);
    size_after_second = ftell(rf);
    fclose(rf);
    ASSERT(size_after_second > size_after_first, "Append mode did not increase size");
#elif !PGS_LOG_APPEND && PGS_LOG_OVERRIDE
    ASSERT(PGS_LOG_INFO("second entry override") == PGS_LOG_OK, "Second override write failed");
    ASSERT(pgs_log_flush() == PGS_LOG_OK, "Flush override second failed");
    long size_after_second = 0;
    rf = fopen(fname, "rb");
    ASSERT(rf, "File missing override");
    fseek(rf, 0, SEEK_END);
    size_after_second = ftell(rf);
    fclose(rf);
    ASSERT(size_after_second >= size_after_first, "Override size logic unexpected (should not shrink under initial due to new content)");
#elif !PGS_LOG_APPEND && !PGS_LOG_OVERRIDE
    pgs_log_cleanup();
    ASSERT(PGS_LOG_INFO("post cleanup entry") == PGS_LOG_OK, "Post cleanup log failed");
    ASSERT(pgs_log_flush() == PGS_LOG_OK, "Flush after rotation attempt failed");
    rf = find_log_file(fname, 1);
    ASSERT(rf, "Original or incremented file missing");
    fclose(rf);
#endif
    return 0;
}
#endif

static int test_add_remove_stdout_duplicate_protection() {
#if !PGS_LOG_ENABLED
    ASSERT(PGS_LOG_DEBUG("noop") == PGS_LOG_OK, "Disabled build basic call");
    return 0;
#else
    ASSERT(PGS_LOG_DEBUG("Init system") == PGS_LOG_OK, "Init log failed");
    ASSERT(pgs_log_add_fd_output(stdout) == PGS_LOG_OK, "Adding stdout explicitly failed");
    ASSERT(pgs_log_remove_fd_output(stdout) == PGS_LOG_OK, "Removing stdout failed");
    ASSERT(pgs_log_add_fd_output(stdout) == PGS_LOG_OK, "Re-adding stdout failed");
    return 0;
#endif
}

static int test_buffering_behavior() {
#if PGS_LOG_ENABLE_BUFFERING
#if !PGS_LOG_ENABLED
    return 0;
#endif
    FILE *f = fopen("buffer_test.log", "w+");
    ASSERT(f != NULL, "Failed to open buffer test file");
    ASSERT(pgs_log_add_fd_output(f) == PGS_LOG_OK, "Add buffer file output failed");
    for (int i = 0; i < 50; ++i) {
        ASSERT(PGS_LOG_INFO("Buffered line %d", i) == PGS_LOG_OK, "Buffered write failed");
    }
    ASSERT(pgs_log_flush() == PGS_LOG_OK, "Flush after buffered writes failed");
    ASSERT(pgs_log_remove_fd_output(f) == PGS_LOG_OK, "Remove buffer file failed");
    fclose(f);
#endif
    return 0;
}

static int test_error_detail() {
#if !PGS_LOG_ENABLED
    return 0;
#endif
    Pgs_Log_Error err = pgs_log_remove_fd_output(NULL);
    ASSERT(err != PGS_LOG_OK, "Removing NULL should error");
    Pgs_Log_Error_Detail det = pgs_log_get_last_error();
    ASSERT(det.type != PGS_LOG_OK, "Detail type should not be OK after error");
    return 0;
}

int main() {
    if (test_level_filtering()) return 1;
    if (test_toggle_disable()) return 1;
    if (test_level_strings()) return 1;
    if (test_temp_sprintf_ring()) return 1;
    if (test_add_remove_stdout_duplicate_protection()) return 1;
    if (test_buffering_behavior()) return 1;
    if (test_error_detail()) return 1;
#if PGS_LOG_ENABLE_FILE
    if (test_file_creation_and_flush()) return 1;
    if (test_file_modes()) return 1;
#endif
    printf("All tests passed for this configuration\n");
    return 0;
}
