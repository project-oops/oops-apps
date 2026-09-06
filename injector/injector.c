#include "oops/inject.h"
#include "oops/freestd.h"
#include "oops/syscall.h"
#include "oops/krw.h"

#define O_RDONLY 0

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
    long fd = sys_call(SYS_open, (long)path, O_RDONLY, 0, 0, 0, 0);
    if (fd < 0) {
        return -1;
    }

    size_t total = 0;
    while (total < max_size) {
        long n = sys_call(SYS_read, fd, (long)(buffer + total),
                          (long)(max_size - total), 0, 0, 0);
        if (n <= 0) {
            break;
        }
        total += (size_t)n;
    }
    sys_call(SYS_close, fd, 0, 0, 0, 0, 0);

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

    klog_write("starting oops-apps standalone injector...");

    if (krw_init(args) != 0) {
        klog_write("ERROR: krw_init failed");
        injector_exit(-2);
    }

    if (krw_elevate_current_process() != 0) {
        klog_write("ERROR: krw_elevate_current_process failed");
        injector_exit(-3);
    }

    pid_t target_pid = target_resolve(NULL);
    if (target_pid <= 0) {
        klog_write("ERROR: no running target game process found");
        krw_restore_current_process();
        injector_exit(-4);
    }
    klog_write_num("resolved target pid: ", (int64_t)target_pid);

    const uint8_t *payload_data = NULL;
    size_t payload_size = 0;
    static uint8_t disk_buffer[0x200000]; /* 2 MiB staging buffer */

    if (__payload_start != NULL && __payload_end != NULL && __payload_end > __payload_start) {
        payload_data = __payload_start;
        payload_size = (size_t)(__payload_end - __payload_start);
        klog_write_num("using embedded payload blob, size: ", (int64_t)payload_size);
    } else {
        if (read_file_from_disk("/data/porthole.elf", disk_buffer, sizeof(disk_buffer), &payload_size) == 0 ||
            read_file_from_disk("/data/payload.elf", disk_buffer, sizeof(disk_buffer), &payload_size) == 0 ||
            read_file_from_disk("/data/obscene-payload.elf", disk_buffer, sizeof(disk_buffer), &payload_size) == 0) {
            payload_data = disk_buffer;
            klog_write_num("loaded payload from disk, size: ", (int64_t)payload_size);
        }
    }

    if (payload_data == NULL || payload_size == 0) {
        klog_write("ERROR: no payload binary found");
        krw_restore_current_process();
        injector_exit(-5);
    }

    int ret = oops_inject_elf(target_pid, payload_data, payload_size, args);
    krw_restore_current_process();
    injector_exit(ret);
}
