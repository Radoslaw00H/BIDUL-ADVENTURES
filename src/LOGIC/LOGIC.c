// Game code for BIDUL ADVENTURES: Full 3D FPS with enemies, projectiles, health

#include "LOGIC.h"
#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===== CLOCK =====
static LARGE_INTEGER g_freq;
static LARGE_INTEGER g_start;
static double g_tick_interval;

void clock_init(void) {
    // Get high-res timer frequency
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);
    // 165 Hz = 1/165 seconds per frame
    g_tick_interval = 1.0 / 165.0;
}

int clock_tick(void) {
    // Check if enough time has passed for next frame
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    double elapsed = (double)(now.QuadPart - g_start.QuadPart) / (double)g_freq.QuadPart;

    if (elapsed >= g_tick_interval) {
        // Advance start time by frame interval
        g_start.QuadPart += (LONGLONG)(g_tick_interval * g_freq.QuadPart);
        return 1;  // Do a frame
    }
    return 0;  // Not yet
}

// ===== WORLD =====
// Map: 0=empty, 1-8=walls
uint8_t g_world_map[WORLD_HEIGHT][WORLD_WIDTH];
Player g_player;
Entity *g_entities = NULL;
int g_entity_count = 0;
Projectile *g_projectiles = NULL;
int g_projectile_count = 0;

// ===== HUD DROPS (falling bullets from right-side block) =====
#define MAX_DROPS 128
typedef struct {
    double x;
    double y;
    double vy;    // vertical speed in screen pixels/sec
} Drop;
static Drop g_drops[MAX_DROPS];
static int g_drop_count = 0;
static double g_drop_timer = 0.0;

// Movement input state
static double g_fwd_speed = 0.0;
static double g_strafe_speed = 0.0;
static double g_turn_speed = 0.0;

// Init game
void game_init(void) {
    // Player setup
    g_player.pos.x = 128.0;
    g_player.pos.y = 128.0;
    g_player.pos.z = 0.0;
    g_player.angle_yaw = 0.0;
    g_player.angle_pitch = 0.0;
    g_player.vel.x = 0.0;
    g_player.vel.y = 0.0;
    g_player.vel.z = 0.0;
    g_player.health = 100.0;
    g_player.max_health = 100.0;
    g_player.shoot_cooldown = 0.0;
    g_player.shoot_recoil = 0.0;  // Gun recoil animation state
    
    // World map init
    memset(g_world_map, 0, sizeof(g_world_map));
    
    // Border walls
    for (int i = 0; i < WORLD_WIDTH; i++) {
        g_world_map[0][i] = 1;
        g_world_map[WORLD_HEIGHT - 1][i] = 1;
    }
    for (int i = 0; i < WORLD_HEIGHT; i++) {
        g_world_map[i][0] = 1;
        g_world_map[i][WORLD_WIDTH - 1] = 1;
    }
    
    // Maze-like walls
    for (int y = 10; y < WORLD_HEIGHT - 10; y += 35) {
        for (int x = 10; x < WORLD_WIDTH - 10; x += 40) {
            g_world_map[y][x] = 1 + ((x + y) % 7);
            g_world_map[y + 5][x + 3] = 2 + ((x + y) % 6);
        }
    }
    
    // Allocate dynamic arrays
    g_entities = (Entity*)malloc(MAX_ENTITIES * sizeof(Entity));
    g_projectiles = (Projectile*)malloc(MAX_PROJECTILES * sizeof(Projectile));
    g_entity_count = 0;
    g_projectile_count = 0;
    g_drop_count = 0;
    g_drop_timer = 0.0;
    
    // Spawn initial enemies
    srand(time(NULL));
    for (int i = 0; i < 5; i++) {
        double x = 30.0 + (rand() % 180);
        double y = 30.0 + (rand() % 180);
        spawn_enemy(x, y, 0.0);
    }
    // debug popup so user knows this binary executed
    MessageBoxA(NULL, "Game initialized - check right side!", "Debug", MB_OK);
}

void game_cleanup(void) {
    if (g_entities) free(g_entities);
    if (g_projectiles) free(g_projectiles);
    g_entities = NULL;
    g_projectiles = NULL;
}

// Spawn enemy entity with RECTANGLE dimensions
void spawn_enemy(double x, double y, double z) {
    if (g_entity_count >= MAX_ENTITIES) return;
    
    Entity *e = &g_entities[g_entity_count];
    e->pos.x = x;
    e->pos.y = y;
    e->pos.z = z;
    e->vel.x = 0.0;
    e->vel.y = 0.0;
    e->vel.z = 0.0;
    e->health = 50.0;
    e->max_health = 50.0;
    e->attack_timer = 0.0;
    e->texture_id = 3 + (rand() % 5);  // Textures 3-7
    e->ai_angle = 0.0;
    e->width = 1.5;   // Rectangle width
    e->height = 2.5;  // Rectangle height (taller than wide)
    e->bob_angle = 0.0;  // Animation bobbing angle
    g_entity_count++;
}

// Shoot projectile - RED, SLOW
void player_shoot(void) {
    if (g_projectile_count >= MAX_PROJECTILES) return;
    if (g_player.shoot_cooldown > 0.0) return;
    
    // Shoot forward
    double dx = cos(g_player.angle_yaw);
    double dy = sin(g_player.angle_yaw);
    
    Projectile *p = &g_projectiles[g_projectile_count];
    p->pos.x = g_player.pos.x + dx * 2.0;
    p->pos.y = g_player.pos.y + dy * 2.0;
    p->pos.z = g_player.pos.z + 0.5;
    p->vel.x = dx * 30.0;  // Slower bullets (was 80)
    p->vel.y = dy * 30.0;
    p->vel.z = 0.0;
    p->lifetime = 5.0;  // 5 seconds before despawn
    p->age = 0.0;
    p->color_r = 255;  // RED BULLETS
    p->color_g = 0;
    p->color_b = 0;
    g_projectile_count++;
    
    // Gun recoil animation
    g_player.shoot_recoil = 0.15;  // 150ms recoil
    g_player.shoot_cooldown = 0.15;  // 150ms between shots
}


// ===== PROCEDURAL COLORS =====
static uint32_t get_color(uint8_t texture_id, uint8_t shade) {
    shade = shade % 8;
    
    if (texture_id == 1) {
        // Stone walls - gray
        uint8_t c = 40 + (shade * 18);
        return (c << 16) | (c << 8) | c;  // BGR format
    } else if (texture_id >= 2 && texture_id <= 4) {
        // Brick walls - red/brown
        uint8_t r = 150 + (shade * 10);
        uint8_t g = 50 + (shade * 8);
        uint8_t b = 20;
        return (b << 16) | (g << 8) | r;
    } else {
        // Enemy/metal walls - blue/cyan
        uint8_t r = 80 + (shade * 12);
        uint8_t g = 100 + (shade * 12);
        uint8_t b = 120 + (shade * 10);
        return (b << 16) | (g << 8) | r;
    }
}

static uint8_t get_texture_detail(double x, double y, uint8_t texture_id) {
    // Procedural detail via hash
    int ix = (int)x;
    int iy = (int)y;
    uint32_t hash = ((ix * 3731) ^ (iy * 8377)) & 0x7;
    return hash % 3;
}

// ===== 3D RAYCASTING =====
RayHit raycast(double x, double y, double z, double angle_yaw, double angle_pitch) {
    // Cast ray in 3D with yaw/pitch
    double dx = cos(angle_yaw) * cos(angle_pitch);
    double dy = sin(angle_yaw) * cos(angle_pitch);
    double dz = sin(angle_pitch);
    
    RayHit hit = {0};
    double step = 0.15;
    double max_dist = 150.0;
    
    // Step along ray until wall
    for (double t = 0.0; t < max_dist; t += step) {
        double rx = x + dx * t;
        double ry = y + dy * t;
        
        int gx = (int)rx;
        int gy = (int)ry;
        
        // Boundary check
        if (gx < 0 || gx >= WORLD_WIDTH || gy < 0 || gy >= WORLD_HEIGHT) {
            hit.distance = t;
            hit.texture_id = 1;
            hit.hit_x = rx;
            hit.hit_y = ry;
            hit.hit_z = dz * t;
            return hit;
        }
        
        // Wall check
        if (g_world_map[gy][gx] != 0) {
            hit.distance = t;
            hit.texture_id = g_world_map[gy][gx];
            hit.hit_x = rx;
            hit.hit_y = ry;
            hit.hit_z = dz * t;
            return hit;
        }
    }
    
    // Sky miss
    hit.distance = max_dist;
    hit.texture_id = 0;
    hit.hit_z = dz * max_dist;
    return hit;
}

// ===== GAME UPDATE =====
void game_update(void) {
    double dt = 1.0 / 165.0;  // Delta time
    
    // === PLAYER PHYSICS ===
    // Apply friction
    g_fwd_speed *= 0.85;
    g_strafe_speed *= 0.85;
    g_turn_speed *= 0.90;
    
    // Movement
    double move_fwd = g_fwd_speed * dt;
    double move_strafe = g_strafe_speed * dt;
    
    double next_x = g_player.pos.x + cos(g_player.angle_yaw) * move_fwd 
                    - sin(g_player.angle_yaw) * move_strafe;
    double next_y = g_player.pos.y + sin(g_player.angle_yaw) * move_fwd 
                    + cos(g_player.angle_yaw) * move_strafe;
    
    // Collision check
    int gx = (int)next_x;
    int gy = (int)next_y;
    if (gx > 0 && gx < WORLD_WIDTH && gy > 0 && gy < WORLD_HEIGHT) {
        if (g_world_map[gy][gx] == 0) {
            g_player.pos.x = next_x;
            g_player.pos.y = next_y;
        }
    }
    
    // Rotation
    g_player.angle_yaw += g_turn_speed * dt;
    
    // Update shoot cooldown and recoil animation
    if (g_player.shoot_cooldown > 0.0) {
        g_player.shoot_cooldown -= dt;
    }
    // Recoil animation decay
    if (g_player.shoot_recoil > 0.0) {
        g_player.shoot_recoil -= dt / 0.15;  // Decay over 150ms
        if (g_player.shoot_recoil < 0.0) g_player.shoot_recoil = 0.0;
    }
    
    // === PROJECTILES ===
    // === HUD DROPS (falling bullets from right-side block) ===
    // Update existing drops
    g_drop_timer -= dt;
    for (int i = 0; i < g_drop_count; ) {
        g_drops[i].y += g_drops[i].vy * dt;
        // Remove if past bottom
        if (g_drops[i].y >= SCREEN_HEIGHT) {
            g_drops[i] = g_drops[g_drop_count - 1];
            g_drop_count--;
            continue;
        }
        i++;
    }
    // Spawn new drops from right-side block periodically
    if (g_drop_timer <= 0.0) {
        g_drop_timer = 0.10 + ((rand() % 40) / 1000.0); // ~0.10-0.14s
        if (g_drop_count < MAX_DROPS) {
            int bx = SCREEN_WIDTH - 60;
            int by = SCREEN_HEIGHT / 2 - 20;
            g_drops[g_drop_count].x = bx + (rand() % 14) - 6;
            g_drops[g_drop_count].y = by + (rand() % 8) - 4;
            g_drops[g_drop_count].vy = 80.0 + (rand() % 80); // px/sec
            g_drop_count++;
        }
    }

    // === PROJECTILES ===
    for (int i = 0; i < g_projectile_count; ) {
        Projectile *p = &g_projectiles[i];
        p->age += dt;
        
        // Update position
        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->pos.z += p->vel.z * dt;
        
        // Remove if expired or out of bounds
        if (p->age >= p->lifetime || p->pos.x < 0 || p->pos.x >= WORLD_WIDTH ||
            p->pos.y < 0 || p->pos.y >= WORLD_HEIGHT) {
            g_projectiles[i] = g_projectiles[g_projectile_count - 1];
            g_projectile_count--;
            continue;
        }
        
        // Wall collision
        int px = (int)p->pos.x;
        int py = (int)p->pos.y;
        if (g_world_map[py][px] != 0) {
            g_projectiles[i] = g_projectiles[g_projectile_count - 1];
            g_projectile_count--;
            continue;
        }
        
        // Enemy collision
        for (int j = 0; j < g_entity_count; j++) {
            Entity *e = &g_entities[j];
            double dx = p->pos.x - e->pos.x;
            double dy = p->pos.y - e->pos.y;
            double dist = sqrt(dx * dx + dy * dy);
            
            if (dist < 2.0) {  // Hit radius
                e->health -= 25.0;
                g_projectiles[i] = g_projectiles[g_projectile_count - 1];
                g_projectile_count--;
                goto next_proj;
            }
        }
        
        i++;
        next_proj:;
    }
    
    // === ENEMIES ===
    for (int i = 0; i < g_entity_count; ) {
        Entity *e = &g_entities[i];
        
        // Dead check
        if (e->health <= 0.0) {
            g_entities[i] = g_entities[g_entity_count - 1];
            g_entity_count--;
            continue;
        }
        
        // Animation: bobbing while moving
        e->bob_angle += dt * 4.0;  // Bobbing speed
        
        // AI: Chase player with dynamic movement
        double dx = g_player.pos.x - e->pos.x;
        double dy = g_player.pos.y - e->pos.y;
        double dist = sqrt(dx * dx + dy * dy);
        
        if (dist > 0.1) {
            // More dynamic chase speed: varies between 12-18
            double base_speed = 15.0;
            double bob_mod = sin(e->bob_angle) * 3.0;  // Varies chase speed
            double chase_speed = base_speed + bob_mod;
            
            e->vel.x = (dx / dist) * chase_speed;
            e->vel.y = (dy / dist) * chase_speed;
            e->ai_angle = atan2(dy, dx);
        } else {
            e->vel.x *= 0.9;
            e->vel.y *= 0.9;
        }
        
        // Update position with smooth movement
        double next_ex = e->pos.x + e->vel.x * dt;
        double next_ey = e->pos.y + e->vel.y * dt;
        
        int ex = (int)next_ex;
        int ey = (int)next_ey;
        if (ex > 0 && ex < WORLD_WIDTH && ey > 0 && ey < WORLD_HEIGHT) {
            if (g_world_map[ey][ex] == 0) {
                e->pos.x = next_ex;
                e->pos.y = next_ey;
            }
        }
        
        // Attack cooldown
        if (e->attack_timer > 0.0) {
            e->attack_timer -= dt;
        }
        
        // Attack player if close
        if (dist < 5.0 && e->attack_timer <= 0.0) {
            g_player.health -= 15.0;
            e->attack_timer = 2.0;
        }
        
        i++;
    }
    
    // Spawn new enemies if count low
    if (g_entity_count < 3 && (rand() % 100) < 2) {
        double x = 50.0 + (rand() % 160);
        double y = 50.0 + (rand() % 160);
        spawn_enemy(x, y, 0.0);
    }
    
    // Game over / respawn
    if (g_player.health <= 0.0) {
        g_player.health = 100.0;
        g_player.pos.x = 128.0;
        g_player.pos.y = 128.0;
        g_player.shoot_recoil = 0.0;
        g_entity_count = 0;
        for (int i = 0; i < 3; i++) {
            spawn_enemy(30.0 + (rand() % 180), 30.0 + (rand() % 180), 0.0);
        }
    }
}

// ===== GAME RENDER =====
void game_render(uint32_t* backbuffer, int width, int height) {
    // Clear backbuffer
    for (int i = 0; i < width * height; i++) {
        backbuffer[i] = 0x000000;
    }
    
    // Render 3D view
    for (int col = 0; col < width; col++) {
        double fov_h = 60.0 * (3.14159 / 180.0);
        double col_angle_yaw = g_player.angle_yaw + (fov_h / 2.0) - (fov_h * col / width);
        double pitch = g_player.angle_pitch;
        
        RayHit hit = raycast(g_player.pos.x, g_player.pos.y, g_player.pos.z, col_angle_yaw, pitch);
        
        if (hit.distance >= 150.0) {
            // Sky
            for (int row = 0; row < height / 2; row++) {
                backbuffer[row * width + col] = 0x6688AA;  // Blue sky BGR
            }
            // Floor
            for (int row = height / 2; row < height; row++) {
                backbuffer[row * width + col] = 0x3A5C3A;  // Green floor BGR
            }
            continue;
        }
        
        // Wall height
        double wall_height = (double)height / (hit.distance + 0.1);
        if (wall_height > height) wall_height = height;
        
        int start_row = (int)((height - wall_height) / 2);
        if (start_row < 0) start_row = 0;
        int end_row = start_row + (int)wall_height;
        if (end_row > height) end_row = height;
        
        // Texture shading
        uint8_t detail = get_texture_detail(hit.hit_x, hit.hit_y, hit.texture_id);
        uint8_t shade = 7 - (uint8_t)((hit.distance / 80.0) * 7);
        if (shade < 0) shade = 0;
        if (shade > 7) shade = 7;
        shade = (shade + detail) % 8;
        
        uint32_t color = get_color(hit.texture_id, shade);
        
        // Draw wall
        for (int row = start_row; row < end_row; row++) {
            backbuffer[row * width + col] = color;
        }
        
        // Floor and ceiling
        for (int row = 0; row < start_row; row++) {
            backbuffer[row * width + col] = 0x6688AA;
        }
        for (int row = end_row; row < height; row++) {
            backbuffer[row * width + col] = 0x3A5C3A;
        }
    }
    
    // Draw DETAILED GUN SPRITE with recoil animation
    // Recoil moves gun backward
    int recoil_offset = (int)(g_player.shoot_recoil * 15.0);  // Recoil push
    int gun_x = width / 2 - 30 - recoil_offset;
    int gun_y = height - 80;
    int gun_w = 60;
    int gun_h = 70;
    
    // GRIP - bottom dark section
    for (int y = gun_y + 50; y < gun_y + gun_h; y++) {
        for (int x = gun_x + 10; x < gun_x + 40; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                uint8_t shade = (y - (gun_y + 50)) * 3;  // Gradient in grip
                uint32_t grip_col = (shade << 16) | (shade << 8) | shade;  // Dark gray BGR
                backbuffer[y * width + x] = grip_col;
            }
        }
    }
    
    // TRIGGER GUARD - curve around trigger
    for (int y = gun_y + 35; y < gun_y + 50; y++) {
        for (int x = gun_x + 15; x < gun_x + 35; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                backbuffer[y * width + x] = 0x2A2A2A;  // Very dark gray BGR
            }
        }
    }
    
    // TRIGGER - small red rectangle
    for (int y = gun_y + 40; y < gun_y + 48; y++) {
        for (int x = gun_x + 22; x < gun_x + 28; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                backbuffer[y * width + x] = 0x0000AA;  // Red trigger BGR
            }
        }
    }
    
    // SLIDE - main body
    for (int y = gun_y + 10; y < gun_y + 45; y++) {
        for (int x = gun_x + 5; x < gun_x + 55; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                uint8_t c = 80 + ((x - gun_x) % 8) * 3;  // Metal texture
                uint32_t col = (c << 16) | ((c - 20) << 8) | (c - 30);  // Dark metallic BGR\n                backbuffer[y * width + x] = col;
            }
        }
    }
    
    // BARREL - bright metallic tube
    for (int y = gun_y + 12; y < gun_y + 28; y++) {
        for (int x = gun_x + 48; x < gun_x + 58; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                uint8_t bright = 180 + ((x - (gun_x + 48)) % 4) * 10;  // Shiny
                uint32_t barrel_col = (bright << 16) | ((bright - 20) << 8) | (bright - 40);  // Bright gray BGR
                backbuffer[y * width + x] = barrel_col;
            }
        }
    }
    
    // BARREL OPENING - dark hole
    for (int y = gun_y + 14; y < gun_y + 26; y++) {
        for (int x = gun_x + 56; x < gun_x + 60; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                backbuffer[y * width + x] = 0x000000;  // Black opening BGR
            }
        }
    }
    
    // FRONT SIGHT - small post
    for (int y = gun_y + 8; y < gun_y + 15; y++) {
        for (int x = gun_x + 42; x < gun_x + 46; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                backbuffer[y * width + x] = 0x0000FF;  // Red sight BGR
            }
        }
    }
    
    // REAR SIGHT - small post
    for (int y = gun_y + 5; y < gun_y + 12; y++) {
        for (int x = gun_x + 18; x < gun_x + 22; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                backbuffer[y * width + x] = 0x00FF00;  // Green rear sight BGR
            }
        }
    }
    
    // MUZZLE FLASH - when just fired
    if (g_player.shoot_recoil > 0.05) {
        // Draw orange/yellow flash
        for (int y = gun_y + 13; y < gun_y + 27; y++) {
            for (int x = gun_x + 58; x < gun_x + 65; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    double flash_intensity = g_player.shoot_recoil * 2.0;  // Fade over time
                    uint8_t flash_r = (uint8_t)(255.0 * flash_intensity);
                    uint8_t flash_g = (uint8_t)(200.0 * flash_intensity * 0.8);
                    uint8_t flash_b = 0;
                    uint32_t flash = (flash_b << 16) | (flash_g << 8) | flash_r;  // Orange BGR
                    backbuffer[y * width + x] = flash;
                }
            }
        }
    }
    
    // Draw right-side protruding block (simple HUD element) - make it large and bright
    int block_w = 96;
    int block_h = 120;
    int block_x = width - block_w - 8;
    int block_y = height / 2 - (block_h / 2);
    // Block fill (bright magenta) for visibility
    for (int y = block_y; y < block_y + block_h; y++) {
        for (int x = block_x; x < block_x + block_w; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                backbuffer[y * width + x] = 0xFF00FF; // Magenta BGR
            }
        }
    }
    // Border (black) on left/top/bottom to make it pop
    for (int y = block_y; y < block_y + block_h; y++) {
        int x = block_x;
        if (x >= 0 && x < width && y >= 0 && y < height) backbuffer[y * width + x] = 0x000000;
        x = block_x + block_w - 1;
        if (x >= 0 && x < width && y >= 0 && y < height) backbuffer[y * width + x] = 0x000000;
    }
    for (int x = block_x; x < block_x + block_w; x++) {
        int y = block_y;
        if (x >= 0 && x < width && y >= 0 && y < height) backbuffer[y * width + x] = 0x000000;
        y = block_y + block_h - 1;
        if (x >= 0 && x < width && y >= 0 && y < height) backbuffer[y * width + x] = 0x000000;
    }
    // Small muzzle at block front (at right edge)
    for (int y = block_y + block_h/2 - 8; y < block_y + block_h/2 + 8; y++) {
        for (int x = block_x + block_w; x < block_x + block_w + 10; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                backbuffer[y * width + x] = 0x000000;
            }
        }
    }

    // Draw falling drops (small bullets) from the block
    for (int i = 0; i < g_drop_count; i++) {
        int dx = (int)g_drops[i].x;
        int dy = (int)g_drops[i].y;
        for (int yy = dy; yy < dy + 3; yy++) {
            for (int xx = dx; xx < dx + 3; xx++) {
                if (xx >= 0 && xx < width && yy >= 0 && yy < height) {
                    backbuffer[yy * width + xx] = 0x00AAFF; // Yellowish-ish BGR
                }
            }
        }
    }
    
    // Draw HEALTH BAR at top center
    int bar_width = 200;
    int bar_height = 20;
    int bar_x = (width - bar_width) / 2;
    int bar_y = 10;

    // *** DEBUG OVERLAY: large magenta block covering right half ***
    for (int y = 0; y < height; y++) {
        for (int x = width/2; x < width; x++) {
            backbuffer[y * width + x] = 0xFF00FF; // magenta
        }
    }
    
    // Black border
    for (int y = bar_y; y < bar_y + bar_height; y++) {
        for (int x = bar_x; x < bar_x + bar_width; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                backbuffer[y * width + x] = 0x000000;  // Black BGR
            }
        }
    }
    
    // Health fill (green to red)
    double health_ratio = g_player.health / g_player.max_health;
    int health_width = (int)(bar_width * health_ratio);
    
    for (int y = bar_y + 1; y < bar_y + bar_height - 1; y++) {
        for (int x = bar_x + 1; x < bar_x + health_width; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                // Green to red gradient
                uint8_t r = (uint8_t)(255.0 * (1.0 - health_ratio));
                uint8_t g = (uint8_t)(255.0 * health_ratio);
                uint8_t b = 0;
                backbuffer[y * width + x] = (b << 16) | (g << 8) | r;  // BGR
            }
        }
    }
}
// ===== INPUT HANDLING =====
void handle_key_input(uint8_t key, int is_down) {
    // Reduced sensitivity - smoother controls
    double accel = 35.0;
    double turn_accel = 3.0;
    
    if (is_down) {
        // Forward/backward
        if (key == 'W' || key == 'w') g_fwd_speed = accel;
        if (key == 'S' || key == 's') g_fwd_speed = -accel;
        // Strafe left/right
        if (key == 'A' || key == 'a') g_strafe_speed = accel;
        if (key == 'D' || key == 'd') g_strafe_speed = -accel;
        // Rotate with arrows
        if (key == VK_LEFT) g_turn_speed = turn_accel;
        if (key == VK_RIGHT) g_turn_speed = -turn_accel;
        // Shoot with SPACE or ARROW UP
        if (key == VK_SPACE || key == VK_UP) player_shoot();
    } else {
        // Release keys - smooth deceleration
        if (key == 'W' || key == 'w' || key == 'S' || key == 's') g_fwd_speed = 0.0;
        if (key == 'A' || key == 'a' || key == 'D' || key == 'd') g_strafe_speed = 0.0;
        if (key == VK_LEFT || key == VK_RIGHT) g_turn_speed = 0.0;
    }
}

// Dummy tick function
void tick(void) {
    // Logic handled in game_update
}