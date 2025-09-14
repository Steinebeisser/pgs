#define PGS_LOG_STRIP_PREFIX
#define PGS_LOG_IMPLEMENTATION
#include "pgs_log.h"

int main() {
    if (!pgs_log_add_fd_output(stderr)) return 1;

    // TODO: add tests
    return 0;
}
