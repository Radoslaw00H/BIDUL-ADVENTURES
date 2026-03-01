#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>
#include <assert.h>

#define TARGET_FPS 165                 // Target frame rate in Hz
#define WORLD_WIDTH 512                // Huge world map width in tiles (512x512)
#define WORLD_HEIGHT 512               // Huge world map height in tiles (4x bigger)
#define SCREEN_WIDTH 640               // Screen width in pixels
#define SCREEN_HEIGHT 480              // Screen height in pixels
#define MAX_ENTITIES 256               // Maximum enemies (more for bigger map)
#define MAX_PROJECTILES 512            // Maximum bullets in flight (double)
#define MAX_TEXTURES 3                 // Maximum entity texture types
#define TEXTURE_MAX_WIDTH 256          // Maximum texture width in pixels
#define TEXTURE_MAX_HEIGHT 256         // Maximum texture height in pixels
#define MEMORY_POOL_SIZE (512 * 1024 * 1024)  // 512 MiB for all game assets

// 3D vector
typedef struct {
    double x, y, z;
} Vec3;

// Player with full 3D state
typedef struct {
    Vec3 pos;           // Position in 3D space
    double angle_yaw;   // Rotation around Z axis (left/right)
    double angle_pitch; // Rotation up/down
    Vec3 vel;           // Velocity vector (x, y, z)
    double health;      // Health points (0-100)
    double max_health;
    double shoot_cooldown;
    double shoot_recoil; // Animation: gun recoil (0-1, 0=reset)
} Player;

// Enemy entity
typedef struct {
    Vec3 pos;
    Vec3 vel;
    double health;
    double max_health;
    double attack_timer;
    uint8_t texture_id;
    double ai_angle;
    double width;   // Render as rectangle
    double height;
    double bob_angle; // Animation bobbing
    uint8_t type;   // 0=normal, 1=red aggressive, 2=boss
} Entity;

// Projectile (bullet)
typedef struct {
    Vec3 pos;
    Vec3 vel;
    double lifetime;
    double age;
    uint8_t color_r; // RGB for bullet
    uint8_t color_g;
    uint8_t color_b;
} Projectile;

// Texture for rendering entities with graphics
typedef struct {
    uint32_t *pixels;              // Pixel buffer in BGR format
    int width;                      // Texture width in pixels
    int height;                     // Texture height in pixels
    int loaded;                     // 1 = loaded successfully, 0 = failed
} Texture;

// Raycast result
typedef struct {
    double distance;
    uint8_t texture_id;
    double hit_x;
    double hit_y;
    double hit_z;
} RayHit;

// World map (now 3D capable)
extern uint8_t g_world_map[WORLD_HEIGHT][WORLD_WIDTH];
extern Player g_player;
extern Entity *g_entities;
extern int g_entity_count;
extern Projectile *g_projectiles;
extern int g_projectile_count;
extern Texture g_textures[MAX_TEXTURES];   // Textures for normal, red, boss entities
extern double g_spawn_timer;               // Timer for spawning new enemies every 5 seconds

// Core game functions
void clock_init(void);                    // Initialize high-resolution timer
int clock_tick(void);                     // Check if frame time has elapsed
void game_update(void);                   // Physics, logic, enemy AI
void game_render(uint32_t* backbuffer, int width, int height);  // 3D raycasting render
void game_init(void);                     // Initialize game state and load assets
void game_cleanup(void);                  // Free all dynamic memory
void init(void);                          // Initialize memory pool (from init.c)

// Gameplay functions
void player_shoot(void);
void spawn_enemy(double x, double y, double z, uint8_t type);
void spawn_normal_enemy(double x, double y, double z);
void spawn_red_enemy(double x, double y, double z);
void spawn_boss(double x, double y, double z);
void handle_key_input(uint8_t key, int is_down);
RayHit raycast(double x, double y, double z, double angle_yaw, double angle_pitch);
void tick(void);

#endif // LOGIC_H

