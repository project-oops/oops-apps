/*
 * Mbed TLS's two platform hooks on this target (`oops_config.h` says why each is
 * needed).
 *
 * Entropy: `getentropy` returns at most 256 bytes a call, so a larger request is filled
 * in pieces. A failure is reported as one: Mbed TLS then refuses to seed, which is the
 * right answer to having no randomness, where a partly filled buffer would not be.
 *
 * Milliseconds: the monotonic clock, which is what upstream's POSIX arm reads. That arm
 * is chosen by `_POSIX_VERSION`, which `common/posix` does not define - it would move
 * every port that tests it onto code paths this layer does not implement.
 */
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen);
int64_t mbedtls_ms_time(void);

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    size_t done = 0;

    (void)data;
    *olen = 0;
    while (done < len) {
        const size_t n = len - done < 256 ? len - done : 256;
        if (getentropy(output + done, n) != 0) {
            return -1;
        }
        done += n;
    }
    *olen = done;
    return 0;
}

int64_t mbedtls_ms_time(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return (int64_t)time(NULL) * 1000;
    }
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
