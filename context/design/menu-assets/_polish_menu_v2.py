"""Chroma-key polish for menu chrome v2 assets + title backdrop composite for Pencil.

Green-screen (#00FF00) keyed to true alpha. Writes:
  context/design/menu-assets/*.png (transparent)
  context/design/menu-assets/pencil-matte/*.png (iron composite for in-panel fills)
  wrathful title composited onto menu backdrop patch (no box)
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
ORIGINALS = Path(r"C:\Users\johnr\.cursor\projects\c-Users-johnr-Documents-3DGameIdea\assets")
MATTE_DIR = ROOT / "pencil-matte"
CONCEPTS = Path(__file__).resolve().parents[2] / "art" / "concepts"
IRON = (45, 41, 35, 255)
SCRIM = (26, 24, 20, 255)

# v2 sources in Cursor assets → canonical names in menu-assets
MAP = {
    "menu-btn-primary-v2.png": "menu-btn-primary.png",
    "menu-btn-secondary-v2.png": "menu-btn-secondary.png",
    "menu-panel-frame-v2.png": "menu-panel-frame.png",
    "menu-title-plate-v2.png": "menu-title-plate.png",
    "menu-archetype-card-v2.png": "menu-archetype-card.png",
    "icon-ashfell-blade-v2.png": "icon-ashfell-blade.png",
    "icon-outrider-v2.png": "icon-outrider.png",
    "icon-runecaster-v2.png": "icon-runecaster.png",
}


def chroma_key_green(arr: np.ndarray, soft: float = 28.0) -> np.ndarray:
    """Key near-green pixels to alpha. Soft edge by distance from pure green."""
    rgb = arr[..., :3].astype(np.float32)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    # green dominance
    greenness = g - np.maximum(r, b)
    # also catch near-#00FF00
    dist = np.sqrt((r - 0.0) ** 2 + (g - 255.0) ** 2 + (b - 0.0) ** 2)
    key = (greenness > 40) & (g > 140) & (dist < 220)
    soft_key = (greenness > 18) & (g > 100)

    alpha = arr[..., 3].astype(np.float32)
    alpha[key] = 0
    # soften fringe
    fringe = soft_key & ~key
    alpha[fringe] = np.clip(255.0 - (greenness[fringe] - 18.0) / soft * 255.0, 0, 255)
    out = arr.copy()
    out[..., 3] = alpha.astype(np.uint8)
    transparent = out[..., 3] == 0
    out[transparent, 0:3] = 0
    return out


def trim(arr: np.ndarray, pad: int = 8) -> np.ndarray:
    a = arr[..., 3]
    ys, xs = np.where(a > 8)
    if len(xs) == 0:
        return arr
    y0 = max(0, int(ys.min()) - pad)
    y1 = min(arr.shape[0], int(ys.max()) + 1 + pad)
    x0 = max(0, int(xs.min()) - pad)
    x1 = min(arr.shape[1], int(xs.max()) + 1 + pad)
    return arr[y0:y1, x0:x1]


def polish_mapped() -> None:
    MATTE_DIR.mkdir(parents=True, exist_ok=True)
    for src_name, dest_name in MAP.items():
        src = ORIGINALS / src_name
        if not src.exists():
            print(f"missing {src_name}")
            continue
        im = Image.open(src).convert("RGBA")
        arr = chroma_key_green(np.array(im))
        arr = trim(arr)
        result = Image.fromarray(arr)
        result.save(ROOT / dest_name, optimize=True)

        matte_color = SCRIM if dest_name.startswith("icon-") else IRON
        base = Image.new("RGBA", result.size, matte_color)
        Image.alpha_composite(base, result).convert("RGB").save(
            MATTE_DIR / dest_name, optimize=True
        )
        print(f"{dest_name}: a0={float((arr[..., 3] == 0).mean()):.1%} size={result.size}")


def composite_title_for_pencil() -> None:
    """Bake transparent title onto a crop of the main-menu backdrop so Pencil has no box."""
    bg = Image.open(CONCEPTS / "act0-main-menu.png").convert("RGBA")
    logo = Image.open(CONCEPTS / "wrathful-conquest-title-logo.png").convert("RGBA")
    # Match pen placement: logo at (30,36) size 540x290 on 1920x1080 screen
    screen_w, screen_h = 1920, 1080
    lx, ly, lw, lh = 30, 36, 540, 290
    bg_r = bg.resize((screen_w, screen_h), Image.Resampling.LANCZOS)
    logo_r = logo.resize((lw, lh), Image.Resampling.LANCZOS)
    layer = Image.new("RGBA", (screen_w, screen_h), (0, 0, 0, 0))
    layer.paste(logo_r, (lx, ly), logo_r)
    composed = Image.alpha_composite(bg_r, layer)
    # Export only the logo rect (seamless with backdrop behind it)
    patch = composed.crop((lx, ly, lx + lw, ly + lh)).convert("RGB")
    out = ROOT / "wrathful-conquest-title-on-menu.png"
    patch.save(out, optimize=True)
    # Also keep matte-free transparent logo copy in menu-assets
    logo.save(ROOT / "wrathful-conquest-title-logo.png", optimize=True)
    print(f"wrote title patch {out} and transparent logo")


def main() -> None:
    polish_mapped()
    composite_title_for_pencil()


if __name__ == "__main__":
    main()
