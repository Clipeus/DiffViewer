"""Generates the toolbar icons and the application icon in sources/res.

Run:  python tools/make_icons.py
Requires Pillow.
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sources", "res")
SS = 8  # supersampling factor for 16x16 icons (drawn at 128x128)

BLUE = (30, 110, 220, 255)
DARK_BLUE = (20, 80, 170, 255)
GREEN = (35, 140, 60, 255)
RED = (205, 55, 55, 255)
PURPLE = (120, 70, 210, 255)
GRAY = (95, 95, 95, 255)
WHITE = (255, 255, 255, 255)
YELLOW = (250, 200, 70, 255)
YELLOW_DARK = (200, 150, 30, 255)


def S(v):
    return v * SS


def canvas(size=16):
    img = Image.new("RGBA", (size * SS, size * SS), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def save(img, name, sizes=(16,)):
    path = os.path.join(ROOT, name)
    base = img.size[0] // SS
    first = img.resize((sizes[0], sizes[0]), Image.LANCZOS)
    extra = [img.resize((s, s), Image.LANCZOS) for s in sizes[1:]]
    first.save(path, format="ICO", sizes=[(s, s) for s in sizes], bitmap_format="bmp",
               append_images=extra)
    print("wrote", path)


def arrow_up(d, color, top=2, bottom=14, bar=None):
    d.polygon([(S(8), S(top)), (S(2.5), S(top + 6)), (S(13.5), S(top + 6))], fill=color)
    d.rectangle([S(6), S(top + 5.5), S(10), S(bottom)], fill=color)
    if bar is not None:
        d.rectangle([S(3), S(bar), S(13), S(bar + 1.8)], fill=color)


def icon_files():
    img, d = canvas()
    # back document
    d.polygon([(S(2), S(1)), (S(7), S(1)), (S(9.5), S(3.5)), (S(9.5), S(11)), (S(2), S(11))], fill=WHITE, outline=GRAY, width=SS)
    # front document
    d.polygon([(S(6), S(4.5)), (S(11), S(4.5)), (S(13.5), S(7)), (S(13.5), S(15)), (S(6), S(15))], fill=WHITE, outline=GRAY, width=SS)
    for y in (8.5, 10.5, 12.5):
        d.line([(S(7.5), S(y)), (S(12), S(y))], fill=BLUE, width=SS)
    save(img, "Files_16x16.ico")


def icon_folder():
    img, d = canvas()
    d.rounded_rectangle([S(1), S(2.5), S(7), S(6)], radius=S(0.8), fill=YELLOW_DARK)
    d.rounded_rectangle([S(1), S(4.5), S(15), S(13.5)], radius=S(1), fill=YELLOW, outline=YELLOW_DARK, width=SS)
    save(img, "Folder_16x16.ico")


def icon_pullrequest():
    img, d = canvas()
    r = S(2.2)
    for (cx, cy) in ((4, 3.2), (4, 12.8), (12, 12.8)):
        d.ellipse([S(cx) - r, S(cy) - r, S(cx) + r, S(cy) + r], outline=PURPLE, width=int(S(1.6)))
    d.line([(S(4), S(5.4)), (S(4), S(10.6))], fill=PURPLE, width=int(S(1.6)))
    d.line([(S(12), S(10.6)), (S(12), S(6.5))], fill=PURPLE, width=int(S(1.6)))
    d.arc([S(7), S(2.5), S(13.2), S(10.5)], start=180, end=270, fill=PURPLE, width=int(S(1.6)))
    d.line([(S(8.5), S(2.5)), (S(10), S(2.5))], fill=PURPLE, width=int(S(1.6)))
    d.polygon([(S(10.6), S(0.6)), (S(10.6), S(4.4)), (S(8.0), S(2.5))], fill=PURPLE)
    save(img, "PullRequest_16x16.ico")


def icon_refresh():
    img, d = canvas()
    d.arc([S(2.5), S(2.5), S(13.5), S(13.5)], start=20, end=300, fill=BLUE, width=int(S(2)))
    d.polygon([(S(14.8), S(2.2)), (S(15.2), S(8.2)), (S(10.0), S(4.6))], fill=BLUE)
    save(img, "Refresh_16x16.ico")


def icon_prev():
    img, d = canvas()
    arrow_up(d, BLUE, top=2, bottom=14)
    save(img, "Prev_16x16.ico")


def icon_next():
    img, d = canvas()
    arrow_up(d, BLUE, top=2, bottom=14)
    save(img.transpose(Image.FLIP_TOP_BOTTOM), "Next_16x16.ico")


def icon_first():
    img, d = canvas()
    arrow_up(d, BLUE, top=4.5, bottom=15, bar=1)
    save(img, "First_16x16.ico")


def icon_last():
    img, d = canvas()
    arrow_up(d, BLUE, top=4.5, bottom=15, bar=1)
    save(img.transpose(Image.FLIP_TOP_BOTTOM), "Last_16x16.ico")


def icon_swap():
    img, d = canvas()
    d.line([(S(2), S(5)), (S(11), S(5))], fill=BLUE, width=int(S(1.8)))
    d.polygon([(S(10), S(1.8)), (S(14.5), S(5)), (S(10), S(8.2))], fill=BLUE)
    d.line([(S(5), S(11)), (S(14), S(11))], fill=GREEN, width=int(S(1.8)))
    d.polygon([(S(6), S(7.8)), (S(1.5), S(11)), (S(6), S(14.2))], fill=GREEN)
    save(img, "Swap_16x16.ico")


def icon_whitespace():
    img, d = canvas()
    w = int(S(1.8))
    d.line([(S(3), S(6)), (S(3), S(12.5)), (S(13), S(12.5)), (S(13), S(6))], fill=BLUE, width=w, joint="curve")
    for x in (5.5, 8, 10.5):
        d.ellipse([S(x) - S(0.8), S(3) - S(0.8), S(x) + S(0.8), S(3) + S(0.8)], fill=GRAY)
    save(img, "Whitespace_16x16.ico")


def icon_help():
    img, d = canvas()
    d.ellipse([S(1), S(1), S(15), S(15)], fill=BLUE)
    try:
        font = ImageFont.truetype("arialbd.ttf", int(S(11.5)))
    except OSError:
        font = ImageFont.load_default()
    bbox = d.textbbox((0, 0), "?", font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    d.text((S(8) - tw / 2 - bbox[0], S(8) - th / 2 - bbox[1]), "?", fill=WHITE, font=font)
    save(img, "Help_16x16.ico")


def icon_app():
    img, d = canvas(16)
    d.rounded_rectangle([S(1), S(1.5), S(15), S(14.5)], radius=S(1.5), fill=(248, 248, 248, 255), outline=GRAY, width=SS)
    # left (removed) and right (added) halves
    d.rectangle([S(1.5), S(2), S(7.8), S(14)], fill=(255, 215, 215, 255))
    d.rectangle([S(8.2), S(2), S(14.5), S(14)], fill=(215, 245, 215, 255))
    d.line([(S(8), S(1.5)), (S(8), S(14.5))], fill=GRAY, width=SS)
    for i, (ll, rl) in enumerate(((3.5, 3.0), (2.5, 4.0), (4.0, 2.5), (3.0, 3.5))):
        y = 4 + i * 2.6
        d.rectangle([S(2.8), S(y), S(2.8 + ll), S(y + 1.1)], fill=RED)
        d.rectangle([S(9.4), S(y), S(9.4 + rl), S(y + 1.1)], fill=GREEN)
    # rounded corners overlay to restore the frame
    d.rounded_rectangle([S(1), S(1.5), S(15), S(14.5)], radius=S(1.5), outline=GRAY, width=SS)
    save(img, "DiffViewer.ico", sizes=(256, 48, 32, 16))


if __name__ == "__main__":
    os.makedirs(ROOT, exist_ok=True)
    for fn in (icon_files, icon_folder, icon_pullrequest, icon_refresh, icon_prev, icon_next,
               icon_first, icon_last, icon_swap, icon_whitespace, icon_help, icon_app):
        fn()
