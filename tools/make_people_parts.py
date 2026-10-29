"""Makes the character-builder part templates in assets/people/.

The base parts are sliced out of your own drawing, assets/guests/you_f4.png:
  people/body_f4.png           your four expressions with the hair and sweater
                               taken off (skin in gray, so it can be tinted)
  people/hair/classic.png      your hair, in gray
  people/sweaters/plain.png    your sweater, in gray

The rest are drawn here to the same proportions: more hairstyles, sweater
patterns, and hats. They're templates -- paint over them.

Tinting: parts use exactly four grays (the "ramp keys") which the game swaps
for a color ramp from game/content.h:
    255 highlight, 192 light, 128 mid, 64 shadow
Any other color (black outlines, a white snowflake, a red Santa hat) stays put.

Existing files are never overwritten unless you pass --force.

Usage: python tools/make_people_parts.py [--force]
"""

import pathlib
import re
import sys

from PIL import Image

ASSETS = pathlib.Path(__file__).resolve().parent.parent / "assets"
SOURCE = ASSETS / "guests" / "you_f4.png"
OUT = ASSETS / "people"
W, H = 24, 32

# The four tint keys, plus fixed colors used in the drawn parts.
COLORS = {
    "H": (255, 255, 255),  # highlight (tinted)
    "L": (192, 192, 192),  # light (tinted)
    "M": (128, 128, 128),  # mid (tinted)
    "S": (64, 64, 64),     # shadow (tinted)
    "B": (0, 0, 0),        # outline
    "W": (250, 250, 245),  # white yarn / fur trim
    "R": (205, 35, 45),    # Santa red
    "G": (250, 210, 60),   # gold
    "N": (140, 90, 50),    # antler / reindeer brown
    "T": (30, 110, 50),    # tree green
}

# Reference colors in your drawing, for slicing it into layers.
REF = {
    "hair": (173, 78, 26),
    "skin": (197, 151, 130),
    "knit": (55, 109, 3),
    "brow": (136, 99, 82),
}


# --- Slicing your drawing -----------------------------------------------------

def classify(pixel):
    r, g, b, a = pixel
    if a < 128:
        return "."
    if r < 40 and g < 40 and b < 40:
        return "B"
    best, best_d = "other", 40 * 40 * 3  # anything not close to a reference color stays as drawn
    for name, (rr, gg, bb) in REF.items():
        d = (r - rr) ** 2 + (g - gg) ** 2 + (b - bb) ** 2
        if d < best_d:
            best, best_d = name, d
    return best


def slice_frame(src, frame):
    px = src.load()
    cls = [[classify(px[frame * W + x, y]) for x in range(W)] for y in range(H)]
    orig = [[px[frame * W + x, y] for x in range(W)] for y in range(H)]

    def at(x, y):
        return cls[y][x] if 0 <= x < W and 0 <= y < H else "."

    def near(x, y, what, diagonal=False):
        steps = [(1, 0), (-1, 0), (0, 1), (0, -1)]
        if diagonal:
            steps += [(1, 1), (1, -1), (-1, 1), (-1, -1)]
        return any(at(x + dx, y + dy) == what for dx, dy in steps)

    body = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    hair = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    knit = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for y in range(H):
        for x in range(W):
            c = cls[y][x]
            if c == ".":
                continue
            hair_line = c == "B" and near(x, y, "hair")
            outer = c == "B" and near(x, y, ".", diagonal=True)

            # Body: hair becomes scalp, the hairline goes, the outer outline stays.
            if c in ("hair", "skin", "knit"):
                body.putpixel((x, y), COLORS["L"] + (255,))
            elif c == "brow":
                body.putpixel((x, y), COLORS["M"] + (255,))
            elif c == "B":
                body.putpixel((x, y), (COLORS["L"] if hair_line and not outer else COLORS["B"]) + (255,))
            else:
                body.putpixel((x, y), orig[y][x])

            # Hair and sweater layers (taken from the first frame only).
            if c == "hair":
                hair.putpixel((x, y), COLORS["L"] + (255,))
            elif hair_line:
                hair.putpixel((x, y), COLORS["B"] + (255,))
            if c == "knit":
                knit.putpixel((x, y), COLORS["L"] + (255,))
            elif c == "B" and near(x, y, "knit"):
                knit.putpixel((x, y), COLORS["B"] + (255,))
    return body, hair, knit


# --- Drawn parts --------------------------------------------------------------
# Each is a list of (row, 24-character string). '.' = nothing.

HAIR = {
    "long": [
        (1, "........BBBBBBB........."),
        (2, "......BBLLLLLLLBB......."),
        (3, ".....BLHLLLLLLLLLB......"),
        (4, "....BLHLLLLBLLLLLLB....."),
        (5, "....BLLLLLBMBLLLLLB....."),
        (6, "...BLLLLLBM.MBLLLLLB...."),
        (7, "...BLLLLB.....BLLLLB...."),
        (8, "..BLLLLB.......BLLLLB..."),
        (9, "..BLLLB.........BLLLB..."),
        (10, "..BLLLB.........BLLLB..."),
        (11, "..BLLLB.........BLLLB..."),
        (12, "..BLLLB.........BLLLB..."),
        (13, "..BLLMB.........BMLLB..."),
        (14, "..BLLMB.........BMLLB..."),
        (15, "..BLMMB.........BMMLB..."),
        (16, "..BMMSB.........BSMMB..."),
        (17, "...BSSB.........BSSB...."),
        (18, "....BB...........BB....."),
    ],
    "curly": [
        (0, ".......BBB.BBB.BB......."),
        (1, ".....BBLLLBLLLBLLBB....."),
        (2, "....BLLHLLLLHLLLLLLB...."),
        (3, "...BLLLLLMLLLLLMLLLLB..."),
        (4, "..BLLHLLLLLLHLLLLLLLLB.."),
        (5, "..BLLLLMLLLLLLLMLLLLLB.."),
        (6, ".BLLMLLLLLHLLLLLLMLLLLB."),
        (7, ".BLLLLLLMLLLLLMLLLLLLLB."),
        (8, ".BLLLMB.........BLLMLLB."),
        (9, ".BLLLB...........BLLLB.."),
        (10, "..BLMB...........BMLB..."),
        (11, "...BB.............BB...."),
    ],
    "bun": [
        (0, ".........BBBBB.........."),
        (1, "........BLLHLLB........."),
        (2, "........BLLLLMB........."),
        (3, ".......BBBMMMBBB........"),
        (4, ".....BBLLLLLLLLLBB......"),
        (5, "....BLLHLLLLLLLLLLB....."),
        (6, "....BLLLLLLLLLLLLLB....."),
        (7, "...BLLLLBBBBBBBLLLLB...."),
        (8, "...BLLB.........BLLB...."),
        (9, "...BMB...........BMB...."),
    ],
    "spiky": [
        (0, "....B...B...B...B...B..."),
        (1, "...BLB.BLB.BLB.BLB.BLB.."),
        (2, "...BLLBLLLBLHLBLLLBLLB.."),
        (3, "...BLLLLLLLLLLLLLLLLLB.."),
        (4, "...BLHLLLLLLLLLLLLLLLB.."),
        (5, "...BLLLLLMLLLLLMLLLLLB.."),
        (6, "...BLLLMBBBBBBBBBMLLLB.."),
        (7, "...BLLB.........BLLB...."),
        (8, "...BBB...........BB....."),
    ],
}

HATS = {
    "santa_hat": [
        (0, ".........BBBB..........."),
        (1, ".......BBRRRRBB........."),
        (2, "......BRRRRRRRRBB..BBB.."),
        (3, ".....BRRRRRRRRRRRBBWWWB."),
        (4, ".....BRRRRRRRRRRRRRBWWB."),
        (5, "....BWWWWWWWWWWWWWWB.B.."),
        (6, "...BWWWWWWWWWWWWWWWWB..."),
        (7, "...BBBBBBBBBBBBBBBBBB..."),
    ],
    "antlers": [
        (0, "..N.N.N........N.N.N...."),
        (1, "..NNNNN........NNNNN...."),
        (2, "....NN..........NN......"),
        (3, "....N............N......"),
    ],
    "bow": [
        (1, "..............BB.BB....."),
        (2, ".............BRRBRRB...."),
        (3, ".............BRRRRRB...."),
        (4, "..............BB.BB....."),
    ],
    # Knit beanie in the tint grays, so it matches the sweater color.
    "beanie": [
        (0, "..........BWWB.........."),
        (1, "........BBBWWBBB........"),
        (2, "......BBLLLLLLLLBB......"),
        (3, ".....BLLLLLLLLLLLLB....."),
        (4, "....BLLHLLHLLHLLHLLB...."),
        (5, "....BLLLLLLLLLLLLLLB...."),
        (6, "...BMMMMMMMMMMMMMMMMB..."),
        (7, "...BBBBBBBBBBBBBBBBBB..."),
    ],
}

# Sweater patterns: stamped onto the plain sweater, only over its knit.
SNOWFLAKE = ["..W..", "W.W.W", ".WWW.", "W.W.W", "..W.."]
REINDEER = ["N.N...N.N", ".NN...NN.", "..NNNNN..", "..NWNWN..", "...NNN...", "...NRN...", "....N...."]
TREE = ["...G...", "...T...", "..TTT..", ".TTWTT.", "..TTT..", ".TWTTT.", "TTTTTWT", "...N..."]


def draw(rows):
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for y, line in rows:
        assert len(line) == W, f"row {y} is {len(line)} wide: {line!r}"
        for x, ch in enumerate(line):
            if ch != ".":
                img.putpixel((x, y), COLORS[ch] + (255,))
    return img


def stamp(sweater, pattern, left, top):
    out = sweater.copy()
    knit = COLORS["L"] + (255,)
    for dy, line in enumerate(pattern):
        for dx, ch in enumerate(line):
            x, y = left + dx, top + dy
            if ch != "." and out.getpixel((x, y)) == knit:
                out.putpixel((x, y), COLORS[ch] + (255,))
    return out


def stripes(sweater):
    out = sweater.copy()
    knit = COLORS["L"] + (255,)
    for y in (24, 28):
        for x in range(W):
            if out.getpixel((x, y)) == knit:
                out.putpixel((x, y), COLORS["W"] + (255,))
    return out


def main():
    force = "--force" in sys.argv
    if not SOURCE.exists():
        sys.exit(f"Need {SOURCE} to slice the base parts from.")
    src = Image.open(SOURCE).convert("RGBA")
    frames = src.width // W

    body = Image.new("RGBA", (W * frames, H), (0, 0, 0, 0))
    for f in range(frames):
        b, h, k = slice_frame(src, f)
        body.paste(b, (f * W, 0))
        if f == 0:
            classic, plain = h, k

    files = {
        f"body_f{frames}.png": body,
        "hair/classic.png": classic,
        "sweaters/plain.png": plain,
        "sweaters/stripes.png": stripes(plain),
        "sweaters/snowflake.png": stamp(stamp(stamp(plain, SNOWFLAKE, 3, 22), SNOWFLAKE, 14, 22), SNOWFLAKE, 8, 26),
        "sweaters/reindeer.png": stamp(plain, REINDEER, 6, 22),
        "sweaters/tree.png": stamp(plain, TREE, 7, 22),
    }
    files.update({f"hair/{name}.png": draw(rows) for name, rows in HAIR.items()})
    files.update({f"hats/{name}.png": draw(rows) for name, rows in HATS.items()})

    for rel, img in files.items():
        path = OUT / rel
        # A painted version may have gained frames (spiky.png -> spiky_f2.png).
        base = re.sub(r"_f\d+$", "", path.stem)
        existing = [path] if path.exists() else list(path.parent.glob(f"{base}_f*.png"))
        if existing and not force:
            print(f"skip   people/{rel} ({existing[0].name} already exists)")
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        img.save(path)
        print(f"wrote  people/{rel}")


if __name__ == "__main__":
    main()
