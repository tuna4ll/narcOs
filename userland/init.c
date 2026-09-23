#include <narcos/narc.h>

static int write_bytes(const void *data, size_t length) {
    const uint8_t *bytes = data;
    while (length) {
        narc_result_t result = narc_write(1, bytes, length);
        if (result.status != NARC_OK || result.value <= 0) return -1;
        bytes += (size_t)result.value;
        length -= (size_t)result.value;
    }
    return 0;
}

int narc_main(void) {
    static const char banner[] = "narcOs init\nlibnarc: native ABI ready\n";
    if (write_bytes(banner, sizeof(banner) - 1) != 0) return 1;
    narc_result_t abi = narc_abi_query();
    narc_result_t pid = narc_getpid();
    if (abi.status != NARC_OK || abi.value != (int64_t)NARC_ABI_VERSION ||
        pid.status != NARC_OK || pid.value <= 0)
        return 2;

    static const char motd_path[] = "/etc/motd";
    narc_result_t file = narc_open(motd_path, sizeof(motd_path) - 1, NARC_OPEN_READ);
    if (file.status != NARC_OK) return 3;

    uint8_t buffer[64];
    narc_result_t read = narc_read((int)file.value, buffer, sizeof(buffer));
    if (read.status != NARC_OK) return 4;
    static const char motd[] = "motd: ";
    if (write_bytes(motd, sizeof(motd) - 1) != 0 ||
        write_bytes(buffer, (size_t)read.value) != 0)
        return 5;

    narc_result_t seek = narc_seek((int)file.value, 0, NARC_SEEK_BEGIN);
    narc_result_t close = narc_close((int)file.value);
    if (seek.status != NARC_OK || seek.value != 0 || close.status != NARC_OK) return 6;

    static const char missing_path[] = "/missing";
    narc_result_t missing = narc_open(missing_path, sizeof(missing_path) - 1, NARC_OPEN_READ);
    if (missing.status != NARC_NOT_FOUND) return 7;
    if (narc_yield().status != NARC_OK) return 8;
    return 0;
}
