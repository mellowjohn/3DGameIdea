# TICKET-0228: Agent build coordination lease + editor dashboard

- Epic: EPIC-0001
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/3a9d3efc56958142a040e19f99ed8512

## Goal

Multiple agents share one Windows checkout and collide on `engine` rebuilds (locked `engine.exe`, LNK1168, stomped binaries). Ship a same-machine, file-backed **build coordination lease**: agents claim a ticket, acquire (or queue for) the rebuild slot through a command/MCP surface, and a Diagnostics **Coordination** dashboard shows who holds the lease, who is waiting, ticket status totals, and Act 0 MVP readiness claims.

## Context links

- [DEC-0003](../../decisions/index.md#dec-0003-automation-first-tools) — command-backed, deterministic automation
- [DEC-0015](../../decisions/index.md#dec-0015-hybrid-project-tracking) — `epics.md` is authoritative for ticket status; dashboard is read-only over it
- `.cursor/rules/rebuild-after-code-changes.mdc` + `.cursor/rules/reset-editor-after-edits.mdc` — the rebuild loop the lease serializes
- `context/formats/world-forge-mvp-readiness.md` — Act 0 MVP checklist consumed by the dashboard
- Feature doc: `context/features/agent-build-coordination.md`
- Precedents: `src/automation/live_automation_control.cpp` (`.engine/` request files), `src/automation/project_git_commands.cpp` (offline bridge-op shape)

## Acceptance criteria

- [x] `BuildCoordinator` persists `.engine/build-coordinator.json` at the coordination root (nearest ancestor with `context/planning/epics.md`, fallback project root); read-modify-write serialized by a path-keyed named mutex; atomic save
- [x] Actions `status | acquire | wait | release | heartbeat | clear-stale` via `engine build-coordination` CLI and `engine_build_coordination` MCP tool; both work offline (no live editor)
- [x] `acquire` requires `agentId` + `ticketId` + `summary`; `ticketId` is validated against the authoritative `epics.md` ticket tables; FIFO queue with per-agent dedupe; holder re-acquire is idempotent (same token)
- [x] Lease expiry and optional process-bound dead-owner-PID reclaim recorded as bounded events; `release` requires the current lease token; `clear-stale` clears expired/dead leases (`force` clears any, for owner use)
- [x] Read-only planning backlog reader parses `epics.md` ticket rows (id, title, status, priority, epic); never rewrites Markdown or Notion
- [x] Diagnostics → **Coordination** tab: active lease with expiry countdown and PID liveness, FIFO queue, recent events, ticket status totals, Act 0 MVP workstream view with linked ticket status + active claim badges, and a confirmed stale-lease clear action
- [x] `.engine/` coordinator state stays out of git; docs updated (`AGENTS.md`, rebuild/reset rules, feature doc `context/features/agent-build-coordination.md`)
- [x] `automation` suite covers FIFO ordering, duplicate-acquire idempotency, token-protected release, expiry/dead-PID recovery, malformed state recovery, ticket validation, and `epics.md` parsing; rebuild `engine`

## Out of scope

- Remote/multi-machine coordination or a hosted service
- Intercepting direct manual MSBuild invocations (documented bypass, shown as "outside coordinator control")
- Writing ticket status/ownership from the dashboard (repo-first + Notion flow stays authoritative)
- Wrapping the actual MSBuild invocation inside the engine

## Dependencies

None blocking. Consumes existing `WorldForgeMvpReadinessAsset` and `epics.md`; extends existing Diagnostics window.

## Verification

- Rebuild `engine` + `engine_suite_tests` (MSBuild Debug) — succeeded; pre-existing C4996 getenv warning in `render_app.cpp` only
- `engine_suite_tests --suite automation` — **155/155 pass**
- `engine_suite_tests --suite world_forge` — **277/277 pass**
- Two-client CLI handoff: A acquire → held across processes; B acquire → queued position 1 + `retryAfterMs`; B wait times out cleanly; A release → B wait acquires; B release → idle
- Diagnostics → Coordination tab wired (lease/queue/backlog/Act 0 MVP + clear/force-clear)

## What changed

### Summary

Agents on this machine now reserve the engine rebuild slot instead of colliding: `engine build-coordination` / MCP `engine_build_coordination` grant a timed, token-protected lease with a FIFO wait queue stored in `.engine/build-coordinator.json`. A new Diagnostics → Coordination tab visualizes the active lease, waiting agents, recent reclaim events, ticket status totals parsed read-only from `epics.md`, and the Act 0 MVP checklist with per-item ticket status and live claim badges.

### Files / surfaces

- Created: `include/engine/automation/build_coordination.h`, `src/automation/build_coordination.cpp`, `include/engine/automation/planning_backlog.h`, `src/automation/planning_backlog.cpp`, `context/features/agent-build-coordination.md`, `context/planning/tickets/TICKET-0228.md`
- Modified: `src/automation/command.cpp`, `src/automation/mcp_server.cpp`, `src/rendering/render_app.cpp` (Coordination tab), `CMakeLists.txt`, `.gitignore`, `AGENTS.md`, `.cursor/rules/rebuild-after-code-changes.mdc`, `.cursor/rules/reset-editor-after-edits.mdc`, `context/features/index.md`, `context/features/mcp-live-editor.md`, `context/planning/epics.md`, `tests/suite_tests.cpp`

### Schema / API / format deltas

- New state file `.engine/build-coordinator.json` (schemaVersion 1: `lease`, `queue[]`, `events[]`) — generated, git-ignored
- CLI: `engine build-coordination --project <dir> --action status|acquire|wait|release|heartbeat|clear-stale [--agent <id>] [--ticket TICKET-####] [--summary <text>] [--token <token>] [--lease-seconds N] [--timeout-seconds N] [--force] [--json]`
- MCP: `engine_build_coordination` (same params + optional `pid`; offline)
- Error codes `BUILD-COORD-*` (params, unknown ticket, token mismatch, not held, wait timeout, lock timeout, save failed)
- Metadata: `state`, `leaseToken` (acquire/wait/heartbeat success only), `leaseAgentId`, `leaseTicketId`, `leaseExpiresInMs`, `queuePosition`, `queueLength`, `retryAfterMs`, `queueJson`, `eventsJson`
- Default lease is **not** process-bound (`pid=0`) so transient CLI/MCP processes survive the kill→rebuild→restart loop; pass `pid` to opt into dead-owner reclaim

### Seed / sample data

- None. Coordinator state is runtime-generated; ticket catalog and Act 0 readiness reuse existing authored files.

### Tests / verification evidence

- `automation` 155/155; `world_forge` 277/277; `engine` rebuilt; two-client CLI handoff verified end-to-end

### Decisions & tradeoffs

- Enforcement is workflow-level: agents must acquire before MSBuild; a direct manual MSBuild cannot be intercepted and the dashboard labels the coordinator as advisory over such bypasses
- Coordination root anchors to the repo root (where `epics.md` lives) so all sample projects in one checkout share a single build lease
- Ticket IDs are validated strictly against `epics.md`; no free-form claims
- PID binding is opt-in because the mandated rebuild loop kills the MCP `engine.exe` while its agent still holds the lease

### Leftover risk / follow-ons

- A hung-but-alive holder keeps the slot until expiry (default 10 min) or owner Force clear
- Dashboard refresh is polling (2 s status / 10 s backlog); no push notification
- Multi-machine coordination would need a different backend (out of scope)
- Reload Cursor MCP after this rebuild to pick up `engine_build_coordination`

## Agent notes

Owner chose same-machine scope and enforced-for-agent-workflows policy (2026-07-26 planning session).
