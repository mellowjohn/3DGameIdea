# Resource Registry

Track every external dependency, tool, reference, and nontrivial asset with its purpose, version or revision, provenance, and license.

## Intake policy

- Prefer permissive licenses such as MIT, BSD, zlib, Apache-2.0, BSL-1.0, CC0, and similarly compatible terms after reviewing the actual license text.
- Verify commercial use, modification, redistribution, attribution, notice-file, patent, trademark, and source-distribution conditions before importing a resource.
- Reject resources labeled noncommercial, personal use, editorial use, no derivatives, or lacking reliable author/source and license information.
- Do not assume that a resource is reusable because it is publicly downloadable, AI-generated, free of charge, or described as royalty-free.
- Record modifications to third-party resources and preserve required copyright and attribution notices in distributions.
- Escalate GPL, AGPL, LGPL, custom licenses, marketplace terms, and conflicting dual licenses for a specific compatibility decision before adoption.
- Windows, Direct3D, GPU drivers, and related platform SDK/runtime components are permitted only as documented platform requirements under their vendor terms; they are not project-owned or presumed modifiable.

For content assets, extend the registry entry with creator/source URL, acquisition date, original filename or asset ID, license evidence, required credits, and whether the distributed file was modified.

| Resource | Kind | Purpose | Version/revision | License | Location |
| --- | --- | --- | --- | --- | --- |
| CMake | build tool | Configure and generate MSVC builds | 4.3.3 portable | BSD-3-Clause | temporary tool bootstrap |
| vcpkg | package manager | Reproducible dependency graph | baseline `66c0373` (release 2026.01.16) | MIT | `vcpkg.json` |
| SDL3 | library | Window, input, platform lifecycle | 3.4.0 at pinned baseline | zlib | integrated in M2 |
| EnTT | library | ECS storage | 3.16.0 at pinned baseline | MIT | integrated in M3 |
| nlohmann JSON | library | Versioned project/scene/asset serialization | 3.12.0#2 at pinned baseline | MIT | integrated in M3 |
| Jolt Physics | library | World collision, queries, triggers, and character physics | 5.5.0 at pinned baseline | MIT | pinned for M4 integration |
| DirectX 12 Agility SDK | library | D3D12 runtime features | pinned by baseline | Microsoft | planned integration |
| DirectXTex | library | Texture processing | pinned by baseline | MIT | planned integration |
| EnTT | library | ECS storage foundation | pinned by baseline | MIT | planned integration |
| fastgltf | library | glTF 2.0 parsing and accessor decoding | 0.9.0 at pinned baseline | MIT | integrated initial mesh importer |
| simdjson | transitive library | fastgltf JSON parsing backend | 4.2.4 at pinned baseline | Apache-2.0 | transitive through fastgltf |
| Jolt Physics | library | Collision and rigid bodies | pinned by baseline | MIT | planned integration |
| Lua | library | Sandboxed gameplay scripting | pinned by baseline | MIT | integrated minimal event slice |
| Dear ImGui | library | Dockable editor interface with SDL3 and D3D12 bindings | 1.91.9 docking at pinned baseline | MIT | integrated editor MVP |
| Font Awesome Free (solid) | font/icons | Editor toolbar and viewport tab glyphs (`fa-solid-900.ttf`) | 6.x webfont | SIL OFL 1.1 (font); icons CC BY 4.0 | `assets/editor/fonts/` |
| Cinzel | font | In-scene game UI / HUD / menus / dialogue | google/fonts ofl/cinzel + fontsource Bold; acquired 2026-07-15 | SIL OFL 1.1 | `assets/ui/fonts/cinzel/` |
| Roboto | font | Engine / editor chrome (transparent tooling UI) | google/fonts ofl/roboto + fontsource weights; acquired 2026-07-15 | SIL OFL 1.1 | `assets/ui/fonts/roboto/` |
| Source Sans 3 | font | Archived candidate (superseded by Roboto for engine UI) | google/fonts ofl/sourcesans3; acquired 2026-07-15 | SIL OFL 1.1 | `assets/ui/fonts/source-sans-3/` |
| JetBrains Mono | font | Diagnostics / console / IDs | v2.304 release; acquired 2026-07-15 | SIL OFL 1.1 | `assets/ui/fonts/jetbrains-mono/` |
| World Forge concept placeholders | image | Editor kind preview cards (person/deity/artifact/org/faction/region/poi) | generated 2026-07-15; resized 256² | Project-owned AI placeholders (see PROVENANCE) | `assets/world-forge/placeholders/` |
| ImGuizmo | library | Editor move, rotate, and scale gizmos | 2024-05-29#1 at pinned baseline | MIT | integrated editor viewport |
| miniaudio | library | Audio device and playback | pinned by baseline | public domain or MIT-0 | planned integration |
| spdlog | library | Production structured logging sink | pinned by baseline | MIT | planned integration |
| Catch2 | library | Unit and integration tests | pinned by baseline | BSL-1.0 | planned integration |
| Starting player Squire turnaround | concept art | Player-character art direction for the Squire starting archetype | 2026-07-03 import | Project-owned (author: project owner) | `context/art/reference/starting-player-squire-turnaround.png` |
| Notion Wrathful Conquest | planning tool | Campaign hub + Engine Planning Board (Epics/Tickets) mirroring `EPIC`/`TICKET` IDs (DEC-0015) | board seeded 2026-07-10 | Notion proprietary SaaS; not redistributed | https://app.notion.com/p/39ad3efc569581309306e0d8e84cb026 |
| Vite / React / TypeScript | blog toolchain | Wrathful Conquest public GitHub Pages devlog (`blog/`) | Vite 8 / React 19 | MIT | `blog/` |
| react-router-dom | blog library | Client routes for archive and article pages | 7.x | MIT | `blog/` |
| marked | blog library | Markdown → HTML for posts | 18.x | MIT | `blog/` |
| Instrument Serif + Source Sans 3 | web fonts | Devlog display and body typography (Google Fonts CDN) | linked 2026-07-16 | SIL OFL 1.1 | `blog/index.html` |
| peaceiris/actions-gh-pages | CI action | Deploy `blog/dist` to `gh-pages` | v4 | MIT | `.github/workflows/deploy-blog.yml` |
| Formspree | SaaS | Optional blog email signup capture; owner sends updates manually | account as needed | Formspree proprietary SaaS; not redistributed | https://formspree.io/ |
| Devlog site imagery | image | Hero, OG share, and post covers for `blog/` | generated 2026-07-16; compressed WebP/JPEG | Project-owned AI placeholders | `blog/public/images/` |
| Engine blog captures | image | Launch-post screenshots (editor, world, World Forge, MCP) | captured 2026-07-16 via `engine editor/capture --hidden` | Project-owned | `blog/public/images/blog-*.webp`; source PPM in `out/captures/` |

The foundation deliberately uses the standard library where practical. Replace internal sinks and the temporary test runner when their integrations land; preserve public contracts.

Do not add a dependency until its ownership, update strategy, and distribution implications are understood.
