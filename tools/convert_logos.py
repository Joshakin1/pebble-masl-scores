"""Convert team logo images into small Pebble bitmap resources.

Usage: python tools/convert_logos.py <input_dir>

Each <team>.png in input_dir (named by the team's short name in lowercase,
e.g. comets.png) becomes resources/images/logo_<team>.png, fit inside a
LOGO_W x LOGO_H box on a transparent canvas and snapped to Pebble's 64 colors.
Logos are only used on color watches; at this size they don't survive
conversion to black and white.

After adding a team, also add it to package.json and s_logos in src/c/main.c.
"""

import os
import sys

from PIL import Image

LOGO_W, LOGO_H = 24, 20
OUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'resources', 'images')


def fit(im):
    im = im.convert('RGBA')
    im = im.crop(im.getbbox())
    im.thumbnail((LOGO_W, LOGO_H), Image.LANCZOS)
    canvas = Image.new('RGBA', (LOGO_W, LOGO_H), (0, 0, 0, 0))
    canvas.paste(im, ((LOGO_W - im.width) // 2, (LOGO_H - im.height) // 2))
    return canvas


# Pebble color uses 2 bits per channel: 0x00, 0x55, 0xAA, 0xFF.
def snap(v):
    return min((0, 85, 170, 255), key=lambda c: abs(c - v))


def to_color(im):
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            px[x, y] = (snap(r), snap(g), snap(b), 255) if a >= 128 else (0, 0, 0, 0)
    return im


def main(in_dir):
    os.makedirs(OUT_DIR, exist_ok=True)
    for name in sorted(os.listdir(in_dir)):
        team, ext = os.path.splitext(name)
        if ext.lower() != '.png':
            continue
        logo = to_color(fit(Image.open(os.path.join(in_dir, name))))
        logo.save(os.path.join(OUT_DIR, 'logo_%s.png' % team))
        print(team)


if __name__ == '__main__':
    main(sys.argv[1])
