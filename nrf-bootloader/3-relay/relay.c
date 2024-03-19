////////////////////////////////////////////////////////////////
// relay.c
// author: Cary Xiao
// 
// This is the first program sent to the USB-connected pi, which
// enables the USB-connected pi to send the second program
// via NFC.   
// 
// NOTE!!!!! this program only runs correctly under my-install-relay.
////////////////////////////////////////////////////////////////

#include "rpi.h"
#include "memmap.h"
#include "nrf-test.h"

enum {
    START = 0x8000, // address at the start of the program
};

void notmain(void) {
    printk("Starting up relay on UART pi.\n");
    // test out the bss start and end code, see if it equals the size of the overall stuff
    uint32_t *end = __prog_end__;
    uint8_t *size = ((uint8_t *)end - START);
    // if ((addr >= (uint32_t) &put32 && addr <= (uint32_t) __prog_end__) || 

    printk("start=%x, end=%x, diff=%d\n", START, end, size);
    
    // look at header
    uint32_t *prog_2_start = __prog_end__ + 1; // 8 = 32 / sizeof(uint32_t)

// set if to 1 below to debug the start location / first bits. Can run 
// "my-install-relay relay.bin" to ensure each address matches up exactly. 
#if 0
    printk("Comparing start of program with program past end.\n");
    for (int i = 0; i < 50; i++) {
        uint32_t instr_start = *((uint32_t *)START + i);
        uint32_t instr_post = *(prog_2_start + i);
        printk("i=%d, val=%x      vs.       val=%x\n", i, instr_start, instr_post);
        // assert(instr_start == instr_post);
    }
#endif

    uint32_t s2 = *(prog_2_start - 1);

    printk("2nd program size=%d\n", s2);

    // TODO: continue on this code above to determine how to get the second program

    /*
    Action items:
    - figure out how to get the second file
    - figure out how to send the code via NRF
    - figure out how to receive the code in bootloader
    - figure out how to boot the code that way!
    - Also look into the cstart stuff probably
    */

    // uint8_t *program = NULL; // TODO: change to OG 

    // unsigned nbytes = 4; // TODO: change to size of the program

    // nrf_t *c = client_mk_ack(client_addr, nbytes);


    // trace("configuring reliable (acked) server=[%x] with %d nbyte msgs\n",
                // server_addr, nbytes);
    // putk("hello world!");
    // nrf_t *s = server_mk_ack(server_addr, nbytes);

    // reset the times so we get a bit better measurement.
    // nrf_stat_start(s);
    // nrf_stat_start(c);

    // run test.
    // one_way_ack(s, c, 1);

    // emit all the stats.
    // nrf_stat_print(s, "server: done with one-way test");
    // nrf_stat_print(c, "client: done with one-way test");
    clean_reboot();
}