"""Project-owned HUD status icons (bleed / poison) — true RGBA, no backdrop."""
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[2] / "samples" / "open-world-rpg" / "assets" / "ui" / "hud"


def faceted_droplet(path: Path, colors, accent=None, size: int = 64) -> None:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = size // 2
    tip = (cx, 8)
    left_mid = (14, 28)
    right_mid = (size - 14, 28)
    left_bot = (18, 48)
    right_bot = (size - 18, 48)
    bot = (cx, 56)
    outline = [tip, right_mid, right_bot, bot, left_bot, left_mid]
    d.polygon(outline, fill=(12, 10, 10, 255))
    main, dark, light = colors
    d.polygon([tip, right_mid, bot, left_mid], fill=main)
    d.polygon([tip, right_mid, right_bot, bot], fill=dark)
    d.polygon([tip, left_mid, left_bot, bot], fill=light)
    d.polygon([left_bot, bot, right_bot], fill=dark)
    if accent:
        d.ellipse([cx - 6, 22, cx + 4, 32], fill=accent)
        d.ellipse([cx - 4, 24, cx + 1, 29], fill=(255, 255, 255, 90))
    d.line(outline + [outline[0]], fill=(8, 6, 6, 255), width=2)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)
    corner = img.getpixel((1, 1))
    mid = img.getpixel((cx, cx))
    print(f"wrote {path.name} corner={corner} mid={mid}")


def main() -> None:
    faceted_droplet(
        OUT / "hud-status-bleed.png",
        colors=[(180, 42, 42, 255), (120, 24, 28, 255), (220, 72, 72, 255)],
        accent=(255, 160, 140, 200),
    )
    faceted_droplet(
        OUT / "hud-status-poison.png",
        colors=[(72, 160, 58, 255), (36, 96, 40, 255), (140, 210, 90, 255)],
        accent=(140, 70, 180, 230),
    )


if __name__ == "__main__":
    main()
