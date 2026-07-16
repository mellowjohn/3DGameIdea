# Game and engine fonts

| Role | Face | Path |
| --- | --- | --- |
| **Engine / editor chrome** | Roboto | `roboto/Roboto-Regular.ttf` |
| **In-scene game UI** (HUD, menus, dialogue, titles) | Cinzel | `cinzel/Cinzel-Regular.ttf` |
| Diagnostics / console | JetBrains Mono | `jetbrains-mono/JetBrainsMono-Regular.ttf` |

All are SIL OFL 1.1 — keep each folder’s `OFL.txt` with redistributions.

- **Roboto:** transparent tooling UI (Inspector, Hierarchy Viewports chrome). Not used for player-facing game canvases.
- **Cinzel:** everything drawn as game content in the scene / Game viewport overlays.
- **JetBrains Mono:** developer console / IDs (`GameFonts::mono()`).

`source-sans-3/` may remain on disk as unused archive; preferred UI body for the engine is Roboto.

Provenance: `context/resources/index.md` and each folder’s `PROVENANCE.md`.
