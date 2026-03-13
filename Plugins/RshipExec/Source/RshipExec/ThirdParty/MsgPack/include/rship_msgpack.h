/**
 * @file rship_msgpack.h
 * @brief Minimal MessagePack encoder/decoder for embedded systems
 *
 * This implementation focuses on the subset of MessagePack needed for Rship:
 *   - nil, boolean, integers (signed/unsigned up to 64-bit)
 *   - floats (32/64-bit)
 *   - strings, binary data
 *   - arrays and maps
 *
 * Design principles:
 *   - No dynamic allocation
 *   - Fixed-size buffer writing
 *   - Streaming decoder (no full parse required)
 *   - Minimal code size for Profile A
 *
 * @copyright Copyright (c) 2024 Rship
 * @license AGPL-3.0-or-later
 */

#ifndef RSHIP_MSGPACK_H
#define RSHIP_MSGPACK_H

#include "rship_config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * MessagePack Type Tags
 *============================================================================*/

/* Positive fixint: 0x00 - 0x7f (value is the byte itself) */
/* Negative fixint: 0xe0 - 0xff (value is the byte itself as int8_t) */

#define MSGPACK_NIL             0xC0
#define MSGPACK_FALSE           0xC2
#define MSGPACK_TRUE            0xC3

#define MSGPACK_BIN8            0xC4
#define MSGPACK_BIN16           0xC5
#define MSGPACK_BIN32           0xC6

#define MSGPACK_EXT8            0xC7
#define MSGPACK_EXT16           0xC8
#define MSGPACK_EXT32           0xC9

#define MSGPACK_FLOAT32         0xCA
#define MSGPACK_FLOAT64         0xCB

#define MSGPACK_UINT8           0xCC
#define MSGPACK_UINT16          0xCD
#define MSGPACK_UINT32          0xCE
#define MSGPACK_UINT64          0xCF

#define MSGPACK_INT8            0xD0
#define MSGPACK_INT16           0xD1
#define MSGPACK_INT32           0xD2
#define MSGPACK_INT64           0xD3

#define MSGPACK_FIXEXT1         0xD4
#define MSGPACK_FIXEXT2         0xD5
#define MSGPACK_FIXEXT4         0xD6
#define MSGPACK_FIXEXT8         0xD7
#define MSGPACK_FIXEXT16        0xD8

#define MSGPACK_STR8            0xD9
#define MSGPACK_STR16           0xDA
#define MSGPACK_STR32           0xDB

#define MSGPACK_ARRAY16         0xDC
#define MSGPACK_ARRAY32         0xDD

#define MSGPACK_MAP16           0xDE
#define MSGPACK_MAP32           0xDF

/* Mask for fixstr (0xa0-0xbf, 5-bit length in lower bits) */
#define MSGPACK_FIXSTR_MASK     0xA0
#define MSGPACK_FIXSTR_MAX      31

/* Mask for fixarray (0x90-0x9f, 4-bit length in lower bits) */
#define MSGPACK_FIXARRAY_MASK   0x90
#define MSGPACK_FIXARRAY_MAX    15

/* Mask for fixmap (0x80-0x8f, 4-bit length in lower bits) */
#define MSGPACK_FIXMAP_MASK     0x80
#define MSGPACK_FIXMAP_MAX      15

/*============================================================================
 * Encoder Types
 *============================================================================*/

/**
 * @brief MessagePack encoder context
 *
 * Writes to a fixed-size buffer. Tracks write position and overflow state.
 */
typedef struct rship_msgpack_writer {
    uint8_t* buffer;        /**< Output buffer */
    size_t   capacity;      /**< Buffer capacity */
    size_t   pos;           /**< Current write position */
    bool     overflow;      /**< Set if write would exceed capacity */
} rship_msgpack_writer_t;

/*============================================================================
 * Encoder API
 *============================================================================*/

/**
 * @brief Initialize a MessagePack writer
 *
 * @param w       Writer context to initialize
 * @param buffer  Output buffer
 * @param capacity Buffer size in bytes
 */
void msgpack_writer_init(rship_msgpack_writer_t* w,
                         uint8_t* buffer,
                         size_t capacity);

/**
 * @brief Reset writer to beginning of buffer
 *
 * @param w Writer context
 */
void msgpack_writer_reset(rship_msgpack_writer_t* w);

/**
 * @brief Get current write position (encoded length so far)
 *
 * @param w Writer context
 * @return Bytes written (or would have been written if overflow)
 */
size_t msgpack_writer_len(const rship_msgpack_writer_t* w);

/**
 * @brief Check if writer has overflowed
 *
 * @param w Writer context
 * @return true if any write exceeded buffer capacity
 */
bool msgpack_writer_overflow(const rship_msgpack_writer_t* w);

/*----------------------------------------------------------------------------
 * Primitive Type Encoders
 *----------------------------------------------------------------------------*/

/**
 * @brief Encode nil value
 * @param w Writer context
 * @return true on success, false if overflow
 */
bool msgpack_write_nil(rship_msgpack_writer_t* w);

/**
 * @brief Encode boolean value
 * @param w Writer context
 * @param value Boolean value to encode
 * @return true on success, false if overflow
 */
bool msgpack_write_bool(rship_msgpack_writer_t* w, bool value);

/**
 * @brief Encode unsigned integer (auto-selects smallest format)
 * @param w Writer context
 * @param value Value to encode (0 to UINT64_MAX)
 * @return true on success, false if overflow
 */
bool msgpack_write_uint(rship_msgpack_writer_t* w, uint64_t value);

/**
 * @brief Encode signed integer (auto-selects smallest format)
 * @param w Writer context
 * @param value Value to encode (INT64_MIN to INT64_MAX)
 * @return true on success, false if overflow
 */
bool msgpack_write_int(rship_msgpack_writer_t* w, int64_t value);

/**
 * @brief Encode 32-bit float
 * @param w Writer context
 * @param value Float value
 * @return true on success, false if overflow
 */
bool msgpack_write_float32(rship_msgpack_writer_t* w, float value);

/**
 * @brief Encode 64-bit double
 * @param w Writer context
 * @param value Double value
 * @return true on success, false if overflow
 */
bool msgpack_write_float64(rship_msgpack_writer_t* w, double value);

/*----------------------------------------------------------------------------
 * String and Binary Encoders
 *----------------------------------------------------------------------------*/

/**
 * @brief Encode string (UTF-8)
 *
 * Automatically selects fixstr, str8, str16, or str32 format.
 *
 * @param w Writer context
 * @param str Null-terminated string
 * @return true on success, false if overflow
 */
bool msgpack_write_str(rship_msgpack_writer_t* w, const char* str);

/**
 * @brief Encode string with explicit length
 *
 * @param w Writer context
 * @param str String data (not null-terminated)
 * @param len String length in bytes
 * @return true on success, false if overflow
 */
bool msgpack_write_str_len(rship_msgpack_writer_t* w,
                           const char* str,
                           size_t len);

/**
 * @brief Encode binary data
 *
 * Automatically selects bin8, bin16, or bin32 format.
 *
 * @param w Writer context
 * @param data Binary data
 * @param len Data length in bytes
 * @return true on success, false if overflow
 */
bool msgpack_write_bin(rship_msgpack_writer_t* w,
                       const uint8_t* data,
                       size_t len);

/*----------------------------------------------------------------------------
 * Container Encoders
 *----------------------------------------------------------------------------*/

/**
 * @brief Write array header
 *
 * After calling this, write exactly `count` elements.
 *
 * @param w Writer context
 * @param count Number of array elements to follow
 * @return true on success, false if overflow
 */
bool msgpack_write_array(rship_msgpack_writer_t* w, size_t count);

/**
 * @brief Write map header
 *
 * After calling this, write exactly `count` key-value pairs
 * (2 * count elements).
 *
 * @param w Writer context
 * @param count Number of key-value pairs to follow
 * @return true on success, false if overflow
 */
bool msgpack_write_map(rship_msgpack_writer_t* w, size_t count);

/*----------------------------------------------------------------------------
 * Raw Data Writing
 *----------------------------------------------------------------------------*/

/**
 * @brief Write raw bytes to output buffer
 *
 * Used for embedding pre-encoded MessagePack or binary payloads.
 *
 * @param w Writer context
 * @param data Raw data to write
 * @param len Data length
 * @return true on success, false if overflow
 */
bool msgpack_write_raw(rship_msgpack_writer_t* w,
                       const uint8_t* data,
                       size_t len);

/*============================================================================
 * Decoder Types
 *============================================================================*/

/**
 * @brief MessagePack value types
 */
typedef enum msgpack_type {
    MSGPACK_TYPE_NIL,
    MSGPACK_TYPE_BOOL,
    MSGPACK_TYPE_UINT,
    MSGPACK_TYPE_INT,
    MSGPACK_TYPE_FLOAT,
    MSGPACK_TYPE_DOUBLE,
    MSGPACK_TYPE_STR,
    MSGPACK_TYPE_BIN,
    MSGPACK_TYPE_ARRAY,
    MSGPACK_TYPE_MAP,
    MSGPACK_TYPE_EXT,
    MSGPACK_TYPE_INVALID
} msgpack_type_t;

/**
 * @brief Decoded MessagePack value
 *
 * For strings/binary/arrays/maps, data points into the original buffer.
 * The decoder does not copy data.
 */
typedef struct msgpack_value {
    msgpack_type_t type;
    union {
        bool          boolean;
        uint64_t      u64;
        int64_t       i64;
        float         f32;
        double        f64;
        struct {
            const uint8_t* ptr;
            size_t         len;
        } data;                    /**< For STR, BIN */
        struct {
            size_t count;          /**< Element count (array) or pair count (map) */
        } container;
        struct {
            int8_t         type;
            const uint8_t* ptr;
            size_t         len;
        } ext;                     /**< For EXT */
    } v;
} msgpack_value_t;

/**
 * @brief MessagePack reader context
 *
 * Reads from a fixed buffer. Tracks read position.
 */
typedef struct rship_msgpack_reader {
    const uint8_t* buffer;      /**< Input buffer */
    size_t         len;         /**< Buffer length */
    size_t         pos;         /**< Current read position */
} rship_msgpack_reader_t;

/*============================================================================
 * Decoder API
 *============================================================================*/

/**
 * @brief Initialize a MessagePack reader
 *
 * @param r       Reader context to initialize
 * @param buffer  Input buffer containing MessagePack data
 * @param len     Buffer length in bytes
 */
void msgpack_reader_init(rship_msgpack_reader_t* r,
                         const uint8_t* buffer,
                         size_t len);

/**
 * @brief Get remaining bytes in reader
 *
 * @param r Reader context
 * @return Bytes remaining
 */
size_t msgpack_reader_remaining(const rship_msgpack_reader_t* r);

/**
 * @brief Check if reader is at end of buffer
 *
 * @param r Reader context
 * @return true if no more data
 */
bool msgpack_reader_eof(const rship_msgpack_reader_t* r);

/**
 * @brief Read next value from buffer
 *
 * For containers (array/map), this only reads the header.
 * You must then read the contained elements.
 *
 * @param r   Reader context
 * @param val Output value (may not be modified on error)
 * @return true on success, false on error (malformed or end of buffer)
 */
bool msgpack_read(rship_msgpack_reader_t* r, msgpack_value_t* val);

/**
 * @brief Peek at next value type without consuming it
 *
 * @param r Reader context
 * @return Type of next value, or MSGPACK_TYPE_INVALID if none
 */
msgpack_type_t msgpack_peek_type(const rship_msgpack_reader_t* r);

/**
 * @brief Skip the next value (including nested containers)
 *
 * @param r Reader context
 * @return true on success, false on error
 */
bool msgpack_skip(rship_msgpack_reader_t* r);

/*----------------------------------------------------------------------------
 * Convenience Readers
 *----------------------------------------------------------------------------*/

/**
 * @brief Read nil value
 * @param r Reader context
 * @return true if next value is nil, false otherwise
 */
bool msgpack_read_nil(rship_msgpack_reader_t* r);

/**
 * @brief Read boolean value
 * @param r Reader context
 * @param out Output value
 * @return true if next value is boolean, false otherwise
 */
bool msgpack_read_bool(rship_msgpack_reader_t* r, bool* out);

/**
 * @brief Read unsigned integer
 *
 * Works for positive fixint, uint8/16/32/64.
 *
 * @param r Reader context
 * @param out Output value
 * @return true if next value is unsigned int, false otherwise
 */
bool msgpack_read_uint(rship_msgpack_reader_t* r, uint64_t* out);

/**
 * @brief Read signed integer
 *
 * Works for fixint (positive/negative), int8/16/32/64, and unsigned types.
 *
 * @param r Reader context
 * @param out Output value
 * @return true if next value is integer, false otherwise
 */
bool msgpack_read_int(rship_msgpack_reader_t* r, int64_t* out);

/**
 * @brief Read double (accepts float32 or float64)
 * @param r Reader context
 * @param out Output value
 * @return true if next value is float, false otherwise
 */
bool msgpack_read_double(rship_msgpack_reader_t* r, double* out);

/**
 * @brief Read string (returns pointer into buffer)
 * @param r Reader context
 * @param out Output pointer (not null-terminated!)
 * @param len Output length
 * @return true if next value is string, false otherwise
 */
bool msgpack_read_str(rship_msgpack_reader_t* r,
                      const char** out,
                      size_t* len);

/**
 * @brief Read binary data (returns pointer into buffer)
 * @param r Reader context
 * @param out Output pointer
 * @param len Output length
 * @return true if next value is binary, false otherwise
 */
bool msgpack_read_bin(rship_msgpack_reader_t* r,
                      const uint8_t** out,
                      size_t* len);

/**
 * @brief Read array header
 * @param r Reader context
 * @param count Output element count
 * @return true if next value is array, false otherwise
 */
bool msgpack_read_array(rship_msgpack_reader_t* r, size_t* count);

/**
 * @brief Read map header
 * @param r Reader context
 * @param count Output pair count
 * @return true if next value is map, false otherwise
 */
bool msgpack_read_map(rship_msgpack_reader_t* r, size_t* count);

#ifdef __cplusplus
}
#endif

#endif /* RSHIP_MSGPACK_H */
