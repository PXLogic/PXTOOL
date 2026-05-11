#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "c_decoder_api.h"  // defines CDecoderDef (needed for map value type);
                             // the extern c_decoder_def symbol is decoder-side only

struct srd_decoder;

namespace pv {
namespace cdecoders {

/**
 * Singleton registry: loads C decoders from .dylib/.so, builds srd_decoder
 * metadata, injects them into libsigrokdecode's global decoder list.
 */
class CDecoderRegistry {
public:
    static CDecoderRegistry& instance();

    /**
     * Scan @p dir_path for .dylib (macOS) / .so (Linux) files,
     * dlopen each, read c_decoder_def, validate api_version,
     * build srd_decoder, call srd_decoder_register().
     * Safe to call multiple times (skips already-loaded IDs).
     */
    void load_c_decoders(const std::string &dir_path);

    /** Returns true iff @p dec was registered by this registry (pointer match). */
    bool is_c_decoder(const srd_decoder *dec) const;

    /**
     * Returns true iff a C decoder with id == @p id was loaded.
     * Use this when you hold a Python srd_decoder* but want to know whether
     * a C version exists for the same protocol.
     */
    bool has_c_decoder_for_id(const char *id) const;

    /**
     * Returns the CDecoderDef for @p dec, or nullptr if not a C decoder.
     * Lifetime: valid until unload_all().
     */
    CDecoderDef* get_c_decoder_def(const srd_decoder *dec) const;

    /**
     * Returns the CDecoderDef for the given protocol id, or nullptr.
     * Use this when you only have a Python srd_decoder* but need the C def.
     * Lifetime: valid until unload_all().
     */
    CDecoderDef* get_c_decoder_def_by_id(const char *id) const;

    /** dlclose all handles, free built srd_decoder structs. */
    void unload_all();

private:
    CDecoderRegistry() = default;
    ~CDecoderRegistry();
    CDecoderRegistry(const CDecoderRegistry&) = delete;
    CDecoderRegistry& operator=(const CDecoderRegistry&) = delete;

    srd_decoder* build_srd_decoder(CDecoderDef *def);
    void free_srd_decoder(srd_decoder *dec);

    std::unordered_map<const srd_decoder*, CDecoderDef*> _c_decoder_map;
    std::unordered_map<std::string, CDecoderDef*>        _c_decoder_by_id;
    std::vector<srd_decoder*> _owned_decoders;
    std::vector<void*>        _dl_handles;
};

} // namespace cdecoders
} // namespace pv
