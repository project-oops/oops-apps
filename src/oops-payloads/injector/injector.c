#include "oops/inject.h"
#include "oops/freestd.h"
#include "oops/syscall.h"
#include "oops/krw.h"
#include "oops/fs.h"
#include "oops/system.h"

extern const uint8_t __payload_start[] __attribute__((weak, visibility("hidden")));
extern const uint8_t __payload_end[] __attribute__((weak, visibility("hidden")));

static void injector_exit(int code) __attribute__((noreturn));
static void injector_exit(int code) {
    sys_call(SYS_exit, (long)code, 0, 0, 0, 0, 0);
    for (;;) {
        __asm__ volatile("pause");
    }
}

static int read_file_from_disk(const char *path, uint8_t *buffer, size_t max_size,
                               size_t *out_size) {
    int fd = oops_fs_open(path, OOPS_O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }

    size_t total = 0;
    while (total < max_size) {
        long n = oops_fs_read(fd, buffer + total, max_size - total);
        if (n <= 0) {
            break;
        }
        total += (size_t)n;
    }
    oops_fs_close(fd);

    if (out_size != NULL) {
        *out_size = total;
    }
    return (total > 0) ? 0 : -1;
}

int injector_start(payload_args_t *args);

int injector_start(payload_args_t *args) {
    if (args == NULL) {
        injector_exit(-1);
    }

    sys_call_init(args);

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

    oops_klog("INJECTOR",
              "starting oops-apps standalone injector (v " OOPS_APP_VERSION ")...");

    if (krw_init(args) != 0) {
        oops_klog("INJECTOR", "ERROR: krw_init failed");
        injector_exit(-2);
    }

    if (krw_elevate_current_process() != 0) {
        oops_klog("INJECTOR", "ERROR: krw_elevate_current_process failed");
        injector_exit(-3);
    }

    pid_t target_pid = target_resolve(NULL);
    if (target_pid <= 0) {
        oops_klog("INJECTOR", "ERROR: no running target game process found");
        krw_restore_current_process();
        injector_exit(-4);
    }
    oops_kprintf("INJECTOR", "resolved target pid: %lld\n", (long long)target_pid);

    const uint8_t *payload_data = NULL;
    size_t payload_size = 0;
    static uint8_t disk_buffer[0x200000]; /* 2 MiB staging buffer */

    if (__payload_start != NULL && __payload_end != NULL &&
        __payload_end > __payload_start) {
        payload_data = __payload_start;
        payload_size = (size_t)(__payload_end - __payload_start);
        oops_kprintf("INJECTOR", "using embedded payload blob, size: %llu\n",
                     (unsigned long long)payload_size);
    } else {
        if (read_file_from_disk("/data/tracer-prospero.elf", disk_buffer,
                                sizeof(disk_buffer), &payload_size) == 0 ||
            read_file_from_disk("/data/tracer.elf", disk_buffer, sizeof(disk_buffer),
                                &payload_size) == 0 ||
            read_file_from_disk("/data/home-launcher-prospero.elf", disk_buffer,
                                sizeof(disk_buffer), &payload_size) == 0 ||
            read_file_from_disk("/data/porthole-prospero.elf", disk_buffer,
                                sizeof(disk_buffer), &payload_size) == 0 ||
            read_file_from_disk("/data/obscene-probe-prospero.elf", disk_buffer,
                                sizeof(disk_buffer), &payload_size) == 0 ||
            read_file_from_disk("/data/porthole.elf", disk_buffer, sizeof(disk_buffer),
                                &payload_size) == 0 ||
            read_file_from_disk("/data/payload.elf", disk_buffer, sizeof(disk_buffer),
                                &payload_size) == 0 ||
            read_file_from_disk("/data/obscene-payload.elf", disk_buffer,
                                sizeof(disk_buffer), &payload_size) == 0) {
            payload_data = disk_buffer;
            oops_kprintf("INJECTOR", "loaded payload from disk, size: %llu\n",
                         (unsigned long long)payload_size);
        }
    }

    if (payload_data == NULL || payload_size == 0) {
        oops_klog("INJECTOR", "ERROR: no payload binary found");
        krw_restore_current_process();
        injector_exit(-5);
    }

    int ret = oops_inject_elf(target_pid, payload_data, payload_size, args);
    krw_restore_current_process();
    injector_exit(ret);
}
