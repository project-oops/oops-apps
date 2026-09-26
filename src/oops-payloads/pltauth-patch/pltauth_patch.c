/*
 * pltauth-patch: SceShellCore entitlement bypass for native PS5 Big App homebrew.
 *
 * Background:
 * On PS5 (Prospero), native Big Apps (category 0 / native_game) are the ONLY execution
 * mode that receives exclusive direct HDMI scanout (OBS_VIDEO_BUS_MAIN) and direct
 * LibAgc GPU hardware access. PS4 PKGs run under backward compatibility (bc_boost)
 * where rtld rejects PS5 LibAgc libraries with "ERROR: ABIVERSION mismatch".
 *
 * When launching native Big App 0 without a retail PSN license ticket, PFAuthClient
 * inside SceShellCore queries /dev/pltauth (sceSblPltAuth2VeriR1C2GenR2), receives
 * error -35, logs "[PFAuthClient] Notified error:(0x80de0051)", and SceShellCore
 * terminates the process.
 *
 * Solution:
 * SceShellCore is the userland gatekeeper that enforces entitlement checks and
 * terminates unlicensed applications. This payload applies the verified FW 12.40
 * SceShellCore binary patch set (from SharpProspero / ShellCorePatchData) directly into
 * SceShellCore's resident memory using kernel Direct Map (DMAP) physical page
 * translation.
 *
 * By writing directly through the kernel's DMAP (pml4u - cr3) via VirtToPhys page table
 * translation, text segment write protection is completely bypassed with zero ptrace
 * thread interruptions and zero risk of kernel mode page fault panics.
 *
 * Once applied, SceShellCore remains patched for the entire console uptime, allowing
 * native Big App 0 applications to launch and execute uninterrupted.
 */

#include "oops/freestd.h"
#include "oops/syscall.h"
#include "oops/krw.h"

/* Function prototypes for -Wmissing-prototypes */
void klog_write(const char *msg);
void klog_write_hex(const char *prefix, uint64_t hex);
void klog_write_num(const char *prefix, int64_t num);
int pltauth_patch_start(payload_args_t *args);

/* Direct socket/terminal logging: elfldr maps socket to stdout (fd 1) and stderr (fd 2)
 */
void klog_write(const char *msg) {
    if (msg == NULL) {
        return;
    }
    char kbuf[300];
    int klen = oops_snprintf(kbuf, sizeof(kbuf), "[PLTAUTH] %s\n", msg);
    if (klen > 0) {
        sys_call(SYS_klog, 7, (long)kbuf, 0, 0, 0, 0);
    }
    char buf[256];
    int len = oops_snprintf(buf, sizeof(buf), "%s\n", msg);
    if (len > 0) {
        sys_call(SYS_write, 1, (long)buf, (long)len, 0, 0, 0);
        sys_call(SYS_write, 2, (long)buf, (long)len, 0, 0, 0);
    }
}

void klog_write_hex(const char *prefix, uint64_t hex) {
    char buf[128];
    oops_snprintf(buf, sizeof(buf), "%s: 0x%llx", prefix ? prefix : "",
                  (unsigned long long)hex);
    klog_write(buf);
}

void klog_write_num(const char *prefix, int64_t num) {
    char buf[128];
    oops_snprintf(buf, sizeof(buf), "%s: %lld", prefix ? prefix : "", (long long)num);
    klog_write(buf);
}

static int str_contains_case_insensitive(const char *haystack, const char *needle) {
    if (haystack == NULL || needle == NULL) {
        return 0;
    }
    size_t hlen = obs_strlen(haystack);
    size_t nlen = obs_strlen(needle);
    if (nlen == 0 || hlen < nlen) {
        return 0;
    }
    for (size_t i = 0; i <= hlen - nlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            char c1 = haystack[i + j];
            char c2 = needle[j];
            if (c1 >= 'A' && c1 <= 'Z') {
                c1 = (char)(c1 + ('a' - 'A'));
            }
            if (c2 >= 'A' && c2 <= 'Z') {
                c2 = (char)(c2 + ('a' - 'A'));
            }
            if (c1 != c2) {
                break;
            }
        }
        if (j == nlen) {
            return 1;
        }
    }
    return 0;
}

/* One binary patch entry relative to SceShellCore module base */
typedef struct {
    uint64_t offset;
    uint8_t len;
    uint8_t data[16];
} shellcore_patch_t;

/* Verified retail SceShellCore patches for FW 12.20 and FW 12.40 (SharpProspero /
 * ShellCorePatchData) */
static const shellcore_patch_t FW_1240_PATCHES[] = {
    {0x00C870C3ULL, 3, {0x52, 0xEB, 0xE2}},
    {0x00C870A8ULL, 7, {0xE8, 0x23, 0xF8, 0xFF, 0xFF, 0x58, 0xC3}},
    {0x00C868B6ULL, 5, {0xE9, 0x07, 0x00, 0x00, 0x00}},
    {0x00C868C2ULL, 10, {0x31, 0xC0, 0x50, 0xE8, 0x06, 0x00, 0x00, 0x00, 0x58, 0xC3}},
    {0x00789EE6ULL, 2, {0xEB, 0x04}},
    {0x00330D81ULL, 2, {0xEB, 0x04}},
    {0x00331151ULL, 2, {0xEB, 0x04}},
    {0x007AC232ULL, 1, {0xEB}},
    {0x007930A5ULL, 2, {0x90, 0xE9}},
    {0x007AC9C8ULL, 1, {0xEB}},
    {0x007AEF86ULL, 4, {0x9E, 0x01, 0x00, 0x00}},
    {0x00214E81ULL,
     14,
     {0xE8, 0x3A, 0xFC, 0x67, 0x00, 0x31, 0xC9, 0xFF, 0xC1, 0xE9, 0xC4, 0xFE, 0xFF,
      0xFF}},
    {0x00214D53ULL,
     11,
     {0x83, 0xF8, 0x02, 0x0F, 0x43, 0xC1, 0xE9, 0x60, 0x0A, 0x00, 0x00}},
    {0x00215260ULL, 5, {0xE9, 0x1C, 0xFC, 0xFF, 0xFF}},
    {0x007D2350ULL, 1, {0xC3}},
    {0x017438E0ULL, 3, {0x31, 0xC0, 0xC3}},
    {0x01747E40ULL, 3, {0x31, 0xC0, 0xC3}},
    {0x006557AAULL, 2, {0x66, 0x90}},
    {0x00B1BEBAULL, 1, {0xEB}},
    {0x00AF9483ULL, 2, {0xEB, 0x03}},
    {0x00328EE0ULL, 2, {0x90, 0xE9}},
    {0x00328F5AULL, 2, {0x90, 0xE9}},
    {0x0032905CULL, 1, {0xEB}},
    {0x00329130ULL, 1, {0xEB}},
    {0x00329351ULL, 2, {0x90, 0xE9}},
    {0x00329462ULL, 1, {0xEB}},
    {0x0032993AULL, 2, {0x90, 0xE9}},
    {0x003299CDULL, 2, {0x90, 0xE9}},
    {0x00788378ULL, 1, {0xEB}},
    {0x0078BF72ULL, 1, {0xEB}},
    {0x0078FE10ULL, 4, {0x48, 0x31, 0xC0, 0xC3}},
};

/* Verified retail SceShellCore patches for FW 12.60 */
static const shellcore_patch_t FW_1260_PATCHES[] = {
    {0x00C8CF23ULL, 3, {0x52, 0xEB, 0xE2}},
    {0x00C8CF08ULL, 7, {0xE8, 0x23, 0xF8, 0xFF, 0xFF, 0x58, 0xC3}},
    {0x00C8C716ULL, 5, {0xE9, 0x07, 0x00, 0x00, 0x00}},
    {0x00C8C722ULL, 10, {0x31, 0xC0, 0x50, 0xE8, 0x06, 0x00, 0x00, 0x00, 0x58, 0xC3}},
    {0x0078B136ULL, 2, {0xEB, 0x04}},
    {0x00331471ULL, 2, {0xEB, 0x04}},
    {0x00331841ULL, 2, {0xEB, 0x04}},
    {0x007AD482ULL, 1, {0xEB}},
    {0x007942F5ULL, 2, {0x90, 0xE9}},
    {0x007ADC18ULL, 1, {0xEB}},
    {0x007B01D6ULL, 4, {0x9E, 0x01, 0x00, 0x00}},
    {0x00214E81ULL,
     14,
     {0xE8, 0x8A, 0x0E, 0x68, 0x00, 0x31, 0xC9, 0xFF, 0xC1, 0xE9, 0xC4, 0xFE, 0xFF,
      0xFF}},
    {0x00214D53ULL,
     11,
     {0x83, 0xF8, 0x02, 0x0F, 0x43, 0xC1, 0xE9, 0x60, 0x0A, 0x00, 0x00}},
    {0x00215260ULL, 5, {0xE9, 0x1C, 0xFC, 0xFF, 0xFF}},
    {0x007D35A0ULL, 1, {0xC3}},
    {0x0174A680ULL, 3, {0x31, 0xC0, 0xC3}},
};

static const shellcore_patch_t *get_shellcore_patches(uint32_t fw, size_t *out_count) {
    uint32_t masked = fw & 0xFFFF0000u;
    switch (masked) {
    case 0x12200000u:
    case 0x12400000u:
    case 0x0C200000u:
    case 0x0C400000u:
        *out_count = sizeof(FW_1240_PATCHES) / sizeof(FW_1240_PATCHES[0]);
        return FW_1240_PATCHES;
    case 0x12600000u:
    case 0x12700000u:
    case 0x0C600000u:
    case 0x0C700000u:
        *out_count = sizeof(FW_1260_PATCHES) / sizeof(FW_1260_PATCHES[0]);
        return FW_1260_PATCHES;
    default:
        *out_count = 0;
        return NULL;
    }
}

/* Dynamic allproc discovery: scans kdata for proc chain */
static uintptr_t resolve_allproc(void) {
    uintptr_t kdata = krw_kdata_base();
    if (kdata == 0) {
        return krw_allproc_addr();
    }
    pid_t mypid = (pid_t)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0);
    uintptr_t start = kdata + 0x2600000;
    uintptr_t end = kdata + 0x2B00000;
    uintptr_t user_allproc = 0;
    int max_user_chain = 0;

    for (uintptr_t addr = start; addr < end; addr += 8) {
        uintptr_t p = krw_read64(addr);
        if ((p >> 40) != 0xffffcd && (p >> 40) != 0xffffff) {
            continue;
        }

        int user_chain_len = 0;
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
                user_chain_len++;
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
            user_allproc = addr;
            break;
        }

        if (user_chain_len > max_user_chain && user_chain_len >= 5) {
            user_allproc = addr;
            max_user_chain = user_chain_len;
        }
    }

    if (user_allproc != 0) {
        krw_set_allproc_addr(user_allproc);
        return user_allproc;
    }
    return krw_allproc_addr();
}

/* Locate SceShellCore proc structure in kernel allproc linked list */
static uintptr_t find_shellcore_proc(pid_t *out_pid) {
    uintptr_t allproc = resolve_allproc();
    if (allproc == 0) {
        return 0;
    }
    klog_write_hex("pltauth-patch: using allproc: ", allproc);
    uintptr_t proc = 0;
    if (krw_copyout(allproc, &proc, sizeof(proc)) != 0 || proc == 0) {
        klog_write("pltauth-patch: failed to read first proc from allproc");
        return 0;
    }

    /* Candidate p_comm offsets across FreeBSD / Prospero firmwares */
    const uintptr_t comm_offsets[] = {0x5E4, 0x604, 0x61E, 0x5DC, 0x274};

    while (proc != 0) {
        pid_t pid = 0;
        krw_copyout(proc + 0xBC, &pid, sizeof(pid));

        for (size_t i = 0; i < sizeof(comm_offsets) / sizeof(comm_offsets[0]); i++) {
            char comm[32];
            memset(comm, 0, sizeof(comm));
            krw_copyout(proc + comm_offsets[i], comm, sizeof(comm) - 1);
            if (obs_strcmp(comm, "SceShellCore") == 0 ||
                obs_strcmp(comm, "SceShellCore.elf") == 0) {
                if (out_pid != NULL) {
                    *out_pid = pid;
                }
                return proc;
            }
        }

        /* Fallback: scan 0x200..0x7F0 for "SceShellCore" */
        char pbuf[2048];
        memset(pbuf, 0, sizeof(pbuf));
        if (krw_copyout(proc, pbuf, sizeof(pbuf)) == 0) {
            for (size_t off = 0x200; off < sizeof(pbuf) - 16; off++) {
                if (obs_strncmp(pbuf + off, "SceShellCore", 12) == 0) {
                    if (out_pid != NULL) {
                        *out_pid = pid;
                    }
                    return proc;
                }
            }
        }

        uintptr_t next = 0;
        if (krw_copyout(proc, &next, sizeof(next)) != 0 || next == proc) {
            break;
        }
        proc = next;
    }
    return 0;
}

/* Walk SceShellCore's loaded module list (p_dynlib at kproc + 0x3E8) to find module
 * base */
static uintptr_t find_shellcore_module_base(uintptr_t kproc) {
    uintptr_t kaddr = 0;
    if (krw_copyout(kproc + 0x3E8, &kaddr, sizeof(kaddr)) != 0 || kaddr == 0) {
        klog_write("pltauth-patch: ERROR - failed to read p_dynlib (kproc + 0x3E8)");
        return 0;
    }

    uintptr_t cur = 0;
    if (krw_copyout(kaddr, &cur, sizeof(cur)) != 0 || cur == 0) {
        klog_write("pltauth-patch: ERROR - failed to read initial dynlib_obj");
        return 0;
    }

    uintptr_t fallback_base = 0;
    int count = 0;

    while (cur != 0 && count < 128) {
        uintptr_t next = 0;
        uintptr_t path_ptr = 0;
        uint32_t handle = 0;
        uint64_t mapbase = 0;

        krw_copyout(cur + 0x00, &next, sizeof(next));
        krw_copyout(cur + 0x08, &path_ptr, sizeof(path_ptr));
        krw_copyout(cur + 0x28, &handle, sizeof(handle));
        krw_copyout(cur + 0x30, &mapbase, sizeof(mapbase));

        char mod_path[128];
        memset(mod_path, 0, sizeof(mod_path));
        if (path_ptr != 0) {
            krw_copyout(path_ptr, mod_path, sizeof(mod_path) - 1);
        }

        if (mapbase != 0) {
            if (mod_path[0] != '\0') {
                klog_write_hex(mod_path, mapbase);
            }
            if (str_contains_case_insensitive(mod_path, "shell_core") ||
                str_contains_case_insensitive(mod_path, "SceShellCore")) {
                klog_write_hex("pltauth-patch: matched SceShellCore by name, base=",
                               mapbase);
                return (uintptr_t)mapbase;
            }
            if (handle == 1 && fallback_base == 0) {
                fallback_base = (uintptr_t)mapbase;
            }
        }

        if (next == 0 || next == cur) {
            break;
        }
        cur = next;
        count++;
    }

    if (fallback_base != 0) {
        klog_write_hex("pltauth-patch: matched SceShellCore by main handle 1, base=",
                       fallback_base);
        return fallback_base;
    }

    return 0;
}

/*
 * x86-64 4-level page table translation.
 * Resolves user virtual address (va) to physical address (pa) using the process's CR3
 * and kernel direct physical memory map (DMAP_BASE).
 * Checks Present bits at each level, handles 1GB and 2MB superpages, and returns 0 if
 * unmapped.
 */
static uint64_t virt_to_phys(uint64_t cr3, uint64_t dmap, uint64_t va) {
    uint64_t pml4i = (va >> 39) & 0x1FFULL;
    uint64_t pdpti = (va >> 30) & 0x1FFULL;
    uint64_t pdi = (va >> 21) & 0x1FFULL;
    uint64_t pti = (va >> 12) & 0x1FFULL;

    uint64_t pml4e =
        krw_read64((uintptr_t)(dmap + (cr3 & 0x000FFFFFFFFFF000ULL) + pml4i * 8));
    if ((pml4e & 1ULL) == 0) {
        return 0;
    }

    uint64_t pdpte =
        krw_read64((uintptr_t)(dmap + (pml4e & 0x000FFFFFFFFFF000ULL) + pdpti * 8));
    if ((pdpte & 1ULL) == 0) {
        return 0;
    }
    if ((pdpte & 0x80ULL) != 0) {
        /* 1 GB page */
        return (pdpte & 0x000FFFFFC0000000ULL) | (va & 0x3FFFFFFFULL);
    }

    uint64_t pde =
        krw_read64((uintptr_t)(dmap + (pdpte & 0x000FFFFFFFFFF000ULL) + pdi * 8));
    if ((pde & 1ULL) == 0) {
        return 0;
    }
    if ((pde & 0x80ULL) != 0) {
        /* 2 MB page */
        return (pde & 0x000FFFFFFFE00000ULL) | (va & 0x1FFFFFULL);
    }

    uint64_t pte =
        krw_read64((uintptr_t)(dmap + (pde & 0x000FFFFFFFFFF000ULL) + pti * 8));
    if ((pte & 1ULL) == 0) {
        return 0;
    }

    return (pte & 0x000FFFFFFFFFF000ULL) | (va & 0xFFFULL);
}

/* Copies data to a user virtual address through the direct physical memory map */
static int phys_copyin(uint64_t cr3, uint64_t dmap, uint64_t uva, const void *src,
                       size_t len) {
    const uint8_t *s = (const uint8_t *)src;
    while (len > 0) {
        uint64_t pa = virt_to_phys(cr3, dmap, uva);
        if (pa == 0) {
            return -1;
        }
        size_t page_rem = (size_t)(0x1000ULL - (uva & 0xFFFULL));
        size_t chunk = (len < page_rem) ? len : page_rem;
        if (krw_copyin(s, (uintptr_t)(dmap + pa), chunk) != 0) {
            return -1;
        }
        uva += (uint64_t)chunk;
        s += chunk;
        len -= chunk;
    }
    return 0;
}

/* Copies data from a user virtual address through the direct physical memory map */
static int phys_copyout(uint64_t cr3, uint64_t dmap, uint64_t uva, void *dst,
                        size_t len) {
    uint8_t *d = (uint8_t *)dst;
    while (len > 0) {
        uint64_t pa = virt_to_phys(cr3, dmap, uva);
        if (pa == 0) {
            return -1;
        }
        size_t page_rem = (size_t)(0x1000ULL - (uva & 0xFFFULL));
        size_t chunk = (len < page_rem) ? len : page_rem;
        if (krw_copyout((uintptr_t)(dmap + pa), d, chunk) != 0) {
            return -1;
        }
        uva += (uint64_t)chunk;
        d += chunk;
        len -= chunk;
    }
    return 0;
}

int pltauth_patch_start(payload_args_t *args) {
    if (args == NULL) {
        return -1;
    }

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

    sys_call_init(args);
    klog_write("pltauth-patch: starting SceShellCore entitlement bypass payload "
               "(v " OOPS_APP_VERSION ")...");

    if (krw_init(args) != 0) {
        klog_write("pltauth-patch: ERROR - krw_init failed");
        return -2;
    }

    uint32_t fw = krw_fw_version();
    klog_write_hex("pltauth-patch: detected kernel firmware version: ", (uint64_t)fw);

    size_t patch_count = 0;
    const shellcore_patch_t *patches = get_shellcore_patches(fw, &patch_count);
    if (patches == NULL || patch_count == 0) {
        klog_write("pltauth-patch: ERROR - unsupported firmware version for "
                   "SceShellCore bypass");
        return -3;
    }
    klog_write_num("pltauth-patch: loaded patch set count=", (int64_t)patch_count);

    /* Locate SceShellCore */
    pid_t shell_pid = 0;
    uintptr_t kproc = find_shellcore_proc(&shell_pid);
    if (kproc == 0) {
        /* Fallback: try krw_find_proc_by_name */
        kproc = krw_find_proc_by_name("SceShellCore");
        if (kproc != 0) {
            krw_copyout(kproc + 0xBC, &shell_pid, sizeof(shell_pid));
        }
    }
    if (kproc == 0) {
        klog_write("pltauth-patch: ERROR - SceShellCore not found in process census");
        return -4;
    }
    klog_write_num("pltauth-patch: located SceShellCore pid=", (int64_t)shell_pid);
    klog_write_hex("pltauth-patch: located SceShellCore kproc=", kproc);

    /* Resolve SceShellCore module base address */
    uintptr_t module_base = find_shellcore_module_base(kproc);
    if (module_base == 0) {
        klog_write(
            "pltauth-patch: ERROR - could not determine SceShellCore module base");
        return -5;
    }
    if (module_base < 0x80000000ULL || module_base >= 0x800000000000ULL) {
        klog_write_hex("pltauth-patch: ERROR - invalid userland module base: ",
                       module_base);
        return -5;
    }
    klog_write_hex("pltauth-patch: SceShellCore module base: ", module_base);

    /* Read vmspace from SceShellCore (p_vmspace at kproc + 0x200) */
    uintptr_t vmspace = 0;
    if (krw_copyout(kproc + 0x200, &vmspace, sizeof(vmspace)) != 0 || vmspace == 0) {
        klog_write("pltauth-patch: ERROR - failed to read vmspace");
        return -6;
    }

    /* Locate vm_pmap within vmspace based on firmware version:
     * FW >= 6.00: 0x2E8
     * FW 1.05 - 5.x: 0x2E0
     * FW 1.00 - 1.02: 0x2C0
     */
    uint32_t fw_major = (fw >> 24) & 0xFFu;
    uintptr_t pmap_off = 0x2E8;
    if (fw_major < 6) {
        if (fw_major == 1 && ((fw >> 16) & 0xFFu) < 3) {
            pmap_off = 0x2C0;
        } else {
            pmap_off = 0x2E0;
        }
    }

    uintptr_t pml4u = 0;
    uintptr_t cr3 = 0;
    if (krw_copyout(vmspace + pmap_off + 0x20, &pml4u, sizeof(pml4u)) != 0 ||
        krw_copyout(vmspace + pmap_off + 0x28, &cr3, sizeof(cr3)) != 0) {
        klog_write("pltauth-patch: ERROR - failed to read pml4u / cr3 from pmap");
        return -7;
    }

    if (pml4u < 0xffff800000000000ULL || cr3 == 0 || cr3 > 0x8000000000ULL) {
        klog_write_hex("pltauth-patch: ERROR - invalid pml4u: ", pml4u);
        klog_write_hex("pltauth-patch: ERROR - invalid cr3:   ", cr3);
        return -8;
    }

    uintptr_t dmap = pml4u - (cr3 & 0x000FFFFFFFFFF000ULL);
    if (dmap < 0xffff800000000000ULL) {
        klog_write_hex("pltauth-patch: ERROR - invalid dmap base: ", dmap);
        return -9;
    }

    klog_write_hex("pltauth-patch: pml4u: ", pml4u);
    klog_write_hex("pltauth-patch: cr3:   ", cr3);
    klog_write_hex("pltauth-patch: dmap:  ", dmap);

    /* Apply patches directly through direct physical map */
    size_t applied_count = 0;
    size_t already_count = 0;
    size_t failed_count = 0;

    for (size_t i = 0; i < patch_count; i++) {
        uint64_t target_va = (uint64_t)module_base + patches[i].offset;
        uint8_t cur_data[16];
        memset(cur_data, 0, sizeof(cur_data));

        /* Read existing instruction bytes before writing */
        if (phys_copyout(cr3, dmap, target_va, cur_data, patches[i].len) != 0) {
            klog_write_hex("pltauth-patch: cannot read target opcode at offset: ",
                           patches[i].offset);
            failed_count++;
            continue;
        }

        /* Check if already patched */
        if (memcmp(cur_data, patches[i].data, patches[i].len) == 0) {
            already_count++;
            continue;
        }

        /* Safety check: ensure target memory is not unmapped/blank (all 0x00 or all
         * 0xFF) */
        int all_zero = 1;
        int all_ff = 1;
        for (uint8_t b = 0; b < patches[i].len; b++) {
            if (cur_data[b] != 0x00)
                all_zero = 0;
            if (cur_data[b] != 0xFF)
                all_ff = 0;
        }
        if (all_zero || all_ff) {
            klog_write_hex(
                "pltauth-patch: refusing to patch blank/invalid memory at offset: ",
                patches[i].offset);
            failed_count++;
            continue;
        }

        /* Write patch bytes */
        if (phys_copyin(cr3, dmap, target_va, patches[i].data, patches[i].len) != 0) {
            klog_write_hex("pltauth-patch: failed writing patch offset: ",
                           patches[i].offset);
            failed_count++;
            continue;
        }

        /* Verify written bytes */
        memset(cur_data, 0, sizeof(cur_data));
        if (phys_copyout(cr3, dmap, target_va, cur_data, patches[i].len) != 0 ||
            memcmp(cur_data, patches[i].data, patches[i].len) != 0) {
            klog_write_hex("pltauth-patch: verification failed at offset: ",
                           patches[i].offset);
            failed_count++;
            continue;
        }

        applied_count++;
    }

    klog_write_num("pltauth-patch: patches applied: ", (int64_t)applied_count);
    klog_write_num("pltauth-patch: already applied: ", (int64_t)already_count);
    klog_write_num("pltauth-patch: failed patches:  ", (int64_t)failed_count);

    if (failed_count > 0) {
        klog_write("pltauth-patch: WARNING - some patches failed to apply or verify");
        return -10;
    }

    klog_write("pltauth-patch: SUCCESS - SceShellCore entitlement bypass active!");
    klog_write("pltauth-patch: returning 0 to elfldr.");
    return 0;
}
