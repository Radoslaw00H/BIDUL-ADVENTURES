@echo off
REM Build BIDUL ADVENTURES - 165Hz Doom-like Raycaster
REM Using GCC (MinGW-w64) compiler

if not exist "bin" mkdir bin

REM Compile with GCC
REM -O2 = optimize for speed
REM -I = include directories
REM -luser32 -lgdi32 = Windows API libraries
REM -lm = math library for cos/sin

echo Building BIDUL ADVENTURES...
gcc.exe -O2 ^
    -I src ^
    -I src/INITIALIZATION ^
    -I src/LOGIC ^
    -I src/WinAPI_HANDLER ^
    -Wl,--subsystem,windows ^
    src/main.c ^
    src/INITIALIZATION/init.c ^
    src/LOGIC/LOGIC.c ^
    src/WinAPI_HANDLER/WINDOWS_HANDLER.c ^
    -o bin/bidul_adventures.exe ^
    -luser32 -lgdi32 -lm

if errorlevel 1 (
    echo Build FAILED
    echo.
    pause
    exit /b 1
)

echo Build Complete. Running game...
echo.
bin\bidul_adventures.exe

pause
