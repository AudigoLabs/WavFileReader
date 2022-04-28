//
//  wav_file_types.h
//  Audigo-iOS-App-R2
//
//  Created by Brian Gomberg on 4/27/22.
//  Copyright © 2022 Audigo Labs, Inc. All rights reserved.
//

#pragma once

#include <inttypes.h>

typedef struct __attribute__((packed)) {
    char id[4];
    uint32_t size;
} wav_chunk_header_t;
_Static_assert(sizeof(wav_chunk_header_t) == 8, "Invalid size");

typedef struct __attribute__((packed)) {
    wav_chunk_header_t header;
    char format[4];
} wav_riff_chunk_desc_t;
_Static_assert(sizeof(wav_riff_chunk_desc_t) == 12, "Invalid size");

typedef struct __attribute__((packed)) {
    enum {
        FORMAT_PCM = 1,
    } format : 16;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t bytes_per_second;
    uint16_t bytes_per_frame;
    uint16_t bits_per_sample;
} wav_fmt_sub_chunk_data_t;
_Static_assert(sizeof(wav_fmt_sub_chunk_data_t) == 16, "Invalid size");
