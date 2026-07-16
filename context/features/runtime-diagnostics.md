# Runtime Diagnostics

The engine writes structured JSON Lines to `out/logs/engine.jsonl` by default. Override it with `--log-file <path>`.

Console output uses severity colors when attached to a Windows console: warnings are yellow and errors/fatal events are bright red. Every process writes session start and finish events. Recoverable errors include stable code, category, subsystem, correlation ID, remediation, causes, and source context.

Every event also carries an independent priority label: `P0` critical immediate action, `P1` high, `P2` normal, or `P3` low. Console text shows both, for example `[error][P1]`; JSON exposes `severity` and `priority` separately.

The logger retains the latest 128 errors and a total error count for the planned in-engine console overlay. Log writes flush immediately so crash diagnostics preserve the latest event.
