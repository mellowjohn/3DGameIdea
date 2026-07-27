from PIL import Image, ImageFilter
import math

W, H = 512, 128
img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
px = img.load()


def curve_y(t: float) -> float:
    return 0.5 + 0.18 * math.sin(t * math.pi * 2.0 - 0.4) + 0.06 * math.sin(t * math.pi * 4.0)


for x in range(W):
    t = x / (W - 1)
    edge = min(t, 1.0 - t)
    taper = max(0.0, min(1.0, edge / 0.12))
    taper = taper * taper * (3 - 2 * taper)
    cy = curve_y(t) * (H - 1)
    for y in range(H):
        dy = abs(y - cy)
        core = math.exp(-(dy * dy) / (2.2 * 2.2))
        glow = math.exp(-(dy * dy) / (9.0 * 9.0))
        wisp = 0.55 * math.exp(-((dy - 2.5) ** 2) / (1.4 * 1.4)) + 0.35 * math.exp(
            -((dy + 2.0) ** 2) / (1.6 * 1.6)
        )
        a = (0.95 * core + 0.35 * glow + 0.25 * wisp) * taper
        a = max(0.0, min(1.0, a))
        if a < 0.01:
            continue
        r = int(210 + 45 * core)
        g = int(225 + 30 * core)
        b = 240
        px[x, y] = (min(255, r), min(255, g), min(255, b), int(a * 255))

img = img.filter(ImageFilter.GaussianBlur(radius=0.8))
out = r"c:\Users\johnr\Documents\3DGameIdea\samples\open-world-rpg\assets\vfx\wind_streak.png"
img.save(out, "PNG")
print("wrote", out, img.size, img.mode)
