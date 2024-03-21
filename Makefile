# list out the driver program source
PROGS += play_wav_light.c

LIBS += ./libgcc.a

STAFF_OBJS += $(CS140E_2024_PATH)/libpi/staff-objs/kmalloc.o
# your source, shared by driver code.
#   if you want to use our staff-hc-sr04.o,
#   comment SRC out and uncomment STAFF_OBJS
# COMMON_SRC += i2s.c
# COMMON_SRC += pwm.c
COMMON_SRC += pwm/pwm.c audio/audio.c fat32/fat32.c
COMMON_SRC += fat32/mbr.c fat32/pi-sd.c fat32/mbr-helpers.c fat32/fat32-helpers.c fat32/fat32-lfn-helpers.c fat32/external-code/unicode-utf8.c fat32/external-code/emmc.c fat32/external-code/mbox.c 
COMMON_SRC += neopixel/neopixel.c

CFLAGS_EXTRA  = -I fat32/external-code -I i2s/ -I nrf/ -I pwm/ -I fat32/ -I audio/

# define this if you need to give the device for your pi
TTYUSB = 

# set RUN = 1 if you want the code to automatically run after building.
RUN = 1

# DEPS = ./Makefile
# SUPPORT_OBJS := $(SRC:.c=.o)
include $(CS140E_2024_PATH)/libpi/mk/Makefile.template

clean::
	make -C tests clean
