# TICKET-0279: Async MCP job runner for heavy recipes

- Epic: EPIC-0009
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: (mirror pending — no Notion MCP in this session)

## Goal

Agents can submit large terrain/scene/water polish recipes through MCP without hitting the 60s named-pipe bridge timeout or freezing the editor — jobs run chunked across frames with status/wait/cancel.

## Context links

- `context/features/mcp-live-editor.md`
- `.cursor/rules/mcp-no-python-substitutes.mdc`
- `skills/live-editor-mcp/SKILL.md`
- Related: TICKET-0274 (terrain/scene graybox helpers), TICKET-0245 (bake already offline+reload for long work)

## Acceptance criteria

- [x] MCP tool `engine_job_call` with `kind=submit|status|wait|cancel|list`
- [x] `submit` accepts `ops[]` with `target` terrain|scene|water + same action fields as existing apply tools; returns `jobId` immediately (`status=queued`)
- [x] Editor ticks ops across frames (`opsPerFrame`, default 1) so bridge stays responsive
- [x] `wait` polls until `succeeded|failed|cancelled` (or `timeoutSeconds`)
- [x] `cancel` stops queued/running jobs
- [x] Named `automation` suite covers submit → tick → succeeded, empty reject, cancel
- [x] Feature note + live-editor skill mention heavy recipes use `engine_job_call`

## Out of scope

- Arbitrary Lua/Python script execution inside the editor
- Parallel multi-job execution (queue is serial: one running job)
- Splitting a single heavy op (e.g. one huge `set_height_along`) across frames — ops are the chunk unit
- Desktop Diagnostics UI panel for job progress (optional follow-on)

## Dependencies

Soft prerequisite: live editor MCP (TICKET-0274 helpers useful as op vocabulary).

## Verification

- Rebuild `engine` + `engine_suite_tests` — succeeded (pre-existing getenv/hide warnings only)
- `engine_suite_tests --suite automation --json` → **214/214 passed**
- Desktop: editor status reports `mcpJobsPending` / `mcpJobsHasWork` after rebuild; Cursor host may need **MCP server reload** to list `engine_job_call` (binary embeds the tool; host schema was stale in-session)

## What changed

- Summary: Live editor now owns an async MCP job queue. Agents submit multi-op terrain/scene/water recipes via `engine_job_call`, get a `jobId` immediately, and poll/`wait` while the editor applies one (or few) ops per frame — avoiding the 60s bridge timeout that killed heavy A0-03 sculpt batches.
- Files / surfaces touched: `mcp_job_queue.{h,cpp}`, `editor_session` (`job_call` + status metadata), `render_app` (queue + per-frame tick), `mcp_server` (`engine_job_call` + wait loop), CMake, automation suite tests, `mcp-live-editor.md`, features index, live-editor skill, `epics.md` + this stub.
- Schema / API / format deltas: new MCP tool `engine_job_call` (`submit|status|wait|cancel|list`); bridge op `job_call`; error codes `JOB-*`; editor_status metadata `mcpJobsPending` / `mcpJobsHasWork`.
- Seed / sample data: none.
- Tests / verification evidence: automation suite 214/214; rebuild Debug `engine` + `engine_suite_tests`.
- Decisions & tradeoffs: serial single active job; chunk unit = one apply op (not densified strip samples); wait implemented in MCP proxy like `seek_times`.
- Leftover risk / follow-ons: Cursor must reload the engine MCP server to advertise `engine_job_call`; Notion card not mirrored this session; optional Diagnostics job UI; optional mid-op chunking for huge strip grades.

## Agent notes

Owner requested async jobs after landfall terrain polish timed out. Implemented end-to-end in this session.
