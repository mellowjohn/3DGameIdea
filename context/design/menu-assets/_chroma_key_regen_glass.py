"""Chroma-key regenerated glass UI overlays (#00FF00) → true RGBA.

Prefer this path over morphological matte punches on stained glass.
Run: python context/design/menu-assets/_chroma_key_regen_glass.py
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[3]
ASSETS = Path(r"C:\Users\johnr\.cursor\projects\c-Users-johnr-Documents-3DGameIdea\assets")
MENU = REPO / "samples" / "open-world-rpg" / "assets" / "ui" / "menu"
OUT_DEBUG = REPO / "out" / "glass-chroma-preview"


@dataclass(frozen=True)
class Job:
    source_name: str
    dest_rel: str


JOBS = [
    Job("regen-prologue-dialogue-plate.png", "prologue/prologue-glass-dialogue-plate.png"),
    Job("regen-prologue-dialogue-plate.png", "creation/prologue-glass-dialogue-plate.png"),
    Job("regen-creation-caption.png", "creation/creation-glass-card-caption.png"),
    Job("regen-creation-hover-glow.png", "creation/creation-glass-card-hover-glow.png"),
    Job("regen-creation-appearance-panel.png", "creation/creation-glass-appearance-panel.png"),
    Job("regen-glass-card-ashfell-blade.png", "creation/glass-card-ashfell-blade.png"),
    Job("regen-glass-card-outrider.png", "creation/glass-card-outrider.png"),
    Job("regen-glass-card-runecaster.png", "creation/glass-card-runecaster.png"),
    Job("regen-glass-card-ashens-levy.png", "creation/glass-card-ashens-levy.png"),
    Job("regen-glass-card-calrenoth-breach.png", "creation/glass-card-calrenoth-breach.png"),
    Job("regen-glass-card-frangiturs-claim.png", "creation/glass-card-frangiturs-claim.png"),
]


def green_key_mask(rgb: np.ndarray) -> np.ndarray:
    r = rgb[..., 0].astype(np.float32)
    g = rgb[..., 1].astype(np.float32)
    b = rgb[..., 2].astype(np.float32)
    return (g > 90) & (g > r + 30) & (g > b + 30)


def soft_alpha_from_green(rgb: np.ndarray) -> np.ndarray:
    r = rgb[..., 0].astype(np.float32)
    g = rgb[..., 1].astype(np.float32)
    b = rgb[..., 2].astype(np.float32)
    greenness = np.maximum(0.0, g - np.maximum(r, b))
    alpha = np.full(g.shape, 255.0, dtype=np.float32)
    hard = green_key_mask(rgb)
    alpha[hard] = 0.0
    fringe = (~hard) & (g > r + 12) & (g > b + 12) & (greenness > 18)
    alpha[fringe] = np.clip(255.0 - (greenness[fringe] - 18.0) / 50.0 * 255.0, 0, 255)
    return alpha


def despill(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    out = rgb.astype(np.float32).copy()
    r, g, b = out[..., 0], out[..., 1], out[..., 2]
    edge = (alpha > 8) & (alpha < 250) & (g > r) & (g > b)
    spill = np.maximum(0.0, g - np.maximum(r, b))
    g[edge] = np.maximum(np.maximum(r[edge], b[edge]), g[edge] - spill[edge] * 0.85)
    out[..., 1] = g
    return np.clip(out, 0, 255)


def chroma_key(path: Path) -> Image.Image:
    arr = np.array(Image.open(path).convert("RGBA"))
    rgb = arr[..., :3]
    alpha = soft_alpha_from_green(rgb)
    rgb = despill(rgb, alpha)
    out = np.zeros_like(arr)
    out[..., :3] = rgb
    out[..., 3] = np.clip(alpha, 0, 255).astype(np.uint8)
    kill = out[..., 3] < 6
    out[kill, :3] = 0
    out[kill, 3] = 0
    return Image.fromarray(out)


def report(im: Image.Image, name: str) -> None:
    a = np.array(im)
    print(
        f"{name}: size={im.size} meanA={a[..., 3].mean():.1f} "
        f"cornerA={[int(a[0, 0, 3]), int(a[0, -1, 3]), int(a[-1, 0, 3]), int(a[-1, -1, 3])]} "
        f"transparent%={(a[..., 3] < 8).mean() * 100:.1f} midA={int(a[a.shape[0] // 2, a.shape[1] // 2, 3])}"
    )


def main() -> None:
    OUT_DEBUG.mkdir(parents=True, exist_ok=True)
    for job in JOBS:
        src = ASSETS / job.source_name
        if not src.exists():
            print(f"MISSING {job.source_name}")
            continue
        im = chroma_key(src)
        dest = MENU / job.dest_rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        im.save(dest)
        im.save(OUT_DEBUG / Path(job.dest_rel).name)
        report(im, job.dest_rel)


if __name__ == "__main__":
    main()
