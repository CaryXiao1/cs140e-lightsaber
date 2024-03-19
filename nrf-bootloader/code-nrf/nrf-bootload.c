/*
nrf-bootload.c

This is 

*/


#include "nrf-test.h"

/////////////////////////////////
// Info used for NRF
// Copied from code-nrf/3-one-way-ack-Nbytes.c
/////////////////////////////////

enum { ntrial = 1000, timeout_usec = 1000, nbytes = 32 }; // used by NRF
enum {
    server_addr = 0xd5d5d5, // sending pi running my-install
    client_addr = 0xe5e5e5,
};

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

// send 32 byte packets from <server> to <client>.  
static void
one_way_ack(nrf_t *server, nrf_t *client, int verbose_p) {
    unsigned client_addr = client->rxaddr;
    unsigned ntimeout = 0, npackets = 0;

    for(unsigned i = 0; i < ntrial; i++) {
        if(verbose_p && i  && i % 100 == 0)
            trace("sent %d ack'd packets\n", i);

        // output("sent %d\n", i);
        if(!send32_ack(server, client_addr, i))
            panic("send failed\n");

        uint32_t x;
        int ret = recv32(client, &x);
        // output("ret=%d, got %d\n", ret, x);
        if(ret == nbytes) {
            if(x != i)
                nrf_output("client: received %d (expected=%d)\n", x,i);
            assert(x == i);
            npackets++;
        } else {
            if(verbose_p) 
                output("receive failed for packet=%d, nbytes=%d ret=%d\n", i, nbytes, ret);
            ntimeout++;
        }
    }
    trace("trial: total successfully sent %d ack'd packets lost [%d]\n",
        npackets, ntimeout);
    assert((ntimeout + npackets) == ntrial);
}


void notmain(void) {
    printk("Finished bootloading relay. Ready to start forwarding bytes to ");
    /*
    pseudocode:
    1. 
    2. 
    */
}