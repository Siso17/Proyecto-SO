@echo off
echo ============================================================
echo   Compilando Simulador de Kernel - UCAB (Windows MinGW)
echo ============================================================
g++ -O2 -Wall -Wextra -Iinclude src/main.cpp src/pcb.cpp src/scheduler.cpp src/cli.cpp -o main.exe
if %ERRORLEVEL% EQU 0 (
    echo.
    echo [EXITO] Compilacion finalizada correctamente.
    echo Ejecutable generado: main.exe
    echo Para ejecutar introduzca: main.exe
) else (
    echo.
    echo [ERROR] Ocurrio un fallo durante la compilacion.
)
pause
