Cary Xiao: caryxiao@stanford.edu \
Ekin Tiu: ekintiu@stanford.edu

# CS140E Final Project: Lightsaber!

For our final project, we created a lightsaber prototype. On top of using various lab code to implement certain features, We also implemented a networked bootloader and PWM code to wirelessly send code to the lightsaber and to play sound while polling from the gyroscope, respectively.

![Picture of lightsaber we built](images/lightsaber-tmp.jpg)


## Overall Design

As an overview of the lightsaber itself, we designed and 3D-printed a hilt on top of a 24-inch plexiglass tube. We continuously sample from a gyroscope to determine the acceleration of the blade and use that information to determine how loud the sound of the lightsaber should play. Furthermore, we keep track of the direction of acceleration on the lightsaber, and if the direction suddenly changes, we detect that as a collision and briefly change the sound of the lightsaber accordingly.

## Networked Bootloader

As seen in the photo above, our goal was to have the lightsaber powered directly by batteries. However, because most of our lab code uses UART to send code to the r/pi to run programs, we either needed to change the bootloader to statically run our lightsaber code, or we would need to send program information wirelessly using the nrf receivers. We decided to go with the latter option, as this would allow us to quickly change the code running on the lightsaber and speed up our debugging process without having to re-compute and replace the `kernel.img` file that's in the SD card of the lightsaber's r/pi. 

### Method

To implement the networked bootloader, we decided to use two r/pis: one connected to a computer via UART, and the second powered wirelessly that would get the code. Because of this, we send *two* programs to the first r/pi using a modified `my-install` called `my-install-relay` (in `final-project/nrf-bootloader/1-unix-side/`). Aside from concatenating the two programs, `my-install-relay` also sends a `uint32_t` directly before the second message to indicate to the first r/pi how long the second program is. 

The first r/pi then runs the first program that is sent, `relay.bin` (in `nrf-bootloader/3-relay/`). This program then locates the start of the second program, finds the size of the array, then moves the entire program into the heap before using NRF to send the program and its size to the second r/pi. 

Finally, the second r/pi has a modified bootloader (see `nrf-bootloader/2-pi-side/`) that uses NRF instead of UART in `get-code.h`. When the first r/pi sends the code, the second r/pi is able to pick it up, move it to `0x8000`, and then run the program by calling `BRANCHTO(ARM_BASE)`. 

### Difficulties

A notable issue we had was figuring out how reliably find the start of the second program on the first r/pi. While this sounds relatively simple by looking at the address `__prog_end__ + 4`. Our current understanding is that `__prog_end__` is set to `ARM_BASE` size of the first program, rounded up to the nearest power of 8, plus 16. However, if the program size is a multiple of 32, then `__prog_end__` is set to `ARM_BASE` plus the size of the first program. 

We also had an issue with sending the code to the second r/pi. This is because the second program is initially in "free" memory, which is the reason why we first put it into an allocated space on the heap. However, something we realized is that we needed to first copy it to the stack before allocating the requisite amount from the heap. We found out that if we were to try copying directly from the heap, the system would directly allocate the space we were trying to preserve, zeroing out all the values. Thus, we copied it to the stack, then moved it to the heap so that it would be preserved across function calls. 

### Testing and Debugging

To test our nrf bootloading, we created two methods to either test the functionality or debug output: 

1. `nrf-bootloader-test.c` (in `nrf-bootloader/3-relay`). This test requires two r/pis with the original UART functionality and uses the first r/pi to send `hello-f.bin` to the second r/pi, which then copies the program into memory and attempts to run it via a `BRANCHTO`. Please see the file for more information about how to run it.

2. `pi-echo.c` (in `nrf-bootloader/1-unix-side`). This modified version of `my-install` enables you to see output from the second r/pi after you change the bootloader to use NRF. We used this program to debug our modified `get-code.c` and ensure that it correctly got programs, ran them, and either restarted or waited for another program.

## PWM

Todo!

## Designing the Hilt

While this is not necessarily related to computer systems, we did spend a non-negligible amount of time CADing and 3D-printing the hilt that housed all our devices. To see .obj and .stl files exported from Fusion 360 

![Fusion 360 screenshot of lightsaber hilt](images/lightsaber-design.png)