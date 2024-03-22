#include "rpi.h"
#include "pwm.h"

// Nominal clock frequencies
#define F_OSC   19200000
#define F_PLLD 500000000
#define F_AUDIO 1000000


#define BM_PASSWORD 0x5A000000

#define CM_PWMCTL 0x201010A0
#define CM_PWMDIV 0x201010A4

#define CM_ENABLE  (1 << 4)
#define CM_KILL    (1 << 5)
#define CM_BUSY    (1 << 7) 
#define CM_FLIP    (1 << 8)
#define CM_MASH0   (0 << 9)
#define CM_MASH1   (1 << 9)
#define CM_MASH2   (2 << 9)
#define CM_MASH3   (3 << 9)


#define PWM_CTL    0x2020C000
#define PWM_STATUS 0x2020C004
#define PWM_DMAC   0x2020C008
#define PWM_FIFO   0x2020C018

#define PWM0_RANGE 0x2020C010
#define PWM0_DATA  0x2020C014

#define PWM1_RANGE 0x2020C020
#define PWM1_DATA  0x2020C024

#define PWM0_ENABLE      0x0001
#define PWM0_SERIAL      0x0002
#define PWM0_REPEATFF    0x0004
#define PWM0_OFFSTATE    0x0008
#define PWM0_REVPOLARITY 0x0010
#define PWM0_USEFIFO     0x0020
#define PWM0_MARKSPACE   0x0080

#define PWM1_ENABLE      0x0100
#define PWM1_SERIAL      0x0200
#define PWM1_REPEATFF    0x0400
#define PWM1_OFFSTATE    0x0800
#define PWM1_REVPOLARITY 0x1000
#define PWM1_USEFIFO     0x2000
#define PWM1_MARKSPACE   0x8000

#define PWM_CLEARFIFO    (1 << 6)



/*
 * this version is fragile. because divisor is 12-bits
 * we need to change the pwm clock depending on the input
 * frequency.
 */
void play_tone(int freq) {
    if (freq == 0) {
        pwm_disable(0);
        pwm_disable(1);
        return;
    }

    // Assumes pwm init and gpio pin setup

    pwm_set_clock( F_AUDIO);
    delay_ms(2);

    pwm_set_mode( 0, PWM_MARKSPACE);
    pwm_set_fifo( 0, 0 );
    pwm_enable( 0 );

    pwm_set_mode( 1, PWM_MARKSPACE);
    pwm_set_fifo( 1, 0 );
    pwm_enable( 1 );

    // assumes pwm_clock(F_AUDIO)
    int divisor = F_AUDIO / freq;
    int divisor2 = divisor / 4;

    pwm_set_range(0,  divisor);
    pwm_set_width(0,  divisor2);

    pwm_set_range(1,  divisor);
    pwm_set_width(1,  divisor2);

}

void pwm_init(void)
{
    //pwm_set_clock( F_OSC );
    //pwm_set_mode( 0, PWM_MARKSPACE );
    //pwm_set_fifo( 0, 0 );
    //pwm_set_mode( 1, PWM_MARKSPACE );
    //pwm_set_fifo( 1, 0 );
}

void set_sample_rate(int sample_rate) {
    pwm_disable(0);
    pwm_disable(1);
    pwm_clear_fifo();
    delay_ms(2);

    audio_init(sample_rate);

}

void audio_init(int sample_rate) {
    // sample_rate = 44.1kHz
    gpio_set_function(18, GPIO_FUNC_ALT5);
    gpio_set_function(12, GPIO_FUNC_ALT0);
    delay_ms(2);

    pwm_init();

//    pwm_set_clock( 19200000 / CLOCK_DIVISOR ); // 9600000 Hz

    int clock_rate = 19200000 / 2; // 9600000 Hz
    int range = clock_rate / sample_rate; //  clock_rate = output frequency
    pwm_set_clock(clock_rate); // 
    delay_ms(2);

    pwm_set_mode( 0, PWM_SIGMADELTA );
    pwm_set_mode( 1, PWM_SIGMADELTA );

    pwm_set_fifo(0, 1);
    pwm_set_fifo(1, 1);

    // enable both channels
    pwm_enable(0);
    pwm_enable(1);

    // pwm range is 1024 cycles
    pwm_set_range(0, range); // range is (9.6MHz / 44.1kHz)
    pwm_set_range(1, range); 
    // FIFO data is x / (9.6MHz / 44.1kHz = 217.687075Hz)
    delay_ms(2);
}

/*
 * pwm frequency in Hz = 19 200 000 Hz / pwmClock / pwmRange
 * pwmClock is really a divider (e.g., if we want a divider of 16, we
 * should pass in a freq of 19 200 000 / 16)
 */
void pwm_set_clock(int freq) {
    // freq = 9600000 Hz 
    int timer = F_OSC; // Use the fastest clock, 19.2MHz oscillator
    int source = 1;

    // divisor = 2, divisor = divi
    int divisor  = timer / freq; // for freq=16, divisor = 1.2e6
    int fraction = (timer % freq) * 4096 / freq; // fraction = 0
    // max fraction is 2^12 = 4096
    int mash = fraction ? CM_MASH1 : CM_MASH0;

    // output frequency will be (timer / divisor) = freq = 9.6MHz
    if( mash == CM_MASH0 && divisor < 1 )
        divisor = 1;
    else if( mash == CM_MASH1 && divisor < 2 )
        divisor = 2;
    if (divisor > 4095)
        divisor = 4095;

    // Set fraction, clamp fraction
    if (fraction > 4095)
        fraction = 4095;

    int pwm = GET32(PWM_CTL); // save pwm control register

    // turn off pwm before changing the clock
    PUT32(PWM_CTL, 0);

    PUT32(CM_PWMCTL, BM_PASSWORD | source) ;          // turn off clock
    while (GET32(CM_PWMCTL) & CM_BUSY) ;     // wait for clock to stop

    PUT32(CM_PWMDIV, BM_PASSWORD | (divisor << 12) | fraction);
    PUT32(CM_PWMCTL, BM_PASSWORD | CM_ENABLE | mash | source);
    PUT32(PWM_CTL, pwm); // restore pwm control register
}


void pwm_enable(int chan) 
{
    int ra = GET32(PWM_CTL);
    if (chan == 0) {
        ra |=  PWM0_ENABLE;
    }
    if (chan == 1) {
        ra |=  PWM1_ENABLE;
    }
    PUT32(PWM_CTL, ra);
}

void pwm_disable(int chan) 
{
    int ra = GET32(PWM_CTL);
    if (chan == 0) {
        ra &=  ~PWM0_ENABLE;
    }
    if (chan == 1) {
        ra &=  ~PWM1_ENABLE;
    }
    PUT32(PWM_CTL, ra);
}


void pwm_set_mode(int chan, int markspace) {
    int ra = GET32(PWM_CTL);
    if (chan == 0) {
        if (markspace) {
            ra |=  PWM0_MARKSPACE;
        } else {
            ra &= ~PWM0_MARKSPACE;
        }
    }
    if (chan == 1) {
        if (markspace) {
            ra |=  PWM1_MARKSPACE;
        } else {
            ra &= ~PWM1_MARKSPACE;
        }
    }
    PUT32(PWM_CTL, ra);
}


void pwm_set_fifo(int chan, int usefifo) {
    int ra = GET32(PWM_CTL);
    if (chan == 0) {
        if (usefifo) {
            ra |=  PWM0_USEFIFO;
        } else {
            ra &= ~PWM0_USEFIFO;
        }
    }
    if (chan == 1) {
        if (usefifo) {
            ra |=  PWM1_USEFIFO;
        } else {
            ra &= ~PWM1_USEFIFO;
        }
    }
    PUT32(PWM_CTL, ra);
}

void pwm_clear_fifo(void)
{
    unsigned ra = GET32(PWM_CTL);
    PUT32(PWM_CTL, ra | PWM_CLEARFIFO);
}

/*
 * range should be between 0 and 4095
 */
void pwm_set_range(int chan, int range) {
    if (chan == 0) {
        PUT32(PWM0_RANGE, range);
    }
    if (chan == 1) {
        PUT32(PWM1_RANGE, range);
    }
}

void pwm_set_width(int chan, int width) {
    if (chan == 0) {
        PUT32(PWM0_DATA, width);
    }
    if (chan == 1) {
        PUT32(PWM1_DATA, width);
    }
}

unsigned pwm_get_status(void) {
    return GET32(PWM_STATUS);
}

void pwm_write(int value) {
    PUT32(PWM_FIFO, value);
}
