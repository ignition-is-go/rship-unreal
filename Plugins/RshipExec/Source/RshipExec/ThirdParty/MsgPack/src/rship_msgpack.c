/**
 * @file rship_msgpack.c
 * @brief MessagePack encoder/decoder implementation
 *
 * @copyright Copyright (c) 2024 Rship
 * @license AGPL-3.0-or-later
 */

#include "rship_msgpack.h"
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * @brief Write a single byte to the output buffer
 */
static bool write_byte(rship_msgpack_writer_t* w, uint8_t byte) {
    if (w->pos >= w->capacity) {
        w->overflow = true;
        w->pos++;
        return false;
    }
    w->buffer[w->pos++] = byte;
    return true;
}

/**
 * @brief Write multiple bytes to the output buffer
 */
static bool write_bytes(rship_msgpack_writer_t* w,
                        const uint8_t* data,
                        size_t len) {
    if (w->pos + len > w->capacity) {
        w->overflow = true;
        w->pos += len;
        return false;
    }
    memcpy(w->buffer + w->pos, data, len);
    w->pos += len;
    return true;
}

/**
 * @brief Write 16-bit big-endian value
 */
static bool write_be16(rship_msgpack_writer_t* w, uint16_t value) {
    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value);
    return write_bytes(w, buf, 2);
}

/**
 * @brief Write 32-bit big-endian value
 */
static bool write_be32(rship_msgpack_writer_t* w, uint32_t value) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)(value);
    return write_bytes(w, buf, 4);
}

/**
 * @brief Write 64-bit big-endian value
 */
static bool write_be64(rship_msgpack_writer_t* w, uint64_t value) {
    uint8_t buf[8];
    buf[0] = (uint8_t)(value >> 56);
    buf[1] = (uint8_t)(value >> 48);
    buf[2] = (uint8_t)(value >> 40);
    buf[3] = (uint8_t)(value >> 32);
    buf[4] = (uint8_t)(value >> 24);
    buf[5] = (uint8_t)(value >> 16);
    buf[6] = (uint8_t)(value >> 8);
    buf[7] = (uint8_t)(value);
    return write_bytes(w, buf, 8);
}

/*============================================================================
 * Writer Initialization
 *============================================================================*/

void msgpack_writer_init(rship_msgpack_writer_t* w,
                         uint8_t* buffer,
                         size_t capacity) {
    w->buffer = buffer;
    w->capacity = capacity;
    w->pos = 0;
    w->overflow = false;
}

void msgpack_writer_reset(rship_msgpack_writer_t* w) {
    w->pos = 0;
    w->overflow = false;
}

size_t msgpack_writer_len(const rship_msgpack_writer_t* w) {
    return w->pos;
}

bool msgpack_writer_overflow(const rship_msgpack_writer_t* w) {
    return w->overflow;
}

/*============================================================================
 * Primitive Encoders
 *============================================================================*/

bool msgpack_write_nil(rship_msgpack_writer_t* w) {
    return write_byte(w, MSGPACK_NIL);
}

bool msgpack_write_bool(rship_msgpack_writer_t* w, bool value) {
    return write_byte(w, value ? MSGPACK_TRUE : MSGPACK_FALSE);
}

bool msgpack_write_uint(rship_msgpack_writer_t* w, uint64_t value) {
    if (value <= 0x7F) {
        /* Positive fixint: 0x00 - 0x7f */
        return write_byte(w, (uint8_t)value);
    } else if (value <= 0xFF) {
        return write_byte(w, MSGPACK_UINT8) &&
               write_byte(w, (uint8_t)value);
    } else if (value <= 0xFFFF) {
        return write_byte(w, MSGPACK_UINT16) &&
               write_be16(w, (uint16_t)value);
    } else if (value <= 0xFFFFFFFF) {
        return write_byte(w, MSGPACK_UINT32) &&
               write_be32(w, (uint32_t)value);
    } else {
        return write_byte(w, MSGPACK_UINT64) &&
               write_be64(w, value);
    }
}

bool msgpack_write_int(rship_msgpack_writer_t* w, int64_t value) {
    if (value >= 0) {
        /* Positive values use unsigned encoding */
        return msgpack_write_uint(w, (uint64_t)value);
    } else if (value >= -32) {
        /* Negative fixint: 0xe0 - 0xff */
        return write_byte(w, (uint8_t)value);
    } else if (value >= -128) {
        return write_byte(w, MSGPACK_INT8) &&
               write_byte(w, (uint8_t)(int8_t)value);
    } else if (value >= -32768) {
        return write_byte(w, MSGPACK_INT16) &&
               write_be16(w, (uint16_t)(int16_t)value);
    } else if (value >= -2147483648LL) {
        return write_byte(w, MSGPACK_INT32) &&
               write_be32(w, (uint32_t)(int32_t)value);
    } else {
        return write_byte(w, MSGPACK_INT64) &&
               write_be64(w, (uint64_t)value);
    }
}

bool msgpack_write_float32(rship_msgpack_writer_t* w, float value) {
    union {
        float f;
        uint32_t u;
    } conv;
    conv.f = value;
    return write_byte(w, MSGPACK_FLOAT32) &&
           write_be32(w, conv.u);
}

bool msgpack_write_float64(rship_msgpack_writer_t* w, double value) {
    union {
        double d;
        uint64_t u;
    } conv;
    conv.d = value;
    return write_byte(w, MSGPACK_FLOAT64) &&
           write_be64(w, conv.u);
}

/*============================================================================
 * String and Binary Encoders
 *============================================================================*/

bool msgpack_write_str_len(rship_msgpack_writer_t* w,
                           const char* str,
                           size_t len) {
    bool ok;

    if (len <= MSGPACK_FIXSTR_MAX) {
        ok = write_byte(w, (uint8_t)(MSGPACK_FIXSTR_MASK | len));
    } else if (len <= 0xFF) {
        ok = write_byte(w, MSGPACK_STR8) &&
             write_byte(w, (uint8_t)len);
    } else if (len <= 0xFFFF) {
        ok = write_byte(w, MSGPACK_STR16) &&
             write_be16(w, (uint16_t)len);
    } else {
        ok = write_byte(w, MSGPACK_STR32) &&
             write_be32(w, (uint32_t)len);
    }

    if (!ok) return false;
    return write_bytes(w, (const uint8_t*)str, len);
}

bool msgpack_write_str(rship_msgpack_writer_t* w, const char* str) {
    if (str == NULL) {
        /* Write empty string for NULL */
        return msgpack_write_str_len(w, "", 0);
    }
    return msgpack_write_str_len(w, str, strlen(str));
}

bool msgpack_write_bin(rship_msgpack_writer_t* w,
                       const uint8_t* data,
                       size_t len) {
    bool ok;

    if (len <= 0xFF) {
        ok = write_byte(w, MSGPACK_BIN8) &&
             write_byte(w, (uint8_t)len);
    } else if (len <= 0xFFFF) {
        ok = write_byte(w, MSGPACK_BIN16) &&
             write_be16(w, (uint16_t)len);
    } else {
        ok = write_byte(w, MSGPACK_BIN32) &&
             write_be32(w, (uint32_t)len);
    }

    if (!ok) return false;
    return write_bytes(w, data, len);
}

/*============================================================================
 * Container Encoders
 *============================================================================*/

bool msgpack_write_array(rship_msgpack_writer_t* w, size_t count) {
    if (count <= MSGPACK_FIXARRAY_MAX) {
        return write_byte(w, (uint8_t)(MSGPACK_FIXARRAY_MASK | count));
    } else if (count <= 0xFFFF) {
        return write_byte(w, MSGPACK_ARRAY16) &&
               write_be16(w, (uint16_t)count);
    } else {
        return write_byte(w, MSGPACK_ARRAY32) &&
               write_be32(w, (uint32_t)count);
    }
}

bool msgpack_write_map(rship_msgpack_writer_t* w, size_t count) {
    if (count <= MSGPACK_FIXMAP_MAX) {
        return write_byte(w, (uint8_t)(MSGPACK_FIXMAP_MASK | count));
    } else if (count <= 0xFFFF) {
        return write_byte(w, MSGPACK_MAP16) &&
               write_be16(w, (uint16_t)count);
    } else {
        return write_byte(w, MSGPACK_MAP32) &&
               write_be32(w, (uint32_t)count);
    }
}

bool msgpack_write_raw(rship_msgpack_writer_t* w,
                       const uint8_t* data,
                       size_t len) {
    return write_bytes(w, data, len);
}

/*============================================================================
 * Reader Helpers
 *============================================================================*/

/**
 * @brief Read a single byte from the input buffer
 */
static bool read_byte(rship_msgpack_reader_t* r, uint8_t* out) {
    if (r->pos >= r->len) {
        return false;
    }
    *out = r->buffer[r->pos++];
    return true;
}

/**
 * @brief Peek at next byte without consuming
 */
static bool peek_byte(const rship_msgpack_reader_t* r, uint8_t* out) {
    if (r->pos >= r->len) {
        return false;
    }
    *out = r->buffer[r->pos];
    return true;
}

/**
 * @brief Read 16-bit big-endian value
 */
static bool read_be16(rship_msgpack_reader_t* r, uint16_t* out) {
    if (r->pos + 2 > r->len) {
        return false;
    }
    *out = ((uint16_t)r->buffer[r->pos] << 8) |
           ((uint16_t)r->buffer[r->pos + 1]);
    r->pos += 2;
    return true;
}

/**
 * @brief Read 32-bit big-endian value
 */
static bool read_be32(rship_msgpack_reader_t* r, uint32_t* out) {
    if (r->pos + 4 > r->len) {
        return false;
    }
    *out = ((uint32_t)r->buffer[r->pos] << 24) |
           ((uint32_t)r->buffer[r->pos + 1] << 16) |
           ((uint32_t)r->buffer[r->pos + 2] << 8) |
           ((uint32_t)r->buffer[r->pos + 3]);
    r->pos += 4;
    return true;
}

/**
 * @brief Read 64-bit big-endian value
 */
static bool read_be64(rship_msgpack_reader_t* r, uint64_t* out) {
    if (r->pos + 8 > r->len) {
        return false;
    }
    *out = ((uint64_t)r->buffer[r->pos] << 56) |
           ((uint64_t)r->buffer[r->pos + 1] << 48) |
           ((uint64_t)r->buffer[r->pos + 2] << 40) |
           ((uint64_t)r->buffer[r->pos + 3] << 32) |
           ((uint64_t)r->buffer[r->pos + 4] << 24) |
           ((uint64_t)r->buffer[r->pos + 5] << 16) |
           ((uint64_t)r->buffer[r->pos + 6] << 8) |
           ((uint64_t)r->buffer[r->pos + 7]);
    r->pos += 8;
    return true;
}

/*============================================================================
 * Reader Initialization
 *============================================================================*/

void msgpack_reader_init(rship_msgpack_reader_t* r,
                         const uint8_t* buffer,
                         size_t len) {
    r->buffer = buffer;
    r->len = len;
    r->pos = 0;
}

size_t msgpack_reader_remaining(const rship_msgpack_reader_t* r) {
    return r->len - r->pos;
}

bool msgpack_reader_eof(const rship_msgpack_reader_t* r) {
    return r->pos >= r->len;
}

/*============================================================================
 * Type Detection
 *============================================================================*/

msgpack_type_t msgpack_peek_type(const rship_msgpack_reader_t* r) {
    uint8_t tag;
    if (!peek_byte(r, &tag)) {
        return MSGPACK_TYPE_INVALID;
    }

    /* Positive fixint: 0x00 - 0x7f */
    if (tag <= 0x7F) {
        return MSGPACK_TYPE_UINT;
    }

    /* Fixmap: 0x80 - 0x8f */
    if ((tag & 0xF0) == MSGPACK_FIXMAP_MASK) {
        return MSGPACK_TYPE_MAP;
    }

    /* Fixarray: 0x90 - 0x9f */
    if ((tag & 0xF0) == MSGPACK_FIXARRAY_MASK) {
        return MSGPACK_TYPE_ARRAY;
    }

    /* Fixstr: 0xa0 - 0xbf */
    if ((tag & 0xE0) == MSGPACK_FIXSTR_MASK) {
        return MSGPACK_TYPE_STR;
    }

    /* Negative fixint: 0xe0 - 0xff */
    if (tag >= 0xE0) {
        return MSGPACK_TYPE_INT;
    }

    /* Specific tags */
    switch (tag) {
        case MSGPACK_NIL:
            return MSGPACK_TYPE_NIL;

        case MSGPACK_FALSE:
        case MSGPACK_TRUE:
            return MSGPACK_TYPE_BOOL;

        case MSGPACK_BIN8:
        case MSGPACK_BIN16:
        case MSGPACK_BIN32:
            return MSGPACK_TYPE_BIN;

        case MSGPACK_EXT8:
        case MSGPACK_EXT16:
        case MSGPACK_EXT32:
        case MSGPACK_FIXEXT1:
        case MSGPACK_FIXEXT2:
        case MSGPACK_FIXEXT4:
        case MSGPACK_FIXEXT8:
        case MSGPACK_FIXEXT16:
            return MSGPACK_TYPE_EXT;

        case MSGPACK_FLOAT32:
            return MSGPACK_TYPE_FLOAT;

        case MSGPACK_FLOAT64:
            return MSGPACK_TYPE_DOUBLE;

        case MSGPACK_UINT8:
        case MSGPACK_UINT16:
        case MSGPACK_UINT32:
        case MSGPACK_UINT64:
            return MSGPACK_TYPE_UINT;

        case MSGPACK_INT8:
        case MSGPACK_INT16:
        case MSGPACK_INT32:
        case MSGPACK_INT64:
            return MSGPACK_TYPE_INT;

        case MSGPACK_STR8:
        case MSGPACK_STR16:
        case MSGPACK_STR32:
            return MSGPACK_TYPE_STR;

        case MSGPACK_ARRAY16:
        case MSGPACK_ARRAY32:
            return MSGPACK_TYPE_ARRAY;

        case MSGPACK_MAP16:
        case MSGPACK_MAP32:
            return MSGPACK_TYPE_MAP;

        default:
            return MSGPACK_TYPE_INVALID;
    }
}

/*============================================================================
 * Generic Reader
 *============================================================================*/

bool msgpack_read(rship_msgpack_reader_t* r, msgpack_value_t* val) {
    uint8_t tag;

    if (!read_byte(r, &tag)) {
        return false;
    }

    /* Positive fixint: 0x00 - 0x7f */
    if (tag <= 0x7F) {
        val->type = MSGPACK_TYPE_UINT;
        val->v.u64 = tag;
        return true;
    }

    /* Fixmap: 0x80 - 0x8f */
    if ((tag & 0xF0) == MSGPACK_FIXMAP_MASK) {
        val->type = MSGPACK_TYPE_MAP;
        val->v.container.count = tag & 0x0F;
        return true;
    }

    /* Fixarray: 0x90 - 0x9f */
    if ((tag & 0xF0) == MSGPACK_FIXARRAY_MASK) {
        val->type = MSGPACK_TYPE_ARRAY;
        val->v.container.count = tag & 0x0F;
        return true;
    }

    /* Fixstr: 0xa0 - 0xbf */
    if ((tag & 0xE0) == MSGPACK_FIXSTR_MASK) {
        size_t len = tag & 0x1F;
        if (r->pos + len > r->len) {
            r->pos--;  /* Restore tag byte */
            return false;
        }
        val->type = MSGPACK_TYPE_STR;
        val->v.data.ptr = r->buffer + r->pos;
        val->v.data.len = len;
        r->pos += len;
        return true;
    }

    /* Negative fixint: 0xe0 - 0xff */
    if (tag >= 0xE0) {
        val->type = MSGPACK_TYPE_INT;
        val->v.i64 = (int8_t)tag;
        return true;
    }

    /* Handle specific tags */
    switch (tag) {
        case MSGPACK_NIL:
            val->type = MSGPACK_TYPE_NIL;
            return true;

        case MSGPACK_FALSE:
            val->type = MSGPACK_TYPE_BOOL;
            val->v.boolean = false;
            return true;

        case MSGPACK_TRUE:
            val->type = MSGPACK_TYPE_BOOL;
            val->v.boolean = true;
            return true;

        case MSGPACK_BIN8: {
            uint8_t len8;
            if (!read_byte(r, &len8)) return false;
            if (r->pos + len8 > r->len) return false;
            val->type = MSGPACK_TYPE_BIN;
            val->v.data.ptr = r->buffer + r->pos;
            val->v.data.len = len8;
            r->pos += len8;
            return true;
        }

        case MSGPACK_BIN16: {
            uint16_t len16;
            if (!read_be16(r, &len16)) return false;
            if (r->pos + len16 > r->len) return false;
            val->type = MSGPACK_TYPE_BIN;
            val->v.data.ptr = r->buffer + r->pos;
            val->v.data.len = len16;
            r->pos += len16;
            return true;
        }

        case MSGPACK_BIN32: {
            uint32_t len32;
            if (!read_be32(r, &len32)) return false;
            if (r->pos + len32 > r->len) return false;
            val->type = MSGPACK_TYPE_BIN;
            val->v.data.ptr = r->buffer + r->pos;
            val->v.data.len = len32;
            r->pos += len32;
            return true;
        }

        case MSGPACK_FLOAT32: {
            uint32_t bits;
            if (!read_be32(r, &bits)) return false;
            union { float f; uint32_t u; } conv;
            conv.u = bits;
            val->type = MSGPACK_TYPE_FLOAT;
            val->v.f32 = conv.f;
            return true;
        }

        case MSGPACK_FLOAT64: {
            uint64_t bits;
            if (!read_be64(r, &bits)) return false;
            union { double d; uint64_t u; } conv;
            conv.u = bits;
            val->type = MSGPACK_TYPE_DOUBLE;
            val->v.f64 = conv.d;
            return true;
        }

        case MSGPACK_UINT8: {
            uint8_t v;
            if (!read_byte(r, &v)) return false;
            val->type = MSGPACK_TYPE_UINT;
            val->v.u64 = v;
            return true;
        }

        case MSGPACK_UINT16: {
            uint16_t v;
            if (!read_be16(r, &v)) return false;
            val->type = MSGPACK_TYPE_UINT;
            val->v.u64 = v;
            return true;
        }

        case MSGPACK_UINT32: {
            uint32_t v;
            if (!read_be32(r, &v)) return false;
            val->type = MSGPACK_TYPE_UINT;
            val->v.u64 = v;
            return true;
        }

        case MSGPACK_UINT64: {
            uint64_t v;
            if (!read_be64(r, &v)) return false;
            val->type = MSGPACK_TYPE_UINT;
            val->v.u64 = v;
            return true;
        }

        case MSGPACK_INT8: {
            uint8_t v;
            if (!read_byte(r, &v)) return false;
            val->type = MSGPACK_TYPE_INT;
            val->v.i64 = (int8_t)v;
            return true;
        }

        case MSGPACK_INT16: {
            uint16_t v;
            if (!read_be16(r, &v)) return false;
            val->type = MSGPACK_TYPE_INT;
            val->v.i64 = (int16_t)v;
            return true;
        }

        case MSGPACK_INT32: {
            uint32_t v;
            if (!read_be32(r, &v)) return false;
            val->type = MSGPACK_TYPE_INT;
            val->v.i64 = (int32_t)v;
            return true;
        }

        case MSGPACK_INT64: {
            uint64_t v;
            if (!read_be64(r, &v)) return false;
            val->type = MSGPACK_TYPE_INT;
            val->v.i64 = (int64_t)v;
            return true;
        }

        case MSGPACK_STR8: {
            uint8_t len8;
            if (!read_byte(r, &len8)) return false;
            if (r->pos + len8 > r->len) return false;
            val->type = MSGPACK_TYPE_STR;
            val->v.data.ptr = r->buffer + r->pos;
            val->v.data.len = len8;
            r->pos += len8;
            return true;
        }

        case MSGPACK_STR16: {
            uint16_t len16;
            if (!read_be16(r, &len16)) return false;
            if (r->pos + len16 > r->len) return false;
            val->type = MSGPACK_TYPE_STR;
            val->v.data.ptr = r->buffer + r->pos;
            val->v.data.len = len16;
            r->pos += len16;
            return true;
        }

        case MSGPACK_STR32: {
            uint32_t len32;
            if (!read_be32(r, &len32)) return false;
            if (r->pos + len32 > r->len) return false;
            val->type = MSGPACK_TYPE_STR;
            val->v.data.ptr = r->buffer + r->pos;
            val->v.data.len = len32;
            r->pos += len32;
            return true;
        }

        case MSGPACK_ARRAY16: {
            uint16_t count;
            if (!read_be16(r, &count)) return false;
            val->type = MSGPACK_TYPE_ARRAY;
            val->v.container.count = count;
            return true;
        }

        case MSGPACK_ARRAY32: {
            uint32_t count;
            if (!read_be32(r, &count)) return false;
            val->type = MSGPACK_TYPE_ARRAY;
            val->v.container.count = count;
            return true;
        }

        case MSGPACK_MAP16: {
            uint16_t count;
            if (!read_be16(r, &count)) return false;
            val->type = MSGPACK_TYPE_MAP;
            val->v.container.count = count;
            return true;
        }

        case MSGPACK_MAP32: {
            uint32_t count;
            if (!read_be32(r, &count)) return false;
            val->type = MSGPACK_TYPE_MAP;
            val->v.container.count = count;
            return true;
        }

        /* Extension types */
        case MSGPACK_FIXEXT1: {
            if (r->pos + 2 > r->len) return false;
            val->type = MSGPACK_TYPE_EXT;
            val->v.ext.type = (int8_t)r->buffer[r->pos++];
            val->v.ext.ptr = r->buffer + r->pos;
            val->v.ext.len = 1;
            r->pos += 1;
            return true;
        }

        case MSGPACK_FIXEXT2: {
            if (r->pos + 3 > r->len) return false;
            val->type = MSGPACK_TYPE_EXT;
            val->v.ext.type = (int8_t)r->buffer[r->pos++];
            val->v.ext.ptr = r->buffer + r->pos;
            val->v.ext.len = 2;
            r->pos += 2;
            return true;
        }

        case MSGPACK_FIXEXT4: {
            if (r->pos + 5 > r->len) return false;
            val->type = MSGPACK_TYPE_EXT;
            val->v.ext.type = (int8_t)r->buffer[r->pos++];
            val->v.ext.ptr = r->buffer + r->pos;
            val->v.ext.len = 4;
            r->pos += 4;
            return true;
        }

        case MSGPACK_FIXEXT8: {
            if (r->pos + 9 > r->len) return false;
            val->type = MSGPACK_TYPE_EXT;
            val->v.ext.type = (int8_t)r->buffer[r->pos++];
            val->v.ext.ptr = r->buffer + r->pos;
            val->v.ext.len = 8;
            r->pos += 8;
            return true;
        }

        case MSGPACK_FIXEXT16: {
            if (r->pos + 17 > r->len) return false;
            val->type = MSGPACK_TYPE_EXT;
            val->v.ext.type = (int8_t)r->buffer[r->pos++];
            val->v.ext.ptr = r->buffer + r->pos;
            val->v.ext.len = 16;
            r->pos += 16;
            return true;
        }

        case MSGPACK_EXT8: {
            uint8_t len8;
            if (!read_byte(r, &len8)) return false;
            if (r->pos + 1 + len8 > r->len) return false;
            val->type = MSGPACK_TYPE_EXT;
            val->v.ext.type = (int8_t)r->buffer[r->pos++];
            val->v.ext.ptr = r->buffer + r->pos;
            val->v.ext.len = len8;
            r->pos += len8;
            return true;
        }

        case MSGPACK_EXT16: {
            uint16_t len16;
            if (!read_be16(r, &len16)) return false;
            if (r->pos + 1 + len16 > r->len) return false;
            val->type = MSGPACK_TYPE_EXT;
            val->v.ext.type = (int8_t)r->buffer[r->pos++];
            val->v.ext.ptr = r->buffer + r->pos;
            val->v.ext.len = len16;
            r->pos += len16;
            return true;
        }

        case MSGPACK_EXT32: {
            uint32_t len32;
            if (!read_be32(r, &len32)) return false;
            if (r->pos + 1 + len32 > r->len) return false;
            val->type = MSGPACK_TYPE_EXT;
            val->v.ext.type = (int8_t)r->buffer[r->pos++];
            val->v.ext.ptr = r->buffer + r->pos;
            val->v.ext.len = len32;
            r->pos += len32;
            return true;
        }

        default:
            val->type = MSGPACK_TYPE_INVALID;
            return false;
    }
}

/*============================================================================
 * Skip Implementation
 *============================================================================*/

bool msgpack_skip(rship_msgpack_reader_t* r) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) {
        return false;
    }

    /* For containers, we need to skip all elements */
    if (val.type == MSGPACK_TYPE_ARRAY) {
        for (size_t i = 0; i < val.v.container.count; i++) {
            if (!msgpack_skip(r)) {
                return false;
            }
        }
    } else if (val.type == MSGPACK_TYPE_MAP) {
        for (size_t i = 0; i < val.v.container.count; i++) {
            /* Skip key and value */
            if (!msgpack_skip(r) || !msgpack_skip(r)) {
                return false;
            }
        }
    }

    return true;
}

/*============================================================================
 * Convenience Readers
 *============================================================================*/

bool msgpack_read_nil(rship_msgpack_reader_t* r) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) return false;
    return val.type == MSGPACK_TYPE_NIL;
}

bool msgpack_read_bool(rship_msgpack_reader_t* r, bool* out) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) return false;
    if (val.type != MSGPACK_TYPE_BOOL) return false;
    *out = val.v.boolean;
    return true;
}

bool msgpack_read_uint(rship_msgpack_reader_t* r, uint64_t* out) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) return false;

    if (val.type == MSGPACK_TYPE_UINT) {
        *out = val.v.u64;
        return true;
    }

    /* Allow positive signed integers */
    if (val.type == MSGPACK_TYPE_INT && val.v.i64 >= 0) {
        *out = (uint64_t)val.v.i64;
        return true;
    }

    return false;
}

bool msgpack_read_int(rship_msgpack_reader_t* r, int64_t* out) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) return false;

    if (val.type == MSGPACK_TYPE_INT) {
        *out = val.v.i64;
        return true;
    }

    /* Allow unsigned integers that fit in int64 */
    if (val.type == MSGPACK_TYPE_UINT && val.v.u64 <= INT64_MAX) {
        *out = (int64_t)val.v.u64;
        return true;
    }

    return false;
}

bool msgpack_read_double(rship_msgpack_reader_t* r, double* out) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) return false;

    if (val.type == MSGPACK_TYPE_DOUBLE) {
        *out = val.v.f64;
        return true;
    }

    if (val.type == MSGPACK_TYPE_FLOAT) {
        *out = (double)val.v.f32;
        return true;
    }

    return false;
}

bool msgpack_read_str(rship_msgpack_reader_t* r,
                      const char** out,
                      size_t* len) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) return false;
    if (val.type != MSGPACK_TYPE_STR) return false;
    *out = (const char*)val.v.data.ptr;
    *len = val.v.data.len;
    return true;
}

bool msgpack_read_bin(rship_msgpack_reader_t* r,
                      const uint8_t** out,
                      size_t* len) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) return false;
    if (val.type != MSGPACK_TYPE_BIN) return false;
    *out = val.v.data.ptr;
    *len = val.v.data.len;
    return true;
}

bool msgpack_read_array(rship_msgpack_reader_t* r, size_t* count) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) return false;
    if (val.type != MSGPACK_TYPE_ARRAY) return false;
    *count = val.v.container.count;
    return true;
}

bool msgpack_read_map(rship_msgpack_reader_t* r, size_t* count) {
    msgpack_value_t val;
    if (!msgpack_read(r, &val)) return false;
    if (val.type != MSGPACK_TYPE_MAP) return false;
    *count = val.v.container.count;
    return true;
}
