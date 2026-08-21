# Runtime Diagnostics

The engine writes one structured JSON Lines file per normal process by default:
`<project>/out/logs/engine-<mode>-<local-start-time>-pid<PID>.jsonl`.
This prevents concurrent editor, benchmark, and command processes from mixing their
events. Override the location with `--log-file <path>` when a caller owns log
routing.

**Editor Diagnostics → Console** is a separate in-panel session/cheat terminal for play-test inventory and flags (see [diagnostics-console.md](diagnostics-console.md)). It does not replace process logging.

Console output uses severity colors when attached to a Windows console: warnings are yellow and errors/fatal events are bright red. Every process writes session start and finish events. Each JSONL line includes `process` metadata (session ID, PID, start time, command mode, project, requested world, and build configuration). Recoverable errors include stable code, category, subsystem, correlation ID, remediation, causes, and source context. Rare structured events use `event` plus a JSON `data` object; editor performance logs the first threshold breach, a materially worse breach (5 FPS), then at most one heartbeat every 15 seconds. The in-editor dip history remains more detailed.

Every event also carries an independent priority label: `P0` critical immediate action, `P1` high, `P2` normal, or `P3` low. Console text shows both, for example `[error][P1]`; JSON exposes `severity` and `priority` separately.

The logger retains the latest 128 errors and a total error count for the planned in-engine console overlay. Log writes flush immediately so crash diagnostics preserve the latest event.
