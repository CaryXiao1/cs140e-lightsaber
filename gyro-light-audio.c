// engler: simplistic mpu6050 gyro driver code, mirrors
// <driver-accel.c>.  
//  1. initializes gyroscope,
//  2. prints N readings.
//
// as with <driver-accel.c>:
//   - <mpu-6050.h> has the interface description.
//   - <mpu-6050.c> is where your code will go.
//
//
// Obvious extension:
//  0. validate the 'data ready' matches what we set it to.
//  1. device interrupts.
//  2. bit bang i2c
//  3. multiple devices.
//  4. extend the interface to give more control.
//  
//
// KEY: document why you are doing what you are doing.
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
// 
// also: a sentence or two will go a long way in a year when you want 
// to re-use the code.
#include "rpi.h"
#include "mpu-6050.h"
#include "neopixel/WS2812B.h"
#include "neopixel/neopixel.h"
#include "fat32.h"
#include "wav.h"
#include "audio.h"
#include "pwm.h"

#define SAMPLE_RATE 44100
#define GAIN 8.0
#define FILENAME "LIT.WAV"

int clamp(int x, int min, int max) {
    if (x < min) return min; 
    if (x > max) return max; 
    return x; 
}

// the pin used to control the light strip.
enum { pix_pin = 17 };

// crude routine to write a pixel at a given location.
void lights_on(neo_t h, int npixels, int intensity) {
    int i = 0; // shift;
    while (i < 64) {
        // g, r, b
        neopix_write(h,i, 0x0, intensity ,0x0);
        i++;
    }

    neopix_flush(h);
}

void dummy_pwm_write() {
    unsigned status = pwm_get_status();
    while (status & PWM_FULL1) {
        status = pwm_get_status();
    }
    pwm_write( 128);
    pwm_write(128);
}

void notmain(void) {
    delay_ms(100);   // allow time for i2c/device to boot up.
    i2c_init(); // intialize i2c
    delay_ms(100);   // allow time for i2c/dev to settle after init.

    // from application note.
    uint8_t dev_addr = 0b1101000;

    enum { 
        WHO_AM_I_REG  = 0x75, 
        WHO_AM_I_VAL = 0x68,       
    };

    uint8_t v = imu_rd(dev_addr, WHO_AM_I_REG);
    if(v != WHO_AM_I_VAL)
        panic("Initial probe failed: expected %b (%x), have %b (%x)\n", 
            WHO_AM_I_VAL, WHO_AM_I_VAL, v, v);
    printk("SUCCESS: mpu-6050 acknowledged our ping: WHO_AM_I=%b!!\n", v);

    // hard reset: it won't be when your pi reboots.
    mpu6050_reset(dev_addr);

    // part 2: get the gyro working.
    gyro_t g = mpu6050_gyro_init(dev_addr, gyro_500dps);
    assert(g.dps==500);

    // neopixel init
    int i = 0;
    unsigned npixels = 100;  // you'll have to figure this out.
    neo_t h = neopix_init(pix_pin, npixels);

    kmalloc_init();
    pi_sd_init();

    gpio_set_output(pix_pin);

    fat32_fs_t fs;
    pi_dirent_t root;
    config_fs(&fs, &root);
    audio_init(SAMPLE_RATE);

    // init fat32 filesystem
    pi_file_t *file = fat32_read(&fs, &root, FILENAME);
    // skip header
    // TODO: view header
    wav_header_t *header = (wav_header_t*)(file->data); 
    printk("WAV sample rate: %d\n", header->sample_rate);  // confirm sample rate = 44100
    file->data = file->data + sizeof(wav_header_t);
    int16_t *data = (int16_t*)file->data;
    int sample = 0; 
    int sample_interval = 1000; 
    int max_sample = (file->n_data - sizeof(wav_header_t)) / sizeof(int16_t); 
    printk("max_sample: %d\n", max_sample);
    while(1) {
        imu_xyz_t xyz_raw = gyro_rd(&g);
        uint32_t g_const = 25000; // 25000 for swing, TODO: figure out the tracking of collisions
        uint32_t scale_max = g_const * g_const; 
        // dummy_pwm_write(); 

        // get length of overall movement from 3 component
        uint32_t overall = xyz_raw.y * xyz_raw.y + xyz_raw.z * xyz_raw.z; // xyz_ra w.x * xyz_raw.x + 
        float accel_norm = clamp(overall, 100000, scale_max) / (float)scale_max; // normalize to between 0 and 1
        uint32_t accel_scale = clamp(accel_norm * 255, 10, 255); 
        // dummy_pwm_write(); 

        // Set lights depending on acceleration
        lights_on(h, npixels, accel_scale); 
        // printk("swing! id=%d, %d, %d\n", i, accel_scale);

    
        int sample_end = sample + sample_interval; 
        while(sample < sample_end && sample < max_sample) {
            unsigned status = pwm_get_status();
            while (status & PWM_FULL1) {
                status = pwm_get_status();
            }
            unsigned wave = data[sample] + 0x8000; // add 0x8000 to make unsigned and within pwm range
            uint8_t pcm = wave>>8; // Convert to an 8-bit PCM (pulse code modulation) output
            // printk("pcm: %d\n", pcm);
            pwm_write( pcm );
            pwm_write( pcm );
            sample++; 
        }

        if (sample == max_sample) sample = 0; 

    }
}
