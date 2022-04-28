#include "wav_file.h"
#include "wav_file_types.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_NUM_CHANNELS        1
#define MAX_NUM_CHANNELS        2
#define MAX_BYTES_PER_SAMPLE    4

#define _STR1(S) #S
#define _STR2(S) _STR1(S)
#define _LOG_LOCATION __FILE__ ":" _STR2(__LINE__)
#define LOG_ERROR(FMT, ...) printf("ERROR [" _LOG_LOCATION "]" FMT "\n", ##__VA_ARGS__)

struct wav_file {
    FILE* file;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bytes_per_sample;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t num_frames;
};

static bool read(wav_file_handle_t wav_file, void* ptr, size_t length) {
    return fread(ptr, length, 1, wav_file->file) == 1;
}

static bool seek_rel(wav_file_handle_t wav_file, int32_t offset) {
    return fseek(wav_file->file, offset, SEEK_CUR) == 0;
}

static bool seek_abs(wav_file_handle_t wav_file, uint32_t position) {
    return fseek(wav_file->file, position, SEEK_SET) == 0;
}

static wav_file_result_t parse_file(wav_file_handle_t wav_file) {
    // Parse the RIFF header
    wav_riff_chunk_desc_t riff;
    if (!read(wav_file, &riff, sizeof(riff))) {
        LOG_ERROR("Failed to read RIFF header");
        return WAV_FILE_RESULT_FILE_ERROR;
    } else if (memcmp(riff.header.id, "RIFF", 4)) {
        LOG_ERROR("Invalid RIFF header id");
        return WAV_FILE_RESULT_FILE_ERROR;
    } else if (memcmp(riff.format, "WAVE", 4)) {
        LOG_ERROR("Invalid RIFF header format");
        return WAV_FILE_RESULT_FILE_ERROR;
    }

    // Parse each sub-chunk
    wav_chunk_header_t chunk_header;
    while (read(wav_file, &chunk_header, sizeof(chunk_header))) {
        const long offset_raw = ftell(wav_file->file);
        if (offset_raw < 0) {
            LOG_ERROR("Failed to get file position (errno=%d)", errno);
            return WAV_FILE_RESULT_FILE_ERROR;
        } else if (offset_raw > UINT32_MAX) {
            LOG_ERROR("Invalid file offset (%ld)", offset_raw);
            return WAV_FILE_RESULT_FILE_ERROR;
        }
        const uint32_t offset = (uint32_t)offset_raw;

        if (!memcmp(chunk_header.id, "JUNK", sizeof(chunk_header.id)) || !memcmp(chunk_header.id, "FLLR", sizeof(chunk_header.id))) {
            // Ignore these sub-chunks
            if (!seek_rel(wav_file, chunk_header.size)) {
                LOG_ERROR("Failed to seek past sub-chunk");
                return WAV_FILE_RESULT_FILE_ERROR;
            }
        } else if (!memcmp(chunk_header.id, "fmt ", sizeof(chunk_header.id))) {
            wav_fmt_sub_chunk_data_t fmt_data;
            if (!read(wav_file, &fmt_data, sizeof(fmt_data))) {
                LOG_ERROR("Failed to read fmt sub-chunk data");
                return WAV_FILE_RESULT_FILE_ERROR;
            } else if (fmt_data.format != FORMAT_PCM) {
                LOG_ERROR("Unsupported format (%u)", fmt_data.format);
                return WAV_FILE_RESULT_FILE_ERROR;
            } else if (fmt_data.num_channels < MIN_NUM_CHANNELS || fmt_data.num_channels > MAX_NUM_CHANNELS) {
                LOG_ERROR("Unsupported number of channels (%u)", fmt_data.num_channels);
                return WAV_FILE_RESULT_FILE_ERROR;
            } else if ((fmt_data.bytes_per_frame % fmt_data.num_channels) || ((fmt_data.bytes_per_frame / fmt_data.num_channels) > MAX_BYTES_PER_SAMPLE)) {
                LOG_ERROR("Invalid bytes per frame (%u)", fmt_data.bytes_per_frame);
                return WAV_FILE_RESULT_FILE_ERROR;
            } else if (fmt_data.bytes_per_second != fmt_data.sample_rate * fmt_data.num_channels * fmt_data.bits_per_sample / 8) {
                LOG_ERROR("Invalid bytes per second (bytes_per_second=%u, sample_rate=%u, num_channels=%u, bits_per_sample=%u)", fmt_data.bytes_per_second, fmt_data.sample_rate, fmt_data.num_channels, fmt_data.bits_per_sample);
                return WAV_FILE_RESULT_FILE_ERROR;
            }
            wav_file->num_channels = fmt_data.num_channels;
            wav_file->sample_rate = fmt_data.sample_rate;
            wav_file->bytes_per_sample = fmt_data.bytes_per_frame / fmt_data.num_channels;
            wav_file->bits_per_sample = fmt_data.bits_per_sample;
        } else if (!memcmp(chunk_header.id, "data", sizeof(chunk_header.id))) {
            if (wav_file->data_offset) {
                LOG_ERROR("Multiple data sub-chunks");
                return WAV_FILE_RESULT_FILE_ERROR;
            } else if (!wav_file->bytes_per_sample) {
                LOG_ERROR("Data sub-chunk came before fmt sub-chunk");
                return WAV_FILE_RESULT_FILE_ERROR;
            }
            wav_file->data_offset = offset;
            wav_file->num_frames = chunk_header.size / (wav_file->bytes_per_sample * wav_file->num_channels);
            if (!seek_rel(wav_file, chunk_header.size)) {
                LOG_ERROR("Failed to seek past sub-chunk");
                return WAV_FILE_RESULT_FILE_ERROR;
            }
        } else {
            // Ignore unknown sub-chunks
            if (!seek_rel(wav_file, chunk_header.size)) {
                LOG_ERROR("Failed to seek past sub-chunk");
                return WAV_FILE_RESULT_FILE_ERROR;
            }
        }
    }

    // Make sure we have data and seek to the start of it
    if (!wav_file->data_offset) {
        LOG_ERROR("No data sub-chunk found");
        return WAV_FILE_RESULT_FILE_ERROR;
    } else if (!seek_abs(wav_file, wav_file->data_offset)) {
        LOG_ERROR("Failed to seek to data");
        return WAV_FILE_RESULT_FILE_ERROR;
    }

    return WAV_FILE_RESULT_SUCCESS;
}

wav_file_result_t wav_file_open(const char* path, wav_file_handle_t* wav_file_out) {
    // Open the file
    FILE* file = fopen(path, "r");
    if (!file) {
        return WAV_FILE_RESULT_OPEN_FAILED;
    }

    // Allocate a file handle
    wav_file_handle_t wav_file = malloc(sizeof(struct wav_file));
    memset(wav_file, 0, sizeof(struct wav_file));
    wav_file->file = file;

    // Parse the file
    const wav_file_result_t result = parse_file(wav_file);
    if (result != WAV_FILE_RESULT_SUCCESS) {
        wav_file_close(wav_file);
        return result;
    }

    // Return the handle
    *wav_file_out = wav_file;
    return WAV_FILE_RESULT_SUCCESS;
}

uint16_t wav_file_get_num_channels(wav_file_handle_t wav_file) {
    return wav_file->num_channels;
}

uint32_t wav_file_get_sample_rate(wav_file_handle_t wav_file) {
    return wav_file->sample_rate;
}

double wav_file_get_duration(wav_file_handle_t wav_file) {
    return (double)wav_file->num_frames / wav_file->sample_rate;
}

wav_file_result_t wav_file_set_seek(wav_file_handle_t wav_file, double position) {
    if (position > wav_file_get_duration(wav_file)) {
        return WAV_FILE_RESULT_INVALID_PARAM;
    }
    const uint32_t frame_offset = position * wav_file->sample_rate + 0.5;
    if (frame_offset > wav_file->num_frames) {
        return WAV_FILE_RESULT_INVALID_PARAM;
    }
    if (!seek_abs(wav_file, wav_file->data_offset + frame_offset * wav_file->num_channels * wav_file->bytes_per_sample)) {
        return WAV_FILE_RESULT_FILE_ERROR;
    }
    return WAV_FILE_RESULT_SUCCESS;
}

uint32_t wav_file_read(wav_file_handle_t wav_file, float* const* data, uint32_t max_num_frames) {
    const float MAX_SAMPLE_VALUE = (uint32_t)(1 << 31);
    uint8_t frame_buffer[wav_file->bytes_per_sample * 2];
    for (uint32_t frame = 0; frame < max_num_frames; frame++) {
        if (fread(frame_buffer, wav_file->bytes_per_sample * 2, 1, wav_file->file) != 1) {
            return frame;
        }
        for (uint16_t channel = 0; channel < wav_file->num_channels; channel++) {
            uint32_t temp = 0;
            memcpy(&temp, &frame_buffer[wav_file->bytes_per_sample * channel], wav_file->bytes_per_sample);
            temp <<= (32 - wav_file->bits_per_sample);
            data[channel][frame] = ((float)(int32_t)temp) / MAX_SAMPLE_VALUE;
        }
    }
    return max_num_frames;
}

void wav_file_close(wav_file_handle_t wav_file) {
    fclose(wav_file->file);
    free(wav_file);
}
