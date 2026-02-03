from PIL import Image
import struct
import os
import re

def to_rgb565_gint(r, g, b):
    # gint's C_RGB(r, g, b) uses 5 bits for R, 5 bits for G, 5 bits for B (usually)
    # Based on display-cg.h: #define C_RGB(r,g,b) (((r) << 11) | ((g) << 6) | (b)) where channels are 0..31
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g5 << 6) | b5

def convert(image_path, output_header):
    img = Image.open(image_path).convert("RGBA")
    width, height = img.size
    
    pixels = []
    for y in range(height):
        for x in range(width):
            r, g, b, a = img.getpixel((x, y))
            if a < 128: pixels.append(-1) # transparent
            else: pixels.append(to_rgb565_gint(r, g, b))
    
    with open(output_header, "w") as f:
        f.write("#ifndef ENEMY_SPRITE_H\n#define ENEMY_SPRITE_H\n\n")
        f.write(f"#define ENEMY_SPR_WIDTH {width}\n")
        f.write(f"#define ENEMY_SPR_HEIGHT {height}\n\n")
        f.write("static const int enemy_sprite[] = {\n")
        for i, p in enumerate(pixels):
            f.write(f"{p}, ")
            if (i + 1) % width == 0:
                f.write("\n")
        f.write("};\n\n#endif\n")

def convert_enemy(image_path, output_header):
    img = Image.open(image_path).convert("RGBA")
    width, height = img.size
    pixels = []
    for y in range(height):
        for x in range(width):
            r, g, b, a = img.getpixel((x, y))
            if a < 128: pixels.append(-1)
            else: pixels.append(to_rgb565_gint(r, g, b))
    with open(output_header, "w") as f:
        f.write("#ifndef ENEMY_SPRITE_H\n#define ENEMY_SPRITE_H\n\n")
        f.write(f"#define ENEMY_SPR_WIDTH {width}\n")
        f.write(f"#define ENEMY_SPR_HEIGHT {height}\n\n")
        f.write("static const int enemy_sprite[] = {\n")
        for i, p in enumerate(pixels):
            f.write(f"{p}, ")
            if (i + 1) % width == 0: f.write("\n")
        f.write("};\n\n#endif\n")

def convert_wall(image_path, output_header):
    img = Image.open(image_path).convert("RGB")
    tex_size = 128
    
    # tiles for levels 1-7
    # Level 1: (0,0), Level 2: (1,0), Level 3: (0,1), Level 4: (3,2), 
    # Level 5: (3,0), Level 6: (0,2), Level 7: (0,3)
    tiles_coords = [
        (0, 0), (1, 0), (0, 1), (3, 2), (3, 0), (0, 2), (0, 3)
    ]
    
    with open(output_header, "w") as f:
        f.write("#ifndef WALL_TEXTURE_H\n#define WALL_TEXTURE_H\n\n")
        f.write(f"#define TEX_WIDTH {tex_size}\n")
        f.write(f"#define TEX_HEIGHT {tex_size}\n\n")
        f.write("static const uint16_t wall_textures[7][128 * 128] = {\n")
        
        for tx, ty in tiles_coords:
            f.write("{\n")
            for y in range(ty * tex_size, (ty + 1) * tex_size):
                for x in range(tx * tex_size, (tx + 1) * tex_size):
                    r, g, b = img.getpixel((x, y))
                    f.write(f"{to_rgb565_gint(r, g, b)}, ")
                f.write("\n")
            f.write("},\n")
            
        f.write("};\n\n#endif\n")

# transparent key
SHOOT_EFFECT_TRANSPARENT = 0xF81F

def convert_sprite_strip(folder_path, output_header):
    # convert sprite strip
    folder = os.path.abspath(folder_path)
    if not os.path.isdir(folder):
        raise FileNotFoundError(f"Folder not found: {folder}")
    frame_files = []
    for name in os.listdir(folder):
        m = re.match(r"(.+)_(\d+)\.png$", name, re.IGNORECASE)
        if m:
            frame_files.append((int(m.group(2)), os.path.join(folder, name)))
    frame_files.sort(key=lambda x: x[0])
    if not frame_files:
        raise FileNotFoundError(f"No *_N.png files in {folder}")
    num_frames = len(frame_files)
    first = Image.open(frame_files[0][1]).convert("RGBA")
    sw, sh = first.size
    # half resolution
    width, height = sw // 2, sh // 2
    with open(output_header, "w") as f:
        f.write("#ifndef SHOOT_EFFECT_H\n#define SHOOT_EFFECT_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define SHOOT_EFFECT_FRAMES {num_frames}\n")
        f.write(f"#define SHOOT_EFFECT_WIDTH {width}\n")
        f.write(f"#define SHOOT_EFFECT_HEIGHT {height}\n")
        f.write(f"#define SHOOT_EFFECT_TRANSPARENT 0xF81F\n\n")
        f.write(f"static const uint16_t shoot_effect_frames[{num_frames}][{width} * {height}] = {{\n")
        for _, path in frame_files:
            img = Image.open(path).convert("RGBA")
            if img.size != (sw, sh):
                raise ValueError(f"Frame size mismatch: {path}")
            f.write("{\n")
            for dy in range(height):
                for dx in range(width):
                    x, y = dx * 2, dy * 2
                    r, g, b, a = img.getpixel((x, y))
                    p = SHOOT_EFFECT_TRANSPARENT if a < 128 else to_rgb565_gint(r, g, b)
                    f.write(f"{p}, ")
                f.write("\n")
            f.write("},\n")
        f.write("};\n\n#endif\n")
    print(f"Converted shoot effect: {num_frames} frames {width}x{height} (half-res, uint16_t) -> {output_header}")

def convert_sprite_strip_named(folder_path, output_header, prefix):
    # convert named sprite strip
    folder = os.path.abspath(folder_path)
    if not os.path.isdir(folder):
        raise FileNotFoundError(f"Folder not found: {folder}")
    frame_files = []
    for name in os.listdir(folder):
        m = re.match(r"(.+)_(\d+)\.png$", name, re.IGNORECASE)
        if m:
            frame_files.append((int(m.group(2)), os.path.join(folder, name)))
    frame_files.sort(key=lambda x: x[0])
    if not frame_files:
        raise FileNotFoundError(f"No *_N.png files in {folder}")
    num_frames = len(frame_files)
    first = Image.open(frame_files[0][1]).convert("RGBA")
    sw, sh = first.size
    width, height = sw // 2, sh // 2
    guard = prefix + "_H"
    array_name = prefix.lower() + "_frames"
    with open(output_header, "w") as f:
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define {prefix}_FRAMES {num_frames}\n")
        f.write(f"#define {prefix}_WIDTH {width}\n")
        f.write(f"#define {prefix}_HEIGHT {height}\n")
        f.write(f"#define {prefix}_TRANSPARENT 0xF81F\n\n")
        f.write(f"static const uint16_t {array_name}[{num_frames}][{width} * {height}] = {{\n")
        for _, path in frame_files:
            img = Image.open(path).convert("RGBA")
            if img.size != (sw, sh):
                raise ValueError(f"Frame size mismatch: {path}")
            f.write("{\n")
            for dy in range(height):
                for dx in range(width):
                    x, y = dx * 2, dy * 2
                    r, g, b, a = img.getpixel((x, y))
                    p = 0xF81F if a < 128 else to_rgb565_gint(r, g, b)
                    f.write(f"{p}, ")
                f.write("\n")
            f.write("},\n")
        f.write("};\n\n#endif\n")
    print(f"Converted {prefix}: {num_frames} frames {width}x{height} -> {output_header}")

def _bbox_non_transparent(img, alpha_thresh=128):
    # bounding box
    w, h = img.size
    min_x, min_y = w, h
    max_x, max_y = -1, -1
    for y in range(h):
        for x in range(w):
            if img.getpixel((x, y))[3] >= alpha_thresh:
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    if max_x < 0:
        return None
    return (min_x, min_y, max_x, max_y)

def convert_sprite_strip_cropped(folder_path, output_header, prefix, scale=2):
    # convert cropped sprite strip
    TRANSPARENT = 0xF81F
    folder = os.path.abspath(folder_path)
    if not os.path.isdir(folder):
        raise FileNotFoundError(f"Folder not found: {folder}")
    frame_files = []
    for name in os.listdir(folder):
        m = re.match(r"(.+)_(\d+)\.png$", name, re.IGNORECASE)
        if m:
            frame_files.append((int(m.group(2)), os.path.join(folder, name)))
    frame_files.sort(key=lambda x: x[0])
    if not frame_files:
        raise FileNotFoundError(f"No *_N.png files in {folder}")
    num_frames = len(frame_files)
    first = Image.open(frame_files[0][1]).convert("RGBA")
    sw, sh = first.size
    logical_w = sw // scale
    logical_h = sh // scale

    frame_infos = []  # (x, y, w, h, offset)
    packed = []

    for _, path in frame_files:
        img = Image.open(path).convert("RGBA")
        if img.size != (sw, sh):
            raise ValueError(f"Frame size mismatch: {path}")
        bbox = _bbox_non_transparent(img)
        if bbox is None:
            frame_infos.append((0, 0, 1, 1, len(packed)))
            packed.append(TRANSPARENT)
            continue
        min_x, min_y, max_x, max_y = bbox
        # crop region
        x_hr = min_x // scale
        y_hr = min_y // scale
        w_hr = (max_x - min_x + scale) // scale
        h_hr = (max_y - min_y + scale) // scale
        if w_hr < 1:
            w_hr = 1
        if h_hr < 1:
            h_hr = 1
        offset = len(packed)
        frame_infos.append((x_hr, y_hr, w_hr, h_hr, offset))
        for dy in range(h_hr):
            for dx in range(w_hr):
                sx = min_x + dx * scale
                sy = min_y + dy * scale
                if sx >= sw:
                    sx = sw - 1
                if sy >= sh:
                    sy = sh - 1
                r, g, b, a = img.getpixel((sx, sy))
                p = TRANSPARENT if a < 128 else to_rgb565_gint(r, g, b)
                packed.append(p)
    total_pixels = len(packed)

    guard = prefix + "_H"
    with open(output_header, "w") as f:
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define {prefix}_FRAMES {num_frames}\n")
        f.write(f"#define {prefix}_WIDTH {logical_w}\n")
        f.write(f"#define {prefix}_HEIGHT {logical_h}\n")
        f.write(f"#define {prefix}_TRANSPARENT 0xF81F\n\n")
        f.write("typedef struct { int x, y, w, h; int offset; } " + prefix.lower() + "_frame_t;\n\n")
        info_name = prefix.lower() + "_frame_info"
        f.write(f"static const {prefix.lower()}_frame_t {info_name}[{num_frames}] = {{\n")
        for (x, y, w, h, off) in frame_infos:
            f.write(f"  {{ {x}, {y}, {w}, {h}, {off} }},\n")
        f.write("};\n\n")
        pixels_name = prefix.lower() + "_pixels"
        f.write(f"static const uint16_t {pixels_name}[{total_pixels}] = {{\n")
        for i in range(0, total_pixels, 16):
            chunk = packed[i:i + 16]
            f.write("  " + ", ".join(str(p) for p in chunk) + ",\n")
        f.write("};\n\n#endif\n")
    print(f"Converted {prefix} (cropped): {num_frames} frames, {total_pixels} pixels (was {num_frames * logical_w * logical_h}) -> {output_header}")

def convert_screen(image_path, output_header, var_name):
    img = Image.open(image_path).convert("RGB")
    width, height = img.size
    pixels = []
    for y in range(height):
        for x in range(width):
            r, g, b = img.getpixel((x, y))
            pixels.append(to_rgb565_gint(r, g, b))
    
    with open(output_header, "w") as f:
        f.write(f"#ifndef {var_name.upper()}_H\n#define {var_name.upper()}_H\n\n")
        f.write(f"static const uint16_t {var_name}[] = {{\n")
        for i, p in enumerate(pixels):
            f.write(f"{p}, ")
            if (i + 1) % width == 0: f.write("\n")
        f.write("};\n\n#endif\n")

if __name__ == "__main__":
    convert_enemy("scripts/enemy_texture.png", "src/assets/enemy_sprite.h")
    convert_wall("scripts/spr_wall_texture_map_1a_0.png", "src/assets/wall_texture.h")
    convert_sprite_strip_cropped("scripts/spr_player_gun_shoot_effect", "src/assets/shoot_effect.h", "SHOOT_EFFECT", scale=2)
    convert_sprite_strip_cropped("scripts/spr_player_gun_idle", "src/assets/gun_idle.h", "GUN_IDLE", scale=2)
    convert_sprite_strip_cropped("scripts/spr_gun_shoot", "src/assets/gun_shoot.h", "GUN_SHOOT", scale=2)
    convert_sprite_strip_cropped("scripts/spr_enemy_melee_walk", "src/assets/enemy_melee_walk.h", "ENEMY_MELEE_WALK", scale=1)
    convert_sprite_strip_cropped("scripts/spr_enemy_melee_attack_loop", "src/assets/enemy_melee_attack.h", "ENEMY_MELEE_ATTACK", scale=1)
    convert_screen("graphics/startscreen.png", "src/screens/startscreen.h", "startscreen")
    convert_screen("graphics/controlsscreen.png", "src/screens/controlsscreen.h", "controlsscreen")
    convert_screen("graphics/deathscreen.png", "src/screens/deathscreen.h", "deathscreen")
    print("Converted sprites, textures, and screens.")
