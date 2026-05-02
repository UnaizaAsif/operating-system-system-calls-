@echo off
REM ================================================================
REM build_all.bat  —  Compile all Windows OS CCP programs
REM Requires: MinGW-w64 (gcc) OR Visual Studio (cl)
REM ================================================================

echo ============================================
echo  OS CCP  -  Windows Build Script
echo ============================================
echo.

REM Check for gcc (MinGW)
where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [MinGW] gcc found - using MinGW compiler
    goto :USE_GCC
)

REM Check for MSVC cl
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [MSVC] cl found - using MSVC compiler
    goto :USE_MSVC
)

echo ERROR: Neither gcc nor cl found!
echo Install MinGW-w64 from https://winlibs.com
pause
exit /b 1

REM ── MinGW-w64 Build ─────────────────────────────────────────────
:USE_GCC
echo.
echo [1/5] Compiling file_io_win.c ...
gcc -Wall -Wno-unused-function -o file_io_win.exe file_io_win.c
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [2/5] Compiling process_win.c ...
gcc -Wall -Wno-unused-function -o process_win.exe process_win.c
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [3/5] Compiling memory_win.c ...
gcc -Wall -Wno-unused-function -o memory_win.exe memory_win.c
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [4/5] Compiling ipc_win.c ...
gcc -Wall -Wno-unused-function -o ipc_win.exe ipc_win.c
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [5/5] Compiling permissions_win.c ...
gcc -Wall -Wno-unused-function -o permissions_win.exe permissions_win.c -ladvapi32
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [6/6] Compiling fft_main_win.c ...
gcc -Wall -Wno-unused-function -o fft_main_win.exe fft_main_win.c -lm -ladvapi32
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

goto :DONE

REM ── MSVC Build ───────────────────────────────────────────────────
:USE_MSVC
echo.
echo [1/5] Compiling file_io_win.c ...
cl /W3 /nologo file_io_win.c /link /out:file_io_win.exe
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [2/5] Compiling process_win.c ...
cl /W3 /nologo process_win.c /link /out:process_win.exe
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [3/5] Compiling memory_win.c ...
cl /W3 /nologo memory_win.c /link /out:memory_win.exe
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [4/5] Compiling ipc_win.c ...
cl /W3 /nologo ipc_win.c /link /out:ipc_win.exe
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [5/5] Compiling permissions_win.c ...
cl /W3 /nologo permissions_win.c advapi32.lib /link /out:permissions_win.exe
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

echo [6/6] Compiling fft_main_win.c ...
cl /W3 /nologo fft_main_win.c advapi32.lib /link /out:fft_main_win.exe
if %ERRORLEVEL% NEQ 0 ( echo FAILED & pause & exit /b 1 )
echo      OK

:DONE
echo.
echo ============================================
echo  ALL COMPILED SUCCESSFULLY
echo ============================================
echo.
echo Now create input_signal.txt then run:
echo   run_all.bat
pause
