@echo off
REM ===== BUILD SCRIPT FOR BIDUL ADVENTURES =====
REM Compiles Full 3D FPS with 512x512 maze, texture loading, and enemies
REM Using GCC (MinGW-w64) compiler with optimizations
REM Now: Single consolidated main.c with all code (no separate LOGIC or WINDOWS_HANDLER)

if not exist "bin" mkdir bin           REM Create output directory if missing

REM -O2 = Optimize for speed (but preserve debuggability)
REM -Wl,--subsystem,windows = Build as Windows GUI app (no console)
REM -luser32 -lgdi32 -lm = Link Windows GUI, GDI, and math libraries

echo Building BIDUL ADVENTURES - Consolidated Single File Edition...
gcc.exe -O2 ^
    -Wl,--subsystem,windows ^
    src/main.c ^
    -o bin/bidul_adventures.exe ^
    -luser32 -lgdi32 -lm

if errorlevel 1 (
    echo Build FAILED - Check errors above
    echo.
    pause
    exit /b 1
)

echo Build Complete!
echo.
echo NOTE: All game code is now in src/main.c
echo Textures load attempt order:
echo   1. PNG files from src/GRAPHICS_SOUNDS/GRAPHICS/
echo   2. BMP files (convert PNG to BMP if needed)
echo   3. Procedural patterns (fallback if files missing)
echo.
echo Running game...
echo.
bin\bidul_adventures.exe

pause
