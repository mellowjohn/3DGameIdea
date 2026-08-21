# Act 0 Landfall Approach Graybox (A0-03)

Status: **draft** — first playable landing slice from LD `act0-ld-a0-03-approach-perspective.png`.

## World

- Path: `samples/open-world-rpg/worlds/landfall-approach.world.json`
- Presentation: `open_world` (terrain + water + placements)
- Open: `engine editor --project samples/open-world-rpg --world worlds/landfall-approach.world.json`

Shared project terrain/water stores were sculpted for this corridor (same `assets/terrain/*` as other open-world docs). Menu backdrop keep plateau near ~(300, 240) is reused as the distant Calrenoth gate silhouette.

## Route landmarks (SW → NE)

| Marker / entity | Role | Approx XZ |
| --- | --- | --- |
| `a003_player_spawn` | Play-test spawn (~1.8 m capsule; scale reference) | 141, 92 |
| `a003_scale_ruler_*` / `a003_scale_mannequin_*` | 1.8 m rulers + mannequins at spawn / choke / gate | — |
| `a003_marker_rescue` / `a003_wheelbarrow` | Arkand wheelbarrow staging | 143, 94 |
| `a003_marker_approach` | Mid forest road | 205, 155 |
| `a003_marker_choke` | Cleared final approach beat | 238, 174 |
| `a003_marker_gate` / `a003_gate_portal_clear` | Outer gate with distinct recessed double doors, piers, and lintel | 276, 202 |
| `a003_wall_link_s` | South curtain with gap aligned to approach | 298, 210 |
| `a003_wall_link_n` + e/w | Curtain ring + merlons | keep perimeter |
| `a003_keep_mass` (+ towers, barracks/stable) | Outer bailey silhouette (no solid fill) | 300, 240 |
| `a003_citadel_keep_mass` (+ `a003_citadel_tower_*`) | Raised inner citadel, scaled above the outer work | 350, 286 |
| `a003_citadel_inner_gate` | Second gate at the end of the winding bailey road | 337, 274 |
| `a003_cam_ld_perspective` | LD establishing camera | 95, 32, 40 |
| `a003_cam_look_target` | Framing look-at | 240, 170 |
| `a000_cam_landfall_title_wide` / `a000_cam_road_gate_reveal` | Opening-to-playable handoff framing | 116, 70 / 185, 130 |
| `a003_handoff_player_control` | Control returns after the opening wheelbarrow frame | 145, 96 |
| `a003_imperium_pressure_*` | Readable under-fire approach staging | 218, 165 / 244, 181 |
| `a003_cam_siege_pressure` | Siege-pressure review camera | 232, 202 |

## LD checklist vs sheet

- [x] Uphill single-file forest road toward keep
- [x] River / water on the right of the path
- [x] Wheelbarrow + crate/barrel/log staging at start
- [x] Mid-road chevaux choke
- [x] Gate silhouette + banners/torches
- [x] Player spawn + 1.8 m scale rulers/mannequins
- [x] Denser left-path forest + path-cleared foliage
- [x] Approach rocks / siege debris / wall links
- [x] Walkable ramp to gate (path grades kept under player `maxSlopeRatio` 0.45; choke props offset off centerline)
- [x] Readable dirt road paint (`assets/materials/dirt_road.material.json`, LD `#756848`) + mud shoulders + grass flanks; sparse timber edge markers
- [x] Passable gatehouse arch + south curtain gap (was solid wall block)
- [x] Keep graybox densified (bailey sheds, north curtain, merlons); island flatten/smooth pass
- [x] LD densify pass: left forest canopy wall, river-right carve+water, red banners, taller dark smoke, gate towers, staging/choke clutter, narrower dirt bed
- [x] Terrain polish via `engine_job_call` (soft south apron, strip-grade road, river banks, dirt/mud/grass paint + foliage clear); props re-snapped
- [x] LD pass-2: continuous road strip+smooth, left forest_floor, stone keep/river cliffs, meadow flanks, river carve/banks; gate approach kept walkable
- [x] LD pass-3: de-rib SE cliffs (`steep_cliff` + `smooth_natural`), re-strip+soft road, mud→dirt shoulders, meander river carve/banks + stone/mud paint; snap + save
- [x] Combat blockers / Imperium staging read (A0-04): flanking timber barricades, breach-rubble mass, and pressure anchors
- [x] Opening-to-playable handoff anchors (A0-00/A0-03): title wide, road/gate reveal, and control-return marker
- [x] Castle-and-landscape massing pass: paired gate bastions, crowned inner keep, stone promontory, and off-route approach ridges
- [x] Float-and-terrain cleanup: removed obsolete graybox smoke plumes, re-grounded displaced scene pieces, and smoothed the castle, road, and river transition tears
- [x] Sampled terrain blend pass: verified the spawn-to-gate grade, then layered dirt/mud road shoulders, sand/mud river banks, and forest-floor/stone transition bands
- [x] Gate readability polish: cleared obsolete spike barricades and centerline vegetation, softened the final road banks, and added recessed timber doors to the portal
- [x] Peninsula and citadel scale pass: expanded the inland platform, extended the outer-gate-to-citadel route into a winding 100 m bailey road, and added a larger raised inner keep with four grounded towers
- [x] Castle support and alignment pass: snapped lifted gate, tower, wall, crate, and barrel assets to terrain; leveled the citadel foundation; replaced detached tower instances with a single connected inner-curtain and corner-tower prefab
- [x] Citadel circulation pass: widened the inner precinct, aligned its south gate to the approach road, added static wall/keep collision, and graded/painted a narrow stone entry spur plus perimeter loop around the keep
- [x] Gate and courtyard traversal repair: reopened the outer portal by removing its blocking door leaves and flattened the localized citadel courtyard pockets to the shared 22 m interior grade
- [ ] Optional further terrain: jaggeder river silhouette, softer paint blend at road edge (mesh still somewhat faceted)
- [ ] Wire opening handoff / Landfall quest start
- [ ] Further keep art vs LD (crenellations/massing); play-test walk spawn→citadel and traverse the inner-citadel loop
- [ ] Tune LD camera framing vs sheet once densify settles

### Walkability note

Player `maxSlopeRatio` is **0.45** (~24°). An earlier climb spiked ~0.5+ between choke and gate (plus a height dip), so the controller slid/rejected the slope. Prefer **`set_height_along`** (MCP strip grade: lateral `halfWidth` + soft `skirtWidth`) for the corridor — overlapping circular `set_height` stamps read as a bumpy road. Keep island: **`flatten_pad` / `plateau`** with skirt. Target ramp ≈2.8→18 m with local ratios ≈0.06–0.20.

### Navigation read

Road uses a dedicated tan dirt material against green grass, with a mud shoulder ring and foliage cleared on the bed so the route stays obvious from spawn to gate.

## Screenshots

Under `samples/open-world-rpg/out/`:

- `a003-landfall-approach-overview-*.png`
- `a003-approach-ld-polish-*.png`
- `a003-spawn-uphill-view-*.png`
- `a003-player-spawn-final-*.png`
- `a003-scale-choke-mannequin-*.png`
- `a003-landfall-gate-*.png`
- `a003-landfall-rescue-staging-*.png`

## Related

- Concepts: `context/art/concepts/act0-ld-a0-03-approach-perspective.png`
- Tracker: TICKET-0269 (approach route); keep interior = TICKET-0270
