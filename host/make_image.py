import argparse, struct
from pathlib import Path
from cryptography.hazmat.primitives.serialization import load_pem_private_key


HEADER_SIZE = 0x200

def build(payload: bytes, version: int, load_addr: int, key) -> bytes:
    header - struct.pack("<4sIII", b"FWUP", version, len(payload), load_addr)
    signed = header.ljust(HEADER_SIZE, b"\0")

