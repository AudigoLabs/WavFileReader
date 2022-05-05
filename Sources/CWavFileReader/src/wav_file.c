#include "wav_file.h"
#include "wav_file_types.h"

#include <Accelerate/Accelerate.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_NUM_CHANNELS        1
#define MAX_NUM_CHANNELS        2
#define MAX_BYTES_PER_SAMPLE    4
#define READ_BUFFER_SIZE_FRAMES 1024

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

static bool read_header(wav_file_handle_t wav_file, void* ptr, size_t length) {
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
    if (!read_header(wav_file, &riff, sizeof(riff))) {
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
    while (read_header(wav_file, &chunk_header, sizeof(chunk_header))) {
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
            if (!read_header(wav_file, &fmt_data, sizeof(fmt_data))) {
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
    return wav_file_set_offset(wav_file, position * wav_file->sample_rate + 0.5);
}

wav_file_result_t wav_file_set_offset(wav_file_handle_t wav_file, uint32_t offset) {
    if (offset > wav_file->num_frames) {
        return WAV_FILE_RESULT_INVALID_PARAM;
    }
    if (!seek_abs(wav_file, wav_file->data_offset + offset * wav_file->num_channels * wav_file->bytes_per_sample)) {
        return WAV_FILE_RESULT_FILE_ERROR;
    }
    return WAV_FILE_RESULT_SUCCESS;
}

uint32_t wav_file_read(wav_file_handle_t wav_file, float* const* data, uint32_t max_num_frames) {
    const uint32_t LEFT_SHIFT_AMOUNT = 32 - wav_file->bits_per_sample;
    const float MAX_SAMPLE_VALUE = (uint32_t)(1 << (wav_file->bits_per_sample - 1));
    const uint32_t bytes_per_frame = wav_file->bytes_per_sample * wav_file->num_channels;
    uint8_t read_buffer[bytes_per_frame * READ_BUFFER_SIZE_FRAMES];
    uint32_t num_frames_read = 0;
    while (max_num_frames > 0) {
        const uint32_t chunk_frames = max_num_frames < READ_BUFFER_SIZE_FRAMES ? max_num_frames : READ_BUFFER_SIZE_FRAMES;
        const size_t frames_read = fread(read_buffer, bytes_per_frame, chunk_frames, wav_file->file);
        if (frames_read == 0) {
            break;
        }
        for (uint16_t channel = 0; channel < wav_file->num_channels; channel++) {
            // Convert the fixed-point data into a float
            switch (wav_file->bytes_per_sample) {
                case 1:
                    vDSP_vflt8((const char*)&read_buffer[channel * wav_file->bytes_per_sample], wav_file->num_channels, &data[channel][num_frames_read], 1, frames_read);
                    break;
                case 2:
                    vDSP_vflt16((const int16_t*)&read_buffer[channel * wav_file->bytes_per_sample], wav_file->num_channels, &data[channel][num_frames_read], 1, frames_read);
                    break;
                case 3:
                    vDSP_vflt24((const vDSP_int24*)&read_buffer[channel * wav_file->bytes_per_sample], wav_file->num_channels, &data[channel][num_frames_read], 1, frames_read);
                    break;
                case 4:
                    vDSP_vflt32((const int32_t*)&read_buffer[channel * wav_file->bytes_per_sample], wav_file->num_channels, &data[channel][num_frames_read], 1, frames_read);
                    break;
                default:
                    LOG_ERROR("Invalid bytes per sample: %u", wav_file->bytes_per_sample);
                    abort();
            }
            // Scale the float data to be within the range [-1,1]
            vDSP_vsdiv(&data[channel][num_frames_read], 1, &MAX_SAMPLE_VALUE, &data[channel][num_frames_read], 1, frames_read);
        }
        num_frames_read += frames_read;
        max_num_frames -= frames_read;
    }
    return num_frames_read;
}

void wav_file_close(wav_file_handle_t wav_file) {
    fclose(wav_file->file);
    free(wav_file);
}
