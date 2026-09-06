/*
 * Porthole - self-test for the wire contract.
 *
 * The payload is a scaffold, but the bytes on the two sockets are a *decision* both
 * ends honour, so the one thing worth testing today is that the record layout and its
 * decoder agree - before either end is written against a shape that turns out wrong.
 * Runs on an ordinary host, like the probe's `make host`: a contract nobody can
 * exercise is not a contract.
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "porthole.h"

/* The record is 24 bytes with its fields exactly where VIDEO.md says. If the compiler
 * padded it, every offset below the pad is wrong and both ends would disagree silently.
 */
_Static_assert(sizeof(porthole_pad) == PORTHOLE_PAD_BYTES,
               "the pad record must be 24 bytes");
_Static_assert(offsetof(porthole_pad, version) == 4, "version at 4");
_Static_assert(offsetof(porthole_pad, slot) == 6, "slot at 6");
_Static_assert(offsetof(porthole_pad, buttons) == 8, "buttons at 8");
_Static_assert(offsetof(porthole_pad, sticks) == 12, "sticks at 12");
_Static_assert(offsetof(porthole_pad, triggers) == 16, "triggers at 16");
_Static_assert(offsetof(porthole_pad, sequence) == 20, "sequence at 20");

/* A well-formed record: slot 2, a couple of buttons, sticks off-centre, R2
 * half-pressed, seq 7. */
static void good_record(uint8_t b[PORTHOLE_PAD_BYTES]) {
    memset(b, 0, PORTHOLE_PAD_BYTES);
    b[0] = 'P';
    b[1] = 'P';
    b[2] = 'A';
    b[3] = 'D';
    b[4] = 1;    /* version 1 */
    b[6] = 2;    /* slot 2 */
    b[8] = 0x05; /* buttons: bits 0 and 2 */
    b[12] = 128;
    b[13] = 200; /* LX centre, LY pushed */
    b[14] = 128;
    b[15] = 128; /* RX/RY centre */
    b[17] = 127; /* R2 half */
    b[20] = 7;   /* sequence 7 */
}

int main(void) {
    unsigned int failures = 0;
    uint8_t bytes[PORTHOLE_PAD_BYTES];
    porthole_pad pad;

    /* A good record decodes, and the fields land where they were put. */
    good_record(bytes);
    if (porthole_pad_decode(bytes, &pad) != PORTHOLE_OK) {
        printf("FAIL: a well-formed record was rejected\n");
        failures++;
    } else if (pad.version != 1 || pad.slot != 2 || pad.buttons != 0x05u ||
               pad.sticks[1] != 200 || pad.triggers[1] != 127 || pad.sequence != 7) {
        printf("FAIL: a field decoded to the wrong value\n");
        failures++;
    }

    /* Bad magic is refused - a stray connection writing anything must not read as a
     * pad. */
    good_record(bytes);
    bytes[2] = 'X';
    if (porthole_pad_decode(bytes, &pad) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: a record with wrong magic was accepted\n");
        failures++;
    }

    /* A slot beyond the fourth is refused, not clamped onto pad three. */
    good_record(bytes);
    bytes[6] = 4;
    if (porthole_pad_decode(bytes, &pad) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: a fifth-pad record was accepted\n");
        failures++;
    }

    /* A non-zero reserved byte is refused, so a version bump into that room is a clean
     * upgrade. */
    good_record(bytes);
    bytes[7] = 0xFF;
    if (porthole_pad_decode(bytes, &pad) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: a record with a dirty reserved byte was accepted\n");
        failures++;
    }

    /* The scaffold is honest about being a scaffold: the three real operations refuse,
     * and the go/no-go refuses with the specific "no encoder" that the obSCEne probe
     * exists to resolve. */
    if (porthole_encoder_get_api() != NULL) {
        printf("FAIL: encoder api should be NULL when unopened\n");
        failures++;
    }
    if (porthole_encoder_open() != PORTHOLE_NO_ENCODER) {
        printf("FAIL: the encoder should report NO_ENCODER on host\n");
        failures++;
    }
    if (porthole_encoder_get_api() != NULL) {
        printf("FAIL: encoder api should still be NULL after failed open\n");
        failures++;
    }
    if (porthole_run() != PORTHOLE_NO_ENCODER) {
        printf("FAIL: run() should refuse cleanly while the encoder is unreachable\n");
        failures++;
    }

    /* Milestone M2: Encoder Configuration & Lifecycle Tests */
    porthole_encoder_config cfg;
    porthole_encoder_config_default(&cfg);
    if (cfg.size != sizeof(porthole_encoder_config) ||
        cfg.codec != PORTHOLE_CODEC_AVC ||
        cfg.width != 1920 ||
        cfg.height != 1080 ||
        cfg.fps_num != 60 ||
        cfg.fps_den != 1 ||
        cfg.bitrate != 10000000 ||
        cfg.profile != PORTHOLE_PROFILE_AVC_HIGH ||
        cfg.level != 42 ||
        cfg.rc_mode != PORTHOLE_RC_CBR ||
        cfg.gop_size != 60) {
        printf("FAIL: porthole_encoder_config_default did not set expected parameters\n");
        failures++;
    }

    if (porthole_encoder_config_validate(&cfg) != PORTHOLE_OK) {
        printf("FAIL: valid default configuration was rejected\n");
        failures++;
    }

    /* Configuration validation invariant checks */
    if (porthole_encoder_config_validate(NULL) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: null encoder config was accepted\n");
        failures++;
    }

    porthole_encoder_config bad_cfg = cfg;
    bad_cfg.size = sizeof(porthole_encoder_config) - 4;
    if (porthole_encoder_config_validate(&bad_cfg) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: incorrect config size was accepted\n");
        failures++;
    }

    bad_cfg = cfg;
    bad_cfg.codec = 99; /* Unsupported codec */
    if (porthole_encoder_config_validate(&bad_cfg) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: unsupported codec was accepted\n");
        failures++;
    }

    bad_cfg = cfg;
    bad_cfg.width = 1921; /* Odd width */
    if (porthole_encoder_config_validate(&bad_cfg) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: odd width was accepted\n");
        failures++;
    }

    bad_cfg = cfg;
    bad_cfg.height = 0; /* Zero height */
    if (porthole_encoder_config_validate(&bad_cfg) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: zero height was accepted\n");
        failures++;
    }

    bad_cfg = cfg;
    bad_cfg.bitrate = 0; /* Zero bitrate */
    if (porthole_encoder_config_validate(&bad_cfg) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: zero bitrate was accepted\n");
        failures++;
    }

    /* Host behavior: QueryMemory and SessionCreate report NO_ENCODER on valid config */
    size_t queried_size = 0x12345;
    if (porthole_encoder_query_memory(&cfg, &queried_size) != PORTHOLE_NO_ENCODER) {
        printf("FAIL: query_memory should report NO_ENCODER on host\n");
        failures++;
    }
    if (queried_size != 0) {
        printf("FAIL: query_memory should zero out_size on host\n");
        failures++;
    }
    if (porthole_encoder_query_memory(NULL, &queried_size) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: query_memory with null config was accepted\n");
        failures++;
    }

    if (porthole_encoder_session_create(&cfg) != PORTHOLE_NO_ENCODER) {
        printf("FAIL: session_create should report NO_ENCODER on host\n");
        failures++;
    }
    if (porthole_encoder_session_create(NULL) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: session_create with null config was accepted\n");
        failures++;
    }

    if (porthole_encoder_is_active() != 0) {
        printf("FAIL: encoder session should not be active on host\n");
        failures++;
    }

    const porthole_encoder_session *sess = porthole_encoder_get_session();
    if (sess == NULL || sess->handle != -1 || sess->session_active != 0) {
        printf("FAIL: encoder session state mismatch on host\n");
        failures++;
    }

    if (porthole_encoder_session_destroy() != PORTHOLE_OK) {
        printf("FAIL: session_destroy should succeed when inactive\n");
        failures++;
    }

    /* Controller Pad Subsystem Tests */
    porthole_pad_reset();
    if (porthole_pad_get_api() != NULL) {
        printf("FAIL: pad api should be NULL when unopened\n");
        failures++;
    }
    if (porthole_pad_open() != PORTHOLE_OK) {
        printf("FAIL: porthole_pad_open should succeed on host\n");
        failures++;
    }
    if (porthole_pad_get_api() == NULL) {
        printf("FAIL: pad api should not be NULL after open\n");
        failures++;
    }

    /* Null and bad slot rejection */
    if (porthole_pad_apply(NULL) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: null pad record was accepted\n");
        failures++;
    }
    porthole_pad test_pad;
    test_pad.slot = 4;
    test_pad.sequence = 1;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: slot > 3 was accepted\n");
        failures++;
    }

    /* Sequence freshness check on slot 0 */
    test_pad.slot = 0;
    test_pad.buttons = 0;
    test_pad.sticks[0] = 128;
    test_pad.sticks[1] = 128;
    test_pad.sticks[2] = 128;
    test_pad.sticks[3] = 128;
    test_pad.triggers[0] = 0;
    test_pad.triggers[1] = 0;
    test_pad.sequence = 10;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: valid pad record sequence 10 failed\n");
        failures++;
    }

    /* Duplicate sequence (10) should be dropped as stale */
    test_pad.sequence = 10;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: duplicate sequence 10 failed\n");
        failures++;
    }

    /* Older sequence (9) should be dropped as stale */
    test_pad.sequence = 9;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: older sequence 9 failed\n");
        failures++;
    }

    /* Newer sequence (11) should succeed */
    test_pad.sequence = 11;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: newer sequence 11 failed\n");
        failures++;
    }

    /* Independent slot tracking: slot 1 at sequence 5 is fresh */
    test_pad.slot = 1;
    test_pad.sequence = 5;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: slot 1 sequence 5 failed\n");
        failures++;
    }

    /* Reset allows sequence to start over */
    porthole_pad_reset();
    test_pad.slot = 0;
    test_pad.sequence = 1;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: sequence 1 failed after reset\n");
        failures++;
    }

    /* Display Subsystem Tests (backed by oops-sdk) */
    porthole_display_close();
    if (porthole_display_get_framebuffer() != NULL) {
        printf("FAIL: display fb should be NULL when closed\n");
        failures++;
    }
    if (porthole_display_open() != PORTHOLE_OK) {
        printf("FAIL: porthole_display_open should succeed on host\n");
        failures++;
    }
    uint32_t *fb = porthole_display_get_framebuffer();
    if (fb == NULL) {
        printf("FAIL: display fb should not be NULL after open\n");
        failures++;
    } else {
        porthole_display_draw_test_pattern(0);
        /* Check that first pixel of top bar is white (0xFFFFFFFF) */
        if (fb[0] != 0xFFFFFFFFu) {
            printf("FAIL: test pattern pixel (0,0) was not white\n");
            failures++;
        }
        /* Check that pixel in yellow bar (x=300) is yellow (0xFFFFFF00) */
        if (fb[300] != 0xFFFFFF00u) {
            printf("FAIL: test pattern pixel (300,0) was not yellow\n");
            failures++;
        }
    }
    if (porthole_display_flip() != 0) {
        printf("FAIL: display flip should succeed\n");
        failures++;
    }
    porthole_display_close();
    if (porthole_display_get_framebuffer() != NULL) {
        printf("FAIL: display fb should be NULL after close\n");
        failures++;
    }

    /* Video Capture / Encode Tests */
    uint8_t test_out[4096];
    size_t test_len = 0;
    if (porthole_capture_encode(NULL, sizeof(test_out), &test_len) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: capture with NULL buffer was accepted\n");
        failures++;
    }
    if (porthole_capture_encode(test_out, sizeof(test_out), NULL) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: capture with NULL len was accepted\n");
        failures++;
    }
    if (porthole_capture_encode(test_out, sizeof(test_out), &test_len) != PORTHOLE_NO_ENCODER) {
        printf("FAIL: capture should report NO_ENCODER while encoder is closed\n");
        failures++;
    }

    if (failures == 0) {
        printf("porthole selftest: ok (wire contract holds; display, video & pad subsystems verified)\n");
        return 0;
    }
    printf("porthole selftest: %u failure(s)\n", failures);
    return 1;
}
