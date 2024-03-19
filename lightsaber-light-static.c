/*
 * simple test to use your buffered neopixel interface to push a cursor around
 * a light array.
 */
#include "rpi.h"
#include "WS2812B.h"
#include "neopixel.h"

// the pin used to control the light strip.
enum { pix_pin = 21 };



// crude routine to write a pixel at a given location.
void place_cursor_rainbow(neo_t h, int shift) {
    int i = 0; // shift;
    while (i < 64) {
        neopix_write(h,i,0x0,0x88,0x0);
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
    unsigned npixels = 100;  // you'll have to figure this out.
    neo_t h = neopix_init(pix_pin, npixels);

    // does 10 increasingly faster loops.
    for(int j = 0; j < 10; j++) {
        output("loop %d\n", j);
        for(int i = 0; i < npixels; i++) {
            place_cursor_rainbow(h,i % 2);
            delay_ms(100);
        }
    }
    output("done!\n");
}
