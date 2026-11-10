"""Populate all sixteen title/author slots, including recent console languages."""
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
if len(data) != 0x4000:
    raise ValueError("Expected a 16 KiB NACP")
primary = data[:0x300]
if not primary[0] or not primary[0x200]:
    raise ValueError("NACP must contain a title and author")
for language in range(16):
    data[language * 0x300:(language + 1) * 0x300] = primary
path.write_bytes(data)
