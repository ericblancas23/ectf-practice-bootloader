import argparse, struct, sys, time
from pathlib import Path
import serial

CHUNK = 256


def expect_k(port, what):
    b = port.read(1)
    if b == b"K":
        return
    if not b:
        sys.exit(f"\ntimeout waiting for ack ({what})")
    msg = (b + port.readline()).decode(errors="replace").strip()
    sys.exit(f"\nbootloader error during {what}: {msg}")


def main():
    p = argparse.ArgumentParser(description="Send a signed image to the bootloader over UART")
    p.add_argument("--port", default="/dev/ttyACM0")
    p.add_argument("--image-a", default="build/app_a.img")
    p.add_argument("--image-b", default="build/app_b.img")
    a = p.parse_args()

    with serial.Serial(a.port, 115200, timeout=10) as port:
        port.reset_input_buffer()
        print("Hold the blue USER button, tap black RESET, then release USER...")

        deadline = time.time() + 60
        line = b""
        while not line.startswith(b"READY"):
            if time.time() > deadline:
                sys.exit("no READY from bootloader")
            line = port.readline().strip()

        slot = line.split()[1].decode()
        path = a.image_a if slot == "A" else a.image_b
        data = Path(path).read_bytes()
        print(f"bootloader is updating slot {slot}: sending {path} ({len(data)} bytes)")

        port.write(struct.pack("<I", len(data)))
        expect_k(port, "erase")

        for off in range(0, len(data), CHUNK):
            port.write(data[off:off + CHUNK])
            expect_k(port, f"chunk at offset {off}")
            print(f"\r  {min(off + CHUNK, len(data))}/{len(data)} bytes", end="", flush=True)
        print()

        result = port.readline().decode(errors="replace").strip()
        print(f"bootloader: {result}")
        sys.exit(0 if result == "OK" else 1)


if __name__ == "__main__":
    main()
