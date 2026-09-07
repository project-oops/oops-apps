/*
 * Porthole PS5 standalone ELF payload entry point.
 *
 * Executed by elfldr (port 9021/9020) with payload_args_t in rdi.
 */

#include "porthole.h"
#include "oops/freestd.h"
#include "oops/krw.h"
#include "oops/syscall.h"

void klog_write(const char *msg) {
    if (msg == NULL) return;
    char buf[256];
    const char *prefix = "[PORTHOLE] ";
    size_t plen = obs_strlen(prefix);
    size_t mlen = obs_strlen(msg);
    if (plen + mlen + 2 > sizeof(buf)) {
        mlen = sizeof(buf) - plen - 2;
    }
    memcpy(buf, prefix, plen);
    memcpy(buf + plen, msg, mlen);
    buf[plen + mlen] = '\n';
    buf[plen + mlen + 1] = '\0';
    sys_call(SYS_klog, 7, (long)buf, 0, 0, 0, 0);
}

void klog_write_hex(const char *prefix, uint64_t hex) {
    char buf[128];
    size_t plen = obs_strlen(prefix);
    if (plen > sizeof(buf) - OBS_NUM_MAX - 1) {
        plen = sizeof(buf) - OBS_NUM_MAX - 1;
    }
    memcpy(buf, prefix, plen);
    size_t hlen = obs_format_hex(buf + plen, hex);
    buf[plen + hlen] = '\0';
    klog_write(buf);
}

void klog_write_num(const char *prefix, int64_t num) {
    char buf[128];
    size_t plen = obs_strlen(prefix);
    if (plen > sizeof(buf) - OBS_NUM_MAX - 1) {
        plen = sizeof(buf) - OBS_NUM_MAX - 1;
    }
    memcpy(buf, prefix, plen);
    size_t nlen = obs_format_i64(buf + plen, num);
    buf[plen + nlen] = '\0';
    klog_write(buf);
}

int porthole_start(const payload_args_t *args);

int porthole_start(const payload_args_t *args) {
    if (args != NULL) {
        sys_call_init(args);
    }

    klog_write("Porthole payload entry reached");

    porthole_set_payload_args(args);

    klog_write("Opening video encoder subsystem (M1)...");
    porthole_status enc_status = porthole_encoder_open();
    if (enc_status != PORTHOLE_OK) {
        klog_write_num("porthole_encoder_open failed with status: ", (int64_t)enc_status);
        return (int)enc_status;
    }

    const porthole_encoder_api *api = porthole_encoder_get_api();
    if (api != NULL) {
        klog_write("Encoder API self-resolved successfully (M1):");
        klog_write_hex("  QueryMemorySize: ", (uint64_t)(uintptr_t)api->query_memory_size);
        klog_write_hex("  CreateEncoder:   ", (uint64_t)(uintptr_t)api->create_encoder);
        klog_write_hex("  GetAuData:       ", (uint64_t)(uintptr_t)api->get_au_data);
        klog_write_hex("  SetInputFrame:   ", (uint64_t)(uintptr_t)api->set_input_frame);
        klog_write_hex("  StartSequence:   ", (uint64_t)(uintptr_t)api->start_sequence);
        klog_write_hex("  StopSequence:    ", (uint64_t)(uintptr_t)api->stop_sequence);
        klog_write_hex("  DeleteEncoder:   ", (uint64_t)(uintptr_t)api->delete_encoder);
    }

#if !defined(PORTHOLE_ENCODER_SESSION)
    /* The struct-taking encoder calls are compiled out unless a build asks for them, so this
     * run exercises the sockets and the template stream and nothing that could corrupt the
     * stack. See porthole_encoder_session_enabled in porthole.h. */
    klog_write("Encoder session gated off (build without PORTHOLE_ENCODER_SESSION): "
               "serving the template stream");
#else
    klog_write("Inspecting hardware encoder session (M2)...");
    if (porthole_encoder_is_active()) {
        const porthole_encoder_session *sess = porthole_encoder_get_session();
        klog_write("Encoder session initialized and active (M2):");
        klog_write_num("  Handle:          ", (int64_t)sess->handle);
        klog_write_hex("  Work mem bytes:  ", (uint64_t)sess->work_mem_size);
        klog_write_hex("  Work mem phys:   ", (uint64_t)sess->work_mem_phys);
        klog_write_hex("  Work mem vaddr:  ", (uint64_t)(uintptr_t)sess->work_mem);
        klog_write_num("  Width:           ", (int64_t)sess->config.width);
        klog_write_num("  Height:          ", (int64_t)sess->config.height);
        klog_write_num("  Bitrate:         ", (int64_t)sess->config.bitrate);
    } else {
        klog_write("Encoder session not active; trying explicit session creation...");
        porthole_encoder_config cfg;
        porthole_encoder_config_default(&cfg);
        porthole_status s_rc = porthole_encoder_session_create(&cfg);
        if (s_rc == PORTHOLE_OK && porthole_encoder_is_active()) {
            const porthole_encoder_session *sess = porthole_encoder_get_session();
            klog_write("Encoder session explicitly created (M2):");
            klog_write_num("  Handle:          ", (int64_t)sess->handle);
            klog_write_hex("  Work mem bytes:  ", (uint64_t)sess->work_mem_size);
        } else {
            klog_write_num("Encoder session creation returned: ", (int64_t)s_rc);
        }
    }
#endif

    klog_write("Initializing controller pad subsystem (M5)...");
    porthole_status pad_status = porthole_pad_open();
    if (pad_status == PORTHOLE_OK) {
        const porthole_pad_api *papi = porthole_pad_get_api();
        if (papi != NULL) {
            klog_write("Pad API self-resolved successfully:");
            klog_write_hex("  scePadInit:                 ", (uint64_t)(uintptr_t)papi->pad_init);
            klog_write_hex("  scePadVirtualDeviceAdd:     ", (uint64_t)(uintptr_t)papi->virtual_device_add);
            klog_write_hex("  scePadVirtualDeviceInsert:  ", (uint64_t)(uintptr_t)papi->virtual_device_insert);
            klog_write_hex("  scePadVirtualDeviceDelete:  ", (uint64_t)(uintptr_t)papi->virtual_device_delete);
        }
    }

    /* Start main payload dual-socket server loop (M4/M5) */
    klog_write("Starting dual-socket server (9805 video, 9806 input)...");
    return (int)porthole_run();
}
