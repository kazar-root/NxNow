import argparse
from pathlib import Path
import struct


def verify(nro_path, resources):
    data = nro_path.read_bytes()
    if data[0x10:0x14] != b"NRO0":
        raise ValueError("Not a Nintendo Switch NRO")
    executable_size = struct.unpack_from("<I", data, 0x18)[0]
    if data[executable_size:executable_size + 4] != b"ASET":
        raise ValueError("NRO has no asset header")
    icon_offset, icon_size = struct.unpack_from("<QQ", data, executable_size + 8)
    if data[executable_size + icon_offset:executable_size + icon_offset + icon_size] != (resources / "icon/nxnow-icon.jpg").read_bytes():
        raise ValueError("HB Menu icon differs from the supplied NxNow icon")
    nacp_offset, nacp_size = struct.unpack_from("<QQ", data, executable_size + 24)
    nacp = data[executable_size + nacp_offset:executable_size + nacp_offset + nacp_size]
    def c_string(data):
        return data.split(b"\0", 1)[0].decode("utf-8")
    for language in range(16):
        title = c_string(nacp[language * 0x300:language * 0x300 + 0x200])
        author = c_string(nacp[language * 0x300 + 0x200:(language + 1) * 0x300])
        if title != "NxNow" or author != "KazaR-RooT":
            raise ValueError(f"Wrong HB Menu metadata for language {language}: {title}, {author}")
    version = c_string(nacp[0x3060:0x3070])
    if version != "0.2.1":
        raise ValueError(f"Wrong NRO version: {version}")
    print("PASS HB Menu: NxNow / KazaR-RooT / 0.2.1; supplied icon; all 16 languages")
    offset, size = struct.unpack_from("<QQ", data, executable_size + 40)
    romfs = data[executable_size + offset:executable_size + offset + size]
    header = struct.unpack_from("<10Q", romfs)
    if header[0] != 80:
        raise ValueError("Unexpected RomFS header")
    directory_offset, directory_size = header[3:5]
    file_offset, file_size = header[7:9]
    directories = romfs[directory_offset:directory_offset + directory_size]
    files = romfs[file_offset:file_offset + file_size]

    def directory_path(entry):
        parent, _, _, _, _, length = struct.unpack_from("<6I", directories, entry)
        name = directories[entry + 24:entry + 24 + length].decode("utf-8")
        if entry == 0:
            return ""
        prefix = directory_path(parent)
        return f"{prefix}/{name}" if prefix else name

    entries = {}
    position = 0
    while position < len(files):
        parent, _, payload, length, _, name_length = struct.unpack_from("<IIQQII", files, position)
        name = files[position + 32:position + 32 + name_length].decode("utf-8")
        prefix = directory_path(parent)
        path = f"{prefix}/{name}" if prefix else name
        entries[path] = romfs[header[9] + payload:header[9] + payload + length]
        position += 32 + (name_length + 3) // 4 * 4

    expected = sorted((resources / "font").glob("*"))
    expected.append(resources / "img" / "nxnow-logo-mark.png")
    expected.append(resources / "img" / "nxnow-logo-mark-small.png")
    expected.append(resources / "credits.txt")
    expected.extend(sorted((resources / "icon").glob("*")))
    expected.extend(sorted((resources / "certs").glob("*")))
    for asset in expected:
        if not asset.is_file():
            raise ValueError(f"Required bundled UI asset missing: {asset}")
        path = asset.relative_to(resources).as_posix()
        if entries.get(path) != asset.read_bytes():
            raise ValueError(f"NRO asset missing or stale: {path}")
        print(f"PASS embedded {path}: {len(entries[path])} bytes")
    for path in ("shaders/fill_vsh.dksh", "shaders/fill_fsh.dksh", "shaders/fill_aa_fsh.dksh",
                 "shaders/nxnow_video_vsh.dksh", "shaders/nxnow_video_fsh.dksh",
                 "shortcut/NXNowShortcut.nro", "shortcut/NXNowForwarderInstaller.nro",
                 "licenses/LICENSE.txt", "licenses/forwarder-LICENSE.txt"):
        if not entries.get(path):
            raise ValueError(f"Missing embedded runtime asset: {path}")
    installer = resources.parent / "tools/nxnow-forwarder-installer/nxnow-forwarder-installer.nro"
    if entries["shortcut/NXNowForwarderInstaller.nro"] != installer.read_bytes():
        raise ValueError("Embedded HOME installer is stale")
    stale = [path for path in entries if "now" in path.lower() and "nxnow" not in path.lower()]
    if stale:
        raise ValueError(f"Stale branded asset names: {stale}")
    print("PASS embedded shaders, auxiliary tools and licenses; NxNow asset names")
    print(f"PASS NRO executable and RomFS: {len(data)} bytes, {len(entries)} files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Verify the final NRO contains exact bundled UI assets.")
    parser.add_argument("nro", type=Path)
    parser.add_argument("--resources", type=Path, default=Path("resources"))
    arguments = parser.parse_args()
    verify(arguments.nro, arguments.resources)
