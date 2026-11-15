from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import sys
import zlib


ARROW_SHA256 = "6d18fe0829e3767f9ba972cb8774a908e6a0c01526295e11284df4fba6bbfa1d"
MAGNIFIER_SHA256 = "c16942c7a2805306fe84d09621bb4b91f6df7ab027b1ed54a41ad4e7e91b4d95"
SOURCE_RCO_SHA256 = "6ee58e58d1e78fecd99ea93e585c81efde3e940e3e0ca5cec4e64ba7cacdc806"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def paeth(left: int, up: int, upper_left: int) -> int:
    estimate = left + up - upper_left
    left_distance = abs(estimate - left)
    up_distance = abs(estimate - up)
    diagonal_distance = abs(estimate - upper_left)
    if left_distance <= up_distance and left_distance <= diagonal_distance:
        return left
    return up if up_distance <= diagonal_distance else upper_left


def decode_png(data: bytes) -> tuple[int, int, bytes]:
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError("asset is not a PNG")
    offset = 8
    width = height = None
    compressed = bytearray()
    while offset + 12 <= len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + size]
        offset += size + 12
        if chunk_type == b"IHDR":
            width, height, depth, color_type, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", payload)
            )
            if (depth, color_type, compression, filtering, interlace) != (
                8,
                6,
                0,
                0,
                0,
            ):
                raise AssertionError("asset must be non-interlaced 8-bit RGBA PNG")
        elif chunk_type == b"IDAT":
            compressed.extend(payload)
        elif chunk_type == b"IEND":
            break
    if width is None or height is None:
        raise AssertionError("PNG header is missing")

    raw = zlib.decompress(bytes(compressed))
    stride = width * 4
    expected = height * (stride + 1)
    if len(raw) != expected:
        raise AssertionError(f"PNG scanline payload {len(raw)} != {expected}")
    output = bytearray()
    previous = bytearray(stride)
    cursor = 0
    for _ in range(height):
        filter_type = raw[cursor]
        cursor += 1
        row = bytearray(raw[cursor : cursor + stride])
        cursor += stride
        for index in range(stride):
            left = row[index - 4] if index >= 4 else 0
            up = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 1:
                row[index] = (row[index] + left) & 0xFF
            elif filter_type == 2:
                row[index] = (row[index] + up) & 0xFF
            elif filter_type == 3:
                row[index] = (row[index] + (left + up) // 2) & 0xFF
            elif filter_type == 4:
                row[index] = (
                    row[index] + paeth(left, up, upper_left)
                ) & 0xFF
            elif filter_type != 0:
                raise AssertionError(f"unsupported PNG filter {filter_type}")
        output.extend(row)
        previous = row
    return width, height, bytes(output)


def visible_pixels(
    width: int,
    height: int,
    rgba: bytes,
) -> list[tuple[int, int, tuple[int, int, int, int]]]:
    pixels = []
    for y in range(height):
        for x in range(width):
            offset = (y * width + x) * 4
            color = tuple(rgba[offset : offset + 4])
            if color[3] != 0:
                pixels.append((x, y, color))
    return pixels


def bounding_box(
    pixels: list[tuple[int, int, tuple[int, int, int, int]]],
) -> tuple[int, int, int, int]:
    return (
        min(item[0] for item in pixels),
        min(item[1] for item in pixels),
        max(item[0] for item in pixels),
        max(item[1] for item in pixels),
    )


def resize_premultiplied_bilinear(
    width: int,
    height: int,
    rgba: bytes,
    output_width: int,
    output_height: int,
) -> bytes:
    pixels = [tuple(rgba[index : index + 4]) for index in range(0, len(rgba), 4)]
    output = bytearray(output_width * output_height * 4)
    for output_y in range(output_height):
        source_y = (output_y + 0.5) * height / output_height - 0.5
        y0 = max(0, min(height - 1, math.floor(source_y)))
        y1 = max(0, min(height - 1, y0 + 1))
        fraction_y = source_y - math.floor(source_y)
        for output_x in range(output_width):
            source_x = (output_x + 0.5) * width / output_width - 0.5
            x0 = max(0, min(width - 1, math.floor(source_x)))
            x1 = max(0, min(width - 1, x0 + 1))
            fraction_x = source_x - math.floor(source_x)
            samples = (
                ((1.0 - fraction_x) * (1.0 - fraction_y), pixels[y0 * width + x0]),
                (fraction_x * (1.0 - fraction_y), pixels[y0 * width + x1]),
                ((1.0 - fraction_x) * fraction_y, pixels[y1 * width + x0]),
                (fraction_x * fraction_y, pixels[y1 * width + x1]),
            )
            alpha = sum(weight * color[3] for weight, color in samples)
            destination = (output_y * output_width + output_x) * 4
            output[destination + 3] = round(alpha)
            if alpha == 0.0:
                continue
            for channel in range(3):
                output[destination + channel] = round(
                    sum(
                        weight * color[channel] * color[3]
                        for weight, color in samples
                    )
                    / alpha
                )
    return bytes(output)


def load_generator(root: Path):
    path = root / "tools/assets/generate_skeuomorphic_magnifier.py"
    spec = importlib.util.spec_from_file_location("magnifier_generator", path)
    if spec is None or spec.loader is None:
        raise AssertionError("unable to load magnifier generator")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def verify_arrow(root: Path) -> None:
    arrow_path = root / "docs/assets/sony-common-corner-back-arrow.png"
    metadata_path = root / "docs/assets/sony-common-corner-back-arrow.json"
    arrow = arrow_path.read_bytes()
    if sha256(arrow) != ARROW_SHA256:
        raise AssertionError("stock Back arrow bytes changed")

    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    expected_metadata = {
        "schema": 1,
        "source_rco_sha256": SOURCE_RCO_SHA256,
        "texture_id": "0x352FFDE1",
        "tree_node_offset": "0x00009ED8",
        "filetable_offset": "0x00035270",
        "compressed_size": 1514,
        "uncompressed_size": 5328,
        "compressed_sha256": "c3dc3cd62a2f6145f0622c123c6cb9b340658a282238de00101f3ab49610981b",
        "gim_sha256": "a007d2290f159db87ccd49ac689053e3b57c927ff323de7275676c69a074c2ba",
        "png_sha256": ARROW_SHA256,
        "width": 64,
        "height": 64,
        "gim_format": 5,
        "gim_order": 0,
    }
    for key, expected in expected_metadata.items():
        if metadata.get(key) != expected:
            raise AssertionError(
                f"arrow metadata {key!r}: {metadata.get(key)!r} != {expected!r}"
            )

    width, height, rgba = decode_png(arrow)
    pixels = visible_pixels(width, height, rgba)
    if (width, height) != (64, 64):
        raise AssertionError("stock Back arrow must remain 64x64")
    if bounding_box(pixels) != (14, 17, 56, 55):
        raise AssertionError("stock Back arrow visible bounds changed")
    if len(pixels) != 907:
        raise AssertionError("stock Back arrow alpha population changed")
    if sum(color[3] == 255 for _, _, color in pixels) != 49:
        raise AssertionError("stock Back arrow opaque highlight population changed")
    if (min(color[3] for _, _, color in pixels), max(color[3] for _, _, color in pixels)) != (
        6,
        255,
    ):
        raise AssertionError("stock Back arrow alpha range changed")
    if not all(color[0] == color[1] == color[2] for _, _, color in pixels):
        raise AssertionError("stock Back arrow must remain grayscale")


def verify_magnifier(root: Path) -> None:
    generator = load_generator(root)
    expected_geometry = {
        "cx": 26.7,
        "cy": 25.8,
        "outer": 15.3,
        "inner": 10.0,
        "hx0": 37.2,
        "hy0": 36.3,
        "hx1": 49.0,
        "hy1": 48.1,
        "handle": 3.8,
    }
    for key, expected in expected_geometry.items():
        if generator.PARAMETERS.get(key) != expected:
            raise AssertionError(
                f"magnifier geometry {key!r}: "
                f"{generator.PARAMETERS.get(key)!r} != {expected!r}"
            )

    asset_path = root / "resources/texture/search.png"
    preview_path = root / "docs/assets/livearea-search-magnifier.png"
    asset = asset_path.read_bytes()
    preview = preview_path.read_bytes()
    if asset != preview:
        raise AssertionError("review magnifier must be byte-identical to the runtime asset")
    if sha256(asset) != MAGNIFIER_SHA256:
        raise AssertionError("skeuomorphic magnifier bytes changed")

    generated = generator.generate_png()
    if generated != asset:
        raise AssertionError("magnifier generator no longer reproduces the runtime asset")

    width, height, rgba = decode_png(asset)
    pixels = visible_pixels(width, height, rgba)
    if (width, height) != (64, 64):
        raise AssertionError("magnifier must remain 64x64")
    if bounding_box(pixels) != (10, 9, 55, 55):
        raise AssertionError("magnifier optical bounds changed")
    if len(pixels) != 1061:
        raise AssertionError("magnifier alpha population changed")
    if (min(color[3] for _, _, color in pixels), max(color[3] for _, _, color in pixels)) != (
        1,
        255,
    ):
        raise AssertionError("magnifier alpha range changed")
    if not all(color[0] == color[1] == color[2] for _, _, color in pixels):
        raise AssertionError("magnifier must remain grayscale")
    dark_edge = [color for _, _, color in pixels if max(color[:3]) <= 64]
    if len(dark_edge) < 300 or max(color[3] for color in dark_edge) > 32:
        raise AssertionError("magnifier lost its low-alpha dark edge/shadow")
    if sum(min(color[:3]) >= 220 and color[3] >= 180 for _, _, color in pixels) < 500:
        raise AssertionError("magnifier lost its bright beveled body")
    if sum(color[3] >= 240 for _, _, color in pixels) != 112:
        raise AssertionError("magnifier highlight population changed")
    center = tuple(rgba[(25 * width + 26) * 4 : (25 * width + 26) * 4 + 4])
    if center != (0, 0, 0, 0):
        raise AssertionError("magnifier negative space must remain transparent")
    if tuple(rgba[:4]) != (0, 0, 0, 0):
        raise AssertionError("magnifier canvas corners must remain transparent")

    rendered = resize_premultiplied_bilinear(width, height, rgba, 60, 60)
    rendered_pixels = visible_pixels(60, 60, rendered)
    if bounding_box(rendered_pixels) != (9, 8, 52, 52):
        raise AssertionError("rendered magnifier optical bounds changed")
    if len(rendered_pixels) != 991:
        raise AssertionError("rendered magnifier alpha population changed")
    rendered_alpha = [color[3] for _, _, color in rendered_pixels]
    if (min(rendered_alpha), max(rendered_alpha)) != (1, 255):
        raise AssertionError("rendered magnifier alpha range changed")
    rendered_edge = [
        color for _, _, color in rendered_pixels if max(color[:3]) <= 64
    ]
    if len(rendered_edge) != 325 or max(color[3] for color in rendered_edge) > 32:
        raise AssertionError("rendered magnifier edge treatment changed")
    if sum(color[3] >= 240 for _, _, color in rendered_pixels) != 79:
        raise AssertionError("rendered magnifier highlight population changed")


def verify_integration(root: Path) -> None:
    resource = (root / "resources/livearea_search_plugin.xml").read_text(
        encoding="utf-8"
    )
    required = (
        'color="0.21,0.43,0.73,0.9" '
        'texture0="_common_texture_corner_button_base" '
        'id="home_search_bg_obj"',
        'size="60,60,0" texture0="texture_home_search" id="home_search_corner_obj" pos="13,-13,0"',
        '<texture src="texture/search.png" compress="on" '
        'id="texture_home_search" type="texture/png" />',
    )
    for marker in required:
        if marker not in resource:
            raise AssertionError(f"missing magnifier integration contract: {marker}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    verify_arrow(root)
    verify_magnifier(root)
    verify_integration(root)
    print("home icon asset contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
