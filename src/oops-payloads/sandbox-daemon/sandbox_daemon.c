/*
 * sandbox-daemon — decoupled on-demand filesystem namespace elevation service.
 *
 * Runs as a background ELF under pldmgr / elfldr (category daemon, root privilege).
 * Receives kernel R/W primitives via payload_args_t from the session loader.
 *
 * Architecture:
 *   1. Dynamic allproc discovery — scans [kdata+0x2600000, kdata+0x2B00000] for a
 *      valid proc chain containing the daemon's own PID, then sets krw_allproc_addr().
 *   2. Resolves rootvnode from PID 1 (mini-syscore) fd_rdir.
 *   3. Listens on TCP 127.0.0.1:9069 for client handshake requests.
 *   4. On each client connection: reads 4-byte PID, walks allproc to locate kproc,
 *      repoints fd_rdir/fd_jdir to rootvnode, elevates credentials, sends status.
 *
 * This is the server-side half of the decoupled namespace service (D004).
 * Client apps connect to 127.0.0.1:9069 and send their PID; the daemon handles
 * the kernel-side credential and vnode manipulation.
 *
 * Provenance references:
 *   - LightningMods/etaHEN: trigger-file mechanism (replaced by loopback IPC)
 *   - ArkSama/PS5-Lapy-JB-Daemon: minimal polling loop pattern (replaced by IPC)
 *   - pltauth-patch: dynamic allproc discovery via kdata memory scanning
 *   - FreeBSD kernel: struct proc / struct ucred / struct filedesc layout
 *
 * See oops-sdk/docs/decisions/D004 for the full decision log.
 */

#include "oops/krw.h"
#include "oops/syscall.h"
#include "oops/freestd.h"
#include "oops/offsets.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define SANDBOX_LISTEN_BACKLOG 4
#define SANDBOX_AUTHID 0x4801000000000013ULL
#define SANDBOX_TCP_PORT 9069

/* Socket address constants (AF_INET) */
#define SANDBOX_AF_INET 2
#define SANDBOX_INADDR_LOOPBACK 0x0100007FUL /* 127.0.0.1 in little-endian */

/* ------------------------------------------------------------------ */
/* Sleep helper (self-contained, uses SYS_nanosleep)                  */
/* ------------------------------------------------------------------ */

static void sandbox_sleep_us(uint32_t us) {
    struct {
        long tv_sec;
        long tv_nsec;
    } req;
    req.tv_sec = (long)(us / 1000000u);
    req.tv_nsec = (long)((us % 1000000u) * 1000u);
    (void)sys_call(240 /* SYS_nanosleep */, (long)&req, 0, 0, 0, 0, 0);
}

/* ------------------------------------------------------------------ */
/* Dual-stream telemetry                                              */
/* ------------------------------------------------------------------ */

void sandbox_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void sandbox_log_hex(const char *prefix, uintptr_t val);

/**
 * Write a formatted diagnostic message to both klog and stdout/stderr.
 * Uses direct sys_call(SYS_klog) and sys_call(SYS_write) without pulling
 * in unresolved external library dependencies.
 */
void sandbox_log(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    (void)oops_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* klog stream */
    char kbuf[300];
    int klen = oops_snprintf(kbuf, sizeof(kbuf), "[SANDBOX] %s\n", buf);
    if (klen > 0) {
        sys_call(SYS_klog, 7, (long)kbuf, 0, 0, 0, 0);
    }

    /* stdout / stderr stream (elfldr maps socket to fd 1 & 2) */
    char outbuf[300];
    int outlen = oops_snprintf(outbuf, sizeof(outbuf), "SANDBOX: %s\n", buf);
    if (outlen > 0) {
        sys_call(SYS_write, 1, (long)outbuf, (long)outlen, 0, 0, 0);
        sys_call(SYS_write, 2, (long)outbuf, (long)outlen, 0, 0, 0);
    }
}

/**
 * Log a hex value with a prefix to both streams.
 */
void sandbox_log_hex(const char *prefix, uintptr_t val) {
    char buf[128];
    oops_snprintf(buf, sizeof(buf), "%s: 0x%lx", prefix, (unsigned long)val);
    sandbox_log("%s", buf);
}

/* ------------------------------------------------------------------ */
/* Dynamic allproc discovery                                          */
/* ------------------------------------------------------------------ */

/**
 * Resolve the allproc head by scanning kernel data memory.
 *
 * Scans [kdata+0x2600000, kdata+0x2B00000] for a pointer that starts
 * a valid proc chain containing the calling daemon's own PID.
 * Sets krw_set_allproc_addr() once located.
 *
 * Mirrors the resolve_allproc() pattern from pltauth-patch.
 */
static uintptr_t sandbox_resolve_allproc(void) {
    uintptr_t kdata = krw_kdata_base();
    if (kdata == 0) {
        return krw_allproc_addr();
    }

    pid_t mypid = (pid_t)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0);
    uintptr_t start = kdata + 0x2600000;
    uintptr_t end = kdata + 0x2B00000;
    uintptr_t found = 0;

    for (uintptr_t addr = start; addr < end; addr += 8) {
        uintptr_t p = krw_read64(addr);
        /* Check for kernel-space pointer (x86_64 top half) */
        if ((p >> 40) != 0xffffcd && (p >> 40) != 0xffffff) {
            continue;
        }

        int chain_len = 0;
        int has_mypid = 0;
        uintptr_t curr = p;
        for (int i = 0; i < 200; i++) {
            if (curr == 0 || (curr >> 40) != 0xffffcd) {
                break;
            }
            pid_t pid = (pid_t)krw_read32(curr + 0xBC);
            if (pid < 0 || pid > 65535) {
                break;
            }
            if (pid > 54) {
                chain_len++;
            }
            if (mypid > 0 && pid == mypid) {
                has_mypid = 1;
            }
            uintptr_t next = krw_read64(curr);
            if (next == 0 || next == curr) {
                break;
            }
            curr = next;
        }

        if (has_mypid) {
            found = addr;
            break;
        }

        /* If we haven't found our PID yet, keep the longest valid chain */
        if (chain_len > 5) {
            if (found == 0 || chain_len > 10) {
                found = addr;
            }
        }
    }

    if (found != 0) {
        krw_set_allproc_addr(found);
        return found;
    }

    /* Fallback to static offset */
    return krw_allproc_addr();
}

/**
 * Locate a proc struct for the given PID on the allproc chain.
 * Returns kernel address of the proc, or 0 if not found.
 */
static uintptr_t sandbox_find_proc_by_pid(pid_t target_pid) {
    uintptr_t allproc = krw_allproc_addr();
    if (allproc == 0) {
        return 0;
    }

    uintptr_t proc = krw_read64(allproc);
    if (proc == 0 || proc == allproc) {
        return 0;
    }

    for (int i = 0; i < 4096; i++) {
        pid_t p_pid = (pid_t)krw_read32(proc + 0xBC);
        if (p_pid == target_pid) {
            return proc;
        }
        uintptr_t next = krw_read64(proc);
        if (next == 0 || next == allproc || next == proc) {
            break;
        }
        proc = next;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Rootvnode resolution                                               */
/* ------------------------------------------------------------------ */

/**
 * Resolve the kernel rootvnode dynamically.
 * Reads fd_rdir from proc(1) (mini-syscore, permanently rooted at system /).
 * Falls back to the static rootvnode symbol if that fails.
 */
static uintptr_t sandbox_resolve_rootvnode(void) {
    /* 1. Check PID 1 (mini-syscore) */
    uintptr_t proc1 = sandbox_find_proc_by_pid(1);
    if (proc1 != 0) {
        uintptr_t fd1 = 0;
        if (krw_copyout(proc1 + 0x48, &fd1, sizeof(fd1)) == 0 && fd1 != 0) {
            uintptr_t cdir1 = 0, rdir1 = 0, jdir1 = 0;
            krw_copyout(fd1 + 0x08, &cdir1, sizeof(cdir1));
            krw_copyout(fd1 + 0x10, &rdir1, sizeof(rdir1));
            krw_copyout(fd1 + 0x18, &jdir1, sizeof(jdir1));
            sandbox_log("PID 1 fd=0x%lx: cdir=0x%lx, rdir=0x%lx, jdir=0x%lx",
                        (unsigned long)fd1, (unsigned long)cdir1, (unsigned long)rdir1,
                        (unsigned long)jdir1);
            if (rdir1 != 0 && (rdir1 >> 40) != 0)
                return rdir1;
            if (cdir1 != 0 && (cdir1 >> 40) != 0)
                return cdir1;
        }
    }

    /* 2. Check self (daemon) */
    pid_t mypid = (pid_t)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0);
    uintptr_t proc_self = sandbox_find_proc_by_pid(mypid);
    if (proc_self != 0) {
        uintptr_t fd_self = 0;
        if (krw_copyout(proc_self + 0x48, &fd_self, sizeof(fd_self)) == 0 &&
            fd_self != 0) {
            uintptr_t cdir_self = 0, rdir_self = 0, jdir_self = 0;
            krw_copyout(fd_self + 0x08, &cdir_self, sizeof(cdir_self));
            krw_copyout(fd_self + 0x10, &rdir_self, sizeof(rdir_self));
            krw_copyout(fd_self + 0x18, &jdir_self, sizeof(jdir_self));
            sandbox_log("self PID %d fd=0x%lx: cdir=0x%lx, rdir=0x%lx, jdir=0x%lx",
                        (int)mypid, (unsigned long)fd_self, (unsigned long)cdir_self,
                        (unsigned long)rdir_self, (unsigned long)jdir_self);
            if (rdir_self != 0 && (rdir_self >> 40) != 0)
                return rdir_self;
            if (cdir_self != 0 && (cdir_self >> 40) != 0)
                return cdir_self;
        }
    }

    /* 3. Fallback to krw_get_root_vnode() */
    return krw_get_root_vnode();
}

/* ------------------------------------------------------------------ */
/* Credential elevation                                               */
/* ------------------------------------------------------------------ */

/**
 * Apply full namespace elevation and credential escalation to a target proc.
 *
 * 1. Repoint fd_rdir (0x10), fd_jdir (0x18), and fd_cdir (0x08) to rootvnode.
 * 2. Elevate credentials:
 *    - cr_uid = 0, cr_ruid = 0 (root)
 *    - cr_sceauthid = SANDBOX_AUTHID
 *    - cr_scecaps = 0xFFFFFFFFFFFFFFFFULL (all caps)
 *    - cr_prison = borrow from PID 1 (prison0)
 * 3. Emit klog entry.
 *
 * Returns 0 on success, -1 on failure.
 */
static int sandbox_elevate_process(pid_t target_pid, uintptr_t rootvnode) {
    /* Step 1: Find the target proc */
    uintptr_t kproc = sandbox_find_proc_by_pid(target_pid);
    if (kproc == 0) {
        sandbox_log("proc not found for PID %d", (int)target_pid);
        return -1;
    }

    sandbox_log("found proc for PID %d at 0x%lx", (int)target_pid,
                (unsigned long)kproc);

    /* Step 2: Repoint fd_rdir, fd_jdir, and fd_cdir to rootvnode */
    if (rootvnode != 0) {
        int rc_rdir = krw_set_proc_rootdir(target_pid, rootvnode);
        int rc_jdir = krw_set_proc_jaildir(target_pid, rootvnode);
        int rc_cdir = krw_set_proc_cdir(target_pid, rootvnode);
        if (rc_rdir != 0 || rc_jdir != 0) {
            sandbox_log("failed to repoint dirs (rdir=%d, jdir=%d, cdir=%d) for PID %d",
                        rc_rdir, rc_jdir, rc_cdir, (int)target_pid);
            /* Continue anyway - credential elevation may still help */
        } else {
            sandbox_log("repointed fd_rdir, fd_jdir, and fd_cdir to rootvnode (0x%lx) "
                        "for PID %d",
                        (unsigned long)rootvnode, (int)target_pid);
        }
    } else {
        sandbox_log("rootvnode unavailable for PID %d", (int)target_pid);
    }

    /* Step 3: Elevate credentials */
    uintptr_t ucred = krw_get_ucred(target_pid);
    if (ucred != 0) {
        uint32_t zeros[2] = {0, 0};
        (void)krw_copyin(zeros, ucred + 0x04, sizeof(zeros)); /* cr_uid, cr_ruid */
        sandbox_log("set cr_uid=0, cr_ruid=0 for PID %d", (int)target_pid);
    }

    /* Set cr_sceauthid */
    (void)krw_set_ucred_authid(target_pid, SANDBOX_AUTHID);
    sandbox_log("set cr_sceauthid=0x%llx for PID %d",
                (unsigned long long)SANDBOX_AUTHID, (int)target_pid);

    /* Set cr_scecaps = all ones */
    uint8_t all_caps[16];
    for (int i = 0; i < 16; i++) {
        all_caps[i] = 0xFF;
    }
    (void)krw_set_ucred_caps(target_pid, all_caps);
    sandbox_log("set cr_scecaps=all-ones for PID %d", (int)target_pid);

    /* Set cr_sceattrs byte 3 (attribute flag) */
    uint8_t attrs[32];
    if (krw_get_ucred_attrs(target_pid, attrs) == 0) {
        attrs[3] |= 0x80;
        (void)krw_set_ucred_attrs(target_pid, attrs);
    }

    /* Step 4: Borrow cr_prison from PID 1 (prison0) */
    if (ucred != 0) {
        uintptr_t kproc1 = krw_get_proc(1);
        if (kproc1 != 0) {
            uintptr_t ucred1 = 0;
            if (krw_copyout(kproc1 + 0x40, &ucred1, sizeof(ucred1)) == 0 &&
                ucred1 != 0) {
                uintptr_t prison1 = 0;
                if (krw_copyout(ucred1 + 0x30, &prison1, sizeof(prison1)) == 0 &&
                    prison1 != 0) {
                    (void)krw_copyin(&prison1, ucred + 0x30, sizeof(prison1));
                    sandbox_log("borrowed cr_prison from PID 1 for PID %d",
                                (int)target_pid);
                }
            }
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Socket helpers                                                       */
/* ------------------------------------------------------------------ */

/**
 * Minimal sockaddr_in construction via sys_call.
 * Writes sin_family, sin_port (big-endian), sin_addr, and padding
 * to the buffer. Returns 0 on success.
 */
static int sandbox_fill_sockaddr(char *buf, uint32_t addr, uint16_t port) {
    uint8_t *ubuf = (uint8_t *)buf;
    /* sin_len = 16, sin_family = AF_INET (2), FreeBSD/PS5 socket ABI */
    ubuf[0] = 16;
    ubuf[1] = (uint8_t)SANDBOX_AF_INET;
    /* Port in big-endian */
    ubuf[2] = (uint8_t)((port >> 8) & 0xFF);
    ubuf[3] = (uint8_t)(port & 0xFF);
    /* sin_addr (127.0.0.1 in network byte order) */
    ubuf[4] = (uint8_t)(addr & 0xFF);
    ubuf[5] = (uint8_t)((addr >> 8) & 0xFF);
    ubuf[6] = (uint8_t)((addr >> 16) & 0xFF);
    ubuf[7] = (uint8_t)((addr >> 24) & 0xFF);
    /* Zero padding */
    for (int i = 8; i < 16; i++) {
        ubuf[i] = 0;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* IPC handshake handler                                              */
/* ------------------------------------------------------------------ */

/**
 * Handle a single client connection: read PID, elevate, send status.
 */
static void sandbox_handle_client(int client_fd, uintptr_t rootvnode) {
    /* Read 4-byte PID */
    pid_t client_pid = 0;
    long nread = sys_call(SYS_read, client_fd, (long)&client_pid, 4, 0, 0, 0);
    if (nread != 4) {
        sandbox_log("client read failed (n=%ld), closing", nread);
        sys_call(SYS_close, client_fd, 0, 0, 0, 0, 0);
        return;
    }

    sandbox_log("handshake accepted for PID %d", (int)client_pid);

    /* Perform namespace elevation */
    int result = sandbox_elevate_process(client_pid, rootvnode);

    /* Send status code back */
    int32_t status = (int32_t)result;
    sys_call(SYS_write, client_fd, (long)&status, 4, 0, 0, 0);
    sys_call(SYS_close, client_fd, 0, 0, 0, 0, 0);

    if (result == 0) {
        sandbox_log("namespace elevated for PID %d", (int)client_pid);
    } else {
        sandbox_log("namespace elevation failed for PID %d (rc=%d)", (int)client_pid,
                    result);
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int sandbox_daemon_start(const payload_args_t *args);

int sandbox_daemon_start(const payload_args_t *args) {
#ifndef OOPS_HOST_BUILD
    /* Initialize kernel R/W from payload arguments */
    if (args != NULL) {
        krw_init(args);
        sys_call_init(args);
    } else {
        sys_call_init(NULL);
    }

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

    sandbox_log("daemon starting v%s (args=%s)...", OOPS_APP_VERSION,
                args != NULL ? "present" : "null");

    if (!krw_is_ready()) {
        sandbox_log("ERROR: no kernel R/W available, sleeping");
        for (;;) {
            sandbox_sleep_us(1000000u);
        }
    }

    /* Dynamic allproc discovery */
    uintptr_t allproc = sandbox_resolve_allproc();
    if (allproc != 0) {
        sandbox_log_hex("daemon started: allproc = ", allproc);
    } else {
        sandbox_log("WARNING: allproc resolution failed, using static offset");
    }

    /* Resolve rootvnode from PID 1 / self */
    uintptr_t rootvnode = sandbox_resolve_rootvnode();
    sandbox_log_hex("daemon started: rootvnode = ", rootvnode);

    /* Test directory reading syscalls on /data/homebrew */
    int test_dfd = (int)sys_call(SYS_open, (long)"/data/homebrew", 0, 0, 0, 0, 0);
    if (test_dfd >= 0) {
        char test_buf[256];
        long basep554 = 0;
        long n554 = sys_call(554 /* SYS_getdirentries */, test_dfd, (long)test_buf,
                             sizeof(test_buf), (long)&basep554, 0, 0);
        if (n554 > 0) {
            uint16_t r16 = *(uint16_t *)(test_buf + 16);
            uint16_t n20 = *(uint16_t *)(test_buf + 20);
            sandbox_log("SYS_getdirentries(554) SUCCESS: n=%ld, reclen@16=%u, "
                        "namlen@20=%u, name@24='%s'",
                        n554, (unsigned int)r16, (unsigned int)n20, test_buf + 24);
            sandbox_log(
                "  raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x "
                "%02x %02x %02x %02x",
                (uint8_t)test_buf[0], (uint8_t)test_buf[1], (uint8_t)test_buf[2],
                (uint8_t)test_buf[3], (uint8_t)test_buf[4], (uint8_t)test_buf[5],
                (uint8_t)test_buf[6], (uint8_t)test_buf[7], (uint8_t)test_buf[8],
                (uint8_t)test_buf[9], (uint8_t)test_buf[10], (uint8_t)test_buf[11],
                (uint8_t)test_buf[12], (uint8_t)test_buf[13], (uint8_t)test_buf[14],
                (uint8_t)test_buf[15]);
        } else {
            sandbox_log("SYS_getdirentries(554) returned %ld", n554);
        }

        long basep196 = 0;
        long n196 = sys_call(196 /* SYS_getdirentries_compat11 */, test_dfd,
                             (long)test_buf, sizeof(test_buf), (long)&basep196, 0, 0);
        if (n196 > 0) {
            uint16_t r4 = *(uint16_t *)(test_buf + 4);
            uint8_t n7 = *(uint8_t *)(test_buf + 7);
            sandbox_log("SYS_getdirentries(196) SUCCESS: n=%ld, reclen@4=%u, "
                        "namlen@7=%u, name@8='%s'",
                        n196, (unsigned int)r4, (unsigned int)n7, test_buf + 8);
        } else {
            sandbox_log("SYS_getdirentries(196) returned %ld", n196);
        }
        sys_call(SYS_close, test_dfd, 0, 0, 0, 0, 0);
    } else {
        sandbox_log("open(/data/homebrew) failed rc=%d", test_dfd);
    }

    /* Create TCP listener on 127.0.0.1:9069 */
    int sock = (int)sys_call(SYS_socket, SANDBOX_AF_INET, 1 /* SOCK_STREAM */,
                             6 /* IPPROTO_TCP */, 0, 0, 0);
    if (sock < 0) {
        sandbox_log("ERROR: socket() failed (rc=%d)", sock);
        for (;;) {
            sandbox_sleep_us(1000000u);
        }
    }

    /* Set SO_REUSEADDR so daemon restart does not fail with EADDRINUSE */
    int opt = 1;
    (void)sys_call(SYS_setsockopt, sock, 0xFFFF /* SOL_SOCKET */,
                   0x0004 /* SO_REUSEADDR */, (long)&opt, sizeof(opt), 0);

    /* Bind to 127.0.0.1:9069 */
    char sockaddr[16];
    sandbox_fill_sockaddr(sockaddr, SANDBOX_INADDR_LOOPBACK, SANDBOX_TCP_PORT);
    long bind_rc = sys_call(SYS_bind, sock, (long)sockaddr, 16, 0, 0, 0);
    if (bind_rc != 0) {
        sandbox_log("ERROR: bind() failed (rc=%ld)", bind_rc);
        sys_call(SYS_close, sock, 0, 0, 0, 0, 0);
        for (;;) {
            sandbox_sleep_us(1000000u);
        }
    }

    /* Listen */
    long listen_rc = sys_call(SYS_listen, sock, SANDBOX_LISTEN_BACKLOG, 0, 0, 0, 0);
    if (listen_rc != 0) {
        sandbox_log("ERROR: listen() failed (rc=%ld)", listen_rc);
        sys_call(SYS_close, sock, 0, 0, 0, 0, 0);
        for (;;) {
            sandbox_sleep_us(1000000u);
        }
    }

    sandbox_log("daemon started: listening on 127.0.0.1:%d", SANDBOX_TCP_PORT);

    /* Main accept loop */
    for (;;) {
        char client_addr[16];
        uint32_t client_addr_len = sizeof(client_addr);
        int client_fd = (int)sys_call(SYS_accept, sock, (long)client_addr,
                                      (long)&client_addr_len, 0, 0, 0);
        if (client_fd < 0) {
            sandbox_log("accept() failed (rc=%d), retrying", client_fd);
            sandbox_sleep_us(100000u); /* 100ms backoff */
            continue;
        }
        sandbox_handle_client(client_fd, rootvnode);
    }
#else
    (void)args;
    return 0;
#endif
}
