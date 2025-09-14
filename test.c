#define PGS_LOG_IMPLEMENTATION
#define PGS_LOG_STRIP_PREFIX
#define PGS_LOG_TIMESTAMP_FORMAT "%H:%M:%S"
#define PGS_LOG_FORMAT "%M - %T [%L]"
#include "pgs_log.h"

int main() {
    if (!pgs_log_add_fd_output(stderr)) return 1;
    minimal_log_level = PGS_LOG_WARN;
    LOG_INFO("test");
    LOG_WARN("test");
    return 0;
}
