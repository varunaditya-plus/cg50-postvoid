#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "map.h"
#include "assets/enemy_sprite.h"
#include "assets/wall_texture.h"
#include "screens/startscreen.h"
#include "screens/controlsscreen.h"
#include "screens/deathscreen.h"

#define SCREEN_WIDTH 396
#define SCREEN_HEIGHT 224
#define H_RES 4 // render each 4th pixel horizontally for faster rendering
#define MAX_ENEMIES 10

// player state
float posX = 1.5f, posY = MAP_HEIGHT / 2.0f + 0.5f;
float dirX = 1.0f, dirY = 0.0f;
float planeX = 0.0f, planeY = 0.66f;
float pitch = 0.0f;
float playerHP = 125.0f;
float maxHP = 125.0f;
float hpDecay = 0.25f; // per frame

float zBuffer[SCREEN_WIDTH];
float sphereX = MAP_WIDTH - 2.5f;
float sphereY = MAP_HEIGHT - 2.5f;

typedef struct { float x, y, dx, dy; bool active; } Bullet;
Bullet bullet = {0};

typedef struct { float x, y; bool alive; float speed; int mode; } Enemy;
Enemy enemies[MAX_ENEMIES];
int actualEnemyCount = 0;

void render() {
    uint16_t *vram = gint_vram;
    int horiz = (int)pitch;

    // fast clear: background
    dclear(C_RGB(4, 4, 4));
    int ceilingEnd = (SCREEN_HEIGHT / 2) + horiz;
    if (ceilingEnd > 0) {
        int cLimit = ceilingEnd > SCREEN_HEIGHT ? SCREEN_HEIGHT : ceilingEnd;
        drect(0, 0, SCREEN_WIDTH - 1, cLimit - 1, C_RGB(2, 2, 2));
    }

    // wall rendering (optimized)
    for(int x = 0; x < SCREEN_WIDTH; x += H_RES) {
        float cameraX = 2.0f * x / (float)SCREEN_WIDTH - 1.0f; 
        float rayDirX = dirX + planeX * cameraX;
        float rayDirY = dirY + planeY * cameraX;

        int mapX = (int)posX;
        int mapY = (int)posY;

        float deltaDistX = (rayDirX == 0) ? 1e30f : fabsf(1.0f / rayDirX);
        float deltaDistY = (rayDirY == 0) ? 1e30f : fabsf(1.0f / rayDirY);

        float sideDistX, sideDistY;
        int stepX, stepY;
        int hit = 0, side;

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = (posX - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0f - posX) * deltaDistX;
        }
        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = (posY - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0f - posY) * deltaDistY;
        }

        int iter = 0;
        while (hit == 0 && iter < 64) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }
            if (worldMap[mapX][mapY] == 1) hit = 1;
            iter++;
        }

        float perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
        if (perpWallDist < 0.1f) perpWallDist = 0.1f;
        
        for(int i=0; i<H_RES; i++) zBuffer[x+i] = perpWallDist;

        int lineHeight = (int)(SCREEN_HEIGHT / perpWallDist);
        int drawStart = -lineHeight / 2 + SCREEN_HEIGHT / 2 + horiz;
        int drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2 + horiz;

        float wallX = (side == 0) ? (posY + perpWallDist * rayDirY) : (posX + perpWallDist * rayDirX);
        wallX -= (float)((int)wallX);

        int texX = (int)(wallX * (float)TEX_WIDTH);
        if((side == 0 && rayDirX > 0) || (side == 1 && rayDirY < 0)) texX = TEX_WIDTH - texX - 1;

        float step_tex = (float)TEX_HEIGHT / lineHeight;
        float texPos = (drawStart - horiz - SCREEN_HEIGHT / 2 + lineHeight / 2) * step_tex;

        int y_start = (drawStart < 0) ? 0 : drawStart;
        int y_end = (drawEnd >= SCREEN_HEIGHT) ? SCREEN_HEIGHT - 1 : drawEnd;

        int shade = (side == 1) ? 180 : 255;
        float intensity = 1.0f / (1.0f + perpWallDist * 0.2f);
        int final_shade = (int)(shade * intensity);

        uint16_t *dest = &vram[y_start * SCREEN_WIDTH + x];
        for(int y = y_start; y <= y_end; y++) {
            int texY = (int)texPos & (TEX_HEIGHT - 1);
            texPos += step_tex;
            uint16_t color = wall_texture[(texY << 7) | texX];

            if (final_shade < 240) {
                int r = ((color >> 11) * final_shade) >> 8;
                int g = (((color >> 5) & 0x3F) * final_shade) >> 8;
                int b = ((color & 0x1F) * final_shade) >> 8;
                color = (r << 11) | (g << 5) | b;
            }

            dest[0] = color; dest[1] = color; dest[2] = color; dest[3] = color;
            dest += SCREEN_WIDTH;
        }
    }

    // enemy and sphere thingy rendering
    for(int i = 0; i < actualEnemyCount + 1; i++) {
        float sx, sy;
        bool active = false;
        bool isSphere = (i == actualEnemyCount);
        
        if (isSphere) { sx = sphereX; sy = sphereY; active = true; }
        else { sx = enemies[i].x; sy = enemies[i].y; active = enemies[i].alive; }
        
        if (!active) continue;

        float rx = sx - posX, ry = sy - posY;
        float invDet = 1.0f / (planeX * dirY - dirX * planeY);
        float tx = invDet * (dirY * rx - dirX * ry);
        float ty = invDet * (-planeY * rx + planeX * ry);

        if(ty > 0.3f) {
            int screenX = (int)((SCREEN_WIDTH / 2) * (1 + tx / ty));
            int h = (int)fabsf(SCREEN_HEIGHT / ty);
            int w = isSphere ? h : (int)(h * (float)ENEMY_SPR_WIDTH / ENEMY_SPR_HEIGHT);
            int x_s = screenX - w/2, x_e = screenX + w/2;
            
            for(int x = x_s; x < x_e; x++) {
                if(x >= 0 && x < SCREEN_WIDTH && ty < zBuffer[x]) {
                    if (isSphere) {
                        float dx = (x - screenX) / (float)(w / 2.0f);
                        float dy_lim = 1.0f - dx * dx;
                        if (dy_lim <= 0) continue;
                        dy_lim = sqrtf(dy_lim);
                        int vh = (int)(h * dy_lim);
                        int vs = SCREEN_HEIGHT/2 - vh/2 + horiz;
                        int ve = SCREEN_HEIGHT/2 + vh/2 + horiz;
                        if (vs < 0) vs = 0;
                        if (ve >= SCREEN_HEIGHT) ve = SCREEN_HEIGHT - 1;
                        color_t color = (dy_lim > 0.7f) ? C_WHITE : C_LIGHT;
                        for(int y=vs; y<=ve; y++) vram[y * SCREEN_WIDTH + x] = color;
                    } else {
                        int texX = (x - x_s) * ENEMY_SPR_WIDTH / w;
                        for(int y = 0; y < h; y++) {
                            int dy = y + SCREEN_HEIGHT / 2 - h / 2 + horiz;
                            if(dy >= 0 && dy < SCREEN_HEIGHT) {
                                int texY = y * ENEMY_SPR_HEIGHT / h;
                                int color = enemy_sprite[texY * ENEMY_SPR_WIDTH + texX];
                                if(color != -1) vram[dy * SCREEN_WIDTH + x] = color;
                            }
                        }
                    }
                }
            }
        }
    }

    // health bar (will eventually be white liquid jar)
    int hp_bar_w = 100;
    int hp_bar_h = 10;
    int hp_x = 10;
    int hp_y = SCREEN_HEIGHT - 20;
    drect(hp_x - 1, hp_y - 1, hp_x + hp_bar_w + 1, hp_y + hp_bar_h + 1, C_WHITE);
    int current_hp_w = (int)(playerHP * hp_bar_w / maxHP);
    if (current_hp_w > 0) {
        if (current_hp_w > hp_bar_w) current_hp_w = hp_bar_w;
        drect(hp_x, hp_y, hp_x + current_hp_w, hp_y + hp_bar_h, C_RGB(31, 0, 0));
    }

    // bullet and crosshair rendering
    if(bullet.active) {
        float bx = bullet.x - posX, by = bullet.y - posY;
        float invDetB = 1.0f / (planeX * dirY - dirX * planeY);
        float tbx = invDetB * (dirY * bx - dirX * by);
        float tby = invDetB * (-planeY * bx + planeX * by);
        if(tby > 0.1f) {
            int bsx = (int)((SCREEN_WIDTH / 2) * (1 + tbx / tby));
            int bsy = SCREEN_HEIGHT / 2 + horiz;
            if(bsx >= 0 && bsx < SCREEN_WIDTH && tby < zBuffer[bsx]) drect(bsx-1, bsy-1, bsx+1, bsy+1, C_WHITE);
        }
    }

    int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2 + horiz;
    dline(cx - 4, cy, cx + 4, cy, C_WHITE);
    dline(cx, cy - 4, cx, cy + 4, C_WHITE);
    dupdate();
}

unsigned int entropy_seed = 0;

bool show_splash(const uint16_t *image) {
    uint16_t *vram = gint_vram;
    for(int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        vram[i] = image[i];
    }
    dupdate();
    clearevents();
    
    while(1) {
        // increment seed while waiting for user input
        entropy_seed++;

        if(keydown(KEY_EXIT) || keydown(KEY_MENU)) {
            return false;
        }
        if(keydown(KEY_F6)) {
            // wait for key release to prevent double skips
            while(keydown(KEY_F6)) { clearevents(); }
            return true;
        }
        clearevents();
    }
}

int main(void) {
    while(1) {
        // Initial seed with RTC as a fallback
        srand(rtc_ticks());

        if (!show_splash(startscreen)) return 0;
        if (!show_splash(controlsscreen)) return 0;

        // Re-seed using the entropy gathered during the splash screens
        srand(rtc_ticks() ^ entropy_seed);

        while(1) {
            generateMap();

            // player state reset
            posX = 1.5f; posY = MAP_HEIGHT / 2.0f + 0.5f;
            dirX = 1.0f; dirY = 0.0f;
            planeX = 0.0f; planeY = 0.66f;
            pitch = 0.0f;
            playerHP = 125.0f;

            // scan worldMap for enemies (marked as 3) and sphere (marked as 2)
            actualEnemyCount = 0;
            for(int y = 0; y < MAP_HEIGHT; y++) {
                for(int x = 0; x < MAP_WIDTH; x++) {
                    if(worldMap[x][y] == 3 && actualEnemyCount < MAX_ENEMIES) {
                        enemies[actualEnemyCount].x = (float)x + 0.5f;
                        enemies[actualEnemyCount].y = (float)y + 0.5f;
                        enemies[actualEnemyCount].alive = true;
                        enemies[actualEnemyCount].speed = 0.08f + (rand() % 10) * 0.01f; // faster enemies
                        enemies[actualEnemyCount].mode = 0; // 0 = idle, 1 = hunt
                        actualEnemyCount++;
                    }
                    if(worldMap[x][y] == 2) {
                        sphereX = (float)x + 0.5f;
                        sphereY = (float)y + 0.5f;
                    }
                }
            }

            if (worldMap[(int)posX + 1][(int)posY] == 0) { dirX = 1.0f; dirY = 0.0f; planeX = 0.0f; planeY = 0.66f; }
            else if (worldMap[(int)posX][(int)posY + 1] == 0) { dirX = 0.0f; dirY = 1.0f; planeX = -0.66f; planeY = 0.0f; }
            else if (worldMap[(int)posX - 1][(int)posY] == 0) { dirX = -1.0f; dirY = 0.0f; planeX = 0.0f; planeY = -0.66f; }
            else if (worldMap[(int)posX][(int)posY - 1] == 0) { dirX = 0.0f; dirY = -1.0f; planeX = 0.66f; planeY = 0.0f; }

            bool died = false;
            while(1) {
                // Update HP
                playerHP -= hpDecay;
                if (playerHP <= 0) { died = true; break; }

                // update bullet if active
                if(bullet.active) {
                    bullet.x += bullet.dx * 0.7f; // faster bullet
                    bullet.y += bullet.dy * 0.7f;
                    int bx = (int)bullet.x, by = (int)bullet.y;
                    if(bx < 0 || bx >= MAP_WIDTH || by < 0 || by >= MAP_HEIGHT || worldMap[bx][by] == 1) {
                        bullet.active = false;
                    } else {
                        for(int i = 0; i < actualEnemyCount; i++) {
                            if(enemies[i].alive) {
                                float dx = bullet.x - enemies[i].x;
                                float dy = bullet.y - enemies[i].y;
                                if(dx*dx + dy*dy < 0.3f) { 
                                    enemies[i].alive = false; 
                                    bullet.active = false; 
                                    playerHP += 35.0f; // gain HP on kill
                                    if (playerHP > maxHP) playerHP = maxHP;
                                    break; 
                                }
                            }
                        }
                    }
                }

                // enemy AI logic
                for(int i = 0; i < actualEnemyCount; i++) {
                    if(!enemies[i].alive) continue;

                    float edx = posX - enemies[i].x;
                    float edy = posY - enemies[i].y;
                    float distSq = edx * edx + edy * edy;

                    if(enemies[i].mode == 0 && distSq < 64.0f) { // 8 blocks aggro
                        enemies[i].mode = 1;
                    }

                    if(enemies[i].mode == 1) {
                        float dist = sqrtf(distSq);
                        if(dist > 0.1f) {
                            float moveX = (edx / dist) * enemies[i].speed;
                            float moveY = (edy / dist) * enemies[i].speed;
                            if(worldMap[(int)(enemies[i].x + moveX)][(int)enemies[i].y] != 1) enemies[i].x += moveX;
                            if(worldMap[(int)enemies[i].x][(int)(enemies[i].y + moveY)] != 1) enemies[i].y += moveY;
                        }
                        if(distSq < 0.25f) { // damage player
                            playerHP -= 1.0f;
                        }
                    }
                }

                render();
                clearevents();
                if(keydown(KEY_EXIT) || keydown(KEY_MENU)) return 0;

                float moveStep = 0.25f; // faster movement
                float rotStep = 0.12f; // faster rotation
                if(keydown(KEY_8)) { if(worldMap[(int)(posX + dirX * moveStep)][(int)posY] != 1) posX += dirX * moveStep; if(worldMap[(int)posX][(int)(posY + dirY * moveStep)] != 1) posY += dirY * moveStep; }
                if(keydown(KEY_5)) { if(worldMap[(int)(posX - dirX * moveStep)][(int)posY] != 1) posX -= dirX * moveStep; if(worldMap[(int)posX][(int)(posY - dirY * moveStep)] != 1) posY -= dirY * moveStep; }
                if(keydown(KEY_6)) { if(worldMap[(int)(posX + planeX * moveStep)][(int)posY] != 1) posX += planeX * moveStep; if(worldMap[(int)posX][(int)(posY + planeY * moveStep)] != 1) posY += planeY * moveStep; }
                if(keydown(KEY_4)) { if(worldMap[(int)(posX - planeX * moveStep)][(int)posY] != 1) posX -= planeX * moveStep; if(worldMap[(int)posX][(int)(posY - planeY * moveStep)] != 1) posY -= planeY * moveStep; }
                if(keydown(KEY_RIGHT)) { float odx = dirX; dirX = dirX * cosf(rotStep) - dirY * sinf(rotStep); dirY = odx * sinf(rotStep) + dirY * cosf(rotStep); float opx = planeX; planeX = planeX * cosf(rotStep) - planeY * sinf(rotStep); planeY = opx * sinf(rotStep) + planeY * cosf(rotStep); }
                if(keydown(KEY_LEFT)) { float odx = dirX; dirX = dirX * cosf(-rotStep) - dirY * sinf(-rotStep); dirY = odx * sinf(-rotStep) + dirY * cosf(-rotStep); float opx = planeX; planeX = planeX * cosf(-rotStep) - planeY * sinf(-rotStep); planeY = opx * sinf(-rotStep) + planeY * cosf(-rotStep); }
                if(keydown(KEY_UP)) { pitch += 5.0f; if (pitch > 110) pitch = 110; }
                if(keydown(KEY_DOWN)) { pitch -= 5.0f; if (pitch < -110) pitch = -110; }
                if(keydown(KEY_F6) && !bullet.active) { bullet.x = posX + dirX * 0.2f; bullet.y = posY + dirY * 0.2f; bullet.dx = dirX; bullet.dy = dirY; bullet.active = true; }
            }

            if (died) {
                if (!show_splash(deathscreen)) return 0;
                // if F6 pressed (show_splash returns true), loop will continue to generateMap()
            } else {
                break;
            }
        }
    }
    return 0;
}
