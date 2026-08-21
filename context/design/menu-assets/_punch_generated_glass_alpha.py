"""Punch baked mattes / checkerboards from generated glass UI overlays → true RGBA.

Safer than border flood-fill: stained-glass leading is near-black and would otherwise
tunnel into the art. Uses chroma/luma subject masks + morphological close/fill.

Run: python context/design/menu-assets/_punch_generated_glass_alpha.py
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

REPO = Path(__file__).resolve().parents[3]
MENU = REPO / "samples" / "open-world-rpg" / "assets" / "ui" / "menu"
OUT_DEBUG = REPO / "out" / "glass-punch-preview"
OUT_COMP = REPO / "out" / "glass-punch-composite"


def luma(rgb: np.ndarray) -> np.ndarray:
    return 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]


def chroma(rgb: np.ndarray) -> np.ndarray:
    mx = np.maximum(np.maximum(rgb[..., 0], rgb[..., 1]), rgb[..., 2])
    mn = np.minimum(np.minimum(rgb[..., 0], rgb[..., 1]), rgb[..., 2])
    return mx - mn


def soft_edge(alpha: np.ndarray, px: float = 2.0) -> np.ndarray:
    dist = ndimage.distance_transform_edt(alpha > 0)
    fringe = (dist > 0) & (dist < px)
    out = alpha.astype(np.float32)
    out[fringe] = np.minimum(out[fringe], (dist[fringe] / px) * 255.0)
    return out


def compose_preview(im: Image.Image, path: Path) -> None:
    OUT_COMP.mkdir(parents=True, exist_ok=True)
    a = np.array(im.convert("RGBA"))
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    cell = ((xx // 32) + (yy // 32)) % 2
    bg = np.zeros((h, w, 4), dtype=np.uint8)
    bg[cell == 0] = (255, 0, 255, 255)
    bg[cell == 1] = (40, 200, 255, 255)
    Image.alpha_composite(Image.fromarray(bg), Image.fromarray(a)).save(OUT_COMP / path.name)


def save(im: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    im.save(path)
    OUT_DEBUG.mkdir(parents=True, exist_ok=True)
    im.save(OUT_DEBUG / path.name)
    compose_preview(im, path)
    a = np.array(im)
    corners = [int(a[0, 0, 3]), int(a[0, -1, 3]), int(a[-1, 0, 3]), int(a[-1, -1, 3])]
    print(
        f"{path.name}: size={im.size} meanA={a[..., 3].mean():.1f} "
        f"cornerA={corners} transparent={(a[..., 3] < 8).mean() * 100:.1f}% midA={a[a.shape[0]//2, a.shape[1]//2, 3]}"
    )


def punch_arch_card(path: Path) -> Image.Image:
    """Keep the stained-glass arch; punch only outside the closed silhouette."""
    arr = np.array(Image.open(path).convert("RGBA"))
    rgb = arr[..., :3].astype(np.float32)
    L = luma(rgb)
    C = chroma(rgb)
    # Subject = anything with color or non-flat black (includes dark leading inside art).
    subject = (C > 16) | (L > 38)
    # Seal gaps in leading so fill_holes recovers the full arch.
    subject = ndimage.binary_closing(subject, iterations=6)
    subject = ndimage.binary_dilation(subject, iterations=1)
    subject = ndimage.binary_fill_holes(subject)
    # Drop tiny islands outside the main arch.
    labeled, n = ndimage.label(subject)
    if n:
        sizes = ndimage.sum(subject, labeled, index=range(1, n + 1))
        keep_id = int(np.argmax(sizes)) + 1
        subject = labeled == keep_id

    alpha = np.where(subject, 255.0, 0.0)
    alpha = soft_edge(alpha, px=2.0)
    out = arr.copy()
    out[..., 3] = np.clip(alpha, 0, 255).astype(np.uint8)
    out[out[..., 3] == 0, :3] = 0
    return Image.fromarray(out)


def is_checker_or_matte(rgb: np.ndarray) -> np.ndarray:
    L = luma(rgb)
    C = chroma(rgb)
    # Neutral studio / checker cells (not warm metal, not saturated glass).
    return (C < 28) & (((L > 30) & (L < 200)) | (L < 25) | (L > 210))


def punch_frame_plate(path: Path, glass_a: int = 170) -> Image.Image:
    """Ornate frame with glass well: transparent outside, soft dark glass inside."""
    arr = np.array(Image.open(path).convert("RGBA"))
    rgb = arr[..., :3].astype(np.float32)
    L = luma(rgb)
    C = chroma(rgb)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    metal = (
        ((r > b + 10) & (C > 18) & (r > 50))
        | ((r > 110) & (g > 45) & (C > 28))
        | ((C > 35) & (L > 40))
        | ((C > 20) & (L > 85) & (L < 210))
    )
    # Grow frame into a closed ring, fill interior.
    ring = ndimage.binary_closing(ndimage.binary_dilation(metal, iterations=2), iterations=5)
    filled = ndimage.binary_fill_holes(ring)
    interior = filled & ~ndimage.binary_erosion(ring, iterations=2)
    # If erosion ate the ring, fall back to filled XOR outer.
    if interior.mean() < 0.02:
        interior = filled & ~ring

    exterior = ~filled
    # Kill leftover matte speckles outside.
    exterior |= is_checker_or_matte(rgb) & ~filled

    alpha = np.full(L.shape, 255.0, dtype=np.float32)
    alpha[exterior] = 0.0

    # Replace checker / flat matte inside the well with smoked glass.
    matte_in = is_checker_or_matte(rgb) & interior
    glass_rgb = np.array([26.0, 20.0, 14.0], dtype=np.float32)
    # Prefer existing dark non-matte tint when present.
    keep_tint = interior & ~is_checker_or_matte(rgb) & (L < 90)
    alpha[interior] = float(glass_a)
    rgb[matte_in] = glass_rgb
    # Keep metal/glow fully opaque on the rim.
    alpha[ring & metal] = 255.0
    alpha[keep_tint] = np.maximum(alpha[keep_tint], 200.0)

    alpha = soft_edge(alpha, px=2.2)
    out = np.zeros_like(arr)
    out[..., :3] = np.clip(rgb, 0, 255)
    out[..., 3] = np.clip(alpha, 0, 255).astype(np.uint8)
    out[out[..., 3] == 0, :3] = 0
    return Image.fromarray(out)


def punch_glow(path: Path) -> Image.Image:
    arr = np.array(Image.open(path).convert("RGBA"))
    rgb = arr[..., :3].astype(np.float32)
    L = luma(rgb)
    C = chroma(rgb)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    glow = ((r > 65) & (g > 20) & (r > b + 5) & (C > 18)) | ((r > 130) & (g > 70) & (C > 25))
    glow |= (L > 140) & (C > 35) & (r > 100)
    glow = ndimage.binary_dilation(glow, iterations=1) | glow
    strength = np.clip((np.maximum(C - 12, 0) / 90.0) * (np.maximum(r - 35, 0) / 200.0), 0, 1)
    strength = np.maximum(strength, np.where(glow, 0.35, 0.0))
    alpha = np.where(glow, np.clip(strength * 255.0, 0, 255), 0.0)
    # Explicitly kill neutral mattes even if slightly warm-adjacent.
    alpha[is_checker_or_matte(rgb) & ~glow] = 0.0
    out = np.zeros_like(arr)
    out[..., :3] = np.clip(rgb, 0, 255)
    out[..., 3] = alpha.astype(np.uint8)
    out[out[..., 3] < 6, :3] = 0
    out[out[..., 3] < 6, 3] = 0
    return Image.fromarray(out)


def punch_hollow_frame(path: Path) -> Image.Image:
    """Appearance panel: keep ornate rim only; hollow center stays transparent."""
    arr = np.array(Image.open(path).convert("RGBA"))
    rgb = arr[..., :3].astype(np.float32)
    L = luma(rgb)
    C = chroma(rgb)
    rim = (C > 14) & (L > 28) & (L < 220)
    rim |= (C > 22) & (L > 20)
    rim = ndimage.binary_closing(rim, iterations=3)
    # Remove large interior fills: keep only a shell near the outer silhouette.
    filled = ndimage.binary_fill_holes(rim)
    # Distance from exterior — keep pixels near the rim band.
    exterior = ~filled
    dist_in = ndimage.distance_transform_edt(~exterior)
    # Approximate frame thickness band.
    band = filled & (dist_in <= max(18, int(0.04 * min(arr.shape[0], arr.shape[1]))))
    # Also keep high-chroma metal anywhere in filled area (tracery).
    metal = (C > 20) & (L > 35) & filled
    keep = band | metal | rim
    # Drop border-connected matte.
    keep &= ~is_checker_or_matte(rgb) | metal

    alpha = np.where(keep, 255.0, 0.0)
    alpha = soft_edge(alpha, px=2.0)
    out = arr.copy()
    out[..., 3] = np.clip(alpha, 0, 255).astype(np.uint8)
    out[out[..., 3] == 0, :3] = 0
    return Image.fromarray(out)


def main() -> None:
    dialogue = MENU / "prologue" / "prologue-glass-dialogue-plate.png"
    im = punch_frame_plate(dialogue, glass_a=165)
    save(im, dialogue)
    dup = MENU / "creation" / "prologue-glass-dialogue-plate.png"
    if dup.exists():
        save(im, dup)

    caption = MENU / "creation" / "creation-glass-card-caption.png"
    save(punch_frame_plate(caption, glass_a=180), caption)

    appearance = MENU / "creation" / "creation-glass-appearance-panel.png"
    save(punch_hollow_frame(appearance), appearance)

    glow = MENU / "creation" / "creation-glass-card-hover-glow.png"
    save(punch_glow(glow), glow)

    for name in [
        "glass-card-ashfell-blade.png",
        "glass-card-outrider.png",
        "glass-card-runecaster.png",
        "glass-card-ashens-levy.png",
        "glass-card-calrenoth-breach.png",
        "glass-card-frangiturs-claim.png",
    ]:
        path = MENU / "creation" / name
        save(punch_arch_card(path), path)


if __name__ == "__main__":
    main()
