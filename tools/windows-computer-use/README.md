# Windows Computer Use MCP

Local Windows desktop capture / record MCP for article stills, motion montages, and optional play-test loops.

- Package: [sshh12/windows-computer-use-mcp](https://github.com/sshh12/windows-computer-use-mcp) (MIT)
- Why not Screencast MCP: that package is **CC-BY-NC-ND**, which this repo rejects for dependencies and tools.

## Prerequisites

- Windows 10/11
- Python 3.10+ (`py -3.12` on this machine)
- FFmpeg on `PATH` (`winget install Gyan.FFmpeg`)

## Bootstrap (once per checkout)

```powershell
cd tools\windows-computer-use
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
```

`requirements.txt` pins `mcp>=1.2.0,<2`. MCP SDK 2.x drops `mcp.server.fastmcp` and the server will fail discovery with `ModuleNotFoundError`.

Cursor loads it from `.cursor/mcp.json` as server `windows-computer-use`. Captures land in `out/article-captures/` (gitignored). After a config change, reload MCP / restart Cursor.

## Agent workflow

Use skill `skills/record-article-capture/SKILL.md` for blog / article media.
