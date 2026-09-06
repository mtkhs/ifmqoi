#pragma once

#include <cstdint>

#define QOI_SRGB 0
#define QOI_LINEAR 1

#define QOI_OP_INDEX 0x00
#define QOI_OP_DIFF 0x40
#define QOI_OP_LUMA 0x80
#define QOI_OP_RUN 0xc0
#define QOI_OP_RGB 0xfe
#define QOI_OP_RGBA 0xff

#define QOI_MASK_2 0xc0

#define QOI_MAGIC \
    (((uint32_t)'q') << 24 | ((uint32_t)'o') << 16 | \
     ((uint32_t)'i') << 8 | ((uint32_t)'f'))

#define QOI_HEADER_SIZE 14
#define QOI_END_MARKER_SIZE 8

#define QOI_PIXELS_MAX ((uint32_t)400000000)

struct qoi_desc {
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t colorspace;
};

struct qoi_rgba {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

inline uint32_t qoi_read_32(const uint8_t* bytes, int* p) {
    uint32_t a = bytes[(*p)++];
    uint32_t b = bytes[(*p)++];
    uint32_t c = bytes[(*p)++];
    uint32_t d = bytes[(*p)++];
    return (a << 24) | (b << 16) | (c << 8) | d;
}

inline int qoi_color_hash(const qoi_rgba& c) {
    return c.r * 3 + c.g * 5 + c.b * 7 + c.a * 11;
}

void* qoi_decode(const void* data, int size, qoi_desc* desc, int channels);
