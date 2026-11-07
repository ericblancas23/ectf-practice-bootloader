CC_ARM  = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
ARCH    = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS  = $(ARCH) -Os -g -Wall -ffunction-sections -fdata-sections -DSTM32L476xx \
          -Ithird_party/cmsis -Icommon -Ithird_party/monocypher -Ibuild
LDFLAGS = --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections

CMSIS = third_party/cmsis/startup_stm32l476xx.s third_party/cmsis/system_stm32l4xx.c
MONO  = third_party/monocypher/monocypher.c third_party/monocypher/monocypher-ed25519.c

VERSION ?= 2
DELAY   ?= 500000

all: build/verifier build/bootloader.bin build/app_a_v$(VERSION).img

build:
	mkdir -p build

build/verifier: host/verifier.c common/verify_core.c $(MONO) | build
	gcc -Wall -Wextra -O2 -Icommon -Ithird_party/monocypher $^ -o $@

build/pubkey.h: keys/public.raw | build
	python3 -c "import sys; b=open(sys.argv[1],'rb').read(); assert len(b)==32; print('static const unsigned char PUBKEY[32] = {' + ','.join(hex(x) for x in b) + '};')" $< > $@

build/bootloader.elf: bootloader/main.c common/verify_core.c $(MONO) $(CMSIS) bootloader/bootloader.ld build/pubkey.h
	$(CC_ARM) $(CFLAGS) $(LDFLAGS) -T bootloader/bootloader.ld \
	    bootloader/main.c common/verify_core.c $(MONO) $(CMSIS) -o $@

build/app_a.elf: app/main.c $(CMSIS) app/app_a.ld | build
	$(CC_ARM) $(CFLAGS) $(LDFLAGS) -DBLINK_DELAY=$(DELAY) -T app/app_a.ld \
	    app/main.c $(CMSIS) -o $@

build/%.bin: build/%.elf
	$(OBJCOPY) -O binary $< $@

build/app_a_v$(VERSION).img: build/app_a.bin
	python3 host/make_image.py $< $@ --version $(VERSION) --addr 0x08020000

flash-boot: build/bootloader.bin
	st-flash --reset write $< 0x08000000

flash-a: build/app_a_v$(VERSION).img
	st-flash --reset write $< 0x08020000

clean:
	rm -rf build

.PHONY: all flash-boot flash-a clean