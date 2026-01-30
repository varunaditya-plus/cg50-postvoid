import random

MAP_SIZE = 15
maze = [[1 for _ in range(MAP_SIZE)] for _ in range(MAP_SIZE)]

def generate_maze(x, y):
    maze[y][x] = 0
    directions = [(0, 2), (0, -2), (2, 0), (-2, 0)]
    random.shuffle(directions)
    
    for dx, dy in directions:
        nx, ny = x + dx, y + dy
        if 0 < nx < MAP_SIZE-1 and 0 < ny < MAP_SIZE-1 and maze[ny][nx] == 1:
            maze[y + dy//2][x + dx//2] = 0
            generate_maze(nx, ny)

generate_maze(1, 1)

# add multiple paths by randomly removing some walls
for _ in range(MAP_SIZE * 2):
    rx = random.randint(1, MAP_SIZE-2)
    ry = random.randint(1, MAP_SIZE-2)
    if maze[ry][rx] == 1:
        maze[ry][rx] = 0

# create exit room at bottom right
ROOM_SIZE = 2
for i in range(MAP_SIZE - ROOM_SIZE - 1, MAP_SIZE - 1):
    for j in range(MAP_SIZE - ROOM_SIZE - 1, MAP_SIZE - 1):
        maze[i][j] = 0

# place temporary sphere in the middle of the exit room
sphere_x = MAP_SIZE - 2
sphere_y = MAP_SIZE - 2
maze[sphere_y][sphere_x] = 2

# find air spaces and then pick enemy locations (fix algorithm so theyre spaced)
air_spaces = []
for y in range(1, MAP_SIZE - 1):
    for x in range(1, MAP_SIZE - 1):
        if maze[y][x] == 0 and (x > 2 or y > 2) and (x < MAP_SIZE-4 or y < MAP_SIZE-4):
            air_spaces.append((x, y))

enemies = random.sample(air_spaces, min(4, len(air_spaces)))
for ex, ey in enemies:
    maze[ey][ex] = 3

with open("src/map.h", "w") as f:
    f.write("#ifndef MAP_H\n#define MAP_H\n\n")
    f.write(f"#define MAP_WIDTH {MAP_SIZE}\n")
    f.write(f"#define MAP_HEIGHT {MAP_SIZE}\n\n")
    f.write("static const int worldMap[MAP_WIDTH][MAP_HEIGHT] = {\n")
    for row in maze:
        f.write("    {" + ", ".join(map(str, row)) + "},\n")
    f.write("};\n\n")
    f.write("#endif\n")

print(f"Generated maze {MAP_SIZE}x{MAP_SIZE} with 4 enemies marked as '3'.")
