#include "rpi.h"
#include "staff-pwm.h"
#include "fat32.h"
#include "wav.h"
#include "audio.h"

#define SAMPLE_RATE 44100

void notmain() {
    kmalloc_init();
    pi_sd_init();

    fat32_fs_t fs;
    pi_dirent_t root;
    config_fs(&fs, &root);
    printk("About to play wav."); 
    play_wav(&fs, &root, "LIT.WAV", 44100);
}