/*
 * simple test to use your buffered neopixel interface to push a cursor around
 * a light array.
 */
#include "rpi.h"
#include "WS2812B.h"
#include "neopixel.h"

// the pin used to control the light strip.
enum { pix_pin = 14 }; // used to be 21, but is 14 on the lightsaber

// crude routine to write a pixel at a given location.
void place_cursor(neo_t h, int i) {
    neopix_write(h,i-2,0xff,0,0);
    neopix_write(h,i-1,0,0,0xff);
    neopix_write(h,i,0,0xff,0);
    neopix_flush(h);
}

void notmain(void) {
    caches_enable(); 
    gpio_set_output(pix_pin);

    // make sure when you implement the neopixel 
    // interface works and pushes a pixel around your light
    // array.
    unsigned npixels = 56;  // you'll have to figure this out.
    neo_t h = neopix_init(pix_pin, npixels);

    // does 10 increasingly faster loops.
    for(int j = 0; j < 10; j++) {
        output("loop %d\n", j);
        for(int i = 20 - j*2; i < 30 + j; i++) {
            place_cursor(h,i);
            delay_ms(10-j);
        }
        // for(int i = npixels; i > 0; i--) {
        //     place_cursor(h,i);
        //     delay_ms(10-j);
        // }
    }
    output("done!\n");
}
