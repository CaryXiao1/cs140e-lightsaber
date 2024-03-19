// bootloader helpers and interface.  The file that includes this
// must provide three routines:
//   uint8_t boot_get8(void): write 8 bits on network.
//   void boot_put8(uint8_t b): read 8 bits on network.
//   boot_has_data(): returns 1 if there is data on the network.
//
// we could provide these as routines in a structure (poor-man's
// OO) but we want the lowest overhead possible given that we want 
// to be able to run without interrupts and finite sized input buffers.
// 
// The first half of the file you don't have to modify (but can!).
// The second is your bootloader.
//  
// much more robust than xmodem, which seems to have bugs in terms of 
// recovery with inopportune timeouts.

#ifndef __GETCODE_H__
#define __GETCODE_H__
#include "boot-crc32.h"  // has the crc32 implementation.
#include "boot-defs.h"   // protocol opcode values.
#include "memmap.h"

/***************************************************************
 * Helper routines.  You shouldn't need to modify these for
 * the initial version.
 */

// return a 32-bit: little endien order.
//
// NOTE: as-written, the code will loop forever if data does
// not show up or the hardware dropped bytes (e.g. b/c 
// you didn't read soon enough)
//
// After you get the simple version working, you should fix 
// it by making a timeout version.
static inline uint32_t boot_get32(void) {
    uint32_t u = boot_get8();
        u |= boot_get8() << 8;
        u |= boot_get8() << 16;
        u |= boot_get8() << 24;
    return u;
}

// send 32-bits on network connection.  (possibly let the
// network implementation override for efficiency)
static inline void boot_put32(uint32_t u) {
    boot_put8((u >> 0)  & 0xff);
    boot_put8((u >> 8)  & 0xff);
    boot_put8((u >> 16) & 0xff);
    boot_put8((u >> 24) & 0xff);
}

// send a string <msg> to the unix side to print.  
// message format:
//  [PRINT_STRING, strlen(msg), <msg>]
//
// DANGER DANGER DANGER!!!  Be careful WHEN you call this!
// DANGER DANGER DANGER!!!  Be careful WHEN you call this!
// DANGER DANGER DANGER!!!  Be careful WHEN you call this!
// Why:
//   1. We do not have interrupts. 
//   2. The UART RX FIFO can only hold 8 bytes before it starts 
//      dropping them.   
//   3. So if you print at the same time the laptop sends data,
//      you will likely lose some, leading to weird bugs.  (multiple
//      people always do this and waste a long time)
//
// So: only call boot_putk right after you have completely 
// received a message and the laptop side is quietly waiting 
// for a response.
static inline void boot_putk(const char *msg) {
    // send the <PRINT_STRING> opcode
    boot_put32(PRINT_STRING);

    unsigned n = strlen(msg);
    // send length.
    boot_put32(strlen(msg));

    // send the bytes in the string [we don't include 0]
    for(n = 0; msg[n]; n++)
        boot_put8(msg[n]);
}

// example of how to use macros to get file and lineno info
// if we don't do the LINE_STR() hack (see assert.h), 
// what happens?
#define boot_todo(msg) \
   boot_err(BOOT_ERROR, __FILE__ ":" LINE_STR() ":TODO:" msg "\n")

// send back an error and die.   note: we have to flush the output
// otherwise rebooting could trash the TX queue (example of a hardware
// race condition since both reboot and TX share hardware state)
static inline void 
boot_err(uint32_t error_opcode, const char *msg) {
    boot_putk(msg);
    boot_put32(error_opcode);
    uart_flush_tx();
    rpi_reboot();
}

/*****************************************************************
 * Your bootloader implementation goes below. 
 *
 * NOTE: f you need to print: use boot_putk.  only do this 
 * when you are *not* waiting for data or you'll lose it.
 */

// wait until:
//   (1) there is data (uart_has_data() == 1): return 1.
//   (2) <timeout> usec expires, return 0.
//
// look at libpi/staff-src/timer.c
//   - call <timer_get_usec()> to get usec
//   - look at <delay_us()> : for how to correctly 
//     wait for <n> microseconds given that the hardware
//     counter can overflow.
static unsigned 
has_data_timeout(unsigned timeout) {
    // boot_todo("has_data_timeout: implement this routine");
    // return 0;
    int start = timer_get_usec();
    int end = timer_get_usec();
    while (end - start < timeout && !uart_has_data()) {
        end = timer_get_usec();
    }
    return uart_has_data();
}

// iterate:
//   - send GET_PROG_INFO to server
//   - call <has_data_timeout(<usec_timeout>)>
//       - if =1 then return.
//       - if =0 then repeat.
//
// NOTE:
//   1. make sure you do the delay right: otherwise you'll
//      keep blasting <GET_PROG_INFO> messages to your laptop
//      which can end in tears.
//   2. rember the green light that blinks on your ttyl device?
//      Its from this loop (since the LED goes on for each 
//      received packet)
static void wait_for_data(unsigned usec_timeout) {
    while (1) {
        boot_put32(GET_PROG_INFO);
        if (has_data_timeout(usec_timeout)) return;
    }

}

// IMPLEMENT this routine.
//
// Simple bootloader: put all of your code here.
uint32_t get_code(void) {
    // 0. keep sending GET_PROG_INFO every 300ms until 
    // there is data: implement this.
    wait_for_data(300 * 1000);

    /****************************************************************
     * Add your code below: 2,3,4,5,6
     */
    uint32_t addr = 0;

    uint32_t op = boot_get32();
    if (op != PUT_PROG_INFO) boot_err(op, "PUT_PROG_INFO validation incorrect.\n");
    addr = boot_get32();
    uint32_t n = boot_get32();
    uint32_t cksum = boot_get32();

    // 3. If the binary will collide with us, abort with a BOOT_ERROR. 
    // 
    //    check that the sent code (<base_addr> through 
    //    <base_addr>+<nbytes>) doesn't collide with
    //    the bootloader code using the address of <PUT32>
    //    (the first code address we need) to __prog_end__
    //    (the last).
    //
    //    refer back to:
    //       - your gprof lab code
    //       - libpi/include/memmap.h
    //       - libpi/memmap 
    //    for definitions.
    // boot_todo("check that binary will not hit the bootloader code");
    // check for collision
    if ((addr >= (uint32_t) &put32 && addr <= (uint32_t) __prog_end__) || 
            ((addr + n) >= (uint32_t) &put32 && (addr + n) <= (uint32_t) __prog_end__)) {
        boot_err(BOOT_ERROR, "Code Collision");
    }

    // 4. send [GET_CODE, cksum] back.
    // boot_todo("send [GET_CODE, cksum] back\n");
    boot_put32(GET_CODE);
    boot_put32(cksum);

    // 5. we expect: [PUT_CODE, <code>]
    //  read each sent byte and write it starting at 
    //  <addr> using PUT8
    //
    // common mistake: computing the offset incorrectly.
    // boot_todo("boot_get8() each code byte and use PUT8() to write it to memory");
    op = boot_get32();
    if (op != PUT_CODE) boot_err(op, "PUT_CODE validation incorrect");
    for (int i = 0; i < n; i++) {
        uint8_t val = boot_get8();
        put8((void *) addr + i, val);
    }


    // 6. verify the cksum of the copied code using:
    //         boot-crc32.h:crc32.
    //    if fails, abort with a BOOT_ERROR.
    // boot_todo("verify the checksum of copied code");
    if (cksum != crc32((void *) addr, n)) boot_err(cksum, "checksum validation failed");

    // 7. send back a BOOT_SUCCESS!
    boot_putk("Cary: success: Received the program!");
    boot_putk("Cary: uses my UART!");
    // boot_todo("fill in your name above");

    // woo!
    boot_put32(BOOT_SUCCESS);

    // We used to have these delays to stop the unix side from getting 
    // confused.  I believe it's b/c the code we call re-initializes the 
    // uart.  Instead we now flush the hardware tx buffer.   If this
    // isn't working, put the delay back.  However, it makes it much faster
    // to test multiple programs without it.
    // delay_ms(500);
    uart_flush_tx();

    return addr;
}
#endif
