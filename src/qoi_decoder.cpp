#include "qoi.h"
#include <cstdlib>
#include <cstring>

void* qoi_decode(const void* data, int size, qoi_desc* desc, int channels) {
    if (data == nullptr || desc == nullptr ||
        (channels != 0 && channels != 3 && channels != 4) ||
        size < QOI_HEADER_SIZE + QOI_END_MARKER_SIZE) {
        return nullptr;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    int p = 0;

    uint32_t magic = qoi_read_32(bytes, &p);
    if (magic != QOI_MAGIC) {
        return nullptr;
    }

    desc->width = qoi_read_32(bytes, &p);
    desc->height = qoi_read_32(bytes, &p);
    desc->channels = bytes[p++];
    desc->colorspace = bytes[p++];

    if (desc->width == 0 || desc->height == 0 ||
        desc->channels < 3 || desc->channels > 4 ||
        desc->colorspace > 1 ||
        desc->height >= QOI_PIXELS_MAX / desc->width) {
        return nullptr;
    }

    if (channels == 0) {
        channels = desc->channels;
    }

    const uint32_t px_len = desc->width * desc->height;
    const size_t px_size = static_cast<size_t>(px_len) * channels;
    uint8_t* pixels = static_cast<uint8_t*>(malloc(px_size));
    if (pixels == nullptr) {
        return nullptr;
    }

    qoi_rgba index[64];
    memset(index, 0, sizeof(index));

    qoi_rgba px;
    px.r = 0;
    px.g = 0;
    px.b = 0;
    px.a = 255;

    uint32_t run = 0;
    size_t chunks_len = size - QOI_END_MARKER_SIZE;

    for (uint32_t px_pos = 0; px_pos < px_len; px_pos++) {
        if (run > 0) {
            run--;
        }
        else if (p < static_cast<int>(chunks_len)) {
            uint8_t b1 = bytes[p++];

            if (b1 == QOI_OP_RGB) {
                px.r = bytes[p++];
                px.g = bytes[p++];
                px.b = bytes[p++];
            }
            else if (b1 == QOI_OP_RGBA) {
                px.r = bytes[p++];
                px.g = bytes[p++];
                px.b = bytes[p++];
                px.a = bytes[p++];
            }
            else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
                px = index[b1];
            }
            else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
                px.r += ((b1 >> 4) & 0x03) - 2;
                px.g += ((b1 >> 2) & 0x03) - 2;
                px.b += (b1 & 0x03) - 2;
            }
            else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
                uint8_t b2 = bytes[p++];
                uint8_t vg = (b1 & 0x3f) - 32;
                px.r += vg - 8 + ((b2 >> 4) & 0x0f);
                px.g += vg;
                px.b += vg - 8 + (b2 & 0x0f);
            }
            else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
                run = (b1 & 0x3f);
            }

            index[qoi_color_hash(px) % 64] = px;
        }

        if (channels == 4) {
            pixels[px_pos * 4 + 0] = px.r;
            pixels[px_pos * 4 + 1] = px.g;
            pixels[px_pos * 4 + 2] = px.b;
            pixels[px_pos * 4 + 3] = px.a;
        }
        else {
            pixels[px_pos * 3 + 0] = px.r;
            pixels[px_pos * 3 + 1] = px.g;
            pixels[px_pos * 3 + 2] = px.b;
        }
    }

    return pixels;
}
