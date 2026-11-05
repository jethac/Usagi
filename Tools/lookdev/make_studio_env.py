"""Generate the default image based lighting environment for the deferred renderer.

Writes two RGBA16F DDS cube maps (D3D/Vulkan face order and orientation):
  <out>/env_studio_radiance.dds    128^2, 8 mips, GGX prefiltered (mip m has
                                   perceptual roughness m / 5; mips 6-7 are
                                   box filtered from mip 5)
  <out>/env_studio_irradiance.dds  32^2, cosine convolved radiance / pi,
                                   so diffuse = albedo * texture(N)

The environment is an analytic "workshop" studio: warm walls, two overhead
strip lights, a large cool window on the sun side and a warm lamp, chosen so
glossy toy cars seen from above pick up readable highlights.

Usage: python make_studio_env.py <output dir> [--preview <png>]
Needs numpy (and Pillow for --preview).
"""
import argparse
import math
import os
import struct

import numpy as np

RADIANCE_SIZE = 128
RADIANCE_MIPS = 8          # 128 .. 1
ROUGH_MIPS = 6             # mips 0..5 map roughness 0..1
IRRADIANCE_SIZE = 32
SOURCE_SIZE = 256
SAMPLES = 192

# Sun direction used by UsagiGP's Root.yml (direction the light travels)
SUN_TRAVEL = np.array([-0.35, -0.85, 0.4])


def normalize(v):
    return v / np.linalg.norm(v, axis=-1, keepdims=True)


def face_dirs(size):
    """Per-texel directions for the 6 cube faces, shape (6, size, size, 3)."""
    c = (np.arange(size) + 0.5) / size * 2.0 - 1.0
    u, v = np.meshgrid(c, c)   # u across, v down
    one = np.ones_like(u)
    faces = [
        np.stack([one, -v, -u], -1),    # +X
        np.stack([-one, -v, u], -1),    # -X
        np.stack([u, one, v], -1),      # +Y
        np.stack([u, -one, -v], -1),    # -Y
        np.stack([u, -v, one], -1),     # +Z
        np.stack([-u, -v, -one], -1),   # -Z
    ]
    return normalize(np.stack(faces, 0))


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def studio(d):
    """Analytic HDR radiance for unit directions d (..., 3)."""
    x, y, z = d[..., 0], d[..., 1], d[..., 2]
    wall = np.array([0.42, 0.33, 0.25]) * 0.55
    ceiling = np.array([0.55, 0.52, 0.48]) * 0.6
    floor = np.array([0.22, 0.16, 0.11]) * 0.45
    up = smooth(0.0, 0.6, y)[..., None]
    down = smooth(0.0, -0.5, y)[..., None]
    col = wall * (1.0 - up - down) + ceiling * up + floor * down

    # Low frequency variation so walls aren't flat (shelves, clutter)
    az = np.arctan2(x, z)
    var = 1.0 + 0.18 * np.sin(az * 5.0 + 1.3) * np.cos(y * 7.0) + 0.1 * np.sin(az * 11.0 - y * 4.0)
    col = col * var[..., None]

    # Two overhead strip lights running along Z
    with np.errstate(divide='ignore', invalid='ignore'):
        safe_y = np.maximum(y, 1e-3)
        px, pz = x / safe_y, z / safe_y
    for sx in (-0.5, 0.5):
        strip = smooth(0.09, 0.06, np.abs(px - sx)) * smooth(1.3, 1.1, np.abs(pz)) * smooth(0.15, 0.3, y)
        col = col + strip[..., None] * np.array([1.0, 0.98, 0.93]) * 16.0

    # Window on the sun side: a large cool rectangle in the wall
    sun_to = -SUN_TRAVEL / np.linalg.norm(SUN_TRAVEL)
    sun_az = math.atan2(sun_to[0], sun_to[2])
    daz = np.angle(np.exp(1j * (az - sun_az)))
    elev = np.arcsin(np.clip(y, -1.0, 1.0))
    window = smooth(0.62, 0.55, np.abs(daz)) * smooth(0.05, 0.12, elev) * smooth(0.95, 0.85, elev)
    # Mullions
    window = window * (0.75 + 0.25 * smooth(0.02, 0.05, np.abs(np.sin(daz * 6.0))))
    col = col + window[..., None] * np.array([0.85, 0.93, 1.05]) * 6.0

    # Sun disc seen through the window (keeps the specular peak consistent with the key light)
    sun_dot = np.sum(d * sun_to, -1)
    col = col + (smooth(0.9990, 0.9996, sun_dot))[..., None] * np.array([1.0, 0.96, 0.88]) * 60.0

    # Warm desk lamp opposite the window
    lamp_dir = normalize(np.array([-0.75, 0.35, 0.55]))
    lamp = smooth(0.985, 0.995, np.sum(d * lamp_dir, -1))
    col = col + lamp[..., None] * np.array([1.0, 0.72, 0.42]) * 10.0
    return col


def downsample(cube):
    f, h, w, c = cube.shape
    return cube.reshape(f, h // 2, 2, w // 2, 2, c).mean(axis=(2, 4))


def dir_to_face_uv(d):
    ax, ay, az = np.abs(d[..., 0]), np.abs(d[..., 1]), np.abs(d[..., 2])
    face = np.zeros(d.shape[:-1], dtype=np.int32)
    u = np.zeros(d.shape[:-1])
    v = np.zeros(d.shape[:-1])
    x, y, z = d[..., 0], d[..., 1], d[..., 2]
    mx = (ax >= ay) & (ax >= az)
    my = (~mx) & (ay >= az)
    mz = (~mx) & (~my)
    # Inverse of face_dirs
    m = mx & (x > 0); face[m] = 0; u[m] = -z[m] / ax[m]; v[m] = -y[m] / ax[m]
    m = mx & (x <= 0); face[m] = 1; u[m] = z[m] / ax[m]; v[m] = -y[m] / ax[m]
    m = my & (y > 0); face[m] = 2; u[m] = x[m] / ay[m]; v[m] = z[m] / ay[m]
    m = my & (y <= 0); face[m] = 3; u[m] = x[m] / ay[m]; v[m] = -z[m] / ay[m]
    m = mz & (z > 0); face[m] = 4; u[m] = x[m] / az[m]; v[m] = -y[m] / az[m]
    m = mz & (z <= 0); face[m] = 5; u[m] = -x[m] / az[m]; v[m] = -y[m] / az[m]
    return face, u, v


def sample_level(cube, face, u, v):
    size = cube.shape[1]
    fx = np.clip((u * 0.5 + 0.5) * size - 0.5, 0.0, size - 1.0)
    fy = np.clip((v * 0.5 + 0.5) * size - 0.5, 0.0, size - 1.0)
    x0 = np.floor(fx).astype(np.int32); y0 = np.floor(fy).astype(np.int32)
    x1 = np.minimum(x0 + 1, size - 1); y1 = np.minimum(y0 + 1, size - 1)
    tx = (fx - x0)[..., None]; ty = (fy - y0)[..., None]
    a = cube[face, y0, x0]; b = cube[face, y0, x1]
    c = cube[face, y1, x0]; d = cube[face, y1, x1]
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty


def sample_pyramid(pyramid, d, lod):
    face, u, v = dir_to_face_uv(d)
    lod = np.clip(lod, 0.0, len(pyramid) - 1.0)
    l0 = np.floor(lod).astype(np.int32)
    out = np.zeros(d.shape[:-1] + (3,))
    t = (lod - l0)[..., None]
    for level in np.unique(l0):
        m = l0 == level
        s0 = sample_level(pyramid[level], face[m], u[m], v[m])
        s1 = sample_level(pyramid[min(level + 1, len(pyramid) - 1)], face[m], u[m], v[m])
        out[m] = s0 * (1 - t[m]) + s1 * t[m]
    return out


def hammersley(n):
    i = np.arange(n, dtype=np.uint32)
    b = i.copy()
    b = ((b << 16) | (b >> 16)) & 0xFFFFFFFF
    b = ((b & 0x55555555) << 1) | ((b & 0xAAAAAAAA) >> 1)
    b = ((b & 0x33333333) << 2) | ((b & 0xCCCCCCCC) >> 2)
    b = ((b & 0x0F0F0F0F) << 4) | ((b & 0xF0F0F0F0) >> 4)
    b = ((b & 0x00FF00FF) << 8) | ((b & 0xFF00FF00) >> 8)
    return (i + 0.5) / n, b.astype(np.float64) / 4294967296.0


def prefilter(pyramid, size, roughness):
    n = face_dirs(size).reshape(-1, 3)
    a = max(roughness * roughness, 1e-4)
    e1, e2 = hammersley(SAMPLES)
    phi = 2.0 * math.pi * e1
    cos_t = np.sqrt((1.0 - e2) / (1.0 + (a * a - 1.0) * e2))
    sin_t = np.sqrt(1.0 - cos_t * cos_t)
    h_t = np.stack([sin_t * np.cos(phi), sin_t * np.sin(phi), cos_t], -1)  # tangent space

    up = np.where(np.abs(n[:, 2:3]) < 0.999, np.array([[0.0, 0.0, 1.0]]), np.array([[1.0, 0.0, 0.0]]))
    tx = normalize(np.cross(up, n))
    ty = np.cross(n, tx)

    src_size = pyramid[0].shape[1]
    omega_p = 4.0 * math.pi / (6.0 * src_size * src_size)
    acc = np.zeros((n.shape[0], 3))
    wsum = np.zeros((n.shape[0], 1))
    for s in range(SAMPLES):
        h = tx * h_t[s, 0] + ty * h_t[s, 1] + n * h_t[s, 2]
        noh = h_t[s, 2]
        l = 2.0 * np.sum(n * h, -1, keepdims=True) * h - n
        nol = np.sum(n * l, -1)
        valid = nol > 0
        if not np.any(valid):
            continue
        d_ggx = (a * a) / (math.pi * ((noh * noh) * (a * a - 1.0) + 1.0) ** 2)
        pdf = d_ggx / 4.0   # V = N
        omega_s = 1.0 / (SAMPLES * pdf + 1e-8)
        lod = np.full(nol.shape, max(0.5 * math.log2(omega_s / omega_p) + 1.0, 0.0))
        col = sample_pyramid(pyramid, l, lod)
        w = np.where(valid, nol, 0.0)[:, None]
        acc += col * w
        wsum += w
    return (acc / np.maximum(wsum, 1e-8)).reshape(6, size, size, 3)


def sh9_basis(d):
    x, y, z = d[..., 0], d[..., 1], d[..., 2]
    return np.stack([
        0.282095 * np.ones_like(x),
        0.488603 * y, 0.488603 * z, 0.488603 * x,
        1.092548 * x * y, 1.092548 * y * z, 0.315392 * (3 * z * z - 1),
        1.092548 * x * z, 0.546274 * (x * x - y * y)], -1)


def irradiance(source):
    size = source.shape[1]
    d = face_dirs(size)
    c = (np.arange(size) + 0.5) / size * 2.0 - 1.0
    u, v = np.meshgrid(c, c)
    texel = 4.0 / (size * size) / (1.0 + u * u + v * v) ** 1.5   # solid angle
    w = np.broadcast_to(texel, (6, size, size))[..., None]
    basis = sh9_basis(d)
    coeffs = np.einsum('fhwk,fhwc->kc', basis * w, source)
    band = np.array([math.pi, 2.0 * math.pi / 3.0, 2.0 * math.pi / 3.0, 2.0 * math.pi / 3.0,
                     math.pi / 4.0, math.pi / 4.0, math.pi / 4.0, math.pi / 4.0, math.pi / 4.0])
    out_d = face_dirs(IRRADIANCE_SIZE)
    e = np.einsum('fhwk,kc->fhwc', sh9_basis(out_d), coeffs * band[:, None])
    return np.maximum(e / math.pi, 0.0)


def write_dds_cube_rgba16f(path, mips):
    """mips: list of arrays (6, s, s, 3), largest first."""
    size = mips[0].shape[1]
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x8   # caps, height, width, pixelformat, mipcount, pitch
    pitch = size * 8
    header = struct.pack('<4sIIIIIII44x', b'DDS ', 124, flags, size, size, pitch, 0, len(mips))
    pixfmt = struct.pack('<II4sIIIII', 32, 0x4, b'DX10', 0, 0, 0, 0, 0)
    caps = struct.pack('<IIIII', 0x1000 | 0x8 | 0x400000, 0x200 | 0xFC00, 0, 0, 0)
    dx10 = struct.pack('<IIIII', 10, 3, 0x4, 1, 0)   # R16G16B16A16_FLOAT, TEXTURE2D, TEXTURECUBE, 1 array
    with open(path, 'wb') as f:
        f.write(header + pixfmt + caps + dx10)
        for face in range(6):
            for m in mips:
                rgba = np.concatenate([m[face], np.ones(m[face].shape[:-1] + (1,))], -1)
                f.write(rgba.astype(np.float16).tobytes())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--preview')
    args = ap.parse_args()

    hi = studio(face_dirs(SOURCE_SIZE * 2))
    pyramid = [downsample(hi)]
    while pyramid[-1].shape[1] > 1:
        pyramid.append(downsample(pyramid[-1]))

    mips = [pyramid[1]]   # 128, sharp
    for m in range(1, ROUGH_MIPS):
        size = RADIANCE_SIZE >> m
        mips.append(prefilter(pyramid, size, m / (ROUGH_MIPS - 1.0)))
    while len(mips) < RADIANCE_MIPS:
        mips.append(downsample(mips[-1]))

    irr = irradiance(pyramid[2])

    os.makedirs(args.out, exist_ok=True)
    write_dds_cube_rgba16f(os.path.join(args.out, 'env_studio_radiance.dds'), mips)
    write_dds_cube_rgba16f(os.path.join(args.out, 'env_studio_irradiance.dds'), [irr])
    print('mean irradiance', irr.mean(axis=(0, 1, 2)), 'max radiance', mips[0].max())

    if args.preview:
        from PIL import Image
        rows = []
        for m in mips[:ROUGH_MIPS]:
            s = m.shape[1]
            strip = np.concatenate([np.repeat(np.repeat(m[f], RADIANCE_SIZE // s, 0), RADIANCE_SIZE // s, 1) for f in range(6)], 1)
            rows.append(strip)
        irr_up = np.concatenate([np.repeat(np.repeat(irr[f], RADIANCE_SIZE // IRRADIANCE_SIZE, 0), RADIANCE_SIZE // IRRADIANCE_SIZE, 1) for f in range(6)], 1)
        rows.append(irr_up)
        img = np.concatenate(rows, 0)
        img = img / (1.0 + img)   # Reinhard for viewing
        Image.fromarray((np.clip(img, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)).save(args.preview)


if __name__ == '__main__':
    main()
