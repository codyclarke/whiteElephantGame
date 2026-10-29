"""Writes placeholder PNGs into assets/ that match the game's built-in art.

They're templates at the right sizes, with the right frame counts, for you to
paint over. Existing files are never overwritten unless you pass --force.

People (you and the guests) are built from parts instead; their templates come
from tools/make_people_parts.py.

Usage: python tools/make_placeholders.py [--force]
"""

import math
import pathlib
import sys

from PIL import Image

ASSETS = pathlib.Path(__file__).resolve().parent.parent / "assets"

# Same colors as the palette in game/game.cpp.
BG = (28, 22, 44)
RUG = (74, 32, 40)
GOLD = (250, 210, 60)
DIM_GOLD = (125, 105, 30)
TEXT = (240, 236, 220)
TREE = (30, 140, 70)
TRUNK = (110, 70, 40)
COUCH = (120, 60, 50)
COUCH_DARK = (90, 42, 36)
GIFT_COLORS = [(220, 40, 60), (40, 120, 220), (240, 200, 60), (160, 80, 200), (40, 180, 120)]


class Canvas:
    """Tiny drawing helper matching the game's fill_rect / fill_circle."""

    def __init__(self, w, h, fill=None):
        self.img = Image.new("RGBA", (w, h), (0, 0, 0, 0) if fill is None else fill + (255,))
        self.px = self.img.load()

    def rect(self, x, y, w, h, color):
        for yy in range(max(y, 0), min(y + h, self.img.height)):
            for xx in range(max(x, 0), min(x + w, self.img.width)):
                self.px[xx, yy] = color + (255,)

    def outline(self, x, y, w, h, color):
        self.rect(x, y, w, 1, color)
        self.rect(x, y + h - 1, w, 1, color)
        self.rect(x, y, 1, h, color)
        self.rect(x + w - 1, y, 1, h, color)

    def circle(self, cx, cy, r, color):
        for dy in range(-r, r + 1):
            dx = int(math.sqrt(r * r + r - dy * dy))
            self.rect(cx - dx, cy + dy, dx * 2 + 1, 1, color)

    def paste(self, other, x, y):
        self.img.alpha_composite(other.img, (x, y))


def tree():
    # 2 frames of 64x64; the ornaments swap colors between them.
    ornaments = [(-4, -20), (5, -11), (-10, -4), (12, 2), (-18, 8), (2, 6), (20, 11)]
    sheet = Canvas(128, 64)
    for frame in range(2):
        c = Canvas(64, 64)
        c.rect(28, 53, 9, 6, TRUNK)
        for t in range(4):
            w = 14 + t * 12
            c.rect(32 - w // 2, 12 + t * 10, w, 11, TREE)
        for i, (dx, dy) in enumerate(ornaments):
            c.rect(32 + dx, 38 + dy, 2, 2, GIFT_COLORS[(i + frame * 2) % 5])
        c.rect(31, 4, 3, 7, GOLD)
        c.rect(29, 6, 7, 3, GOLD)
        sheet.paste(c, frame * 64, 0)
    return sheet


def gift_wrapped():
    # 5 frames of 14x12, one per wrapping paper.
    sheet = Canvas(14 * 5, 12)
    for i, color in enumerate(GIFT_COLORS):
        c = Canvas(14, 12)
        c.rect(0, 3, 14, 9, color)
        c.rect(6, 3, 2, 9, GOLD)
        c.rect(0, 7, 14, 1, GOLD)
        c.rect(3, 1, 3, 2, GOLD)
        c.rect(8, 1, 3, 2, GOLD)
        sheet.paste(c, i * 14, 0)
    return sheet


def gift_open():
    # 5 frames of 12x8: the opened gift shown above someone's head.
    sheet = Canvas(12 * 5, 8)
    for i, color in enumerate(GIFT_COLORS):
        c = Canvas(12, 8)
        c.rect(1, 2, 10, 6, color)
        c.rect(0, 0, 12, 2, color)
        c.rect(5, 0, 2, 8, GOLD)
        sheet.paste(c, i * 12, 0)
    return sheet


def couch():
    c = Canvas(112, 30)
    c.rect(6, 2, 100, 18, COUCH)
    c.rect(0, 12, 112, 18, COUCH_DARK)
    c.rect(0, 4, 10, 26, COUCH)
    c.rect(102, 4, 10, 26, COUCH)
    return c


def room():
    # The party room behind everyone, from below the meters to above the ticker.
    c = Canvas(240, 196, BG)
    c.rect(34, 36, 172, 128, RUG)
    c.outline(38, 40, 164, 120, DIM_GOLD)
    return c


def main():
    force = "--force" in sys.argv
    files = {}
    files.update({
        "tree_f2.png": tree(),
        "gift_wrapped_f5.png": gift_wrapped(),
        "gift_open_f5.png": gift_open(),
        "couch.png": couch(),
        "room.png": room(),
    })
    for rel, canvas in files.items():
        path = ASSETS / rel
        if path.exists() and not force:
            print(f"skip   {rel} (already exists)")
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        canvas.img.save(path)
        print(f"wrote  {rel}")


if __name__ == "__main__":
    main()
