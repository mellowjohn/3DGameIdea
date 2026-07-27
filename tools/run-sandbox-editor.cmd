@echo off
setlocal
set ROOT=%~dp0..
set ENGINE=%ROOT%\build\windows-msvc-debug\Debug\engine.exe
set PROJECT=%ROOT%\samples\open-world-rpg
if not exist "%ENGINE%" (
  echo Missing %ENGINE% — rebuild the engine target first.
  exit /b 1
)
echo Launching sandbox editor...
echo   world: worlds/sandbox.world.json
echo Enable Diagnostics - MCP connection, then run context/testing/dialogue-sandbox-mcp.md scenarios.
"%ENGINE%" editor --project "%PROJECT%" --world worlds/sandbox.world.json %*
