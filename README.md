# Xback

> Xwallpaper if it sucked less.

It's an X11 wallpaper utility designed to simply suck less (because *some* X11 wallpaper utilites have decided to inflate themselves)

## Dependencies

- libX11
- libXrandr
- libpng
- libjpeg
- libwebp

All of which your system probably has by default if you're using X11.

## Build

```bash
gcc -O2 -o xback main.c $(pkg-config --libs --cflags x11 xrandr libpng libjpeg libwebp)
```

## Usage

```bash
xback <monitor> <image> [--stretch|--fill|--focus]
```

```bash
# Fill monitor (default, sensible)
xback eDP-1 ~/walls/mywall.jpg

# Stretch to fit, aspect ratio be damned
xback eDP-1 ~/walls/mywall.jpg --stretch

# Fit entire image, letterboxed
xback eDP-1 ~/walls/mywall.jpg --focus
```

## Scale Modes

| Mode | Behaviour |
|------|-----------|
| `--fill` | Scale and crop to fill the monitor. Preserves aspect ratio. Default. |
| `--stretch` | Stretch to exact monitor dimensions. Looks dreadful. Users will use it anyway. |
| `--focus` | Fit entire image with letterboxing. Black bars. Whole image visible. |

## Supported Formats

PNG, JPEG, WebP. Detected by magic bytes, not file extension, because relying on file extensions is the behaviour of a fool.

## Installation (Arch/Arch-based distributions)

```bash
yay -S xback
```

## Why **YOU** should use xback

* It's not as bloated as its contemporaries 
* It's incredibly portable (compiles on nearly every compiler you can think of)
* Has small list of dependencies

