from __future__ import annotations

import argparse
import binascii
import math
import struct
import zlib
from pathlib import Path

SIZE = 64
SCALE = 8
CONTENT_SIZE = 60
CONTENT_OFFSET_X = 2
CONTENT_OFFSET_Y = 4
PARAMETERS = {
    "cx": 26.7,
    "cy": 25.8,
    "outer": 15.3,
    "inner": 10.0,
    "hx0": 37.2,
    "hy0": 36.3,
    "hx1": 49.0,
    "hy1": 48.1,
    "handle": 3.8,
    "shadow_x": 1.0,
    "shadow_y": 1.4,
    "shadow_blur": 2.3,
    "shadow_alpha": 25.0,
    "body_alpha": 190.0,
    "directional_alpha": 60.0,
    "highlight_alpha": 22.0,
    "shade_alpha": 5.0,
    "specular_alpha": 45.0,
    "edge_gray": 3.0,
}


def clamp(value: float, lower: float, upper: float) -> float:
    return lower if value < lower else upper if value > upper else value


def capsule_sdf(
    x: float,
    y: float,
    ax: float,
    ay: float,
    bx: float,
    by: float,
    radius: float,
) -> float:
    vx = bx - ax
    vy = by - ay
    length_squared = vx * vx + vy * vy
    t = clamp(((x - ax) * vx + (y - ay) * vy) / length_squared, 0.0, 1.0)
    nearest_x = ax + t * vx
    nearest_y = ay + t * vy
    return math.hypot(x - nearest_x, y - nearest_y) - radius


def ring_sdf(
    x: float,
    y: float,
    cx: float,
    cy: float,
    outer: float,
    inner: float,
) -> float:
    distance = math.hypot(x - cx, y - cy)
    return max(distance - outer, inner - distance)


def body_sdf(x: float, y: float) -> float:
    ring = ring_sdf(
        x,
        y,
        PARAMETERS["cx"],
        PARAMETERS["cy"],
        PARAMETERS["outer"],
        PARAMETERS["inner"],
    )
    handle = capsule_sdf(
        x,
        y,
        PARAMETERS["hx0"],
        PARAMETERS["hy0"],
        PARAMETERS["hx1"],
        PARAMETERS["hy1"],
        PARAMETERS["handle"],
    )
    return min(ring, handle)


def body_normal(x: float, y: float) -> tuple[float, float]:
    epsilon = 0.12
    gradient_x = body_sdf(x + epsilon, y) - body_sdf(x - epsilon, y)
    gradient_y = body_sdf(x, y + epsilon) - body_sdf(x, y - epsilon)
    length = math.hypot(gradient_x, gradient_y)
    if length == 0.0:
        return 0.0, 0.0
    return gradient_x / length, gradient_y / length


def generate_source_rgba() -> bytes:
    high_size = SIZE * SCALE
    accumulated = [0.0] * (SIZE * SIZE * 4)
    light_x = -math.sqrt(0.5)
    light_y = -math.sqrt(0.5)

    for sample_y in range(high_size):
        y = (sample_y + 0.5) / SCALE
        for sample_x in range(high_size):
            x = (sample_x + 0.5) / SCALE
            signed_distance = body_sdf(x, y)
            red = green = blue = 0.0
            alpha = 0.0

            shadow_distance = body_sdf(
                x - PARAMETERS["shadow_x"],
                y - PARAMETERS["shadow_y"],
            )
            if shadow_distance < PARAMETERS["shadow_blur"]:
                alpha = PARAMETERS["shadow_alpha"] * clamp(
                    1.0
                    - max(shadow_distance, 0.0) / PARAMETERS["shadow_blur"],
                    0.0,
                    1.0,
                )

            if signed_distance < 0.0:
                normal_x, normal_y = body_normal(x, y)
                light = normal_x * light_x + normal_y * light_y
                edge = math.exp(signed_distance / 1.15)
                specular = math.exp(
                    -(
                        ((x - (PARAMETERS["cx"] - 5.2)) / 5.2) ** 2
                        + ((y - (PARAMETERS["cy"] - 5.0)) / 2.2) ** 2
                    )
                )
                directional = clamp(
                    0.5
                    + (PARAMETERS["cx"] - x)
                    / (2.0 * PARAMETERS["outer"])
                    + (PARAMETERS["cy"] - y)
                    / (2.0 * PARAMETERS["outer"]),
                    0.0,
                    1.0,
                )
                alpha = clamp(
                    PARAMETERS["body_alpha"]
                    + PARAMETERS["directional_alpha"] * directional
                    + PARAMETERS["highlight_alpha"]
                    * max(light, 0.0)
                    * edge
                    - PARAMETERS["shade_alpha"]
                    * max(-light, 0.0)
                    * edge
                    + PARAMETERS["specular_alpha"] * specular,
                    0.0,
                    255.0,
                )
                luminance = 255.0
                luminance -= (
                    PARAMETERS["edge_gray"]
                    * max(-light, 0.0)
                    * edge
                )
                red = green = blue = clamp(luminance, 236.0, 255.0)

            pixel_index = (
                (sample_y // SCALE) * SIZE + sample_x // SCALE
            ) * 4
            accumulated[pixel_index] += red * alpha / 255.0
            accumulated[pixel_index + 1] += green * alpha / 255.0
            accumulated[pixel_index + 2] += blue * alpha / 255.0
            accumulated[pixel_index + 3] += alpha

    sample_count = SCALE * SCALE
    output = bytearray(SIZE * SIZE * 4)
    for index in range(0, len(output), 4):
        alpha = accumulated[index + 3] / sample_count
        output[index + 3] = round(clamp(alpha, 0.0, 255.0))
        if alpha == 0.0:
            continue
        output[index] = round(
            clamp(accumulated[index] / sample_count * 255.0 / alpha, 0.0, 255.0)
        )
        output[index + 1] = round(
            clamp(
                accumulated[index + 1] / sample_count * 255.0 / alpha,
                0.0,
                255.0,
            )
        )
        output[index + 2] = round(
            clamp(
                accumulated[index + 2] / sample_count * 255.0 / alpha,
                0.0,
                255.0,
            )
        )
    return bytes(output)


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


def generate_rgba() -> bytes:
    content = resize_premultiplied_bilinear(
        SIZE,
        SIZE,
        generate_source_rgba(),
        CONTENT_SIZE,
        CONTENT_SIZE,
    )
    output = bytearray(SIZE * SIZE * 4)
    for y in range(CONTENT_SIZE):
        source = y * CONTENT_SIZE * 4
        destination = (
            (y + CONTENT_OFFSET_Y) * SIZE + CONTENT_OFFSET_X
        ) * 4
        output[destination : destination + CONTENT_SIZE * 4] = content[
            source : source + CONTENT_SIZE * 4
        ]
    return bytes(output)


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


def generate_png() -> bytes:
    return encode_png(SIZE, SIZE, generate_rgba())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(generate_png())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
