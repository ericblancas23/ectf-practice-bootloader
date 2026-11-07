# eMITRE Practice Bootloader

A secure A/B bootloader for the **STM32 NUCLEO-L476RG** (Cortex-M4F, 1 MB flash, 96 KB SRAM), built as practice for the MITRE Embedded Capture the Flag (eCTF).

The bootloader verifies an **Ed25519-signed** firmware image in flash before jumping to it. The same verification code (`common/verify_core.c`) compiles for both the host PC and the microcontroller, so the host test suite exercises the exact logic that runs on the chip.

## Status

| Phase | Description | Status |
|---|---|---|
| 1 | Shared verify core (Monocypher Ed25519) | Done |
| 2 | Host image builder and verifier | Done |
| 3 | Bare-metal blink app | Done |
| 4 | Flashing and GDB on hardware | Done |
| 5 | Bootloader: verify slot A and jump | Done |
| 6 | A/B slot selection and anti-rollback counter | Next |
| 7 | UART firmware updater | Planned |
| 8 | Fault testing (power loss mid-update) | Planned |
| 9 | Attack and harden (Ghidra, glitch resistance, RDP) | Planned |

## Repository layout

```
app/          Blink application and its linker scripts (blink_test.ld, app_a.ld)
bootloader/   Bootloader source and linker script
common/       verify_core.c/.h, shared by host and bootloader
host/         verifier.c (host-side checker) and make_image.py (image signer)
keys/         Signing keys (git-ignored, never committed)
third_party/  Monocypher 4.0.2, CMSIS core and STM32L4 device files
Makefile
```

## Flash map

| Region | Address | Size |
|---|---|---|
| Bootloader | `0x08000000` | 64 KB |
| Rollback state | `0x08010000` | 2 KB (reserved for Phase 6) |
| Slot A | `0x08020000` | 384 KB |
| Slot B | `0x08080000` | 384 KB (reserved for Phase 6) |

## Image format

```
[ header, padded to 0x200 bytes ][ payload (app .bin) ][ Ed25519 signature, 64 bytes ]
```

Header fields (little-endian):

| Offset | Field | Notes |
|---|---|---|
| 0 | magic | `"FWUP"` |
| 4 | version | Checked against the minimum allowed version |
| 8 | payload_len | Bounds-checked without integer overflow |
| 12 | load_addr | Must match the slot the image sits in |

The signature covers the header and the payload, so the version and load address cannot be changed without invalidating it. The header is padded to 0x200 so the app's vector table is aligned for `SCB->VTOR`.

## Prerequisites (Ubuntu / WSL)

```bash
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi gdb-multiarch \
                 openocd stlink-tools make python3-pip
pip install --break-system-packages cryptography
```

On Windows with WSL, pass the board through with [usbipd-win](https://github.com/dorssel/usbipd-win) from an admin PowerShell:

```powershell
usbipd list                          # find the 0483:374b ST-Link entry
usbipd bind --busid <BUSID>          # one time
usbipd attach --wsl --busid <BUSID>  # after every replug
```

Confirm the board is visible from WSL with `st-info --probe`.

## Keys

Generate an Ed25519 key pair and export the raw 32-byte public key that gets embedded in the bootloader:

```bash
openssl genpkey -algorithm ed25519 -out keys/private.pem
openssl pkey -in keys/private.pem -pubout -out keys/public.pem
openssl pkey -pubin -in keys/public.pem -outform DER | tail -c 32 > keys/public.raw
```

All key files are git-ignored. Never commit `private.pem`.

## Build

```bash
make                       # host verifier, bootloader, and signed app image
make VERSION=3 DELAY=100000  # signed image with a different version and blink speed
make clean
```

Build outputs go to `build/`:

| File | Description |
|---|---|
| `verifier` | Host-side image checker |
| `bootloader.bin` | Bootloader, flashed at `0x08000000` |
| `app_a_v<N>.img` | Signed app image for slot A |

## Flash

```bash
make flash-boot   # bootloader -> 0x08000000
make flash-a      # signed image -> 0x08020000
```

Expected LED (LD2) behavior:

| LED | Meaning |
|---|---|
| Solid briefly, then slow blink | Image verified, app running |
| Rapid blink | Image rejected |

Verification takes a moment at the default 4 MHz clock.

## Testing

Check an image on the host before flashing it:

```bash
build/verifier build/app_a_v2.img keys/public.raw [min_version]
```

Tamper test: flip one payload byte and confirm both the host and the board reject it:

```bash
python3 -c "import sys; d=bytearray(open(sys.argv[1],'rb').read()); d[0x300]^=1; open(sys.argv[2],'wb').write(d)" \
    build/app_a_v2.img build/app_a_tampered.img
build/verifier build/app_a_tampered.img keys/public.raw   # REJECT
st-flash --reset write build/app_a_tampered.img 0x08020000 # rapid blink
make flash-a                                               # restore
```

## Debugging

```bash
openocd -f board/st_nucleo_l4.cfg            # terminal 1
gdb-multiarch build/bootloader.elf           # terminal 2
(gdb) target extended-remote :3333
(gdb) monitor reset halt
(gdb) break main
(gdb) continue
```

To step into the app after the jump, load its symbols too: `add-symbol-file build/app_a.elf`.

## Security notes and known limitations

- The minimum version is currently `0` on the device. Anti-rollback is not enforced until Phase 6.
- Only slot A is checked. There is no fallback slot yet.
- The signature check is a single branch, which is vulnerable to fault injection. Hardening is planned for Phase 9.
- Flash read protection (RDP) is not set, so a debugger can read and rewrite everything. **Never set RDP level 2 on this board:** it is permanent.

## Third-party code

- [Monocypher](https://monocypher.org) 4.0.2 (BSD-2-Clause / CC0)
- [CMSIS](https://github.com/ARM-software/CMSIS_5) core headers (Apache-2.0)
- [STM32L4 CMSIS device files](https://github.com/STMicroelectronics/cmsis_device_l4) (Apache-2.0)
