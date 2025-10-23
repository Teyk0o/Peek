/*
 * Signature and Hash Verification
 */

#ifndef PEEK_UPDATER_VERIFIER_H
#define PEEK_UPDATER_VERIFIER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Verify RSA-SHA256 signature of manifest
 * Returns: 0 if valid, non-zero if invalid
 */
int verifier_check_manifest_signature(const char *manifest_path, const char *public_key);

/*
 * Verify SHA256 hash of binary
 * Returns: 0 if valid, non-zero if hash mismatch
 */
int verifier_check_binary_hash(const char *binary_path, const char *expected_hash);

#ifdef __cplusplus
}
#endif

#endif /* PEEK_UPDATER_VERIFIER_H */
