#!/usr/bin/env python3
"""
Convert static wall texture atlas to C header.
Input: single 512x512 PNG (e.g. spr_wall_texture_map_1a_0.png).
Layout: 128x128 tiles, 4+4+4+1 = 13 tiles. Output 64x64 tiles (downsampled) to fit ROM.
Output: wall_texture.h with wall_textures[NUM_LEVELS][64*64] (RGB565).
"""
from PIL import Image
import os

# Output 64x64 tiles (downsampled from 128x128) to fit ROM
TEX_SIZE = 64
SRC_TILE = 128  # source tiles in 512x512 are 128x128
# Tile coords in 512x512: (tx, ty) in 128-units. Layout: 4+4+4+1 = 13 tiles
TILES_COORDS = [
    (0, 0), (1, 0), (2, 0), (3, 0),
    (0, 1), (1, 1), (2, 1), (3, 1),
    (0, 2), (1, 2), (2, 2), (3, 2),
    (0, 3),
]


def to_rgb565_gint(r, g, b):
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g5 << 6) | b5


def convert_static_wall(image_path, output_header):
    """Read single 512x512 PNG, extract 13 tiles at 64x64, write C header (no animation)."""
    path = os.path.abspath(image_path)
    if not os.path.isfile(path):
        raise FileNotFoundError(f"File not found: {path}")

    img = Image.open(path).convert("RGB")
    w, h = img.size
    if w != 512 or h != 512:
        raise ValueError(f"Expected 512x512, got {w}x{h}: {path}")

    num_levels = len(TILES_COORDS)

    with open(output_header, "w") as f:
        f.write("#ifndef WALL_TEXTURE_H\n#define WALL_TEXTURE_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define TEX_WIDTH {TEX_SIZE}\n")
        f.write(f"#define TEX_HEIGHT {TEX_SIZE}\n")
        f.write(f"#define NUM_LEVELS {num_levels}\n\n")
        f.write(f"static const uint16_t wall_textures[{num_levels}][{TEX_SIZE * TEX_SIZE}] = {{\n")

        for tx, ty in TILES_COORDS:
            f.write("  {\n")
            for dy in range(TEX_SIZE):
                for dx in range(TEX_SIZE):
                    x = tx * SRC_TILE + dx * 2
                    y = ty * SRC_TILE + dy * 2
                    r, g, b = img.getpixel((x, y))
                    f.write(f"{to_rgb565_gint(r, g, b)}, ")
                f.write("\n")
            f.write("  },\n")

        f.write("};\n\n#endif\n")

    print(f"Converted static wall: 1 image, {num_levels} levels (64x64) -> {output_header}")


if __name__ == "__main__":
    import sys
    image = sys.argv[1] if len(sys.argv) > 1 else "scripts/spr_wall_texture_map_1a/spr_wall_texture_map_1a_0.png"
    out = sys.argv[2] if len(sys.argv) > 2 else "src/assets/wall_texture.h"
    convert_static_wall(image, out)
