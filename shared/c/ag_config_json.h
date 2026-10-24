#ifndef AG_CONFIG_JSON_H
#define AG_CONFIG_JSON_H
#include "ag_core.h"

#define AG_CONFIG_JSON_MAX_BYTES 4096u
#define AG_CONFIG_JSON_MAX_DEPTH 8u
#define AG_CONFIG_JSON_MAX_TOKENS 512u
#define AG_CONFIG_JSON_MAX_STRING 31u

typedef enum {
    AG_CONFIG_JSON_OK = 0, AG_CONFIG_JSON_ARGUMENT, AG_CONFIG_JSON_SIZE,
    AG_CONFIG_JSON_SYNTAX, AG_CONFIG_JSON_TYPE, AG_CONFIG_JSON_NUMBER,
    AG_CONFIG_JSON_STRING_SUBSET, AG_CONFIG_JSON_DEPTH, AG_CONFIG_JSON_TOKENS,
    AG_CONFIG_JSON_UNKNOWN_KEY, AG_CONFIG_JSON_DUPLICATE_KEY,
    AG_CONFIG_JSON_MISSING_KEY, AG_CONFIG_JSON_ARRAY_LIMIT, AG_CONFIG_JSON_CONFIG_INVALID
} ag_config_json_result_t;
typedef struct {
    size_t offset; /* Byte offset, never beyond supplied length. */
    ag_config_result_t validation; /* Meaningful for CONFIG_INVALID only. */
} ag_config_json_error_t;

/* Schema-1 wire configuration, NOT an RPC envelope or an authorization/commit API.
 * Length-delimited input; no NUL required. No heap or retained input pointers.
 * Unknown/duplicate keys rejected at every level. Integers only, signed int32
 * lexical bound; no booleans, null, floats, exponents or numeric strings.
 * String subset: printable ASCII, including equivalent JSON escapes and ASCII
 * \uXXXX. Non-ASCII/surrogates/control characters rejected, never normalized.
 * On ANY error, output remains byte-for-byte untouched. Error detail is optional.
 * Caller provides valid objects and excludes concurrent changes to input/mask.
 */
ag_config_json_result_t ag_config_json_parse(const uint8_t *input, size_t length,
    const ag_pid_mask_t *supported, ag_config_t *output, ag_config_json_error_t *error);
#endif
