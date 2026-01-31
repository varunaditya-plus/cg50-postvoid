from PIL import Image
import struct

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
    convert_wall("scripts/wall_texture_map.png", "src/assets/wall_texture.h")
    convert_screen("graphics/startscreen.png", "src/screens/startscreen.h", "startscreen")
    convert_screen("graphics/controlsscreen.png", "src/screens/controlsscreen.h", "controlsscreen")
    convert_screen("graphics/deathscreen.png", "src/screens/deathscreen.h", "deathscreen")
    print("Converted sprites, textures, and screens.")
