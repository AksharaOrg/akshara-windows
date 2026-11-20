"""Builds the Akshara app icon and square tiles from assets/source/AksharaIconMaster.png.

    python tools/generate-app-icons.py

The master is the iOS app icon: a full-bleed dark square whose rounded corners are filled with white, because
iOS masks them itself. Windows shows icons as they are, so the corners are cut out here: a rounded-rectangle
mask with the master's corner radius, anti-aliased, with the dark fill under the edge so no white fringe is left.
Writes assets/windows/Akshara.ico (app, Start menu, Settings window) and the Square*Logo.png tiles. Needs Pillow.
The IME's own icon (assets/ime) is made by generate-icons.ps1.
"""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
MASTER = ROOT / "assets" / "source" / "AksharaIconMaster.png"
OUT = ROOT / "assets" / "windows"
RADIUS = 198 / 1024          # measured on the master: its corners are circular arcs of this radius
FILL = (16, 16, 16)          # the master's background
SUPERSAMPLE = 4
ICO_SIZES = [256, 128, 64, 48, 40, 32, 24, 20, 16]
TILES = [44, 50, 71, 150, 310]


def rounded(master: Image.Image) -> Image.Image:
    size = master.width
    big = size * SUPERSAMPLE
    mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, big - 1, big - 1), radius=round(RADIUS * big), fill=255)
    mask = mask.resize((size, size), Image.Resampling.LANCZOS)
    # Under the anti-aliased edge the master blends into white; paint the background colour there instead.
    edge = mask.point(lambda a: 255 if a < 255 else 0)
    rgb = master.convert("RGB")
    rgb.paste(Image.new("RGB", rgb.size, FILL), mask=edge)
    icon = rgb.convert("RGBA")
    icon.putalpha(mask)
    return icon


def main():
    icon = rounded(Image.open(MASTER))
    frames = [icon.resize((n, n), Image.Resampling.LANCZOS) for n in ICO_SIZES]
    frames[0].save(OUT / "Akshara.ico", format="ICO", sizes=[(n, n) for n in ICO_SIZES], append_images=frames[1:])
    print(f"wrote {OUT / 'Akshara.ico'}")
    for n in TILES:
        path = OUT / f"Square{n}x{n}Logo.png"
        icon.resize((n, n), Image.Resampling.LANCZOS).save(path, optimize=True)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
