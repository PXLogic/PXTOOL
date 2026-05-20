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
#include <stdlib.h>
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

/* ------------------------------------------------------------------------- */
/* Per-run state shared between streaming (v2) and batch (v1) paths          */
/* ------------------------------------------------------------------------- */

typedef struct spi_state {
    uint8_t  prev_clk;
    uint8_t  prev_cs;
    uint8_t  mosi_byte;
    uint8_t  miso_byte;
    int      bit_count;
    uint64_t byte_start;
    int      primed;         /* false until we see the first sample of the run */
} spi_state;

static void *spi_create(uint64_t samplerate, int num_channels)
{
    (void)samplerate;
    if (num_channels < 4) return NULL;       /* same guard as v1 entry */
    spi_state *st = (spi_state*)calloc(1, sizeof(*st));
    return st;                                /* may be NULL — host handles it */
}

static void spi_destroy(void *inst)
{
    free(inst);
}

/* Core per-sample loop, shared by the v1 and v2 entries. */
static int spi_decode_samples(spi_state *st,
                              uint64_t chunk_start,
                              uint64_t chunk_len,
                              const uint8_t **ch,
                              void (*put_annotation)(void *ctx,
                                                     uint64_t ss, uint64_t es,
                                                     unsigned int ann_row,
                                                     unsigned int ann_class,
                                                     const char *text),
                              void *ctx,
                              volatile int *stop_flag)
{
    char text_buf[64];
    uint64_t i_start = 0;

    /* On the very first sample we ever see in this run, latch baseline
     * levels so the first sample doesn't fabricate a CLK edge. */
    if (!st->primed) {
        if (chunk_len == 0) return 0;
        st->prev_clk = ch[CH_CLK][0];
        st->prev_cs  = ch[CH_CS][0];
        st->primed   = 1;
        i_start = 1;                          /* skip the priming sample */
    }

    for (uint64_t i = i_start; i < chunk_len; i++) {
        if ((i % STOP_POLL_INTERVAL) == 0 && *stop_flag)
            return 0;

        uint64_t abs     = chunk_start + i;
        uint8_t cur_clk  = ch[CH_CLK][i];
        uint8_t cur_cs   = ch[CH_CS][i];
        uint8_t cur_mosi = ch[CH_MOSI][i];
        uint8_t cur_miso = ch[CH_MISO][i];

        /* CS deasserted — reset state */
        if (cur_cs && !st->prev_cs) {
            st->bit_count = 0;
            st->mosi_byte = 0;
            st->miso_byte = 0;
        }

        /* Rising CLK edge while CS is asserted (active-low, so cs==0) */
        if (cur_clk && !st->prev_clk && !cur_cs) {
            uint64_t bit_start = abs - 1;
            uint64_t bit_end   = abs;

            if (st->bit_count == 0)
                st->byte_start = bit_start;

            /* Emit individual bits */
            snprintf(text_buf, sizeof(text_buf), "%u", cur_mosi);
            put_annotation(ctx, bit_start, bit_end, ROW_BITS, CLASS_BITS_MOSI, text_buf);

            snprintf(text_buf, sizeof(text_buf), "%u", cur_miso);
            put_annotation(ctx, bit_start, bit_end, ROW_BITS, CLASS_BITS_MISO, text_buf);

            /* MSB first */
            st->mosi_byte = (uint8_t)((st->mosi_byte << 1) | (cur_mosi & 1));
            st->miso_byte = (uint8_t)((st->miso_byte << 1) | (cur_miso & 1));
            st->bit_count++;

            if (st->bit_count == 8) {
                /* Emit assembled bytes.
                 *
                 * Format string mirrors the Python decoder's `"%02x"` (lower
                 * case, 2 digits, no "0x"/"MOSI: " prefix) -- the
                 * MOSI vs MISO distinction is already conveyed by the
                 * annotation row, so a prefix on the text just makes
                 * cross-engine log diffs noisy. See pd.py:222 / pd.py:209.
                 */
                snprintf(text_buf, sizeof(text_buf), "%02x", st->mosi_byte);
                put_annotation(ctx, st->byte_start, bit_end, ROW_DATA, CLASS_DATA_MOSI, text_buf);

                snprintf(text_buf, sizeof(text_buf), "%02x", st->miso_byte);
                put_annotation(ctx, st->byte_start, bit_end, ROW_DATA, CLASS_DATA_MISO, text_buf);

                st->mosi_byte = 0;
                st->miso_byte = 0;
                st->bit_count = 0;
            }
        }

        st->prev_clk = cur_clk;
        st->prev_cs  = cur_cs;
    }

    return 0;
}

/* v2 streaming entry — called repeatedly with non-overlapping chunks; state
 * persists across calls via inst. */
static int spi_decode_chunk(void *inst,
                            uint64_t chunk_start, uint64_t chunk_end,
                            int num_channels,
                            const uint8_t **ch,
                            void (*put_annotation)(void *ctx,
                                                   uint64_t ss, uint64_t es,
                                                   unsigned int ann_row,
                                                   unsigned int ann_class,
                                                   const char *text),
                            void *ctx,
                            volatile int *stop_flag)
{
    if (num_channels < 4) return -1;
    if (chunk_end < chunk_start) return -1;
    spi_state *st = (spi_state*)inst;
    uint64_t len = chunk_end - chunk_start + 1;
    return spi_decode_samples(st, chunk_start, len, ch,
                              put_annotation, ctx, stop_flag);
}

/* v1 entry — kept so older hosts (or batch-mode callers) still work.
 * Allocates a transient state, runs the chunk loop once over the whole
 * range, and frees it. */
static int spi_decode(uint64_t samplerate,
                      uint64_t start_sample, uint64_t end_sample,
                      int       num_channels,
                      const uint8_t **ch,
                      void (*put_annotation)(void *ctx,
                                             uint64_t ss, uint64_t es,
                                             unsigned int ann_row,
                                             unsigned int ann_class,
                                             const char *text),
                      void *ctx,
                      volatile int *stop_flag)
{
    (void)samplerate;
    if (num_channels < 4) return -1;
    spi_state st;
    memset(&st, 0, sizeof(st));
    uint64_t len = end_sample - start_sample + 1;
    return spi_decode_samples(&st, start_sample, len, ch,
                              put_annotation, ctx, stop_flag);
}

/* --- Channel declarations --- */
static const char *ch_ids[]   = { "CLK",  "MOSI",  "MISO",  "CS",  NULL };
static const char *ch_names[] = { "Clock", "MOSI", "MISO", "Chip Select", NULL };

/* --- Annotation row declarations --- */
static const char *row_ids[]   = { "bits",       "data",       NULL };
static const char *row_descs[] = { "Individual bits", "Decoded bytes", NULL };

/* --- Annotation classes (row_id:class_label) ---
 * The label after the ':' MUST match the id of an entry in the partnering
 * Python decoder's `annotations` tuple (case-insensitive, ignoring '-' / '_'
 * separators). The host walks the Python decoder's annotation tuple to
 * translate these into row routing — if the labels don't match, every
 * annotation falls back to a positional class index and ends up in the wrong
 * row (or a hidden bits/warning row, where it becomes invisible). See the
 * "Annotation routing contract" section in README.md. */
static const char *ann_classes[] = {
    "bits:mosi-bit",
    "bits:miso-bit",
    "data:mosi-data",
    "data:miso-data",
    NULL
};

/* --- Exported decoder definition --- */
CDecoderDef c_decoder_def = {
    .api_version   = C_DECODER_API_VERSION,   /* now 2 */
    .id            = "spi",
    .name          = "SPI",
    .longname      = "Serial Peripheral Interface",
    .desc          = "SPI Mode 0 (CPOL=0, CPHA=0) C decoder example",
    .channel_ids   = ch_ids,
    .channel_names = ch_names,
    .ann_row_ids   = row_ids,
    .ann_row_descs = row_descs,
    .ann_classes   = ann_classes,
    .decode        = spi_decode,        /* batch wrapper (kept) */
    .create        = spi_create,        /* v2 streaming */
    .decode_chunk  = spi_decode_chunk,  /* v2 streaming */
    .destroy       = spi_destroy,       /* v2 streaming */
};
