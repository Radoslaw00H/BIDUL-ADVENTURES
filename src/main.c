// BIDUL ADVENTURES - Full 3D FPS with Enemies, Combat, and Health
// Main entry point: Initialize and run game loop

#include "init.h"
#include "WINDOWS_HANDLER.h"
#include "LOGIC.h"
#include <windows.h>
#include <stdbool.h>

int main(void) {
    // Init Windows window + DIB section
    if (!windows_init("Bidul Adventure", 640, 480)) {
        return 1;
    }
    
    // Init memory pool
    init();
    
    // Init game logic (entities, projectiles, world)
    game_init();
    
    // Init high-res clock
    clock_init();
    
    // Main game loop - 165 Hz target
    while (windows_poll_events()) {
        // Wait for next frame tick
        if (clock_tick()) {
            game_update();  // Physics/logic/enemies/projectiles
            game_render(g_dib.bits, g_dib.width, g_dib.height);  // 3D raycasting
            windows_swap_buffers();  // Blit to screen
        }
    }
    
    // Cleanup
    game_cleanup();  // Free dynamic memory
    windows_cleanup();
    return 0;
}
