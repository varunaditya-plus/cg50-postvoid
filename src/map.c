#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "map.h"

int worldMap[MAP_WIDTH][MAP_HEIGHT];

static void create_room(int rx, int ry, int rw, int rh) {
    for (int x = rx; x < rx + rw; x++) {
        for (int y = ry; y < ry + rh; y++) {
            if (x > 0 && x < MAP_WIDTH - 1 && y > 0 && y < MAP_HEIGHT - 1) {
                worldMap[x][y] = 0;
            }
        }
    }
}

void generateMap(void) {
    // 1. Initialize with walls
    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            worldMap[x][y] = 1;
        }
    }

    // 2. Path-based generation (inspired by obj_world_builder)
    int path_x = 1;
    int path_y = MAP_HEIGHT / 2;
    int goal_x = MAP_WIDTH - 2;

    while (path_x <= goal_x) {
        worldMap[path_x][path_y] = 0;
        
        // Occasionally clear extra Y for wider corridors
        if (rand() % 100 < 40) {
            int extra_y = path_y + (rand() % 2 == 0 ? 1 : -1);
            if (extra_y > 0 && extra_y < MAP_HEIGHT - 1) {
                worldMap[path_x][extra_y] = 0;
            }
        }

        // Randomly turn
        if (rand() % 100 < 30) {
            int new_y = path_y + (rand() % 2 == 0 ? 1 : -1);
            if (new_y > 0 && new_y < MAP_HEIGHT - 1) {
                path_y = new_y;
            }
        } else {
            path_x++;
        }

        // Randomly create a room
        if (path_x % 10 == 5 && rand() % 100 < 50) {
            int rw = 3 + rand() % 4;
            int rh = 3 + rand() % 4;
            int rx = path_x - rw / 2;
            int ry = path_y - rh / 2;
            create_room(rx, ry, rw, rh);
            
            // Add a pillar (inspired by scr_wb_create_room "pillar" type)
            if (rw >= 3 && rh >= 3 && rand() % 100 < 40) {
                int px = rx + rw / 2;
                int py = ry + rh / 2;
                if (px > 0 && px < MAP_WIDTH - 1 && py > 0 && py < MAP_HEIGHT - 1) {
                    worldMap[px][py] = 1;
                }
            }
        }
    }

    // 3. Ensure the start is clear
    worldMap[1][MAP_HEIGHT / 2] = 0;
    worldMap[2][MAP_HEIGHT / 2] = 0;

    // 4. Place exit sphere at the end of the path and create a small room around it
    create_room(goal_x - 1, path_y - 1, 3, 3);
    worldMap[goal_x][path_y] = 2;

    // 5. Find air spaces and pick enemy locations
    struct { int x, y; } air_spaces[MAP_WIDTH * MAP_HEIGHT];
    int air_count = 0;

    for (int x = 1; x < MAP_WIDTH - 1; x++) {
        for (int y = 1; y < MAP_HEIGHT - 1; y++) {
            // Avoid placing enemies too close to the start or on the goal
            if (worldMap[x][y] == 0 && (x > 4)) {
                air_spaces[air_count].x = x;
                air_spaces[air_count].y = y;
                air_count++;
            }
        }
    }

    // 6. Place enemies (marked as 3)
    int enemies_to_place = (air_count < 10) ? air_count : 10;
    // Shuffle air_spaces
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
