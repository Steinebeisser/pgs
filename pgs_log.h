#ifndef PGS_LOG_H
#define PGS_LOG_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef _WIN32
#   include <direct.h>
#endif

/*
 * PLACEHOLDER
 * %L = LOG LEVEL
 * %T = TIMESTAMP
 * %F = FILE
 * %l = LINE
 * %M = MESSAGE
*/

/*  
 * TODO:    - log colors
 *          - rotating log files (time & size)
 *          - buffer sizes
 *          - cleanup
 *          - thread safety
 *          - better memory size handling PGS_LOG_MAX_ENTRY_LEN or 512 bytes per path
 *          - error codes instead of bool
 *          - cap number adding
 *          - stuff like NOB_DEPRECATED warning
 *          - minimal log level
*/

#ifndef PGS_LOG_TIMESTAMP_FORMAT
#define PGS_LOG_TIMESTAMP_FORMAT "%d-%m-%Y %H:%M:%S"
#endif
#ifndef PGS_LOG_FORMAT
#define PGS_LOG_FORMAT "[%L] %T %F:%l - \"%M\""
#endif
#ifndef PGS_LOG_MAX_ENTRY_LEN
#define PGS_LOG_MAX_ENTRY_LEN 2048
#endif
#ifndef PGS_LOG_PATH
#define PGS_LOG_PATH "logs/log/%d-%m-%Y.log"
#endif
#ifndef PGS_LOG_APPEND
#define PGS_LOG_APPEND true
#endif
#ifndef PGS_LOG_OVERRIDE
#define PGS_LOG_OVERRIDE false
#endif
#ifndef PGS_LOG_ENABLE_FILE
#define PGS_LOG_ENABLE_FILE true
#endif
#ifndef PGS_LOG_ENABLE_STDOUT
#define PGS_LOG_ENABLE_STDOUT true
#endif
#ifndef PGS_LOG_MAX_FD
#define PGS_LOG_MAX_FD 8
#endif

typedef enum {
    PGS_LOG_DEBUG,
    PGS_LOG_INFO,
    PGS_LOG_WARN,
    PGS_LOG_ERROR,
    PGS_LOG_FATAL,
} Pgs_Log_Level;

typedef struct Pgs_Output {
    FILE *fd;
} Pgs_Output;

extern Pgs_Log_Level pgs_log_minimal_log_level;

bool pgs_log(Pgs_Log_Level level, const char *file, int line, const char *fmt, ...);
const char *pgs_log_level_to_string(Pgs_Log_Level level);
const char *pgs_log_timestamp_string();
bool pgs_log_add_fd_output(FILE *file);
bool pgs_mkdir_if_not_exists(const char *path);
bool pgs_create_dirs_for_path(const char *fullpath);
bool pgs_log_file_exists(const char *file_path);
int pgs_log_get_last_occurence_of(const char ch, const char *string);

#define PGS_LOG_DEBUG(fmt, ...) pgs_log(PGS_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PGS_LOG_INFO(fmt, ...) pgs_log(PGS_LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PGS_LOG_WARN(fmt, ...) pgs_log(PGS_LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PGS_LOG_ERROR(fmt, ...) pgs_log(PGS_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PGS_LOG_FATAL(fmt, ...) pgs_log(PGS_LOG_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // PGS_LOG_H

#ifdef PGS_LOG_IMPLEMENTATION

Pgs_Log_Level pgs_log_minimal_log_level = PGS_LOG_DEBUG;

static Pgs_Output pgs_outputs[PGS_LOG_MAX_FD];
static int pgs_output_count = 0;
static bool pgs_log_initialized = false;

bool pgs_log(Pgs_Log_Level level, const char *file, int line, const char *fmt, ...) {
    if (level < pgs_log_minimal_log_level) {
        return true;
    }
    if (!pgs_log_initialized) {
#if PGS_LOG_ENABLE_STDOUT
    if (!pgs_log_add_fd_output(stdout)) return false;
#endif
#if PGS_LOG_ENABLE_FILE
        size_t filename_size = 1024;
        char filename[filename_size];
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);

        if (strftime(filename, sizeof(filename), PGS_LOG_PATH, tm_info) == 0) {
            fprintf(stderr, "Failed to format log filename\n");
            return false;
        }

        if (!pgs_create_dirs_for_path(filename)) {
            return false;
        }

        FILE *log_file = NULL;

        if (pgs_log_file_exists(filename)) {
#if PGS_LOG_APPEND
            log_file = fopen(filename, "a");
#elif PGS_LOG_OVERRIDE
            log_file = fopen(filename, "w");
#else
            char base[512];
            char ext[128];
            int file_ending_pos = pgs_log_get_last_occurence_of('.', filename);
            if (file_ending_pos == -1) {
                strcpy(base, filename);
                ext[0] = '\0';
            } else {
                strncpy(base, filename, file_ending_pos);
                base[file_ending_pos] = '\0';
                strcpy(ext, filename + file_ending_pos); 
            }

            int number = 0;

            while (pgs_log_file_exists(filename)) {
                number += 1;
                snprintf(filename, filename_size, "%s(%d)%s", base, number, ext);
            }
            log_file = fopen(filename, "w");
#endif
        } else {
             log_file = fopen(filename, "w");
        }

        printf("Log file: %s\n", filename);
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: `%s`:%s\n", filename, strerror(errno));
            return false;
        }

        if (!pgs_log_add_fd_output(log_file)) {
            fprintf(stderr, "Failed to add log file to file descriptors\n");
            return false;
        }
#endif
        pgs_log_initialized = true;
    }
    va_list ap;
    va_start(ap, fmt);

    char msg[PGS_LOG_MAX_ENTRY_LEN];
    int msg_len = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (msg_len < 0) {
        return false;
    }

    const char *format = PGS_LOG_FORMAT;
    char log_string[PGS_LOG_MAX_ENTRY_LEN];
    size_t pos = 0;

    while (*format && pos < PGS_LOG_MAX_ENTRY_LEN - 1) {
        if (*format == '%' && *(format +1)) {
            switch (*(format + 1)) {
                case 'L': pos += snprintf(log_string + pos, sizeof(log_string) - pos, "%s", pgs_log_level_to_string(level)); break;
                case 'T': pos += snprintf(log_string + pos, sizeof(log_string) - pos, "%s", pgs_log_timestamp_string()); break;
                case 'F': pos += snprintf(log_string + pos, sizeof(log_string) - pos, "%s", file); break;
                case 'l': pos += snprintf(log_string + pos, sizeof(log_string) - pos, "%d", line); break;
                case 'M': pos += snprintf(log_string + pos, sizeof(log_string) - pos, "%s", msg); break;
                default: {
                    log_string[pos++] = *format;
                    log_string[pos++] = *(format + 1);
                } break;
            }
            format += 2;
        } else {
            log_string[pos++] = *format++;
        }
    }

    log_string[pos] = '\0';

    for (int i = 0; i < pgs_output_count; ++i) {
        Pgs_Output *o = &pgs_outputs[i];
        fprintf(o->fd, "%s\n", log_string);
    }
    return true;
}

const char *pgs_log_level_to_string(Pgs_Log_Level level) {
    switch (level) {
        case PGS_LOG_DEBUG: return "DEBUG";
        case PGS_LOG_INFO: return "INFO";
        case PGS_LOG_WARN: return "WARN";
        case PGS_LOG_ERROR: return "ERROR";
        case PGS_LOG_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

const char *pgs_log_timestamp_string() {
    static char buffer[64];
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buffer, sizeof(buffer), PGS_LOG_TIMESTAMP_FORMAT, tm_info);
    return buffer;
}

bool pgs_log_add_fd_output(FILE *file) {
    if (pgs_output_count >= PGS_LOG_MAX_FD) {
        fprintf(stderr, "Reached max file descriptor count, you can add `#define PGS_LOG_MAX_FD` and increase the number and recompile\n");
        return false;
    }

    Pgs_Output *o = &pgs_outputs[pgs_output_count++];

    o->fd = file;

    return true;
}

bool pgs_mkdir_if_not_exists(const char *path) {
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    if (result < 0) {
        if (errno == EEXIST) {
            return true;
        }
        fprintf(stderr, "could not create directory `%s`: %s", path, strerror(errno));
        return false;
    }

    return true;
}

bool pgs_create_dirs_for_path(const char *fullpath) {
    size_t len = strlen(fullpath);
    if (len == 0) return false;

    char tmp[512];
    if (len >= sizeof(tmp)) {
        fprintf(stderr, "path too long\n");
        return false;
    }

    strncpy(tmp, fullpath, sizeof(tmp));
    tmp[len] = '\0';

    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char orig = tmp[i];
            tmp[i] = '\0';
            if (!pgs_mkdir_if_not_exists(tmp)) {
                return false;
            }
            tmp[i] = orig;
        }
    }
    return true;
}

bool pgs_log_file_exists(const char *file_path) {
    struct stat buffer;
    return (stat(file_path, &buffer) == 0);
}

int pgs_log_get_last_occurence_of(const char ch, const char *string) {
    int last_pos = -1;
    int pos = 0;
    while (*(string + pos)) {
        if (*(string + pos) == ch) {
            last_pos = pos;
        }
        pos += 1;
    }
    return last_pos;
}

#endif // PGS_LOG_IMPLEMENTATION

#ifndef PGS_LOG_STRIP_PREFIX_GUARD_
#define PGS_LOG_STRIP_PREFIX_GUARD_

    #ifdef PGS_LOG_STRIP_PREFIX

        #define log pgs_log
        #define level_to_string pgs_log_level_to_string
        #define log_timestamp_string pgs_log_timestamp_string
        #define log_add_fd_output pgs_log_add_fd_output
        #define mkdir_if_not_exists pgs_mkdir_if_not_exists
        #define create_dirs_for_path pgs_create_dirs_for_path
        #define log_file_exists pgs_log_file_exists
        #define log_get_last_occurence_of pgs_log_get_last_occurence_of

        #define LOG_DEBUG PGS_LOG_DEBUG
        #define LOG_INFO PGS_LOG_INFO
        #define LOG_WARN PGS_LOG_WARN
        #define LOG_ERROR PGS_LOG_ERROR
        #define LOG_FATAL PGS_LOG_FATAL

        #define minimal_log_level pgs_log_minimal_log_level

    #endif // PGS_LOG_STRIP_PREFIX

#endif // PGS_LOG_STRIP_PREFIX_GUARD_
