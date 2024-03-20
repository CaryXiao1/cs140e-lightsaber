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

enum {
    START = 0x8000, // address at the start of the program
    timeout_usec = 1000, 
    nbytes = 4,
};
static const int PRINT_DEBUG = 1;

#if 1
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
one_way_ack(nrf_t *server, uint32_t client_addr, uint32_t *data, uint32_t size) {
    unsigned ntimeout = 0, npackets = 0;
    // figure out if we need to deal with case where size % 4 != 0
    assert(size % 4 == 0);
    for(unsigned i = 0; i < size / 4; i++) {
        if(i && i % 100 == 0)
            trace("sent %d ack'd packets\n", i);
        if(!send32_ack(server, client_addr, *(data + i)))
            panic("send failed\n");
        if (i == 0) {
            printk("sent %d as header.\n", *data);
        }

    }
    trace("finished sending code. sent a total of %d packets.\n", size / 4);
}
#endif



void notmain(void) {
    printk("Starting up relay on UART pi.\n");
    // test out the bss start and end code, see if it equals the size of the overall stuff
    uint32_t *end = __prog_end__;
    uint8_t *size = ((uint8_t *)end - START);
    // if ((addr >= (uint32_t) &put32 && addr <= (uint32_t) __prog_end__) || 

    printk("start=%x, end=%x, diff=%d\n", START, end, size);
    
    // look at header
    uint32_t *head_start = __prog_end__; // 8 = 32 / sizeof(uint32_t)

// set if to 1 below to debug the start location / first bits. Can run 
// "my-install-relay relay.bin" to ensure each address matches up exactly. 
#if 1
    printk("Comparing start of program with program past end.\n");
    for (int i = 0; i < 10; i++) {
        uint32_t instr_start = *((uint32_t *)START + i);
        uint32_t instr_post = *(head_start + 1 + i);
        printk("i=%d, val=%x       vs.       val=%x\n", i, instr_start, instr_post);
        // assert(instr_start == instr_post);
    }
#endif

#if 1
    // move 2nd program into heap (but first into stack!)
    uint32_t s2 = *(head_start);
    printk("2nd program size=%d\n", s2);
    printk("Moving 2nd program to heap...\n");
    uint32_t data_stack[s2 + 4]; // need to put program in heap first before putting in heap!
    data_stack[0] = s2;
    memcpy(data_stack + 1, head_start + 1, s2);
    uint32_t *data = kmalloc(s2 + 4);
    memcpy(data, data_stack, s2 + 4);
    
    // send code to client
    trace("configuring reliable (acked) server=[%x] with %d nbyte msgs\n",
                server_addr, nbytes);
    nrf_t *s = server_mk_ack(server_addr, nbytes);
    // trace("finished server_mk_ack and starting start_stat\n");
    nrf_stat_start(s);
    // *head_start = s2;
    // printk("%d\n", *head_start);
    one_way_ack(s, client_addr, head_start, s2 + 4); // need to add header & its space
#endif

    // reset the times so we get a bit better measurement.
    // nrf_stat_start(c);

    // run test.

    // emit all the stats.
    // nrf_stat_print(s, "server: done with one-way test");
    // nrf_stat_print(c, "client: done with one-way test");
    clean_reboot();
}