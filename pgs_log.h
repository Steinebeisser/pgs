/*
      PGS_LOG - simple/fast way to log

    Most things you will use:

        Logging Macros:
            PGS_LOG_DEBUG(fmt, ...)
            PGS_LOG_INFO(fmt, ...)
            PGS_LOG_WARN(fmt, ...)
            PGS_LOG_ERROR(fmt, ...)
            PGS_LOG_FATAL(fmt, ...)

        Set minimum log level:
            pgs_log_minimal_log_level

        Toggle logging on/off at runtime:
            pgs_log_toggle(bool enabled)

        Check last error (v0.2.0+):
            Pgs_Log_Error_Detail err = pgs_log_get_last_error();
            pgs_log_print_error_detail();

    Placeholder formatting (for custom PGS_LOG_FORMAT):
        %L = LOG LEVEL
        %T = TIMESTAMP
        %F = FILE
        %l = LINE
        %M = MESSAGE
*/

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
#include <stdlib.h>

#ifdef _WIN32
#   include <direct.h>
#endif

/*
 * TODO:    - log colors
 *          - rotating log files (time & size)
 *          - buffer sizes
 *          - cleanup
 *          - thread safety
 *          - better memory size handling PGS_LOG_MAX_ENTRY_LEN or 512 bytes per path
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
#define PGS_LOG_PATH "logs/%d-%m-%Y.log"
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
#ifndef PGS_LOG_USE_DETAIL_ERROR
#define PGS_LOG_USE_DETAIL_ERROR true
#endif
#ifndef PGS_LOG_ERROR_MESSAGE_SIZE
#define PGS_LOG_ERROR_MESSAGE_SIZE 1024
#endif
#ifndef PGS_LOG_ENABLED
#define PGS_LOG_ENABLED true
#endif

typedef enum {
    PGS_LOG_DEBUG,
    PGS_LOG_INFO,
    PGS_LOG_WARN,
    PGS_LOG_ERROR,
    PGS_LOG_FATAL,
} Pgs_Log_Level;

typedef enum {
    PGS_LOG_OK = 0,
    PGS_LOG_ERR,
    PGS_LOG_ERR_FILE,
} Pgs_Log_Error;

#if PGS_LOG_USE_DETAIL_ERROR
typedef struct {
    Pgs_Log_Error type;
    char message[PGS_LOG_ERROR_MESSAGE_SIZE];
    int errno_value;
} Pgs_Log_Error_Detail;
#else
typedef struct {
} Pgs_Log_Error_Detail;
#endif

typedef struct {
    FILE *fd;
} Pgs_Log_Output;

extern Pgs_Log_Level pgs_log_minimal_log_level;

Pgs_Log_Error_Detail pgs_log_get_last_error();
Pgs_Log_Error pgs_log_set_last_error(Pgs_Log_Error type, const char *message, int errn);

Pgs_Log_Error pgs_log(Pgs_Log_Level level, const char *file, int line, const char *fmt, ...);

const char *pgs_log_level_to_string(Pgs_Log_Level level);
const char *pgs_log_timestamp_string();

Pgs_Log_Error pgs_log_add_fd_output(FILE *file);
Pgs_Log_Error pgs_log_remove_fd_output(FILE *file);

Pgs_Log_Error pgs_log_mkdir_if_not_exists(const char *path);
Pgs_Log_Error pgs_log_create_dirs_for_path(const char *fullpath);

bool pgs_log_file_exists(const char *file_path);
int pgs_log_get_last_occurence_of(const char ch, const char *string);

void pgs_log_cleanup();

void pgs_log_toggle(bool enabled);

void pgs_log_print_error_detail();

char *pgs_log_temp_sprintf(const char *format, ...);


#define PGS_LOG_DEBUG(fmt, ...) pgs_log(PGS_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PGS_LOG_INFO(fmt, ...) pgs_log(PGS_LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PGS_LOG_WARN(fmt, ...) pgs_log(PGS_LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PGS_LOG_ERROR(fmt, ...) pgs_log(PGS_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PGS_LOG_FATAL(fmt, ...) pgs_log(PGS_LOG_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)


#endif // PGS_LOG_H

#ifdef PGS_LOG_IMPLEMENTATION

Pgs_Log_Level pgs_log_minimal_log_level = PGS_LOG_DEBUG;

static Pgs_Log_Output pgs_outputs[PGS_LOG_MAX_FD];
static int pgs_output_count = 0;
static bool pgs_log_initialized = false;
static bool pgs_log_is_enabled = PGS_LOG_ENABLED;

#if PGS_LOG_USE_DETAIL_ERROR
static Pgs_Log_Error_Detail pgs_log_last_error = { .type = PGS_LOG_OK, .message = {0}, .errno_value = 0, };
#else
static Pgs_Log_Error_Detail pgs_log_last_error = { .type = PGS_LOG_OK };
#endif


Pgs_Log_Error_Detail pgs_log_get_last_error() {
    return pgs_log_last_error;
}

Pgs_Log_Error pgs_log_set_last_error(Pgs_Log_Error type, const char *msg, int errn) {
    pgs_log_last_error.type = type;
#if PGS_LOG_USE_DETAIL_ERROR
    pgs_log_last_error.errno_value = errn;
    if (errn != 0) {
        snprintf(pgs_log_last_error.message, sizeof(pgs_log_last_error.message), "%s: %s", msg, strerror(errn));
    } else {
        strncpy(pgs_log_last_error.message, msg, sizeof(pgs_log_last_error.message) - 1);
        pgs_log_last_error.message[sizeof(pgs_log_last_error.message) - 1] = '\0';
    }
#endif
    return type;
}

Pgs_Log_Error pgs_log(Pgs_Log_Level level, const char *file, int line, const char *fmt, ...) {
    if (!pgs_log_is_enabled)
        return pgs_log_set_last_error(PGS_LOG_OK, "Loggind disabled", 0);

    if (level < pgs_log_minimal_log_level)
        return pgs_log_set_last_error(PGS_LOG_OK, "Below minimal Log Level", 0);

    if (!pgs_log_initialized) {
#if PGS_LOG_ENABLE_STDOUT
        if (pgs_log_add_fd_output(stdout) != PGS_LOG_OK) 
            return pgs_log_set_last_error(PGS_LOG_ERR_FILE, "Failed to add STDOUT as output", 0);
#endif
#if PGS_LOG_ENABLE_FILE
        size_t filename_size = 1024;
        char filename[filename_size];
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);

        if (strftime(filename, sizeof(filename), PGS_LOG_PATH, tm_info) == 0) {
            return pgs_log_set_last_error(PGS_LOG_ERR_FILE, "Failed to format log filename", 0);
        }

        if (pgs_log_create_dirs_for_path(filename) != PGS_LOG_OK) {
            return pgs_log_set_last_error(PGS_LOG_ERR_FILE, pgs_log_temp_sprintf("Failed to create dirs for path %s", filename), 0);
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
            return pgs_log_set_last_error(PGS_LOG_ERR_FILE, "Failed to open log file", errno);
        }

        if (pgs_log_add_fd_output(log_file) != PGS_LOG_OK) {
            fclose(log_file);
            return pgs_log_set_last_error(PGS_LOG_ERR, "Failed to add log file to file descriptors", 0);
        }
#endif
        pgs_log_initialized = true;
        atexit(pgs_log_cleanup);
    }
    va_list ap;
    va_start(ap, fmt);

    char msg[PGS_LOG_MAX_ENTRY_LEN];
    int msg_len = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (msg_len < 0) {
        return pgs_log_set_last_error(PGS_LOG_ERR, "vsnprintf failed", 0);
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
        Pgs_Log_Output *o = &pgs_outputs[i];
        fprintf(o->fd, "%s\n", log_string);
    }

    return pgs_log_set_last_error(PGS_LOG_OK, "Log entry written", 0);
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

Pgs_Log_Error pgs_log_add_fd_output(FILE *file) {
    if (!file)
        return pgs_log_set_last_error(PGS_LOG_ERR_FILE, "No File passed to add to output", 0);

    if (pgs_output_count >= PGS_LOG_MAX_FD) {
        return pgs_log_set_last_error(PGS_LOG_ERR_FILE, "Reached max file descriptor count, you can add `#define PGS_LOG_MAX_FD` and increase the number and recompile", 0);
    }

    Pgs_Log_Output *o = &pgs_outputs[pgs_output_count++];

    o->fd = file;

    return pgs_log_set_last_error(PGS_LOG_OK, "Added fd to output", 0);
}


Pgs_Log_Error pgs_log_remove_fd_output(FILE *file) {
    if (!file) 
        return pgs_log_set_last_error(PGS_LOG_ERR_FILE, "No file passed", 0);

    int fd_index = -1;

    for (int i = 0; i < pgs_output_count; ++i) {
        Pgs_Log_Output *out = &pgs_outputs[i];
        if (out->fd == file) {
            fd_index = i;
            break;
        };
    }

    if (fd_index == -1) 
        return pgs_log_set_last_error(PGS_LOG_ERR_FILE, "file does not exist in output", 0);

    Pgs_Log_Output *out = &pgs_outputs[fd_index];

    if (out->fd != stdout && out->fd != stderr)
        fclose(out->fd);

    pgs_outputs[fd_index] = pgs_outputs[--pgs_output_count];

    return pgs_log_set_last_error(PGS_LOG_OK, "Removed File from output", 0);
}

Pgs_Log_Error pgs_log_mkdir_if_not_exists(const char *path) {
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    if (result < 0) {
        if (errno == EEXIST) {
            return pgs_log_set_last_error(PGS_LOG_OK, "Folder does already exist", 0);
        }
        return pgs_log_set_last_error(PGS_LOG_ERR, pgs_log_temp_sprintf("could not create directory `%s`", path), errno);
    }

    return pgs_log_set_last_error(PGS_LOG_OK, "Created Directory", 0);
}

Pgs_Log_Error pgs_log_create_dirs_for_path(const char *fullpath) {
    if (!fullpath)
        return pgs_log_set_last_error(PGS_LOG_ERR, "no path passed", 0);
    size_t len = strlen(fullpath);
    if (len == 0) 
        return pgs_log_set_last_error(PGS_LOG_ERR, "Path doesnt have a length", 0);

    char tmp[512];
    if (len >= sizeof(tmp)) {
        return pgs_log_set_last_error(PGS_LOG_ERR, "Path too long", 0);
    }

    strncpy(tmp, fullpath, sizeof(tmp));
    tmp[len] = '\0';

    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char orig = tmp[i];
            tmp[i] = '\0';
            if (pgs_log_mkdir_if_not_exists(tmp) != PGS_LOG_OK) {
                return pgs_log_set_last_error(PGS_LOG_ERR, "Failed to create directories", 0);
            }
            tmp[i] = orig;
        }
    }
    return pgs_log_set_last_error(PGS_LOG_OK, pgs_log_temp_sprintf("Created dirs for path %s", fullpath), 0);
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

void pgs_log_cleanup() {
    for (int i = 0; i < pgs_output_count; ++i) {
        Pgs_Log_Output *out = &pgs_outputs[i];
        if (!out->fd) continue;
        if (out->fd == stdout || out->fd == stderr) continue;

        fclose(out->fd);
        out->fd = NULL;
    }
    pgs_output_count = 0;
    pgs_log_initialized = false;
}

void pgs_log_toggle(bool enabled) {
    pgs_log_is_enabled = enabled;
}

void pgs_log_print_error_detail() {
#if PGS_LOG_USE_DETAIL_ERROR
    const char *type_str = "UNKNOWN";
    switch (pgs_log_last_error.type) {
        case PGS_LOG_OK:       type_str = "OK"; break;
        case PGS_LOG_ERR:      type_str = "ERROR"; break;
        case PGS_LOG_ERR_FILE: type_str = "FILE ERROR"; break;
    }

    fprintf(stderr, "==================== PGS LOG ERROR ====================\n");
    fprintf(stderr, "Type      : %s\n", type_str);
    fprintf(stderr, "Errno     : %d ", pgs_log_last_error.errno_value);
    if (pgs_log_last_error.errno_value != 0) {
        fprintf(stderr, " (%s)\n", strerror(pgs_log_last_error.errno_value));
    } else {
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "Message   : %s\n", pgs_log_last_error.message[0] ? pgs_log_last_error.message : "(none)");
    fprintf(stderr, "=======================================================\n");
#else
    fprintf(stderr, "PGS_LOG: detailed error reporting is disabled at compile time.\n");
#endif
}

char *pgs_log_temp_sprintf(const char *format, ...) {
    static char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return buffer;
}

#endif // PGS_LOG_IMPLEMENTATION

#ifndef PGS_LOG_STRIP_PREFIX_GUARD_
#define PGS_LOG_STRIP_PREFIX_GUARD_

    #ifdef PGS_LOG_STRIP_PREFIX

        #define log pgs_log
        #define level_to_string pgs_log_level_to_string
        #define timestamp_string pgs_log_timestamp_string
        #define add_fd_output pgs_log_add_fd_output
        #define remove_fd_output pgs_log_remove_fd_output
        #define mkdir_if_not_exists pgs_log_mkdir_if_not_exists
        #define create_dirs_for_path pgs_log_create_dirs_for_path
        #define file_exists pgs_log_file_exists
        #define get_last_occurence_of pgs_log_get_last_occurence_of
        #define log_toggle pgs_log_toggle
        #define print_error_detail pgs_log_print_error_detail
        #define temp_sprintf pgs_log_temp_sprintf
        #define get_last_error pgs_log_get_last_error
        #define set_last_error pgs_log_set_last_error
        #define cleanup pgs_log_cleanup


        #define LOG_DEBUG PGS_LOG_DEBUG
        #define LOG_INFO PGS_LOG_INFO
        #define LOG_WARN PGS_LOG_WARN
        #define LOG_ERROR PGS_LOG_ERROR
        #define LOG_FATAL PGS_LOG_FATAL

        #define Log_Level Pgs_Log_Level
        #define Log_Error Pgs_Log_Error
        #define Log_Error_Detail Pgs_Log_Error_Detail
        #define Log_Output Pgs_Log_Output

        #define minimal_log_level pgs_log_minimal_log_level

    #endif // PGS_LOG_STRIP_PREFIX

#endif // PGS_LOG_STRIP_PREFIX_GUARD_

/* 
    Revision History:

        0.2.0 (2025-09-17) Extended Error Handling
                            - Added Revision History and introduction text at top
                            - Add Pgs_Log_Error enum for error codes
                            - Add Pgs_Log_Error_Detail for better reports
                            - add toggle for enabling logging
                            - improve error messages/handling
                            - change return typesof most functions from bool to Pgs_Log_Erro
                            - change to dual license (add public domain)
                            - fix naming inconsistencies
                            Breaking: bool style checks not possible anymore, if caring about about return type must check again PGS_LOG_OK

        0.1.0 (2025-09-14) Initial Release
                            - Basic Logging to stdout/stderr/file
                            - Auto Log file creation
                            - custom log formats
                            - custom log file formats
                            - Log Levels
                            - setting min log level
*/


/*
   ------------------------------------------------------------------------------
   This software is available under 2 licenses -- choose whichever you prefer.
   ------------------------------------------------------------------------------
   ALTERNATIVE A - MIT License
   Copyright (c) 2025 Paul Geisthardt
   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   ------------------------------------------------------------------------------
   ALTERNATIVE B - Public Domain (www.unlicense.org)
   This is free and unencumbered software released into the public domain.
   Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
   software, either in source code form or as a compiled binary, for any purpose,
   commercial or non-commercial, and by any means.
   In jurisdictions that recognize copyright laws, the author or authors of this
   software dedicate any and all copyright interest in the software to the public
   domain. We make this dedication for the benefit of the public at large and to
   the detriment of our heirs and successors. We intend this dedication to be an
   overt act of relinquishment in perpetuity of all present and future rights to
   this software under copyright law.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
   ------------------------------------------------------------------------------
*/
