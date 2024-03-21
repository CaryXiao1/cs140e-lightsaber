#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

/* If you call INIT_WAV(), then all defaults are filled out. only attributes left to fill out are 
*  "overall size" and "data_size"
*/
typedef struct __attribute__((packed)) {
    unsigned char riff[4];              // RIFF string
    uint32_t overall_size;          // overall size of file in bytes minus 8 (!!!)
    unsigned char wave[4];              // WAVE string
    unsigned char fmt_chunk_marker[4];  // fmt string with trailing null char
    uint32_t length_of_fmt;             // length of the format data
    uint16_t format_type;               // format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
    uint16_t channels;                      // no.of channels
    uint32_t sample_rate;                   // sampling rate (blocks per second)
    uint32_t byterate;                      // SampleRate * NumChannels * BitsPerSample/8
    uint16_t block_align;                   // NumChannels * BitsPerSample/8
    uint16_t bits_per_sample;               // bits per sample, 8- 8bits, 16- 16 bits etc
    unsigned char data_chunk_header [4];        // DATA string or FLLR string
    uint32_t data_size;                     // NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
} wav_header_t;

#define INIT_WAV_HEADER(W) wav_header_t W = { .riff = {'R', 'I', 'F', 'F'}, .wave = {'W', 'A', 'V', 'E'}, .fmt_chunk_marker = {'f', 'm', 't', ' '}, .length_of_fmt = 16, .format_type = 1, .channels = 1, .sample_rate = 44100, .byterate = 176400, .block_align = 4, .bits_per_sample = 32, .data_chunk_header = {'d', 'a', 't', 'a'}}