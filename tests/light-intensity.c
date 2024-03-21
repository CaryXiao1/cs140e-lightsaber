/*
 * simple test to use your buffered neopixel interface to push a cursor around
 * a light array.
 */
#include "rpi.h"
#include "neopixel/WS2812B.h"
#include "neopixel/neopixel.h"

// the pin used to control the light strip.
enum { pix_pin = 21 };

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

void notmain(void) {
    caches_is_enabled(); 
    gpio_set_output(pix_pin);

    // make sure when you implement the neopixel 
    // interface works and pushes a pixel around your light
    // array.
    unsigned iters = 1000; 
    unsigned npixels = 100;  // you'll have to figure this out.
    neo_t h = neopix_init(pix_pin, npixels);

    // does 10 increasingly faster loops.
    for(int j = 0; j < iters; j++) {
        output("loop %d\n", j);
        lights_on(h, npixels, j / 4); 
        delay_ms(10); 
    }
    output("done!\n");
}
