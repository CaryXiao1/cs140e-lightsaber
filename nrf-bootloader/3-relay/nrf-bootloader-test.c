///////////////////////////////////////////////
// nrf-bootloader-test.c
// tests sending code from another raspberry pi
// to this one via hello-h.bin from Lab 19.
//
// To use this function, perform the following:
// 1) call "my-install nrf-bootloader-test.bin" on this pi.
// 2) on the other pi, call "my-install-relay hello-f.bin" from the other pi.
// 
// If everything goes right, the information should be 
// sent from the other pi to this pi and this pi should print
// "hello world from address 0x90024"!
///////////////////////////////////////////////

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
static uint32_t
get_code_ack(nrf_t *c, uint32_t server_addr, uint32_t **data_set) {
    // unsigned client_addr = client_address;
    // unsigned server_addr = s->rxaddr;
    unsigned npackets = 0, ntimeout = 0;
    uint32_t exp = 0, got = 0;
    uint32_t wait = 0;
    // wait on client!
    trace("now waiting on relay.");
    while (!net_get32(c, &got)) {
        if (wait++ % 2000 == 0) printk(".");
    }
    printk("\nreceived header, allocating %d bytes to store program.\n", got + 4);
    // first value should be length!
    uint32_t *data = kmalloc(got + 4);
    npackets++;
    for (int i = 0; i < got / 4; i++) {
        if(!net_get32(c, data + i)) {
            ntimeout++;
            i--;
        }
        // else if(got != exp)
        //     nrf_output("server: received %d (expected=%d)\n", got,exp);
        else {
            npackets++;

        }
    }
    trace("trial: total successfully received %d ack'd packets lost [%d]\n",
        npackets, ntimeout);
    printk("printing buffer:\n");
    for (int i = 0; i < 10; i++) {
        printk("i=%d, val=%x\n", i, data[i]);
    }
    *data_set = data;
    return got + 4;
}
 
void notmain(void) {
    // configure client
    trace("send total=%d, %d-byte messages from server=[%x] to client=[%x]\n",
                ntrial, nbytes, server_addr, client_addr);
    nrf_t *c = client_mk_ack(client_addr, nbytes);
 
    nrf_stat_start(c);
 
    // run test.
    uint32_t *data;
    uint32_t size = get_code_ack(c, server_addr, &data);
    // emit stats
    // nrf_stat_print(c, "client: done with one-way test");
    memcpy((uint32_t *)0x90010, data + 4, size - (4 * 5)); // Assuming we're using hello-f.bin
    BRANCHTO(0x90010);
    
    clean_reboot();
}