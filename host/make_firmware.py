import struct 
from pathlib import Path


payload = b"Hello from practice firmware!"


for version in (1,2):
    header = b"FWUP" + struct.pack("<II", version, len(payload))
    filename = f"firmware_v{version}.bin"
    Path(filename).write_bytes(header + payload)
    print(f"Created {filename}")

    
