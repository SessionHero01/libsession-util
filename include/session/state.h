#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/namespaces.h"
#include "config/profile_pic.h"
#include "export.h"

// State object: this type holds the internal object which manages the entire state.
typedef struct state_object {
    // Internal opaque object pointer; calling code should leave this alone.
    void* internals;

    // When an error occurs in the C API this string will be set to the specific error message.  May
    // be empty.
    const char* last_error;

    // Sometimes used as the backing buffer for `last_error`.  Should not be touched externally.
    char _error_buf[256];
} state_object;

typedef struct state_config_message {
    NAMESPACE namespace_;
    const char* hash;
    uint64_t timestamp_ms;
    const unsigned char* data;
    size_t datalen;
} state_config_message;

/// API: state/state_set_logger
///
/// Sets a logging function; takes the log function pointer and a context pointer (which can be NULL
/// if not needed).  The given function pointer will be invoked with one of the above values, a
/// null-terminated c string containing the log message, and the void* context object given when
/// setting the logger (this is for caller-specific state data and won't be touched).
///
/// The logging function must have signature:
///
/// void log(config_log_level lvl, const char* msg, void* ctx);
///
/// Can be called with callback set to NULL to clear an existing logger.
///
/// The config object itself has no log level: the caller should filter by level as needed.
///
/// Declaration:
/// ```cpp
/// VOID config_set_logger(
///     [in, out]   state_object*                                   state,
///     [in]        void(*)(config_log_level, const char*, void*)   callback,
///     [in]        void*                                           ctx
/// );
/// ```
///
/// Inputs:
/// - `conf` -- [in] Pointer to config_object object
/// - `callback` -- [in] Callback function
/// - `ctx` --- [in, optional] Pointer to an optional context. Set to NULL if unused
LIBSESSION_EXPORT void state_set_logger(
        state_object* state, void (*callback)(config_log_level, const char*, void*), void* ctx);

/// API: state/state_create
///
/// Constructs a new state which generates it's own random ed25519 key pair.
///
/// When done with the object the `state_object` must be destroyed by passing the pointer to
/// state_free().
///
/// Declaration:
/// ```cpp
/// INT state_init(
///     [out]   state_object**          state,
///     [out]   char*                   error
/// );
/// ```
///
/// Inputs:
/// - `state` -- [out] Pointer to the state object
/// - `error` -- [out] the pointer to a buffer in which we will write an error string if an error
/// occurs; error messages are discarded if this is given as NULL.  If non-NULL this must be a
/// buffer of at least 256 bytes.
///
/// Outputs:
/// - `int` -- Returns 0 on success; returns a non-zero error code and write the exception message
/// as a C-string into `error` (if not NULL) on failure.
LIBSESSION_EXPORT bool state_create(state_object** state, char* error)
        __attribute__((warn_unused_result));

/// API: state/state_create
///
/// Constructs a new state which generates it's own random ed25519 key pair.
///
/// When done with the object the `state_object` must be destroyed by passing the pointer to
/// state_free().
///
/// Declaration:
/// ```cpp
/// INT state_init(
///     [out]   state_object**          state,
///     [in]    const unsigned char*    ed25519_secretkey,
///     [out]   char*                   error
/// );
/// ```
///
/// Inputs:
/// - `state` -- [out] Pointer to the state object
/// - `ed25519_secretkey` -- [in] must be the 32-byte secret key seed value.  (You can also pass the
/// pointer to the beginning of the 64-byte value libsodium calls the "secret key" as the first 32
/// bytes of that are the seed).  This field cannot be null.
/// - `error` -- [out] the pointer to a buffer in which we will write an error string if an error
/// occurs; error messages are discarded if this is given as NULL.  If non-NULL this must be a
/// buffer of at least 256 bytes.
///
/// Outputs:
/// - `int` -- Returns 0 on success; returns a non-zero error code and write the exception message
/// as a C-string into `error` (if not NULL) on failure.
LIBSESSION_EXPORT bool state_init(
        state_object** state, const unsigned char* ed25519_secretkey, char* error)
        __attribute__((warn_unused_result));

/// API: state/state_free
///
/// Frees a state object.
///
/// Declaration:
/// ```cpp
/// VOID state_free(
///     [in, out]   state_object*      state
/// );
/// ```
///
/// Inputs:
/// - `conf` -- [in] Pointer to config_object object
LIBSESSION_EXPORT void state_free(state_object* state);

LIBSESSION_EXPORT bool state_load(
        state_object* state,
        NAMESPACE namespace_,
        const char* pubkey_hex,
        const unsigned char* dump,
        size_t dumplen);

LIBSESSION_EXPORT void state_set_send_callback(
        state_object* state, void (*callback)(const char*, const unsigned char*, size_t));
// std::function<void(std::string_view swarm, ustring data)> send;

LIBSESSION_EXPORT config_string_list* state_merge(
        state_object* state, const char* pubkey_hex_, state_config_message* configs, size_t count);

/// API: state/state_dump
///
/// Returns a bt-encoded dict containing the dumps of each of the current config states for
/// storage in the database; the values in the dict would individually get passed into `load` to
/// reconstitute the object (including the push/not pushed status).  Resets the `needs_dump()`
/// flag to false.  Allocates a new buffer and sets
/// it in `out` and the length in `outlen`.  Note that this is binary data, *not* a null-terminated
/// C string.
///
/// NB: It is the caller's responsibility to `free()` the buffer when done with it.
///
/// Immediately after this is called `state_needs_dump` will start returning falst (until the
/// configuration is next modified).
///
/// Declaration:
/// ```cpp
/// VOID state_dump(
///     [in]    state_object*          state
///     [in]    bool                   full_dump
///     [out]   unsigned char**        out
///     [out]   size_t*                outlen
/// );
///
/// ```
///
/// Inputs:
/// - `state` -- [in] Pointer to state_object object
/// - `full_dump` -- [in] Flag when true the returned bt-encoded dict will include dumps for the
/// entire state, even if they would normally return `false` for `needs_dump()`.
/// - `out` -- [out] Pointer to the output location
/// - `outlen` -- [out] Length of output
LIBSESSION_EXPORT void state_dump(
        state_object* state, bool full_dump, unsigned char** out, size_t* outlen);

/// API: state/state_dump_namespace
///
/// Returns a binary dump of the current state of the config object for the specified namespace and
/// pubkey.  This dump can be used to resurrect the object at a later point (e.g. after a restart).
/// Allocates a new buffer and sets it in `out` and the length in `outlen`.  Note that this is
/// binary data, *not* a null-terminated C string.
///
/// NB: It is the caller's responsibility to `free()` the buffer when done with it.
///
/// Immediately after this is called `state_needs_dump` will start returning false (until the
/// configuration is next modified).
///
/// Declaration:
/// ```cpp
/// VOID state_dump(
///     [in]    state_object*          state
///     [in]    NAMESPACE              namespace
///     [in]    const char*            pubkey_hex
///     [out]   unsigned char**        out
///     [out]   size_t*                outlen
/// );
///
/// ```
///
/// Inputs:
/// - `state` -- [in] Pointer to state_object object
/// - `namespace` -- [in] the namespace where config messages of the desired dump are stored.
/// - `pubkey_hex` -- [in] optional pubkey the dump is associated to (in hex). Required for group
/// dumps.
/// - `out` -- [out] Pointer to the output location
/// - `outlen` -- [out] Length of output
LIBSESSION_EXPORT void state_dump_namespace(
        state_object* state,
        NAMESPACE namespace_,
        const char* pubkey_hex,
        unsigned char** out,
        size_t* outlen);

/// User Profile functions

/// API: state/state_get_profile_name
///
/// Returns a pointer to the currently-set name (null-terminated), or NULL if there is no name at
/// all.  Should be copied right away as the pointer may not remain valid beyond other API calls.
///
/// Declaration:
/// ```cpp
/// CONST CHAR* state_get_profile_name(
///     [in]    const state_object*    state
/// );
/// ```
///
/// Inputs:
/// - `state` -- [in] Pointer to the state object
///
/// Outputs:
/// - `char*` -- Pointer to the currently-set name as a null-terminated string, or NULL if there is
/// no name
LIBSESSION_EXPORT const char* state_get_profile_name(const state_object* state);

/// API: state/state_set_profile_name
///
/// Sets the user profile name to the null-terminated C string.  Returns 0 on success, non-zero on
/// error (and sets the state_object's error string).
///
/// Declaration:
/// ```cpp
/// BOOL state_set_profile_name(
///     [in]    state_object*   state,
///     [in]    const char*     name
/// );
/// ```
///
/// Inputs:
/// - `state` -- [in] Pointer to the state object
/// - `name` -- [in] Pointer to the name as a null-terminated C string
///
/// Outputs:
/// - `bool` -- Returns true on success, false on error
LIBSESSION_EXPORT bool state_set_profile_name(state_object* state, const char* name);

/// API: state/state_get_profile_pic
///
/// Obtains the current profile pic.  The pointers in the returned struct will be NULL if a profile
/// pic is not currently set, and otherwise should be copied right away (they will not be valid
/// beyond other API calls on this config object).
///
/// Declaration:
/// ```cpp
/// USER_PROFILE_PIC state_get_profile_pic(
///     [in]    const state_object*    state
/// );
/// ```
///
/// Inputs:
/// - `state` -- [in] Pointer to the state object
///
/// Outputs:
/// - `user_profile_pic` -- Pointer to the currently-set profile pic
LIBSESSION_EXPORT user_profile_pic state_get_profile_pic(const state_object* state);

/// API: state/state_set_profile_pic
///
/// Sets a user profile
///
/// Declaration:
/// ```cpp
/// BOOL state_set_profile_pic(
///     [in]    state_object*       state,
///     [in]    user_profile_pic    pic
/// );
/// ```
///
/// Inputs:
/// - `state` -- [in] Pointer to the satet object
/// - `pic` -- [in] Pointer to the pic
///
/// Outputs:
/// - `bool` -- Returns true on success, false on error
LIBSESSION_EXPORT bool state_set_profile_pic(state_object* state, user_profile_pic pic);

/// API: state/state_get_profile_blinded_msgreqs
///
/// Returns true if blinded message requests should be retrieved (from SOGS servers), false if they
/// should be ignored.
///
/// Declaration:
/// ```cpp
/// INT state_get_profile_blinded_msgreqs(
///     [in]    const state_object*    state
/// );
/// ```
///
/// Inputs:
/// - `state` -- [in] Pointer to the state object
///
/// Outputs:
/// - `int` -- Will be -1 if the state does not have the value explicitly set, 0 if the setting is
///   explicitly disabled, and 1 if the setting is explicitly enabled.
LIBSESSION_EXPORT int state_get_profile_blinded_msgreqs(const state_object* state);

/// API: state/state_set_profile_blinded_msgreqs
///
/// Sets whether blinded message requests should be retrieved from SOGS servers.  Set to 1 (or any
/// positive value) to enable; 0 to disable; and -1 to clear the setting.
///
/// Declaration:
/// ```cpp
/// VOID state_set_profile_blinded_msgreqs(
///     [in]    state_object*       state,
///     [in]    int                 enabled
/// );
/// ```
///
/// Inputs:
/// - `state` -- [in] Pointer to the state object
/// - `enabled` -- [in] true if they should be enabled, false if disabled
///
/// Outputs:
/// - `void` -- Returns Nothing
LIBSESSION_EXPORT void state_set_profile_blinded_msgreqs(state_object* state, int enabled);

#ifdef __cplusplus
}  // extern "C"
#endif
