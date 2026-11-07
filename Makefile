.RECIPEPREFIX = >

CC_ARM  = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
ARCH    = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS  = $(ARCH) -Os -g -Wall -ffunction-sections -fdata-sections -DSTM32L476xx -Ithird_party/cmsis -Icommon -Ithird_party/monocypher -Ibuild
LDFLAGS = --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections

CMSIS = third_party/cmsis/startup_stm32l476xx.s third_party/cmsis/system_stm32l4xx.c
MONO  = third_party/monocypher/monocypher.c third_party/monocypher/monocypher-ed25519.c

SLOT_A = 0x08020000
SLOT_B = 0x08080000

VERSION_A ?= 2
VERSION_B ?= 3
DELAY_A   ?= 500000
DELAY_B   ?= 250000

all: build/verifier build/bootloader.bin build/app_a.img build/app_b.img

build:
> mkdir -p build

keys/public.raw: keys/public.pem
> openssl pkey -pubin -in $< -outform DER | tail -c 32 > $@

build/verifier: host/verifier.c common/verify_core.c common/verify_core.h $(MONO) | build
> gcc -Wall -Wextra -O2 -Icommon -Ithird_party/monocypher host/verifier.c common/verify_core.c $(MONO) -o $@

build/pubkey.h: keys/public.raw | build
> python3 -c "import sys; b=open(sys.argv[1],'rb').read(); assert len(b)==32; print('static const unsigned char PUBKEY[32] = {' + ','.join(hex(x) for x in b) + '};')" $< > $@

build/bootloader.elf: bootloader/main.c bootloader/bootloader.ld common/verify_core.c common/verify_core.h $(MONO) $(CMSIS) build/pubkey.h
> $(CC_ARM) $(CFLAGS) $(LDFLAGS) -T bootloader/bootloader.ld bootloader/main.c common/verify_core.c $(MONO) $(CMSIS) -o $@

# Apps always rebuild so VERSION_x / DELAY_x changes take effect.
build/app_a.elf: FORCE | build
> $(CC_ARM) $(CFLAGS) $(LDFLAGS) -DBLINK_DELAY=$(DELAY_A) -T app/app_a.ld app/main.c $(CMSIS) -o $@

build/app_b.elf: FORCE | build
> $(CC_ARM) $(CFLAGS) $(LDFLAGS) -DBLINK_DELAY=$(DELAY_B) -T app/app_b.ld app/main.c $(CMSIS) -o $@

build/%.bin: build/%.elf
> $(OBJCOPY) -O binary $< $@

build/app_a.img: build/app_a.bin FORCE
> python3 host/make_image.py $< $@ --version $(VERSION_A) --addr $(SLOT_A)

build/app_b.img: build/app_b.bin FORCE
> python3 host/make_image.py $< $@ --version $(VERSION_B) --addr $(SLOT_B)

build/zero.bin: | build
> head -c 2048 /dev/zero > $@

flash-boot: build/bootloader.bin
> st-flash --reset write $< 0x08000000

flash-a: build/app_a.img
> st-flash --reset write $< $(SLOT_A)

flash-b: build/app_b.img
> st-flash --reset write $< $(SLOT_B)

# Overwrite a slot's first page with zeros, destroying its header.
wipe-a: build/zero.bin
> st-flash --reset write $< $(SLOT_A)

wipe-b: build/zero.bin
> st-flash --reset write $< $(SLOT_B)

clean:
> rm -rf build

FORCE:

.PHONY: all flash-boot flash-a flash-b wipe-a wipe-b clean FORCE
