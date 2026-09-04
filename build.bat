@echo off
rem One-shot Windows build: produces pmm.exe (needs MinGW gcc + curl in PATH)
set SRC=src\main.c src\json.c src\ini.c src\pmm.c src\http.c src\repo.c src\install.c src\sha256.c src\sha1.c src\mirrors.c src\pdm.c src\out.c
gcc -O2 -Wall -static -Wextra -std=c11 -o pmm.exe %SRC%
if errorlevel 1 (echo pmm build failed & exit /b 1)
echo built pmm.exe
