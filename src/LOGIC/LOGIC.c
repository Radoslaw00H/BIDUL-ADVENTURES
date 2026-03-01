// ===== BIDUL ADVENTURES: Full 3D FPS with enemies, projectiles, health =====
// [LIBRARIES] ====================================================================================
#include "LOGIC.h"      // Core game definitions (structs, function declarations)
#include <windows.h>    // Windows API library  (CreateWindow, MessageBox, etc)
#include <math.h>       // Math library (cos, sin, sqrt, atan2) 
#include <stdlib.h>     // Standard library (malloc, free, rand, srand)
#include <string.h>     // String operations (memset, memcpy)
#include <time.h>       // Time functions (time() for random seed)
#include <stdio.h>      // File I/O (fopen, fread for image loading)

// ===== MEMORY POOL (from init.c merged here) ===================================================
#define MEMORY_SIZE_BYTES (512 * 1024 * 1024)  // 512 MiB for all dynamic allocations

static uint8_t g_memory_pool[MEMORY_SIZE_BYTES];  // Static memory pool array (allocated at startup)

typedef struct {                           // Memory pool tracking struct
    uint8_t* base;                         // Pointer to memory pool start
    uint32_t size;                         // Total size in bytes
    uint32_t used;                         // Already-allocated bytes
} MemoryPool;

static MemoryPool g_pool;                  // Global memory pool instance

void init(void) {                          // Initialize memory pool for game startup
    g_pool.base = g_memory_pool;           // Point to static pool array
    g_pool.size = MEMORY_SIZE_BYTES;       // Set total pool size
    g_pool.used = 0;                       // Start with zero bytes used
}

// ===== CLOCK (High-Resolution Timer) ========================================================
static LARGE_INTEGER g_freq;               // CPU frequency counter (for timing)
static LARGE_INTEGER g_start;              // Timer start point
static double g_tick_interval;             // Time per frame (1/165 seconds)


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

// Texture system - stores loaded graphics for normal, red aggressive, and boss entities
Texture g_textures[MAX_TEXTURES];  // Index 0=normal, 1=red, 2=boss

// Spawn timer - spawns new enemies every 5 seconds (0.125 * boss every 5 sec = 1 boss every 40 sec)
double g_spawn_timer = 0.0;

// ===== IMAGE LOADING (BMP format with procedural fallback) ===================================
// BMP file header structure (only what we need)
#pragma pack(1)                            // Pack structs without padding
typedef struct {
    uint16_t signature;                    // "BM" = 0x424D
    uint32_t file_size;                    // File size in bytes
    uint16_t reserved1;                    // Reserved (must be 0)
    uint16_t reserved2;                    // Reserved (must be 0)
    uint32_t pixel_offset;                 // Offset to pixel data
} BMPFileHeader;

typedef struct {
    uint32_t header_size;                  // DIB header size
    int32_t width;                         // Image width in pixels
    int32_t height;                        // Image height in pixels
    uint16_t planes;                       // Color planes (must be 1)
    uint16_t bits_per_pixel;               // Bits per pixel (24 or 32)
    uint32_t compression;                  // Compression type (0 = none)
    uint32_t image_size;                   // Image data size
    int32_t x_pixels_per_meter;            // Horizontal resolution
    int32_t y_pixels_per_meter;            // Vertical resolution
    uint32_t num_colors;                   // Number of colors in palette
    uint32_t important_colors;             // Important colors count
} BMPInfoHeader;
#pragma pack()

// Load BMP file from disk
// Parameters: path = file path, texture = output texture struct
// Returns: 1 if loaded successfully, 0 if load failed
int load_bmp_file(const char *path, Texture *texture) {
    FILE *file = fopen(path, "rb");        // Open file in binary read mode
    if (!file) {                           // If file doesn't exist
        return 0;                          // Return failure
    }

    BMPFileHeader file_header;             // BMP file header
    size_t read = fread(&file_header, sizeof(BMPFileHeader), 1, file);  // Read header
    if (read != 1 || file_header.signature != 0x424D) {  // Check "BM" signature
        fclose(file);                      // Close file on error
        return 0;                          // Invalid BMP file
    }

    BMPInfoHeader info_header;             // BMP info header
    fread(&info_header, sizeof(BMPInfoHeader), 1, file);  // Read info
    if (info_header.width <= 0 || info_header.height <= 0) {  // Check dimensions
        fclose(file);                      // File is invalid
        return 0;
    }

    // Allocate memory for texture pixels
    int num_pixels = info_header.width * abs(info_header.height);  // Total pixels
    texture->pixels = (uint32_t*)malloc(num_pixels * sizeof(uint32_t));  // Allocate buffer
    if (!texture->pixels) {                // Memory allocation failed
        fclose(file);
        return 0;
    }

    // Set texture dimensions
    texture->width = info_header.width;    // Store width
    texture->height = abs(info_header.height);  // Store height (absolute value)

    // Seek to pixel data
    fseek(file, file_header.pixel_offset, SEEK_SET);  // Jump to pixel start

    // Read 24-bit BGR data and convert to 32-bit BGRA
    if (info_header.bits_per_pixel == 24) {
        // For each scanline (BMP stores upside-down)
        for (int y = 0; y < texture->height; y++) {
            for (int x = 0; x < texture->width; x++) {
                uint8_t b, g, r;           // Read BGR bytes
                fread(&b, 1, 1, file);     // Read blue channel
                fread(&g, 1, 1, file);     // Read green channel
                fread(&r, 1, 1, file);     // Read red channel
                // Pack into 32-bit BGRA (with alpha = 255)
                uint32_t pixel = (255 << 24) | (r << 16) | (g << 8) | b;
                texture->pixels[y * texture->width + x] = pixel;  // Write pixel
            }
        }
    } else if (info_header.bits_per_pixel == 32) {
        // 32-bit BGRA (already in correct format)
        fread(texture->pixels, num_pixels * sizeof(uint32_t), 1, file);  // Read all pixels
    } else {
        // Unsupported bit depth
        free(texture->pixels);             // Free allocated memory
        fclose(file);
        return 0;
    }

    fclose(file);                          // Close file when done
    texture->loaded = 1;                   // Mark as successfully loaded
    return 1;
}

// Create procedurally generated texture with pattern
// Parameters: type = entity type (0=normal, 1=red, 2=boss), texture = output texture
// Returns: 1 on success
int create_procedural_texture(int type, Texture *texture) {
    // Allocate 256x256 pixel texture
    int size = 256;                        // Texture size in pixels
    int num_pixels = size * size;          // Total pixel count
    texture->pixels = (uint32_t*)malloc(num_pixels * sizeof(uint32_t));  // Allocate memory
    if (!texture->pixels) return 0;        // Failed to allocate

    texture->width = size;                 // Set width
    texture->height = size;                // Set height

    // Generate different pattern for each entity type
    if (type == 0) {
        // NORMAL ENEMY: Orange with stipple pattern
        uint32_t base_color = 0x0080FF;    // Orange in BGR
        for (int i = 0; i < num_pixels; i++) {
            // Checkerboard stipple for texture effect
            int x = i % size;              // Get X coordinate
            int y = i / size;              // Get Y coordinate
            if ((x + y) % 4 < 2) {
                texture->pixels[i] = base_color;  // Base orange
            } else {
                // Slightly darker for depth
                texture->pixels[i] = (base_color >> 1) & 0x7F7FFF;
            }
        }
    } else if (type == 1) {
        // RED AGGRESSIVE: Bright red with noise pattern
        uint32_t base_color = 0x0000FF;    // Bright red in BGR
        for (int i = 0; i < num_pixels; i++) {
            int x = i % size;
            int y = i / size;
            // Add noise based on position
            int noise = ((x * 73 + y * 97) % 256);  // Pseudo-random noise
            if (noise > 200) {
                texture->pixels[i] = base_color;   // Bright red
            } else if (noise > 100) {
                texture->pixels[i] = (base_color >> 1) & 0x7F7FFF;  // Dark red
            } else {
                texture->pixels[i] = (base_color >> 2) & 0x3F3FFF;  // Very dark red
            }
        }
    } else {
        // BOSS: Magenta with hexagon pattern
        uint32_t base_color = 0xFF00FF;    // Magenta in BGR
        for (int i = 0; i < num_pixels; i++) {
            int x = i % size;
            int y = i / size;
            // Create hexagon-like pattern
            int dist = (abs(x - 128) + abs(y - 128)) / 16;  // Distance from center
            if (dist % 3 == 0) {
                texture->pixels[i] = base_color;   // Bright magenta
            } else {
                texture->pixels[i] = (base_color >> 1) & 0x7F7FFF;  // Dark magenta
            }
        }
    }

    texture->loaded = 1;                   // Mark as successfully generated
    return 1;
}

// Load texture with fallback chain
// Attempts: PNG → BMP → Procedural generation
void load_entity_texture(int type, const char *filename) {
    Texture *tex = &g_textures[type];       // Get texture slot for this type
    
    // Try to load from graphics folder
    char path[512];                        // Path buffer
    snprintf(path, sizeof(path), "src/GRAPHICS_SOUNDS/GRAPHICS/%s", filename);  // Build path

    // First, try BMP file
    char bmp_path[512];                    // BMP path buffer
    snprintf(bmp_path, sizeof(bmp_path), "%s.bmp", filename);  // Replace extension
    if (load_bmp_file(bmp_path, tex)) {    // Try loading as BMP
        return;                            // Success - exit
    }

    // Second, try PNG file (will fail without proper library)
    if (load_bmp_file(filename, tex)) {    // Try original filename (PNG)
        return;                            // Success - exit
    }

    // Fallback: Generate procedural texture
    create_procedural_texture(type, tex);  // Create pattern-based texture
}


// ===== MAZE GENERATION (for 512x512 huge world) ===========================================
// Generate massive random maze using cellular automata algorithm
// Parameters: none
// Returns: void - modifies g_world_map directly
void generate_random_maze(void) {
    // Clear entire map to empty walkable space
    memset(g_world_map, 0, sizeof(g_world_map));  // Fill with zeros (empty)

    // Create solid border walls (keep player bounded)
    for (int x = 0; x < WORLD_WIDTH; x++) {
        g_world_map[0][x] = 1;             // Top border
        g_world_map[WORLD_HEIGHT - 1][x] = 1;  // Bottom border
    }
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        g_world_map[y][0] = 1;             // Left border
        g_world_map[y][WORLD_WIDTH - 1] = 1;   // Right border
    }

    // Create large room structure - divide into quadrants with walls
    // This creates multiple distinct areas connected by corridors
    int rooms_x = 4;                       // Number of rooms horizontally
    int rooms_y = 4;                       // Number of rooms vertically
    int room_width = WORLD_WIDTH / rooms_x;  // Width of each room
    int room_height = WORLD_HEIGHT / rooms_y;  // Height of each room

    // Create room grid with thick walls between rooms
    for (int ry = 0; ry < rooms_y; ry++) {
        for (int rx = 0; rx < rooms_x; rx++) {
            // Fill walls at the boundaries of each room
            int x_start = rx * room_width;      // Left edge of room
            int y_start = ry * room_height;     // Top edge of room
            int x_end = (rx + 1) * room_width;  // Right edge of room
            int y_end = (ry + 1) * room_height; // Bottom edge of room

            // Create vertical walls on left side of room
            for (int y = y_start; y < y_end; y++) {
                g_world_map[y][x_start] = 2;   // Wall type 2
                g_world_map[y][x_start + 1] = 2;  // Double thickness
            }

            // Create horizontal walls on top of room
            for (int x = x_start; x < x_end; x++) {
                g_world_map[y_start][x] = 2;   // Wall type 2
                g_world_map[y_start + 1][x] = 2;  // Double thickness
            }
        }
    }

    // Create random inner obstacles and structures throughout the map
    // This adds complexity and variety to gameplay
    for (int i = 0; i < 300; i++) {
        // Pick random location in playable area
        int x = 20 + (rand() % (WORLD_WIDTH - 40));
        int y = 20 + (rand() % (WORLD_HEIGHT - 40));

        // Randomly create wall structures
        if (g_world_map[y][x] == 0) {
            int wall_type = 1 + (rand() % 7);  // Random wall variant 1-7
            
            // Create clusters of walls (not just isolated pixels)
            int size = 1 + (rand() % 4);       // Size of structure (1-4 tiles)
            for (int dy = -size; dy <= size; dy++) {
                for (int dx = -size; dx <= size; dx++) {
                    int nx = x + dx;           // Neighbor X
                    int ny = y + dy;           // Neighbor Y
                    if (nx > 1 && nx < WORLD_WIDTH - 1 && ny > 1 && ny < WORLD_HEIGHT - 1) {
                        if (g_world_map[ny][nx] == 0) {  // Only fill empty spaces
                            g_world_map[ny][nx] = wall_type;  // Place wall
                        }
                    }
                }
            }
        }
    }

    // Create open pathways (corridors) by clearing random lines
    // This ensures the maze is traversable and not too dense
    for (int i = 0; i < 50; i++) {
        // Pick random corridor endpoints
        int x1 = 30 + (rand() % (WORLD_WIDTH - 60));  // Start X
        int y1 = 30 + (rand() % (WORLD_HEIGHT - 60));  // Start Y
        int x2 = 30 + (rand() % (WORLD_WIDTH - 60));  // End X
        int y2 = 30 + (rand() % (WORLD_HEIGHT - 60));  // End Y

        // Draw line clearing between the two points
        int dx = x2 - x1;                  // X distance
        int dy = y2 - y1;                  // Y distance
        int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);  // Number of steps
        if (steps == 0) steps = 1;         // Avoid division by zero

        for (int step = 0; step < steps; step++) {
            // Calculate interpolated position
            int x = x1 + (dx * step) / steps;  // Current X
            int y = y1 + (dy * step) / steps;  // Current Y

            // Clear a few tiles wide corridor
            for (int cy = y - 2; cy <= y + 2; cy++) {
                for (int cx = x - 2; cx <= x + 2; cx++) {
                    if (cx > 1 && cx < WORLD_WIDTH - 1 && cy > 1 && cy < WORLD_HEIGHT - 1) {
                        g_world_map[cy][cx] = 0;  // Clear to empty
                    }
                }
            }
        }
    }

    // Create symmetrical puzzle-like sections for visual interest
    int puzzle_x = 100 + (rand() % 300);   // Random X position for puzzle
    int puzzle_y = 100 + (rand() % 300);   // Random Y position for puzzle
    for (int i = 0; i < 30; i++) {
        // Draw concentric square patterns
        int dist = i * 3;                  // Distance from center
        for (int x = puzzle_x - dist; x <= puzzle_x + dist; x++) {
            for (int y = puzzle_y - dist; y <= puzzle_y + dist; y++) {
                // Only draw on the perimeter of the square (not filled)
                if ((x == puzzle_x - dist || x == puzzle_x + dist ||
                    y == puzzle_y - dist || y == puzzle_y + dist) &&
                    x > 10 && x < WORLD_WIDTH - 10 && y > 10 && y < WORLD_HEIGHT - 10) {
                    if (g_world_map[y][x] == 0) {
                        g_world_map[y][x] = 3 + (i % 5);  // Alternating wall types
                    }
                }
            }
        }
    }
}


// ===== GAME INITIALIZATION =====================================================================
void game_init(void) {
    // ===== PLAYER INITIALIZATION =====
    // Set initial player position to world center
    g_player.pos.x = 256.0;                // Player X coordinate (center of huge map)
    g_player.pos.y = 256.0;                // Player Y coordinate (center of huge map)
    g_player.pos.z = 0.0;                  // Player Z coordinate (height/elevation)
    
    // Set player rotation angles to zero (looking forward)
    g_player.angle_yaw = 0.0;              // Left-right rotation (horizontal camera)
    g_player.angle_pitch = 0.0;            // Up-down rotation (vertical camera tilt)
    
    // Initialize velocity to zero (player starts stationary)
    g_player.vel.x = 0.0;                  // X velocity (units/sec)
    g_player.vel.y = 0.0;                  // Y velocity (units/sec)
    g_player.vel.z = 0.0;                  // Z velocity (vertical movement)
    
    // Set player health to maximum
    g_player.health = 100.0;               // Current health points (0-100 range)
    g_player.max_health = 100.0;           // Maximum health capacity
    
    // Initialize weapon state
    g_player.shoot_cooldown = 0.0;         // Time until next shot is ready (seconds)
    g_player.shoot_recoil = 0.0;           // Gun kickback animation state (0-1 range)
    
    // ===== MAZE GENERATION =====
    // Generate huge random maze structure for this play session
    generate_random_maze();                // Create maze layout in g_world_map
    
    // ===== TEXTURE LOADING WITH FALLBACK =====
    // Load entity textures with fallback to procedural generation
    
    // Texture 0: Normal enemy texture
    load_entity_texture(0, "NORMAL_ENTITIY.png");  // Try to load NORMAL_ENTITIY.png or .bmp
    
    // Texture 1: Red aggressive enemy texture
    load_entity_texture(1, "RED_AGRESSIVE_ENTITIY.png");  // Try to load RED_AGRESSIVE_ENTITIY.png
    
    // Texture 2: Boss enemy texture
    load_entity_texture(2, "BOSS_ENTITY.png");     // Try to load BOSS_ENTITY.png or .bmp
    
    // ===== MEMORY ALLOCATION =====
    // Allocate dynamic arrays for entities (enemies)
    g_entities = (Entity*)malloc(MAX_ENTITIES * sizeof(Entity));  // Allocate entity array
    
    // Allocate dynamic array for projectiles (bullets)
    g_projectiles = (Projectile*)malloc(MAX_PROJECTILES * sizeof(Projectile));  // Allocate projectile array
    
    // Initialize entity and projectile counters to zero
    g_entity_count = 0;                    // Number of active entities (starts at 0)
    g_projectile_count = 0;                // Number of active projectiles (starts at 0)
    g_drop_count = 0;                      // Number of falling drops (HUD effect)
    g_drop_timer = 0.0;                    // Timer for drop spawning effect (milliseconds)
    
    // Initialize spawn system timer
    g_spawn_timer = 0.0;                   // Reset spawn timer for 5-second spawning intervals
    
    // ===== INITIAL ENEMY SPAWNING =====
    // Seed random number generator with current time for variety
    srand(time(NULL));                     // Seed with current system time
    
    // Spawn initial wave of normal enemies (5 total)
    for (int i = 0; i < 5; i++) {
        // Generate random spawn position in playable area of huge map
        double x = 50.0 + (rand() % 400);  // Random X between 50-450 (avoid edges)
        double y = 50.0 + (rand() % 400);  // Random Y between 50-450 (avoid edges)
        // Spawn normal enemy at this location with Z=0
        spawn_normal_enemy(x, y, 0.0);
    }
    
    // Spawn initial wave of red aggressive enemies (2 total)
    for (int i = 0; i < 2; i++) {
        // Generate random spawn position in slightly more central area
        double x = 100.0 + (rand() % 300);  // Random X between 100-400
        double y = 100.0 + (rand() % 300);  // Random Y between 100-400
        // Spawn red aggressive enemy at this location
        spawn_red_enemy(x, y, 0.0);
    }
    
    // Spawn initial boss enemy (1 total, placed centrally)
    spawn_boss(256.0, 256.0, 0.0);         // Boss spawns at center location
    
    // Debug message to confirm initialization
    MessageBoxA(NULL, "Game initialized!\nHuge 512x512 maze with texture loading enabled", "BIDUL ADVENTURES", MB_OK);
}


void game_cleanup(void) {
    // Free entity array if allocated
    if (g_entities) free(g_entities);  // Release entity memory
    
    // Free projectile array if allocated
    if (g_projectiles) free(g_projectiles);  // Release projectile memory
    
    // Free all texture pixel buffers
    for (int i = 0; i < MAX_TEXTURES; i++) {
        // Check if this texture has allocated pixels
        if (g_textures[i].pixels) {
            // Free texture pixel buffer
            free(g_textures[i].pixels);
            // Clear texture struct
            g_textures[i].pixels = NULL;
            g_textures[i].loaded = 0;
        }
    }
    
    // Set pointers to NULL for safety
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
    
    // ===== SPAWN TIMER SYSTEM =====
    // Increment spawn timer by delta time
    g_spawn_timer += dt;                   // Add frame time to spawn counter
    
    // Check if 5 seconds have passed for next spawn wave
    if (g_spawn_timer >= 5.0) {
        g_spawn_timer = 0.0;               // Reset timer for next 5-second cycle
        
        // Spawn wave: 5 normal enemies
        for (int i = 0; i < 5; i++) {
            // Generate random spawn position in playable area
            double x = 40.0 + (rand() % 160); // Random X between 40-200
            double y = 40.0 + (rand() % 160); // Random Y between 40-200
            // Spawn normal orange enemy at this location
            spawn_normal_enemy(x, y, 0.0);
        }
        
        // Spawn wave: 2 red aggressive enemies
        for (int i = 0; i < 2; i++) {
            // Generate random spawn position away from player
            double x = 50.0 + (rand() % 150);  // Random X between 50-200
            double y = 50.0 + (rand() % 150);  // Random Y between 50-200
            // Spawn red aggressive enemy at this location
            spawn_red_enemy(x, y, 0.0);
        }
        
        // Spawn wave: 0.125 boss (1 boss every 40 seconds = 8 waves)
        // This accumulates: after 8 spawn events (40 seconds), spawn 1 boss
        // Simple implementation: 12.5% chance per spawn event = 1 boss per 40 sec average
        if ((rand() % 1000) < 125) {
            // Random boss spawn location far from player
            double x = 60.0 + (rand() % 130);  // Random X between 60-190
            double y = 60.0 + (rand() % 130);  // Random Y between 60-190
            // Spawn boss at this location
            spawn_boss(x, y, 0.0);
        }
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
            
            // Draw entity body with texture based on type
            // Get appropriate texture for this entity type
            Texture *tex = &g_textures[e->type];  // Texture 0=normal, 1=red, 2=boss
            
            // Draw entity by sampling from loaded texture
            for (int y = 0; y < ent_height; y++) {
                for (int x = 0; x < ent_width; x++) {
                    int sx = ex + x;                // Screen space X coordinate
                    int sy = ey + y;                // Screen space Y coordinate
                    
                    // Check if pixel is within screen bounds
                    if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                        // Calculate texture UV coordinates (normalize to 0-1 range)
                        double u = (double)x / (double)ent_width;  // X position in texture (0-1)
                        double v = (double)y / (double)ent_height; // Y position in texture (0-1)
                        
                        // Sample texture using nearest-neighbor filtering
                        uint32_t ent_color;
                        if (tex->loaded && tex->pixels) {
                            // Texture loaded - sample from it
                            int tx = (int)(u * (double)(tex->width - 1));  // Texture X coordinate
                            int ty = (int)(v * (double)(tex->height - 1)); // Texture Y coordinate
                            // Clamp to valid texture bounds
                            if (tx < 0) tx = 0;     // Prevent underflow
                            if (tx >= tex->width) tx = tex->width - 1;  // Prevent overflow
                            if (ty < 0) ty = 0;     // Prevent underflow
                            if (ty >= tex->height) ty = tex->height - 1; // Prevent overflow
                            // Read pixel from texture and use as entity color
                            ent_color = tex->pixels[ty * tex->width + tx];
                        } else {
                            // Fallback to solid colors if texture not loaded
                            if (e->type == 2) {
                                ent_color = 0xFF00FF;  // Magenta fallback for boss
                            } else if (e->type == 1) {
                                ent_color = 0x0000FF;  // Bright red fallback for aggressive
                            } else {
                                ent_color = 0x0080FF;  // Orange fallback for normal
                            }
                        }
                        
                        // Write color to screen buffer
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
    // Check if player pressed R key to respawn after death
    if ((key == 'R' || key == 'r') && is_down && g_is_dead) {
        // Clear dead state
        g_is_dead = 0;                     // Player is alive again
        g_death_timer = 0.0;              // Reset death timer
        
        // Restore player to full health
        g_player.health = 100.0;           // Restore to maximum health
        
        // Reset player position to map center
        g_player.pos.x = 128.0;            // Center X coordinate
        g_player.pos.y = 128.0;            // Center Y coordinate
        
        // Reset weapon state
        g_player.shoot_recoil = 0.0;       // Clear any recoil animation
        
        // Clear all existing enemies
        g_entity_count = 0;                // Remove all hostile entities
        
        // Reset spawn timer for next wave
        g_spawn_timer = 0.0;               // Begin new 5-second spawn cycle
        
        // Respawn with initial mixed enemy types - 4 normal enemies
        for (int i = 0; i < 4; i++) {
            // Generate random spawn position in playable area
            double x = 30.0 + (rand() % 180);  // Random X between 30-210
            double y = 30.0 + (rand() % 180);  // Random Y between 30-210
            // Spawn normal orange enemy at random location
            spawn_normal_enemy(x, y, 0.0);
        }
        
        // Respawn 2 red aggressive enemies
        for (int i = 0; i < 2; i++) {
            // Generate random spawn position slightly more central
            double x = 40.0 + (rand() % 160);  // Random X between 40-200
            double y = 40.0 + (rand() % 160);  // Random Y between 40-200
            // Spawn red aggressive enemy at random location
            spawn_red_enemy(x, y, 0.0);
        }
        
        // Respawn 1 boss enemy
        spawn_boss(128.0, 80.0, 0.0);      // Boss spawns at center location
        
        // Exit respawn handler early
        return;
    }
    
    // If dead, ignore all other input except respawn (R key)
    if (g_is_dead) return;                 // Death mode - no other keys work
    
    // ===== PARALLEL KEY HANDLING =====
    // Process key down events
    if (is_down) {
        // Forward movement (W key)
        if (key == 'W' || key == 'w') g_key_w = 1;
        // Left strafe (A key)
        if (key == 'A' || key == 'a') g_key_a = 1;
        // Backward movement (S key)
        if (key == 'S' || key == 's') g_key_s = 1;
        // Right strafe (D key)
        if (key == 'D' || key == 'd') g_key_d = 1;
        // Turn left (LEFT arrow key)
        if (key == VK_LEFT) g_key_left = 1;
        // Turn right (RIGHT arrow key)
        if (key == VK_RIGHT) g_key_right = 1;
        // Shoot (SPACE key)
        if (key == VK_SPACE) {
            g_key_space = 1;               // Mark space as pressed
            player_shoot();                // Fire projectile on key press
        }
        // Shoot (UP arrow key)
        if (key == VK_UP) {
            g_key_up = 1;                  // Mark up arrow as pressed
            player_shoot();                // Fire projectile on key press
        }
    } else {
        // Process key release events - mark keys as no longer active
        // Forward movement release (W key up)
        if (key == 'W' || key == 'w') g_key_w = 0;
        // Left strafe release (A key up)
        if (key == 'A' || key == 'a') g_key_a = 0;
        // Backward movement release (S key up)
        if (key == 'S' || key == 's') g_key_s = 0;
        // Right strafe release (D key up)
        if (key == 'D' || key == 'd') g_key_d = 0;
        // Turn left release (LEFT arrow key up)
        if (key == VK_LEFT) g_key_left = 0;
        // Turn right release (RIGHT arrow key up)
        if (key == VK_RIGHT) g_key_right = 0;
        // Space key release
        if (key == VK_SPACE) g_key_space = 0;
        // Up arrow key release
        if (key == VK_UP) g_key_up = 0;
    }
}

// Dummy tick function for compatibility with game loop
void tick(void) {
    // Logic is handled in game_update function instead
    // This function exists for framework compatibility
}