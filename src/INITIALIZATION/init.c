// Initialization of the Windows Sounds etc

#define MEMORY_SIZE_ARR (192 * 1024 * 1024) // 192 MiB

#include "init.h"

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

static uint8_t MEMORY[MEMORY_SIZE_ARR];

typedef struct {
    uint8_t* MEMORY;
    uint32_t MEMORY_SIZE;
    uint32_t MEMORY_USED;
}

// Global state structure for the game
