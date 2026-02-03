from PIL import Image
import struct
import os
import re

def to_rgb565_gint(r, g, b):
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g5 << 6) | b5

def quantize_to_palette(img, max_colors=256, transparent_color=None):
    # img is PIL image
    if img.mode == "RGBA":
        # Separate alpha
        alpha = img.split()[3]
        img_rgb = img.convert("RGB")
        # Quantize RGB part
        img_p = img_rgb.quantize(colors=max_colors - (1 if transparent_color is not None else 0), method=Image.MEDIANCUT)
        palette_rgb = img_p.getpalette()[:(max_colors*3)]
        
        palette_565 = []
        for i in range(0, len(palette_rgb), 3):
            palette_565.append(to_rgb565_gint(palette_rgb[i], palette_rgb[i+1], palette_rgb[i+2]))
        
        indices = list(img_p.getdata())
        
        if transparent_color is not None:
            trans_idx = len(palette_565)
            palette_565.append(transparent_color)
            # update indices where alpha < 128
            alpha_data = list(alpha.getdata())
            for i in range(len(indices)):
                if alpha_data[i] < 128:
                    indices[i] = trans_idx
        return palette_565, indices
    else:
        img_p = img.quantize(colors=max_colors, method=Image.MEDIANCUT)
        palette_rgb = img_p.getpalette()[:(max_colors*3)]
        palette_565 = []
        for i in range(0, len(palette_rgb), 3):
            palette_565.append(to_rgb565_gint(palette_rgb[i], palette_rgb[i+1], palette_rgb[i+2]))
        indices = list(img_p.getdata())
        return palette_565, indices

def _bbox_non_transparent(img, alpha_thresh=128):
    w, h = img.size
    min_x, min_y = w, h
    max_x, max_y = -1, -1
    for y in range(h):
        for x in range(w):
            if img.getpixel((x, y))[3] >= alpha_thresh:
                min_x, min_y = min(min_x, x), min(min_y, y)
                max_x, max_y = max(max_x, x), max(max_y, y)
    return (min_x, min_y, max_x, max_y) if max_x >= 0 else None

def compress_rle(indices):
    if not indices: return []
    compressed = []
    curr_idx = indices[0]
    curr_count = 0
    for idx in indices:
        if idx == curr_idx and curr_count < 255:
            curr_count += 1
        else:
            compressed.append(curr_count)
            compressed.append(curr_idx)
            curr_idx = idx
            curr_count = 1
    compressed.append(curr_count)
    compressed.append(curr_idx)
    return compressed

def convert_enemy(image_path, output_header):
    img = Image.open(image_path).convert("RGBA")
    width, height = img.size
    palette, indices = quantize_to_palette(img, transparent_color=0xF81F)
    
    with open(output_header, "w") as f:
        f.write("#ifndef ENEMY_SPRITE_H\n#define ENEMY_SPRITE_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define ENEMY_SPR_WIDTH {width}\n")
        f.write(f"#define ENEMY_SPR_HEIGHT {height}\n\n")
        f.write(f"static const uint16_t enemy_sprite_palette[{len(palette)}] = {{\n")
        f.write(", ".join(map(str, palette)) + "\n};\n\n")
        f.write(f"static const uint8_t enemy_sprite[{len(indices)}] = {{\n")
        for i, p in enumerate(indices):
            f.write(f"{p}, ")
            if (i + 1) % width == 0: f.write("\n")
        f.write("};\n\n#endif\n")

def convert_wall(image_path, output_header):
    img = Image.open(image_path).convert("RGB")
    tex_size = 128
    palette, indices = quantize_to_palette(img)
    
    # tiles for levels 1-7
    tiles_coords = [(0, 0), (1, 0), (0, 1), (3, 2), (3, 0), (0, 2), (0, 3)]
    
    with open(output_header, "w") as f:
        f.write("#ifndef WALL_TEXTURE_H\n#define WALL_TEXTURE_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define TEX_WIDTH {tex_size}\n")
        f.write(f"#define TEX_HEIGHT {tex_size}\n\n")
        f.write(f"static const uint16_t wall_palette[{len(palette)}] = {{\n")
        f.write(", ".join(map(str, palette)) + "\n};\n\n")
        f.write(f"static const uint8_t wall_textures[7][128 * 128] = {{\n")
        
        for tx, ty in tiles_coords:
            f.write("{\n")
            for y in range(ty * tex_size, (ty + 1) * tex_size):
                for x in range(tx * tex_size, (tx + 1) * tex_size):
                    idx = indices[y * img.width + x]
                    f.write(f"{idx}, ")
                f.write("\n")
            f.write("},\n")
        f.write("};\n\n#endif\n")

def convert_sprite_strip_cropped(folder_path, output_header, prefix, scale=1):
    folder = os.path.abspath(folder_path)
    frame_files = []
    for name in os.listdir(folder):
        m = re.match(r"(.+)_(\d+)\.png$", name, re.IGNORECASE)
        if m: frame_files.append((int(m.group(2)), os.path.join(folder, name)))
    frame_files.sort(key=lambda x: x[0])
    
    num_frames = len(frame_files)
    frames = []
    for _, path in frame_files:
        img = Image.open(path).convert("RGBA")
        if scale > 1:
            img = img.resize((img.width // scale, img.height // scale), Image.LANCZOS)
        frames.append(img)
    
    sw, sh = frames[0].size
    logical_w, logical_h = sw, sh # already scaled

    # collect pixels to build a global palette for the strip
    all_img = Image.new("RGBA", (sw, sh * num_frames))
    for i, img in enumerate(frames):
        all_img.paste(img, (0, i * sh))
    
    TRANSPARENT = 0xF81F
    palette, _ = quantize_to_palette(all_img, transparent_color=TRANSPARENT)
    trans_idx = len(palette) - 1

    frame_infos = []
    packed_indices = []

    for img in frames:
        bbox = _bbox_non_transparent(img)
        if bbox is None:
            frame_infos.append((0, 0, 1, 1, len(packed_indices)))
            packed_indices.append(trans_idx)
            continue
        
        min_x, min_y, max_x, max_y = bbox
        w, h = max_x - min_x, max_y - min_y
        
        offset = len(packed_indices)
        frame_infos.append((min_x, min_y, w, h, offset))
        
        for dy in range(h):
            for dx in range(w):
                r, g, b, a = img.getpixel((min_x + dx, min_y + dy))
                if a < 128:
                    packed_indices.append(trans_idx)
                else:
                    # Find closest color
                    c565 = to_rgb565_gint(r, g, b)
                    best_idx = 0
                    best_diff = 1000000
                    for i, pc in enumerate(palette):
                        if i == trans_idx: continue
                        diff = abs((pc >> 11) - (c565 >> 11)) + abs(((pc >> 6) & 0x1F) - ((c565 >> 6) & 0x1F)) + abs((pc & 0x1F) - (c565 & 0x1F))
                        if diff < best_diff:
                            best_diff = diff
                            best_idx = i
                    packed_indices.append(best_idx)

    guard = prefix + "_H"
    with open(output_header, "w") as f:
        f.write(f"#ifndef {guard}\n#define {guard}\n\n#include <stdint.h>\n\n")
        f.write(f"#define {prefix}_FRAMES {num_frames}\n#define {prefix}_WIDTH {logical_w}\n#define {prefix}_HEIGHT {logical_h}\n#define {prefix}_TRANSPARENT_IDX {trans_idx}\n\n")
        f.write(f"typedef struct {{ int x, y, w, h; int offset; }} {prefix.lower()}_frame_t;\n\n")
        f.write(f"static const {prefix.lower()}_frame_t {prefix.lower()}_frame_info[{num_frames}] = {{\n")
        for (x, y, w, h, off) in frame_infos: f.write(f"  {{ {x}, {y}, {w}, {h}, {off} }},\n")
        f.write("};\n\n")
        f.write(f"static const uint16_t {prefix.lower()}_palette[{len(palette)}] = {{\n")
        f.write(", ".join(map(str, palette)) + "\n};\n\n")
        f.write(f"static const uint8_t {prefix.lower()}_pixels[{len(packed_indices)}] = {{\n")
        for i in range(0, len(packed_indices), 16):
            f.write("  " + ", ".join(map(str, packed_indices[i:i+16])) + ",\n")
        f.write("};\n\n#endif\n")

def convert_screen(image_path, output_header, var_name):
    img = Image.open(image_path).convert("RGB")
    palette, indices = quantize_to_palette(img)
    
    with open(output_header, "w") as f:
        f.write(f"#ifndef {var_name.upper()}_H\n#define {var_name.upper()}_H\n\n#include <stdint.h>\n\n")
        f.write(f"static const uint16_t {var_name}_palette[{len(palette)}] = {{\n")
        f.write(", ".join(map(str, palette)) + "\n};\n\n")
        f.write(f"static const uint8_t {var_name}_pixels[{len(indices)}] = {{\n")
        for i in range(0, len(indices), 16):
            f.write("  " + ", ".join(map(str, indices[i:i+16])) + ",\n")
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
    convert_screen("graphics/controls/preset1.png", "src/screens/preset1.h", "preset1")
    convert_screen("graphics/controls/preset2.png", "src/screens/preset2.h", "preset2")
    convert_screen("graphics/controls/preset3.png", "src/screens/preset3.h", "preset3")
    convert_screen("graphics/deathscreen.png", "src/screens/deathscreen.h", "deathscreen")
    convert_screen("graphics/winscreen.png", "src/screens/winscreen.h", "winscreen")
    print("Converted sprites, textures, and screens.")
