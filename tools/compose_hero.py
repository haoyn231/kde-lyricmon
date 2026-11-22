#!/usr/bin/env python3
"""Compose a README hero image: a popup render floating over a blurred wallpaper.

The popup is placed on a padded canvas, given a rounded mask at ~97% opacity and
a soft drop shadow, over a cover-cropped wallpaper that is blurred and veiled by
a light/dark wash (this is what Plasma's blur-behind looks like, minus the code).

All geometry is expressed in 1x units and scaled by (popup_width / 486), so the
same numbers work for a 1x render, a 2x render, or a differently sized popup.

Vendored from the user-level `readme-hero-shot` skill so this repo can re-render
its own hero images without depending on a home-directory path. Keep the two in
sync when the skill changes.

Calibrated against kde-perfmon's committed screenshot; see references/recipe.md.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageStat

# 1x units. The 2x equivalents used by the existing screenshot are in comments.
BASE_POPUP_WIDTH = 486.0
PAD_X = 54.0            # 108 @2x
PAD_Y = 59.0            # 118 @2x
RADIUS = 22.0           # 44  @2x
POPUP_ALPHA = 246 / 255 # 96.5%: lets a hint of the wallpaper through
BG_BLUR = 7.0           # sigma, 14 @2x
SHADOW_ALPHA = 0.075
SHADOW_BLUR = 13.0      # 26 @2x
SHADOW_OFFSET = 5.0     # 10 @2x, downward only

THEMES = {
    # theme -> (veil color, veil strength). Light popups sit on a whitened
    # wallpaper; dark popups need the wallpaper dimmed instead, otherwise the
    # popup loses all contrast (10% white already looks washed out).
    "light": ("#ffffff", 0.17),
    "dark": ("#000000", 0.45),
}


def plasma_wallpaper() -> Path:
    """First existing wallpaper referenced by the Plasma desktop config."""
    conf = Path.home() / ".config/plasma-org.kde.plasma.desktop-appletsrc"
    if not conf.is_file():
        sys.exit(f"no Plasma config at {conf}; pass --wallpaper explicitly")
    images = re.findall(r"^Image=file://(\S+)$", conf.read_text(errors="ignore"), re.M)
    if not images:
        sys.exit(f"no Image=file:// entry in {conf}; pass --wallpaper explicitly")
    from urllib.parse import unquote

    paths = [Path(unquote(p)) for p in images]
    found = [p for p in paths if p.is_file()]
    if not found:
        sys.exit(f"none of the configured wallpapers exist: {paths}")
    if len(images) > 1:
        print(f"note: {len(images)} wallpapers configured, using {found[0]}", file=sys.stderr)
    return found[0]


def cover_crop(image: Image.Image, width: int, height: int) -> Image.Image:
    """Scale to cover the canvas, then center-crop the overflow."""
    factor = max(width / image.width, height / image.height)
    resized = image.resize(
        (round(image.width * factor), round(image.height * factor)), Image.LANCZOS
    )
    left = (resized.width - width) // 2
    top = (resized.height - height) // 2
    return resized.crop((left, top, left + width, top + height))


def rounded_mask(size, box, radius, *, shift_y=0.0, blur=0.0):
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    left, top, right, bottom = box
    draw.rounded_rectangle(
        [left, top + shift_y, right, bottom + shift_y], radius=radius, fill=255
    )
    if blur:
        mask = mask.filter(ImageFilter.GaussianBlur(blur))
    return mask


def scaled(mask: Image.Image, alpha: float) -> Image.Image:
    """Multiply an L mask by alpha (keeps the rounded-corner antialiasing)."""
    if alpha >= 1.0:
        return mask
    table = [round(v * alpha) for v in range(256)]
    return mask.point(table)


def compose(popup: Image.Image, wallpaper: Image.Image, args) -> Image.Image:
    scale = popup.width / BASE_POPUP_WIDTH
    pad_x, pad_y = round(args.pad_x * scale), round(args.pad_y * scale)
    width, height = popup.width + 2 * pad_x, popup.height + 2 * pad_y

    box = (pad_x, pad_y, pad_x + popup.width - 1, pad_y + popup.height - 1)
    radius = args.radius * scale

    # Background: cover crop -> blur -> veil.
    background = cover_crop(wallpaper.convert("RGB"), width, height)
    background = background.filter(ImageFilter.GaussianBlur(args.bg_blur * scale))
    if args.overlay > 0:
        background = Image.blend(
            background,
            Image.new("RGB", background.size, args.overlay_color),
            args.overlay,
        )

    # Drop shadow: the same rounded rect, pushed down, blurred, then used to
    # blend black into the background.
    shadow = rounded_mask(
        (width, height),
        box,
        radius,
        shift_y=round(args.shadow_offset * scale),
        blur=args.shadow_blur * scale,
    )
    canvas = Image.composite(
        Image.new("RGB", (width, height), (0, 0, 0)),
        background,
        scaled(shadow, args.shadow_alpha),
    )

    # Popup: opaque pixels inside the rounded mask, at ~97% opacity.
    layer = Image.new("RGB", (width, height), (0, 0, 0))
    layer.paste(popup.convert("RGB"), (pad_x, pad_y))
    canvas = Image.composite(
        layer, canvas, scaled(rounded_mask((width, height), box, radius), args.alpha)
    )
    return canvas


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--popup", required=True, type=Path, help="popup render PNG")
    parser.add_argument("--out", required=True, type=Path, help="output PNG")
    parser.add_argument("--wallpaper", default="auto", help="'auto' or an image path")
    parser.add_argument("--theme", choices=sorted(THEMES), default="light")
    parser.add_argument("--overlay", type=float, default=None, help="veil strength 0..1")
    parser.add_argument("--overlay-color", default=None, help="veil color, e.g. #ffffff")
    parser.add_argument("--pad-x", type=float, default=PAD_X)
    parser.add_argument("--pad-y", type=float, default=PAD_Y)
    parser.add_argument("--radius", type=float, default=RADIUS)
    parser.add_argument("--alpha", type=float, default=POPUP_ALPHA)
    parser.add_argument("--bg-blur", type=float, default=BG_BLUR)
    parser.add_argument("--shadow-blur", type=float, default=SHADOW_BLUR)
    parser.add_argument("--shadow-alpha", type=float, default=SHADOW_ALPHA)
    parser.add_argument("--shadow-offset", type=float, default=SHADOW_OFFSET)
    parser.add_argument("--compare", type=Path, help="existing image; prints MAE")
    args = parser.parse_args()

    veil_color, veil = THEMES[args.theme]
    if args.overlay is None:
        args.overlay = veil
    if args.overlay_color is None:
        args.overlay_color = veil_color

    popup = Image.open(args.popup)
    wallpaper = (
        plasma_wallpaper()
        if args.wallpaper == "auto"
        else Path(args.wallpaper).expanduser()
    )
    if not wallpaper.is_file():
        sys.exit(f"wallpaper not found: {wallpaper}")

    image = compose(popup, Image.open(wallpaper), args)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.out, optimize=True)

    scale = popup.width / BASE_POPUP_WIDTH
    print(
        f"popup {popup.width}x{popup.height} @{scale:g}x  "
        f"wallpaper {wallpaper.name}  veil {args.overlay:.0%} {args.overlay_color}"
    )
    print(f"saved {args.out} {image.width}x{image.height} "
          f"{args.out.stat().st_size / 1024:.0f} KiB")

    if args.compare:
        reference = Image.open(args.compare).convert("RGB")
        if reference.size != image.size:
            print(f"compare: size differs {reference.size} vs {image.size}", file=sys.stderr)
            return 1
        channels = ImageStat.Stat(ImageChops.difference(image, reference)).mean
        print(f"compare: mean abs error {sum(channels) / len(channels):.2f}/255 "
              f"vs {args.compare}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
