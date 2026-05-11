/**
 * c_decoder_api.h — C ABI for DSView native C protocol decoders
 *
 * This header defines the contract between the DSView host (C++) and
 * user-written C decoder shared libraries (.dylib / .so).
 *
 * Host side: loads the shared library, reads the exported `c_decoder_def`
 *            symbol, and calls the `decode` function.
 * Decoder side: fills in a `CDecoderDef` struct and exports it as
 *               `c_decoder_def`.
 *
 * Pure C header — safe to include from both C decoder source files and
 * C++ host code.
 */

#ifndef DSVIEW_PV_CDECODERS_C_DECODER_API_H
#define DSVIEW_PV_CDECODERS_C_DECODER_API_H

#include <stdint.h>

/**
 * CDecoderDef — interface struct that every C decoder must implement.
 *
 * Fill all fields and export an instance named `c_decoder_def` from
 * your shared library.
 */
typedef struct {
    /* --- Metadata (maps to srd_decoder fields) --- */
    const char *id;        /* Short machine-readable identifier, e.g. "spi" */
    const char *name;      /* Display name, e.g. "SPI"                       */
    const char *longname;  /* Full name, e.g. "Serial Peripheral Interface"  */
    const char *desc;      /* One-line description                            */

    /* --- Channel declarations (NULL-terminated arrays) --- */
    const char **channel_ids;    /* e.g. {"CLK", "DATA", NULL} */
    const char **channel_names;  /* Human-readable labels, same length as channel_ids */

    /* --- Annotation row declarations (NULL-terminated arrays) --- */
    const char **ann_row_ids;    /* e.g. {"bits", "data", NULL} */
    const char **ann_row_descs;  /* Human-readable row descriptions, same length */

    /**
     * Annotation class declarations (NULL-terminated array).
     *
     * Each entry is a string in the format "row_id:class_label", e.g.:
     *   {"bits:bit", "data:byte", NULL}
     *
     * The registry parses this to associate each annotation class with the
     * correct row.  Every class — even a row with a single class — must be
     * declared here.  The ann_class index passed to put_annotation refers to
     * the position of the matching entry within its row (0-based).
     */
    const char **ann_classes;

    /**
     * decode — synchronous decode function, called from the decoder's own
     * worker thread.
     *
     * Parameters:
     *   samplerate        — capture sample rate in Hz
     *   start_sample      — index of the first sample to decode (inclusive)
     *   end_sample        — index of the last sample to decode (inclusive)
     *   num_channels      — number of channels (== length of channel_ids)
     *   channel_samples   — channel_samples[ch][sample_index] is 0 or 1;
     *                       valid indices: ch in [0, num_channels),
     *                       sample_index in [0, end_sample - start_sample]
     *   put_annotation    — callback to emit an annotation:
     *                         ctx       : opaque host context (DecoderStack *)
     *                         ss        : annotation start sample (absolute)
     *                         es        : annotation end sample   (absolute)
     *                         ann_row   : row index into ann_row_ids (0-based)
     *                         ann_class : class index within that row (0-based)
     *                         text      : UTF-8 annotation text, may be NULL
     *   ctx               — opaque context pointer, pass unchanged to
     *                       put_annotation
     *   stop_flag         — decoder must poll this periodically and return
     *                       immediately when *stop_flag != 0
     */
    void (*decode)(
        uint64_t samplerate,
        uint64_t start_sample,
        uint64_t end_sample,
        int       num_channels,
        const uint8_t **channel_samples,
        void (*put_annotation)(void *ctx, uint64_t ss, uint64_t es,
                               int ann_row, int ann_class, const char *text),
        void *ctx,
        volatile int *stop_flag
    );
} CDecoderDef;

/**
 * Every decoder shared library must export exactly one symbol with this name.
 * The extern "C" block prevents C++ name mangling when the header is included
 * from C++ translation units.
 */
#ifdef __cplusplus
extern "C" {
#endif

extern CDecoderDef c_decoder_def;

#ifdef __cplusplus
}
#endif

#endif /* DSVIEW_PV_CDECODERS_C_DECODER_API_H */
