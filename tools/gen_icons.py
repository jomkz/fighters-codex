#!/usr/bin/env python3
"""Single source of truth for the fxs object-category icon set (#363).

One generator emits, from one vector description per icon:

  * theme-aware SVG sources   -> gui/assets/icons/<name>.svg
  * a baked, zero-runtime-dep C++ form fxs consumes at two sizes
    -> gui/src/assets/icons_baked.{h,cpp}

The baked form is 8-bit coverage (alpha) per pixel: the glyphs are monochrome,
so RGB is white by construction and only the coverage carries information. fxs
expands it to RGBA and tints it with the active theme's foreground at draw time
(ui/icons.cpp), so one baked artifact serves both light and dark — the SVG
sources carry the same light/dark recipe for docs and design review.

Rasterisation is pure stdlib (a nonzero-winding scanline fill supersampled 4x
for anti-aliasing) so the currency check runs on the docs-status job's bare
python3, exactly like tools/check_status.py.

Usage:
  tools/gen_icons.py               regenerate every committed artifact
  tools/gen_icons.py --check       fail if any committed artifact is stale
  tools/gen_icons.py --self-test   internal rasteriser sanity checks
"""
import argparse
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SVG_DIR = os.path.join(ROOT, "gui", "assets", "icons")
BAKED_H = os.path.join(ROOT, "gui", "src", "assets", "icons_baked.h")
BAKED_CPP = os.path.join(ROOT, "gui", "src", "assets", "icons_baked.cpp")

CANVAS = 48          # icon design grid (viewBox 0 0 48 48)
SIZES = (24, 48)     # baked pixel sizes: 1x nav bar + 2x hidpi
SUPERSAMPLE = 4      # AA: render NxSS then box-average

# Foreground colours reused from the docs diagrams' theme recipe
# (docs/fa/diagrams/*.svg: .lbl light/dark).
FG_LIGHT = "#22303c"
FG_DARK = "#dce4ea"


# ---------------------------------------------------------------------------
# Shape builders — each returns a subpath (list of (x, y)), wound consistently.
# Solid subpaths share a winding direction so overlaps union under the nonzero
# rule; hole() reverses a subpath so it subtracts.
# ---------------------------------------------------------------------------
def circle(cx, cy, r, seg=64):
    return [(cx + r * math.cos(2 * math.pi * i / seg),
             cy + r * math.sin(2 * math.pi * i / seg)) for i in range(seg)]


def rect(x, y, w, h):
    return [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]


def rrect(x, y, w, h, r, seg=8):
    r = min(r, w / 2, h / 2)
    # corner centres, each swept as a quarter arc (clockwise around the rect)
    corners = [(x + w - r, y + r, -math.pi / 2, 0.0),
               (x + w - r, y + h - r, 0.0, math.pi / 2),
               (x + r, y + h - r, math.pi / 2, math.pi),
               (x + r, y + r, math.pi, 3 * math.pi / 2)]
    pts = []
    for cx, cy, a0, a1 in corners:
        for i in range(seg + 1):
            a = a0 + (a1 - a0) * i / seg
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def poly(*pts):
    return list(pts)


def hole(subpath):
    return list(reversed(subpath))


# ---------------------------------------------------------------------------
# Icon definitions. Order fixes the C++ enum. Each icon is a list of subpaths;
# holes are reversed so they cut through the nonzero fill.
# ---------------------------------------------------------------------------
ICONS = [
    ("Aircraft", "aircraft", "Aircraft category", [
        poly((24, 3), (27, 16), (27, 22), (46, 31), (46, 34), (27, 28),
             (27, 37), (32, 43), (32, 45), (24, 42), (16, 45), (16, 43),
             (21, 37), (21, 28), (2, 34), (2, 31), (21, 22), (21, 16)),
    ]),
    ("Vehicles", "vehicles", "Vehicles category", [
        rrect(4, 33, 40, 8, 3),          # track base
        rect(9, 26, 30, 8),              # hull
        rect(17, 20, 13, 7),             # turret
        rect(29, 22, 15, 3),             # barrel
    ]),
    ("Weapons", "weapons", "Weapons category", [
        poly((24, 3), (29, 13), (19, 13)),          # nose
        rect(19, 13, 10, 22),                        # body
        poly((19, 29), (12, 41), (19, 38)),          # left fin
        poly((29, 29), (36, 41), (29, 38)),          # right fin
        poly((19, 35), (29, 35), (26, 44), (22, 44)),  # tail
    ]),
    ("Missions", "missions", "Missions category", [
        rect(13, 5, 3, 40),              # pole
        poly((16, 6), (38, 9), (38, 21), (16, 18)),    # flag
        rrect(8, 42, 16, 3, 1),          # base
    ]),
    ("Campaigns", "campaigns", "Campaigns category", [
        circle(24, 18, 9),                             # medal disc
        hole(circle(24, 18, 4)),                       # medal centre
        poly((18, 25), (14, 45), (21, 39)),            # left ribbon
        poly((30, 25), (34, 45), (27, 39)),            # right ribbon
    ]),
    ("Terrain", "terrain", "Terrain category", [
        circle(36, 13, 5),               # sun
        poly((5, 41), (19, 16), (33, 41)),   # far peak
        poly((23, 41), (33, 23), (44, 41)),  # near peak
    ]),
    ("Audio", "audio", "Audio category", [
        circle(18, 36, 7),               # note head
        rect(24, 11, 3, 25),             # stem
        poly((27, 11), (36, 16), (36, 25), (27, 20)),  # flag
    ]),
    ("ArtUI", "art-ui", "Art and UI category", [
        rrect(5, 11, 38, 26, 3),         # frame
        hole(rrect(10, 15, 28, 18, 2)),  # mat (cut)
        circle(16, 21, 3),               # sun (inside the mat)
        poly((11, 33), (22, 21), (31, 33)),   # back hill
        poly((26, 33), (33, 25), (39, 33)),   # front hill
    ]),
    ("Archives", "archives", "Archives (raw LIB) category", [
        rect(6, 12, 36, 8),              # lid
        rect(9, 20, 30, 20),             # body
        hole(rect(20, 25, 8, 3)),        # handle slot
    ]),
]


# ---------------------------------------------------------------------------
# Rasteriser — nonzero-winding scanline fill, supersampled for anti-aliasing.
# ---------------------------------------------------------------------------
def rasterize(subpaths, size, ss=SUPERSAMPLE):
    n = size * ss
    scale = n / CANVAS
    edges = []  # (ytop, ybot, x_at_ytop, dxdy, winding)
    for sp in subpaths:
        m = len(sp)
        for i in range(m):
            x0, y0 = sp[i]
            x1, y1 = sp[(i + 1) % m]
            x0 *= scale; y0 *= scale; x1 *= scale; y1 *= scale
            if y0 == y1:
                continue
            w = 1 if y1 > y0 else -1
            if y0 > y1:
                x0, y0, x1, y1 = x1, y1, x0, y0
            edges.append((y0, y1, x0, (x1 - x0) / (y1 - y0), w))

    cov = bytearray(n * n)  # 1 where inside (super-res)
    for y in range(n):
        yc = y + 0.5
        xs = []
        for ytop, ybot, xtop, dxdy, w in edges:
            if ytop <= yc < ybot:
                xs.append((xtop + (yc - ytop) * dxdy, w))
        if not xs:
            continue
        xs.sort()
        wind = 0
        row = y * n
        for k in range(len(xs) - 1):
            wind += xs[k][1]
            if wind != 0:
                a = max(0, int(math.ceil(xs[k][0] - 0.5)))
                b = min(n, int(math.ceil(xs[k + 1][0] - 0.5)))
                if b > a:
                    cov[row + a:row + b] = b"\x01" * (b - a)

    # box-downsample coverage -> 8-bit alpha
    out = bytearray(size * size)
    area = ss * ss
    for oy in range(size):
        for ox in range(size):
            acc = 0
            for sy in range(ss):
                base = (oy * ss + sy) * n + ox * ss
                acc += sum(cov[base:base + ss])
            out[oy * size + ox] = (acc * 255 + area // 2) // area
    return out


# ---------------------------------------------------------------------------
# SVG emission — theme-aware, one <path> using the nonzero (default) fill rule.
# ---------------------------------------------------------------------------
def _fmt(v):
    return ("%.2f" % v).rstrip("0").rstrip(".")


def svg_for(subpaths, aria):
    d = []
    for sp in subpaths:
        d.append("M" + " ".join("%s %s" % (_fmt(x), _fmt(y)) for x, y in sp) + "Z")
    path = "".join(d)
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" '
        'viewBox="0 0 48 48" role="img" aria-label="%s icon">\n'
        '  <style>\n'
        '    :root { color-scheme: light dark; }\n'
        '    .ico { fill: %s; }\n'
        '    @media (prefers-color-scheme: dark) { .ico { fill: %s; } }\n'
        '    :root[data-theme="dark"] .ico { fill: %s; }\n'
        '    :root[data-theme="light"] .ico { fill: %s; }\n'
        '  </style>\n'
        '  <path class="ico" d="%s"/>\n'
        '</svg>\n'
    ) % (aria, FG_LIGHT, FG_DARK, FG_DARK, FG_LIGHT, path)


# ---------------------------------------------------------------------------
# Baked C++ emission.
# ---------------------------------------------------------------------------
BANNER = "// Generated by tools/gen_icons.py. Do not edit.\n"


def baked_header():
    ids = ", ".join(name for name, _, _, _ in ICONS)
    return (
        BANNER +
        "#pragma once\n"
        "#include <cstdint>\n\n"
        "namespace fxs::icons {\n\n"
        "enum class Id { %s, Count };\n"
        "inline constexpr int Count = static_cast<int>(Id::Count);\n\n"
        "// One baked size variant: `size`x`size` 8-bit coverage. The glyph is\n"
        "// monochrome, so RGB is white by construction and only `a8` (alpha)\n"
        "// is stored; expand with ToRgba() before uploading, then tint per theme.\n"
        "struct Coverage { int size; const uint8_t* a8; };\n\n"
        "// Baked variant nearest the requested pixel size (24 or 48).\n"
        "const Coverage& Get(Id id, int px);\n"
        "// Stable lower-case identifier, matching the SVG source basename.\n"
        "const char* Name(Id id);\n"
        "// Expand coverage to RGBA8 (white, coverage in alpha) into `dst`,\n"
        "// which must hold cov.size*cov.size*4 bytes.\n"
        "void ToRgba(const Coverage& cov, uint8_t* dst);\n\n"
        "} // namespace fxs::icons\n"
    ) % ids


def _bytes_array(cname, data):
    lines = ["static const uint8_t %s[] = {" % cname]
    for i in range(0, len(data), 20):
        lines.append("    " + ",".join(str(b) for b in data[i:i + 20]) + ",")
    lines.append("};")
    return "\n".join(lines)


def baked_cpp():
    out = [BANNER, '#include "assets/icons_baked.h"\n', "namespace fxs::icons {\n"]
    for name, slug, _, subpaths in ICONS:
        for size in SIZES:
            data = rasterize(subpaths, size)
            out.append(_bytes_array("%s_%d" % (name, size), data))
    out.append("")
    for name, slug, _, _ in ICONS:
        variants = ", ".join("{%d, %s_%d}" % (s, name, s) for s in SIZES)
        out.append("static const Coverage k%s[] = { %s };" % (name, variants))
    out.append("")
    table = ", ".join("k%s" % name for name, _, _, _ in ICONS)
    out.append("static const Coverage* const kTable[] = { %s };" % table)
    names = ", ".join('"%s"' % slug for _, slug, _, _ in ICONS)
    out.append('static const char* const kNames[] = { %s };\n' % names)
    out.append(
        "const Coverage& Get(Id id, int px) {\n"
        "    const Coverage* v = kTable[static_cast<int>(id)];\n"
        "    return px <= %d ? v[0] : v[1];\n"
        "}\n\n"
        "const char* Name(Id id) { return kNames[static_cast<int>(id)]; }\n\n"
        "void ToRgba(const Coverage& cov, uint8_t* dst) {\n"
        "    const int n = cov.size * cov.size;\n"
        "    for (int i = 0; i < n; ++i) {\n"
        "        dst[i * 4 + 0] = 255; dst[i * 4 + 1] = 255;\n"
        "        dst[i * 4 + 2] = 255; dst[i * 4 + 3] = cov.a8[i];\n"
        "    }\n"
        "}\n" % SIZES[0]
    )
    out.append("} // namespace fxs::icons\n")
    return "\n".join(out)


# ---------------------------------------------------------------------------
# Artifact assembly / IO.
# ---------------------------------------------------------------------------
def artifacts():
    """Return {abs_path: content_str} for every committed artifact."""
    out = {}
    for name, slug, aria, subpaths in ICONS:
        out[os.path.join(SVG_DIR, slug + ".svg")] = svg_for(subpaths, aria)
    out[BAKED_H] = baked_header()
    out[BAKED_CPP] = baked_cpp()
    return out


def write_all():
    os.makedirs(SVG_DIR, exist_ok=True)
    os.makedirs(os.path.dirname(BAKED_H), exist_ok=True)
    for path, content in artifacts().items():
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)
    print("gen_icons: wrote %d artifacts" % len(artifacts()))
    return 0


def check_all():
    stale = []
    for path, content in artifacts().items():
        rel = os.path.relpath(path, ROOT)
        try:
            with open(path, "r", encoding="utf-8") as f:
                if f.read() != content:
                    stale.append(rel)
        except FileNotFoundError:
            stale.append(rel + " (missing)")
    if stale:
        sys.stderr.write(
            "gen_icons: committed icon artifacts are stale:\n  " +
            "\n  ".join(stale) +
            "\nRun 'python3 tools/gen_icons.py' and commit.\n")
        return 1
    print("gen_icons: %d artifacts up to date" % len(artifacts()))
    return 0


def self_test():
    # A full-canvas square is fully covered; an empty icon is fully clear.
    full = rasterize([rect(0, 0, CANVAS, CANVAS)], 24)
    assert min(full) == 255, "full square should be opaque everywhere"
    assert rasterize([], 24) == bytearray(24 * 24), "empty should be clear"
    # A hole punches transparency through a solid.
    ring = rasterize([rect(4, 4, 40, 40), hole(rect(16, 16, 16, 16))], 48)
    assert ring[0] == 0, "outside the square is clear"
    assert ring[24 * 48 + 24] == 0, "centre of the hole is clear"
    assert ring[8 * 48 + 24] == 255, "the border between them is solid"
    # Every real icon produces coverage at both sizes.
    for name, _, _, sp in ICONS:
        for size in SIZES:
            assert sum(rasterize(sp, size)) > 0, "%s/%d empty" % (name, size)
    print("gen_icons: self-test passed (%d icons)" % len(ICONS))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--check", action="store_true",
                   help="fail if any committed artifact is stale")
    g.add_argument("--self-test", action="store_true",
                   help="internal rasteriser sanity checks")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    if args.check:
        return check_all()
    return write_all()


if __name__ == "__main__":
    sys.exit(main())
