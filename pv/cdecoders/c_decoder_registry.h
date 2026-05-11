#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "c_decoder_api.h"

namespace pv {
namespace cdecoders {

/**
 * Singleton registry: loads C decoder shared libraries (.dylib/.so),
 * and stores them by protocol ID for fast lookup.
 *
 * C decoders are NOT injected into libsigrokdecode's decoder list.
 * The Python decoder with the same ID is still used for the "Add decoder"
 * dialog and channel configuration. When the user chooses the C engine,
 * execute_c_decode_stack() retrieves the CDecoderDef by protocol ID and
 * calls its decode() function directly, bypassing the Python interpreter.
 *
 * Not thread-safe. Must only be called from the main thread.
 */
class CDecoderRegistry {
public:
    static CDecoderRegistry& instance();

    /**
     * Scan @p dir_path for .dylib (macOS) / .so (Linux) files,
     * dlopen each, read c_decoder_def, validate api_version,
     * register by ID. Safe to call multiple times (skips already-loaded IDs).
     */
    void load_c_decoders(const std::string &dir_path);

    /**
     * Returns true iff a C decoder with id == @p id was loaded.
     */
    bool has_c_decoder_for_id(const char *id) const;

    /**
     * Returns the CDecoderDef for the given protocol id, or nullptr.
     * Lifetime: valid until unload_all().
     */
    CDecoderDef* get_c_decoder_def_by_id(const char *id) const;

    /**
     * dlclose all handles and clear the registry.
     */
    void unload_all();

private:
    CDecoderRegistry() = default;
    ~CDecoderRegistry();
    CDecoderRegistry(const CDecoderRegistry&) = delete;
    CDecoderRegistry& operator=(const CDecoderRegistry&) = delete;

    std::unordered_map<std::string, CDecoderDef*> _c_decoder_by_id;
    std::vector<void*> _dl_handles;
};

} // namespace cdecoders
} // namespace pv
