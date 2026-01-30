#include <stdlib.h>
#include <stdbool.h>
#include "map.h"

int worldMap[MAP_WIDTH][MAP_HEIGHT];

// A simple shuffle function for the directions
static void shuffle_directions(int directions[4][2]) {
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int tempX = directions[i][0];
        int tempY = directions[i][1];
        directions[i][0] = directions[j][0];
        directions[i][1] = directions[j][1];
        directions[j][0] = tempX;
        directions[j][1] = tempY;
    }
}

static void recursiveBacktracker(int x, int y) {
    worldMap[x][y] = 0;
    
    int directions[4][2] = {
        {0, 2}, {0, -2}, {2, 0}, {-2, 0}
    };
    
    shuffle_directions(directions);
    
    for (int i = 0; i < 4; i++) {
        int dx = directions[i][0];
        int dy = directions[i][1];
        int nx = x + dx;
        int ny = y + dy;
        
        if (nx > 0 && nx < MAP_WIDTH - 1 && ny > 0 && ny < MAP_HEIGHT - 1) {
            if (worldMap[nx][ny] == 1) {
                worldMap[x + dx / 2][y + dy / 2] = 0;
                recursiveBacktracker(nx, ny);
            }
        }
    }
}

void generateMap(void) {
    // 1. Initialize maze with walls
    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            worldMap[x][y] = 1;
        }
    }
    
    // 2. Generate maze starting from (1, 1)
    recursiveBacktracker(1, 1);
    
    // 3. Add multiple paths by randomly removing some walls
    for (int i = 0; i < MAP_WIDTH * 2; i++) {
        int rx = 1 + (rand() % (MAP_WIDTH - 2));
        int ry = 1 + (rand() % (MAP_HEIGHT - 2));
        if (worldMap[rx][ry] == 1) {
            worldMap[rx][ry] = 0;
        }
    }
    
    // 4. Create exit room at bottom right
    int ROOM_SIZE = 2;
    for (int x = MAP_WIDTH - ROOM_SIZE - 1; x < MAP_WIDTH - 1; x++) {
        for (int y = MAP_HEIGHT - ROOM_SIZE - 1; y < MAP_HEIGHT - 1; y++) {
            worldMap[x][y] = 0;
        }
    }
    
    // 5. Place temporary sphere in the middle of the exit room
    int sphere_x = MAP_WIDTH - 2;
    int sphere_y = MAP_HEIGHT - 2;
    worldMap[sphere_x][sphere_y] = 2;
    
    // 6. Find air spaces and then pick enemy locations
    struct { int x, y; } air_spaces[MAP_WIDTH * MAP_HEIGHT];
    int air_count = 0;
    
    for (int x = 1; x < MAP_WIDTH - 1; x++) {
        for (int y = 1; y < MAP_HEIGHT - 1; y++) {
            // Logic from Python: maze[y][x] == 0 and (x > 2 or y > 2) and (x < MAP_SIZE-4 or y < MAP_SIZE-4)
            if (worldMap[x][y] == 0 && (x > 2 || y > 2) && (x < MAP_WIDTH - 4 || y < MAP_HEIGHT - 4)) {
                air_spaces[air_count].x = x;
                air_spaces[air_count].y = y;
                air_count++;
            }
        }
    }
    
    // 7. Place up to 4 enemies (marked as 3)
    int enemies_to_place = (air_count < 4) ? air_count : 4;
    // Simple random sampling: shuffle air_spaces and take first N
    for (int i = 0; i < air_count; i++) {
        int j = rand() % air_count;
        int tempX = air_spaces[i].x;
        int tempY = air_spaces[i].y;
        air_spaces[i].x = air_spaces[j].x;
        air_spaces[i].y = air_spaces[j].y;
        air_spaces[j].x = tempX;
        air_spaces[j].y = tempY;
    }
    
    for (int i = 0; i < enemies_to_place; i++) {
        worldMap[air_spaces[i].x][air_spaces[i].y] = 3;
    }
}
