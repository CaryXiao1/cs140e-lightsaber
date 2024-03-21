/*
 * simplified i2c implementation --- no dma, no interrupts.  the latter
 * probably should get added.  the pi's we use can only access i2c1
 * so we hardwire everything in.
 *
 * datasheet starts at p28 in the broadcom pdf.
 *
 */
#include "rpi.h"
#include "libc/helper-macros.h"
#include "i2c.h"

typedef struct {
	uint32_t control; // "C" register, p29
	uint32_t status;  // "S" register, p31

#	define check_dlen(x) assert(((x) >> 16) == 0)
	uint32_t dlen; 	// p32. number of bytes to xmit, recv
					// reading from dlen when TA=1
					// or DONE=1 returns bytes still
					// to recv/xmit.  
					// reading when TA=0 and DONE=0
					// returns the last DLEN written.
					// can be left over multiple pkts.

    // Today address's should be 7 bits.
#	define check_dev_addr(x) assert(((x) >> 7) == 0)
	uint32_t 	dev_addr;   // "A" register, p 33, device addr 

	uint32_t fifo;  // p33: only use the lower 8 bits.
#	define check_clock_div(x) assert(((x) >> 16) == 0)
	uint32_t clock_div;     // p34
	// we aren't going to use this: fun to mess w/ tho.
	uint32_t clock_delay;   // p34
	uint32_t clock_stretch_timeout;     // broken on pi.
} RPI_i2c_t;

// offsets from table "i2c address map" p 28
_Static_assert(offsetof(RPI_i2c_t, control) == 0, "wrong offset");
_Static_assert(offsetof(RPI_i2c_t, status) == 0x4, "wrong offset");
_Static_assert(offsetof(RPI_i2c_t, dlen) == 0x8, "wrong offset");
_Static_assert(offsetof(RPI_i2c_t, dev_addr) == 0xc, "wrong offset");
_Static_assert(offsetof(RPI_i2c_t, fifo) == 0x10, "wrong offset");
_Static_assert(offsetof(RPI_i2c_t, clock_div) == 0x14, "wrong offset");
_Static_assert(offsetof(RPI_i2c_t, clock_delay) == 0x18, "wrong offset");

/*
 * There are three BSC masters inside BCM. The register addresses starts from
 *	 BSC0: 0x7E20_5000 (0x20205000)
 *	 BSC1: 0x7E80_4000
 *	 BSC2 : 0x7E80_5000 (0x20805000)
 * the PI can only use BSC1.
 */
static volatile RPI_i2c_t *i2c = (void*)0x20804000; 	// BSC1

// extend so this can fail.
int i2c_write(unsigned addr, uint8_t data[], unsigned nbytes) {
    // wait until transfer is not active
	while(bit_get(GET32((uint32_t)&(i2c->status)), 0)) // while active
		; 

	// check in status that fifo is 
	uint32_t bsc_status = GET32((uint32_t)&(i2c->status)); 
	if (bit_get(bsc_status, 9) || // there is clock stretch timeout 
		bit_get(bsc_status, 8) // and there are errors.
	) { 
		panic("error in status"); 
		return 0; 
	} 

	// Set the device address and length
	PUT32((uint32_t)&(i2c->dev_addr), addr); // pg32
	PUT32((uint32_t)&(i2c->dlen), nbytes); // pg33

	// While FIFO can't accept data
	while (!bit_get(bsc_status, 4))
		;

	// Set control reg to read and start transfer RMW
	uint32_t bsc_control = 0; 
	bsc_control = bit_set(bsc_control, 15); // keep control enabled 
	bsc_control = bit_set(bsc_control, 7); // p30 -- start transfer
	bsc_control = bit_clr(bsc_control, 0); // p30 -- Write packet transfer
	PUT32((uint32_t)&(i2c->control), bsc_control); 

	// wait until transfer started before reading
	while(!bit_get(GET32((uint32_t)&(i2c->status)), 0)) // while transfer is not active
		;

	// Write the bytes from FIFO into i2c
	for (int i = 0; i < nbytes; i++) {
		while (!bit_get(GET32((uint32_t)&(i2c->status)), 4)); // While FIFO can't accept data
		PUT32((uint32_t)&(i2c->fifo), data[i]); 
	}

	// Wait until the transfer is done
	while(!bit_get(GET32((uint32_t)&(i2c->status)), 1)) // transferring from i2c to FIFO of rpi
		; 

	// Check that TA is 0 and there were no errors
	bsc_status = GET32((uint32_t)&(i2c->status));
	bsc_status = bit_set(bsc_status, 1);  // clear writing done
	PUT32((uint32_t)&(i2c->status), bsc_status); 

	assert(bit_get(bsc_status, 0) == 0);  // check TA is 0
	assert(bit_get(bsc_status, 9) == 0); // check no clock stretch timeout
	assert(bit_get(bsc_status, 8) == 0); // check no ERR ACK detected

	return 1;
}

// extend so it returns failure. 
// addr on i2c
int i2c_read(unsigned addr, uint8_t data[], unsigned nbytes) {
    // todo("implement");
	// wait until transfer is not active
	while(bit_get(GET32((uint32_t)&(i2c->status)), 0)) // while active
		; 
	
	// check in status that fifo is 
	uint32_t bsc_status = GET32((uint32_t)&(i2c->status)); 
	if (bit_get(bsc_status, 9) || // there is clock stretch timeout 
		bit_get(bsc_status, 8) // and there are errors.
	) { 
		panic("error in status"); 
		return 0; 
	}
	
	// Set the device address and length
	PUT32((uint32_t)&(i2c->dev_addr), addr); // pg32
	PUT32((uint32_t)&(i2c->dlen), nbytes); // pg33

	// Wait until FIFO is empty
	while(bit_get(bsc_status, 5))
		; 

	// Set control reg to read and start transfer RMW
	uint32_t bsc_control = 0; 
	bsc_control = bit_set(bsc_control, 15); // keep control enabled 
	bsc_control = bit_set(bsc_control, 7); // p30 -- start transfer
	bsc_control = bit_set(bsc_control, 0); // p30 -- Read transfer
	PUT32((uint32_t)&(i2c->control), bsc_control); 

	// wait until transfer started before reading
	while(!bit_get(GET32((uint32_t)&(i2c->status)), 0)) // while transfer is not active
		;

	// Read from FIFO into data
	for (int i = 0; i < nbytes; i++) {
		while(!bit_get(GET32((uint32_t)&(i2c->status)), 5));  // while fifo is empty, hang
		uint32_t bsc_fifo = GET32((uint32_t)&(i2c->fifo));
		data[i] = bits_get(bsc_fifo, 0, 7); 
	}
	
	// Wait until the transfer is done
	while(!bit_get(GET32((uint32_t)&(i2c->status)), 1)) // transferring from i2c to FIFO of rpi
		; 

	// Check that TA is 0 and there were no errors
	bsc_status = 0;
	bsc_status = bit_set(bsc_status, 1);  // clear writing done
	PUT32((uint32_t)&(i2c->status), bsc_status); 

	bsc_status = GET32((uint32_t)&(i2c->status));
	assert(bit_get(bsc_status, 0) == 0);  // check TA is 0
	assert(bit_get(bsc_status, 9) == 0); // check no clock stretch timeout
	assert(bit_get(bsc_status, 8) == 0); // check no ERR ACK detected

	return 1;
}

void i2c_init(void) {
	// 1) Setup GPIO
	dev_barrier(); 
	// Set up pins 2 and 3 from gpio.png 
	gpio_set_function(2, GPIO_FUNC_ALT0); // p102 
	gpio_set_function(3, GPIO_FUNC_ALT0); 
	dev_barrier();

	// 2) Enable the BSC we want
	uint32_t bsc_control = 1 << 15; 
	PUT32((uint32_t)&(i2c->control), bsc_control); // p29
	// TODO: clock divider -- see fifo register

	// 3) Clear the BSC status register
	uint32_t bsc_status = 0;
	bsc_status = bit_set(bsc_status, 1); // p32 - clear DONE 
	bsc_status = bit_set(bsc_status, 8); // clear ERR ACK Error
	bsc_status = bit_set(bsc_status, 9); // clear CLKT Clock stretch timeout
	PUT32((uint32_t)&(i2c->status), bsc_status); 

	// 4) Sanity check results: Make sure there is no active transfer
	assert(bit_get(GET32((uint32_t)&(i2c->status)), 0) == 0); 
	assert((GET32((uint32_t)&(i2c->clock_div)) & 0xFFFF) == 0x5dc); 
	assert((GET32((uint32_t)&(i2c->clock_stretch_timeout)) & 0xFFFF) == 0x40); 
	dev_barrier(); 
    // todo("setup GPIO, setup i2c, sanity check results");
}

// shortest will be 130 for i2c accel.
void i2c_init_clk_div(unsigned clk_div) {
    todo("same as init but set the clock divider");
}
