# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Anthony Charretier
#
# Writes assets/images/grm-equations.svg: the chain of seven relations through which a GRM network of known
# physics brought up S * E_quant = E_stock (see grm-core's README), typeset (Computer Modern) and flattened to
# plain outlines: one path per equation, in absolute coordinates, nothing to resolve (no font, no <use>).
# The plugin shows it, discreetly, on the side of its dialogs.
#
# Usage: python tools/make_equations_svg.py   (needs matplotlib, only to write the file)
import pathlib

import matplotlib
from matplotlib.path import Path
from matplotlib.textpath import TextPath

EQUATIONS = [
    r"$l_P = \sqrt{\frac{\hbar G}{c^3}}$",
    r"$H_{end} = H_0 \sqrt{\Omega_\Lambda}$",
    r"$\rho_\Lambda = \frac{3 c^2 H_0^2 \Omega_\Lambda}{8 \pi G}$",
    r"$E_{stock} = \frac{4\pi}{3} \rho_\Lambda \left(\frac{c}{H_{end}}\right)^3$",
    r"$T_{floor} = \frac{\hbar H_{end}}{2 \pi k_B}$",
    r"$E_{quant} = k_B T_{floor}$",
    r"$S = \frac{\pi c^2}{H_{end}^2 l_P^2}$",
    r"$S \cdot E_{quant} = E_{stock}$",
]
SIZE = 13.0  # points per em
GAP = 14.0   # between two equations


def svg_path(path, dx, dy):
    """The path's outline as SVG data, y pointing down, moved by (dx, dy)."""
    out = []
    for vertices, code in path.iter_segments(curves=True, simplify=False):
        pts = [(vertices[k] + dx, dy - vertices[k + 1]) for k in range(0, len(vertices), 2)]
        letter = {Path.MOVETO: "M", Path.LINETO: "L", Path.CURVE3: "Q", Path.CURVE4: "C", Path.CLOSEPOLY: "Z"}[code]
        out.append(letter + ("" if code == Path.CLOSEPOLY else " ".join(f"{x:.2f} {y:.2f}" for x, y in pts)))
    return " ".join(out)


matplotlib.rcParams["mathtext.fontset"] = "cm"
paths = [TextPath((0, 0), equation, size=SIZE) for equation in EQUATIONS]
extents = [p.get_extents() for p in paths]
width = max(e.width for e in extents)
pieces, y = [], 0.0
for path, e in zip(paths, extents):
    y += e.y1  # the top of this equation
    pieces.append(f'  <path d="{svg_path(path, (width - e.width) / 2 - e.x0, y)}"/>')
    y += -e.y0 + GAP
height = y - GAP

svg = (
    '<?xml version="1.0" encoding="utf-8"?>\n'
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.2f}" height="{height:.2f}" viewBox="0 0 {width:.2f} {height:.2f}" fill="#000000">\n'
    + "\n".join(pieces)
    + "\n</svg>\n"
)
out = pathlib.Path(__file__).resolve().parent.parent / "assets" / "images" / "grm-equations.svg"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(svg, encoding="utf-8")
print(f"wrote {out} ({width:.0f} x {height:.0f})")
