"""Write a colour grading LUT for the ToneMap post effect.

Layout: a 256x16 RGBA8 strip of sixteen 16x16 slices; red increases across a
slice, green down, blue selects the slice. Values are display (sRGB) space.

Usage: python make_lut.py <out.dds> [--grade neutral|toy] [--png <preview>]
"""
import argparse
import struct

import numpy as np

N = 16


def grade_toy(rgb):
    # Gentle "glossy toy" grade: warm lift in the shadows, a touch cooler
    # highlights, +12% saturation and a soft S-curve.
    luma = (rgb @ np.array([0.2126, 0.7152, 0.0722]))[..., None]
    rgb = luma + (rgb - luma) * 1.12
    shadows = (1.0 - luma) ** 2
    highlights = luma ** 2
    rgb = rgb + shadows * np.array([0.018, 0.008, -0.004]) + highlights * np.array([-0.01, 0.0, 0.012])
    rgb = np.clip(rgb, 0.0, 1.0)
    s = rgb * rgb * (3.0 - 2.0 * rgb)
    return np.clip(rgb + (s - rgb) * 0.25, 0.0, 1.0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--grade', default='toy')
    ap.add_argument('--png')
    args = ap.parse_args()

    c = np.arange(N) / (N - 1.0)
    b, g, r = np.meshgrid(c, c, c, indexing='ij')   # [slice][row][col]
    rgb = np.stack([r, g, b], -1)
    if args.grade == 'toy':
        rgb = grade_toy(rgb)
    strip = np.concatenate([rgb[i] for i in range(N)], 1)   # 16 x 256
    rgba = np.concatenate([strip, np.ones(strip.shape[:-1] + (1,))], -1)
    data = np.round(rgba * 255.0).astype(np.uint8)

    w, h = N * N, N
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x8
    header = struct.pack('<4sIIIIIII44x', b'DDS ', 124, flags, h, w, w * 4, 0, 1)
    pixfmt = struct.pack('<II4sIIIII', 32, 0x4, b'DX10', 0, 0, 0, 0, 0)
    caps = struct.pack('<IIIII', 0x1000, 0, 0, 0, 0)
    dx10 = struct.pack('<IIIII', 28, 3, 0, 1, 0)   # R8G8B8A8_UNORM, TEXTURE2D
    with open(args.out, 'wb') as f:
        f.write(header + pixfmt + caps + dx10 + data.tobytes())
    if args.png:
        from PIL import Image
        Image.fromarray(data).save(args.png)


if __name__ == '__main__':
    main()
