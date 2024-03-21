# do this last.
# PROGS = 2-neopix.c 
# do this second:
PROGS = 1-blink.c  
# do this first.
# PROGS = 0-timing-check.c 
PROGS += 3-neopix-cute.c

# your source, shared by driver code.
#   if you want to use our staff-hc-sr04.o,
#   comment SRC out and uncomment STAFF_OBJS
COMMON_SRC := neopixel.c
# COMMON_SRC := 2-neopix.c
# STAFF_OBJS := staff-hc-sr04.o

OPT_LEVEL = -O3

RUN = 0

DEPS = ./Makefile

include $(CS140E_2024_PATH)/libpi/mk/Makefile.template-fixed
