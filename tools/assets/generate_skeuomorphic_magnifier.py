from __future__ import annotations

import argparse
import binascii
import math
import struct
import zlib
from pathlib import Path

SIZE = 64
SCALE = 8
PARAMETERS = {
    "cx": 26.5,
    "cy": 25.5,
    "outer": 14.6,
    "inner": 8.6,
    "hx0": 36.0,
    "hy0": 35.0,
    "hx1": 51.5,
    "hy1": 50.5,
    "handle": 4.7,
    "shadow_x": 1.4,
    "shadow_y": 2.0,
    "shadow_blur": 2.2,
    "shadow_alpha": 72.0,
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


def generate_rgba() -> bytes:
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

            lens_distance = math.hypot(
                x - PARAMETERS["cx"],
                y - PARAMETERS["cy"],
            )
            if lens_distance < PARAMETERS["inner"] - 0.55:
                glass_alpha = 14.0 + 20.0 * clamp(
                    (
                        PARAMETERS["cy"]
                        + PARAMETERS["inner"]
                        - y
                    )
                    / (2.0 * PARAMETERS["inner"]),
                    0.0,
                    1.0,
                )
                glint = math.exp(
                    -(
                        ((x - (PARAMETERS["cx"] - 4.4)) / 5.8) ** 2
                        + ((y - (PARAMETERS["cy"] - 4.8)) / 2.0) ** 2
                    )
                )
                glass_alpha += 35.0 * glint
                if glass_alpha > alpha:
                    red = green = blue = 255.0
                    alpha = glass_alpha

            if signed_distance < 0.0:
                normal_x, normal_y = body_normal(x, y)
                light = normal_x * light_x + normal_y * light_y
                edge = math.exp(signed_distance / 1.25)
                diagonal = clamp((x + y - 10.0) / 95.0, 0.0, 1.0)
                luminance = 254.0 - 24.0 * diagonal
                luminance += edge * (
                    28.0 * max(light, 0.0)
                    - 34.0 * max(-light, 0.0)
                )
                luminance += 7.0 * math.exp(
                    -((x - 18.0) ** 2 + (y - 16.0) ** 2) / 180.0
                )
                red = green = blue = clamp(luminance, 150.0, 255.0)
                alpha = clamp(
                    244.0
                    + 18.0 * clamp(max(light, 0.0) * edge, 0.0, 1.0),
                    0.0,
                    255.0,
                )

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
