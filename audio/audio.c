#include "rpi.h"
#include "pwm.h"
#include "fat32.h"
#include "wav.h"

#define GAIN 8.0

void play_wav(fat32_fs_t* fs, pi_dirent_t* root, char* filename, int sample_rate) {
    audio_init(sample_rate);

    pi_file_t *file = fat32_read(fs, root, filename);
    file->data = file->data + sizeof(wav_header_t); // skip header

    int16_t *data = (int16_t*)file->data;

    // Sample each 
    for (unsigned int sample = 0; sample < (file->n_data - sizeof(wav_header_t)) / sizeof(int16_t); sample++) {
        unsigned status = pwm_get_status(); 
        while (status & PWM_FULL1) {
            status = pwm_get_status(); // Will operate under sample rate
        }
        unsigned wave = data[sample] + 0x8000;
        uint8_t pcm = wave>>8;
        pwm_write( pcm );
        pwm_write( pcm );
    }
    

}