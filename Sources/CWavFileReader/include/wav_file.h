#pragma once

#include <inttypes.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    // Success (no error)
    WAV_FILE_RESULT_SUCCESS = 0,

    // Failed to open the wav file
    WAV_FILE_RESULT_OPEN_FAILED,

    // The specified file was not a valid wav file or a file read/seek operation failed
    WAV_FILE_RESULT_FILE_ERROR,

    // The properties (i.e. channel count) of the wav file are not (yet) supported
    WAV_FILE_RESULT_UNSUPPORTED,

    // An invalid parameter was passed
    WAV_FILE_RESULT_INVALID_PARAM,
} wav_file_result_t;

struct wav_file;
typedef struct wav_file* _Null_unspecified wav_file_handle_t;

wav_file_result_t wav_file_open(const char* _Null_unspecified path, wav_file_handle_t* _Nonnull wav_file_out);

uint16_t wav_file_get_num_channels(wav_file_handle_t wav_file);

uint32_t wav_file_get_sample_rate(wav_file_handle_t wav_file);

double wav_file_get_duration(wav_file_handle_t wav_file);

wav_file_result_t wav_file_set_seek(wav_file_handle_t, double position);

wav_file_result_t wav_file_set_offset(wav_file_handle_t, uint32_t offset);

uint32_t wav_file_read(wav_file_handle_t wav_file, float* const _Nonnull* _Nonnull data, uint32_t max_num_frames);

void wav_file_close(wav_file_handle_t wav_file);

#ifdef __cplusplus
};
#endif
