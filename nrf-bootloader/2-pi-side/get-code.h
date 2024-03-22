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

#include "nrf-test.h"
 
// useful to mess around with these.
enum { ntrial = 1000, timeout_usec = 1000, nbytes = 4 };
 
// example possible wrapper to recv a 32-bit value.
static int net_get32(nrf_t *nic, uint32_t *out) {
    int ret = nrf_read_exact_timeout(nic, out, 4, timeout_usec);
    if(ret != 4) {
        // debug("receive failed: ret=%d\n", ret);
        return 0;
    }
    return 1;
}
// example possible wrapper to send a 32-bit value.
static void net_put32(nrf_t *nic, uint32_t txaddr, uint32_t x) {
    int ret = nrf_send_ack(nic, txaddr, &x, 4);
    if(ret != 4)
        panic("ret=%d, expected 4\n");
}
 
// send 4 byte packets from <server> to <client>. 
//
// nice thing about loopback is that it's trivial to know what we are
// sending, whether it got through, and do flow-control to coordinate
// sender and receiver.
// returns size of data buffer added and sets the ptr data to the
// start of the program.
static void
get_code_ack(nrf_t *c, uint32_t server_addr) {
    // unsigned client_addr = client_address;
    // unsigned server_addr = s->rxaddr;
    unsigned npackets = 0, ntimeout = 0;
    uint32_t exp = 0, got = 0;
    uint32_t wait = 0;
    // wait on client!
    printk("now waiting on relay.");
    while (!net_get32(c, &got)) {
        if (wait++ % 2000 == 0) printk(".");
    }
    printk("\nreceived header, now getting %d bytes.\n", got);
    printk("bootloader start=%x. end=%x\n", (uint32_t) &put32, (uint32_t) __prog_end__);
    printk("prog start=%x. end=%x\n", ARMBASE, (uint32_t) ARMBASE + got);

    // first value should be length!
    uint32_t *data = (uint32_t *) ARMBASE;
    npackets++;
    for (int i = 0; i < got / 4; i++) {
        if(!net_get32(c, data + i)) {
            ntimeout++;
            i--;
        }
        else {
            npackets++;
        }
    }
    printk("trial: total successfully received %d ack'd packets lost [%d]\n",
        npackets, ntimeout);
    printk("printing buffer:\n");
    for (int i = 0; i < 10; i++) {
        printk("i=%d, val=%x\n", i, data[i]);
    }
    printk("crc32 of program=%d\n", crc32(data, got));
    delay_ms(500);
}

// IMPLEMENT this routine.
//
// Simple bootloader: put all of your code here.
uint32_t get_code(void) {
    // configure client
    nrf_t *c = client_mk_ack(client_addr, nbytes);
 
    nrf_stat_start(c);
 
    // run test.
    uint32_t *data;
    get_code_ack(c, server_addr); // gets code and puts it at ARMBASE
    // emit stats
    // nrf_stat_print(c, "client: done with one-way test");
    // memcpy((uint32_t *) ARMBASE, data + 1, size - 4); // Assuming we're using hello-f.bin
    // BRANCHTO(0x8000);
    // clean_reboot();
    return (uint32_t) ARMBASE;
}
#endif
