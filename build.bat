@echo off
REM ===== BUILD SCRIPT FOR BIDUL ADVENTURES =====
REM Compiles Full 3D FPS with 512x512 maze, texture loading, and enemies
REM Using GCC (MinGW-w64) compiler with optimizations

if not exist "bin" mkdir bin           REM Create output directory if missing

REM Note: init.c is merged into LOGIC.c, so we don't compile it separately
REM -O2 = Optimize for speed (but preserve debuggability)
REM -I = Add include directory paths
REM -Wl,--subsystem,windows = Build as Windows GUI app (no console)
REM -luser32 -lgdi32 -lm = Link Windows GUI, GDI, and math libraries

echo Building BIDUL ADVENTURES - 512x512 Maze Edition...
gcc.exe -O2 ^
    -I src ^
    -I src/LOGIC ^
    -I src/WinAPI_HANDLER ^
    -Wl,--subsystem,windows ^
    src/main.c ^
    src/LOGIC/LOGIC.c ^
    src/WinAPI_HANDLER/WINDOWS_HANDLER.c ^
    -o bin/bidul_adventures.exe ^
    -luser32 -lgdi32 -lm

if errorlevel 1 (
    echo Build FAILED - Check errors above
    echo.
    pause
    exit /b 1
)

echo Build Complete! (311KB executable)
echo.
echo NOTE: Textures load attempt order:
echo   1. PNG files from src/GRAPHICS_SOUNDS/GRAPHICS/
echo   2. BMP files (convert PNG to BMP if needed)
echo   3. Procedural patterns (fallback if files missing)
echo.
echo Running game...
echo.
bin\bidul_adventures.exe

pause
