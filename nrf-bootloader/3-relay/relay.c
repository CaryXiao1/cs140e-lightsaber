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


#if 0
#include "nrf-test.h"
#include "nrf-default-values.h"

// max message size.
typedef struct {
    uint32_t data[NRF_PKT_MAX/4];
} data_t;
_Static_assert(sizeof(data_t) == NRF_PKT_MAX, "invalid size");

// simple method for sending 32 bits: copy into a <data_t> struct,
// send <nbytes> from that.
static inline int send32_ack(nrf_t *nic, uint32_t txaddr, uint32_t x) {
    assert(nbytes>=4);
    data_t d = {0};
    d.data[0] = x;
    return nrf_send_ack(nic, txaddr, &d, nbytes) == nbytes;
}

// simple method for receiing 32 bits: receive into a <data_t> struct,
// copy payload out of that.
static inline int recv32(nrf_t *nic, uint32_t *out) {
    assert(nbytes>=4);
    data_t d;
    int ret = nrf_read_exact_timeout(nic, &d, nbytes, timeout_usec);
    assert(ret<= nbytes);
    if(ret == nbytes)
        memcpy(out, &d, 4);
    return ret;
}

// modified from code-nrf/tests/3-one-way-ack-Nbytes.c.
// 
static void
one_way_ack(nrf_t *server, uint32_t client_addr, uint32_t *data, uint32_t size, int verbose_p) {
    unsigned ntimeout = 0, npackets = 0;
    // figure out if we need to deal with case where size % 4 != 0
    assert(size % 4 == 0);
    for(unsigned i = 0; i < size / 4; i++) {
        if(verbose_p && i  && i % 100 == 0)
            trace("sent %d ack'd packets\n", i);

        // output("sent %d\n", i);
        if(!send32_ack(server, client_addr, *(data + i)))
            panic("send failed\n");

        // uint32_t x;
        // int ret = recv32(client, &x);
        // output("ret=%d, got %d\n", ret, x);
        // if(ret == nbytes) {
        //     if(x != i)
        //         nrf_output("client: received %d (expected=%d)\n", x,i);
        //     assert(x == i);
        //     npackets++;
        // } else {
        //     if(verbose_p) 
        //         output("receive failed for packet=%d, nbytes=%d ret=%d\n", i, nbytes, ret);
        //     ntimeout++;
        // }
    }
    // trace("trial: total successfully sent %d ack'd packets lost [%d]\n",
    //     npackets, ntimeout);
    trace("finished sending code.");
    // assert((ntimeout + npackets) == ntrial);
}
#endif

enum {
    START = 0x8000, // address at the start of the program
    timeout_usec = 1000, 
    nbytes = 32
};

void notmain(void) {
    printk("Starting up relay on UART pi.\n");
    // test out the bss start and end code, see if it equals the size of the overall stuff
    uint32_t *end = __prog_end__;
    uint8_t *size = ((uint8_t *)end - START);
    // if ((addr >= (uint32_t) &put32 && addr <= (uint32_t) __prog_end__) || 

    printk("start=%x, end=%x, diff=%d\n", START, end, size);
    
    // look at header
    uint32_t *prog_2_start = __prog_end__+ 1; // 8 = 32 / sizeof(uint32_t)

// set if to 1 below to debug the start location / first bits. Can run 
// "my-install-relay relay.bin" to ensure each address matches up exactly. 
#if 1
    printk("Comparing start of program with program past end.\n");
    for (int i = 0; i < 50; i++) {
        uint32_t instr_start = *((uint32_t *)START + i);
        uint32_t instr_post = *(prog_2_start + i);
        printk("i=%d, val=%x       vs.       val=%x\n", i, instr_start, instr_post);
        // assert(instr_start == instr_post);
    }
#endif

    uint32_t s2 = *(prog_2_start - 1);

    printk("2nd program size=%d\n", s2);

    // nrf_t *c = client_mk_ack(client_addr, s2);

    // code below performs 
#if 0
    trace("configuring reliable (acked) server=[%x] with %d nbyte msgs\n",
                server_addr, s2);
    nrf_t *s = server_mk_ack(server_addr, nbytes);
    nrf_stat_start(s);
    one_way_ack(s, client_addr, prog_2_start, s2, 1);
#endif

    // reset the times so we get a bit better measurement.
    // nrf_stat_start(c);

    // run test.

    // emit all the stats.
    // nrf_stat_print(s, "server: done with one-way test");
    // nrf_stat_print(c, "client: done with one-way test");
    clean_reboot();
}