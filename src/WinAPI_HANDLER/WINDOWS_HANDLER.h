#ifndef WINDOWS_HANDLER_H
#define WINDOWS_HANDLER_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

// DIB section for backbuffer
typedef struct {
    HBITMAP bitmap;
    uint32_t* bits;  // 32-bit ARGB pixel data
    int width;
    int height;
} DIBSection;

// Window/display globals
extern HWND g_hwnd;
extern HDC g_hdc;
extern DIBSection g_dib;

// Init/cleanup
bool windows_init(const char* title, int width, int height);
void windows_cleanup(void);
bool windows_poll_events(void);
void windows_swap_buffers(void);

#endif // WINDOWS_HANDLER_H
