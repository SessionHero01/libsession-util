#pragma once

#include <oxenc/bt_producer.h>

#include <cassert>
#include <memory>
#include <string_view>

#include "session/config/base.hpp"
#include "session/config/error.h"
#include "session/types.hpp"

namespace session::config {

template <typename ConfigT, typename... Args>
[[nodiscard]] int c_wrapper_init_generic(config_object** conf, char* error, Args&&... args) {
    auto c = std::make_unique<internals<ConfigT>>();
    auto c_conf = std::make_unique<config_object>();

    try {
        c->config = std::make_unique<ConfigT>(std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        if (error) {
            std::string msg = e.what();
            if (msg.size() > 255)
                msg.resize(255);
            std::memcpy(error, msg.c_str(), msg.size() + 1);
        }
        return SESSION_ERR_INVALID_DUMP;
    }

    c_conf->internals = c.release();
    c_conf->last_error = nullptr;
    *conf = c_conf.release();
    return SESSION_ERR_NONE;
}

template <typename ConfigT>
[[nodiscard]] int c_wrapper_init(
        config_object** conf,
        const unsigned char* ed25519_secretkey_bytes,
        const unsigned char* dumpstr,
        size_t dumplen,
        char* error) {
    assert(ed25519_secretkey_bytes);
    ustring_view ed25519_secretkey{ed25519_secretkey_bytes, 32};
    std::optional<ustring_view> dump;
    if (dumpstr && dumplen)
        dump.emplace(dumpstr, dumplen);
    return c_wrapper_init_generic<ConfigT>(conf, error, ed25519_secretkey, dump);
}

template <typename ConfigT>
[[nodiscard]] int c_group_wrapper_init(
        config_object** conf,
        const unsigned char* ed25519_pubkey_bytes,
        const unsigned char* ed25519_secretkey_bytes,
        const unsigned char* dump_bytes,
        size_t dumplen,
        char* error) {

    assert(ed25519_pubkey_bytes);

    ustring_view ed25519_pubkey{ed25519_pubkey_bytes, 32};
    std::optional<ustring_view> ed25519_secretkey;
    if (ed25519_secretkey_bytes)
        ed25519_secretkey.emplace(ed25519_secretkey_bytes, 32);
    std::optional<ustring_view> dump;
    if (dump_bytes && dumplen)
        dump.emplace(dump_bytes, dumplen);

    return c_wrapper_init_generic<ConfigT>(conf, error, ed25519_pubkey, ed25519_secretkey, dump);
}

template <size_t N>
void copy_c_str(char (&dest)[N], std::string_view src) {
    if (src.size() >= N)
        src.remove_suffix(src.size() - N - 1);
    std::memcpy(dest, src.data(), src.size());
    dest[src.size()] = 0;
}

// Throws std::invalid_argument if session_id doesn't look valid.  Can optionally be passed a prefix
// byte for id's that aren't starting with 0x05 (e.g. 0x03 for non-legacy group ids).
void check_session_id(std::string_view session_id, unsigned char prefix = 0x05);

// Checks the session_id (throwing if invalid) then returns it as bytes
std::string session_id_to_bytes(std::string_view session_id, unsigned char prefix = 0x05);

// Checks the session_id (throwing if invalid) then returns it as bytes, omitting the 05 prefix
// (which is the x25519 pubkey).
std::array<unsigned char, 32> session_id_xpk(std::string_view session_id);

// Validates an open group pubkey; we accept it in hex, base32z, or base64 (padded or unpadded).
// Throws std::invalid_argument if invalid.
void check_encoded_pubkey(std::string_view pk);

// Takes a 32-byte pubkey value encoded as hex, base32z, or base64 and returns the decoded 32 bytes.
// Throws if invalid.
ustring decode_pubkey(std::string_view pk);

// Modifies a string to be (ascii) lowercase.
void make_lc(std::string& s);

// Digs into a config `dict` to get out a config::set; nullptr if not there (or not set)
const config::set* maybe_set(const session::config::dict& d, const char* key);

// Digs into a config `dict` to get out an int64_t; nullopt if not there (or not int)
std::optional<int64_t> maybe_int(const session::config::dict& d, const char* key);

// Digs into a config `dict` to get out a string; nullopt if not there (or not string)
std::optional<std::string> maybe_string(const session::config::dict& d, const char* key);

// Digs into a config `dict` to get out a ustring; nullopt if not there (or not string)
std::optional<ustring> maybe_ustring(const session::config::dict& d, const char* key);

// Digs into a config `dict` to get out a string view; nullopt if not there (or not string).  The
// string view is only valid as long as the dict stays unchanged.
std::optional<std::string_view> maybe_sv(const session::config::dict& d, const char* key);

/// Sets a value to 1 if true, removes it if false.
void set_flag(ConfigBase::DictFieldProxy&& field, bool val);

/// Sets a string value if non-empty, clears it if empty.
void set_nonempty_str(ConfigBase::DictFieldProxy&& field, std::string val);
void set_nonempty_str(ConfigBase::DictFieldProxy&& field, std::string_view val);

/// Sets an integer value, if non-zero; removes it if 0.
void set_nonzero_int(ConfigBase::DictFieldProxy&& field, int64_t val);

/// Sets an integer value, if positive; removes it if <= 0.
void set_positive_int(ConfigBase::DictFieldProxy&& field, int64_t val);

/// Sets a pair of values if the given condition is satisfied, clears both values otherwise.
template <typename Condition, typename T1, typename T2>
void set_pair_if(
        Condition&& condition,
        ConfigBase::DictFieldProxy&& f1,
        T1&& v1,
        ConfigBase::DictFieldProxy&& f2,
        T2&& v2) {
    if (condition) {
        f1 = std::forward<T1>(v1);
        f2 = std::forward<T2>(v2);
    } else {
        f1.erase();
        f2.erase();
    }
}

oxenc::bt_dict::iterator append_unknown(
        oxenc::bt_dict_producer& out,
        oxenc::bt_dict::iterator it,
        oxenc::bt_dict::iterator end,
        std::string_view until);

/// Extracts and unknown keys in the top-level dict into `unknown` that have keys (strictly)
/// between previous and until.
void load_unknowns(
        oxenc::bt_dict& unknown,
        oxenc::bt_dict_consumer& in,
        std::string_view previous,
        std::string_view until);

/// ZSTD-compresses a value.  `prefix` can be prepended on the returned value, if needed.  Throws on
/// serious error.
ustring zstd_compress(ustring_view data, int level = 1, ustring_view prefix = {});

/// ZSTD-decompresses a value.  Returns nullopt if decompression fails.  If max_size is non-zero
/// then this returns nullopt if the decompressed size would exceed that limit.
std::optional<ustring> zstd_decompress(ustring_view data, size_t max_size = 0);

}  // namespace session::config
