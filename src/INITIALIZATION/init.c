// Initialization of the Windows Sounds etc

#define MEMORY_SIZE_ARR (192 * 1024 * 1024) // 192 MiB

#include "init.h"

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

static uint8_t MEMORY[MEMORY_SIZE_ARR];

// Memory allocator struct
typedef struct {
    uint8_t* MEMORY;
    uint32_t MEMORY_SIZE;
    uint32_t MEMORY_USED;
} MemoryPool;

// Global memory pool
static MemoryPool g_pool;

// Initialize game: memory, clock, window
void init(void) {
    // Setup memory pool
    g_pool.MEMORY = MEMORY;
    g_pool.MEMORY_SIZE = MEMORY_SIZE_ARR;
    g_pool.MEMORY_USED = 0;
}