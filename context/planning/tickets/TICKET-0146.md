# TICKET-0146: GPU context in structured diagnostics/crash bundles

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: (see Notion Tickets DB by Ticket ID)

## Goal

Capture the active Direct3D adapter and device state in structured JSONL diagnostics and local crash bundles so rendering failures are actionable without hosted telemetry.

## Context links

- `context/planning/epics.md` (EPIC-0013)
- `context/roadmap.md`
- `context/decisions/index.md` (DEC-0004)
- `src/diagnostics/crash_bundle.cpp`
- `src/rendering/render_app.cpp`
- Related tickets in the same epic (see epics.md)

## Acceptance criteria

- [x] JSONL diagnostic entries include GPU adapter name, driver version, dedicated VRAM, Direct3D feature level, and device-removal HRESULT when known.
- [x] `diagnostic.json` crash bundles include the same structured GPU context.
- [x] Renderer records its selected adapter and device-removal HRESULT rather than relying only on a separately enumerated GPU.
- [x] Diagnostics regression coverage verifies both JSONL and crash-bundle payloads.
- [x] Status/Priority mirrored in Notion when work starts.
- [x] Context indexes updated if behavior or formats change.

## Out of scope

DRED breadcrumbs/page-fault reports, GPU crash dumps, hosted crash upload, and M11 packaging.

## Dependencies

Owner approved implementation on 2026-07-20. DRED is unavailable in the installed Windows SDK and remains out of scope.

## Verification

Rebuilt `engine` successfully. `engine_suite_tests --suite diagnostics --json` passed 9/9 assertions. The existing renderer warnings for unsafe `getenv` and `sscanf` remain unrelated to this ticket. Set Status to needs-approval after verification — never done.

## What changed

- Summary: Structured diagnostics now carry GPU context in every JSONL event and local crash bundle. The renderer replaces the initial hardware snapshot with its actual selected adapter and preserves a device-removal HRESULT when one occurs.
- Files / surfaces touched: Added `include/engine/diagnostics/gpu_diagnostics.h` and `src/diagnostics/gpu_diagnostics.cpp`; updated the logger, crash bundle writer, D3D12 renderer, CMake target, diagnostics regression suite, and Feature Inventory.
- Schema / API / format deltas: JSONL events and `diagnostic.json` now contain `gpuDiagnostics` with `available`, `adapterName`, `driverVersion`, `vendorId`, `deviceId`, `dedicatedVideoMemory`, `featureLevel`, and nullable `deviceRemovalHresult`.
- Seed / sample data: None.
- Tests / verification evidence: Rebuilt `engine`; rebuilt `engine_suite_tests`; `diagnostics` passed 9/9 assertions. Restarted the editor with the rebuilt executable.
- Decisions & tradeoffs: Capture a hardware snapshot at logger initialization, then replace it with the renderer-selected adapter to avoid reporting a different GPU on multi-adapter systems. DRED remains out of scope because the installed Windows SDK lacks its interfaces.
- Leftover risk / follow-ons: The device-removal field is null until a D3D12 removal is observed. GPU crash dumps, DRED breadcrumbs/page faults, and hosted upload remain deferred.

## Agent notes

Minimal approved scope: adapter name, driver version, dedicated VRAM, feature level, and device-removal HRESULT. Preserve diagnostics operation when no hardware D3D12 adapter is available. Work completed 2026-07-20; awaiting owner approval.
