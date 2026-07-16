@echo off
setlocal
cd /d "%~dp0\.."
"%~dp0\..\build\windows-msvc-debug\Debug\engine.exe" mcp --project "%~dp0\..\samples\open-world-rpg"
