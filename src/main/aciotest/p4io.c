#define LOG_MODULE "aciotest-p4io"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <windows.h>

#include "aciotest/p4io.h"
#include "p4iodrv/device.h"
#include "util/log.h"

static void print_u32_bits(const char *label, uint32_t val, uint32_t prev)
{
    printf("  %s: ", label);
    for (int i = 31; i >= 0; i--)
    {
        uint32_t bit = (val >> i) & 1;
        uint32_t changed = ((val ^ prev) >> i) & 1;
        if (changed)
        {
            printf("\x1b[1;33m%u\x1b[0m", bit);
        }
        else
        {
            printf("%u", bit);
        }
        if (i % 8 == 0 && i > 0)
        {
            printf(" ");
        }
    }
    printf("  (0x%08X)\n", val);
}

static void print_bit_labels(void)
{
    printf("  bits:  ");
    for (int i = 31; i >= 0; i--)
    {
        printf("%d", i / 10);
        if (i % 8 == 0 && i > 0)
        {
            printf(" ");
        }
    }
    printf("\n");
    printf("  bits:  ");
    for (int i = 31; i >= 0; i--)
    {
        printf("%d", i % 10);
        if (i % 8 == 0 && i > 0)
        {
            printf(" ");
        }
    }
    printf("\n");
}

void aciotest_p4io_run(void)
{
    log_info("Opening P4IO device...");

    struct p4iodrv_ctx *ctx = p4iodrv_open();

    if (!ctx)
    {
        printf(
            "Opening P4IO device failed. Is it connected and is the driver "
            "installed?\n");
        return;
    }

    printf("Opening P4IO device successful, press ENTER to continue\n");
    getchar();

    printf(
        "\nReading JAMMA data. Hit pads one at a time to discover bit "
        "mappings.\n");
    printf(
        "Changed bits are highlighted in yellow. Press Ctrl-C to exit.\n\n");

    uint32_t prev[4] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    uint32_t jamma[4];
    uint32_t frame = 0;

    while (true)
    {
        if (!p4iodrv_read_jamma(ctx, jamma))
        {
            log_warning("JAMMA read failed");
            Sleep(100);
            continue;
        }

        bool changed = (jamma[0] != prev[0] || jamma[1] != prev[1] ||
                        jamma[2] != prev[2] || jamma[3] != prev[3]);

        if (changed || frame % 100 == 0)
        {
            system("cls");
            printf("P4IO JAMMA Input Monitor  [frame %u]\n", frame);
            printf("==============================================\n");
            print_bit_labels();
            print_u32_bits("jamma[0]", jamma[0], prev[0]);
            print_u32_bits("jamma[1]", jamma[1], prev[1]);
            print_u32_bits("jamma[2]", jamma[2], prev[2]);
            print_u32_bits("jamma[3]", jamma[3], prev[3]);
            printf("\n");

            /* Known system bits from Jubeat research (active HIGH in jamma[0]) */
            printf("Known system bits (jamma[0], active HIGH):\n");
            printf(
                "  bit 28 (coin):    %s\n",
                (jamma[0] >> 28) & 1 ? "PRESSED" : "---");
            printf(
                "  bit 25 (service): %s\n",
                (jamma[0] >> 25) & 1 ? "PRESSED" : "---");
            printf(
                "  bit 24 (test):    %s\n",
                (jamma[0] >> 24) & 1 ? "PRESSED" : "---");
            printf("\n");

            printf("Active LOW bits in jamma[0] (likely drum pads):\n  ");
            bool any = false;
            for (int i = 0; i < 32; i++)
            {
                if (!((jamma[0] >> i) & 1))
                {
                    printf("bit%d ", i);
                    any = true;
                }
            }
            if (!any)
            {
                printf("(none)");
            }
            printf("\n");

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
