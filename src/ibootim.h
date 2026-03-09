//
//  ibootim.h
//  ios1-jb
//
//  Created by Ethan Arbuckle on 3/6/26.
//

#ifndef IBOOTIM_CONVERT_H
#define IBOOTIM_CONVERT_H

#include <stdint.h>
#include <stddef.h>
#include <mach/mach.h>
#include "file_payload.h"

#define IMG2_SIGNATURE     0x496D6732
#define IBOOTIM_SIGNATURE  "iBootIm"
#define IBOOTIM_LZSS_TYPE  0x6C7A7373
#define IBOOTIM_ARGB       0x61726762
#define IBOOTIM_GREY       0x67726579

#pragma pack(push, 1)

typedef struct {
    uint32_t signature;
    uint32_t imageType;
    uint16_t unknown1;
    uint16_t security_epoch;
    uint32_t flags1;
    uint32_t dataLenPadded;
    uint32_t dataLen;
    uint32_t unknown3;
    uint32_t flags2;
    uint8_t reserved[0x40];
    uint32_t unknown4;
    uint32_t header_checksum;
    uint32_t checksum2;
    uint8_t unknown5[0x394];
} img2_inner_header_t;

typedef struct {
    char signature[8];
    uint32_t unknown;
    uint32_t compression_type;
    uint32_t format;
    uint16_t width;
    uint16_t height;
    int16_t offset_x;
    int16_t offset_y;
    uint32_t compressed_size;
    uint32_t reserved[8];
} ibootim_header_t;

#pragma pack(pop)

kern_return_t ibootim_png_to_raw(const payload_t *png_payload, const payload_t *template_payload, uint8_t **out_buf, size_t *out_size);

#endif
