/**
 * spi_c_decoder.c — Example C decoder: Serial Peripheral Interface (SPI)
 *
 * Decodes SPI Mode 0 (CPOL=0, CPHA=0): samples MOSI and MISO on the rising
 * edge of CLK while CS is asserted (active-low).
 *
 * Build (macOS):
 *   clang -shared -fPIC -O2 -o spi.dylib spi_c_decoder.c
 *
 * Build (Linux):
 *   gcc -shared -fPIC -O2 -o spi.so spi_c_decoder.c
 *
 * Install:
 *   cp spi.dylib (or spi.so) $APP_DATA_DIR/cdecoders/
 *   (e.g. ~/.local/share/DSView/cdecoders/ on Linux)
 *
 * Annotation layout:
 *   Row 0 "bits"   — individual decoded bits (mosi and miso, interleaved)
 *   Row 1 "data"   — assembled bytes (mosi and miso)
 *
 * Annotation classes (within each row):
 *   bits row  — class 0: "mosi_bit",  class 1: "miso_bit"
 *   data row  — class 0: "mosi_byte", class 1: "miso_byte"
 */

#include "../c_decoder_api.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Channel indices */
#define CH_CLK  0
#define CH_MOSI 1
#define CH_MISO 2
#define CH_CS   3

/* Annotation row indices */
#define ROW_BITS 0
#define ROW_DATA 1

/* Annotation class indices within ROW_BITS */
#define CLASS_BITS_MOSI 0
#define CLASS_BITS_MISO 1

/* Annotation class indices within ROW_DATA */
#define CLASS_DATA_MOSI 0
#define CLASS_DATA_MISO 1

/* Stop every ~100k samples to check for abort */
#define STOP_POLL_INTERVAL 100000

static int spi_decode(
    uint64_t samplerate,
    uint64_t start_sample,
    uint64_t end_sample,
    int       num_channels,
    const uint8_t **ch,
    void (*put_annotation)(void *ctx,
                           uint64_t ss, uint64_t es,
                           unsigned int ann_row, unsigned int ann_class,
                           const char *text),
    void *ctx,
    volatile int *stop_flag)
{
    (void)samplerate;

    if (num_channels < 4)
        return -1;  /* Need CLK, MOSI, MISO, CS */

    uint8_t prev_clk = ch[CH_CLK][0];
    uint8_t prev_cs  = ch[CH_CS][0];

    uint8_t mosi_byte = 0;
    uint8_t miso_byte = 0;
    int     bit_count = 0;
    uint64_t byte_start = start_sample;

    char text_buf[64];
    uint64_t num_samples = end_sample - start_sample + 1;

    for (uint64_t i = 1; i < num_samples; i++) {
        if ((i % STOP_POLL_INTERVAL) == 0 && *stop_flag)
            return 0;

        uint64_t abs = start_sample + i;
        uint8_t cur_clk  = ch[CH_CLK][i];
        uint8_t cur_cs   = ch[CH_CS][i];
        uint8_t cur_mosi = ch[CH_MOSI][i];
        uint8_t cur_miso = ch[CH_MISO][i];

        /* CS deasserted — reset state */
        if (cur_cs && !prev_cs) {
            bit_count = 0;
            mosi_byte = 0;
            miso_byte = 0;
        }

        /* Rising CLK edge while CS is asserted (active-low, so cs==0) */
        if (cur_clk && !prev_clk && !cur_cs) {
            uint64_t bit_start = abs - 1;
            uint64_t bit_end   = abs;

            if (bit_count == 0)
                byte_start = bit_start;

            /* Emit individual bits */
            snprintf(text_buf, sizeof(text_buf), "%u", cur_mosi);
            put_annotation(ctx, bit_start, bit_end, ROW_BITS, CLASS_BITS_MOSI, text_buf);

            snprintf(text_buf, sizeof(text_buf), "%u", cur_miso);
            put_annotation(ctx, bit_start, bit_end, ROW_BITS, CLASS_BITS_MISO, text_buf);

            /* MSB first */
            mosi_byte = (uint8_t)((mosi_byte << 1) | (cur_mosi & 1));
            miso_byte = (uint8_t)((miso_byte << 1) | (cur_miso & 1));
            bit_count++;

            if (bit_count == 8) {
                /* Emit assembled bytes */
                snprintf(text_buf, sizeof(text_buf), "MOSI: 0x%02X", mosi_byte);
                put_annotation(ctx, byte_start, bit_end, ROW_DATA, CLASS_DATA_MOSI, text_buf);

                snprintf(text_buf, sizeof(text_buf), "MISO: 0x%02X", miso_byte);
                put_annotation(ctx, byte_start, bit_end, ROW_DATA, CLASS_DATA_MISO, text_buf);

                mosi_byte = 0;
                miso_byte = 0;
                bit_count = 0;
            }
        }

        prev_clk = cur_clk;
        prev_cs  = cur_cs;
    }

    return 0;
}

/* --- Channel declarations --- */
static const char *ch_ids[]   = { "CLK",  "MOSI",  "MISO",  "CS",  NULL };
static const char *ch_names[] = { "Clock", "MOSI", "MISO", "Chip Select", NULL };

/* --- Annotation row declarations --- */
static const char *row_ids[]   = { "bits",       "data",       NULL };
static const char *row_descs[] = { "Individual bits", "Decoded bytes", NULL };

/* --- Annotation classes (row_id:class_label) --- */
static const char *ann_classes[] = {
    "bits:mosi_bit",
    "bits:miso_bit",
    "data:mosi_byte",
    "data:miso_byte",
    NULL
};

/* --- Exported decoder definition --- */
CDecoderDef c_decoder_def = {
    .api_version  = C_DECODER_API_VERSION,
    .id           = "spi",
    .name         = "SPI",
    .longname     = "Serial Peripheral Interface",
    .desc         = "SPI Mode 0 (CPOL=0, CPHA=0) C decoder example",
    .channel_ids  = ch_ids,
    .channel_names = ch_names,
    .ann_row_ids  = row_ids,
    .ann_row_descs = row_descs,
    .ann_classes  = ann_classes,
    .decode       = spi_decode,
};
