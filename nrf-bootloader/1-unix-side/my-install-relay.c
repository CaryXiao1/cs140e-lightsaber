// engler, cs140e: driver for "bootloader" for an r/pi connected via 
// a tty-USB device.
//
// most of it is argument parsing.
//
// Unless you know what you are doing:
//              DO NOT MODIFY THIS CODE!
//              DO NOT MODIFY THIS CODE!
//              DO NOT MODIFY THIS CODE!
//              DO NOT MODIFY THIS CODE!
//              DO NOT MODIFY THIS CODE!
//
// You shouldn't have to modify any code in this file.  Though, if you find
// a bug or improve it, let us know!
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "libunix.h"
#include "put-code.h"

static char *progname = 0;

// TODO: add code that uses the CS140E_2024_PATH thing
static const char *RELAY_FILENAME = "../3-relay/relay.bin";

/**
 * Variation of read_file (from libpi/read_file.c) that reads in two files and concatenates them together.
 * Also adds a header that tells how long the header is and the length of the
 * second length of code.
 * Primarily used in final-project/nrf-bootloader/my-install-relay.c
 * 
*/
void *read_file_relay(unsigned *size, const char *name1, const char *name2) {
    const uint32_t HEADER_SIZE = 4; // contains 1. header size and 2. 
    struct stat info;
    if (stat(name1, &info) != 0) panic("file path: stat() failed on name1.");
    unsigned s1 = info.st_size;
    if (stat(name2, &info) != 0) panic("file path: stat() failed on name2.");
    unsigned s2 = info.st_size;
    // round up to next multiple of 32!
    unsigned s_up = s1 + s2 + HEADER_SIZE + 32;
    trace("file1 size=%d, file2 size=%d, HEADER_SIZE=%d, s_up=%d\n", s1, s2, HEADER_SIZE, s_up);
    // alloc space
    char *buf = calloc(1, s_up);
    int fd1 = open(name1, O_RDONLY);
    int fd2 = open(name2, O_RDONLY);


    if (s1 != 0) read_exact(fd1, buf, s1);
    // set start of next program, round up to next multiple of 32
    // based on my (Cary's) understanding, __prog_end__ on the pi needs to be a multiple of 32
    // so we do the same to align the code with __prog_end__. 
    uint32_t offset_correction = (((unsigned long)buf + s1) % 32 != 0) ? (32 - ((unsigned long)buf + s1) % 32) : 0;
    uint32_t *head_start = (uint32_t *)(buf + s1 + offset_correction); // round up to next multiple of 8
    printf("buf=%lx, head_start=%lx, size=%lu\n", (unsigned long)buf, (unsigned long) head_start, (unsigned long) head_start - (unsigned long) buf);
    assert(((unsigned long) head_start & 0xfl) == 0); // make sure 
    *(head_start) = s2;
    
    if (s2 != 0) read_exact(fd2, head_start + (HEADER_SIZE / 4), s2);
    *size = s1 + s2 + HEADER_SIZE;
    close(fd1);
    close(fd2);
    return buf;
}



static void usage(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    output("\nusage: %s  [--trace-all] [--trace-control] ([device] | [--last] | [--first] [--device <device>]) <pi-program> \n", progname);
    output("    pi-program = has a '.bin' suffix\n");
    output("    specify a device using any method:\n");
    output("        <device>: has a '/dev' prefix\n");
    output("       --last: gets the last serial device mounted\n");
    output("        --first: gets the first serial device mounted\n");
    output("        --device <device>: manually specify <device>\n");
    output("    --baud <baud_rate>: manually specify baud_rate\n");
    output("    --trace-all: trace all put/get between rpi and unix side\n");
    output("    --trace-control: trace only control [no data] messages\n");
    exit(1);
}

int main(int argc, char *argv[]) { 
    char *dev_name = 0;
    char *pi_prog = 0;

    // used to pass the file descriptor to another program.
    char **exec_argv = 0;

    // a good extension challenge: tune timeout and baud rate transmission
    //
    // on linux, baud rates are defined in:
    //  /usr/include/asm-generic/termbits.h
    //
    // when I tried with sw-uart, these all worked:
    //      B9600
    //      B115200
    //      B230400
    //      B460800
    //      B576000
    // can almost do: just a weird start character.
    //      B1000000
    // all garbage?
    //      B921600
    unsigned baud_rate = B115200;

    // by default is 0x8000
    unsigned boot_addr = ARMBASE;

    // we do manual option parsing to make things a bit more obvious.
    // you might rewrite using getopt().
    progname = argv[0];
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--trace-control") == 0)  {
            trace_p = TRACE_CONTROL_ONLY;
        } else if(strcmp(argv[i], "--trace-all") == 0)  {
            trace_p = TRACE_ALL;
        } else if(strcmp(argv[i], "--last") == 0)  {
            dev_name = find_ttyusb_last();
        } else if(strcmp(argv[i], "--first") == 0)  {
            dev_name = find_ttyusb_first();
        // we assume: anything that begins with /dev/ is a device.
        } else if(prefix_cmp(argv[i], "/dev/")) {
            dev_name = argv[i];
        // we assume: anything that ends in .bin is a pi-program.
        } else if(suffix_cmp(argv[i], ".bin")) {
            pi_prog = argv[i];
        } else if(strcmp(argv[i], "--baud") == 0) {
            i++;
            if(!argv[i])
                usage("missing argument to --baud\n");
            baud_rate = atoi(argv[i]);
        } else if(strcmp(argv[i], "--addr") == 0) {
            i++;
            if(!argv[i])
                usage("missing argument to --addr\n");
            boot_addr = atoi(argv[i]);
        } else if(strcmp(argv[i], "--exec") == 0) {
            i++;
            if(!argv[i])
                usage("missing argument to --exec\n");
            exec_argv = &argv[i];
            break;
        } else if(strcmp(argv[i], "--device") == 0) {
            i++;
            if(!argv[i])
                usage("missing argument to --device\n");
            dev_name = argv[i];
        } else {
            usage("unexpected argument=<%s>\n", argv[i]);
        }
    }
    if(!pi_prog)
        usage("no pi program\n");

    // 1. get the name of the ttyUSB.
    if(!dev_name) {
        dev_name = find_ttyusb_last();
        if(!dev_name)
            panic("didn't find a device\n");
    }
    // debug_output("done with options: dev name=<%s>, pi-prog=<%s>, trace=%d\n", 
    //         dev_name, pi_prog, trace_p);

    if(exec_argv)
        argv_print("BOOT: --exec argv:", exec_argv);

    // 2. open the ttyUSB in 115200, 8n1 mode
    int tty = open_tty(dev_name);
    if(tty < 0)
        panic("can't open tty <%s>\n", dev_name);

    // timeout is in tenths of a second.  tuning this can speed up
    // checking.
    //
    // if you are on linux you can shrink down the <2*8> timeout
    // threshold.  if your my-install isn't reseting when used 
    // during checkig, it's likely due to this timeout being too
    // small.
    double timeout_tenths = 2*5;
    int fd = set_tty_to_8n1(tty, baud_rate, timeout_tenths);
    if(fd < 0)
        panic("could not set tty: <%s>\n", dev_name);

    // 3. read in program [probably should just make a <file_size>
    //    call and then shard out the pieces].
	unsigned nbytes;
    uint8_t *code = (uint8_t *) read_file_relay(&nbytes, RELAY_FILENAME, pi_prog);
    printf("nbytes=%d\n", nbytes);
    // 4. let's send it!
	debug_output("%s: tty-usb=<%s> program=<%s>: about to boot\n", 
                progname, dev_name, pi_prog);
    simple_boot(fd, boot_addr, code, nbytes);

    // 5. echo output from pi
    if(!exec_argv)
        pi_echo(0, fd, dev_name);
    else {
        todo("not handling exec_argv");
        handoff_to(fd, TRACE_FD, exec_argv);
    }
	return 0;
}
