/*
 * This file is part of the PXTOOL project.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Reference-counting contract of pv::data::decode::Decoder's option maps.
 *
 * commit() used to shallow-copy _options_back into _options, leaving both maps
 * pointing at the same GVariants while only _options_back held a reference. A
 * later set_option() for the same id unref'd that value and dropped the last
 * reference, so _options kept a dangling pointer. The next g_variant_ref() on
 * it -- in create_decoder_inst() or DecoderOptions::getter() -- was a
 * use-after-free that corrupted glib's GVariantTypeInfo refcount; the process
 * then aborted much later, while srd_decoder_unload_all() ran at exit.
 *
 * Run this under AddressSanitizer to catch a regression as a hard failure
 * rather than as damage that only shows up on the way out.
 */

#include <glib.h>
#include <libsigrokdecode.h>

#include <cstdio>

#include "log/xlog.h"
#include "pv/data/decode/decoder.h"

/* pv/log.h expands dsv_info() into xlog_info(dsv_log, ...) + dsv_ui_log().
 * Both have C++ linkage there, so these definitions must not be extern "C". */
xlog_writer *dsv_log = nullptr;
void dsv_ui_log(int, const char *, ...) {}

/* decoder.cpp references these from create_decoder_inst(), which these cases
 * never call; the stubs only satisfy the linker. */
extern "C" {
int srd_inst_channel_set_all(struct srd_decoder_inst *, GHashTable *)
{
    return 0;
}

struct srd_decoder_inst *srd_inst_new(struct srd_session *, const char *,
    GHashTable *)
{
    return nullptr;
}
}

static int failures;

static void check(bool ok, const char *what)
{
    if (ok) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        failures++;
    }
}

/* Touch the variant the way create_decoder_inst() does. On a freed variant this
 * is the use-after-free ASan reports. */
static bool variant_is_usable(GVariant *v)
{
    if (v == nullptr)
        return false;
    g_variant_ref(v);
    bool ok = g_variant_is_of_type(v, G_VARIANT_TYPE_STRING) == TRUE;
    g_variant_unref(v);
    return ok;
}

int main(void)
{
    /* Replacing an option after commit() must leave the committed map holding a
     * live variant. */
    {
        pv::data::decode::Decoder dec(nullptr);

        GVariant *first = g_variant_ref_sink(g_variant_new_string("hex"));
        dec.set_option("format", first);
        g_variant_unref(first); /* the caller hands its reference over */
        dec.commit();

        GVariant *second = g_variant_ref_sink(g_variant_new_string("bin"));
        dec.set_option("format", second);
        g_variant_unref(second);

        /* Not committed yet, so the committed map still holds "hex" -- and it
         * has to still be alive even though set_option() dropped the pending
         * map's reference to it. */
        std::map<std::string, GVariant *> &committed = dec.options();
        check(committed.count("format") == 1, "committed map keeps the option");
        check(variant_is_usable(committed["format"]),
            "committed value survives a later set_option");
        check(g_strcmp0(g_variant_get_string(committed["format"], nullptr),
                  "hex") == 0,
            "committed value is unchanged before commit");

        dec.commit();
        check(g_strcmp0(g_variant_get_string(dec.options()["format"], nullptr),
                  "bin") == 0,
            "commit publishes the new value");
        check(variant_is_usable(dec.options()["format"]),
            "value stays alive after the second commit");
    }

    /* Repeated set_option/commit cycles must not accumulate or over-release
     * references. */
    {
        pv::data::decode::Decoder dec(nullptr);

        for (int i = 0; i < 20; i++) {
            char text[32];
            g_snprintf(text, sizeof(text), "value-%d", i);
            GVariant *v = g_variant_ref_sink(g_variant_new_string(text));
            dec.set_option("format", v);
            g_variant_unref(v);
            dec.commit();
            if (!variant_is_usable(dec.options()["format"])) {
                check(false, "value usable across repeated commits");
                break;
            }
        }
        check(g_strcmp0(g_variant_get_string(dec.options()["format"], nullptr),
                  "value-19") == 0,
            "last value survives repeated set_option/commit");
    }

    /* set_option without a following commit must not disturb the committed
     * map, and teardown must release both maps exactly once. */
    {
        pv::data::decode::Decoder dec(nullptr);

        GVariant *v = g_variant_ref_sink(g_variant_new_string("hex"));
        dec.set_option("format", v);
        g_variant_unref(v);
        dec.commit();

        GVariant *pending = g_variant_ref_sink(g_variant_new_string("dec"));
        dec.set_option("format", pending);
        g_variant_unref(pending);

        check(variant_is_usable(dec.options()["format"]),
            "uncommitted set_option leaves the committed value alive");
    }

    if (failures) {
        printf("decoder options tests FAILED: %d\n", failures);
        return 1;
    }
    printf("decoder options tests passed\n");
    return 0;
}
