/**
 * c_decoder_registry.cpp — CDecoderRegistry implementation
 *
 * Scans a directory for C decoder shared libraries (.dylib/.so), dlopen's
 * each one, reads the exported c_decoder_def symbol, validates the API
 * version, builds a synthetic srd_decoder struct, and injects it into
 * libsigrokdecode's global decoder list via srd_decoder_register().
 */

#include "c_decoder_registry.h"
#include "c_decoder_api.h"

#include <libsigrokdecode.h>
#include <glib.h>

#include <dlfcn.h>
#include <dirent.h>
#include <cstring>
#include <cstdio>
#include <string>

namespace pv {
namespace cdecoders {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

CDecoderRegistry& CDecoderRegistry::instance()
{
    static CDecoderRegistry inst;
    return inst;
}

CDecoderRegistry::~CDecoderRegistry()
{
    unload_all();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void CDecoderRegistry::load_c_decoders(const std::string &dir_path)
{
    DIR *dir = opendir(dir_path.c_str());
    if (!dir) {
        fprintf(stderr, "[CDecoderRegistry] Cannot open directory: %s\n",
                dir_path.c_str());
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        const char *name = entry->d_name;
        const char *dot  = strrchr(name, '.');
        if (!dot)
            continue;

#if defined(__APPLE__)
        const bool is_lib = (strcmp(dot, ".dylib") == 0);
#else
        const bool is_lib = (strcmp(dot, ".so") == 0);
#endif
        if (!is_lib)
            continue;

        std::string path = dir_path + "/" + name;

        void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            fprintf(stderr, "[CDecoderRegistry] dlopen failed for %s: %s\n",
                    path.c_str(), dlerror());
            continue;
        }

        CDecoderDef *def = static_cast<CDecoderDef *>(
            dlsym(handle, "c_decoder_def"));
        if (!def) {
            fprintf(stderr, "[CDecoderRegistry] No c_decoder_def in %s\n",
                    path.c_str());
            dlclose(handle);
            continue;
        }

        if (def->api_version != C_DECODER_API_VERSION) {
            fprintf(stderr,
                    "[CDecoderRegistry] API version mismatch in %s "
                    "(expected %u, got %u)\n",
                    path.c_str(),
                    static_cast<unsigned>(C_DECODER_API_VERSION),
                    static_cast<unsigned>(def->api_version));
            dlclose(handle);
            continue;
        }

        /* Skip IDs that are already in our registry (idempotent call). */
        bool already_loaded = false;
        for (auto *owned : _owned_decoders) {
            if (owned->id && def->id && strcmp(owned->id, def->id) == 0) {
                already_loaded = true;
                break;
            }
        }
        if (already_loaded) {
            dlclose(handle);
            continue;
        }

        srd_decoder *dec = build_srd_decoder(def);
        if (!dec) {
            fprintf(stderr, "[CDecoderRegistry] build_srd_decoder failed for %s\n",
                    path.c_str());
            dlclose(handle);
            continue;
        }

        int rc = srd_decoder_register(dec);
        if (rc != SRD_OK) {
            /* srd_decoder_register already logged the error. */
            free_srd_decoder(dec);
            dlclose(handle);
            continue;
        }

        _owned_decoders.push_back(dec);
        _c_decoder_map[dec] = def;
        _dl_handles.push_back(handle);
    }

    closedir(dir);
}

bool CDecoderRegistry::is_c_decoder(const srd_decoder *dec) const
{
    return _c_decoder_map.count(dec) > 0;
}

CDecoderDef* CDecoderRegistry::get_c_decoder_def(const srd_decoder *dec) const
{
    auto it = _c_decoder_map.find(dec);
    if (it == _c_decoder_map.end())
        return nullptr;
    return it->second;
}

void CDecoderRegistry::unload_all()
{
    for (srd_decoder *dec : _owned_decoders) {
        srd_decoder_unregister(dec);   /* remove from pd_list first */
        free_srd_decoder(dec);         /* then free */
    }
    _owned_decoders.clear();
    for (void *h : _dl_handles)
        dlclose(h);
    _dl_handles.clear();
    _c_decoder_map.clear();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

srd_decoder* CDecoderRegistry::build_srd_decoder(CDecoderDef *def)
{
    if (!def || !def->id) {
        fprintf(stderr, "[CDecoderRegistry] build_srd_decoder: NULL def or id\n");
        return nullptr;
    }

    srd_decoder *dec = g_new0(srd_decoder, 1);

    /* --- Basic metadata --- */
    dec->id       = g_strdup(def->id);
    dec->name     = g_strdup(def->name     ? def->name     : def->id);
    dec->longname = g_strdup(def->longname ? def->longname : def->id);
    dec->desc     = g_strdup(def->desc     ? def->desc     : "");
    dec->license  = g_strdup("gplv2+");

    dec->inputs          = nullptr;
    dec->outputs         = nullptr;
    dec->tags            = nullptr;
    dec->opt_channels    = nullptr;
    dec->options         = nullptr;
    dec->ann_types       = nullptr;
    dec->binary          = nullptr;
    dec->py_mod          = nullptr;
    dec->py_dec          = nullptr;

    /* --- Channels --- */
    {
        GSList *ch_list = nullptr;
        if (def->channel_ids) {
            for (int i = 0; def->channel_ids[i] != nullptr; ++i) {
                srd_channel *ch = g_new0(srd_channel, 1);
                ch->id    = g_strdup(def->channel_ids[i]);
                ch->name  = g_strdup(def->channel_names && def->channel_names[i]
                                     ? def->channel_names[i]
                                     : def->channel_ids[i]);
                ch->desc  = g_strdup("");
                ch->order = i;
                ch->type  = 0;  /* SRD_CHANNEL_COMMON equivalent */
                ch->idn   = nullptr;
                ch_list = g_slist_prepend(ch_list, ch);
            }
        }
        dec->channels = g_slist_reverse(ch_list);
    }

    /* --- Annotations --- */
    /*
     * def->ann_classes is a NULL-terminated array of "row_id:class_label".
     * We build dec->annotations as a GSList of (char**) pairs
     * { class_label, NULL }, preserving the original index order.
     */
    {
        GSList *ann_list = nullptr;
        if (def->ann_classes) {
            for (int k = 0; def->ann_classes[k] != nullptr; ++k) {
                const char *entry = def->ann_classes[k];
                const char *colon = strchr(entry, ':');

                const char *class_label;
                if (colon)
                    class_label = colon + 1;
                else
                    class_label = entry;  /* malformed, use whole string */

                /* Each annotation is a NULL-terminated char* array */
                char **pair = g_new0(char *, 2);
                pair[0] = g_strdup(class_label);
                pair[1] = nullptr;
                ann_list = g_slist_prepend(ann_list, pair);
            }
        }
        dec->annotations = g_slist_reverse(ann_list);
    }

    /* --- Annotation rows --- */
    /*
     * For each row in def->ann_row_ids, scan def->ann_classes to find
     * which class indices (global, 0-based) belong to that row, and
     * collect them as GINT_TO_POINTER(k) in row->ann_classes.
     */
    {
        GSList *row_list = nullptr;
        if (def->ann_row_ids) {
            for (int r = 0; def->ann_row_ids[r] != nullptr; ++r) {
                const char *row_id = def->ann_row_ids[r];

                srd_decoder_annotation_row *row =
                    g_new0(srd_decoder_annotation_row, 1);
                row->id   = g_strdup(row_id);
                row->desc = g_strdup(def->ann_row_descs && def->ann_row_descs[r]
                                     ? def->ann_row_descs[r]
                                     : row_id);
                row->ann_classes = nullptr;

                /* Scan ann_classes to find members of this row */
                if (def->ann_classes) {
                    for (int k = 0; def->ann_classes[k] != nullptr; ++k) {
                        const char *entry = def->ann_classes[k];
                        const char *colon = strchr(entry, ':');
                        if (!colon)
                            continue;

                        /* Compare the row_id prefix */
                        size_t prefix_len = static_cast<size_t>(colon - entry);
                        if (strlen(row_id) == prefix_len &&
                            strncmp(entry, row_id, prefix_len) == 0) {
                            row->ann_classes = g_slist_append(
                                row->ann_classes, GINT_TO_POINTER(k));
                        }
                    }
                }

                row_list = g_slist_prepend(row_list, row);
            }
        }
        dec->annotation_rows = g_slist_reverse(row_list);
    }

    return dec;
}

static void free_srd_channel(void *data)
{
    srd_channel *ch = static_cast<srd_channel *>(data);
    if (!ch) return;
    g_free(ch->id);
    g_free(ch->name);
    g_free(ch->desc);
    g_free(ch->idn);
    g_free(ch);
}

static void free_annotation(void *data)
{
    char **pair = static_cast<char **>(data);
    if (!pair) return;
    for (int i = 0; pair[i]; ++i)
        g_free(pair[i]);
    g_free(pair);
}

static void free_annotation_row(void *data)
{
    srd_decoder_annotation_row *row =
        static_cast<srd_decoder_annotation_row *>(data);
    if (!row) return;
    g_free(row->id);
    g_free(row->desc);
    g_slist_free(row->ann_classes);  /* pointers are GINT_TO_POINTER — no heap alloc */
    g_free(row);
}

void CDecoderRegistry::free_srd_decoder(srd_decoder *dec)
{
    if (!dec) return;

    g_free(dec->id);
    g_free(dec->name);
    g_free(dec->longname);
    g_free(dec->desc);
    g_free(dec->license);

    g_slist_free_full(dec->channels,        free_srd_channel);
    g_slist_free_full(dec->annotations,     free_annotation);
    g_slist_free_full(dec->annotation_rows, free_annotation_row);

    /* These are always NULL for C decoders; free defensively */
    g_slist_free(dec->inputs);
    g_slist_free(dec->outputs);
    g_slist_free(dec->tags);
    g_slist_free(dec->opt_channels);
    g_slist_free(dec->options);
    g_slist_free(dec->ann_types);
    g_slist_free(dec->binary);

    g_free(dec);
}

} // namespace cdecoders
} // namespace pv
