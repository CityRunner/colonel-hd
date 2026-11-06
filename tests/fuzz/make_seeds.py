#!/usr/bin/env python3
"""Writes the synthetic seed corpora of the fuzz targets.

    python3 tests/fuzz/make_seeds.py

The seeds are small hand-made SCI0 structures (no game data): stored, LZW and
Huffman entries, a resource map, patch files, pictures, a view and a font. Run it again after changing
it; the generated files are committed.
"""

import struct
from pathlib import Path

HERE = Path(__file__).resolve().parent


def lzw(codes: list[int]) -> bytes:
    """Packs 9-bit codes least significant bit first."""
    value, bits = 0, 0
    for index, code in enumerate(codes):
        value |= code << (9 * index)
        bits += 9
    return value.to_bytes((bits + 7) // 8, "little")


def msb_bits(fields: list[tuple[int, int]]) -> bytes:
    value, bits = 0, 0
    for field, width in fields:
        value = (value << width) | field
        bits += width
    pad = (8 - bits % 8) % 8
    return (value << pad).to_bytes((bits + pad) // 8, "big")


LZW_ABAB = lzw([ord("A"), ord("B"), 258, 257])
HUFFMAN_XXQ = bytes([2, ord("T"), 0x00, 0x10, ord("X"), 0x00]) + msb_bits(
    [(0, 1), (0, 1), (1, 1), (ord("Q"), 8), (1, 1), (ord("T"), 8)]
)


def entry(type_: int, number: int, method: int, unpacked: int, packed: bytes) -> bytes:
    return struct.pack("<HHHH", (type_ << 11) | number, len(packed) + 4, unpacked, method) + packed


def folder(files: dict[str, bytes]) -> bytes:
    out = b""
    for name, content in files.items():
        out += bytes([len(name)]) + name.encode() + struct.pack("<H", len(content)) + content
    return out


def main() -> None:
    decompress = HERE / "corpus" / "decompress"
    for name, method, unpacked, data in (
        ("stored", 0, 3, b"abc"),
        ("lzw", 1, 4, LZW_ABAB),
        ("huffman", 2, 3, HUFFMAN_XXQ),
    ):
        (decompress / name).write_bytes(bytes([method]) + struct.pack("<HH", len(data), unpacked) + data)

    volume = entry(0, 1, 0, 3, b"abc")
    lzw_offset = len(volume)
    volume += entry(2, 52, 1, 4, LZW_ABAB)
    huffman_offset = len(volume)
    volume += entry(1, 3, 2, 3, HUFFMAN_XXQ)
    resource_map = (
        struct.pack("<HI", (0 << 11) | 1, (1 << 26) | 0)
        + struct.pack("<HI", (2 << 11) | 52, (1 << 26) | lzw_offset)
        + struct.pack("<HI", (1 << 11) | 3, (1 << 26) | huffman_offset)
        + b"\xff" * 6
    )
    for name, data in pictures().items():
        (HERE / "corpus" / "pic" / name).write_bytes(data)
    (HERE / "corpus" / "view_font" / "view").write_bytes(view())
    (HERE / "corpus" / "view_font" / "font").write_bytes(font())
    scene_corpus = HERE / "corpus" / "scene"
    scene_corpus.mkdir(exist_ok=True)
    for name, data in scenes().items():
        (scene_corpus / name).write_bytes(data)

    patch_corpus = HERE / "corpus" / "patch"
    patch_corpus.mkdir(exist_ok=True)
    for name, data in patches().items():
        (patch_corpus / name).write_bytes(data)
    sound_corpus = HERE / "corpus" / "sound"
    sound_corpus.mkdir(exist_ok=True)
    for name, data in sounds().items():
        (sound_corpus / name).write_bytes(data)
    script_corpus = HERE / "corpus" / "script"
    script_corpus.mkdir(exist_ok=True)
    for name, data in scripts().items():
        (script_corpus / name).write_bytes(data)
    config_corpus = HERE / "corpus" / "config"
    config_corpus.mkdir(exist_ok=True)
    for name, data in configs().items():
        (config_corpus / name).write_bytes(data)
    game_folder = HERE / "corpus" / "game_folder"
    (game_folder / "volume").write_bytes(folder({"RESOURCE.MAP": resource_map, "RESOURCE.001": volume}))
    (game_folder / "patches").write_bytes(
        folder(
            {
                "RESOURCE.MAP": resource_map,
                "RESOURCE.001": volume,
                "SCRIPT.052": b"\x82\x00patched",
                "3.p56": b"\x81\x00\x01\x02",
            }
        )
    )


def absolute(x: int, y: int) -> bytes:
    return bytes([((x >> 4) & 0xF0) | ((y >> 8) & 0x0F), x & 0xFF, y & 0xFF])


def pictures() -> dict[str, bytes]:
    outline = b"".join(
        b"\xf6" + absolute(*a) + absolute(*b)
        for a, b in (((10, 10), (60, 10)), ((60, 10), (60, 40)), ((60, 40), (10, 40)), ((10, 40), (10, 10)))
    )
    lines = b"\xf0\x04\xf2\x07" + outline + b"\xf7" + absolute(100, 50) + b"\x3a\x91"
    lines += b"\xf5" + absolute(150, 80) + b"\x85\xfd\x02\x04"
    fills = b"\xf0\x11\xf8" + absolute(30, 20) + b"\xf1\xf2\x09\xf8" + absolute(200, 100)
    patterns = b"\xf0\x21\xf9\x23\xfa\x0a" + absolute(80, 120) + b"\xf9\x13\xf4" + absolute(319, 60) + b"\x12"
    palette = b"\xfe\x00\x29\x22\xfe\x01\x02" + bytes(range(40)) + b"\xfe\x03\x00"
    return {
        "lines": b"\x00" + lines + b"\xff",
        "fills-mirrored": b"\x01" + lines + fills + b"\xff",
        "patterns-palette": b"\x04" + palette + patterns + b"\xfb\x03" + fills + b"\xff",
    }


def view() -> bytes:
    header = bytes([2, 0]) + struct.pack("<HHHHH", 2, 0, 0, 12, 12)
    loop = struct.pack("<HHH", 1, 0, 18)
    cel = struct.pack("<HH", 3, 2) + bytes([0xFE, 5, 7, 0x21, 0x12, 0x33])
    return header + loop + cel


def font() -> bytes:
    return bytes([0, 0, 3, 0, 9, 0, 12, 0, 0xFF, 0, 18, 0, 10, 2, 0xA0, 0x40, 0x01, 0x80, 0, 0])


def scenes() -> dict[str, bytes]:
    """Vector scenes in the analysis-cache format (docs/formats/analysis-cache.md)."""

    def header(plane: int) -> bytes:
        return b"SHDS" + struct.pack("<HB", 1, plane)

    def point(x: float, y: float) -> bytes:
        return struct.pack("<dd", x, y)

    blocks = bytes(320 * 190)
    empty = header(0) + struct.pack("<I", 0) + struct.pack("<I", 0) + blocks + struct.pack("<I", 0)
    faces = struct.pack("<I", 2) + bytes([0, 3, 1, 0])
    edges = struct.pack("<I", 1) + point(0, 0) + point(0, 190) + struct.pack("<i", 1)
    stroke = (
        bytes([1]) + struct.pack("<iI", 4, 2) + point(1.5, 2.5) + point(7.25, 9.125)
        + struct.pack("<d", 0.5) + bytes([0x4C, 1]) + struct.pack("<I", 1) + point(7, 9) + point(0.6, -0.8)
    )
    ellipse = bytes([2]) + struct.pack("<i", 5) + point(50.5, 60.5) + struct.pack("<dd", 3, 2) + bytes([9])
    box = bytes([3]) + struct.pack("<i", -1) + struct.pack("<dddd", 1, 2, 3, 4) + bytes([1])
    block_layer = bytes([4]) + struct.pack("<iI", 8, 2) + struct.pack("<IB", 5, 1) + struct.pack("<IB", 400, 2)
    layers = struct.pack("<I", 4) + stroke + ellipse + box + block_layer
    full = header(1) + faces + edges + blocks + layers
    return {"empty": empty, "layers": full}


# One AdLib instrument: two operators of 13 bytes, then the waveforms
# (tests/unit/audio/adlib_test.cpp).
ADLIB_INSTRUMENT = bytes([1, 2, 3, 4, 5, 1, 6, 7, 8, 1, 0, 1, 1, 2, 3, 0, 9, 10, 0, 11, 12, 20, 0, 1, 0, 0, 1, 2])


def mt32_patch(timbres: int, patches_49_96: bool, rhythm: bool) -> bytes:
    """A patch.001 as tests/unit/audio/mt32_driver_test.cpp builds it."""
    data = bytearray(491)
    for at, text in ((0, b"After"), (20, b"Before"), (40, b"Goodbye")):
        data[at : at + 20] = text.ljust(20, b" ")
    data[60] = 120
    data[62] = 2
    for i in range(11):
        data[74 + i] = i
        data[85 + i] = 10 + i
        data[96 + i] = 20 + i
    data[107:491] = b"\x11" * 384
    data.append(timbres)
    for t in range(timbres):
        data += bytes([t]) * 246
    if patches_49_96:
        data += b"\xab\xcd" + b"\x49" * 384
    if rhythm:
        data += b"\xdc\xba" + b"\x52" * 256 + bytes(range(1, 10))
    return bytes(data)


def patches() -> dict[str, bytes]:
    """AdLib banks of 48 and 96 instruments, and MT-32 patches."""
    bank48 = bytearray(1344)
    bank48[0:28] = ADLIB_INSTRUMENT
    bank96 = bytearray(2690)
    bank96[0:28] = ADLIB_INSTRUMENT
    bank96[1346 : 1346 + 28] = ADLIB_INSTRUMENT
    return {
        "adlib-48": bytes(bank48),
        "adlib-96": bytes(bank96),
        "mt32-plain": mt32_patch(2, False, False),
        "mt32-full": mt32_patch(3, True, True),
    }


def sci0_sound(adlib_channels: list[int], song: bytes) -> bytes:
    """The 0x21-byte SCI0 header (channels played on AdLib), then the song
    (tests/unit/vm/music_device_test.cpp)."""
    header = bytearray(0x21)
    for channel in adlib_channels:
        header[1 + channel * 2] = 1
        header[2 + channel * 2] = 0x04
    return bytes(header) + song


def sounds() -> dict[str, bytes]:
    """A note with a signal, a cue and the end; a loop point; two channels."""
    one_note = bytes([0x00, 0xC0, 0x00, 0x00, 0x90, 0x3C, 0x7F, 0x1E, 0x80, 0x3C,
                      0x00, 0x00, 0xCF, 0x05, 0x1E, 0xBF, 0x60, 0x02, 0x1E, 0xFC])
    looped = bytes([0x00, 0xBF, 0x60, 0x7F, 0x00, 0x90, 0x40, 0x60, 0x0F, 0x80, 0x40,
                    0x00, 0x0F, 0xB0, 0x07, 0x50, 0x00, 0xE0, 0x00, 0x50, 0x0F, 0xFC])
    two = bytes([0x00, 0xC0, 0x01, 0x00, 0xC1, 0x02, 0x00, 0x90, 0x30, 0x40, 0x00, 0x91, 0x37, 0x40,
                 0x20, 0x80, 0x30, 0x00, 0x00, 0x81, 0x37, 0x00, 0x00, 0xFC])
    return {
        "one-note": sci0_sound([0], one_note),
        "loop": sci0_sound([0], looped),
        "two-channels": sci0_sound([0, 1], two),
    }


def scripts() -> dict[str, bytes]:
    """SCI0 scripts as blocks (type, size including the 4-byte head, body),
    then the terminator: script 0 of tests/unit/vm/save_world.hpp (exports
    and four locals), and the same with code and strings blocks."""

    def block(type_: int, body: bytes) -> bytes:
        return struct.pack("<HH", type_, len(body) + 4) + body

    exports = block(7, struct.pack("<HH", 1, 0))
    locals_ = block(10, bytes(8))
    code = block(2, bytes([0x35, 0x01, 0x48]))  # ldi 1; ret
    strings = block(5, b"hello\x00world\x00")
    return {
        "script0": exports + locals_ + b"\x00\x00",
        "code-strings": exports + locals_ + code + strings + b"\x00\x00",
    }


def configs() -> dict[str, bytes]:
    """Configuration files: every key, comments, CRLF, unknown keys."""
    full = (
        "; sierra-hd configuration\n[sierra-hd]\nformat = 1\ngame_folder = /games/cb;1#x\nmode = original\n"
        "scale = 5\npriority_snap = on\nmt32_rom = /roms\nmusic = mt32\nvolume = 40\nsaves = saves\n"
    )
    return {
        "full": full.encode(),
        "crlf": full.replace("\n", "\r\n").encode(),
        "defaults": b"[sierra-hd]\nformat = 1\n",
        "odd": b"# comment\n[other]\nx = 1\n[sierra-hd]\nformat = 1\nscale = auto ; the largest\nnew_key = 3\nvolume = 101\n",
    }


if __name__ == "__main__":
    main()
