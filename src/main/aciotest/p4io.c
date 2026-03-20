#define LOG_MODULE "aciotest-p4io"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <windows.h>

#include "aciotest/p4io.h"
#include "p4iodrv/device.h"
#include "util/log.h"

static void print_u32_bits(const char *label, uint32_t val, uint32_t prev)
{
    printf("  %s: ", label);
    for (int i = 31; i >= 0; i--) {
        uint32_t bit = (val >> i) & 1;
        uint32_t changed = ((val ^ prev) >> i) & 1;
        if (changed) {
            printf("\x1b[1;33m%u\x1b[0m", bit);
        } else {
            printf("%u", bit);
        }
        if (i % 8 == 0 && i > 0) {
            printf(" ");
        }
    }
    printf("  (0x%08X)\n", val);
}

static void print_bit_labels(void)
{
    printf("  bits:  ");
    for (int i = 31; i >= 0; i--) {
        printf("%d", i / 10);
        if (i % 8 == 0 && i > 0) {
            printf(" ");
        }
    }
    printf("\n");
    printf("  bits:  ");
    for (int i = 31; i >= 0; i--) {
        printf("%d", i % 10);
        if (i % 8 == 0 && i > 0) {
            printf(" ");
        }
    }
    printf("\n");
}

static void print_hex_bytes_changed(
    const uint8_t *data, const uint8_t *prev, size_t len, size_t prev_len)
{
    for (size_t i = 0; i < len; i++) {
        bool changed = i >= prev_len || data[i] != prev[i];

        if (changed) {
            printf("\x1b[1;33m%02X\x1b[0m", data[i]);
        } else {
            printf("%02X", data[i]);
        }

        if (i + 1 < len) {
            printf(" ");
        }
    }
}

static struct p4iodrv_ctx *open_p4io_or_print_error(void)
{
    log_info("Opening P4IO device...");

    struct p4iodrv_ctx *ctx = p4iodrv_open();

    if (!ctx) {
        printf(
            "Opening P4IO device failed. Is it connected and is the driver "
            "installed?\n");
        return NULL;
    }

    printf("Opening P4IO device successful, press ENTER to continue\n");
    getchar();

    return ctx;
}

static void print_common_jamma_summary(uint32_t j0)
{
    printf("\nKnown system bits (jamma[0], active HIGH):\n");
    printf("  bit 28 (coin):    %s\n", (j0 >> 28) & 1 ? "PRESSED" : "---");
    printf("  bit 25 (service): %s\n", (j0 >> 25) & 1 ? "PRESSED" : "---");
    printf("  bit 24 (test):    %s\n", (j0 >> 24) & 1 ? "PRESSED" : "---");

    printf("\nActive LOW bits in jamma[0] (often pad inputs):\n  ");

    bool any = false;
    for (int i = 0; i < 32; i++) {
        if (!((j0 >> i) & 1)) {
            printf("bit%d ", i);
            any = true;
        }
    }

    if (!any) {
        printf("(none)");
    }

    printf("\n");
}

static uint16_t decode_j32d_guess(uint16_t packed)
{
    /* Inverse of packed=((v&0x3f)<<10)|((v>>6)&0x0f), range approx 0..1023 */
    return (uint16_t) (((packed >> 10) & 0x3F) | ((packed & 0x0F) << 6));
}

static bool bit_low(uint32_t val, int bit)
{
    return !((val >> bit) & 1);
}

static bool bit_high(uint32_t val, int bit)
{
    return (val >> bit) & 1;
}

static uint32_t build_libextio_dm_input_guess(uint32_t jamma0)
{
    uint32_t v = 0;

    /* decomp clue: gfdm_unit_get_input carries nav on bits 5/6 */
    if (bit_high(jamma0, 17)) {
        v |= 1U << 5;
    }
    if (bit_high(jamma0, 18)) {
        v |= 1U << 6;
    }

    /* decomp clue: drum-related bits cluster around 7..8 and 10..17 */
    if (bit_low(jamma0, 0)) {
        v |= 1U << 15;
    }
    if (bit_low(jamma0, 1)) {
        v |= 1U << 7;
    }
    if (bit_low(jamma0, 2)) {
        v |= 1U << 10;
    }
    if (bit_low(jamma0, 3)) {
        v |= 1U << 11;
    }
    if (bit_low(jamma0, 4)) {
        v |= 1U << 12;
    }
    if (bit_low(jamma0, 5)) {
        v |= 1U << 14;
    }
    if (bit_low(jamma0, 6)) {
        v |= 1U << 16;
    }
    if (bit_low(jamma0, 7)) {
        v |= 1U << 17;
    }
    if (bit_low(jamma0, 8)) {
        v |= 1U << 8;
    }

    return v;
}

static void print_dm_input_guess_summary(uint32_t guess)
{
    struct flag_desc {
        uint8_t bit;
        const char *name;
    };

    static const struct flag_desc flags[] = {
        {5, "NAV_UP"},
        {6, "NAV_DOWN"},
        {7, "HIHAT"},
        {8, "LEFT_PEDAL"},
        {10, "SNARE"},
        {11, "HI_TOM"},
        {12, "LOW_TOM"},
        {14, "FLOOR_TOM"},
        {15, "LEFT_CYMBAL"},
        {16, "RIGHT_CYMBAL"},
        {17, "BASS_PEDAL"},
    };

    printf("  guess set flags:");

    bool any = false;
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
        if ((guess >> flags[i].bit) & 1) {
            printf(" %s", flags[i].name);
            any = true;
        }
    }

    if (!any) {
        printf(" (none)");
    }

    printf("\n");
}

struct dm_map_capture_entry {
    const char *label;
    const char *define_name;
    bool active_high;
};

static bool wait_enter(void)
{
    int ch;

    do {
        ch = getchar();
        if (ch == EOF) {
            return false;
        }
    } while (ch != '\n');

    return true;
}

static bool sample_jamma0_ones(
    struct p4iodrv_ctx *ctx, uint32_t ones[32], size_t nsamples)
{
    uint32_t jamma[4];
    size_t got = 0;
    size_t retries = 0;

    memset(ones, 0, sizeof(uint32_t) * 32);

    while (got < nsamples) {
        if (!p4iodrv_read_jamma(ctx, jamma)) {
            retries++;

            if (retries > nsamples * 4) {
                return false;
            }

            Sleep(2);
            continue;
        }

        uint32_t j0 = jamma[0];

        for (size_t bit = 0; bit < 32; bit++) {
            if ((j0 >> bit) & 1U) {
                ones[bit]++;
            }
        }

        got++;
        Sleep(2);
    }

    return true;
}

static bool ones_to_level(uint32_t ones, size_t nsamples)
{
    return ones > (nsamples / 2);
}

static bool write_map_output_file(
    const struct dm_map_capture_entry *entries,
    size_t nentries,
    const int *found_bits,
    char *path_out,
    size_t path_out_size)
{
    time_t now;
    struct tm *tm_now;
    FILE *fp;

    now = time(NULL);
    tm_now = localtime(&now);

    if (!tm_now) {
        return false;
    }

    if (snprintf(
            path_out,
            path_out_size,
            "p4io-map-%04d%02d%02d-%02d%02d%02d.txt",
            tm_now->tm_year + 1900,
            tm_now->tm_mon + 1,
            tm_now->tm_mday,
            tm_now->tm_hour,
            tm_now->tm_min,
            tm_now->tm_sec) >= (int) path_out_size) {
        return false;
    }

    fp = fopen(path_out, "w");
    if (!fp) {
        return false;
    }

    fprintf(fp, "# Generated by: aciotest p4io map save\n\n");

    for (size_t i = 0; i < nentries; i++) {
        if (found_bits[i] >= 0) {
            fprintf(fp, "#define %-22s %d\n", entries[i].define_name, found_bits[i]);
        } else {
            fprintf(fp, "#define %-22s /* unresolved */\n", entries[i].define_name);
        }
    }

    fclose(fp);
    return true;
}

void aciotest_p4io_run_map(bool save_to_file)
{
    static const struct dm_map_capture_entry entries[] = {
        {"Left Cymbal", "JAMMA_BIT_LEFT_CYMBAL", false},
        {"Hi-Hat", "JAMMA_BIT_HIHAT", false},
        {"Left Pedal", "JAMMA_BIT_LEFT_PEDAL", false},
        {"Snare", "JAMMA_BIT_SNARE", false},
        {"Hi Tom", "JAMMA_BIT_HI_TOM", false},
        {"Bass Pedal", "JAMMA_BIT_BASS_PEDAL", false},
        {"Low Tom", "JAMMA_BIT_LOW_TOM", false},
        {"Floor Tom", "JAMMA_BIT_FLOOR_TOM", false},
        {"Right Cymbal", "JAMMA_BIT_RIGHT_CYMBAL", false},
        {"Service", "JAMMA_BIT_SERVICE", true},
        {"Test", "JAMMA_BIT_TEST", true},
        {"Coin", "JAMMA_BIT_COIN", true},
        {"Start", "JAMMA_BIT_START", true},
        {"Up", "JAMMA_BIT_UP", true},
        {"Down", "JAMMA_BIT_DOWN", true},
        {"Left", "JAMMA_BIT_LEFT", true},
        {"Right", "JAMMA_BIT_RIGHT", true},
        {"Help", "JAMMA_BIT_HELP", true},
        {"Extra1", "JAMMA_BIT_EXTRA1", true},
        {"Extra2", "JAMMA_BIT_EXTRA2", true},
    };

    enum { NENTRIES = sizeof(entries) / sizeof(entries[0]) };
    enum { NSAMPLES = 64 };

    struct p4iodrv_ctx *ctx = open_p4io_or_print_error();
    uint32_t baseline_ones[32];
    uint32_t held_ones[32];
    int found_bits[NENTRIES];
    char out_path[128];

    if (!ctx) {
        return;
    }

    memset(found_bits, -1, sizeof(found_bits));

    printf("\nP4IO guided mapping capture\n");
    printf("Leave all controls released, then press ENTER to capture baseline.\n");

    if (!wait_enter()) {
        p4iodrv_close(ctx);
        return;
    }

    if (!sample_jamma0_ones(ctx, baseline_ones, NSAMPLES)) {
        printf("Failed to capture baseline JAMMA samples.\n");
        p4iodrv_close(ctx);
        return;
    }

    for (size_t i = 0; i < NENTRIES; i++) {
        uint32_t best_score = 0;
        int best_bit = -1;
        int matches = 0;

        printf(
            "\n[%u/%u] Hold %s (%s), then press ENTER...\n",
            (unsigned) (i + 1),
            (unsigned) NENTRIES,
            entries[i].label,
            entries[i].active_high ? "active HIGH" : "active LOW");

        if (!wait_enter()) {
            break;
        }

        if (!sample_jamma0_ones(ctx, held_ones, NSAMPLES)) {
            printf("  sample failed, skipping\n");
            continue;
        }

        for (size_t bit = 0; bit < 32; bit++) {
            bool base = ones_to_level(baseline_ones[bit], NSAMPLES);
            bool held = ones_to_level(held_ones[bit], NSAMPLES);
            bool match = false;
            uint32_t score;

            if (entries[i].active_high) {
                match = !base && held;
                score = held_ones[bit] + (NSAMPLES - baseline_ones[bit]);
            } else {
                match = base && !held;
                score = baseline_ones[bit] + (NSAMPLES - held_ones[bit]);
            }

            if (!match) {
                continue;
            }

            matches++;

            if (best_bit < 0 || score > best_score) {
                best_bit = (int) bit;
                best_score = score;
            }
        }

        found_bits[i] = best_bit;

        if (best_bit < 0) {
            printf("  no matching JAMMA bit detected\n");
        } else if (matches > 1) {
            printf(
                "  candidate JAMMA bit = %d (picked best out of %d matches)\n",
                best_bit,
                matches);
        } else {
            printf("  candidate JAMMA bit = %d\n", best_bit);
        }

        printf("Release %s, then press ENTER to continue.\n", entries[i].label);
        if (!wait_enter()) {
            break;
        }
    }

    printf("\nPaste-ready JAMMA defines for dmio-p4io/dmio.c:\n\n");

    for (size_t i = 0; i < NENTRIES; i++) {
        if (found_bits[i] >= 0) {
            printf("#define %-22s %d\n", entries[i].define_name, found_bits[i]);
        } else {
            printf("#define %-22s /* unresolved */\n", entries[i].define_name);
        }
    }

    printf("\nDone. Re-run p4io map if you want to refine unresolved lines.\n");

    if (save_to_file) {
        if (write_map_output_file(entries, NENTRIES, found_bits, out_path, sizeof(out_path))) {
            printf("Saved mapping defines to: %s\n", out_path);
        } else {
            printf("Failed to save mapping defines file.\n");
        }
    }

    p4iodrv_close(ctx);
}

void aciotest_p4io_run_digital(void)
{
    struct p4iodrv_ctx *ctx = open_p4io_or_print_error();
    if (!ctx) {
        return;
    }

    printf(
        "\nReading JAMMA data. Hit pads one at a time to discover bit "
        "mappings.\n");
    printf("Changed bits are highlighted in yellow. Press ESC to exit.\n\n");

    uint32_t prev[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    uint32_t jamma[4];
    uint32_t frame = 0;

    while (true) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            break;
        }

        if (!p4iodrv_read_jamma(ctx, jamma)) {
            log_warning("JAMMA read failed");
            Sleep(100);
            continue;
        }

        bool changed =
            (jamma[0] != prev[0] || jamma[1] != prev[1] ||
             jamma[2] != prev[2] || jamma[3] != prev[3]);

        if (changed || frame % 100 == 0) {
            system("cls");
            printf("P4IO JAMMA Input Monitor  [frame %u]\n", frame);
            printf("==============================================\n");
            print_bit_labels();
            print_u32_bits("jamma[0]", jamma[0], prev[0]);
            print_u32_bits("jamma[1]", jamma[1], prev[1]);
            print_u32_bits("jamma[2]", jamma[2], prev[2]);
            print_u32_bits("jamma[3]", jamma[3], prev[3]);
            print_common_jamma_summary(jamma[0]);

            prev[0] = jamma[0];
            prev[1] = jamma[1];
            prev[2] = jamma[2];
            prev[3] = jamma[3];
        }

        frame++;
        Sleep(10);
    }

    p4iodrv_close(ctx);
}

void aciotest_p4io_run_analog(void)
{
    struct p4iodrv_ctx *ctx = open_p4io_or_print_error();
    if (!ctx) {
        return;
    }

    if (!p4iodrv_cmd_sci_open(ctx)) {
        log_warning("SCI open failed, continuing with SCI polling attempts");
    }

    printf("\nP4IO analog discovery mode\n");
    printf("Reading JAMMA plus SCI_UPDATE port responses.\n");
    printf("Bytes that change are highlighted in yellow.\n");
    printf("Press ESC to exit.\n\n");

    uint32_t jamma[4] = {0};
    uint32_t jamma_prev[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    uint32_t dm_guess = 0;
    uint32_t dm_guess_prev = 0xFFFFFFFF;

    uint8_t req[2][1] = {{0}, {1}};
    uint8_t sci_buf[2][P4IO_MAX_PAYLOAD];
    uint8_t sci_prev[2][P4IO_MAX_PAYLOAD];
    uint8_t sci_min[2][P4IO_MAX_PAYLOAD];
    uint8_t sci_max[2][P4IO_MAX_PAYLOAD];
    size_t sci_len[2] = {0, 0};
    size_t sci_prev_len[2] = {0, 0};

    memset(sci_buf, 0, sizeof(sci_buf));
    memset(sci_prev, 0, sizeof(sci_prev));
    memset(sci_min, 0xFF, sizeof(sci_min));
    memset(sci_max, 0x00, sizeof(sci_max));

    uint32_t frame = 0;

    while (true) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            break;
        }

        if (!p4iodrv_read_jamma(ctx, jamma)) {
            log_warning("JAMMA read failed");
            Sleep(50);
            continue;
        }

        bool sci_ok[2] = {false, false};

        for (size_t port = 0; port < 2; port++) {
            size_t len = 0;
            sci_ok[port] = p4iodrv_cmd_sci_update(
                ctx,
                req[port],
                sizeof(req[port]),
                sci_buf[port],
                sizeof(sci_buf[port]),
                &len);
            sci_len[port] = len;

            if (sci_ok[port]) {
                for (size_t i = 0; i < len; i++) {
                    if (sci_buf[port][i] < sci_min[port][i]) {
                        sci_min[port][i] = sci_buf[port][i];
                    }
                    if (sci_buf[port][i] > sci_max[port][i]) {
                        sci_max[port][i] = sci_buf[port][i];
                    }
                }
            }
        }

        bool jamma_changed =
            (jamma[0] != jamma_prev[0] || jamma[1] != jamma_prev[1] ||
             jamma[2] != jamma_prev[2] || jamma[3] != jamma_prev[3]);

        dm_guess = build_libextio_dm_input_guess(jamma[0]);

        bool sci_changed = false;
        for (size_t port = 0; port < 2; port++) {
            if (!sci_ok[port] || sci_len[port] != sci_prev_len[port]) {
                sci_changed = true;
                break;
            }

            if (memcmp(sci_buf[port], sci_prev[port], sci_len[port]) != 0) {
                sci_changed = true;
                break;
            }
        }

        if (jamma_changed || sci_changed || frame % 60 == 0) {
            system("cls");

            printf("P4IO Analog Discovery Monitor  [frame %u]\n", frame);
            printf(
                "=============================================================="
                "\n");
            print_bit_labels();
            print_u32_bits("jamma[0]", jamma[0], jamma_prev[0]);
            print_u32_bits("jamma[1]", jamma[1], jamma_prev[1]);
            print_u32_bits("jamma[2]", jamma[2], jamma_prev[2]);
            print_u32_bits("jamma[3]", jamma[3], jamma_prev[3]);
            print_common_jamma_summary(jamma[0]);

            printf("\nlibextio-like DM input guess (from decomp + JAMMA):\n");
            print_u32_bits("guess", dm_guess, dm_guess_prev);
            print_dm_input_guess_summary(dm_guess);

            printf("\nSCI_UPDATE snapshots:\n");

            for (size_t port = 0; port < 2; port++) {
                printf(
                    "  port %u: %s, len=%u\n",
                    (unsigned) port,
                    sci_ok[port] ? "OK" : "FAILED",
                    (unsigned) sci_len[port]);

                if (!sci_ok[port] || sci_len[port] == 0) {
                    continue;
                }

                printf("    raw: ");
                print_hex_bytes_changed(
                    sci_buf[port],
                    sci_prev[port],
                    sci_len[port],
                    sci_prev_len[port]);
                printf("\n");

                if (sci_len[port] > 2) {
                    printf(
                        "    candidate changing channels (byte index -> cur "
                        "[min..max]):\n");
                    printf("      ");

                    bool any = false;
                    for (size_t i = 2; i < sci_len[port]; i++) {
                        if (sci_min[port][i] != sci_max[port][i]) {
                            printf(
                                "b%02u=%3u[%3u..%3u] ",
                                (unsigned) i,
                                (unsigned) sci_buf[port][i],
                                (unsigned) sci_min[port][i],
                                (unsigned) sci_max[port][i]);
                            any = true;
                        }
                    }

                    if (!any) {
                        printf("(no variation yet)");
                    }
                    printf("\n");
                }
            }

            memcpy(jamma_prev, jamma, sizeof(jamma_prev));
            for (size_t port = 0; port < 2; port++) {
                memcpy(sci_prev[port], sci_buf[port], sci_len[port]);
                sci_prev_len[port] = sci_len[port];
            }
        }

        frame++;
        Sleep(10);
    }

    p4iodrv_cmd_sci_close(ctx);
    p4iodrv_close(ctx);
}

struct sci_probe_pattern {
    const char *name;
    uint8_t req[4];
    size_t req_len;
};

void aciotest_p4io_run_analog2(void)
{
    static const struct sci_probe_pattern patterns[] = {
        {"port0-empty", {0x00}, 1},
        {"port1-empty", {0x01}, 1},
        {"port0-zero", {0x00, 0x00}, 2},
        {"port0-subcmd-20", {0x00, 0x20}, 2},
        {"port0-subcmd-21", {0x00, 0x21}, 2},
        {"port0-subcmd-2f", {0x00, 0x2F}, 2},
        {"port0-subcmd-12f-le", {0x00, 0x2F, 0x01, 0x00}, 4},
    };

    enum { NPAT = sizeof(patterns) / sizeof(patterns[0]) };

    struct p4iodrv_ctx *ctx = open_p4io_or_print_error();
    if (!ctx) {
        return;
    }

    if (!p4iodrv_cmd_sci_open(ctx)) {
        log_warning("SCI open failed, continuing with SCI polling attempts");
    }

    printf("\nP4IO analog discovery mode #2 (spice-inspired guesses)\n");
    printf(
        "Probing SCI_UPDATE with multiple payload forms from DDR/gitadora "
        "clues.\n");
    printf("Press ESC to exit.\n\n");

    uint32_t jamma[4] = {0};
    uint32_t jamma_prev[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    uint32_t dm_guess = 0;
    uint32_t dm_guess_prev = 0xFFFFFFFF;

    uint8_t sci_buf[NPAT][P4IO_MAX_PAYLOAD];
    uint8_t sci_prev[NPAT][P4IO_MAX_PAYLOAD];
    uint8_t sci_min[NPAT][P4IO_MAX_PAYLOAD];
    uint8_t sci_max[NPAT][P4IO_MAX_PAYLOAD];
    size_t sci_len[NPAT];
    size_t sci_prev_len[NPAT];
    bool sci_ok[NPAT];

    memset(sci_buf, 0, sizeof(sci_buf));
    memset(sci_prev, 0, sizeof(sci_prev));
    memset(sci_min, 0xFF, sizeof(sci_min));
    memset(sci_max, 0x00, sizeof(sci_max));
    memset(sci_len, 0, sizeof(sci_len));
    memset(sci_prev_len, 0, sizeof(sci_prev_len));
    memset(sci_ok, 0, sizeof(sci_ok));

    uint32_t frame = 0;

    while (true) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            break;
        }

        if (!p4iodrv_read_jamma(ctx, jamma)) {
            log_warning("JAMMA read failed");
            Sleep(50);
            continue;
        }

        for (size_t p = 0; p < NPAT; p++) {
            size_t len = 0;
            sci_ok[p] = p4iodrv_cmd_sci_update(
                ctx,
                patterns[p].req,
                patterns[p].req_len,
                sci_buf[p],
                sizeof(sci_buf[p]),
                &len);
            sci_len[p] = len;

            if (sci_ok[p]) {
                for (size_t i = 0; i < len; i++) {
                    if (sci_buf[p][i] < sci_min[p][i]) {
                        sci_min[p][i] = sci_buf[p][i];
                    }
                    if (sci_buf[p][i] > sci_max[p][i]) {
                        sci_max[p][i] = sci_buf[p][i];
                    }
                }
            }
        }

        bool jamma_changed =
            (jamma[0] != jamma_prev[0] || jamma[1] != jamma_prev[1] ||
             jamma[2] != jamma_prev[2] || jamma[3] != jamma_prev[3]);

        dm_guess = build_libextio_dm_input_guess(jamma[0]);

        bool any_changed = false;
        for (size_t p = 0; p < NPAT; p++) {
            if (!sci_ok[p] || sci_len[p] != sci_prev_len[p] ||
                memcmp(sci_buf[p], sci_prev[p], sci_len[p]) != 0) {
                any_changed = true;
                break;
            }
        }

        if (jamma_changed || any_changed || frame % 60 == 0) {
            system("cls");

            printf("P4IO Analog Discovery Monitor #2  [frame %u]\n", frame);
            printf(
                "=============================================================="
                "\n");
            print_bit_labels();
            print_u32_bits("jamma[0]", jamma[0], jamma_prev[0]);
            print_u32_bits("jamma[1]", jamma[1], jamma_prev[1]);
            print_u32_bits("jamma[2]", jamma[2], jamma_prev[2]);
            print_u32_bits("jamma[3]", jamma[3], jamma_prev[3]);
            print_common_jamma_summary(jamma[0]);

            printf("\nlibextio-like DM input guess (from decomp + JAMMA):\n");
            print_u32_bits("guess", dm_guess, dm_guess_prev);
            print_dm_input_guess_summary(dm_guess);

            printf("\nSCI_UPDATE guess probes:\n");
            for (size_t p = 0; p < NPAT; p++) {
                printf(
                    "  [%u] %-16s reqlen=%u -> %s len=%u\n",
                    (unsigned) p,
                    patterns[p].name,
                    (unsigned) patterns[p].req_len,
                    sci_ok[p] ? "OK" : "FAILED",
                    (unsigned) sci_len[p]);

                if (!sci_ok[p] || sci_len[p] == 0) {
                    continue;
                }

                printf("      raw: ");
                print_hex_bytes_changed(
                    sci_buf[p], sci_prev[p], sci_len[p], sci_prev_len[p]);
                printf("\n");

                if (sci_len[p] >= 4) {
                    printf("      16-bit candidates (offset>=2): ");

                    bool any_word = false;
                    for (size_t i = 2; i + 1 < sci_len[p] && i < 18; i += 2) {
                        uint16_t raw = (uint16_t) (sci_buf[p][i] |
                                                   (sci_buf[p][i + 1] << 8));
                        uint16_t dec = decode_j32d_guess(raw);

                        printf(
                            "w%u=0x%04X->%u ",
                            (unsigned) ((i - 2) / 2),
                            raw,
                            (unsigned) dec);
                        any_word = true;
                    }

                    if (!any_word) {
                        printf("(none)");
                    }
                    printf("\n");
                }

                printf("      changing bytes: ");
                bool any = false;
                for (size_t i = 0; i < sci_len[p]; i++) {
                    if (sci_min[p][i] != sci_max[p][i]) {
                        printf(
                            "b%02u=%3u[%3u..%3u] ",
                            (unsigned) i,
                            (unsigned) sci_buf[p][i],
                            (unsigned) sci_min[p][i],
                            (unsigned) sci_max[p][i]);
                        any = true;
                    }
                }
                if (!any) {
                    printf("(no variation yet)");
                }
                printf("\n");
            }

            memcpy(jamma_prev, jamma, sizeof(jamma_prev));
            dm_guess_prev = dm_guess;
            for (size_t p = 0; p < NPAT; p++) {
                memcpy(sci_prev[p], sci_buf[p], sci_len[p]);
                sci_prev_len[p] = sci_len[p];
            }
        }

        frame++;
        Sleep(10);
    }

    p4iodrv_cmd_sci_close(ctx);
    p4iodrv_close(ctx);
}
