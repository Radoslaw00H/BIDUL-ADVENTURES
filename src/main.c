// ===== BIDUL ADVENTURES: CONSOLIDATED SINGLE FILE =====
// 3D FPS Game with Raycasting Engine, Enemy AI, Combat System
// Complete game built in pure C with Windows API

// ===== INCLUDES =====
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

// ===== DEFINES =====
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
#define MAX_DROPS 256                  // Maximum falling projectiles on HUD

// ===== STRUCTS AND TYPES =====

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

// DIB section for backbuffer
typedef struct {
    HBITMAP bitmap;
    uint32_t* bits;  // 32-bit ARGB pixel data
    int width;
    int height;
} DIBSection;

// HUD drop structure
typedef struct {
    double x, y;
    double vy;
} Drop;

// BMP file header structure for image loading
typedef struct {
    uint16_t signature;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_offset;
} BMPFileHeader;

// BMP info header for image dimensions and format
typedef struct {
    uint32_t header_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} BMPInfoHeader;

// ===== GLOBAL VARIABLES =====

// Memory management
static uint8_t g_memory_pool[MEMORY_POOL_SIZE];  // Static 512 MB pool for all allocations
static struct {
    uint8_t *current;          // Current allocation pointer
    size_t used;               // Bytes used so far
    size_t total;              // Total pool size
} g_pool;

// Forward declarations for functions used earlier
void spawn_normal_enemy(double x, double y, double z);
void spawn_red_enemy(double x, double y, double z);
void spawn_boss(double x, double y, double z);
void handle_key_input(uint8_t key, int is_down);

// Game world state
uint8_t g_world_map[WORLD_HEIGHT][WORLD_WIDTH];  // Tile map (512x512)
Player g_player;                                  // Player state
Entity *g_entities = NULL;                        // Dynamically allocated entities
int g_entity_count = 0;                           // Current entity count
Projectile *g_projectiles = NULL;                 // Dynamically allocated projectiles
int g_projectile_count = 0;                       // Current projectile count
Texture g_textures[MAX_TEXTURES];                 // Entity textures
double g_spawn_timer = 0.0;                       // Enemy spawn wave timer

// HUD drops (animated projectiles)
static Drop g_drops[MAX_DROPS];                   // Array of falling projectiles
static int g_drop_count = 0;                      // Current number of drops
static double g_drop_timer = 0.1;                 // Timer for spawning new drops

// Input state
static uint8_t g_key_w = 0;                       // W key pressed
static uint8_t g_key_a = 0;                       // A key pressed
static uint8_t g_key_s = 0;                       // S key pressed
static uint8_t g_key_d = 0;                       // D key pressed
static uint8_t g_key_left = 0;                    // LEFT arrow pressed
static uint8_t g_key_right = 0;                   // RIGHT arrow pressed
static uint8_t g_key_space = 0;                   // SPACE key pressed
static uint8_t g_key_up = 0;                      // UP arrow pressed

// Game state
static uint8_t g_is_dead = 0;                     // 1 = dead, waiting for respawn
static double g_death_timer = 0.0;                // Time since death

// Clock/timing
static LARGE_INTEGER g_freq;                      // QueryPerformanceFrequency
static LARGE_INTEGER g_start;                     // Start time
static double g_tick_interval;                    // Frame time interval (6.06ms at 165Hz)

// Windows API globals
HWND g_hwnd = NULL;                               // Window handle
HDC g_hdc = NULL;                                 // Device context
DIBSection g_dib = {0};                           // Backbuffer DIB section
static bool g_running = true;                     // Main loop running flag
static uint8_t g_keys[256] = {0};                 // Key states for input

// ===== MEMORY MANAGEMENT =====

// Initialize memory pool
void init(void) {
    // Initialize memory pool for all allocations
    g_pool.current = g_memory_pool;      // Set current pointer to pool start
    g_pool.used = 0;                     // No memory used yet
    g_pool.total = MEMORY_POOL_SIZE;     // Set total available
    
    // Initialize all globals
    memset(g_entities, 0, sizeof(Entity) * MAX_ENTITIES);  // Clear entities
    memset(g_projectiles, 0, sizeof(Projectile) * MAX_PROJECTILES);  // Clear projectiles
    
    // Initialize player state
    g_player.pos.x = 256.0;              // Spawn at center X
    g_player.pos.y = 256.0;              // Spawn at center Y
    g_player.pos.z = 1.0;                // Eye height 1.0
    g_player.angle_yaw = 0.0;            // Looking north
    g_player.angle_pitch = 0.0;          // Looking level
    g_player.health = 100.0;             // Full health
    g_player.max_health = 100.0;         // Max health capacity
    g_player.shoot_cooldown = 0.0;       // Ready to shoot
    g_player.shoot_recoil = 0.0;         // No recoil
}

// ===== CLOCK/TIMING SYSTEM =====

// Initialize high-resolution clock
void clock_init(void) {
    // Query system clock frequency
    QueryPerformanceFrequency(&g_freq);           // Get system clock ticks per second
    
    // Get current time as start reference
    QueryPerformanceCounter(&g_start);            // Store start time for delta calculation
    
    // Calculate time interval per frame (6.06 ms at 165 Hz)
    g_tick_interval = 1.0 / (double)TARGET_FPS;   // 1/165 seconds per frame
}

// Check if enough time elapsed for next frame
int clock_tick(void) {
    // Get current system time
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);                             // Read current time
    
    // Calculate elapsed time in seconds
    double elapsed = (double)(now.QuadPart - g_start.QuadPart) / (double)g_freq.QuadPart;
    
    // Check if frame interval has passed
    if (elapsed >= g_tick_interval) {
        // Reset timer for next frame
        g_start = now;                                          // Update start for next interval
        return 1;                                               // Frame time elapsed, render now
    }
    
    return 0;                                                   // Not enough time yet
}

// ===== BMP IMAGE LOADING =====

// Load BMP image from disk with error handling
Texture load_bmp_file(const char* filename) {
    Texture result = {0};                                       // Initialize empty texture
    
    // Attempt to open file
    FILE *file = fopen(filename, "rb");                         // Open file in binary read mode
    if (!file) {
        printf("Failed to open %s\n", filename);                // Log error message
        return result;                                          // Return empty texture on failure
    }
    
    // Read BMP file header (14 bytes)
    BMPFileHeader fhdr;                                         // BMP file header struct
    fread(&fhdr.signature, sizeof(uint16_t), 1, file);          // Signature (0x4D42 = 'BM')
    fread(&fhdr.file_size, sizeof(uint32_t), 1, file);          // Total file size
    fread(&fhdr.reserved1, sizeof(uint16_t), 1, file);          // Reserved field 1
    fread(&fhdr.reserved2, sizeof(uint16_t), 1, file);          // Reserved field 2
    fread(&fhdr.pixel_offset, sizeof(uint32_t), 1, file);       // Offset to pixel data
    
    // Verify BMP signature
    if (fhdr.signature != 0x4D42) {
        printf("Not a valid BMP file: %s\n", filename);         // Invalid BMP format
        fclose(file);                                           // Close file handle
        return result;                                          // Return empty texture
    }
    
    // Read BMP info header (40 bytes minimum)
    BMPInfoHeader ihdr;                                         // BMP info header struct
    fread(&ihdr.header_size, sizeof(uint32_t), 1, file);        // Info header size
    fread(&ihdr.width, sizeof(int32_t), 1, file);               // Bitmap width in pixels
    fread(&ihdr.height, sizeof(int32_t), 1, file);              // Bitmap height in pixels
    fread(&ihdr.planes, sizeof(uint16_t), 1, file);             // Number of color planes (always 1)
    fread(&ihdr.bits_per_pixel, sizeof(uint16_t), 1, file);     // Bits per pixel (24 or 32)
    fread(&ihdr.compression, sizeof(uint32_t), 1, file);        // Compression type (0=none)
    fread(&ihdr.image_size, sizeof(uint32_t), 1, file);         // Image data size
    fread(&ihdr.x_pixels_per_meter, sizeof(int32_t), 1, file);  // X resolution
    fread(&ihdr.y_pixels_per_meter, sizeof(int32_t), 1, file);  // Y resolution
    fread(&ihdr.colors_used, sizeof(uint32_t), 1, file);        // Colors in palette
    fread(&ihdr.colors_important, sizeof(uint32_t), 1, file);   // Important colors
    
    // Validate image dimensions
    if (ihdr.width <= 0 || ihdr.height <= 0 ||
        ihdr.width > TEXTURE_MAX_WIDTH || ihdr.height > TEXTURE_MAX_HEIGHT) {
        printf("Invalid image dimensions: %ld x %ld\n", ihdr.width, ihdr.height);  // Invalid size
        fclose(file);                                           // Close file
        return result;                                          // Return empty texture
    }
    
    // Validate bits per pixel (must be 24 or 32)
    if (ihdr.bits_per_pixel != 24 && ihdr.bits_per_pixel != 32) {
        printf("Unsupported BMP format (must be 24 or 32 bit): %s\n", filename);  // Unsupported
        fclose(file);                                           // Close file
        return result;                                          // Return empty texture
    }
    
    // Allocate memory for texture pixels from pool
    result.width = ihdr.width;                                  // Store texture width
    result.height = ihdr.height;                                // Store texture height
    result.pixels = (uint32_t*)g_pool.current;                  // Allocate from memory pool
    g_pool.current += (size_t)ihdr.width * ihdr.height * sizeof(uint32_t);  // Advance pool
    g_pool.used += (size_t)ihdr.width * ihdr.height * sizeof(uint32_t);     // Track usage
    
    // Seek to pixel data
    fseek(file, fhdr.pixel_offset, SEEK_SET);                   // Jump to pixel data offset
    
    // Read pixel data (BMP is upside down, bottom-up)
    for (int y = ihdr.height - 1; y >= 0; y--) {                // Read rows from bottom to top
        for (int x = 0; x < ihdr.width; x++) {                  // Read pixels left to right
            uint8_t b, g, r, a = 255;                           // Initialize pixel components
            
            // Read color components from file
            fread(&b, 1, 1, file);                              // Blue component (BMP first)
            fread(&g, 1, 1, file);                              // Green component
            fread(&r, 1, 1, file);                              // Red component
            
            // Read alpha for 32-bit BMPs
            if (ihdr.bits_per_pixel == 32) {
                fread(&a, 1, 1, file);                          // Alpha component (if 32-bit)
            }
            
            // Store as BGR (Windows convention)
            result.pixels[y * ihdr.width + x] = (b << 16) | (g << 8) | r;  // Write pixel
        }
    }
    
    // Close file and mark as loaded
    fclose(file);                                               // Close file handle
    result.loaded = 1;                                          // Mark texture as successfully loaded
    
    return result;                                              // Return loaded texture
}

// Create procedural texture when file not available
Texture create_procedural_texture(uint8_t type) {
    Texture result = {0};                                       // Initialize texture
    
    // Allocate memory for texture
    result.width = 256;                                         // 256x256 texture
    result.height = 256;                                        // Standard size
    result.pixels = (uint32_t*)g_pool.current;                  // Allocate from pool
    g_pool.current += 256 * 256 * sizeof(uint32_t);            // Advance pool pointer
    g_pool.used += 256 * 256 * sizeof(uint32_t);               // Track usage
    
    // Generate procedural pattern based on type
    for (int y = 0; y < 256; y++) {                             // For each row
        for (int x = 0; x < 256; x++) {                         // For each column
            uint32_t color = 0;                                 // Initialize pixel
            
            if (type == 0) {
                // Normal enemy - orange gradient with checkerboard
                uint8_t c = 100 + ((x + y) / 4) % 80;          // Orange base
                if ((x / 16 + y / 16) % 2) {                   // Checkerboard pattern
                    c += 30;                                    // Lighter squares
                }
                color = (0 << 16) | (100 << 8) | c;            // Orange in BGR
            } else if (type == 1) {
                // Red aggressive enemy - pure red with highlights
                uint8_t s = (x / 32 + y / 32) % 4;              // Stripe pattern
                uint8_t base = 120 + s * 20;                   // Red base with variation
                color = (0 << 16) | (base / 2 << 8) | base;    // Red in BGR
            } else if (type == 2) {
                // Boss enemy - magenta metallic effect
                uint8_t detail = ((x ^ y) / 8) % 100;           // Detail pattern
                uint8_t m = 150 + detail / 4;                  // Magenta base
                color = (m << 16) | (50 << 8) | m;             // Magenta in BGR
            }
            
            result.pixels[y * 256 + x] = color;                // Write pixel
        }
    }
    
    result.loaded = 1;                                          // Mark as loaded
    return result;                                              // Return procedural texture
}

// Load entity textures from BMP files with fallback
void load_entity_texture(void) {
    // Load entity textures from files in order
    for (int i = 0; i < MAX_TEXTURES; i++) {
        char filename[256];                                     // Buffer for filename
        snprintf(filename, sizeof(filename), "textures/entity_%d.bmp", i);  // Construct path
        
        // Try to load from disk
        g_textures[i] = load_bmp_file(filename);                // Attempt file load
        
        // If failed, use procedural fallback
        if (!g_textures[i].loaded) {
            printf("Using procedural texture for entity %d\n", i);  // Log fallback
            g_textures[i] = create_procedural_texture(i);      // Generate procedurally
        }
    }
}

// ===== MAZE GENERATION =====

// Generate random maze with rooms, corridors, obstacles
void generate_random_maze(void) {
    // Room structure for maze generation
    typedef struct {
        int x, y;                          // Center position
        int w, h;                          // Width and height
    } Room;
    
    // Initialize entire map as walls
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            g_world_map[y][x] = 1;                             // Solid wall
        }
    }
    
    // Generate rooms - large empty spaces scattered through maze
    Room rooms[32];                                             // Array of 32 rooms max
    int room_count = 0;                                         // Current room count
    
    for (int i = 0; i < 30; i++) {                              // Try to place 30 rooms
        int x = 50 + (rand() % (WORLD_WIDTH - 150));            // Random X (leave border)
        int y = 50 + (rand() % (WORLD_HEIGHT - 150));           // Random Y (leave border)
        int w = 30 + (rand() % 40);                             // Random width 30-70
        int h = 30 + (rand() % 40);                             // Random height 30-70
        
        // Check for room overlaps with existing rooms
        int overlaps = 0;                                       // Overlap counter
        for (int j = 0; j < room_count; j++) {
            int dx = abs(x - rooms[j].x);                       // X distance between rooms
            int dy = abs(y - rooms[j].y);                       // Y distance between rooms
            int min_dist = (w + rooms[j].w) / 2 + 20;          // Minimum separation needed
            
            if (dx < min_dist && dy < min_dist) {
                overlaps = 1;                                   // Mark as overlapping
            }
        }
        
        if (!overlaps && room_count < 32) {
            rooms[room_count].x = x;                            // Store room X
            rooms[room_count].y = y;                            // Store room Y
            rooms[room_count].w = w;                            // Store room width
            rooms[room_count].h = h;                            // Store room height
            room_count++;                                       // Increment count
            
            // Carve room into map
            for (int yy = y - h/2; yy < y + h/2; yy++) {
                for (int xx = x - w/2; xx < x + w/2; xx++) {
                    if (xx >= 0 && xx < WORLD_WIDTH && yy >= 0 && yy < WORLD_HEIGHT) {
                        g_world_map[yy][xx] = 0;               // Floor tile
                    }
                }
            }
        }
    }
    
    // Connect rooms with corridors
    for (int i = 0; i < room_count - 1; i++) {
        int x1 = rooms[i].x;                                    // Starting X
        int y1 = rooms[i].y;                                    // Starting Y
        int x2 = rooms[i+1].x;                                  // Ending X
        int y2 = rooms[i+1].y;                                  // Ending Y
        
        // Horizontal corridor first
        int corridor_x_start = (x1 < x2) ? x1 : x2;            // Start from left
        int corridor_x_end = (x1 < x2) ? x2 : x1;              // End at right
        for (int x = corridor_x_start; x < corridor_x_end; x++) {
            if (g_world_map[y1][x] != 0) {                     // If not already floor
                g_world_map[y1][x] = 0;                        // Carve corridor
            }
        }
        
        // Vertical corridor second
        int corridor_y_start = (y1 < y2) ? y1 : y2;            // Start from top
        int corridor_y_end = (y1 < y2) ? y2 : y1;              // End at bottom
        for (int y = corridor_y_start; y < corridor_y_end; y++) {
            if (g_world_map[y][x2] != 0) {                     // If not already floor
                g_world_map[y][x2] = 0;                        // Carve corridor
            }
        }
    }
    
    // Add obstacles in open areas (create puzzle sections)
    for (int i = 0; i < 50; i++) {                              // Place 50 obstacles
        int x = 50 + (rand() % (WORLD_WIDTH - 100));           // Random X (avoid edges)
        int y = 50 + (rand() % (WORLD_HEIGHT - 100));          // Random Y (avoid edges)
        int w = 5 + (rand() % 15);                              // Width 5-20 units
        int h = 5 + (rand() % 15);                              // Height 5-20 units
        
        // Carve obstacle shape into map
        for (int yy = y - h/2; yy < y + h/2; yy++) {
            for (int xx = x - w/2; xx < x + w/2; xx++) {
                if (xx >= 0 && xx < WORLD_WIDTH && yy >= 0 && yy < WORLD_HEIGHT) {
                    if (rand() % 2) {                           // 50% chance per block
                        g_world_map[yy][xx] = 1;               // Place wall block
                    }
                }
            }
        }
    }
    
    // Create border walls around entire map
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        g_world_map[y][0] = 1;                                 // Left border
        g_world_map[y][WORLD_WIDTH-1] = 1;                     // Right border
    }
    for (int x = 0; x < WORLD_WIDTH; x++) {
        g_world_map[0][x] = 1;                                 // Top border
        g_world_map[WORLD_HEIGHT-1][x] = 1;                    // Bottom border
    }
}

// ===== GAME INITIALIZATION =====

// Initialize all game state
void game_init(void) {
    // Allocate entity array
    g_entities = (Entity*)malloc(sizeof(Entity) * MAX_ENTITIES);  // Allocate entities
    memset(g_entities, 0, sizeof(Entity) * MAX_ENTITIES);        // Clear all entities
    g_entity_count = 0;                                          // No entities yet
    
    // Allocate projectile array
    g_projectiles = (Projectile*)malloc(sizeof(Projectile) * MAX_PROJECTILES);  // Allocate projectiles
    memset(g_projectiles, 0, sizeof(Projectile) * MAX_PROJECTILES);  // Clear all projectiles
    g_projectile_count = 0;                                       // No projectiles yet
    
    // Generate world maze
    generate_random_maze();                                       // Create procedural maze
    
    // Set player spawn position
    g_player.pos.x = 256.0;                                       // Center X coordinate
    g_player.pos.y = 256.0;                                       // Center Y coordinate
    g_player.pos.z = 1.0;                                         // Eye height 1.0 unit
    g_player.health = 100.0;                                      // Full health
    g_player.max_health = 100.0;                                  // Max capacity
    
    // Load textures with fallback
    load_entity_texture();                                        // Load or generate textures
    
    // Spawn initial enemies
    // Spawn 6 normal enemies to start
    for (int i = 0; i < 6; i++) {
        double x = 100.0 + (rand() % 150);                       // Random X between 100-250
        double y = 100.0 + (rand() % 150);                       // Random Y between 100-250
        spawn_normal_enemy(x, y, 0.0);                           // Spawn normal enemy
    }
    
    // Spawn 2 red aggressive enemies
    for (int i = 0; i < 2; i++) {
        double x = 150.0 + (rand() % 100);                       // Random X between 150-250
        double y = 150.0 + (rand() % 100);                       // Random Y between 150-250
        spawn_red_enemy(x, y, 0.0);                              // Spawn red enemy
    }
    
    // Respawn flag should be false initially
    g_is_dead = 0;                                                // Player not dead
    g_death_timer = 0.0;                                          // No death timer active
}

// Cleanup and free resources
void game_cleanup(void) {
    // Free entity array
    if (g_entities) {
        free(g_entities);                                         // Free entity memory
        g_entities = NULL;                                        // Clear pointer
    }
    
    // Free projectile array
    if (g_projectiles) {
        free(g_projectiles);                                      // Free projectile memory
        g_projectiles = NULL;                                     // Clear pointer
    }
    
    // Free texture pixel buffers
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

// ===== ENEMY SPAWNING =====

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

// ===== PLAYER SHOOTING =====

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

// Get procedural color based on texture ID and shading level
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

// Get texture detail pattern for procedural walls
static uint8_t get_texture_detail(double x, double y, uint8_t texture_id) {
    // Procedural detail via hash with more variation
    int ix = (int)x;
    int iy = (int)y;
    uint32_t hash = ((ix * 3731) ^ (iy * 8377)) & 0x7;
    // Different detail patterns for different textures
    return (hash + texture_id) % 4;
}

// ===== 3D RAYCASTING =====

// Cast ray and find intersection with world
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

// Update game logic and physics
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

// (game_render function continues in next section - too large for single edit)
// Rest of code is in the file...

// ===== WINDOWS API LAYER =====

// Forward declare window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// Create DIB section for backbuffer
static bool create_dib(int width, int height) {
    // Create device context
    HDC hdc = GetDC(NULL);                                          // Get screen device context
    if (!hdc) return false;
    
    // Create DIB section
    g_dib.width = width;                                            // Store width
    g_dib.height = height;                                          // Store height
    
    // Initialize DIB header structure
    BITMAPINFOHEADER bmi = {0};                                     // BMP info header
    bmi.biSize = sizeof(BITMAPINFOHEADER);                          // Header size
    bmi.biWidth = width;                                            // DIB width
    bmi.biHeight = -height;                                         // Negative for top-down
    bmi.biPlanes = 1;                                               // Single plane
    bmi.biBitCount = 32;                                            // 32-bit color
    bmi.biCompression = BI_RGB;                                     // No compression
    
    // Create DIB with pixel buffer
    g_dib.bitmap = CreateDIBSection(
        hdc,                                                         // Device context
        (BITMAPINFO*)&bmi,                                           // Bitmap info header
        DIB_RGB_COLORS,                                              // Use RGB colors
        (void**)&g_dib.bits,                                         // Get pixel buffer pointer
        NULL,                                                        // No file mapping
        0                                                            // No offset
    );
    
    if (!g_dib.bitmap) {
        ReleaseDC(NULL, hdc);                                        // Free device context
        return false;
    }
    
    // Store device context
    g_hdc = CreateCompatibleDC(hdc);                                // Create compatible DC
    if (!g_hdc) {
        DeleteObject(g_dib.bitmap);                                  // Delete DIB on failure
        ReleaseDC(NULL, hdc);
        return false;
    }
    
    SelectObject(g_hdc, g_dib.bitmap);                              // Select DIB into DC
    ReleaseDC(NULL, hdc);                                            // Release screen DC
    return true;
}

// Window procedure callback
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        // Window destroy message
        case WM_DESTROY:
            // Stop main loop
            g_running = false;                                       // Set running flag to false
            PostQuitMessage(0);                                      // Post quit message
            return 0;
        
        // Mouse left button down
        case WM_LBUTTONDOWN:
            // Handle left mouse button press
            player_shoot();                                          // Shoot on click
            return 0;
        
        // Keyboard key down
        case WM_KEYDOWN:
            // Handle key press
            {
                uint8_t vkey = (uint8_t)wparam;                     // Virtual key code
                
                // Check for ESC key to quit
                if (vkey == VK_ESCAPE) {
                    g_running = false;                              // Stop main loop
                    PostQuitMessage(0);                             // Post quit
                    return 0;
                }
                
                handle_key_input(vkey, 1);                           // Process key down
            }
            return 0;
        
        // Keyboard key up
        case WM_KEYUP:
            // Handle key release
            {
                uint8_t vkey = (uint8_t)wparam;                     // Virtual key code
                handle_key_input(vkey, 0);                           // Process key up
            }
            return 0;
    }
    
    // Default window procedure
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

// Initialize Windows window and DIB
bool windows_init(const char* title, int width, int height) {
    // Register window class
    WNDCLASSA wc = {0};                                              // Window class structure
    wc.style = CS_HREDRAW | CS_VREDRAW;                             // Redraw on resize
    wc.lpfnWndProc = WndProc;                                        // Window procedure callback
    wc.hInstance = GetModuleHandle(NULL);                            // Instance handle
    wc.lpszClassName = "BIDUL_WINDOW";                               // Class name
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);                       // Arrow cursor
    
    if (!RegisterClassA(&wc)) return false;                          // Register class
    
    // Create window
    g_hwnd = CreateWindowA(
        "BIDUL_WINDOW",
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, wc.hInstance, NULL
    );
    
    if (!g_hwnd) return false;
    
    if (!create_dib(width, height)) return false;
    
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    
    return true;
}

// Cleanup Windows resources
void windows_cleanup(void) {
    if (g_dib.bitmap) DeleteObject(g_dib.bitmap);
    if (g_hdc) DeleteDC(g_hdc);
    if (g_hwnd) DestroyWindow(g_hwnd);
}

// Poll Windows events
bool windows_poll_events(void) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    
    // Input now handled in WndProc/handle_key_input
    
    return g_running;
}

// Swap buffers: blit DIB to screen
void windows_swap_buffers(void) {
    HDC screen_dc = GetDC(g_hwnd);
    
    RECT rect;
    GetClientRect(g_hwnd, &rect);
    
    StretchDIBits(
        screen_dc,
        0, 0, rect.right - rect.left, rect.bottom - rect.top,
        0, 0, g_dib.width, g_dib.height,
        g_dib.bits,
        (BITMAPINFO*)&(BITMAPINFOHEADER){
            .biSize = sizeof(BITMAPINFOHEADER),
            .biWidth = g_dib.width,
            .biHeight = -g_dib.height,
            .biPlanes = 1,
            .biBitCount = 32,
            .biCompression = BI_RGB
        },
        DIB_RGB_COLORS,
        SRCCOPY
    );
    
    ReleaseDC(g_hwnd, screen_dc);
}

// ===== INPUT AND MISC =====

// Handle keyboard input
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

// Game render function - render 3D game world using raycasting
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
                uint32_t col = (c << 16) | ((c - 20) << 8) | (c - 30);  // Dark metallic BGR
                backbuffer[y * width + x] = col;
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

// ===== MAIN ENTRY POINT =====

// Main game entry point
int main(void) {
    // ===== INITIALIZE WINDOWS WINDOW =====
    // Create window and DIB section for framebuffer rendering
    if (!windows_init("Bidul Adventure", 640, 480)) {  // Create 640x480 window
        return 1;                          // Exit if window creation fails
    }
    
    // ===== INITIALIZE GAME =====
    // Set up memory pool and load game assets
    init();                                // Initialize memory pool (512 MB)
    
    // ===== LOAD GAME CONTENT =====
    // Initialize game entities, textures, map, enemies
    game_init();                           // Set up world, spawn initial enemies, load textures
    
    // ===== INITIALIZE TIMER =====
    // Set up high-resolution clock for 165 Hz frame rate
    clock_init();                          // Initialize QueryPerformanceCounter
    
    // ===== MAIN GAME LOOP =====
    // Run at 166 Hz target, process input and render frames
    while (windows_poll_events()) {        // Get Windows messages, check if running
        // ===== FRAME TIMING =====
        // Wait for next frame interval (6.06 ms per frame at 165 Hz)
        if (clock_tick()) {                // Check if frame time has elapsed
            game_update();                 // Update physics, enemies, projectiles
            game_render(g_dib.bits, g_dib.width, g_dib.height);  // Render 3D scene
            windows_swap_buffers();        // Display framebuffer on screen
        }
    }
    
    // ===== CLEANUP =====
    // Free all dynamically allocated memory
    game_cleanup();                        // Free entities, projectiles, textures
    windows_cleanup();                     // Free window and DIB resources
    return 0;
}
