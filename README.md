# Wrathful Conquest / AI RPG Engine

Windows-first C++20 and Direct3D 12 engine for a third-person open-world action RPG. Built so humans and AI coding agents can collaborate on the same runtime, editor, and project content.

The game project lives under `samples/open-world-rpg`. Engine code is in `src/` and `include/`. Durable product memory (decisions, features, formats, backlog) is in [`context/`](context/README.md). Agent workflow rules are in [`AGENTS.md`](AGENTS.md).

## Requirements

- Windows 10/11, x64
- Visual Studio 2019 (MSVC) with C++ desktop workload
- CMake 3.25+
- [vcpkg](https://vcpkg.io/) with `VCPKG_ROOT` set
- Direct3D 12 capable GPU

Dependencies are pinned in [`vcpkg.json`](vcpkg.json) (SDL3, EnTT, Jolt, Dear ImGui, Lua, miniaudio, and others).

## Build

From the repo root:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset debug --target engine
```

Optional: build tests with target `engine_suite_tests`.

Binaries land in `build/windows-msvc-debug/Debug/`.

After C++ changes, rebuild at least the `engine` target. If `engine.exe` is locked by a running editor or MCP process, stop that process first, then rebuild.

## Run

Use the sample project unless you have another project root:

```powershell
$engine = "build\windows-msvc-debug\Debug\engine.exe"
$project = "samples\open-world-rpg"

# Editor (World Forge, Scene, Sculpt, play test)
& $engine editor --project $project

# MCP stdio server for Cursor / live editor automation
& $engine mcp --project $project

# Validate authored assets
& $engine validate --project $project

# Named test suites (or use engine_suite_tests)
& $engine test --project $project --suite collision
```

Useful extras:

- `engine run --debug-world` — lightweight debug traversal world
- `engine editor --project samples/open-world-rpg --coop-local` — local dual-slot co-op prove-out
- Cursor MCP: [`.cursor/mcp.json`](.cursor/mcp.json) launches `tools/mcp-server.cmd`

## Layout

| Path | Role |
|------|------|
| `src/`, `include/` | Engine runtime, editor, automation |
| `tests/` | Suite tests |
| `samples/open-world-rpg/` | Authoritative sample game project |
| `context/` | Architecture, decisions, features, formats, planning, story |
| `skills/` | Agent skills (ticket workflow, interviews, QA) |
| `blog/` | Public devlog (GitHub Pages) |
| `tools/` | Helper scripts and MCP launcher |

## Documentation

- [`context/README.md`](context/README.md) — context library index
- [`context/architecture/overview.md`](context/architecture/overview.md) — system boundaries
- [`context/architecture/content-vs-engine-workflows.md`](context/architecture/content-vs-engine-workflows.md) — C++ vs MCP content edits
- [`context/roadmap.md`](context/roadmap.md) — milestone gates
- [`context/planning/epics.md`](context/planning/epics.md) — epic/ticket backlog
- [`context/features/mcp-live-editor.md`](context/features/mcp-live-editor.md) — live editor MCP tools
- [`AGENTS.md`](AGENTS.md) — rules for coding agents

## License and assets

Third-party licenses and provenance are tracked in [`context/resources/index.md`](context/resources/index.md). Prefer permissively licensed dependencies; do not add assets with unclear or non-commercial terms.
