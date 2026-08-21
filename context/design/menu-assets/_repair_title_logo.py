"""Repair the Wrathful Conquest title logo alpha.

The earlier `_punch_title_logo.py` pass keyed the black backdrop by luminance, which
also chewed the dark iron inside the letters: interior texture became semi-transparent
and ~1.7k pinholes opened up, so the sky bled through the mark in-engine.

This pass rebuilds alpha from the punched PNG:
  * outer background (border-connected transparency) stays transparent
  * enclosed counters above `COUNTER_MIN_PX` stay transparent
  * every other interior pixel becomes opaque, with a 2px feather at the silhouette edge
  * RGB for re-opened pixels is pulled from the nearest surviving textured pixel
  * stray keying crumbs below the wordmark are dropped

Run: python context/design/menu-assets/_repair_title_logo.py
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
SRC = HERE / "wrathful-conquest-title-logo.png"
OUTS = [
    SRC,
    REPO / "samples" / "open-world-rpg" / "assets" / "ui" / "menu" / "wrathful-conquest-title-logo.png",
]

VISIBLE_MIN_ALPHA = 8
COUNTER_MIN_PX = 400
CRUMB_MAX_PX = 3000  # smallest real glyph/flourish part is ~4k px
FEATHER_PX = 2.0


def outer_background(alpha: np.ndarray) -> np.ndarray:
    transparent = alpha <= VISIBLE_MIN_ALPHA
    labels, count = ndimage.label(transparent)
    if count == 0:
        return np.zeros_like(transparent)
    border = np.unique(
        np.concatenate([labels[0], labels[-1], labels[:, 0], labels[:, -1]])
    )
    border = border[border != 0]
    return np.isin(labels, border)


def keep_counters(interior_holes: np.ndarray) -> np.ndarray:
    """Keep only enclosed transparent regions big enough to be real letter counters."""
    labels, count = ndimage.label(interior_holes)
    if count == 0:
        return interior_holes
    sizes = ndimage.sum(interior_holes, labels, np.arange(1, count + 1))
    keep_ids = np.arange(1, count + 1)[sizes >= COUNTER_MIN_PX]
    return np.isin(labels, keep_ids)


def drop_crumbs(solid: np.ndarray) -> np.ndarray:
    labels, count = ndimage.label(solid)
    if count == 0:
        return solid
    sizes = ndimage.sum(solid, labels, np.arange(1, count + 1))
    keep_ids = np.arange(1, count + 1)[sizes > CRUMB_MAX_PX]
    return np.isin(labels, keep_ids)


def repair(path: Path) -> Image.Image:
    arr = np.array(Image.open(path).convert("RGBA"))
    rgb = arr[..., :3].astype(np.float32)
    alpha = arr[..., 3]

    textured = alpha > VISIBLE_MIN_ALPHA
    outer = outer_background(alpha)
    counters = keep_counters((alpha <= VISIBLE_MIN_ALPHA) & ~outer)

    solid = drop_crumbs(~outer & ~counters)
    transparent = ~solid

    # Feather only where the silhouette meets transparency, not across interior texture.
    edge_distance = ndimage.distance_transform_edt(solid)
    out_alpha = np.zeros(alpha.shape, dtype=np.float32)
    out_alpha[solid] = np.clip(edge_distance[solid] / FEATHER_PX, 0.0, 1.0) * 255.0
    # Anti-aliased rim from the original key is a better edge than a hard ramp.
    original_rim = solid & (edge_distance <= FEATHER_PX) & textured
    out_alpha[original_rim] = np.maximum(
        out_alpha[original_rim], alpha[original_rim].astype(np.float32)
    )

    # Pixels that were fully keyed had RGB zeroed; pull colour from nearest texture.
    needs_colour = solid & ~textured
    if needs_colour.any():
        _, indices = ndimage.distance_transform_edt(
            ~textured, return_indices=True
        )
        for channel in range(3):
            source = rgb[..., channel]
            rgb[..., channel] = np.where(
                needs_colour, source[indices[0], indices[1]], source
            )

    out = np.empty_like(arr)
    out[..., :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    out[..., 3] = np.clip(out_alpha, 0, 255).astype(np.uint8)
    out[transparent, 0:3] = 0

    ys, xs = np.where(out[..., 3] > VISIBLE_MIN_ALPHA)
    pad = 8
    y0 = max(0, int(ys.min()) - pad)
    y1 = min(out.shape[0], int(ys.max()) + pad + 1)
    x0 = max(0, int(xs.min()) - pad)
    x1 = min(out.shape[1], int(xs.max()) + pad + 1)
    return Image.fromarray(out[y0:y1, x0:x1], mode="RGBA")


def main() -> None:
    repaired = repair(SRC)
    arr = np.array(repaired)
    opaque = float((arr[..., 3] > 200).mean())
    partial = int(((arr[..., 3] > 8) & (arr[..., 3] <= 200)).sum())
    print(f"size={repaired.size} opaque={opaque*100:.1f}% partial_px={partial}")
    for dest in OUTS:
        if not dest.parent.exists():
            print(f"skip (missing dir) {dest}")
            continue
        repaired.save(dest)
        print(f"wrote {dest}")


if __name__ == "__main__":
    main()
