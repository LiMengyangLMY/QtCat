#!/usr/bin/env python3

from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path

from PIL import Image


CORE_THRESHOLD = 16
SOFT_THRESHOLD = 72
TARGET_SIZE = 256


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Remove edge-connected black background while preserving dark interior details."
    )
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--size", type=int, default=TARGET_SIZE)
    return parser.parse_args()


def is_core_background(pixel: tuple[int, int, int]) -> bool:
    return max(pixel) <= CORE_THRESHOLD


def soft_alpha(pixel: tuple[int, int, int]) -> int:
    value = max(pixel)
    if value <= CORE_THRESHOLD:
        return 0
    if value >= SOFT_THRESHOLD:
        return 255
    return int((value - CORE_THRESHOLD) * 255 / (SOFT_THRESHOLD - CORE_THRESHOLD))


def mark_edge_connected_background(image: Image.Image) -> list[list[bool]]:
    width, height = image.size
    pixels = image.load()
    background = [[False for _ in range(width)] for _ in range(height)]
    queue: deque[tuple[int, int]] = deque()

    def maybe_push(x: int, y: int) -> None:
        if background[y][x]:
            return
        if is_core_background(pixels[x, y]):
            background[y][x] = True
            queue.append((x, y))

    for x in range(width):
        maybe_push(x, 0)
        maybe_push(x, height - 1)
    for y in range(height):
        maybe_push(0, y)
        maybe_push(width - 1, y)

    while queue:
        x, y = queue.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx = x + dx
            ny = y + dy
            if nx < 0 or ny < 0 or nx >= width or ny >= height:
                continue
            maybe_push(nx, ny)

    return background


def build_alpha(image: Image.Image, background: list[list[bool]]) -> list[list[int]]:
    width, height = image.size
    pixels = image.load()
    alpha = [[255 for _ in range(width)] for _ in range(height)]

    for y in range(height):
        for x in range(width):
            if background[y][x]:
                alpha[y][x] = 0

    for y in range(height):
        for x in range(width):
            if background[y][x]:
                continue
            has_background_neighbor = False
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx = x + dx
                ny = y + dy
                if 0 <= nx < width and 0 <= ny < height and background[ny][nx]:
                    has_background_neighbor = True
                    break
            if has_background_neighbor:
                alpha[y][x] = min(alpha[y][x], soft_alpha(pixels[x, y]))

    return alpha


def fill_internal_holes(alpha: list[list[int]]) -> None:
    height = len(alpha)
    width = len(alpha[0]) if height else 0
    visited = [[False for _ in range(width)] for _ in range(height)]

    for start_y in range(height):
        for start_x in range(width):
            if visited[start_y][start_x] or alpha[start_y][start_x] != 0:
                continue

            queue: deque[tuple[int, int]] = deque([(start_x, start_y)])
            visited[start_y][start_x] = True
            region: list[tuple[int, int]] = []
            touches_edge = False

            while queue:
                x, y = queue.popleft()
                region.append((x, y))
                if x == 0 or y == 0 or x == width - 1 or y == height - 1:
                    touches_edge = True

                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx = x + dx
                    ny = y + dy
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    if visited[ny][nx] or alpha[ny][nx] != 0:
                        continue
                    visited[ny][nx] = True
                    queue.append((nx, ny))

            if not touches_edge:
                for x, y in region:
                    alpha[y][x] = 255


def apply_alpha(image: Image.Image, alpha: list[list[int]]) -> Image.Image:
    rgba = image.convert("RGBA")
    rgba_pixels = rgba.load()
    width, height = rgba.size
    for y in range(height):
        for x in range(width):
            red, green, blue, _ = rgba_pixels[x, y]
            rgba_pixels[x, y] = (red, green, blue, alpha[y][x])
    return rgba


def fit_to_canvas(image: Image.Image, size: int) -> Image.Image:
    scale = min(size / image.width, size / image.height)
    resized = image.resize(
        (max(1, round(image.width * scale)), max(1, round(image.height * scale))),
        Image.LANCZOS,
    )
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    offset = ((size - resized.width) // 2, (size - resized.height) // 2)
    canvas.paste(resized, offset, resized)
    return canvas


def main() -> None:
    args = parse_args()
    image = Image.open(args.input).convert("RGB")
    background = mark_edge_connected_background(image)
    alpha = build_alpha(image, background)
    fill_internal_holes(alpha)
    rgba = apply_alpha(image, alpha)
    output = fit_to_canvas(rgba, args.size)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    output.save(args.output)


if __name__ == "__main__":
    main()
