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

/* **The frame buffer and the bitrate must not drift apart.** Raising the configured rate
 * without raising the buffer starts refusing keyframes, and a stream of dependent pictures
 * decodes to nothing while looking exactly like no stream at all - the one fault the host
 * cannot tell from a dead socket. Tying them together makes that a build failure instead. */
_Static_assert(PORTHOLE_FRAME_BYTES * 8u >= PORTHOLE_DEFAULT_BITRATE,
               "one access unit needs a second of the configured bitrate to sit in");
_Static_assert(PORTHOLE_FRAME_BYTES >= 64u * 1024u,
               "a 1080p keyframe needs room whatever a low bitrate would imply");

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

    /* **A version this does not read is refused, not interpreted.** The reserved bytes are
     * where a later version puts gyro or the touchpad, so reading a newer record through this
     * layout would take a real value for a reserved zero. The host half refuses a mismatch
     * and this side did not, which left the version field doing nothing on the receiving end.
     */
    good_record(bytes);
    bytes[4] = 2; /* version 2, low byte */
    if (porthole_pad_decode(bytes, &pad) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: a record of another version was accepted\n");
        failures++;
    }
    good_record(bytes);
    bytes[5] = 1; /* version 256, so the high byte is checked too */
    if (porthole_pad_decode(bytes, &pad) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: only the low byte of the version was checked\n");
        failures++;
    }
    good_record(bytes);
    bytes[4] = 0; /* version 0 is not version 1 either */
    if (porthole_pad_decode(bytes, &pad) != PORTHOLE_BAD_RECORD) {
        printf("FAIL: a zero version was accepted\n");
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
    /* **A host build refuses; a payload does not.** On the target the encoder is attempted
     * and not required, because the template stream needs none - but a host has no target to
     * serve and would bind real ports if it tried, so it stops here rather than starting a
     * server inside a test binary. */
    if (porthole_run() != PORTHOLE_NO_ENCODER) {
        printf("FAIL: run() should refuse cleanly on a host, which has nothing to serve\n");
        failures++;
    }

    /* The struct-taking encoder calls are gated off unless a build asks for them: a first
     * hardware run should exercise the sockets and the template stream and nothing that could
     * corrupt the stack, since D300 reserved those layouts for M2. The default is the gate. */
    if (porthole_encoder_session_enabled() != 0) {
        printf("FAIL: the encoder session gate must be off by default\n");
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

    /* A duplicate sequence (10) is superseded, and says so rather than looking applied. */
    test_pad.sequence = 10;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_STALE) {
        printf("FAIL: duplicate sequence 10 was not reported stale\n");
        failures++;
    }

    /* An older sequence (9) likewise. */
    test_pad.sequence = 9;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_STALE) {
        printf("FAIL: older sequence 9 was not reported stale\n");
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

    /* A new input connection is a new sender, whose count starts over. A host that restarted
     * begins again at 1, and judged against the previous sender's last sequence every record
     * it sent would be stale until the count climbed past - input that silently does nothing.
     * Resequencing forgets the count, and only the count. */
    test_pad.sequence = 500;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: sequence 500 failed\n");
        failures++;
    }
    test_pad.sequence = 3;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_STALE) {
        printf("FAIL: sequence 3 behind 500 was not reported stale\n");
        failures++;
    }
    porthole_pad_resequence();
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: sequence 3 from a new connection was dropped as stale\n");
        failures++;
    }
    if (porthole_pad_apply(&test_pad) != PORTHOLE_STALE) {
        printf("FAIL: freshness is not checked again after a resequence\n");
        failures++;
    }
    /* Every slot forgets, not only the one that last spoke. */
    test_pad.slot = 1;
    test_pad.sequence = 40;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: slot 1 sequence 40 failed\n");
        failures++;
    }
    porthole_pad_resequence();
    test_pad.sequence = 2;
    if (porthole_pad_apply(&test_pad) != PORTHOLE_OK) {
        printf("FAIL: slot 1 was not resequenced\n");
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
    if (porthole_display_is_gpu_accelerated() != 0) {
        printf("FAIL: porthole display should not be GPU accelerated on host\n");
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
    /* **With the session gated off the template stream is the video path**, and needs no
     * encoder at all. That is the whole point of the gate: it is what a first hardware run
     * serves, so it has to be well-formed Annex-B opening on something a decoder can begin
     * from. Capture refusing here was what kept a gated payload from serving anything. */
    if (porthole_capture_encode(test_out, sizeof(test_out), &test_len) != PORTHOLE_OK) {
        printf("FAIL: the template stream should serve with the session gated off\n");
        failures++;
    } else if (test_len != 38u) {
        printf("FAIL: the first template unit should be SPS+PPS+IDR, 38 bytes, got %u\n",
               (unsigned int)test_len);
        failures++;
    } else if (test_out[0] != 0 || test_out[1] != 0 || test_out[2] != 0 || test_out[3] != 1 ||
               (test_out[4] & 0x1Fu) != 7u) {
        printf("FAIL: the template must open with a start code and sequence parameters\n");
        failures++;
    } else if ((test_out[18] & 0x1Fu) != 8u || (test_out[26] & 0x1Fu) != 5u) {
        printf("FAIL: the template keyframe needs picture parameters and an IDR after them\n");
        failures++;
    }

    if (failures == 0) {
        printf("porthole selftest: ok (wire contract holds; display, video & pad subsystems verified)\n");
        return 0;
    }
    printf("porthole selftest: %u failure(s)\n", failures);
    return 1;
}
