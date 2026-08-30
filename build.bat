@echo off
rem One-shot Windows build: produces pmm.exe and pdm.exe (needs MinGW gcc + curl in PATH)
set SRC=src\main.c src\json.c src\ini.c src\pmm.c src\http.c src\repo.c src\install.c src\sha256.c src\sha1.c src\mirrors.c src\pdm.c
gcc -O2 -Wall -static -Wextra -std=c11 -o pmm.exe %SRC%
if errorlevel 1 (echo pmm build failed & exit /b 1)
gcc -O2 -Wall -static -Wextra -std=c11 -o pdm.exe src\pdm_main.c src\pdm.c src\pmm.c src\sha256.c src\install.c src\http.c src\json.c src\ini.c src\mirrors.c src\sha1.c
if errorlevel 1 (echo pdm build failed & exit /b 1)
echo built pmm.exe and pdm.exe
