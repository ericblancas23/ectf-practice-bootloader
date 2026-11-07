import argparse, struct
from pathlib import Path
from cryptography.hazmat.primitives.serialization import load_pem_private_key

HEADER_SIZE = 0x200

def build(payload: bytes, version: int, load_addr: int, key) -> bytes:
    header = struct.pack("<4sIII", b"FWUP", version, len(payload), load_addr)
    signed = header.ljust(HEADER_SIZE, b"\0") + payload
    return signed + key.sign(signed)

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("payload"); p.add_argument("out")
    p.add_argument("--version", type=int, required=True)
    p.add_argument("--addr", type=lambda s: int(s, 0), required=True)
    p.add_argument("--key", default="keys/private.pem")
    a = p.parse_args()
    key = load_pem_private_key(Path(a.key).read_bytes(), password=None)
    out = build(Path(a.payload).read_bytes(), a.version, a.addr, key)
    Path(a.out).write_bytes(out)
    print(f"wrote {a.out}: {len(out)} bytes, version {a.version}, addr 0x{a.addr:08X}")
