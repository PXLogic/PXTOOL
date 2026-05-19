/**
 * c_decoder_registry.cpp — CDecoderRegistry implementation
 *
 * Scans a directory for C decoder shared libraries (.dylib/.so), dlopen's
 * each one, reads the exported c_decoder_def symbol, validates the API
 * version, and registers them by ID in an internal map.
 *
 * C decoders are NOT injected into libsigrokdecode's pd_list. The Python
 * decoder with the same ID remains the one shown in the "Add decoder"
 * dialog. The C decoder is looked up by ID only when execute_c_decode_stack
 * runs, bypassing the Python interpreter entirely.
 */

#include "c_decoder_registry.h"
#include "c_decoder_api.h"

#include "pv/log.h"

#include <dlfcn.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>
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
    errno = 0;
    DIR *dir = opendir(dir_path.c_str());
    if (!dir) {
        // ENOENT is the normal case for the user-installed decoders dir that
        // hasn't been created yet — don't scare end users with a red ERROR for
        // an entirely expected condition. Anything else (permission denied,
        // too many symlinks, etc.) is still surfaced as a real error.
        if (errno == ENOENT) {
            dsv_info("C decoder directory not present, skipping: %s",
                     dir_path.c_str());
        } else {
            dsv_err("Cannot open directory: %s (errno=%d)",
                    dir_path.c_str(), errno);
        }
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
            dsv_err("dlopen failed for %s: %s", path.c_str(), dlerror());
            continue;
        }

        CDecoderDef *def = static_cast<CDecoderDef *>(
            dlsym(handle, "c_decoder_def"));
        if (!def) {
            dsv_err("No c_decoder_def in %s", path.c_str());
            dlclose(handle);
            continue;
        }

        if (def->api_version < 1 || def->api_version > C_DECODER_API_VERSION) {
            dsv_err("Unsupported C decoder API version in %s "
                    "(decoder reports %u, host supports 1..%u)",
                    path.c_str(),
                    static_cast<unsigned>(def->api_version),
                    static_cast<unsigned>(C_DECODER_API_VERSION));
            dlclose(handle);
            continue;
        }

        /* Skip IDs that are already in our registry (idempotent call). */
        if (_c_decoder_by_id.count(def->id ? def->id : "")) {
            dlclose(handle);
            continue;
        }

        if (!def->id) {
            dsv_err("C decoder in %s has NULL id", path.c_str());
            dlclose(handle);
            continue;
        }

        const bool has_batch = (def->decode != nullptr);
        bool has_streaming = false;
        if (def->api_version >= 2) {
            const int triple_count =
                (def->create       != nullptr) +
                (def->decode_chunk != nullptr) +
                (def->destroy      != nullptr);
            if (triple_count == 3) {
                has_streaming = true;
            } else if (triple_count != 0) {
                dsv_err("C decoder '%s' in %s has a partial streaming triple "
                        "(create=%p, decode_chunk=%p, destroy=%p); ignoring "
                        "streaming entries",
                        def->id, path.c_str(),
                        (void*)def->create, (void*)def->decode_chunk,
                        (void*)def->destroy);
                /* Demote to batch-only: zero the partial fields so the host
                 * dispatcher only sees fully-formed triples. */
                def->create = nullptr;
                def->decode_chunk = nullptr;
                def->destroy = nullptr;
            }
        }

        if (!has_batch && !has_streaming) {
            dsv_err("C decoder '%s' in %s exposes neither batch decode nor "
                    "streaming triple; rejecting",
                    def->id, path.c_str());
            dlclose(handle);
            continue;
        }

        /* C decoders are NOT injected into libsigrokdecode's pd_list.
         * The Python decoder with the same ID remains the one shown in the
         * "Add decoder" dialog. The C decoder is looked up by ID only when
         * the decode engine needs to run it. This avoids duplicate-ID
         * conflicts with srd_decoder_register. */
        _c_decoder_by_id[def->id] = def;
        _dl_handles.push_back(handle);
        dsv_info("C decoder '%s' loaded from %s (api=v%u, batch=%s, streaming=%s)",
                 def->id, path.c_str(),
                 static_cast<unsigned>(def->api_version),
                 has_batch ? "yes" : "no",
                 has_streaming ? "yes" : "no");
    }

    closedir(dir);
}


bool CDecoderRegistry::has_c_decoder_for_id(const char *id) const
{
    if (!id) return false;
    return _c_decoder_by_id.count(id) > 0;
}

CDecoderDef* CDecoderRegistry::get_c_decoder_def_by_id(const char *id) const
{
    if (!id) return nullptr;
    auto it = _c_decoder_by_id.find(id);
    if (it == _c_decoder_by_id.end())
        return nullptr;
    return it->second;
}

void CDecoderRegistry::unload_all()
{
    /* C decoders were never registered in libsigrokdecode's pd_list,
     * so no unregistration needed — just dlclose all handles. */
    for (void *h : _dl_handles)
        dlclose(h);
    _dl_handles.clear();
    _c_decoder_by_id.clear();
}


} // namespace cdecoders
} // namespace pv
