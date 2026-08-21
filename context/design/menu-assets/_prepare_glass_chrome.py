"""Prepare stained-glass menu chrome for runtime use.

The previous green-screen chrome had chroma-key green baked into its RGB (up to 5.8% of the
visible pixels on the secondary button), so no alpha despill could clean it. These plates are
generated full-bleed and opaque instead: nothing is keyed, so nothing can fringe.

Per asset this pass trims generated letterbox bands, forces alpha to 255, resamples to a size
close to its on-screen use, and reports any residual green so a bad generation is caught here
rather than in-engine.

Run: python context/design/menu-assets/_prepare_glass_chrome.py
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
RAW = HERE / "glass" / "raw"
DESIGN_OUT = HERE / "glass"
RUNTIME_OUT = REPO / "samples" / "open-world-rpg" / "assets" / "ui" / "menu"

TRIM_LUMA = 20.0
MIN_TRIM_PX = 24


@dataclass(frozen=True)
class Chrome:
    name: str
    target_width: int  # height follows the trimmed aspect


ASSETS = [
    Chrome("glass-menu-panel-frame", 700),
    Chrome("glass-menu-btn-primary", 768),
    Chrome("glass-menu-btn-secondary", 768),
    Chrome("glass-menu-title-plate", 760),
]


def trim_letterbox(arr: np.ndarray) -> np.ndarray:
    """Drop flat near-black bands the generator adds around a non-matching aspect."""
    luma = 0.2126 * arr[..., 0] + 0.7152 * arr[..., 1] + 0.0722 * arr[..., 2]
    rows = np.percentile(luma, 97, axis=1)
    cols = np.percentile(luma, 97, axis=0)

    def bounds(profile: np.ndarray) -> tuple[int, int]:
        lit = np.where(profile >= TRIM_LUMA)[0]
        if not len(lit):
            return 0, len(profile)
        lo = int(lit[0]) if int(lit[0]) >= MIN_TRIM_PX else 0
        hi_gap = len(profile) - 1 - int(lit[-1])
        hi = int(lit[-1]) + 1 if hi_gap >= MIN_TRIM_PX else len(profile)
        return lo, hi

    y0, y1 = bounds(rows)
    x0, x1 = bounds(cols)
    return arr[y0:y1, x0:x1]


def green_report(arr: np.ndarray) -> str:
    r = arr[..., 0].astype(int)
    g = arr[..., 1].astype(int)
    b = arr[..., 2].astype(int)
    greenish = (g > r + 25) & (g > b + 25)
    return f"green={greenish.sum()} ({greenish.mean() * 100:.2f}%)"


def prepare(chrome: Chrome) -> None:
    source = RAW / f"{chrome.name}.png"
    arr = np.array(Image.open(source).convert("RGBA"))
    trimmed = trim_letterbox(arr)
    trimmed = trimmed.copy()
    trimmed[..., 3] = 255  # full-bleed plate: opaque everywhere, nothing keyed

    height = max(1, round(trimmed.shape[0] * chrome.target_width / trimmed.shape[1]))
    image = Image.fromarray(trimmed).resize((chrome.target_width, height), Image.LANCZOS)

    print(f"{chrome.name}: raw={arr.shape[1]}x{arr.shape[0]} "
          f"trimmed={trimmed.shape[1]}x{trimmed.shape[0]} out={image.size[0]}x{image.size[1]} "
          f"{green_report(np.array(image))}")
    for directory in (DESIGN_OUT, RUNTIME_OUT):
        directory.mkdir(parents=True, exist_ok=True)
        image.save(directory / f"{chrome.name}.png")


def main() -> None:
    for chrome in ASSETS:
        prepare(chrome)


if __name__ == "__main__":
    main()
