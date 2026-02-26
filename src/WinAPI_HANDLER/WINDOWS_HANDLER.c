// Windows API layer: WinMain, window creation, DIB section
#include "WINDOWS_HANDLER.h"
#include "LOGIC.h"
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

// Globals
HWND g_hwnd = NULL;
HDC g_hdc = NULL;
DIBSection g_dib = {0};
static bool g_running = true;
static uint8_t g_keys[256] = {0};

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // Extern from LOGIC
    extern void handle_key_input(uint8_t key, int is_down);
    
    switch(msg) {
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wparam < 256) {
                g_keys[wparam] = 1;
                handle_key_input((uint8_t)wparam, 1);  // Call input handler
            }
            if (wparam == VK_ESCAPE) g_running = false;
            return 0;
        case WM_KEYUP:
            if (wparam < 256) {
                g_keys[wparam] = 0;
                handle_key_input((uint8_t)wparam, 0);  // Call input handler
            }
            return 0;
        default:
            return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}

// Create DIB section for backbuffer
static bool create_dib(int width, int height) {
    HDC screen_dc = GetDC(NULL);
    g_hdc = CreateCompatibleDC(screen_dc);
    ReleaseDC(NULL, screen_dc);
    
    BITMAPINFOHEADER bmi_header = {
        .biSize = sizeof(BITMAPINFOHEADER),
        .biWidth = width,
        .biHeight = -height,  // Negative = top-down
        .biPlanes = 1,
        .biBitCount = 32,
        .biCompression = BI_RGB,
        .biSizeImage = 0,
        .biXPelsPerMeter = 0,
        .biYPelsPerMeter = 0,
        .biClrUsed = 0,
        .biClrImportant = 0
    };
    
    void* bits = NULL;
    g_dib.bitmap = CreateDIBSection(g_hdc, (BITMAPINFO*)&bmi_header, DIB_RGB_COLORS, &bits, NULL, 0);
    
    if (!g_dib.bitmap) return false;
    
    g_dib.bits = (uint32_t*)bits;
    g_dib.width = width;
    g_dib.height = height;
    
    SelectObject(g_hdc, g_dib.bitmap);
    return true;
}

// Initialize window + DIB
bool windows_init(const char* title, int width, int height) {
    WNDCLASSA wc = {0};
    wc.style = CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "DoomLikeClass";
    
    if (!RegisterClassA(&wc)) return false;
    
    g_hwnd = CreateWindowExA(
        0, "DoomLikeClass", title,
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

// Cleanup
void windows_cleanup(void) {
    if (g_dib.bitmap) DeleteObject(g_dib.bitmap);
    if (g_hdc) DeleteDC(g_hdc);
    if (g_hwnd) DestroyWindow(g_hwnd);
}

// Poll events
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
