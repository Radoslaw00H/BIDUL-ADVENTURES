// Game code for BIDUL-ADVENTURES

#include "INITIALIZATION/init.h"


#include <windows.h>

// CLOCK

static LARGE_INTEGER g_freq;
static LARGE_INTEGER g_start;
static double g_tick_interval;

void clock_init(void) {
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);

    g_tick_interval = 1.0 / 165.0;
}

// Timer

int clock(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    double elapsed =
        (double)(now.QuadPart - g_start.QuadPart) /
        (double)g_freq.QuadPart;

    if (elapsed >= g_tick_interval){
        g_start.QuadPart +=
            (LONGLONG)(g_tick_interval * g_freq.QuadPart);

        return 1; // tick
    }
    return 0; // not yet
}


// all functions to pass to main
void tick(void) {
    static int CLOCK_CHECKER = 0;
        if (CLOCK_CHECKER == 0) {
        clock_init();
        CLOCK_CHECKER = 1;
    }
        else {
        clock();    
    }
}