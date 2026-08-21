#!/usr/bin/env python3
"""Universal named asset bake orchestrator (player + registered props + editor imports).

Usage:
  python tools/asset_bake.py --list [--json]
  python tools/asset_bake.py --project samples/open-world-rpg --target player [--source path] [--json]
  python tools/asset_bake.py --register --id lamp_post --kind static --source path/to/Lamp.gltf [--json]

`--register` is what the editor's Assets → Import flow calls for a model that has no catalog
target yet: it caches a normalized copy of the source under tools/art/<id>/ and appends a
`generic_static` / `generic_skinned` entry so every later rebake is a named, verified bake.
"""
from __future__ import annotations

import argparse
import base64
import json
import shutil
import subprocess
import struct
import sys
import tempfile
import time
import zlib
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[1]
CATALOG_PATH = Path(__file__).resolve().parent / "asset_bake_catalog.json"

# Ensure tools/ is importable when spawned from engine cwd.
if str(REPO / "tools") not in sys.path:
    sys.path.insert(0, str(REPO / "tools"))

import asset_bake_verify as verify  # noqa: E402


def _gltf_named_joint_world(gltf: dict, joint_name: str) -> tuple[float, float, float]:
    from bake_player_v2_gltf import joint_world_matrices

    worlds = joint_world_matrices(gltf)
    nodes = gltf.get("nodes") or []
    for skin in gltf.get("skins") or []:
        for joint in skin.get("joints") or []:
            if not isinstance(joint, int) or joint < 0 or joint >= len(nodes):
                continue
            if nodes[joint].get("name") != joint_name:
                continue
            mat = worlds[joint]
            return (float(mat[12]), float(mat[13]), float(mat[14]))
    raise RuntimeError(f"joint {joint_name!r} not found")


def _match_player_bake_transform(project: Path, source_gltf: Path) -> tuple[float, tuple[float, float, float]]:
    """Reuse the player's height-normalize scale + Hips alignment so kit shells share bind space."""
    player_report = project / "assets" / "models" / "player.bake.json"
    player_mesh = project / "assets" / "models" / "player.gltf"
    if not player_report.is_file() or not player_mesh.is_file():
        raise RuntimeError("matchPlayerBake needs assets/models/player.bake.json and player.gltf")
    report = json.loads(player_report.read_text(encoding="utf-8"))
    bake = report.get("bake") or report
    scale = float(bake.get("scale") or 0.0)
    if scale <= 1e-8:
        raise RuntimeError("player.bake.json is missing a usable scale")
    player_gltf = json.loads(player_mesh.read_text(encoding="utf-8"))
    armor_gltf = json.loads(source_gltf.read_text(encoding="utf-8"))
    player_hips = _gltf_named_joint_world(player_gltf, "Hips")
    armor_hips = _gltf_named_joint_world(armor_gltf, "Hips")
    offset = (
        player_hips[0] - scale * armor_hips[0],
        player_hips[1] - scale * armor_hips[1],
        player_hips[2] - scale * armor_hips[2],
    )
    return scale, offset


def load_catalog() -> dict:
    return json.loads(CATALOG_PATH.read_text(encoding="utf-8"))


def find_target(catalog: dict, target_id: str) -> dict | None:
    for t in catalog.get("targets") or []:
        if t.get("id") == target_id:
            return t
    return None


def repo_relative(path: Path) -> str:
    """Repo-relative posix path, or the bare file name for sources outside the checkout."""
    try:
        return str(path.resolve().relative_to(REPO)).replace("\\", "/")
    except ValueError:
        return path.name


def append_catalog_target(entry: dict) -> None:
    """Insert one target, keeping the hand-authored formatting of the existing entries."""
    text = CATALOG_PATH.read_text(encoding="utf-8")
    close = text.rfind("]")
    last_entry_end = text.rfind("}", 0, close) if close >= 0 else -1
    if last_entry_end >= 0:
        block = "\n".join("    " + line for line in json.dumps(entry, indent=2).splitlines())
        updated = text[: last_entry_end + 1] + ",\n" + block + text[last_entry_end + 1 :]
        try:
            json.loads(updated)
        except json.JSONDecodeError:
            updated = None
        if updated is not None:
            CATALOG_PATH.write_text(updated, encoding="utf-8")
            return
    catalog = load_catalog()
    catalog.setdefault("targets", []).append(entry)
    CATALOG_PATH.write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")


def resolve_project(path: Path | None) -> Path:
    if path is None:
        return (REPO / "samples/open-world-rpg").resolve()
    p = path.resolve()
    if not p.exists():
        raise SystemExit(f"ASSET-BAKE-SOURCE-MISSING: project not found: {p}")
    return p


def snapshot_models(models_dir: Path) -> dict[str, float]:
    out: dict[str, float] = {}
    if not models_dir.exists():
        return out
    for p in models_dir.rglob("*"):
        if p.is_file():
            out[str(p.relative_to(models_dir)).replace("\\", "/")] = p.stat().st_mtime
    return out


def collateral_changes(before: dict[str, float], after: dict[str, float], allowed: set[str]) -> list[str]:
    changed = []
    for key, mtime in after.items():
        if key in allowed:
            continue
        if key not in before or abs(mtime - before[key]) > 1e-6:
            changed.append(key)
    return sorted(changed)


def copy_if_newer(src: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if not dest.exists() or src.stat().st_mtime > dest.stat().st_mtime:
        shutil.copy2(src, dest)


def player_bbmodel_candidates(explicit: Path | None = None) -> list[Path]:
    """Authoring Blockbench projects that supersede a native Blockbench glTF export."""
    ordered: list[Path] = []
    if explicit is not None:
        ordered.append(explicit.resolve())
    ordered.extend(
        [
            REPO / "tools/art/player/GoodPlayerModel_rigged.bbmodel",
            Path(r"c:\Users\johnr\Documents\GoodPlayerModel_rigged.bbmodel"),
            Path(r"c:\Users\johnr\Documents\GoodPlayerModel.bbmodel"),
            Path(r"c:\Users\johnr\Documents\Models\GoodPlayerModel_rigged.bbmodel"),
        ]
    )
    seen: set[Path] = set()
    out: list[Path] = []
    for path in ordered:
        key = path.resolve() if path.exists() else path
        if key in seen:
            continue
        seen.add(key)
        out.append(path)
    return out


def _player_art_mesh_ok(art_gltf: Path) -> bool:
    """True when the art glTF still has a Blockbench-quality mesh (not our crude bbmodel exporter)."""
    if not art_gltf.exists():
        return False
    try:
        g = json.loads(art_gltf.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    generator = str((g.get("asset") or {}).get("generator") or "")
    if "export_goodplayer_bbmodel_gltf" in generator:
        return False
    try:
        prim = g["meshes"][0]["primitives"][0]
        verts = int(g["accessors"][prim["attributes"]["POSITION"]]["count"])
    except (KeyError, IndexError, TypeError, ValueError):
        return False
    # Native Blockbench GoodPlayerModel exports land ~3k verts; our mesh exporter ~900.
    return verts >= 2000


def refresh_player_gltf_from_bbmodel(src_path: Path) -> tuple[Path, dict[str, Any]]:
    """Sync player animation clips from the Blockbench project without replacing mesh/UVs.

    Native Blockbench glTF export often truncates clip lengths (Attack 0.75s vs bbmodel 1.15s),
    but the Blockbench glTF mesh/UV atlas is the textured source of truth. Full bbmodel→glTF
    mesh export destroys UV seams — only use it when no usable art glTF exists.
    """
    import export_goodplayer_bbmodel_gltf as exporter

    art_gltf = REPO / "tools/art/player/GoodPlayerModel.gltf"
    bb: Path | None = None
    if src_path.suffix.lower() == ".bbmodel" and src_path.exists():
        bb = src_path.resolve()
    else:
        for candidate in player_bbmodel_candidates():
            if candidate.exists():
                bb = candidate.resolve()
                break
    meta: dict[str, Any] = {
        "authoringSource": str(bb) if bb else None,
        "refreshedFromBbmodel": False,
        "animationSyncOnly": False,
    }
    if bb is None:
        return src_path if src_path.exists() else art_gltf, meta

    png = exporter.first_existing(exporter.SRC_PNG_CANDIDATES)
    if _player_art_mesh_ok(art_gltf):
        clips = exporter.sync_animations_from_bbmodel(art_gltf, bb, png)
        extras = json.loads(art_gltf.read_text(encoding="utf-8")).get("extras") or {}
        meta["refreshedFromBbmodel"] = True
        meta["animationSyncOnly"] = True
        meta["exportedClips"] = clips
        meta["animationsReplaced"] = extras.get("animationsReplaced") or []
        meta["animationsAdded"] = extras.get("animationsAdded") or []
        return art_gltf, meta

    # No usable textured mesh — last resort full export (UVs will be poorer).
    print(
        "WARNING: GoodPlayerModel.gltf missing or was overwritten by the crude bbmodel mesh "
        "exporter. Falling back to full bbmodel export; re-export glTF from Blockbench to restore "
        "textures, then Re-import."
    )
    outs = [art_gltf]
    for extra in exporter.OUT_PATHS:
        if extra.resolve() != art_gltf.resolve():
            outs.append(extra)
    exporter.export(bb, png, outs)
    meta["refreshedFromBbmodel"] = True
    meta["exportedClips"] = [
        a.get("name")
        for a in json.loads(art_gltf.read_text(encoding="utf-8")).get("animations") or []
        if a.get("name")
    ]
    return art_gltf, meta


def run_player_bake(source: Path | None, project: Path) -> dict[str, Any]:
    import bake_player_v2_gltf as player_bake

    src = source.resolve() if source is not None else REPO / "tools/art/player/GoodPlayerModel.gltf"
    bake_source, refresh_meta = refresh_player_gltf_from_bbmodel(src)
    bake_meta = player_bake.bake(source_override=bake_source, project_root=project)
    bake_meta.update(refresh_meta)
    return bake_meta


def run_tier1(target_id: str) -> None:
    script = REPO / "tools/bake_tier1_props_gltf.py"
    proc = subprocess.run(
        [sys.executable, str(script), target_id],
        cwd=str(REPO),
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"tier1 bake failed for {target_id}:\n{proc.stdout}\n{proc.stderr}"
        )


def run_generic_static(target: dict, project: Path) -> None:
    """Bake an imported static model through the proven Tier-1 prop path (flatten + atlas clean)."""
    import bake_tier1_props_gltf as tier1

    generic = target.get("generic") or {}
    outputs = target.get("outputs") or {}
    prop = {
        "name": target["id"],
        "src": REPO / target["defaultSource"],
        "dst": project / outputs["mesh"],
        "png": project / outputs["atlas"],
        "target_height": float(generic.get("targetHeight", 1.0)),
        "generator": generic.get("generator", f"AI RPG Engine {target['id']} generic bake"),
        "scene_name": generic.get("sceneName", target["id"]),
        "mesh_name": generic.get("meshName", target["id"]),
        "mat_name": generic.get("materialName", target["id"] + "Atlas"),
    }
    for key, prop_key in (
        ("maxAtlas", "max_atlas"),
        ("cleanBackdrop", "clean_backdrop"),
        ("doubleSided", "double_sided"),
        ("doubleSidedThin", "double_sided_thin"),
        ("foliageAtlas", "foliage_atlas"),
        ("scaleMode", "scale_mode"),
        ("targetSpan", "target_span"),
    ):
        if key in generic:
            prop[prop_key] = generic[key]
    tier1.bake_prop(prop)


def run_generic_skinned(target: dict, project: Path) -> dict[str, Any]:
    """Bake an imported skinned model, preserving the skeleton and every animation clip."""
    import bake_generic_gltf
    import export_goodplayer_bbmodel_gltf as bbmodel_exporter

    generic = target.get("generic") or {}
    outputs = target.get("outputs") or {}
    source = REPO / target["defaultSource"]
    atlas_policy = generic.get("atlasPolicy", "")
    if generic.get("requireDedicatedAtlas"):
        if atlas_policy not in {"dedicated_embedded", "dedicated_material"}:
            raise RuntimeError(
                f"{target['id']}: modular skinned assets require atlasPolicy "
                "dedicated_embedded or dedicated_material")
        if atlas_policy == "dedicated_material" and not generic.get("flatAtlasRgb"):
            raise RuntimeError(
                f"{target['id']}: dedicated_material requires flatAtlasRgb "
                "until a painted dedicated atlas is supplied")
    bake_kwargs = {
        "mesh_out": project / outputs["mesh"],
        "atlas_out": project / outputs["atlas"],
        "generator": generic.get("generator", f"AI RPG Engine {target['id']} generic skinned bake"),
        "target_height": generic.get("targetHeight"),
    }

    def apply_player_bind_space(source_gltf: Path) -> None:
        if not generic.get("matchPlayerBake"):
            return
        scale, offset = _match_player_bake_transform(project, source_gltf)
        bake_kwargs["uniform_scale"] = scale
        bake_kwargs["uniform_offset"] = offset
    def apply_flat_atlas(result: dict[str, Any]) -> dict[str, Any]:
        # Modular gear copied from a character may retain its UV layout while
        # opting into a single material color. This avoids sampling skin/face
        # pixels from the original full-body player atlas.
        rgb = generic.get("flatAtlasRgb")
        if not rgb:
            return result
        if not isinstance(rgb, list) or len(rgb) != 3:
            raise RuntimeError(f"{target['id']}: flatAtlasRgb must be [r,g,b]")
        color = bytes(max(0, min(255, int(channel))) for channel in rgb)
        width = height = 16
        raw = b"".join(b"\x00" + color * width for _ in range(height))
        def png_chunk(kind: bytes, payload: bytes) -> bytes:
            return (struct.pack(">I", len(payload)) + kind + payload +
                    struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))
        png = (b"\x89PNG\r\n\x1a\n" +
               png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
               png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b""))
        bake_kwargs["atlas_out"].write_bytes(png)
        result["flatAtlasRgb"] = list(color)
        result["atlasPolicy"] = atlas_policy
        return result

    if source.suffix.lower() != ".bbmodel":
        apply_player_bind_space(source)
        return apply_flat_atlas(bake_generic_gltf.bake_skinned(source=source, **bake_kwargs))

    # Blockbench's generic glTF codec does not serialize a free-format armature.  Convert the
    # authored .bbmodel in a temporary bake workspace, preserving it as the catalog source.
    data = json.loads(source.read_text(encoding="utf-8"))
    texture_source = next(
        (texture.get("source", "") for texture in data.get("textures") or []
         if str(texture.get("source", "")).startswith("data:image/")),
        "",
    )
    if not texture_source or "base64," not in texture_source:
        raise RuntimeError(f"{source}: no embedded texture available for skinned Blockbench bake")
    with tempfile.TemporaryDirectory(prefix=f"{target['id']}_bbmodel_") as temp_dir:
        temp = Path(temp_dir)
        atlas = temp / "atlas.png"
        atlas.write_bytes(base64.b64decode(texture_source.split("base64,", 1)[1]))
        intermediate = temp / f"{target['id']}.gltf"
        keep_meshes = generic.get("keepMeshes")
        if keep_meshes is not None and not isinstance(keep_meshes, list):
            raise RuntimeError(f"{target['id']}: keepMeshes must be a list of mesh names")
        bbmodel_exporter.export(
            source,
            atlas,
            [intermediate],
            keep_mesh_names=[str(name) for name in keep_meshes] if keep_meshes else None,
        )
        apply_player_bind_space(intermediate)
        result = apply_flat_atlas(
            bake_generic_gltf.bake_skinned(source=intermediate, **bake_kwargs))
    result["authoringSource"] = str(source)
    return result


def run_script(script_rel: str, extra_args: list[str] | None = None) -> None:
    script = REPO / script_rel
    cmd = [sys.executable, str(script)]
    if extra_args:
        cmd.extend(extra_args)
    proc = subprocess.run(
        cmd,
        cwd=str(REPO),
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"script bake failed {script_rel}:\n{proc.stdout}\n{proc.stderr}")


def emit(obj: dict, as_json: bool) -> None:
    if as_json:
        print(json.dumps(obj, indent=2))
    else:
        if obj.get("action") == "list":
            for t in obj.get("targets") or []:
                print(f"{t['id']:24} {t['kind']:8} {t.get('defaultSource', '')}")
            return
        print(obj.get("summary", ""))
        for g in obj.get("verify") or []:
            mark = "OK" if g.get("ok") else "FAIL"
            print(f"  [{mark}] {g.get('code')}: {g.get('detail')}")
            if g.get("remediation") and not g.get("ok"):
                print(f"         -> {g['remediation']}")


def cmd_list(as_json: bool) -> int:
    catalog = load_catalog()
    targets = []
    for t in catalog.get("targets") or []:
        targets.append(
            {
                "id": t["id"],
                "kind": t.get("kind"),
                "baker": t.get("baker"),
                "defaultSource": t.get("defaultSource"),
                "outputs": t.get("outputs"),
                "verify": t.get("verify"),
            }
        )
    emit({"ok": True, "action": "list", "targets": targets}, as_json)
    return 0


def slugify(text: str) -> str:
    out = []
    for ch in text.lower():
        out.append(ch if ch.isalnum() else "_")
    slug = "".join(out)
    while "__" in slug:
        slug = slug.replace("__", "_")
    return slug.strip("_")


def pascal_case(slug: str) -> str:
    return "".join(part.capitalize() for part in slug.split("_") if part) or "Imported"


def cmd_register(
    id_: str,
    kind: str,
    source: Path,
    target_height: float | None,
    fallback_clips: list[str],
    as_json: bool,
) -> int:
    """Cache a normalized copy of `source` under tools/art/<id>/ and append a catalog entry."""
    import gltf_normalize

    target_id = slugify(id_)
    if not target_id:
        emit(
            {
                "ok": False,
                "summary": "invalid target id",
                "verify": [{"code": "ASSET-IMPORT-REGISTER", "ok": False, "detail": f"id={id_!r}"}],
            },
            as_json,
        )
        return 1

    catalog = load_catalog()
    if find_target(catalog, target_id) is not None:
        emit(
            {
                "ok": False,
                "summary": f"target {target_id} already registered",
                "verify": [
                    {
                        "code": "ASSET-IMPORT-REGISTER",
                        "ok": False,
                        "detail": f"{target_id} exists in {CATALOG_PATH.name}",
                        "remediation": "Bake the existing target instead of registering a duplicate.",
                    }
                ],
            },
            as_json,
        )
        return 1

    if not source.exists():
        emit(
            {
                "ok": False,
                "summary": "source missing",
                "verify": [
                    {"code": "ASSET-BAKE-SOURCE-MISSING", "ok": False, "detail": str(source)}
                ],
            },
            as_json,
        )
        return 1

    art_dir = REPO / "tools/art" / target_id.replace("_", "-")
    art_gltf = art_dir / f"{pascal_case(target_id)}.gltf"
    try:
        normalized = gltf_normalize.normalize(source, art_gltf)
        cached = json.loads(art_gltf.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        emit(
            {
                "ok": False,
                "summary": f"could not normalize source: {exc}",
                "verify": [{"code": "ASSET-IMPORT-REGISTER", "ok": False, "detail": str(exc)}],
            },
            as_json,
        )
        return 1

    clips = normalized.get("clips") or fallback_clips
    skinned = kind == "skinned" or int(normalized.get("skins") or 0) > 0
    measured = gltf_normalize.measure_height(cached)
    height = float(target_height) if target_height and target_height > 0 else (measured or 1.0)
    atlas_size = normalized.get("atlasSize") or [512, 512]
    atlas_max = max(1024, max(int(atlas_size[0]), int(atlas_size[1])))
    # Baked node matrices block the scale/feet normalize pass, so do not gate on those numbers.
    normalizes_scale = not any("matrix" in node for node in cached.get("nodes") or [])

    verify: dict[str, Any] = {
        "atlasMin": 8,
        "atlasMax": atlas_max,
        "checkUvTransparent": False,
    }
    if normalizes_scale or not skinned:
        verify["targetHeight"] = round(height, 4)
        verify["heightTolerance"] = round(max(0.08, height * 0.12), 4)
        verify["feetEpsilon"] = 0.05
    else:
        verify["checkHeight"] = False
        verify["checkFeet"] = False
    if skinned:
        verify["requiredClips"] = clips

    entry: dict[str, Any] = {
        "id": target_id,
        "kind": "skinned" if skinned else "static",
        "baker": "generic_skinned" if skinned else "generic_static",
        "defaultSource": str(art_gltf.relative_to(REPO)).replace("\\", "/"),
        "outputs": {
            "mesh": f"assets/models/{target_id}.gltf",
            "atlas": f"assets/models/{target_id}.png",
        },
        "generatorContains": f"{target_id} generic",
        "generic": {
            "targetHeight": round(height, 4),
            "generator": f"AI RPG Engine {target_id} generic"
            + (" skinned bake" if skinned else " bake")
            + " v1",
            "sceneName": pascal_case(target_id),
            "meshName": pascal_case(target_id),
            "materialName": pascal_case(target_id) + "Atlas",
            "maxAtlas": atlas_max,
            # Imported art is not assumed to be a Blockbench UV sheet; do not punch pale texels.
            "cleanBackdrop": False,
        },
        "verify": verify,
        # Provenance only; keep it repo-relative so the catalog stays machine independent.
        "importedFrom": repo_relative(source),
    }
    if normalized.get("atlas"):
        entry["defaultAtlas"] = str(Path(normalized["atlas"]).relative_to(REPO)).replace("\\", "/")

    append_catalog_target(entry)

    emit(
        {
            "ok": True,
            "action": "register",
            "target": entry,
            "clips": clips,
            "measuredHeight": round(measured, 4),
            "summary": f"registered {target_id} ({entry['kind']})",
        },
        as_json,
    )
    return 0


def cmd_bake(project: Path, target_id: str, source: Path | None, as_json: bool) -> int:
    started = time.time()
    catalog = load_catalog()
    target = find_target(catalog, target_id)
    if target is None:
        emit(
            {
                "ok": False,
                "summary": f"unknown target {target_id}",
                "verify": [
                    {
                        "code": "ASSET-BAKE-SOURCE-MISSING",
                        "ok": False,
                        "detail": f"target id not in catalog: {target_id}",
                        "remediation": "Use --list for registered ids.",
                    }
                ],
            },
            as_json,
        )
        return 1

    default_src = REPO / target["defaultSource"]
    src_path = source.resolve() if source else default_src
    if not src_path.exists():
        emit(
            {
                "ok": False,
                "summary": "source missing",
                "verify": [
                    {
                        "code": "ASSET-BAKE-SOURCE-MISSING",
                        "ok": False,
                        "detail": f"tried {src_path}",
                        "remediation": "Export glTF into tools/art/… or pass --source.",
                    }
                ],
            },
            as_json,
        )
        return 1

    # Cache source into tools/art when an override was provided.
    # Player bbmodels are not copied onto the .gltf defaultSource — run_player_bake regenerates
    # GoodPlayerModel.gltf from the Blockbench project so animation lengths stay correct.
    if (
        source is not None
        and source.resolve() != default_src.resolve()
        and not (target.get("baker") == "player_v2" and src_path.suffix.lower() == ".bbmodel")
    ):
        try:
            if str(target.get("baker", "")).startswith("generic"):
                # Generic targets store a normalized single-buffer glTF; a raw copy could be a GLB
                # or reference an external .bin that the bakers cannot read.
                import gltf_normalize

                gltf_normalize.normalize(src_path, default_src)
                src_path = default_src
            else:
                copy_if_newer(src_path, default_src)
                if src_path.suffix.lower() == ".gltf":
                    png = src_path.with_suffix(".png")
                    if png.exists() and target.get("defaultAtlas"):
                        copy_if_newer(png, REPO / target["defaultAtlas"])
                bb = src_path.with_suffix(".bbmodel")
                if not bb.exists():
                    # Common Documents drop next to glTF name
                    alt = src_path.parent / (src_path.stem + ".bbmodel")
                    bb = alt if alt.exists() else bb
                if bb.exists() and target_id == "player":
                    copy_if_newer(bb, REPO / "tools/art/player/GoodPlayerModel_rigged.bbmodel")
        except Exception as exc:  # noqa: BLE001
            emit(
                {
                    "ok": False,
                    "summary": "failed to cache source into tools/art",
                    "verify": [
                        {
                            "code": "ASSET-BAKE-TOOLING",
                            "ok": False,
                            "detail": str(exc),
                        }
                    ],
                },
                as_json,
            )
            return 1

    if target.get("baker") == "player_v2" and src_path.suffix.lower() == ".bbmodel" and src_path.exists():
        try:
            copy_if_newer(src_path, REPO / "tools/art/player/GoodPlayerModel_rigged.bbmodel")
        except Exception as exc:  # noqa: BLE001
            emit(
                {
                    "ok": False,
                    "summary": "failed to cache player bbmodel into tools/art",
                    "verify": [{"code": "ASSET-BAKE-TOOLING", "ok": False, "detail": str(exc)}],
                },
                as_json,
            )
            return 1

    models_dir = project / "assets" / "models"
    before = snapshot_models(models_dir)
    allowed = set()
    for key in (target.get("outputs") or {}).values():
        # outputs are project-relative like assets/models/player.gltf
        if key.startswith("assets/models/"):
            allowed.add(key[len("assets/models/") :])
        elif "/" in key:
            allowed.add(Path(key).name)

    bake_meta: dict[str, Any] = {}
    try:
        baker = target.get("baker")
        if baker == "player_v2":
            # Always pass the resolved source (glTF or bbmodel). run_player_bake refreshes from
            # the Blockbench project when it is newer than the cached art glTF.
            bake_meta = run_player_bake(src_path, project)
        elif baker == "tier1":
            run_tier1(target_id)
        elif baker == "script":
            extra = target.get("scriptArgs")
            run_script(target["script"], extra if isinstance(extra, list) else None)
        elif baker == "generic_static":
            run_generic_static(target, project)
        elif baker == "generic_skinned":
            bake_meta = run_generic_skinned(target, project)
        else:
            raise RuntimeError(f"unknown baker {baker}")
    except ImportError as exc:
        emit(
            {
                "ok": False,
                "summary": "tooling missing",
                "verify": [
                    {
                        "code": "ASSET-BAKE-TOOLING",
                        "ok": False,
                        "detail": str(exc),
                        "remediation": "Install Python deps (Pillow) and ensure tools/ imports.",
                    }
                ],
            },
            as_json,
        )
        return 1
    except OSError as exc:
        detail = str(exc)
        code = "ASSET-BAKE-TOOLING"
        if getattr(exc, "errno", None) in (13, 22) or "being used by another" in detail.lower():
            code = "ASSET-BAKE-TOOLING"
            detail = f"output locked or unwritable: {detail}"
        emit(
            {
                "ok": False,
                "summary": "bake write failed",
                "verify": [
                    {
                        "code": code,
                        "ok": False,
                        "detail": detail,
                        "remediation": "Close engine.exe if it locks the PNG/glTF, then retry.",
                    }
                ],
            },
            as_json,
        )
        return 1
    except Exception as exc:  # noqa: BLE001
        emit(
            {
                "ok": False,
                "summary": f"bake failed: {exc}",
                "verify": [{"code": "ASSET-BAKE-TOOLING", "ok": False, "detail": str(exc)}],
            },
            as_json,
        )
        return 1

    after = snapshot_models(models_dir)
    collateral = collateral_changes(before, after, allowed)

    out_mesh = project / target["outputs"]["mesh"]
    out_atlas = project / target["outputs"]["atlas"] if target["outputs"].get("atlas") else None
    source_atlas = None
    if target.get("defaultAtlas"):
        source_atlas = REPO / target["defaultAtlas"]
    elif src_path.suffix.lower() == ".gltf":
        cand = src_path.with_suffix(".png")
        if cand.exists():
            source_atlas = cand

    # For player, prefer the glTF actually baked from (bake_meta).
    verify_source = Path(bake_meta["source"]) if bake_meta.get("source") else src_path

    gates = verify.verify_bake(
        project_root=project,
        target=target,
        source_gltf=verify_source if verify_source.suffix.lower() in (".gltf", ".glb") else None,
        baked_mesh=out_mesh,
        baked_atlas=out_atlas,
        source_atlas=source_atlas,
    )

    if collateral:
        gates.append(
            {
                "code": "ASSET-BAKE-COLLATERAL",
                "ok": False,
                "detail": f"unexpected model changes: {collateral}",
                "remediation": "Named bake only; revert collateral files under assets/models/.",
            }
        )
    else:
        gates.append(
            {
                "code": "ASSET-BAKE-COLLATERAL",
                "ok": True,
                "detail": "no unexpected assets/models writes",
            }
        )

    ok = verify.all_ok(gates)
    report = {
        "ok": ok,
        "action": "bake",
        "target": target_id,
        "kind": target.get("kind"),
        "source": str(verify_source),
        "outputs": {k: str(project / v) for k, v in (target.get("outputs") or {}).items()},
        "elapsedSec": round(time.time() - started, 3),
        "bake": bake_meta,
        "verify": gates,
        "summary": f"bake {target_id} {'ok' if ok else 'FAILED'}",
        "meshReloads": [target["outputs"]["mesh"]] if ok else [],
    }

    report_rel = (target.get("outputs") or {}).get("report")
    if report_rel:
        report_path = project / report_rel
        try:
            report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
            report["reportPath"] = str(report_path)
        except OSError:
            pass

    emit(report, as_json)
    return 0 if ok else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Universal named asset bake")
    parser.add_argument("--project", type=Path, default=None)
    parser.add_argument("--target", type=str, default=None)
    parser.add_argument("--source", type=Path, default=None)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--register", action="store_true", help="append a generic import target")
    parser.add_argument("--id", type=str, default=None, help="target id for --register")
    parser.add_argument("--kind", type=str, default="static", choices=["static", "skinned"])
    parser.add_argument("--target-height", type=float, default=None)
    parser.add_argument("--clip", action="append", default=[], help="clip name hint for --register")
    args = parser.parse_args(argv)

    try:
        if args.list:
            return cmd_list(args.json)
        if args.register:
            if not args.id or args.source is None:
                parser.error("--register needs --id and --source")
            return cmd_register(
                args.id, args.kind, args.source.resolve(), args.target_height, args.clip, args.json
            )
        if not args.target:
            parser.error("--target is required (named bake only); use --list")
        project = resolve_project(args.project)
        return cmd_bake(project, args.target, args.source, args.json)
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        emit(
            {
                "ok": False,
                "summary": str(exc),
                "verify": [{"code": "ASSET-BAKE-TOOLING", "ok": False, "detail": str(exc)}],
            },
            args.json if "args" in dir() else True,
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
