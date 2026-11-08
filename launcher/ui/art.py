"""The window's pictures, drawn with numpy into RGBA arrays and handed to Tk as PNG data.

Everything is drawn here from shapes and colours: the misty green backdrop, the wooden title band, the bronze label
cells with their mottled texture, the description box, the white glowing cursor, the filigree divider, the Triforce
and the small icons. Tk's canvas draws lines and polygons without antialiasing on Windows, so every curved or soft
shape is a picture; flat rectangles and text stay canvas items.

Colours follow the game's options menu (docs/launcher.md, "Look"). Sizes are in logical pixels times the display
scale s.
"""
import base64
import re
import struct
import zlib

import numpy as np

# ---------------------------------------------------------------- colours

PALETTE = {
    'window': '#06180d',
    'panel': '#3a2b22',
    'label_top': '#7a5c32', 'label_mid': '#775930', 'label_bottom': '#72552d', 'label_border': '#6a4f29',
    'ring': '#2a2019',
    'value': '#2f2d22', 'value_border': '#45402f',
    'orange': '#d69127', 'orange_light': '#e3a234', 'orange_dark': '#8f5f17',
    'desc': '#403024', 'desc_border': '#4f3f30',
    'text': '#e9dcc0', 'text_bright': '#f1e6cc', 'value_text': '#e3d5b6', 'desc_text': '#e9dfc8',
    'muted': '#d4c6a6', 'dim': '#7d786a', 'off': '#5e5b51', 'on_text': '#fdf4de', 'salmon': '#e88f6c',
    'ok': '#a7d68c', 'gold': '#e6b65a', 'band_text': '#f3ecdc', 'status': '#b9a988', 'log_text': '#cfc4ab',
    'line': '#54442f', 'dot': '#5d4b36', 'filigree': '#7a6443', 'glow': '#e9ebee',
    'tri_top': '#d49f36', 'tri_bottom': '#fcbf44',
}


def rgb(hexcolor):
    h = hexcolor.lstrip('#')
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)], dtype=np.float32)


# ---------------------------------------------------------------- PNG and Tk

def png_bytes(img):
    """PNG of an HxWx4 float array in 0..1 (straight alpha) or uint8 array."""
    a = img if img.dtype == np.uint8 else (np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)
    h, w = a.shape[:2]
    raw = b''.join(b'\0' + a[y].tobytes() for y in range(h))

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 3)) + chunk(b'IEND', b''))


def photo(master, img):
    import tkinter as tk
    return tk.PhotoImage(master=master, data=base64.b64encode(png_bytes(img)).decode('ascii'), format='png')


def ico_bytes(images):
    """A Windows .ico holding PNG-compressed images (one HxWx4 array per size, square)."""
    pngs = [png_bytes(im) for im in images]
    head = struct.pack('<HHH', 0, 1, len(pngs))
    offset = 6 + 16 * len(pngs)
    dirs, body = b'', b''
    for im, data in zip(images, pngs):
        n = im.shape[0]
        dirs += struct.pack('<BBBBHHII', n % 256, n % 256, 0, 0, 1, 32, len(data), offset + len(body))
        body += data
    return head + dirs + body


# ---------------------------------------------------------------- drawing primitives

def new(h, w, color=None, alpha=0.0):
    img = np.zeros((h, w, 4), dtype=np.float32)
    if color is not None:
        img[..., :3] = rgb(color)
        img[..., 3] = alpha
    return img


def over(dst, src, x=0, y=0):
    """Composite src over dst at (x, y), straight alpha, clipped to dst."""
    h, w = src.shape[:2]
    H, W = dst.shape[:2]
    x0, y0, x1, y1 = max(x, 0), max(y, 0), min(x + w, W), min(y + h, H)
    if x0 >= x1 or y0 >= y1:
        return dst
    s = src[y0 - y:y1 - y, x0 - x:x1 - x]
    d = dst[y0:y1, x0:x1]
    sa, da = s[..., 3:4], d[..., 3:4]
    oa = sa + da * (1 - sa)
    oc = np.where(oa > 0, (s[..., :3] * sa + d[..., :3] * da * (1 - sa)) / np.maximum(oa, 1e-6), 0)
    d[..., :3], d[..., 3:4] = oc, oa
    return dst


def paint(img, mask, color, alpha=1.0):
    """Paint a colour through a coverage mask (HxW, 0..1) onto img."""
    layer = new(*mask.shape, color)
    layer[..., 3] = np.clip(mask, 0, 1) * alpha
    return over(img, layer)


def vgradient(h, w, stops):
    """HxWx3 colours down the height through (position 0..1, colour) stops."""
    pos = np.array([p for p, _ in stops], dtype=np.float32)
    cols = np.stack([rgb(c) for _, c in stops])
    t = (np.arange(h, dtype=np.float32) + 0.5) / max(h, 1)
    col = np.stack([np.interp(t, pos, cols[:, k]) for k in range(3)], axis=1)
    return np.broadcast_to(col[:, None, :], (h, w, 3)).copy()


def hgradient_alpha(h, w, stops):
    """HxW alphas across the width through (position, alpha) stops."""
    pos = np.array([p for p, _ in stops], dtype=np.float32)
    al = np.array([a for _, a in stops], dtype=np.float32)
    t = (np.arange(w, dtype=np.float32) + 0.5) / max(w, 1)
    return np.broadcast_to(np.interp(t, pos, al)[None, :], (h, w)).copy()


def hgradient_rgba(h, w, stops):
    """An RGBA layer whose colour and alpha run across the width through (position, colour, alpha) stops."""
    pos = np.array([p for p, _, _ in stops], dtype=np.float32)
    cols = np.stack([rgb(c) for _, c, _ in stops])
    al = np.array([a for _, _, a in stops], dtype=np.float32)
    t = (np.arange(w, dtype=np.float32) + 0.5) / max(w, 1)
    layer = new(h, w)
    for k in range(3):
        layer[..., k] = np.interp(t, pos, cols[:, k])[None, :]
    layer[..., 3] = np.interp(t, pos, al)[None, :]
    return layer


def value_noise(h, w, period_x, period_y=None, seed=0):
    """Smooth noise in 0..1 with features about period pixels apart."""
    period_y = period_y or period_x
    rng = np.random.default_rng(seed)
    gh, gw = int(h / period_y) + 3, int(w / period_x) + 3
    g = rng.random((gh, gw), dtype=np.float32)
    ys = np.arange(h, dtype=np.float32) / period_y
    xs = np.arange(w, dtype=np.float32) / period_x
    y0, x0 = ys.astype(int), xs.astype(int)
    fy, fx = ys - y0, xs - x0
    fy, fx = fy * fy * (3 - 2 * fy), fx * fx * (3 - 2 * fx)
    a, b = g[y0][:, x0], g[y0][:, x0 + 1]
    c, d = g[y0 + 1][:, x0], g[y0 + 1][:, x0 + 1]
    top = a + (b - a) * fx[None, :]
    bot = c + (d - c) * fx[None, :]
    return top + (bot - top) * fy[:, None]


def fractal(h, w, period_x, period_y=None, octaves=3, seed=0, contrast=0.4):
    """Fractal noise around mid grey, about contrast wide (the range of an SVG feTurbulence turned grey)."""
    return 0.5 + (_fractal01(h, w, period_x, period_y, octaves, seed) - 0.5) * contrast


def _fractal01(h, w, period_x, period_y=None, octaves=3, seed=0):
    period_y = period_y or period_x
    out = np.zeros((h, w), dtype=np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        out += amp * value_noise(h, w, max(period_x / 2 ** o, 1.5), max(period_y / 2 ** o, 1.5), seed + o * 101)
        total += amp
        amp *= 0.5
    out /= total
    lo, hi = np.percentile(out, 1), np.percentile(out, 99)
    return np.clip((out - lo) / max(hi - lo, 1e-6), 0, 1)


def overlay(rgb_arr, n, opacity):
    """CSS mix-blend-mode overlay of a grey noise n (HxW) over colours, at opacity."""
    n = n[..., None]
    blended = np.where(rgb_arr < 0.5, 2 * rgb_arr * n, 1 - 2 * (1 - rgb_arr) * (1 - n))
    return rgb_arr + (blended - rgb_arr) * opacity


def box_blur(a, r):
    """Three box blurs of radius r over a 2-D array (close to a gaussian)."""
    r = int(round(r))
    if r < 1:
        return a
    out = a.astype(np.float32)
    for _ in range(3):
        for axis in (0, 1):
            pad = [(0, 0), (0, 0)]
            pad[axis] = (r + 1, r)
            p = np.pad(out, pad, mode='edge')
            c = np.cumsum(p, axis=axis, dtype=np.float64)
            if axis == 0:
                out = ((c[2 * r + 1:] - c[:-2 * r - 1]) / (2 * r + 1)).astype(np.float32)
            else:
                out = ((c[:, 2 * r + 1:] - c[:, :-2 * r - 1]) / (2 * r + 1)).astype(np.float32)
    return out


def rect_mask(h, w, x0, y0, x1, y1):
    """Coverage of the rectangle [x0, x1) x [y0, y1) with fractional edges."""
    xs = np.arange(w, dtype=np.float32)
    ys = np.arange(h, dtype=np.float32)
    cx = np.clip(np.minimum(xs + 1, x1) - np.maximum(xs, x0), 0, 1)
    cy = np.clip(np.minimum(ys + 1, y1) - np.maximum(ys, y0), 0, 1)
    return cy[:, None] * cx[None, :]


def ring_mask(h, w, x0, y0, x1, y1, width):
    return np.clip(rect_mask(h, w, x0, y0, x1, y1) - rect_mask(h, w, x0 + width, y0 + width, x1 - width, y1 - width),
                   0, 1)


def polygon_mask(h, w, pts, ss=4):
    """Antialiased coverage of a convex polygon (pixel coordinates), supersampled ss x ss."""
    ys = (np.arange(h * ss, dtype=np.float32) + 0.5) / ss
    xs = (np.arange(w * ss, dtype=np.float32) + 0.5) / ss
    X, Y = np.meshgrid(xs, ys)
    inside = np.ones_like(X, dtype=bool)
    n = len(pts)
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    sign = 1 if area > 0 else -1
    for i in range(n):
        (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
        inside &= sign * ((bx - ax) * (Y - ay) - (by - ay) * (X - ax)) >= 0
    return inside.reshape(h, ss, w, ss).mean(axis=(1, 3)).astype(np.float32)


def stroke_mask(h, w, polylines, width):
    """Antialiased coverage of polylines (lists of (x, y)) stroked with round joins and caps."""
    ys = np.arange(h, dtype=np.float32) + 0.5
    xs = np.arange(w, dtype=np.float32) + 0.5
    X, Y = np.meshgrid(xs, ys)
    best = np.full((h, w), 1e9, dtype=np.float32)
    for line in polylines:
        P = np.asarray(line, dtype=np.float32)
        if len(P) == 1:
            P = np.vstack([P, P])
        for (ax, ay), (bx, by) in zip(P[:-1], P[1:]):
            dx, dy = bx - ax, by - ay
            ll = dx * dx + dy * dy
            if ll < 1e-9:
                d = np.hypot(X - ax, Y - ay)
            else:
                t = np.clip(((X - ax) * dx + (Y - ay) * dy) / ll, 0, 1)
                d = np.hypot(X - (ax + t * dx), Y - (ay + t * dy))
            np.minimum(best, d, out=best)
    return np.clip(width / 2 + 0.5 - best, 0, 1)


def shadow(h, w, mask, color, alpha, blur, dx=0, dy=0):
    """A soft shadow of a mask: blurred, offset, as an RGBA layer of the same size."""
    m = box_blur(np.roll(np.roll(mask, int(dy), 0), int(dx), 1), blur / 2)
    layer = new(h, w, color)
    layer[..., 3] = np.clip(m, 0, 1) * alpha
    return layer


# ---------------------------------------------------------------- SVG path data (the filigree)

def svg_polylines(d, scale=1.0, ox=0.0, oy=0.0, steps=10):
    """Points along an SVG path made of M, L, C and Z commands."""
    toks = re.findall(r'[MLCZmlcz]|-?\d*\.?\d+', d)
    lines, cur, pos, i, cmd = [], [], (0.0, 0.0), 0, None
    start = (0.0, 0.0)

    def num():
        nonlocal i
        v = float(toks[i])
        i += 1
        return v

    while i < len(toks):
        if toks[i] in 'MLCZmlcz':
            cmd = toks[i]
            i += 1
            if cmd in 'Zz':
                cur.append(start)
                continue
        if cmd in 'Mm':
            if cur:
                lines.append(cur)
            pos = (num(), num())
            start = pos
            cur = [pos]
            cmd = 'L'
        elif cmd in 'Ll':
            pos = (num(), num())
            cur.append(pos)
        elif cmd in 'Cc':
            p0 = pos
            c1, c2, p1 = (num(), num()), (num(), num()), (num(), num())
            for k in range(1, steps + 1):
                t = k / steps
                u = 1 - t
                cur.append((u ** 3 * p0[0] + 3 * u * u * t * c1[0] + 3 * u * t * t * c2[0] + t ** 3 * p1[0],
                            u ** 3 * p0[1] + 3 * u * u * t * c1[1] + 3 * u * t * t * c2[1] + t ** 3 * p1[1]))
            pos = p1
    if cur:
        lines.append(cur)
    return [[((x - ox) * scale, (y - oy) * scale) for x, y in line] for line in lines]


# The game's divider ornament, traced from the options menu: two pointed lens ends, S-waves into spiral hooks and a
# diamond knot under the middle, in a 120 x 38 box (centre x 60).
FILIGREE_PATHS = (
    'M20 17.4 C20.4 17.3 21.6 16.8 22.2 16.9 C22.8 17 23.5 17.7 23.5 18.2 C23.5 18.7 23.1 19.4 22.4 19.8 C21.6 20.2 '
    '20.3 20.4 19 20.4 C17.7 20.4 15.9 20.2 14.5 19.9 C13.1 19.6 10.6 19.4 10.5 18.6 C10.4 17.9 12.6 16.3 14 15.4 '
    'C15.4 14.5 17.2 13.6 19 13.2 C20.8 12.8 23.2 12.8 25 13.1 C26.8 13.4 28.1 13.9 29.5 14.8 C30.9 15.7 32.2 17.4 '
    '33.5 18.6 C34.8 19.8 36 21.2 37.5 22 C39 22.8 40.8 23.4 42.5 23.6 C44.2 23.8 46 23.5 47.5 23.1 C49 22.7 50.4 '
    '21.9 51.2 21.2 C52 20.5 52.5 19.5 52.4 18.8 C52.3 18.1 51.5 17.3 50.6 16.9 C49.7 16.5 48.2 16.2 47 16.3 C45.8 '
    '16.4 44.2 17.1 43.6 17.2',
    'M100 17.4 C99.6 17.3 98.4 16.8 97.8 16.9 C97.2 17 96.5 17.7 96.5 18.2 C96.5 18.7 96.8 19.4 97.6 19.8 C98.3 20.2 '
    '99.7 20.4 101 20.4 C102.3 20.4 104.1 20.2 105.5 19.9 C106.9 19.6 109.4 19.4 109.5 18.6 C109.6 17.9 107.4 16.3 '
    '106 15.4 C104.6 14.5 102.8 13.6 101 13.2 C99.2 12.8 96.8 12.8 95 13.1 C93.2 13.4 91.9 13.9 90.5 14.8 C89.1 15.7 '
    '87.8 17.4 86.5 18.6 C85.2 19.8 84 21.2 82.5 22 C81 22.8 79.2 23.4 77.5 23.6 C75.8 23.8 74 23.5 72.5 23.1 C71 '
    '22.7 69.6 21.9 68.8 21.2 C68 20.5 67.5 19.5 67.6 18.8 C67.7 18.1 68.5 17.3 69.4 16.9 C70.3 16.5 71.8 16.2 73 '
    '16.3 C74.2 16.4 75.8 17.1 76.4 17.2',
    'M40.6 17.6 C40.7 17 40.7 14.9 41.2 14 C41.7 13.1 42.6 12.4 43.8 12 C45 11.6 46.7 11.4 48.2 11.5 C49.7 11.6 51.3 '
    '11.8 52.6 12.3 C53.9 12.8 55 13.6 56 14.6 C57 15.6 58.2 17.6 58.6 18.2 L64.6 26 L60 31.6',
    'M79.4 17.6 C79.3 17 79.3 14.9 78.8 14 C78.3 13.1 77.4 12.4 76.2 12 C75 11.6 73.3 11.4 71.8 11.5 C70.3 11.6 68.7 '
    '11.8 67.4 12.3 C66.1 12.8 65 13.6 64 14.6 C63 15.6 61.8 17.6 61.4 18.2 L55.4 26 L60 31.6',
)


# ---------------------------------------------------------------- the window's pieces

def backdrop(W, H, s):
    """The whole window behind the panels: a dim misty field under a see-through dark green, darker at the sides."""
    img = new(H, W, PALETTE['window'], 1.0)
    m = int(30 * s)
    scene = new(H, W - 2 * m)
    scene[..., :3] = vgradient(H, W - 2 * m, [(0, '#394244'), (0.36, '#414b4c'), (0.60, '#474234'), (1, '#3e3523')])
    scene[..., 3] = 1
    over(img, scene, m, 0)
    band = np.zeros((H, W), dtype=np.float32)
    y0, y1 = int(250 * s), int(330 * s)
    t = (np.arange(y1 - y0) + 0.5) / max(y1 - y0, 1)
    band[y0:y1, m:W - m] = (np.interp(t, [0, 0.55, 1], [0, 0.85, 0]))[:, None]
    paint(img, box_blur(band, 10 * s), '#262a22')
    over(img, hgradient_rgba(H, W, [(0, '#05180d', 0.96), (0.14, '#092615', 0.82), (0.5, '#0c2e1b', 0.72),
                                    (0.86, '#092615', 0.82), (1, '#05180d', 0.96)]))
    inner = rect_mask(H, W, 0, 0, W, H)
    vig = 1 - box_blur(np.pad(inner, int(45 * s), constant_values=0), 45 * s)[int(45 * s):-int(45 * s),
                                                                              int(45 * s):-int(45 * s)]
    paint(img, np.clip(vig * 1.6, 0, 1), '#020e07', 0.6)
    return img


def band(w, h, s):
    """The wooden title band, its two-line bottom edge and the shadow under it; (image, extra height below)."""
    pad = int(14 * s)
    img = new(h + pad, w)
    wood = vgradient(h, w, [(0, '#4c3b26'), (0.30, '#5a4529'), (0.65, '#6b5232'), (1, '#806239')])
    wood = overlay(wood, fractal(h, w, 300 * s, 11 * s, 3, seed=9), 0.32)
    wood = overlay(wood, fractal(h, w, 33 * s, None, 3, seed=4), 0.2)
    shade = rect_mask(h + pad, w, 0, 0, w, h + 4 * s)
    over(img, shadow(h + pad, w, shade, '#000000', 0.6, 16 * s, 0, 5 * s))
    top = new(h, w)
    top[..., :3], top[..., 3] = wood, 1
    over(img, top)
    paint(img, rect_mask(h + pad, w, 0, h, w, h + 2 * s), '#2c2217')
    paint(img, rect_mask(h + pad, w, 0, h + 2 * s, w, h + 4 * s), '#a9874f')
    return img


def panel(w, h, s):
    """The left list panel with the shadow on its right."""
    sh = int(26 * s)
    img = new(h, w + sh)
    body = rect_mask(h, w + sh, 0, 0, w, h)
    over(img, shadow(h, w + sh, body, '#000000', 0.35, 18 * s, 6 * s, 0))
    col = np.broadcast_to(rgb(PALETTE['panel']), (h, w, 3)).copy()
    col = overlay(col, fractal(h, w, 33 * s, None, 3, seed=4), 0.18)
    layer = new(h, w)
    layer[..., :3], layer[..., 3] = col, 1
    over(img, layer)
    return img


def label_cell(w, h, s, bright=False):
    """A bronze label cell: gradient, mottled texture, 1 px border and the 1 px dark ring around the row."""
    r = max(1, int(round(s)))
    img = new(h + 2 * r, w + 2 * r, PALETTE['ring'], 1.0)
    col = vgradient(h, w, [(0, PALETTE['label_top']), (0.5, PALETTE['label_mid']), (1, PALETTE['label_bottom'])])
    col = overlay(col, fractal(h, w, 33 * s, None, 3, seed=(w * 7 + h) % 997), 0.22)
    if bright:
        col = np.clip(col * 1.08, 0, 1)
    cell = new(h, w)
    cell[..., :3], cell[..., 3] = col, 1
    over(img, cell, r, r)
    paint(img, ring_mask(h + 2 * r, w + 2 * r, r, r, w + r, h + r, r), PALETTE['label_border'])
    return img


def desc_box(w, h, s):
    pad = int(20 * s)
    img = new(h + 2 * pad, w + 2 * pad)
    body = rect_mask(h + 2 * pad, w + 2 * pad, pad, pad, pad + w, pad + h)
    over(img, shadow(h + 2 * pad, w + 2 * pad, body, '#000000', 0.35, 18 * s, 0, 6 * s))
    col = np.broadcast_to(rgb(PALETTE['desc']), (h, w, 3)).copy()
    col = overlay(col, fractal(h, w, 33 * s, None, 3, seed=11), 0.15)
    layer = new(h, w)
    layer[..., :3], layer[..., 3] = col, 1
    over(img, layer, pad, pad)
    paint(img, ring_mask(h + 2 * pad, w + 2 * pad, pad, pad, pad + w, pad + h, max(1, round(s))),
          PALETTE['desc_border'])
    return img, pad


def glow_frames(w, h, s, n=8):
    """The selection cursor around a w x h box: a 3 px white frame 6 px outside it with a soft blue glow inside and
    out, n frames of one breath. Returns (frames, margin) where the box sits at (margin, margin)."""
    gap, thick, spread = 6 * s, 3 * s, 16 * s
    mg = int(gap + spread)
    H, W = h + 2 * mg, w + 2 * mg
    ring = ring_mask(H, W, mg - gap, mg - gap, mg + w + gap, mg + h + gap, thick)
    soft_out = box_blur(ring, 6 * s)
    soft_far = box_blur(ring, 12 * s)
    frames = []
    for k in range(n):
        t = 0.5 - 0.5 * np.cos(2 * np.pi * k / n)
        img = new(H, W)
        paint(img, np.clip(soft_far * 2.2, 0, 1), '#96b4e6', 0.35 + 0.25 * t)
        paint(img, np.clip(soft_out * 2.8, 0, 1), '#afc5e8', 0.7 + 0.3 * t)
        paint(img, ring, PALETTE['glow'])
        frames.append(img)
    return frames, mg


def filigree(w, s):
    """The divider: a thin line with a dot at each end and the ornament in the middle; (image, height)."""
    h = int(round(26 * s))
    img = new(h, w)
    mid = h / 2
    orn_w = 78 * s
    scale = orn_w / 108.0
    ox = (w - orn_w) / 2
    dot_r = 1.5 * s
    gap = 5 * s
    left_end, right_start = ox - gap, ox + orn_w + gap
    xs0, xs1 = 4 * s + 2 * dot_r + gap, w - 4 * s - 2 * dot_r - gap
    paint(img, rect_mask(h, w, xs0, mid - 0.5 * s, left_end, mid + 0.5 * s), PALETTE['line'])
    paint(img, rect_mask(h, w, right_start, mid - 0.5 * s, xs1, mid + 0.5 * s), PALETTE['line'])
    dots = stroke_mask(h, w, [[(4 * s + dot_r, mid)], [(w - 4 * s - dot_r, mid)]], 2 * dot_r)
    paint(img, dots, PALETTE['dot'])
    lines = []
    for d in FILIGREE_PATHS:
        for line in svg_polylines(d, scale, 6, 8):
            lines.append([(x + ox, y + mid - 13 * scale) for x, y in line])
    paint(img, stroke_mask(h, w, lines, 2.2 * scale), PALETTE['filigree'])
    return img, h


def triforce(w):
    """The Triforce, w pixels wide: three triangles, each shading from top to bottom, on a transparent ground."""
    h = int(round(w * 217 / 250))
    k = w / 250.0
    tris = [((125, 1), (186.2, 107), (63.8, 107)), ((62.2, 109), (123.4, 215), (1, 215)),
            ((187.8, 109), (249, 215), (126.6, 215))]
    img = new(h, w)
    top, bottom = rgb(PALETTE['tri_top']), rgb(PALETTE['tri_bottom'])
    for tri in tris:
        pts = [(x * k, y * k) for x, y in tri]
        mask = polygon_mask(h, w, pts)
        y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
        t = np.clip((np.arange(h, dtype=np.float32) + 0.5 - y0) / max(y1 - y0, 1e-6), 0, 1)[:, None, None]
        layer = new(h, w)
        layer[..., :3] = top + (bottom - top) * t
        layer[..., 3] = mask
        over(img, layer)
    return img


def triforce_ico():
    """The launcher's icon at the sizes Windows asks for."""
    imgs = []
    for n in (16, 24, 32, 48, 64, 128, 256):
        t = triforce(n)
        sq = new(n, n)
        over(sq, t, 0, (n - t.shape[0]) // 2)
        imgs.append(sq)
    return ico_bytes(imgs)


def check_mark(size, s, color=None):
    img = new(size, size)
    k = size / 24.0
    paint(img, stroke_mask(size, size, [[(5 * k, 12.5 * k), (9.2 * k, 16.7 * k), (19 * k, 7 * k)]], 3 * k),
          color or PALETTE['ok'])
    return img


def chevron(w, h, color, left=False):
    img = new(h, w)
    pts = [(w - 0.5, 0), (0.5, h / 2), (w - 0.5, h)] if left else [(0.5, 0), (w - 0.5, h / 2), (0.5, h)]
    paint(img, polygon_mask(h, w, pts), color)
    return img


def framed_icon(size, s, kind):
    """The square-framed icon of a big row (play or refresh), like the icon frames of the game's menu rows."""
    img = new(size, size)
    t = 1.5 * s
    paint(img, ring_mask(size, size, 0.5, 0.5, size - 0.5, size - 0.5, t), '#e9dcc0')
    c = size / 2
    if kind == 'play':
        k = size / 32
        paint(img, polygon_mask(size, size, [(c - 5 * k, c - 7 * k), (c + 7 * k, c), (c - 5 * k, c + 7 * k)]),
              '#efe4ca')
    elif kind == 'refresh':
        import math
        k = size / 32
        r = 7.5 * k
        arcs = []
        for a0, a1 in ((200, 340), (20, 160)):
            arcs.append([(c + r * math.cos(math.radians(a)), c + r * math.sin(math.radians(a)))
                         for a in range(a0, a1 + 1, 6)])
        heads = [[(c + r * math.cos(math.radians(340)) - 3.5 * k, c + r * math.sin(math.radians(340)) - 1 * k),
                  (c + r * math.cos(math.radians(340)), c + r * math.sin(math.radians(340))),
                  (c + r * math.cos(math.radians(340)) + 0.5 * k, c + r * math.sin(math.radians(340)) - 4 * k)],
                 [(c + r * math.cos(math.radians(160)) + 3.5 * k, c + r * math.sin(math.radians(160)) + 1 * k),
                  (c + r * math.cos(math.radians(160)), c + r * math.sin(math.radians(160))),
                  (c + r * math.cos(math.radians(160)) - 0.5 * k, c + r * math.sin(math.radians(160)) + 4 * k)]]
        paint(img, stroke_mask(size, size, arcs + heads, 2.2 * k), '#efe4ca')
    return img


def badge(size, s):
    """The orange "!" disc beside Updates when a new version is ready (the "!" itself is text)."""
    img = new(size + 4, size + 4)
    ring = stroke_mask(size + 4, size + 4, [[((size + 4) / 2, (size + 4) / 2)]], size + 1.5 * s)
    over(img, shadow(size + 4, size + 4, ring, '#fbbd45', 0.6, 6 * s))
    paint(img, ring, '#3a2a14')
    disc = stroke_mask(size + 4, size + 4, [[((size + 4) / 2, (size + 4) / 2)]], size)
    layer = new(size + 4, size + 4)
    layer[..., :3] = vgradient(size + 4, size + 4, [(0, '#fbbd45'), (1, '#e2942a')])
    layer[..., 3] = disc
    over(img, layer)
    return img
