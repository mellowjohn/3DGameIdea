@echo off
setlocal
set ROOT=%~dp0..
set ENGINE=%ROOT%\build\windows-msvc-debug\dev-next\engine.exe
set PROJECT=%ROOT%\samples\open-world-rpg
if not exist "%ENGINE%" (
  echo Missing %ENGINE% — rebuild the engine target first.
  exit /b 1
)
echo Launching combat sandbox editor...
echo   world: worlds/combat-sandbox.world.json
"%ENGINE%" editor --project "%PROJECT%" --world worlds/combat-sandbox.world.json %*
