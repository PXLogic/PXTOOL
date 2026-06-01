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
 *   Row 0 "bits"            — individual decoded bits (mosi and miso)
 *   Row 1 "data"            — assembled bytes        (mosi and miso)
 *   Row 2 "mosi_transfers"  — MOSI byte stream per CS-asserted window
 *   Row 3 "miso_transfers"  — MISO byte stream per CS-asserted window
 *
 * Annotation classes (within each row):
 *   bits row             — class 0: "mosi-bit",      class 1: "miso-bit"
 *   data row             — class 0: "mosi-data",     class 1: "miso-data"
 *   mosi_transfers row   — class 0: "mosi-transfer"
 *   miso_transfers row   — class 0: "miso-transfer"
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
#define ROW_BITS        0
#define ROW_DATA        1
#define ROW_MOSI_XFER   2
#define ROW_MISO_XFER   3

/* Annotation class indices within ROW_BITS */
#define CLASS_BITS_MOSI 0
#define CLASS_BITS_MISO 1

/* Annotation class indices within ROW_DATA */
#define CLASS_DATA_MOSI 0
#define CLASS_DATA_MISO 1

/* Annotation class indices within each transfer row (one class per row). */
#define CLASS_XFER_ONLY 0

/* Stop every ~100k samples to check for abort */
#define STOP_POLL_INTERVAL 100000

/* Set DSV_SPI_DBG=1 to log per-byte DATA and per-CS TRANSFER on stderr
 * (grep "SPI[C]" and compare with SPI[Py] from pd.py). */
static int spi_env_dbg(void)
{
    const char *e = getenv("DSV_SPI_DBG");
    return e && e[0] && !(e[0] == '0' && e[1] == '\0');
}

static void spi_log_data(uint64_t ss, uint64_t es, uint8_t mosi, uint8_t miso)
{
    if (!spi_env_dbg()) return;
    fprintf(stderr, "SPI[C] DATA ss=%llu es=%llu mosi=0x%02x miso=0x%02x\n",
            (unsigned long long)ss, (unsigned long long)es,
            (unsigned)mosi, (unsigned)miso);
}

/* ------------------------------------------------------------------------- */
/* Per-run state shared between streaming (v2) and batch (v1) paths          */
/* ------------------------------------------------------------------------- */

struct spi_state {
    uint8_t  prev_clk;
    uint8_t  prev_cs;
    uint8_t  mosi_byte;
    uint8_t  miso_byte;
    int      bit_count;
    uint64_t byte_start;
    int      primed;         /* false until we see the first sample of the run */

    /* Transfer aggregation: collected between a CS-assert and CS-deassert
     * edge. Emitted as a single annotation per channel spanning the entire
     * CS-asserted window, matching the Python decoder's 'TRANSFER' behaviour
     * (pd.py find_clk_edge: ss_transfer .. samplenum). Buffers grow on
     * demand via realloc to handle arbitrarily long transfers. */
    uint64_t xfer_start_sample;
    int      xfer_active;
    uint8_t *xfer_mosi;
    uint8_t *xfer_miso;
    size_t   xfer_capacity;
    size_t   xfer_count;
};

typedef struct spi_state spi_state;

static void spi_log_transfer(uint64_t ss, uint64_t es, const spi_state *st)
{
    size_t i;

    if (!spi_env_dbg() || !st || st->xfer_count == 0) return;
    fprintf(stderr, "SPI[C] TRANSFER ss=%llu es=%llu count=%zu miso=",
            (unsigned long long)ss, (unsigned long long)es, st->xfer_count);
    for (i = 0; i < st->xfer_count; i++)
        fprintf(stderr, "%s%02x", i ? " " : "", st->xfer_miso[i]);
    fprintf(stderr, " mosi=");
    for (i = 0; i < st->xfer_count; i++)
        fprintf(stderr, "%s%02x", i ? " " : "", st->xfer_mosi[i]);
    fprintf(stderr, "\n");
}

/* Release any dynamically allocated buffers inside the state. Safe to call
 * on a zero-initialised state. Used both from spi_destroy() and from the
 * v1 batch entry which keeps the state on the stack. */
static void spi_state_release(spi_state *st)
{
    if (!st) return;
    free(st->xfer_mosi);
    free(st->xfer_miso);
    st->xfer_mosi     = NULL;
    st->xfer_miso     = NULL;
    st->xfer_capacity = 0;
    st->xfer_count    = 0;
}

static void *spi_create(uint64_t samplerate, int num_channels)
{
    (void)samplerate;
    if (num_channels < 4) return NULL;       /* same guard as v1 entry */
    spi_state *st = (spi_state*)calloc(1, sizeof(*st));
    return st;                                /* may be NULL — host handles it */
}

static void spi_destroy(void *inst)
{
    if (inst) {
        spi_state_release((spi_state*)inst);
        free(inst);
    }
}

/* Grow the transfer byte buffers to hold at least `need` additional bytes.
 * Doubling allocator: amortises to O(N) for a transfer of N bytes. Returns
 * 0 on success, -1 on allocation failure (caller must drop the byte). */
static int spi_xfer_reserve(spi_state *st, size_t need)
{
    if (st->xfer_count + need <= st->xfer_capacity) return 0;
    size_t new_cap = st->xfer_capacity ? st->xfer_capacity : 64;
    while (new_cap < st->xfer_count + need) new_cap *= 2;
    uint8_t *m = (uint8_t*)realloc(st->xfer_mosi, new_cap);
    if (!m) return -1;
    st->xfer_mosi = m;
    uint8_t *s = (uint8_t*)realloc(st->xfer_miso, new_cap);
    if (!s) return -1;                       /* xfer_mosi already grew; next call retries */
    st->xfer_miso = s;
    st->xfer_capacity = new_cap;
    return 0;
}

/* Format `count` bytes as space-separated UPPERCASE hex into `out`. Matches
 * the Python decoder's `format_data(..., 'hex')` output, which uses
 * `f"{x.val:02X}"` -- see pd.py:310. (Note: this is intentionally different
 * from the per-byte data annotation that uses lowercase `%02x` in both
 * decoders -- pd.py mixes cases: byte=lower, transfer=upper.) */
static void spi_format_hex_bytes(const uint8_t *bytes, size_t count,
                                 char *out, size_t out_cap)
{
    size_t pos = 0;
    if (out_cap == 0) return;
    out[0] = 0;
    for (size_t i = 0; i < count; i++) {
        const char *fmt = (i == 0) ? "%02X" : " %02X";
        int written = snprintf(out + pos, out_cap - pos, fmt, bytes[i]);
        if (written < 0 || (size_t)written >= out_cap - pos) {
            /* Truncated: leave previously written bytes intact and stop. */
            out[out_cap - 1] = 0;
            return;
        }
        pos += (size_t)written;
    }
}

/* Emit the accumulated transfer (if any) as one annotation per direction,
 * spanning [xfer_start_sample, end_sample]. Empty transfers (CS pulsed with
 * no clocks) are dropped to match Python's behaviour (ss_transfer is kept
 * but no bytes were appended). Always clears the active flag and the byte
 * count after emission. */
static void spi_emit_transfer(spi_state *st, uint64_t end_sample,
                              void (*put_annotation)(void *ctx,
                                                     uint64_t ss, uint64_t es,
                                                     unsigned int ann_row,
                                                     unsigned int ann_class,
                                                     const char *text),
                              void *ctx)
{
    if (!st->xfer_active) return;
    if (st->xfer_count == 0) {
        st->xfer_active = 0;
        return;
    }

    spi_log_transfer(st->xfer_start_sample, end_sample, st);

    /* Each byte expands to at most 3 chars ("xx " or "xx") + terminator. */
    size_t bufsize = st->xfer_count * 3 + 1;
    char *text = (char*)malloc(bufsize);
    if (text) {
        /* Emit MISO transfer first, then MOSI, to match the Python decoder's
         * order in find_clk_edge() (pd.py:334-345). */
        spi_format_hex_bytes(st->xfer_miso, st->xfer_count, text, bufsize);
        put_annotation(ctx, st->xfer_start_sample, end_sample,
                       ROW_MISO_XFER, CLASS_XFER_ONLY, text);

        spi_format_hex_bytes(st->xfer_mosi, st->xfer_count, text, bufsize);
        put_annotation(ctx, st->xfer_start_sample, end_sample,
                       ROW_MOSI_XFER, CLASS_XFER_ONLY, text);

        free(text);
    }
    /* On allocation failure we silently skip the transfer text but still
     * clear state so we don't accumulate forever. */
    st->xfer_active = 0;
    st->xfer_count  = 0;
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

        /* If CS is already asserted (active-low: cs==0) at the very first
         * sample, open a transfer window starting there. Mirrors the
         * Python decoder priming-time CS-CHANGE handling in
         * pd.py:419-420 -> find_clk_edge(..., first=True). Without this,
         * a capture that begins mid-frame would never emit its first
         * transfer annotation. */
        if (st->prev_cs == 0 && !st->xfer_active) {
            st->xfer_start_sample = chunk_start;
            st->xfer_count        = 0;
            st->xfer_active       = 1;
        }
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

        /* CS asserted (falling edge, active-low) — open a new transfer
         * window. The window's start sample is the CS edge itself, to
         * match Python's `ss_transfer = self.samplenum` (pd.py:329). */
        if (!cur_cs && st->prev_cs) {
            st->xfer_start_sample = abs;
            st->xfer_count        = 0;
            st->xfer_active       = 1;
        }

        /* CS deasserted (rising edge) — emit the accumulated transfer
         * spanning [xfer_start_sample, abs], then reset bit-level state.
         * The order matters: emit BEFORE we clear xfer_count, otherwise
         * spi_emit_transfer would treat it as an empty transfer. */
        if (cur_cs && !st->prev_cs) {
            spi_emit_transfer(st, abs, put_annotation, ctx);
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

            /* Emit individual bits in MISO-then-MOSI order to match the
             * Python decoder's putdata() emit order (pd.py:198-202).
             * Matters when consumers (or our own log diffs) compare the
             * annotation stream position-by-position. */
            snprintf(text_buf, sizeof(text_buf), "%u", cur_miso);
            put_annotation(ctx, bit_start, bit_end, ROW_BITS, CLASS_BITS_MISO, text_buf);

            snprintf(text_buf, sizeof(text_buf), "%u", cur_mosi);
            put_annotation(ctx, bit_start, bit_end, ROW_BITS, CLASS_BITS_MOSI, text_buf);

            /* MSB first */
            st->mosi_byte = (uint8_t)((st->mosi_byte << 1) | (cur_mosi & 1));
            st->miso_byte = (uint8_t)((st->miso_byte << 1) | (cur_miso & 1));
            st->bit_count++;

            if (st->bit_count == 8) {
                /* Emit assembled bytes in MISO-then-MOSI order to match the
                 * Python decoder's putdata() emit order (pd.py:218 /
                 * pd.py:231). Format string `%02x` mirrors Python's lower-
                 * case byte annotation (the MOSI vs MISO distinction is
                 * already conveyed by the annotation row, so a prefix on
                 * the text just makes cross-engine log diffs noisy). */
                snprintf(text_buf, sizeof(text_buf), "%02x", st->miso_byte);
                put_annotation(ctx, st->byte_start, bit_end, ROW_DATA, CLASS_DATA_MISO, text_buf);

                snprintf(text_buf, sizeof(text_buf), "%02x", st->mosi_byte);
                put_annotation(ctx, st->byte_start, bit_end, ROW_DATA, CLASS_DATA_MOSI, text_buf);

                /* Accumulate into the current transfer (best effort: on
                 * allocation failure we drop the byte but keep decoding). */
                spi_log_data(st->byte_start, bit_end, st->mosi_byte, st->miso_byte);

                if (st->xfer_active && spi_xfer_reserve(st, 1) == 0) {
                    st->xfer_mosi[st->xfer_count] = st->mosi_byte;
                    st->xfer_miso[st->xfer_count] = st->miso_byte;
                    st->xfer_count++;
                }

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
    int rc = spi_decode_samples(&st, start_sample, len, ch,
                                put_annotation, ctx, stop_flag);
    /* State is stack-allocated in the v1 entry, but the transfer buffers
     * are heap-allocated; free them before returning. (The v2 path's
     * spi_destroy() does the same via spi_state_release().) */
    spi_state_release(&st);
    return rc;
}

/* --- Channel declarations --- */
static const char *ch_ids[]   = { "CLK",  "MOSI",  "MISO",  "CS",  NULL };
static const char *ch_names[] = { "Clock", "MOSI", "MISO", "Chip Select", NULL };

/* --- Annotation row declarations ---
 * Row order MUST match the ROW_* enum values used in put_annotation calls
 * above. Adding a row here without bumping the enum (or vice versa) will
 * misroute everything. */
static const char *row_ids[] = {
    "bits",            /* ROW_BITS      = 0 */
    "data",            /* ROW_DATA      = 1 */
    "mosi_transfers",  /* ROW_MOSI_XFER = 2 */
    "miso_transfers",  /* ROW_MISO_XFER = 3 */
    NULL
};
static const char *row_descs[] = {
    "Individual bits",
    "Decoded bytes",
    "MOSI transfers",
    "MISO transfers",
    NULL
};

/* --- Annotation classes (row_id:class_label) ---
 * The label after the ':' MUST match the id of an entry in the partnering
 * Python decoder's `annotations` tuple (case-insensitive, ignoring '-' / '_'
 * separators). The host walks the Python decoder's annotation tuple to
 * translate these into row routing — if the labels don't match, every
 * annotation falls back to a positional class index and ends up in the wrong
 * row (or a hidden bits/warning row, where it becomes invisible). See the
 * "Annotation routing contract" section in README.md.
 *
 * The transfer labels map to Python's `annotations[5]` ("miso-transfer")
 * and `annotations[6]` ("mosi-transfer"), which the Python decoder routes
 * into its `miso-transfers` / `mosi-transfers` rows -- so our annotations
 * land in the same on-screen rows as Python's. */
static const char *ann_classes[] = {
    "bits:mosi-bit",
    "bits:miso-bit",
    "data:mosi-data",
    "data:miso-data",
    "mosi_transfers:mosi-transfer",
    "miso_transfers:miso-transfer",
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
