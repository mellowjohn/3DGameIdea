"""Generate low-poly iron/gold quest UI icons with true PNG alpha.

Draws at 4× then LANCZOS-downsamples for clean edges while keeping hard,
blocky chrome language (studs, wells, kind tints).
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
RUNTIME = (
    Path(__file__).resolve().parents[3]
    / "samples"
    / "open-world-rpg"
    / "assets"
    / "ui"
    / "quest"
)

SS = 4  # supersample factor

GOLD = (213, 185, 120, 255)
GOLD_DEEP = (180, 140, 70, 255)
IRON = (45, 41, 35, 255)
IRON_DEEP = (30, 28, 24, 255)
CHROME = (241, 238, 232, 255)
MAIN = (196, 162, 74, 255)
SIDE = (106, 138, 154, 255)
FACTION = (139, 90, 60, 255)
ARCHETYPE = (122, 107, 176, 255)
SUCCESS = (63, 107, 74, 255)


def save(img: Image.Image, name: str) -> None:
    for d in (ROOT, RUNTIME):
        d.mkdir(parents=True, exist_ok=True)
        img.save(d / name, "PNG")


def finish(hi: Image.Image, out_size: int) -> Image.Image:
    return hi.resize((out_size, out_size), Image.Resampling.LANCZOS)


def canvas(out_size: int) -> tuple[Image.Image, ImageDraw.ImageDraw, int]:
    s = out_size * SS
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img), s


def draw_stud(d: ImageDraw.ImageDraw, cx: int, cy: int, half: int, fill) -> None:
    d.rectangle([cx - half, cy - half, cx + half, cy + half], fill=fill)


def draw_glyph_mark(glyph: str, accent, name: str, out_size: int = 128) -> None:
    img, d, s = canvas(out_size)
    m = s // 8  # outer margin
    # Outer iron well
    d.rounded_rectangle(
        [m, m, s - m, s - m],
        radius=s // 12,
        fill=IRON_DEEP,
        outline=accent,
        width=max(SS * 3, 8),
    )
    # Inner face
    inset = m + s // 10
    d.rounded_rectangle(
        [inset, inset, s - inset, s - inset],
        radius=s // 20,
        fill=IRON,
        outline=GOLD_DEEP,
        width=max(SS * 2, 4),
    )
    # Corner studs on the accent rim
    stud = max(SS * 3, 6)
    rim = m + max(SS * 2, 4)
    for x, y in (
        (rim + stud, rim + stud),
        (s - rim - stud, rim + stud),
        (rim + stud, s - rim - stud),
        (s - rim - stud, s - rim - stud),
    ):
        draw_stud(d, x, y, stud, accent)

    cx, cy = s // 2, s // 2
    thick = max(SS * 7, 14)
    if glyph == "?":
        # Cohesive ? : thick arc + stem + separate dot
        r = s // 5
        box = [cx - r, cy - r - s // 14, cx + r, cy + r - s // 14]
        d.arc(box, start=200, end=40, fill=accent, width=thick)
        # Stem under the hook curve
        stem_top = cy + r // 5
        stem_bot = cy + r // 2 + s // 28
        d.rounded_rectangle(
            [cx - thick // 2, stem_top, cx + thick // 2, stem_bot],
            radius=thick // 3,
            fill=accent,
        )
        # Dot
        dot_r = thick // 2 + SS
        dy = cy + r // 2 + s // 9
        d.ellipse([cx - dot_r, dy - dot_r, cx + dot_r, dy + dot_r], fill=accent)
    else:
        # Exclamation: rounded bar + centered dot, clear gap
        bar_top = cy - s // 4
        bar_bot = cy + s // 14
        d.rounded_rectangle(
            [cx - thick // 2, bar_top, cx + thick // 2, bar_bot],
            radius=thick // 3,
            fill=accent,
        )
        dot_r = thick // 2 + SS
        dy = cy + s // 5
        d.ellipse([cx - dot_r, dy - dot_r, cx + dot_r, dy + dot_r], fill=accent)

    save(finish(img, out_size), name)


def draw_pin(color, name: str, out_size: int = 128) -> None:
    """Classic map pin: round head + tapered tip, kind-colored face."""
    img, d, s = canvas(out_size)
    cx = s // 2
    # Tip (drawn first so head overlaps cleanly)
    tip_y = s - s // 10
    head_bottom = int(s * 0.62)
    tip = [
        (cx, tip_y),
        (int(s * 0.28), head_bottom),
        (int(s * 0.72), head_bottom),
    ]
    d.polygon(tip, fill=IRON_DEEP)
    # Head disc
    hr = int(s * 0.30)
    hy = int(s * 0.38)
    d.ellipse([cx - hr, hy - hr, cx + hr, hy + hr], fill=IRON_DEEP, outline=color, width=max(SS * 3, 8))
    # Kind face
    ir = int(hr * 0.72)
    d.ellipse([cx - ir, hy - ir, cx + ir, hy + ir], fill=color)
    # Inner well + chrome stud
    wr = int(hr * 0.32)
    d.ellipse([cx - wr, hy - wr, cx + wr, hy + wr], fill=IRON_DEEP, outline=GOLD, width=max(SS * 2, 4))
    sr = max(wr // 2, SS * 2)
    d.ellipse([cx - sr, hy - sr, cx + sr, hy + sr], fill=CHROME)
    # Tip kind stripe so color reads at small sizes
    tip_inner = [
        (cx, tip_y - SS * 4),
        (int(s * 0.38), head_bottom - SS),
        (int(s * 0.62), head_bottom - SS),
    ]
    d.polygon(tip_inner, fill=color)
    save(finish(img, out_size), name)


def draw_player_arrow(name: str = "quest-player-arrow.png", out_size: int = 96) -> None:
    img, d, s = canvas(out_size)
    cx = s // 2
    # Slightly larger gold underlay for crisp rim
    gold_arrow = [
        (cx, int(s * 0.08)),
        (int(s * 0.86), int(s * 0.74)),
        (cx, int(s * 0.56)),
        (int(s * 0.14), int(s * 0.74)),
    ]
    d.polygon(gold_arrow, fill=GOLD)
    chrome_arrow = [
        (cx, int(s * 0.14)),
        (int(s * 0.78), int(s * 0.68)),
        (cx, int(s * 0.54)),
        (int(s * 0.22), int(s * 0.68)),
    ]
    d.polygon(chrome_arrow, fill=CHROME)
    pr = max(SS * 5, 8)
    py = int(s * 0.80)
    d.ellipse([cx - pr - SS, py - pr - SS, cx + pr + SS, py + pr + SS], fill=GOLD)
    d.ellipse([cx - pr, py - pr, cx + pr, py + pr], fill=CHROME)
    save(finish(img, out_size), name)


def draw_offscreen_chevron(name: str = "quest-offscreen-chevron.png", out_size: int = 96) -> None:
    img, d, s = canvas(out_size)
    # Clean right-pointing edge pointer (rotate-friendly); tip on the right
    body = [
        (int(s * 0.12), int(s * 0.28)),
        (int(s * 0.55), int(s * 0.28)),
        (int(s * 0.55), int(s * 0.12)),
        (int(s * 0.90), int(s * 0.50)),
        (int(s * 0.55), int(s * 0.88)),
        (int(s * 0.55), int(s * 0.72)),
        (int(s * 0.12), int(s * 0.72)),
    ]
    d.polygon(body, fill=GOLD)
    d.line(body + [body[0]], fill=IRON_DEEP, width=max(SS * 2, 4))
    save(finish(img, out_size), name)


def draw_waypoint_ring(name: str = "quest-waypoint-ring.png", out_size: int = 128) -> None:
    img, d, s = canvas(out_size)
    cx = cy = s // 2
    outer_r = int(s * 0.42)
    inner_r = int(s * 0.30)
    w_outer = max(SS * 4, 10)
    w_inner = max(SS * 2, 5)
    d.ellipse(
        [cx - outer_r, cy - outer_r, cx + outer_r, cy + outer_r],
        outline=GOLD,
        width=w_outer,
    )
    d.ellipse(
        [cx - inner_r, cy - inner_r, cx + inner_r, cy + inner_r],
        outline=GOLD_DEEP,
        width=w_inner,
    )
    stud = max(SS * 4, 8)
    for x, y in (
        (cx, cy - outer_r),
        (cx + outer_r, cy),
        (cx, cy + outer_r),
        (cx - outer_r, cy),
    ):
        draw_stud(d, x, y, stud, GOLD)
    save(finish(img, out_size), name)


def draw_path_dash(name: str = "quest-path-dash.png", out_size: int = 64) -> None:
    img, d, s = canvas(out_size)
    pad_x = int(s * 0.12)
    pad_y = int(s * 0.36)
    d.rounded_rectangle(
        [pad_x, pad_y, s - pad_x, s - pad_y],
        radius=max(SS * 2, 4),
        fill=GOLD,
    )
    save(finish(img, out_size), name)


def draw_minimap_quest_dot(color, name: str, out_size: int = 48) -> None:
    img, d, s = canvas(out_size)
    cx = cy = s // 2
    outer = int(s * 0.42)
    inner = int(s * 0.26)
    d.ellipse(
        [cx - outer, cy - outer, cx + outer, cy + outer],
        fill=IRON_DEEP,
        outline=color,
        width=max(SS * 3, 6),
    )
    d.ellipse([cx - inner, cy - inner, cx + inner, cy + inner], fill=color)
    save(finish(img, out_size), name)


def main() -> None:
    draw_glyph_mark("?", GOLD, "quest-mark-available.png")
    draw_glyph_mark("!", MAIN, "quest-mark-turnin.png")
    draw_pin(MAIN, "quest-pin-main.png")
    draw_pin(SIDE, "quest-pin-side.png")
    draw_pin(FACTION, "quest-pin-faction.png")
    draw_pin(ARCHETYPE, "quest-pin-archetype.png")
    draw_player_arrow()
    draw_offscreen_chevron()
    draw_waypoint_ring()
    draw_path_dash()
    draw_minimap_quest_dot(MAIN, "quest-minimap-dot-main.png")
    draw_minimap_quest_dot(SIDE, "quest-minimap-dot-side.png")
    draw_minimap_quest_dot(SUCCESS, "quest-minimap-dot-turnin.png")
    print("wrote icons to", ROOT)
    print("synced to", RUNTIME)


if __name__ == "__main__":
    main()
