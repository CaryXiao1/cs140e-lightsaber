#include "i2s.h"

volatile i2s_regs_t *i2s_regs = (volatile i2s_regs_t *)I2S_REGS_BASE;
volatile cm_regs_t *cm_regs = (volatile cm_regs_t *)CM_REGS_BASE;

#define addr(x) ((uint32_t)&(x))

void i2s_init(void) {
    // 1. Set the I2S pins using GPIO set
    gpio_set_function(I2S_PIN_CLK, GPIO_FUNC_ALT0); 
    gpio_set_function(I2S_PIN_FS, GPIO_FUNC_ALT0); 
    gpio_set_function(I2S_PIN_DIN, GPIO_FUNC_ALT0); // rx
    gpio_set_function(I2S_PIN_DOUT, GPIO_FUNC_ALT0); // tx

    // 2. device barrier 
    dev_barrier(); 

    
    // 3. Configure the Clock Manager (pg. 107-108)
    /**
     * Write 0b0001 to lowest 4 bits of the PCM_CTRL 
     * register (0x20101098)
     * 
     * TODO: MIGHT HAVE TO USE *(volatile unsigned)
     */
    // GPIO clocks used to drive audio devices
    // RMW PCM_CTRL

    // Configure the clock manager
    uint32_t cm_ctrl = CM_REGS_MSB | CM_CTRL_XTAL | CM_CTRL_MASH3; 
    // uint32_t cm_ctrl = 0; 
    // uint32_t cm_ctrl = CM_REGS_MSB; 
    // cm_ctrl = bits_set(cm_ctrl, 0, 3, 0b0001); // uses highest resolution clock
    // cm_ctrl = bits_set(cm_ctrl, 9, 10, 0b11); // This doesn't work for some reason
    cm_regs->pcm_ctrl = cm_ctrl; // PUT

    // RMW PCM_DIV (p105 table in errata)
    uint32_t cm_div = CM_REGS_MSB | (CM_DIV_INT << I2S_CLK_DIV_INT_LB) | (CM_DIV_FRAC);  
    cm_regs->pcm_div = cm_div;

    // Enable I2S clock
    // cm_ctrl = bit_set(cm_regs->pcm_ctrl, 4); 
    cm_regs->pcm_ctrl = cm_ctrl | CM_CTRL_EN; // PUT
    dev_barrier(); // Done with clock manager

    // Configure the I2S peripheral
    // Mode Register: RMW
    uint32_t mode = (i2s_regs->mode); 
    // uint32_t mode = 0; 
    mode |= (63 << I2S_MODE_FLEN_LB) | (32); // PUT
    // Sets the Frame length to FLEN + 1 clocks
    i2s_regs->mode = mode;

    // Receiver config register: RMW
    // uint32_t rx_cfg = 0; 
    uint32_t rx_cfg = 0; 
    // rx_cfg = bit_set(rx_cfg, I2S_RXC_CH2EN); // channel 1 enable
    // rx_cfg = bits_set(rx_cfg, I2S_RXC_CH2WID_LB, I2S_RXC_CH2WID_UB, 8); // channel 1 width
    // rx_cfg = bit_set(rx_cfg, I2S_RXC_CH2WEX); // width extension bit

    rx_cfg = bit_set(rx_cfg, I2S_RXC_CH1EN); // channel 1 enable
    rx_cfg = bits_set(rx_cfg, I2S_RXC_CH1WID_LB, I2S_RXC_CH1WID_UB, 8); // channel 1 width
    rx_cfg = bit_set(rx_cfg, I2S_RXC_CH1WEX); // width extension bit

    i2s_regs->rx_cfg = rx_cfg;
    assert(GET32(addr(i2s_regs->rx_cfg)) == rx_cfg);
 
    // Tranceiver config register: RMW
    uint32_t tx_cfg = 0; 
    tx_cfg = bit_set(tx_cfg, I2S_RXC_CH1EN); // channel 1 enable
    tx_cfg = bits_set(tx_cfg, I2S_RXC_CH1WID_LB, I2S_RXC_CH1WID_UB, 8); // channel 1 width
    tx_cfg = bit_set(tx_cfg, I2S_RXC_CH1WEX); // width extension bit
    i2s_regs->tx_cfg = tx_cfg;
    assert(GET32(addr(i2s_regs->tx_cfg)) == tx_cfg);

    // Control and status register: RMW
    uint32_t cs = (i2s_regs->cs); 
    // uint32_t cs = 0; 
    cs = bit_set(cs, I2S_CS_EN); 
    cs = bit_set(cs, I2S_CS_STBY); 
    cs = bit_set(cs, I2S_CS_RXCLR); // clear RX FIFO
    cs = bit_set(cs, I2S_CS_TXCLR); // clear transmitting FIFO
    // Start transmitting,first data read from TX FIFO placed in first channel
    cs = bit_set(cs, I2S_CS_TXON); // TXON
    cs = bit_set(cs, I2S_CS_RXON); 

    i2s_regs->cs = cs;
    // printk("cs reg: %x\n", cs);
    // unimplemented();
    dev_barrier(); 
}

int32_t i2s_read_sample(void) {
    dev_barrier(); 
    while((!bit_get(i2s_regs->cs, 20))) {
        // printk("waiting"); 
    } // while RX FIFO does not contain data
    return i2s_regs->fifo;
}

/**
 * Sends a single sample to iterate between high and low
 */
int32_t i2s_send_sample(void) {
    /**
     * dev_barrier. I haven't tested without this but you may be switching from another peripheral, and you want a dev_barrier before the first read.
     */
    dev_barrier(); 

    // Try sending some set of samples
    uint32_t low_sample = 0x0000;
    uint32_t high_sample = 0xffffffff;
    

    // Wait for the TXD bit in the CS register to go high. If it's high there is at least 1 sample available in the FIFO. Of course this is sort of a waste as you're just burning CPU cycles.
    // for (int i = 0; i < 50; i++) {
    //     while((!bit_get(i2s_regs->cs, 19))); // while TX FIFO can not accept data
    //     PUT32(addr(i2s_regs->fifo), low_sample);
    // } 
    
    for (int i = 0; i < 50; i++) { 
        while((bit_get(i2s_regs->cs, 19))); // while TX FIFO can not accept data
        PUT32(addr(i2s_regs->fifo), high_sample);
    }
    
    // frame clock is at sampling rate, // t
    // time -- delay in the code, i2s just has to respect the clock
    dev_barrier(); 
    // TODO: Send data to TX FIFO, FIFO is 32 bits
    return 0; 
        


    
}