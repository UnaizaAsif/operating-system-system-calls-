@echo off
REM ================================================================
REM run_all.bat  —  Run all Windows OS CCP programs and collect output
REM ================================================================

echo ============================================
echo  OS CCP  -  Windows Run All Programs
echo ============================================
echo.

REM Create input signal file
echo 1.0 > input_signal.txt
echo 2.0 >> input_signal.txt
echo 3.0 >> input_signal.txt
echo 4.0 >> input_signal.txt
echo 5.0 >> input_signal.txt
echo 6.0 >> input_signal.txt
echo 7.0 >> input_signal.txt
echo 8.0 >> input_signal.txt
echo [OK] input_signal.txt created

echo.
echo --- [1/6] File I/O ---
file_io_win.exe
echo.

echo --- [2/6] Process Management ---
process_win.exe
echo.

echo --- [3/6] Memory Management ---
memory_win.exe
echo.

echo --- [4/6] IPC ---
ipc_win.exe
echo.

echo --- [5/6] Permissions ---
permissions_win.exe
echo.

echo --- [6/6] FFT Main (All APIs Integrated) ---
fft_main_win.exe
echo.

echo ============================================
echo  syscalls_win.log contents:
echo ============================================
type syscalls_win.log
echo.

echo ============================================
echo  FFT Output:
echo ============================================
type fft_output_win.txt

echo.
echo ALL DONE. Screenshot this window for your report!
pause
