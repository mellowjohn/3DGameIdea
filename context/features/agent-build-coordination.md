# Agent build coordination

Status: needs-approval

Same-machine rebuild lease so multiple AI agents sharing one Windows checkout take turns rebuilding `engine` instead of colliding on locked `engine.exe` / LNK1168. Ticket: [TICKET-0228](../planning/tickets/TICKET-0228.md).

## What it is

- A file-backed coordinator at `.engine/build-coordinator.json` (git-ignored), rooted at the checkout that owns `context/planning/epics.md`.
- A FIFO queue of agents waiting for the rebuild slot.
- A Diagnostics → **Coordination** dashboard that shows the active lease, queue, ticket status totals from `epics.md`, and Act 0 MVP readiness with claim badges.
- CLI + MCP surfaces that work **offline** (no live editor required).

It does **not** intercept a direct manual MSBuild invocation. Agents must follow the documented acquire → rebuild → release loop. Manual bypasses remain possible and are labeled as outside coordinator control in the dashboard.

## Required agent rebuild workflow

1. Claim a ticket from `context/planning/epics.md` (Status → `active`, Agent → `cursor-agent`).
2. **Acquire** the rebuild lease (or **wait** until granted). Do **not** start MSBuild first.
3. Stop any locked `engine.exe` (MCP/editor) that owns the Debug/dev-next binary.
4. Rebuild the affected target (`engine` for editor/runtime changes).
5. Restart the same MCP/editor command that was running.
6. **Release** the lease with the granted token.

CLI:

```text
engine build-coordination --project samples/open-world-rpg --action acquire \
  --agent <agentId> --ticket TICKET-#### --summary "<short work>" --json

# ... kill locked engine.exe, MSBuild engine, restart MCP/editor ...

engine build-coordination --project samples/open-world-rpg --action release \
  --token <leaseToken> --json
```

MCP tool: `engine_build_coordination` with the same params. Queued responses report `state=queued`, `queuePosition`, and `retryAfterMs` — poll with `wait` (or retry `acquire`); never start MSBuild while queued.

## Actions

| Action | Requires | Behavior |
| --- | --- | --- |
| `status` | — | Lease + queue + recent events (never returns the lease token) |
| `acquire` | `agentId`, `ticketId`, `summary` | Grants if free and first in line; otherwise queues (per-agent dedupe). Holder re-acquire refreshes expiry with the same token |
| `wait` | same as acquire; optional `timeoutSeconds` (default 60) | Polls until granted or timeout; queue position is preserved across retries |
| `release` | `token` | Only the current holder may release |
| `heartbeat` | `token` | Extends expiry while a long rebuild is in progress |
| `clear-stale` | optional `force` | Drops expired / process-bound dead-owner leases; `force` clears a live lease (owner use) |

`ticketId` must exist in the authoritative `epics.md` ticket tables. Optional `leaseSeconds` (default 600, max 3600). Optional `pid` binds the lease to a long-lived process for dead-owner reclaim; leave unset for transient CLI/MCP processes (expiry-only reclaim — the rebuild loop often kills the MCP engine process while its lease is still held).

## State file

```text
.engine/build-coordinator.json
```

Schema version 1: `lease` (or null), `queue[]`, `events[]` (bounded). Serialized by a path-keyed Windows named mutex; written atomically via temp + rename. Corrupt files reset to idle with a `state_recovered` event.

## Dashboard

Diagnostics → **Coordination** (read-only over ticket ownership):

- Active lease with expiry countdown and optional PID liveness
- FIFO waiting agents
- Recent reclaim / grant / release events
- Ticket status totals + active ticket list from `epics.md`
- Act 0 MVP checklist with linked ticket status and “building now / queued” claim badges
- Clear stale / Force clear (confirmed) for stuck leases

## Recovery

- Expired leases and process-bound dead owners are reclaimed on the next status/acquire/wait.
- Ancient queue entries (30+ minutes) and process-bound dead waiters are dropped.
- Force clear is for owner use when a hung-but-alive process holds the slot past usefulness.

## Related

- [DEC-0003](../decisions/index.md#dec-0003-automation-first-tools) — command-backed automation
- [DEC-0015](../decisions/index.md#dec-0015-hybrid-project-tracking) — `epics.md` is authoritative; dashboard does not write ticket status
- `.cursor/rules/rebuild-after-code-changes.mdc`, `.cursor/rules/reset-editor-after-edits.mdc`
- Act 0 MVP asset: `context/formats/world-forge-mvp-readiness.md`
