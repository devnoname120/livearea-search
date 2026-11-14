from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator


MAX_TEXTURE_SIZE = 64 * 1024 * 1024


@dataclass(frozen=True)
class RcoHeader:
    magic: bytes
    version: int
    tree_offset: int
    tree_size: int
    idtable_offset: int
    idtable_size: int
    idhashtable_offset: int
    idhashtable_size: int
    stringtable_offset: int
    stringtable_size: int
    wstringtable_offset: int
    wstringtable_size: int
    hashtable_offset: int
    hashtable_size: int
    intarraytable_offset: int
    intarraytable_size: int
    floatarraytable_offset: int
    floatarraytable_size: int
    filetable_offset: int
    filetable_size: int


@dataclass(frozen=True)
class RcoAttribute:
    name: str
    attribute_type: int
    first: int
    second: int


@dataclass(frozen=True)
class RcoNode:
    offset: int
    tag: str
    attributes: tuple[RcoAttribute, ...]


@dataclass(frozen=True)
class DecodedGim:
    width: int
    height: int
    format: int
    order: int
    rgba: bytes


def read_u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def read_cstring(data: bytes, offset: int, limit: int) -> str:
    if offset < 0 or offset >= limit:
        raise ValueError(f"string offset 0x{offset:X} is outside the table")
    end = data.find(b"\0", offset, limit)
    if end < 0:
        raise ValueError(f"unterminated string at 0x{offset:X}")
    return data[offset:end].decode("utf-8")


def parse_header(data: bytes) -> RcoHeader:
    if len(data) < struct.calcsize("<4s19i"):
        raise ValueError("RCO header is truncated")
    header = RcoHeader(*struct.unpack_from("<4s19i", data, 0))
    if header.magic not in (b"CXML", b"RCOF"):
        raise ValueError(f"unsupported RCO magic {header.magic!r}")
    for label, offset, size in (
        ("tree", header.tree_offset, header.tree_size),
        ("id hash", header.idhashtable_offset, header.idhashtable_size),
        ("string", header.stringtable_offset, header.stringtable_size),
        ("file", header.filetable_offset, header.filetable_size),
    ):
        if offset < 0 or size < 0 or offset + size > len(data):
            raise ValueError(f"{label} table is outside the RCO")
    return header


def iter_nodes(data: bytes, header: RcoHeader) -> Iterator[RcoNode]:
    seen: set[int] = set()

    def walk(offset: int) -> Iterator[RcoNode]:
        while offset != -1:
            if offset in seen:
                raise ValueError(f"RCO tree cycle at 0x{offset:X}")
            if offset < 0 or offset + 28 > header.tree_size:
                raise ValueError(f"RCO node 0x{offset:X} is outside the tree")
            seen.add(offset)
            base = header.tree_offset + offset
            (
                name_handle,
                attribute_count,
                _parent,
                _previous,
                next_offset,
                first_child,
                _last_child,
            ) = struct.unpack_from("<7i", data, base)
            tag = read_cstring(
                data,
                header.stringtable_offset + name_handle,
                header.stringtable_offset + header.stringtable_size,
            )
            attributes = []
            for index in range(attribute_count):
                attribute_offset = base + 28 + index * 16
                if attribute_offset + 16 > header.tree_offset + header.tree_size:
                    raise ValueError(f"attributes for node 0x{offset:X} are truncated")
                name_offset, attribute_type, first, second = struct.unpack_from(
                    "<4I", data, attribute_offset
                )
                name = read_cstring(
                    data,
                    header.stringtable_offset + name_offset,
                    header.stringtable_offset + header.stringtable_size,
                )
                attributes.append(
                    RcoAttribute(name, attribute_type, first, second)
                )
            yield RcoNode(offset, tag, tuple(attributes))
            if first_child != -1:
                yield from walk(first_child)
            offset = next_offset

    yield from walk(0)


def attribute_map(node: RcoNode) -> dict[str, RcoAttribute]:
    attributes: dict[str, RcoAttribute] = {}
    for attribute in node.attributes:
        if attribute.name in attributes:
            raise ValueError(
                f"duplicate attribute {attribute.name!r} on node 0x{node.offset:X}"
            )
        attributes[attribute.name] = attribute
    return attributes


def decode_id_hash(data: bytes, header: RcoHeader, attribute: RcoAttribute) -> int:
    if attribute.attribute_type not in (11, 12):
        raise ValueError("RCO id attribute is not an id-hash reference")
    offset = header.idhashtable_offset + attribute.first
    if offset < header.idhashtable_offset or offset + 8 > (
        header.idhashtable_offset + header.idhashtable_size
    ):
        raise ValueError("RCO id-hash reference is outside the table")
    return struct.unpack_from("<I", data, offset + 4)[0]


def decode_string(data: bytes, header: RcoHeader, attribute: RcoAttribute) -> str:
    if attribute.attribute_type != 3:
        raise ValueError(f"attribute {attribute.name!r} is not a string")
    start = header.stringtable_offset + attribute.first
    end = start + attribute.second
    if start < header.stringtable_offset or end > (
        header.stringtable_offset + header.stringtable_size
    ):
        raise ValueError(f"attribute {attribute.name!r} is outside the string table")
    return data[start:end].decode("utf-8")


def locate_texture(
    data: bytes,
    header: RcoHeader,
    texture_id: int,
) -> tuple[RcoNode, dict[str, RcoAttribute]]:
    matches = []
    for node in iter_nodes(data, header):
        if node.tag != "texture":
            continue
        attributes = attribute_map(node)
        identifier = attributes.get("id")
        if identifier is None:
            continue
        if decode_id_hash(data, header, identifier) == texture_id:
            matches.append((node, attributes))
    if len(matches) != 1:
        raise ValueError(
            f"expected one texture 0x{texture_id:08X}, found {len(matches)}"
        )
    return matches[0]


def gim_walk(
    data: bytes,
    offset: int,
    end: int,
    found: dict[str, int | None],
    seen: set[int],
) -> None:
    while offset + 16 <= end:
        if offset in seen:
            raise ValueError(f"GIM block cycle at 0x{offset:X}")
        seen.add(offset)
        identifier = read_u16(data, offset)
        size = read_u32(data, offset + 4)
        data_offset = read_u32(data, offset + 12)
        if size < 16 or offset + size > end:
            return
        if identifier in (2, 3):
            gim_walk(data, offset + data_offset, end, found, seen)
        elif identifier == 4 and found["image"] is None:
            found["image"] = offset + data_offset
        elif identifier == 5 and found["palette"] is None:
            found["palette"] = offset + data_offset
        offset += size


def decode_pixel(value: int, pixel_format: int) -> tuple[int, int, int, int]:
    if pixel_format == 0:
        return (
            (value & 31) * 255 // 31,
            ((value >> 5) & 63) * 255 // 63,
            ((value >> 11) & 31) * 255 // 31,
            255,
        )
    if pixel_format == 1:
        return (
            (value & 31) * 255 // 31,
            ((value >> 5) & 31) * 255 // 31,
            ((value >> 10) & 31) * 255 // 31,
            255 if value & 0x8000 else 0,
        )
    if pixel_format == 2:
        return (
            (value & 15) * 17,
            ((value >> 4) & 15) * 17,
            ((value >> 8) & 15) * 17,
            ((value >> 12) & 15) * 17,
        )
    return (
        value & 255,
        (value >> 8) & 255,
        (value >> 16) & 255,
        (value >> 24) & 255,
    )


def decode_gim(data: bytes) -> DecodedGim:
    if len(data) < 64 or data[:11] != b"MIG.00.1PSP":
        raise ValueError("texture is not a supported PSP GIM")
    found: dict[str, int | None] = {"image": None, "palette": None}
    gim_walk(data, 16, len(data), found, set())
    image = found["image"]
    palette_offset = found["palette"]
    if image is None:
        raise ValueError("GIM image block is missing")

    pixel_format = read_u16(data, image + 4)
    order = read_u16(data, image + 6)
    width = read_u16(data, image + 8)
    height = read_u16(data, image + 10)
    pixel_start = read_u32(data, image + 0x1C)
    pixel_end = read_u32(data, image + 0x20)
    if width < 1 or height < 1 or width > 2048 or height > 2048:
        raise ValueError(f"unsupported GIM dimensions {width}x{height}")
    if pixel_end <= pixel_start or image + pixel_end > len(data):
        raise ValueError("GIM pixel range is invalid")

    bits_per_pixel = {0: 16, 1: 16, 2: 16, 3: 32, 4: 4, 5: 8}.get(
        pixel_format
    )
    if bits_per_pixel is None:
        raise ValueError(f"unsupported GIM pixel format {pixel_format}")
    pitch = (width * bits_per_pixel + 7) // 8
    aligned_pitch = (pitch + 15) & ~15
    aligned_height = (height + 7) & ~7
    source = data[image + pixel_start : image + pixel_end]
    linear = bytearray(aligned_pitch * aligned_height)
    if order == 1:
        row_blocks = aligned_pitch // 16
        source_offset = 0
        for block_y in range(0, aligned_height, 8):
            for block_x in range(row_blocks):
                for row in range(8):
                    if source_offset + 16 <= len(source):
                        destination = (
                            (block_y + row) * aligned_pitch + block_x * 16
                        )
                        linear[destination : destination + 16] = source[
                            source_offset : source_offset + 16
                        ]
                    source_offset += 16
    elif order == 0:
        linear[: min(len(linear), len(source))] = source[: len(linear)]
    else:
        raise ValueError(f"unsupported GIM pixel order {order}")

    palette = [(0, 0, 0, 0)] * 256
    if pixel_format >= 4:
        if palette_offset is None:
            raise ValueError("indexed GIM has no palette block")
        palette_format = read_u16(data, palette_offset + 4)
        palette_start = read_u32(data, palette_offset + 0x1C)
        palette_end = read_u32(data, palette_offset + 0x20)
        step = 4 if palette_format == 3 else 2
        if palette_end <= palette_start or palette_offset + palette_end > len(data):
            raise ValueError("GIM palette range is invalid")
        entry_count = min(256, (palette_end - palette_start) // step)
        for index in range(entry_count):
            entry = palette_offset + palette_start + index * step
            value = read_u32(data, entry) if step == 4 else read_u16(data, entry)
            palette[index] = decode_pixel(value, palette_format)

    rgba = bytearray(width * height * 4)
    for y in range(height):
        row = linear[y * aligned_pitch : (y + 1) * aligned_pitch]
        for x in range(width):
            if pixel_format == 3:
                color = tuple(row[x * 4 : x * 4 + 4])
            elif pixel_format == 5:
                color = palette[row[x]]
            elif pixel_format == 4:
                palette_index = (
                    row[x // 2] >> 4 if x & 1 else row[x // 2] & 15
                )
                color = palette[palette_index]
            else:
                value = row[x * 2] | row[x * 2 + 1] << 8
                color = decode_pixel(value, pixel_format)
            destination = (y * width + x) * 4
            rgba[destination : destination + 4] = bytes(color)
    return DecodedGim(width, height, pixel_format, order, bytes(rgba))


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF)
    )


def encode_png(width: int, height: int, rgba: bytes) -> bytes:
    expected_size = width * height * 4
    if len(rgba) != expected_size:
        raise ValueError(f"RGBA payload size {len(rgba)} != {expected_size}")
    scanlines = b"".join(
        b"\0" + rgba[y * width * 4 : (y + 1) * width * 4]
        for y in range(height)
    )
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(
            b"IHDR",
            struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0),
        )
        + png_chunk(b"IDAT", zlib.compress(scanlines, 9))
        + png_chunk(b"IEND", b"")
    )


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_uint32(value: str) -> int:
    parsed = int(value, 0)
    if parsed < 0 or parsed > 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("value must fit uint32")
    return parsed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rco", type=Path, required=True)
    parser.add_argument("--texture-id", type=parse_uint32, required=True)
    parser.add_argument("--png-out", type=Path, required=True)
    parser.add_argument("--gim-out", type=Path)
    parser.add_argument("--metadata-out", type=Path)
    parser.add_argument("--source-label")
    arguments = parser.parse_args()

    rco = arguments.rco.read_bytes()
    header = parse_header(rco)
    node, attributes = locate_texture(rco, header, arguments.texture_id)
    source = attributes.get("src")
    original_size = attributes.get("origsize")
    compression = attributes.get("compress")
    texture_type = attributes.get("type")
    if source is None or source.attribute_type != 8:
        raise ValueError("texture source attribute is missing or invalid")
    if original_size is None or original_size.attribute_type != 1:
        raise ValueError("texture original-size attribute is missing or invalid")
    if compression is None or decode_string(rco, header, compression) != "on":
        raise ValueError("only zlib-compressed RCO textures are supported")
    if texture_type is None or decode_string(rco, header, texture_type) != "texture/gim":
        raise ValueError("texture is not a GIM resource")

    source_start = header.filetable_offset + source.first
    source_end = source_start + source.second
    if source_start < header.filetable_offset or source_end > (
        header.filetable_offset + header.filetable_size
    ):
        raise ValueError("texture source is outside the RCO file table")
    expected_size = original_size.first
    if expected_size == 0 or expected_size > MAX_TEXTURE_SIZE:
        raise ValueError(f"texture original size {expected_size} is outside bounds")
    compressed = rco[source_start:source_end]
    decompressor = zlib.decompressobj()
    gim = decompressor.decompress(compressed, expected_size + 1)
    if decompressor.unconsumed_tail:
        raise ValueError("texture expands beyond its declared original size")
    gim += decompressor.flush()
    if (
        not decompressor.eof
        or decompressor.unused_data
    ):
        raise ValueError("texture zlib stream is incomplete or has trailing data")
    if len(gim) != expected_size:
        raise ValueError(
            f"decompressed GIM size {len(gim)} != {expected_size}"
        )
    decoded = decode_gim(gim)
    png = encode_png(decoded.width, decoded.height, decoded.rgba)

    arguments.png_out.parent.mkdir(parents=True, exist_ok=True)
    arguments.png_out.write_bytes(png)
    if arguments.gim_out is not None:
        arguments.gim_out.parent.mkdir(parents=True, exist_ok=True)
        arguments.gim_out.write_bytes(gim)
    if arguments.metadata_out is not None:
        metadata = {
            "schema": 1,
            "source": arguments.source_label or arguments.rco.name,
            "source_rco_sha256": sha256(rco),
            "texture_id": f"0x{arguments.texture_id:08X}",
            "tree_node_offset": f"0x{node.offset:08X}",
            "filetable_offset": f"0x{source.first:08X}",
            "compressed_size": source.second,
            "uncompressed_size": len(gim),
            "compressed_sha256": sha256(compressed),
            "gim_sha256": sha256(gim),
            "png_sha256": sha256(png),
            "width": decoded.width,
            "height": decoded.height,
            "gim_format": decoded.format,
            "gim_order": decoded.order,
        }
        arguments.metadata_out.parent.mkdir(parents=True, exist_ok=True)
        arguments.metadata_out.write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
