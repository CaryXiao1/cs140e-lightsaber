# need the makefile to give an error if this doesn't exist
PROGS := main.c
# PROGS += relay.c
COMMON_SRC := 

# hmm.  not sure about this.
BUILD_DIR = ./objs
START := $(BUILD_DIR)/boot-start.o

COMMON_SRC := nrf-hw-support.c nrf-public.c nrf-driver.c
STAFF_OBJS += $(CS140E_2024_PATH)/libpi/staff-objs/new-spi.o
STAFF_OBJS += staff-nrf-driver.o

LIBS += $(CS140E_2024_PATH)/lib/libgcc.a
# we can't bootload the bootloader.
RUN=0

# define this if you need to give the device for your pi
TTYUSB = 
BOOTLOADER = pi-install

include $(CS140E_2024_PATH)/libpi/mk/Makefile.template-fixed

all:: kernel.img

clean::
	rm -f $(START) kernel.img

kernel.img: main.bin
	cp main.bin kernel.img
