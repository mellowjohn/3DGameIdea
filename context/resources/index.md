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
| Forum | font | World Forge cartography labels (Chaotic Imperium culture) | google/fonts ofl/forum; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/forum/` |
| EB Garamond | font | World Forge cartography labels (Cristallo culture) | google/fonts ofl/ebgaramond variable; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/eb-garamond/` |
| Cormorant Garamond | font | EB Garamond fallback for cartography labels | google/fonts ofl/cormorantgaramond variable; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/cormorant-garamond/` |
| Uncial Antiqua | font | World Forge cartography labels (Arrotrebae culture) | google/fonts ofl/uncialantiqua; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/uncial-antiqua/` |
| Metamorphous | font | World Forge cartography labels (orc warbands culture) | google/fonts ofl/metamorphous; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/metamorphous/` |
| MedievalSharp | font | World Forge display labels for ancient / draconian sites and inscriptions | google/fonts ofl/medievalsharp; author Wojciech Kalinowski; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/medievalsharp/` |
| Cartography stroke tiles | image | Transparent border / track / road / highway / ferry / river stamps for Map Canvas | generated 2026-07-20 (`tools/generate_cartography_strokes.py`) | Project-owned | `context/art/cartography/strokes/`; runtime `samples/open-world-rpg/assets/ui/cartography/strokes/` |
| Tessera cartography art kit | image | World Forge Map Canvas icons, heraldry chips, stroke references | generated 2026-07-20 (see PROVENANCE) | Project-owned | `context/art/cartography/`; runtime `samples/open-world-rpg/assets/ui/cartography/` |
| World Forge concept placeholders | image | Editor kind preview cards (person/deity/artifact/org/faction/region/poi) | generated 2026-07-15; resized 256² | Project-owned AI placeholders (see PROVENANCE) | `assets/world-forge/placeholders/` |
| Engine app branding icon | image | Windows exe / taskbar icon + editor menu bar, header, and boot splash | generated 2026-07-27; PNG + ICO | Project-owned (see PROVENANCE) | `assets/editor/branding/`; art `context/art/branding/` |
| ImGuizmo | library | Editor move, rotate, and scale gizmos | 2024-05-29#1 at pinned baseline | MIT | integrated editor viewport |
| Campfire crackle SFX | audio | Sample one-shot for interaction smoke | generated 2026-07-22 (procedural WAV) | Project-owned | `samples/open-world-rpg/assets/audio/campfire_crackle.wav` |
| Wind streak particle | image | Soft elongated S-curve alpha for ambient wind trails; morphing 4×4 flipbook | generated 2026-07-28 (`tools/gen_wind_streak.py`) | Project-owned | `samples/open-world-rpg/assets/vfx/wind_streak.png`, `wind_streak_flipbook_4x4.png` |
| Fire flipbook 4×4 | image | Stylized flame atlas for campfire particle flipbook | generated 2026-07-27 (`tools/gen_fire_flipbook.py`) | Project-owned | `samples/open-world-rpg/assets/vfx/fire_flipbook_4x4.png` |
| Smoke flipbook 4×4 | image | Dark curly smoke atlas for campfire plume layer | generated 2026-07-27 (`tools/gen_fire_flipbook.py`) | Project-owned | `samples/open-world-rpg/assets/vfx/smoke_flipbook_4x4.png` |
| Ember flipbook 4×4 | image | Tiny spark flecks for campfire ember layer | generated 2026-07-27 (`tools/gen_fire_flipbook.py`) | Project-owned | `samples/open-world-rpg/assets/vfx/ember_flipbook_4x4.png` |
| miniaudio | library | Audio device and playback | vcpkg baseline (`vcpkg.json`); [miniaudio](https://github.com/mackron/miniaudio) @ pinned baseline | Public domain or MIT-0 (dual-licensed; either applies) | integrated — `src/audio/audio_engine.cpp`; TICKET-0107 |
| spdlog | library | Production structured logging sink | pinned by baseline | MIT | planned integration |
| Catch2 | library | Unit and integration tests | pinned by baseline | BSL-1.0 | planned integration |
| Official Tessera world map | concept art | Clean continent/ocean geography (no towns/roads); Cartography discrete zoom plates | regenerate 2026-07-20 | Project-owned | overview `context/story/official-world-map.png`; master `context/art/cartography/world-map-master.png`; layers `samples/open-world-rpg/assets/ui/cartography/world-map-layers/`; notes `context/story/official-world-map.md` |
| Cartography map frame | image | Ornate parchment 9-slice / full-frame overlay for Map Canvas | generated 2026-07-20 | Project-owned | `context/art/cartography/frame/`; runtime `samples/open-world-rpg/assets/ui/cartography/frame/` |
| Cartography fog veil | image | Parchment mist overlay for discrete layer transitions | generated 2026-07-20 | Project-owned | `context/art/cartography/fog/`; runtime `samples/open-world-rpg/assets/ui/cartography/fog/` |
| Cartography icon + heraldry kit | image | Settlement/landmark icons, faction heraldry, border/travel stroke refs for Map Canvas | generated 2026-07-20 (PIL) | Project-owned | `context/art/cartography/`; runtime `samples/open-world-rpg/assets/ui/cartography/` |
| Forum | font | Imperium cartography map labels | google/fonts ofl/forum; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/forum/` |
| EB Garamond | font | Cristallo cartography map labels | google/fonts ofl/ebgaramond; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/eb-garamond/` |
| Cormorant Garamond | font | Cristallo alternate / packaging | google/fonts ofl/cormorantgaramond; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/cormorant-garamond/` |
| Uncial Antiqua | font | Arrotrebae cartography map labels | google/fonts ofl/uncialantiqua; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/uncial-antiqua/` |
| Metamorphous | font | Orc warband cartography map labels | google/fonts ofl/metamorphous; acquired 2026-07-20 | SIL OFL 1.1 | `assets/ui/fonts/metamorphous/` |
| Starting player Ashfell Blade turnaround (legacy) | concept art | Earlier Ashfell Blade / Squire kit sheet | 2026-07-03 import | Project-owned (author: project owner) | `context/art/reference/starting-player-squire-turnaround.png` |
| Starting player Ashfell Blade turnaround v2 | concept art | Ashfell Blade starter kit turnaround (first-pass concept) | generated 2026-07-28 | Project-owned AI concept | `context/art/reference/starting-player-ashfell-blade-turnaround.png` |
| Starting player Outrider turnaround v1 | concept art | Outrider Lodge starter kit turnaround (first-pass concept) | generated 2026-07-28 | Project-owned AI concept | `context/art/reference/starting-player-outrider-turnaround.png` |
| Starting player Runecaster turnaround v1 | concept art | Runecaster Guild starter kit turnaround (first-pass concept) | generated 2026-07-28 | Project-owned AI concept | `context/art/reference/starting-player-runecaster-turnaround.png` |
| Player character mesh v1 | mesh | Starting player visual for open-world RPG (Blockbench low-poly) | bake 2026-07-17 from Blockbench 5.1.4 export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/player.gltf`; source `tools/art/player/player.blockbench.gltf`; bake `tools/bake_player_gltf.py` |
| Player character atlas v1 | texture | Base-color atlas (256²) sampled as GPU albedo for the player mesh (TICKET-0191) | bake 2026-07-17 from Blockbench export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/player.png` (baked by `tools/bake_player_gltf.py`; embedded in source `tools/art/player/player.blockbench.gltf`) |
| Dead log mesh v1 | mesh | Fallen forest log prop for open-world RPG (Blockbench low-poly) | bake 2026-07-20 from Blockbench 5.1.4 export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/dead_log.gltf`; source `tools/art/dead-log/DeadLog.gltf`; bake `tools/bake_dead_log_gltf.py` |
| Dead log atlas v1 | texture | Base-color atlas for dead log mesh | bake 2026-07-20 from Blockbench export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/dead_log.png` |
| Stump mesh v1 | mesh | Cut forest stump prop for open-world RPG (Blockbench low-poly) | bake 2026-07-20 from Blockbench 5.1.4 export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/stump.gltf`; source `tools/art/stump/Stump.gltf`; bake `tools/bake_stump_gltf.py` |
| Stump atlas v1 | texture | Base-color atlas for stump mesh | bake 2026-07-20 from Blockbench export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/stump.png` |
| Barrel mesh v1 | mesh | Town/dock barrel prop (Blockbench low-poly) | bake 2026-07-27 from Blockbench export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/barrel.gltf`; source `tools/art/barrel/Barrel.gltf`; bake `tools/bake_tier1_props_gltf.py` |
| Barrel atlas v1 | texture | Base-color atlas for barrel mesh | bake 2026-07-27 from Blockbench export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/barrel.png` |
| Lantern mesh v1 | mesh | Hanging lantern prop + warm point light (Blockbench low-poly) | bake 2026-07-27 from Blockbench export (v2 backdrop clean) | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/lantern.gltf`; source `tools/art/lantern/Lantern.gltf`; bake `tools/bake_tier1_props_gltf.py` |
| Lantern atlas v1 | texture | Base-color atlas for lantern mesh | bake 2026-07-27 from Blockbench export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/lantern.png` |
| Wall torch mesh v1 | mesh | Wall-mounted torch prop + warm point light (Blockbench low-poly) | bake 2026-07-27 from Blockbench export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/wall_torch.gltf`; source `tools/art/wall-torch/Wall_Torch.gltf`; bake `tools/bake_tier1_props_gltf.py` |
| Wall torch atlas v1 | texture | Base-color atlas for wall torch mesh | bake 2026-07-27 from Blockbench export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/wall_torch.png` |
| Ashfell arming sword mesh v1 | mesh | Ashfell Blade starter arming sword (Blockbench low-poly) | bake 2026-07-28 from Blockbench 5.1.6 export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/ashfell_arming_sword.gltf`; source `tools/art/ashfell-arming-sword/Ashfell_Arming_Sword.gltf`; bake `tools/bake_tier1_props_gltf.py` |
| Ashfell arming sword atlas v1 | texture | Base-color atlas for Ashfell arming sword | bake 2026-07-28 from Blockbench export | Project-owned (author: project owner) | `samples/open-world-rpg/assets/models/ashfell_arming_sword.png` |
| Notion Wrathful Conquest | planning tool | Campaign hub + Engine Planning Board (Epics/Tickets) mirroring `EPIC`/`TICKET` IDs (DEC-0015) | board seeded 2026-07-10 | Notion proprietary SaaS; not redistributed | https://app.notion.com/p/39ad3efc569581309306e0d8e84cb026 |
| Vite / React / TypeScript | blog toolchain | Wrathful Conquest public GitHub Pages devlog (`blog/`) | Vite 8 / React 19 | MIT | `blog/` |
| react-router-dom | blog library | Client routes for archive and article pages | 7.x | MIT | `blog/` |
| marked | blog library | Markdown → HTML for posts | 18.x | MIT | `blog/` |
| Instrument Serif + Source Sans 3 | web fonts | Devlog display and body typography (Google Fonts CDN) | linked 2026-07-16 | SIL OFL 1.1 | `blog/index.html` |
| peaceiris/actions-gh-pages | CI action | Deploy `blog/dist` to `gh-pages` | v4 | MIT | `.github/workflows/deploy-blog.yml` |
| Formspree | SaaS | Optional blog email signup capture; owner sends updates manually | account as needed | Formspree proprietary SaaS; not redistributed | https://formspree.io/ |
| Devlog site imagery | image | Hero, OG share, and post covers for `blog/` | generated 2026-07-16; compressed WebP/JPEG | Project-owned AI placeholders | `blog/public/images/` |
| Engine blog captures | image | Launch-post screenshots (editor, world, World Forge, MCP) | captured 2026-07-16 via `engine editor/capture --hidden` | Project-owned | `blog/public/images/blog-*.webp`; source PPM in `out/captures/` |
| FFmpeg (Gyan full build) | platform tool | Encode article/screen captures for windows-computer-use MCP | 8.1.2 via winget `Gyan.FFmpeg` (2026-07-29) | LGPL/GPL FFmpeg build; local tool only, not redistributed | on PATH (`ffmpeg`, `ffprobe`) |
| windows-computer-use-mcp | MCP tool | Windows desktop screenshot + timed `record` (mp4 + frame montage) for blog/article media | git `8d367a8` (2026-07-29) | MIT | `tools/windows-computer-use/`; Cursor server `windows-computer-use` in `.cursor/mcp.json`; skill `skills/record-article-capture/` |

The foundation deliberately uses the standard library where practical. Replace internal sinks and the temporary test runner when their integrations land; preserve public contracts.

Do not add a dependency until its ownership, update strategy, and distribution implications are understood.
