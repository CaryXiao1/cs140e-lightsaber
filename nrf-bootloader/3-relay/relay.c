////////////////////////////////////////////////////////////////
// relay.c
// author: Cary Xiao
// 
// This is the first program sent to the USB-connected pi, which
// enables the USB-connected pi to send the second program
// via NFC.   
////////////////////////////////////////////////////////////////

#include "rpi.h"
#include "memmap.h"

// #include "boot-crc32.h"  // has the crc32 implementation.
// #include "boot-defs.h"   // protocol opcode values.
// #include "put-code.h"
// #include "boot-crc32.h"
// #include "boot-defs.h"
// #include "nrf-default-values.h"

void notmain(void) {
    printk("Starting up relay on UART pi.\n");
    // test out the bss start and end code, see if it equals the size of the overall stuff
    uint32_t *start = (uint32_t *) &put32;
    uint32_t *end = __prog_end__;
    // if ((addr >= (uint32_t) &put32 && addr <= (uint32_t) __prog_end__) || 

    printk("start=%x, end=%x, diff=%d\n", start, end, ((uint8_t *)end) - ((uint8_t *)start));
    
    // look at header
    uint32_t *prog_2_start = __prog_end__ + 8; // 8 = 32 / sizeof(uint32_t)

    for (int i = 0; i < 50; i++) {
        printk("i=%d, val=%d\n", i, *(prog_2_start + i));
    }


    printk("target addr=%x\n", prog_2_start);

    printk("bss=%x, bss_end=%x\n", __bss_start__, __bss_end__);
    printk("header length=%d\n", *prog_2_start);
    printk("size of prog2=%d\n", *(prog_2_start + 1));

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