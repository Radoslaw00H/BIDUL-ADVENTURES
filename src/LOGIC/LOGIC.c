// Game code for BIDUL ADVENTURES: Full 3D FPS with enemies, projectiles, health
// [LIBRARIES] ------------------------------------------------------------------------------------------
#include "LOGIC.h"      // Example on how you should comment every line
#include <windows.h>    // Windows APT library
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

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

// Movement input state - key press tracking for parallel input
static int g_key_w = 0, g_key_a = 0, g_key_s = 0, g_key_d = 0;
static int g_key_left = 0, g_key_right = 0;
static int g_key_space = 0, g_key_up = 0;

// Game state
static int g_is_dead = 0;
static double g_death_timer = 0.0;

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
        spawn_normal_enemy(x, y, 0.0);
    }
    // Add red aggressive enemies
    for (int i = 0; i < 2; i++) {
        double x = 40.0 + (rand() % 160);
        double y = 40.0 + (rand() % 160);
        spawn_red_enemy(x, y, 0.0);
    }
    // Add one boss
    spawn_boss(128.0, 80.0, 0.0);
    // debug popup so user knows this binary executed
    MessageBoxA(NULL, "Game initialized - check right side!", "Debug", MB_OK);
}

void game_cleanup(void) {
    if (g_entities) free(g_entities);
    if (g_projectiles) free(g_projectiles);
    g_entities = NULL;
    g_projectiles = NULL;
}

// Spawn enemy entity with type (0=normal, 1=red aggressive, 2=boss)
void spawn_enemy(double x, double y, double z, uint8_t type) {
    if (g_entity_count >= MAX_ENTITIES) return;
    
    Entity *e = &g_entities[g_entity_count];
    e->pos.x = x;
    e->pos.y = y;
    e->pos.z = z;
    e->vel.x = 0.0;
    e->vel.y = 0.0;
    e->vel.z = 0.0;
    e->type = type;
    e->ai_angle = 0.0;
    e->bob_angle = 0.0;
    e->attack_timer = 0.0;
    
    if (type == 2) {
        // BOSS: 4x bigger, 50x health (2500), heavy damage (25% HP = 25 damage)
        e->health = 2500.0;
        e->max_health = 2500.0;
        e->width = 6.0;    // 4x the normal 1.5
        e->height = 10.0;  // 4x the normal 2.5
        e->texture_id = 7; // Unique boss appearance
    } else if (type == 1) {
        // RED AGGRESSIVE: Normal size but faster shooting
        e->health = 50.0;
        e->max_health = 50.0;
        e->width = 1.5;
        e->height = 2.5;
        e->texture_id = 1; // Red color (stone walls = gray, but we'll render as red)
    } else {
        // NORMAL: Standard enemy
        e->health = 50.0;
        e->max_health = 50.0;
        e->width = 1.5;
        e->height = 2.5;
        e->texture_id = 3 + (rand() % 5);  // Textures 3-7
    }
    
    g_entity_count++;
}

// Spawn normal enemy
void spawn_normal_enemy(double x, double y, double z) {
    spawn_enemy(x, y, z, 0);
}

// Spawn red aggressive enemy
void spawn_red_enemy(double x, double y, double z) {
    spawn_enemy(x, y, z, 1);
}

// Spawn boss enemy
void spawn_boss(double x, double y, double z) {
    spawn_enemy(x, y, z, 2);
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


// ===== PROCEDURAL COLORS WITH ADVANCED LIGHTING =====
static uint32_t get_color(uint8_t texture_id, uint8_t shade) {
    shade = shade % 8;
    
    if (texture_id == 1) {
        // Stone walls - gray with complex detail
        uint8_t base = 40 + (shade * 18);
        // Add some variation for detail
        uint8_t detail_variation = shade > 4 ? 15 : 5;
        uint8_t c = base + detail_variation;
        if (c > 255) c = 255;
        return (c << 16) | (c << 8) | c;  // BGR format
    } else if (texture_id >= 2 && texture_id <= 4) {
        // Brick walls - red/brown with specular highlights
        uint8_t base_r = 150 + (shade * 10);
        uint8_t base_g = 50 + (shade * 8);
        uint8_t base_b = 20;
        // Add highlights for lit areas
        if (shade > 5) {
            base_r = (base_r * 110) / 100;
            base_g = (base_g * 110) / 100;
        }
        if (base_r > 255) base_r = 255;
        if (base_g > 255) base_g = 255;
        return (base_b << 16) | (base_g << 8) | base_r;
    } else {
        // Enemy/metal walls - blue/cyan with metallic sheen
        uint8_t base_r = 80 + (shade * 12);
        uint8_t base_g = 100 + (shade * 12);
        uint8_t base_b = 120 + (shade * 10);
        // Metallic reflection on lit surfaces
        if (shade > 5) {
            base_r = (base_r * 115) / 100;
            base_b = (base_b * 115) / 100;
        }
        if (base_r > 255) base_r = 255;
        if (base_b > 255) base_b = 255;
        return (base_b << 16) | (base_g << 8) | base_r;
    }
}

static uint8_t get_texture_detail(double x, double y, uint8_t texture_id) {
    // Procedural detail via hash with more variation
    int ix = (int)x;
    int iy = (int)y;
    uint32_t hash = ((ix * 3731) ^ (iy * 8377)) & 0x7;
    // Different detail patterns for different textures
    return (hash + texture_id) % 4;
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
    // Build velocity from parallel key inputs
    double move_speed = 40.0;  // Units per second
    double turn_speed = 3.0;   // Radians per second
    
    double move_fwd = 0.0;
    double move_strafe = 0.0;
    double move_turn = 0.0;
    
    // Accumulate movement from all active keys
    if (g_key_w) move_fwd += move_speed;
    if (g_key_s) move_fwd -= move_speed;
    if (g_key_d) move_strafe -= move_speed;
    if (g_key_a) move_strafe += move_speed;
    if (g_key_left) move_turn += turn_speed;
    if (g_key_right) move_turn -= turn_speed;
    
    // Apply movement
    double move_fwd_delta = move_fwd * dt;
    double move_strafe_delta = move_strafe * dt;
    
    double next_x = g_player.pos.x + cos(g_player.angle_yaw) * move_fwd_delta 
                    - sin(g_player.angle_yaw) * move_strafe_delta;
    double next_y = g_player.pos.y + sin(g_player.angle_yaw) * move_fwd_delta 
                    + cos(g_player.angle_yaw) * move_strafe_delta;
    
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
    g_player.angle_yaw += move_turn * dt;
    
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
        
        // Check if enemy projectile (green) hits player
        if (p->color_g == 255) {  // Green = enemy fire
            double pdx = p->pos.x - g_player.pos.x;
            double pdy = p->pos.y - g_player.pos.y;
            double pdist = sqrt(pdx * pdx + pdy * pdy);
            if (pdist < 2.0) {
                g_player.health -= 15.0;
                g_projectiles[i] = g_projectiles[g_projectile_count - 1];
                g_projectile_count--;
                continue;
            }
        }
        
        // Check if player projectile (red) hits enemies
        if (p->color_r == 255) {  // Red = player fire
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
            double chase_speed = 5.5;  // Default
            
            if (e->type == 2) {
                // BOSS: Fast movement 15-20 units/sec
                double bob_mod = sin(e->bob_angle) * 2.0;
                chase_speed = 17.0 + bob_mod;
            } else if (e->type == 1) {
                // RED AGGRESSIVE: Medium speed 7-10 units/sec
                double bob_mod = sin(e->bob_angle) * 1.5;
                chase_speed = 8.5 + bob_mod;
            } else {
                // NORMAL: Slow 4-7 units/sec
                double bob_mod = sin(e->bob_angle) * 1.5;
                chase_speed = 5.5 + bob_mod;
            }
            
            e->vel.x = (dx / dist) * chase_speed;
            e->vel.y = (dy / dist) * chase_speed;
            e->ai_angle = atan2(dy, dx);
        } else {
            e->vel.x *= 0.92;
            e->vel.y *= 0.92;
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
        
        // Attack player if close - shoot projectiles with inaccuracy
        double attack_range = 60.0;
        if (e->type == 2) attack_range = 80.0;  // Boss can shoot from further
        
        if (dist < attack_range && e->attack_timer <= 0.0) {
            // Determine attack parameters based on type
            double inaccuracy_amount = 0.3;  // Default
            double shot_speed = 20.0;
            double cooldown = 1.5;
            double damage = 10.0;
            
            if (e->type == 2) {
                // BOSS: High accuracy, fast shots, rapid fire, huge damage
                inaccuracy_amount = 0.1;  // Better accuracy
                shot_speed = 30.0;        // Faster bullets
                cooldown = 0.8;           // Rapid fire
                damage = 15.0;            // Higher damage per shot
            } else if (e->type == 1) {
                // RED AGGRESSIVE: Good accuracy, fast fire
                inaccuracy_amount = 0.2;
                shot_speed = 25.0;
                cooldown = 1.0;           // Faster than normal
                damage = 10.0;
            } else {
                // NORMAL: Standard
                inaccuracy_amount = 0.3;
                shot_speed = 20.0;
                cooldown = 1.5;
                damage = 10.0;
            }
            
            // Add inaccuracy to enemy shots
            double inaccuracy = (rand() % 100) / 100.0 * inaccuracy_amount;
            double shoot_angle = e->ai_angle + (inaccuracy - inaccuracy_amount/2.0);
            
            // Enemy shoots green projectile
            if (g_projectile_count < MAX_PROJECTILES) {
                Projectile *ep = &g_projectiles[g_projectile_count];
                ep->pos.x = e->pos.x + cos(shoot_angle) * 1.5;
                ep->pos.y = e->pos.y + sin(shoot_angle) * 1.5;
                ep->pos.z = e->pos.z + 0.5;
                ep->vel.x = cos(shoot_angle) * shot_speed;
                ep->vel.y = sin(shoot_angle) * shot_speed;
                ep->vel.z = 0.0;
                ep->lifetime = 5.0;
                ep->age = 0.0;
                ep->color_r = 0;    // GREEN: indicate enemy fire
                ep->color_g = 255;
                ep->color_b = 0;
                g_projectile_count++;
            }
            
            // Direct damage if too close (only bosses do this)
            if (e->type == 2 && dist < 8.0) {
                g_player.health -= 25.0;  // Boss deals 25% HP per close hit
            }
            e->attack_timer = cooldown;
        }
        
        i++;
    }
    
    // Spawn new enemies if count low
    if (g_entity_count < 3 && (rand() % 100) < 2) {
        double x = 50.0 + (rand() % 160);
        double y = 50.0 + (rand() % 160);
        spawn_normal_enemy(x, y, 0.0);
    }
    
    // Game over / respawn
    if (g_player.health <= 0.0 && !g_is_dead) {
        g_is_dead = 1;
        g_death_timer = 0.0;
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
            // Deep evening orange sky
            for (int row = 0; row < height / 2; row++) {
                double t = (double)row / (height / 2.0);
                uint8_t r = (uint8_t)(255.0 * (1.0 - t * 0.5));        // 255 to 127
                uint8_t g = (uint8_t)(150.0 * (1.0 - t * 0.7));        // 150 to 45
                uint8_t b = (uint8_t)(60.0 * (1.0 - t * 0.9));         // 60 to 6
                backbuffer[row * width + col] = (b << 16) | (g << 8) | r;
            }
            // Floor
            for (int row = height / 2; row < height; row++) {
                backbuffer[row * width + col] = 0x3A5C3A;  // Green floor BGR
            }
            continue;
        }
        
        // Draw sky first (will be overwritten by sun/moon)
        for (int row = 0; row < height / 2; row++) {
            double t = (double)row / (height / 2.0);
            uint8_t r = (uint8_t)(255.0 * (1.0 - t * 0.5));
            uint8_t g = (uint8_t)(150.0 * (1.0 - t * 0.7));
            uint8_t b = (uint8_t)(60.0 * (1.0 - t * 0.9));
            backbuffer[row * width + col] = (b << 16) | (g << 8) | r;
        }
        
        // Wall height
        double wall_height = (double)height / (hit.distance + 0.1);
        if (wall_height > height) wall_height = height;
        
        int start_row = (int)((height - wall_height) / 2);
        if (start_row < 0) start_row = 0;
        int end_row = start_row + (int)wall_height;
        if (end_row > height) end_row = height;
        
        // Texture shading with fog effect
        uint8_t detail = get_texture_detail(hit.hit_x, hit.hit_y, hit.texture_id);
        uint8_t shade = 7 - (uint8_t)((hit.distance / 80.0) * 7);
        if (shade < 0) shade = 0;
        if (shade > 7) shade = 7;
        
        // Apply detail variation
        shade = (shade + detail) % 8;
        
        uint32_t color = get_color(hit.texture_id, shade);
        
        // Apply distance fog for atmospheric effect
        double fog_factor = (hit.distance / 150.0);  // Full fog at max distance
        if (fog_factor > 1.0) fog_factor = 1.0;
        
        // Blend color towards sky color at distance (evening orange)
        uint8_t fog_r = (uint8_t)(255.0 * (1.0 - fog_factor * 0.5));
        uint8_t fog_g = (uint8_t)(150.0 * (1.0 - fog_factor * 0.7));
        uint8_t fog_b = (uint8_t)(60.0 * (1.0 - fog_factor * 0.9));
        
        uint8_t col_r = ((color >> 0) & 0xFF);
        uint8_t col_g = ((color >> 8) & 0xFF);
        uint8_t col_b = ((color >> 16) & 0xFF);
        
        col_r = (uint8_t)(col_r * (1.0 - fog_factor) + fog_r * fog_factor);
        col_g = (uint8_t)(col_g * (1.0 - fog_factor) + fog_g * fog_factor);
        col_b = (uint8_t)(col_b * (1.0 - fog_factor) + fog_b * fog_factor);
        
        color = (col_b << 16) | (col_g << 8) | col_r;
        
        // Draw wall
        for (int row = start_row; row < end_row; row++) {
            backbuffer[row * width + col] = color;
        }
        
        // Floor and ceiling
        for (int row = 0; row < start_row; row++) {
            double t = (double)row / (height / 2.0);
            uint8_t r = (uint8_t)(255.0 * (1.0 - t * 0.5));
            uint8_t g = (uint8_t)(150.0 * (1.0 - t * 0.7));
            uint8_t b = (uint8_t)(60.0 * (1.0 - t * 0.9));
            backbuffer[row * width + col] = (b << 16) | (g << 8) | r;
        }
        for (int row = end_row; row < height; row++) {
            backbuffer[row * width + col] = 0x3A5C3A;
        }
    }
    
    // Draw PROJECTILES as 3D-like objects (visible bullets)
    for (int i = 0; i < g_projectile_count; i++) {
        Projectile *p = &g_projectiles[i];
        // Simple 3D projection to screen
        double dx = p->pos.x - g_player.pos.x;
        double dy = p->pos.y - g_player.pos.y;
        double dist = sqrt(dx * dx + dy * dy);
        
        if (dist < 150.0 && dist > 0.1) {
            double angle_to_proj = atan2(dy, dx);
            double rel_angle = angle_to_proj - g_player.angle_yaw;
            
            // Normalize angle
            while (rel_angle > 3.14159) rel_angle -= 2*3.14159;
            while (rel_angle < -3.14159) rel_angle += 2*3.14159;
            
            double fov_h = 60.0 * (3.14159 / 180.0);
            double screen_x = width / 2.0 - (rel_angle / (fov_h / 2.0)) * (width / 2.0);
            double wall_height = (double)height / (dist + 0.1);
            
            if (screen_x >= 0 && screen_x < width) {
                int px = (int)screen_x;
                // Make bullets more visible - bigger size, especially for enemy fire
                int psize = (int)(10.0 / (dist / 10.0));  // Bigger than before
                if (p->color_g == 255) {  // Enemy bullets
                    psize += 2;  // Make enemy bullets extra visible
                }
                if (psize < 3) psize = 3;
                
                for (int py = 0; py < psize; py++) {
                    for (int px2 = 0; px2 < psize; px2++) {
                        int sy = (int)(height / 2.0) + py - psize/2;
                        int sx = px + px2 - psize/2;
                        if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                            uint32_t proj_color = (p->color_b << 16) | (p->color_g << 8) | p->color_r;
                            // Make enemy projectiles brighter by boosting green
                            if (p->color_g == 255) {
                                proj_color = 0xFF0000; // Bright lime green in BGR
                            }
                            backbuffer[sy * width + sx] = proj_color;
                        }
                    }
                }
                
                // Draw trailing glow
                for (int py = -5; py <= 5; py++) {
                    int sy = (int)(height / 2.0) + py;
                    int sx = (int)screen_x;
                    if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                        uint8_t glow = (uint8_t)(p->color_g * (5 - abs(py)) / 5.0 / 2.0);
                        backbuffer[sy * width + sx] = (glow << 8);
                    }
                }
            }
        }
    }
    
    // Draw ENTITIES with health bars
    for (int i = 0; i < g_entity_count; i++) {
        Entity *e = &g_entities[i];
        double dx = e->pos.x - g_player.pos.x;
        double dy = e->pos.y - g_player.pos.y;
        double dist = sqrt(dx * dx + dy * dy);
        
        if (dist < 150.0 && dist > 0.5) {
            double angle_to_entity = atan2(dy, dx);
            double rel_angle = angle_to_entity - g_player.angle_yaw;
            
            // Normalize angle
            while (rel_angle > 3.14159) rel_angle -= 2*3.14159;
            while (rel_angle < -3.14159) rel_angle += 2*3.14159;
            
            double fov_h = 60.0 * (3.14159 / 180.0);
            double screen_x = width / 2.0 - (rel_angle / (fov_h / 2.0)) * (width / 2.0);
            double wall_height = (double)height / (dist + 0.1);
            
            // Draw simple rect entity (vertical bar with color)
            int ent_width = (int)(60.0 / (dist / 10.0));
            int ent_height = (int)(80.0 / (dist / 10.0));
            if (ent_width < 4) ent_width = 4;
            if (ent_height < 6) ent_height = 6;
            
            int ex = (int)screen_x - ent_width / 2;
            int ey = (int)(height / 2.0 - ent_height / 2);
            
            // Draw entity body with type-specific color
            for (int y = 0; y < ent_height; y++) {
                for (int x = 0; x < ent_width; x++) {
                    int sx = ex + x;
                    int sy = ey + y;
                    if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                        uint32_t ent_color;
                        if (e->type == 2) {
                            // BOSS: Dark purple/magenta
                            ent_color = 0xFF00FF;  // Magenta BGR
                        } else if (e->type == 1) {
                            // RED AGGRESSIVE: Bright red
                            ent_color = 0x0000FF;  // Bright red BGR
                        } else {
                            // NORMAL: Orange
                            ent_color = 0x0080FF;  // Orange BGR
                        }
                        backbuffer[sy * width + sx] = ent_color;
                    }
                }
            }
            
            // Draw health bar above entity
            int hbar_width = ent_width;
            int hbar_height = 4;
            int hbar_x = ex;
            int hbar_y = ey - 8;
            double health_ratio = e->health / e->max_health;
            int health_width = (int)(hbar_width * health_ratio);
            
            // Health bar background
            for (int y = 0; y < hbar_height; y++) {
                for (int x = 0; x < hbar_width; x++) {
                    int sx = hbar_x + x;
                    int sy = hbar_y + y;
                    if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                        backbuffer[sy * width + sx] = 0x000000;  // Black
                    }
                }
            }
            
            // Health bar fill (green to red)
            for (int y = 0; y < hbar_height; y++) {
                for (int x = 0; x < health_width; x++) {
                    int sx = hbar_x + x;
                    int sy = hbar_y + y;
                    if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                        uint8_t r = (uint8_t)(255.0 * (1.0 - health_ratio));
                        uint8_t g = (uint8_t)(255.0 * health_ratio);
                        backbuffer[sy * width + sx] = (0 << 16) | (g << 8) | r;
                    }
                }
            }
        }
    }
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
    if (health_ratio < 0.0) health_ratio = 0.0;
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
    
    // Draw health percentage text (simple numeric display)
    int percent = (int)(health_ratio * 100.0);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    // Simple pixel-based digit drawing for percentage
    // Draw at bar_x + bar_width + 10
    char health_str[16];
    snprintf(health_str, sizeof(health_str), "%d%%", percent);
    
    // If dead, overlay red screen and "YOU DIED" message
    if (g_is_dead) {
        // Red overlay
        for (int i = 0; i < width * height; i++) {
            uint32_t pixel = backbuffer[i];
            backbuffer[i] = (pixel & 0x00FF00) | 0xFF0000;  // Red overlay
        }
        
        // Draw "YOU DIED" text area (simple large red block with white border)
        int text_w = 200;
        int text_h = 120;
        int text_x = (width - text_w) / 2;
        int text_y = (height - text_h) / 2;
        
        // Black background for text box
        for (int y = text_y; y < text_y + text_h; y++) {
            for (int x = text_x; x < text_x + text_w; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    backbuffer[y * width + x] = 0x000000;
                }
            }
        }
        
        // White border
        for (int y = text_y; y < text_y + text_h; y++) {
            for (int x = text_x; x < text_x + text_w; x += text_w - 1) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    backbuffer[y * width + x] = 0xFFFFFF;
                }
            }
        }
        for (int x = text_x; x < text_x + text_w; x++) {
            for (int y = text_y; y < text_y + text_h; y += text_h - 1) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    backbuffer[y * width + x] = 0xFFFFFF;
                }
            }
        }
        
        // Fill middle with red text area
        for (int y = text_y + 5; y < text_y + 30; y++) {
            for (int x = text_x + 5; x < text_x + text_w - 5; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    backbuffer[y * width + x] = 0x0000FF;  // Red BGR
                }
            }
        }
        
        // Draw respawn message area
        for (int y = text_y + 60; y < text_y + 90; y++) {
            for (int x = text_x + 5; x < text_x + text_w - 5; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    backbuffer[y * width + x] = 0x00FF00;  // Green BGR
                }
            }
        }
    }
    
    // === DRAW SUN and MOON - FIXED SCREEN POSITIONS (overwrite everything last) ===
    int sun_x = (int)(width * 0.75);
    int sun_y = (int)(height * 0.15);
    int moon_x = (int)(width * 0.15);
    int moon_y = (int)(height * 0.10);
    
    // Draw SUN (efficient single pass)
    for (int y = sun_y - 50; y < sun_y + 50; y++) {
        if (y < 0 || y >= height / 2) continue;
        for (int x = sun_x - 50; x < sun_x + 50; x++) {
            if (x < 0 || x >= width) continue;
            int dx = x - sun_x;
            int dy = y - sun_y;
            int dist_sq = dx * dx + dy * dy;
            if (dist_sq < 2500) {  // 50 pixel radius
                double dist_norm = sqrt((double)dist_sq) / 50.0;
                // Bright yellow center, orange halo edges
                uint8_t r, g, b;
                if (dist_norm < 0.3) {
                    r = 255; g = 255; b = 0;  // Bright yellow
                } else if (dist_norm < 0.6) {
                    r = 255; g = 200; b = 0;  // Orange
                } else {
                    uint8_t alpha = (uint8_t)(255.0 * (1.0 - dist_norm));
                    r = alpha; g = alpha/2; b = 0;
                }
                backbuffer[y * width + x] = (b << 16) | (g << 8) | r;
            }
        }
    }
    
    // Draw MOON (efficient single pass)
    for (int y = moon_y - 30; y < moon_y + 30; y++) {
        if (y < 0 || y >= height / 2) continue;
        for (int x = moon_x - 30; x < moon_x + 30; x++) {
            if (x < 0 || x >= width) continue;
            int dx = x - moon_x;
            int dy = y - moon_y;
            int dist_sq = dx * dx + dy * dy;
            if (dist_sq < 900) {  // 30 pixel radius
                double dist_norm = sqrt((double)dist_sq) / 30.0;
                // Bright pale center with subtle shadow
                uint8_t shade = (uint8_t)(255.0 * (1.0 - dist_norm * 0.3));
                backbuffer[y * width + x] = (shade << 16) | (shade << 8) | shade;
            }
        }
    }
    
    // === DRAW CROSSHAIR (center + for aiming) ===
    int cx = width / 2;
    int cy = height / 2;
    int crosshair_size = 12;
    
    // Horizontal line
    for (int x = cx - crosshair_size; x <= cx + crosshair_size; x++) {
        if (x >= 0 && x < width && cy >= 0 && cy < height) {
            backbuffer[cy * width + x] = 0x00FF00;  // Green crosshair BGR
        }
    }
    
    // Vertical line
    for (int y = cy - crosshair_size; y <= cy + crosshair_size; y++) {
        if (cx >= 0 && cx < width && y >= 0 && y < height) {
            backbuffer[y * width + cx] = 0x00FF00;  // Green crosshair BGR
        }
    }
    
    // Center dot
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int x = cx + dx;
            int y = cy + dy;
            if (x >= 0 && x < width && y >= 0 && y < height) {
                backbuffer[y * width + x] = 0xFFFF00;  // Yellow center dot BGR
            }
        }
    }
}
// ===== INPUT HANDLING =====
void handle_key_input(uint8_t key, int is_down) {
    // Respawn on R key
    if ((key == 'R' || key == 'r') && is_down && g_is_dead) {
        g_is_dead = 0;
        g_death_timer = 0.0;
        g_player.health = 100.0;
        g_player.pos.x = 128.0;
        g_player.pos.y = 128.0;
        g_player.shoot_recoil = 0.0;
        g_entity_count = 0;
        // Respawn with mixed enemy types
        for (int i = 0; i < 4; i++) {
            spawn_normal_enemy(30.0 + (rand() % 180), 30.0 + (rand() % 180), 0.0);
        }
        for (int i = 0; i < 2; i++) {
            spawn_red_enemy(40.0 + (rand() % 160), 40.0 + (rand() % 160), 0.0);
        }
        spawn_boss(128.0, 80.0, 0.0);
        return;
    }
    
    if (g_is_dead) return;  // Ignore other input while dead
    
    // Parallel key handling - track which keys are down
    if (is_down) {
        if (key == 'W' || key == 'w') g_key_w = 1;
        if (key == 'A' || key == 'a') g_key_a = 1;
        if (key == 'S' || key == 's') g_key_s = 1;
        if (key == 'D' || key == 'd') g_key_d = 1;
        if (key == VK_LEFT) g_key_left = 1;
        if (key == VK_RIGHT) g_key_right = 1;
        if (key == VK_SPACE) {
            g_key_space = 1;
            player_shoot();  // Shoot on space press
        }
        if (key == VK_UP) {
            g_key_up = 1;
            player_shoot();  // Also shoot on arrow up press
        }
    } else {
        // Key release
        if (key == 'W' || key == 'w') g_key_w = 0;
        if (key == 'A' || key == 'a') g_key_a = 0;
        if (key == 'S' || key == 's') g_key_s = 0;
        if (key == 'D' || key == 'd') g_key_d = 0;
        if (key == VK_LEFT) g_key_left = 0;
        if (key == VK_RIGHT) g_key_right = 0;
        if (key == VK_SPACE) g_key_space = 0;
        if (key == VK_UP) g_key_up = 0;
    }
}

// Dummy tick function
void tick(void) {
    // Logic handled in game_update
}