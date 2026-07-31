#!/usr/bin/env python3
"""Universal named asset bake orchestrator (player + registered props).

Usage:
  python tools/asset_bake.py --list [--json]
  python tools/asset_bake.py --project samples/open-world-rpg --target player [--source path] [--json]
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[1]
CATALOG_PATH = Path(__file__).resolve().parent / "asset_bake_catalog.json"

# Ensure tools/ is importable when spawned from engine cwd.
if str(REPO / "tools") not in sys.path:
    sys.path.insert(0, str(REPO / "tools"))

import asset_bake_verify as verify  # noqa: E402


def load_catalog() -> dict:
    return json.loads(CATALOG_PATH.read_text(encoding="utf-8"))


def find_target(catalog: dict, target_id: str) -> dict | None:
    for t in catalog.get("targets") or []:
        if t.get("id") == target_id:
            return t
    return None


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


def run_player_bake(source: Path | None, project: Path) -> dict[str, Any]:
    import bake_player_v2_gltf as player_bake

    return player_bake.bake(source_override=source, project_root=project)


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


def run_script(script_rel: str) -> None:
    script = REPO / script_rel
    proc = subprocess.run(
        [sys.executable, str(script)],
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
    if source is not None and source.resolve() != default_src.resolve():
        try:
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
        except OSError as exc:
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
            bake_meta = run_player_bake(src_path if source else None, project)
        elif baker == "tier1":
            run_tier1(target_id)
        elif baker == "script":
            run_script(target["script"])
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
    args = parser.parse_args(argv)

    try:
        if args.list:
            return cmd_list(args.json)
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
