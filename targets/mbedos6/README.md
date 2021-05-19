# JerryScript with mbed OS 6

## Introduction

This directory contains the necessary code to build JerryScript for devices
capable of running mbed OS 6. It has been tested on:

- [NXP Freedom K64F](https://developer.mbed.org/platforms/FRDM-K64F/)


## Features

### Peripheral Drivers

Peripheral Drivers are intended as a 1-to-1 mapping to mbed C++ APIs, with a few
differences (due to differences between JavaScript and C++ like lack of operator
overloading).

- [DigitalOut](https://os.mbed.com/docs/mbed-os/v6.3/mbed-os-api-doxy/classmbed_1_1_digital_out.html)
- [thread_sleep_for (delay)](https://os.mbed.com/docs/mbed-os/v6.3/mbed-os-api-doxy/group__mbed__thread.html#ga7653ab16602208ca0c580bed553b4ca5)

## Dependencies

### mbed CLI

mbed CLI is used as the build tool for mbed OS 6. You can find out how to install
it in the [official documentation](https://os.mbed.com/docs/mbed-os/v6.3/build-tools/install-and-set-up.html).

### arm-none-eabi-gcc

arm-none-eabi-gcc is the only currently tested compiler for jerryscript on mbed,
and instructions for building can be found as part of the mbed-cli installation
instructions above.

### make

make is used to automate the process of fetching dependencies, and making sure that
mbed-cli is called with the correct arguments.

### minicom

minicom is a serial communication program, we can get a nice serial interface with it,
you can find more on [minicom's page](https://linux.die.net/man/1/minicom).

## Quick Start

Clone jerryscript:

```
git clone https://github.com/jerryscript-project/jerryscript/
```
Go to mbedos6 directory and run mbedconfig (you may run that as superuser), that will install dependenies and set the mbed config file:

```
cd targets/mbedos6
sudo make mbedconfig
```

Get the arm gcc from: [link](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads), extract it then set the path in mbed config (from mbedos6 directory) : 

```
mbed config GCC_ARM_PATH ~/Path/to/gcc-arm-none-eabi-9-2020-q2-update/bin
```

Once you have all of your dependencies installed, you can build the example project with the 'build' command:

```
make build
```

The produced file (in BUILD/) can then be uploaded to your board with 'flash', and will
run when you press reset.

```
make flash
```

## Tools

You can config and use all the tools from the Makefile located in targets/mbedos6.
See more info in the Makefile.

### pin-generator:

Located in mbedos6/tools. It generates a file, named 'pins.cpp' for a specified target, using target definitions from the
mbed OS source tree. It's expecting to be run from the targets/mbedos6 directory.

```
make generatepins
```

### js2c:

Located in jerryscript/tools. It will parse JS files (that are in targets/mbedos6/js) and generate a C file 'jerry-targetjs.h'
that can be loaded from jerryscript.

```
make js2c
```

### jerrysrcgen:

Located in jerryscript/tools. This will generate a single C from jerryscript.
You can read more about the srcgenerator there: [Single source build mode](https://jerryscript.net/configuration/#single-source-build-mode).

```
make cleanjerry
make jerrysrcgen
```
### mbedinstall:

mbedinstall will install the mbed CLI and the mbed-os 6.3.0 into targets/mbedos6.

```
make mbedinstall
```

### clean / build / flash:

'make flash' will compile the source into the given BUILD directory and flash it to your TARGET board.
If you want to build without flashing use the 'make build' command.
You can clear the Build directory with the 'make clean' command.

```
make clean
make build
make flash
```

### serial port:

You can open a serial interface to the connected board with 'make serialport' or 'make serialport2'.
- 'make serialport' :
                    will use the 'mbed sterm' command that is a mbed CLI builtin,
                    it will find the device and open the port
- 'make serialport2' :
                    uses [minicom](https://linux.die.net/man/1/minicom),it can be faster than 'mbed sterm'
                    but u have to set the PORT where your device is connected

If you want to get access to the serial interface right after flashing, use 'serialport2'.

```
make serialport2
```
