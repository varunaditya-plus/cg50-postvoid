#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "map.h"
#include "assets/enemy_melee_walk.h"
#include "assets/enemy_melee_attack.h"
#include "assets/wall_texture.h"
#include "assets/shoot_effect.h"
#include "assets/gun_idle.h"
#include "assets/gun_shoot.h"
#include "screens/startscreen.h"
#include "screens/preset1.h"
#include "screens/preset2.h"
#include "screens/preset3.h"
#include "screens/deathscreen.h"
#include "screens/winscreen.h"

#define SCREEN_WIDTH 396
#define SCREEN_HEIGHT 224
#define H_RES 4 // render each 4th pixel horizontally for faster rendering
#define NUM_LEVELS 7
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
int currentLevel = 1;
int selectedPreset = 1;

typedef struct { float x, y, dx, dy; bool active; } Bullet;
Bullet bullet = {0};
int shootEffectTimer = 0;
int gunShootTimer = 0;
static int gunIdleAnimFrame = 0;
#define GUN_SHOOT_DURATION 8

typedef struct { float x, y; bool alive; float speed; int mode; bool attacking; int anim_frame; } Enemy;
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
            uint16_t color = wall_textures[currentLevel - 1][texY * TEX_WIDTH + texX];

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
            int spr_w = ENEMY_MELEE_WALK_WIDTH, spr_h = ENEMY_MELEE_WALK_HEIGHT;
            int w = isSphere ? h : (int)(h * (float)spr_w / spr_h);
            int x_s = screenX - w/2, y_s = SCREEN_HEIGHT / 2 - h / 2 + horiz;
            int x_e = screenX + w/2;

            if (isSphere) {
                for(int x = x_s; x < x_e; x++) {
                    if(x >= 0 && x < SCREEN_WIDTH && ty < zBuffer[x]) {
                        int vh = h / 2;
                        int vs = SCREEN_HEIGHT / 2 + horiz;
                        int ve = vs + vh;
                        if (vs < 0) vs = 0;
                        if (ve >= SCREEN_HEIGHT) ve = SCREEN_HEIGHT - 1;
                        for(int y = vs; y <= ve; y++) vram[y * SCREEN_WIDTH + x] = C_WHITE;
                    }
                }
            } else {
                const enemy_melee_walk_frame_t *fi_w = &enemy_melee_walk_frame_info[enemies[i].anim_frame % ENEMY_MELEE_WALK_FRAMES];
                const enemy_melee_attack_frame_t *fi_a = &enemy_melee_attack_frame_info[enemies[i].anim_frame % ENEMY_MELEE_ATTACK_FRAMES];
                int fx, fy, fw, fh;
                const uint16_t *pixels;
                if (enemies[i].attacking) {
                    fx = fi_a->x; fy = fi_a->y; fw = fi_a->w; fh = fi_a->h;
                    pixels = &enemy_melee_attack_pixels[fi_a->offset];
                } else {
                    fx = fi_w->x; fy = fi_w->y; fw = fi_w->w; fh = fi_w->h;
                    pixels = &enemy_melee_walk_pixels[fi_w->offset];
                }
                float scale_x = (float)w / spr_w, scale_y = (float)h / spr_h;
                int crop_scr_w = (int)(fw * scale_x);
                int crop_scr_h = (int)(fh * scale_y);
                int x0_scr = x_s + (int)(fx * scale_x);
                int y0_scr = y_s + (int)(fy * scale_y);
                for (int dy = 0; dy < crop_scr_h; dy++) {
                    int py = y0_scr + dy;
                    if (py < 0 || py >= SCREEN_HEIGHT) continue;
                    int texY = dy * fh / crop_scr_h;
                    for (int dx = 0; dx < crop_scr_w; dx++) {
                        int px = x0_scr + dx;
                        if (px < 0 || px >= SCREEN_WIDTH) continue;
                        if (ty >= zBuffer[px]) continue;
                        int texX = dx * fw / crop_scr_w;
                        uint16_t color = pixels[texY * fw + texX];
                        if (color != ENEMY_MELEE_WALK_TRANSPARENT)
                            vram[py * SCREEN_WIDTH + px] = color;
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
            int bsy = SCREEN_HEIGHT / 2;
            if(bsx >= 0 && bsx < SCREEN_WIDTH && tby < zBuffer[bsx]) drect(bsx-1, bsy-1, bsx+1, bsy+1, C_WHITE);
        }
    }

    int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2;
    dline(cx - 4, cy, cx + 4, cy, C_WHITE);
    dline(cx, cy - 4, cx, cy + 4, C_WHITE);

    // shoot effect
    if (shootEffectTimer > 0) {
        int frame = SHOOT_EFFECT_FRAMES - shootEffectTimer;
        if (frame < 0) frame = 0;
        if (frame >= SHOOT_EFFECT_FRAMES) frame = SHOOT_EFFECT_FRAMES - 1;
        const shoot_effect_frame_t *fi = &shoot_effect_frame_info[frame];
        const uint16_t *src = &shoot_effect_pixels[fi->offset];
        int x0 = cx - SHOOT_EFFECT_WIDTH / 2 + fi->x, y0 = (SCREEN_HEIGHT - SHOOT_EFFECT_HEIGHT) / 2 + fi->y;
        for (int dy = 0; dy < fi->h; dy++) {
            for (int dx = 0; dx < fi->w; dx++) {
                uint16_t color = src[dy * fi->w + dx];
                if (color != SHOOT_EFFECT_TRANSPARENT) {
                    int px = x0 + dx, py = y0 + dy;
                    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT)
                        vram[py * SCREEN_WIDTH + px] = color;
                }
            }
        }
    }

    // gun rendering
    {
        int gun_frame;
        const gun_idle_frame_t *fi_idle;
        const gun_shoot_frame_t *fi_shoot;
        const uint16_t *src;
        int x0, y0, fw, fh;
        uint16_t transparent_key;

        if (gunShootTimer > 0) {
            int t = GUN_SHOOT_DURATION - gunShootTimer - 1;
            gun_frame = t * GUN_SHOOT_FRAMES / GUN_SHOOT_DURATION;
            if (gun_frame < 0) gun_frame = 0;
            if (gun_frame >= GUN_SHOOT_FRAMES) gun_frame = GUN_SHOOT_FRAMES - 1;
            fi_shoot = &gun_shoot_frame_info[gun_frame];
            src = &gun_shoot_pixels[fi_shoot->offset];
            x0 = SCREEN_WIDTH - GUN_SHOOT_WIDTH + fi_shoot->x;
            y0 = SCREEN_HEIGHT - GUN_SHOOT_HEIGHT + fi_shoot->y;
            fw = fi_shoot->w;
            fh = fi_shoot->h;
            transparent_key = GUN_SHOOT_TRANSPARENT;
        } else {
            gun_frame = gunIdleAnimFrame % GUN_IDLE_FRAMES;
            fi_idle = &gun_idle_frame_info[gun_frame];
            src = &gun_idle_pixels[fi_idle->offset];
            x0 = SCREEN_WIDTH - GUN_IDLE_WIDTH + fi_idle->x;
            y0 = SCREEN_HEIGHT - GUN_IDLE_HEIGHT + fi_idle->y;
            fw = fi_idle->w;
            fh = fi_idle->h;
            transparent_key = GUN_IDLE_TRANSPARENT;
        }
        for (int dy = 0; dy < fh; dy++) {
            for (int dx = 0; dx < fw; dx++) {
                uint16_t color = src[dy * fw + dx];
                if (color != transparent_key) {
                    int px = x0 + dx, py = y0 + dy;
                    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT)
                        vram[py * SCREEN_WIDTH + px] = color;
                }
            }
        }
    }

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

        if(keydown(KEY_MENU)) {
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

bool show_controls_selection() {
    uint16_t *vram = gint_vram;
    int current = selectedPreset;
    clearevents();
    while(1) {
        const uint16_t *img;
        if (current == 1) img = preset1;
        else if (current == 2) img = preset2;
        else img = preset3;

        for(int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
            vram[i] = img[i];
        }
        dupdate();

        while(1) {
            entropy_seed++;
            if(keydown(KEY_MENU)) return false;
            if(keydown(KEY_LEFT)) {
                current--;
                if(current < 1) current = 3;
                while(keydown(KEY_LEFT)) clearevents();
                break;
            }
            if(keydown(KEY_RIGHT)) {
                current++;
                if(current > 3) current = 1;
                while(keydown(KEY_RIGHT)) clearevents();
                break;
            }
            if(keydown(KEY_F6)) {
                selectedPreset = current;
                while(keydown(KEY_F6)) clearevents();
                return true;
            }
            clearevents();
        }
    }
}

int main(void) {
    main_menu:
    while(1) {
        // Initial seed with RTC as a fallback
        srand(rtc_ticks());

        if (!show_splash(startscreen)) return 0;
        if (!show_controls_selection()) return 0;

        // Re-seed using the entropy gathered during the splash screens
        srand(rtc_ticks() ^ entropy_seed);

        currentLevel = 1;
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
                        enemies[actualEnemyCount].speed = 0.08f + (rand() % 10) * 0.01f;
                        enemies[actualEnemyCount].mode = 0; // 0 = idle, 1 = hunt, 2 = lunge
                        enemies[actualEnemyCount].attacking = false;
                        enemies[actualEnemyCount].anim_frame = 0;
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
                if (shootEffectTimer > 0) shootEffectTimer--;
                if (gunShootTimer > 0) gunShootTimer--;
                static int gunIdleTick = 0;
                gunIdleTick++;
                if (gunShootTimer == 0 && (gunIdleTick % 8) == 0) gunIdleAnimFrame++;

                // update bullet if active
                if(bullet.active) {
                    bullet.x += bullet.dx * 0.7f;
                    bullet.y += bullet.dy * 0.7f;
                    int bx = (int)bullet.x, by = (int)bullet.y;
                    if(bx < 0 || bx >= MAP_WIDTH || by < 0 || by >= MAP_HEIGHT || worldMap[bx][by] == 1) {
                        bullet.active = false;
                    } else {
                        for(int i = 0; i < actualEnemyCount; i++) {
                            if(!enemies[i].alive) continue;
                            
                            float dx = bullet.x - enemies[i].x;
                            float dy = bullet.y - enemies[i].y;
                            float distSq = dx*dx + dy*dy;
                            
                            // broad-phase: check if bullet is within a reasonable distance (0.6 world units)
                            if(distSq < 0.36f) {
                                // narrow-phase: pixel-perfect billboard collision
                                // vector from player to enemy
                                float pex = enemies[i].x - posX;
                                float pey = enemies[i].y - posY;
                                float peDist = sqrtf(pex*pex + pey*pey);
                                
                                // perpendicular vector for billboard width
                                float perpX = -pey / peDist;
                                float perpY = pex / peDist;
                                
                                // project bullet offset onto perp vector
                                float offset = dx * perpX + dy * perpY;
                                
                                // hitbox logic
                                float worldW = (float)ENEMY_MELEE_WALK_WIDTH / ENEMY_MELEE_WALK_HEIGHT;
                                if (fabsf(offset) < worldW / 2.0f) {
                                    int texX = (int)((offset / worldW + 0.5f) * ENEMY_MELEE_WALK_WIDTH);
                                    if (texX >= 0 && texX < ENEMY_MELEE_WALK_WIDTH) {
                                        const enemy_melee_walk_frame_t *hb = &enemy_melee_walk_frame_info[0];
                                        bool hit = false;
                                        if (texX >= hb->x && texX < hb->x + hb->w) {
                                            int px = texX - hb->x;
                                            for (int ty = hb->y; ty < hb->y + hb->h; ty++) {
                                                int py = ty - hb->y;
                                                if (enemy_melee_walk_pixels[hb->offset + py * hb->w + px] != ENEMY_MELEE_WALK_TRANSPARENT) {
                                                    hit = true;
                                                    break;
                                                }
                                            }
                                        }
                                        
                                        if (hit) {
                                            enemies[i].alive = false; 
                                            bullet.active = false; 
                                            playerHP += 35.0f;
                                            if (playerHP > maxHP) playerHP = maxHP;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

            // enemy AI logic
            bool playerSlowed = false;
            static int enemy_anim_tick = 0;
            enemy_anim_tick++;
            for(int i = 0; i < actualEnemyCount; i++) {
                if(!enemies[i].alive) continue;

                float edx = posX - enemies[i].x;
                float edy = posY - enemies[i].y;
                float distSq = edx * edx + edy * edy;

                enemies[i].attacking = (distSq < 0.49f);  // attack animation

                if(enemies[i].mode == 0 && distSq < 64.0f) { // 8 blocks aggro
                    enemies[i].mode = 1;
                }

                // lunge logic (postvoid attack_run)
                if(enemies[i].mode == 1 && distSq < 9.0f) { // 3 blocks lunge range
                    enemies[i].mode = 2; // lunge mode
                } else if(enemies[i].mode == 2 && distSq > 16.0f) { // exit lunge if too far
                    enemies[i].mode = 1;
                }

                if(enemies[i].mode >= 1) {
                    float dist = sqrtf(distSq);
                    if(dist > 0.1f) {
                        float speed = (enemies[i].mode == 2) ? enemies[i].speed * 2.0f : enemies[i].speed;
                        float moveX = (edx / dist) * speed;
                        float moveY = (edy / dist) * speed;
                        
                        // player hitbox (stop enemy from entering player and becoming invisible)
                        float nextX = enemies[i].x + moveX;
                        float nextY = enemies[i].y + moveY;
                        float n_edx = posX - nextX;
                        float n_edy = posY - nextY;
                        if(n_edx*n_edx + n_edy*n_edy > 0.25f) { // keep 0.5 distance
                            if(worldMap[(int)nextX][(int)enemies[i].y] != 1) enemies[i].x = nextX;
                            if(worldMap[(int)enemies[i].x][(int)nextY] != 1) enemies[i].y = nextY;
                        } else {
                            // if colliding with player slow player down
                            playerSlowed = true;
                        }
                    }
                    if(distSq < 0.49f) { // damage player if close (0.7 blocks)
                        playerHP -= 1.0f;
                    }
                }

                // animation speed
                if (enemy_anim_tick % 4 == 0) {
                    int nf = enemies[i].attacking ? ENEMY_MELEE_ATTACK_FRAMES : ENEMY_MELEE_WALK_FRAMES;
                    enemies[i].anim_frame = (enemies[i].anim_frame + 1) % nf;
                }
            }

                render();

                if(keydown(KEY_MENU)) return 0;

                float moveStep = 0.25f; // base movement
                if (playerSlowed) moveStep *= 0.25f; // slow down factor
                float rotStep = 0.12f; // faster rotation

                bool moveFwd = false, moveBack = false, strafeLeft = false, strafeRight = false;
                bool rotateLeft = false, rotateRight = false, shoot = false;

                if (selectedPreset == 1) {
                    moveFwd = keydown(KEY_8);
                    moveBack = keydown(KEY_5);
                    strafeLeft = keydown(KEY_4);
                    strafeRight = keydown(KEY_6);
                    rotateLeft = keydown(KEY_LEFT);
                    rotateRight = keydown(KEY_RIGHT);
                    shoot = keydown(KEY_F6);
                } else if (selectedPreset == 2) {
                    moveFwd = keydown(KEY_OPTN);
                    moveBack = keydown(KEY_X2);
                    strafeLeft = keydown(KEY_ALPHA);
                    strafeRight = keydown(KEY_POWER);
                    rotateLeft = keydown(KEY_LEFT);
                    rotateRight = keydown(KEY_RIGHT);
                    shoot = keydown(KEY_F6);
                } else if (selectedPreset == 3) {
                    moveFwd = keydown(KEY_UP);
                    moveBack = keydown(KEY_DOWN);
                    strafeLeft = keydown(KEY_LEFT);
                    strafeRight = keydown(KEY_RIGHT);
                    rotateLeft = keydown(KEY_SHIFT);
                    rotateRight = keydown(KEY_OPTN);
                    shoot = keydown(KEY_F1);
                }

                if(moveFwd) { if(worldMap[(int)(posX + dirX * moveStep)][(int)posY] != 1) posX += dirX * moveStep; if(worldMap[(int)posX][(int)(posY + dirY * moveStep)] != 1) posY += dirY * moveStep; }
                if(moveBack) { if(worldMap[(int)(posX - dirX * moveStep)][(int)posY] != 1) posX -= dirX * moveStep; if(worldMap[(int)posX][(int)(posY - dirY * moveStep)] != 1) posY -= dirY * moveStep; }
                if(strafeRight) { if(worldMap[(int)(posX + planeX * moveStep)][(int)posY] != 1) posX += planeX * moveStep; if(worldMap[(int)posX][(int)(posY + planeY * moveStep)] != 1) posY += planeY * moveStep; }
                if(strafeLeft) { if(worldMap[(int)(posX - planeX * moveStep)][(int)posY] != 1) posX -= planeX * moveStep; if(worldMap[(int)posX][(int)(posY - planeY * moveStep)] != 1) posY -= planeY * moveStep; }
                if(rotateRight) { float odx = dirX; dirX = dirX * cosf(rotStep) - dirY * sinf(rotStep); dirY = odx * sinf(rotStep) + dirY * cosf(rotStep); float opx = planeX; planeX = planeX * cosf(rotStep) - planeY * sinf(rotStep); planeY = opx * sinf(rotStep) + planeY * cosf(rotStep); }
                if(rotateLeft) { float odx = dirX; dirX = dirX * cosf(-rotStep) - dirY * sinf(-rotStep); dirY = odx * sinf(-rotStep) + dirY * cosf(-rotStep); float opx = planeX; planeX = planeX * cosf(-rotStep) - planeY * sinf(-rotStep); planeY = opx * sinf(-rotStep) + planeY * cosf(-rotStep); }
                
                if (selectedPreset != 3) {
                    if(keydown(KEY_UP)) { pitch += 5.0f; if (pitch > 110) pitch = 110; }
                    if(keydown(KEY_DOWN)) { pitch -= 5.0f; if (pitch < -110) pitch = -110; }
                }

                if(shoot) {
                    if (!bullet.active) {
                        bullet.x = posX + dirX * 0.2f; bullet.y = posY + dirY * 0.2f;
                        bullet.dx = dirX; bullet.dy = dirY; bullet.active = true;
                        shootEffectTimer = SHOOT_EFFECT_FRAMES;
                        gunShootTimer = GUN_SHOOT_DURATION;
                    } else if (shootEffectTimer > 0) {
                        shootEffectTimer = SHOOT_EFFECT_FRAMES;  // restart effect
                        gunShootTimer = GUN_SHOOT_DURATION;     // restart gun
                    }
                }

                // Check if player is on the exit area (tile type 2)
                if (worldMap[(int)posX][(int)posY] == 2) {
                    if (currentLevel == NUM_LEVELS) {
                        if (!show_splash(winscreen)) return 0;
                        currentLevel = 1;
                        goto main_menu;
                    }
                    currentLevel++;
                    break; // exit inner loop to regenerate map for next level
                }

                clearevents();  // clear events
            }

            if (died) {
                if (!show_splash(deathscreen)) return 0;
                currentLevel = 1;
                // continue to generateMap() for level 1
            }
            // If not died, we just finished a level, so currentLevel was already incremented.
            // Loop continues to generateMap() for the next level.
        }
    }
    return 0;
}
