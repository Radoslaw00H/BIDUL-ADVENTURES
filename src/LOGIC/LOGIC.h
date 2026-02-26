#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>
#include <assert.h>

#define TARGET_FPS 165
#define WORLD_WIDTH 256
#define WORLD_HEIGHT 256
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define MAX_ENTITIES 64
#define MAX_PROJECTILES 256

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
} Entity;

// Projectile (bullet)
typedef struct {
    Vec3 pos;
    Vec3 vel;
    double lifetime;
    double age;
} Projectile;

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

// Core functions
void clock_init(void);
int clock_tick(void);
void game_update(void);
void game_render(uint32_t* backbuffer, int width, int height);
void game_init(void);
void game_cleanup(void);

// Gameplay functions
void player_shoot(void);
void spawn_enemy(double x, double y, double z);
void handle_key_input(uint8_t key, int is_down);
RayHit raycast(double x, double y, double z, double angle_yaw, double angle_pitch);
void tick(void);

#endif // LOGIC_H

