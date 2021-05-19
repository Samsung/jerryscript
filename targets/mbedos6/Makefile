##CONFIG##
CC=GCC_ARM          #toolchain
TARGET=K64F         #target device
BUILD=BUILD         #build dir
PORT=/dev/ttyACM0   #serial port -- used by minicom
LOG_PRINT=          #used for debug wrapping functions

MBED=               #use it if you installed mbed-os outside of this dir
MBED_DEBUG=0        #for debugging mbed-os
MBED_VERBOSE=0
##ENDOFCONFIG##

#jerry
JERRY_HEAP_SIZE=32
JERRY_ROOT= ../..
JERRY_SOURCE=$(JERRY_ROOT)/build/merged_src

#jerry-ext
JERRY_EXT_SOURCE=$(JERRY_ROOT)/jerry-ext/handler
JERRY_EXT_SOURCE_H=$(JERRY_ROOT)/jerry-ext/include/

#extra source for jerry-ext (includes "jerryscript-port.h")
JERRY_EXTRA=$(JERRY_ROOT)/jerry-core/include/

#jrt for JERRY_ASSERT
JERRY_JRT=$(JERRY_ROOT)/jerry-core/jrt

#tools
PIN_GENERATOR=./tools/generate_pins.py
JS_SOURCE=./js

#add logprint
ifneq ($(LOG_PRINT),)
MBED_CLI_FLAGS += -D LOGPRINT
endif
#add verbose
ifeq ($(MBED_VERBOSE), 1)
MBED_CLI_FLAGS += -v
endif
ifeq ($(MBED_VERBOSE), 2)
MBED_CLI_FLAGS += -vv
endif
#add mbed debug
ifeq ($(MBED_DEBUG), 1)
MBED_CLI_FLAGS += --profile ./mbed-os/tools/profiles/debug.json
endif
#add jerry heap size
MBED_CLI_FLAGS += -D "JERRY_GLOBAL_HEAP_SIZE=$(JERRY_HEAP_SIZE)"
#add cc
MBED_CLI_FLAGS += -t $(CC)
#add target board
MBED_CLI_FLAGS += -m $(TARGET)
#add build dir
MBED_CLI_FLAGS += --build $(BUILD)
#set source directories with '--source SOURCE' mbed-cli builtin command
SOURCE += --source .
ifneq ($(MBED),)
SOURCE += --source $(MBED)
endif
SOURCE += --source $(JERRY_SOURCE)
SOURCE += --source $(JERRY_EXTRA)
#SOURCE += --source $(JERRY_JRT)
SOURCE += --source $(JERRY_EXT_SOURCE_H)
SOURCE += --source $(JERRY_EXT_SOURCE)
MBED_CLI_FLAGS += $(SOURCE)

all: js2c jerrysrcgen flash serialport

#first '--source' flag must be named as the project dir because mbed '--flash' is sending binary named $(project dir name).bin,
#and 'mbed compile' creates binary named as the first source
flash: js2c jerrysrcgen generatepins
	mbed compile $(MBED_CLI_FLAGS) --flash

build: js2c jerrysrcgen generatepins
	mbed compile $(MBED_CLI_FLAGS)

#open serial port to the targtet device
serialport:          #sterm -- mbed cli builtin program, it takes time to get access to the port, use sleep if you want open the port whit mbed sterm right after flashing
	#sleep 5
	mbed sterm

serialport2:         #minicom -- 3rd party program, works faster but sometimes you wont get access to the port
	sudo chmod 777 $(PORT)
	minicom -D $(PORT) -b 9600

#srcgenerator.py
jerrysrcgen:
	python3 $(JERRY_ROOT)/tools/srcgenerator.py --jerry-core --jerry-libm --output-dir $(JERRY_SOURCE)

#parse JS code to c  -- output: ./js/include/jerry-targetjs.h
js2c:
	python3 $(JERRY_ROOT)/tools/js2c.py --dest $(JS_SOURCE) --js-source $(JS_SOURCE)

#generate_pins.py -- output: pins.cpp
generatepins:
	python3 $(PIN_GENERATOR) $(TARGET)

#clean Build directory
clean:
	rm -rf BUILD/*

#clean jerry's build directory
cleanjerry:
	rm -rf $(JERRY_SOURCE)

#install Mbed dependencies
mbeddependencies:
	dpkg --add-architecture i386
	apt-get install libc6:i386 libncurses5:i386 libstdc++6:i386
	apt install python python3 python3-pip git mercurial
	apt install gcc-arm-none-eabi

#install mbed-cli and mbed-os
mbedinstall: mbeddependencies
	pip3 install mbed-cli
	python3 -m pip install -U mbed-cli
	mbed import mbed-os
	cd mbed-os && mbed update mbed-os-6.3.0
	pip3 install -r mbed-os/requirements.txt
	pip3 install -r tools/requirements.txt

#set compiler and target in config
mbedconfig: mbeddependencies mbedinstall
	mbed config root .
	mbed toolchain $(CC)
	mbed target $(TARGET)
