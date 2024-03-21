#include "rpi.h"
#include "pwm.h"
#include "fat32.h"
#include "wav.h"
#include "audio.h"
#include "neopixel/WS2812B.h"
#include "neopixel/neopixel.h"

// the pin used to control the light strip.
enum { pix_pin = 21 }; 

void lights_on(neo_t h, int n_pixels) {
    for(int i = 0; i < n_pixels; i++) {
        // g, r, b
        neopix_write(h, i, 0, 0xff, 0);
    }   
    neopix_flush(h); 
}

// crude routine to write a pixel at a given location.
void place_cursor(neo_t h, int i) {
    neopix_write(h,i-2,0xff,0,0);
    neopix_write(h,i-1,0,0xff,0);
    neopix_write(h,i,0,0,0xff);
    neopix_flush(h);
}

#define SAMPLE_RATE 44100
#define GAIN 8.0
void play_wav_light(fat32_fs_t* fs, pi_dirent_t* root, char* filename, int sample_rate, neo_t h, int npixels) {
    if (sample_rate == 8000) {
        audio_init(44100);
    } else {
        audio_init(sample_rate);
    }

    pi_file_t *file = fat32_read(fs, root, filename);

    // skip header
    file->data = file->data + sizeof(wav_header_t);

    int16_t *data = (int16_t*)file->data;

    if (sample_rate == 8000) {
        for (unsigned int sample = 0; sample < (file->n_data - sizeof(wav_header_t)) / sizeof(int16_t) * 5.5125; sample++) {
            unsigned wave = data[(int)(sample / 5.5125)] + 0x8000;
            uint8_t pcm = wave>>8;
            double dsample = ((double) pcm - 128) * GAIN + 128;
            if (dsample > 255.0) dsample = 255.0;
            if (dsample < 0.0) dsample = 0.0;
            pcm = (uint8_t) dsample;
            unsigned status = pwm_get_status();
            while (status & PWM_FULL1) {
                status = pwm_get_status();
            }
            pwm_write( pcm );
            pwm_write( pcm );
        }
    } else {
        for (unsigned int sample = 0; sample < (file->n_data - sizeof(wav_header_t)) / sizeof(int16_t); sample++) {
            unsigned status = pwm_get_status();
            while (status & PWM_FULL1) {
                status = pwm_get_status();
            }
            unsigned wave = data[sample] + 0x8000;
            uint8_t pcm = wave>>8;
            pwm_write( pcm );
            pwm_write( pcm );

            // output("loop %d\n", sample);
            // for(int i = 0; i < npixels; i++) {
            //     place_cursor(h,i);
            //     delay_ms(10); 
            //     // delay_ms(10-sample);
            // }
        }
    }

}

void notmain() {
    kmalloc_init();
    pi_sd_init();

    gpio_set_output(pix_pin);

    // make sure when you implement the neopixel 
    // interface works and pushes a pixel around your light
    // array.
    unsigned npixels = 56;  // you'll have to figure this out.
    neo_t h = neopix_init(pix_pin, npixels);

    fat32_fs_t fs;
    pi_dirent_t root;
    config_fs(&fs, &root);
    printk("About to play wav."); 

    lights_on(h, npixels); 
    play_wav_light(&fs, &root, "LIT.WAV", 44100, h, npixels);
    
}