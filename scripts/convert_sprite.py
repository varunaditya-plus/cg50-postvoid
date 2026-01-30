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
    # take first 128x128 block
    tex_size = 128
    pixels = []
    for y in range(tex_size):
        for x in range(tex_size):
            r, g, b = img.getpixel((x, y))
            pixels.append(to_rgb565_gint(r, g, b))
    
    with open(output_header, "w") as f:
        f.write("#ifndef WALL_TEXTURE_H\n#define WALL_TEXTURE_H\n\n")
        f.write(f"#define TEX_WIDTH {tex_size}\n")
        f.write(f"#define TEX_HEIGHT {tex_size}\n\n")
        f.write("static const uint16_t wall_texture[] = {\n")
        for i, p in enumerate(pixels):
            f.write(f"{p}, ")
            if (i + 1) % tex_size == 0: f.write("\n")
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
    convert_enemy("enemy_texture.png", "src/enemy_sprite.h")
    convert_wall("wall_texture_map.png", "src/wall_texture.h")
    convert_screen("graphics/startscreen.png", "src/startscreen.h", "startscreen")
    convert_screen("graphics/controlsscreen.png", "src/controlsscreen.h", "controlsscreen")
    print("Converted sprites, textures, and screens.")
