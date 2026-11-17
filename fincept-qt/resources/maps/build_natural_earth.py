#!/usr/bin/env python3
"""Build resources/maps/natural_earth.bin — the offline base map WorldMapWidget draws.

Input: Natural Earth GeoJSON (public domain, https://www.naturalearthdata.com — the
GeoJSON copies in github.com/nvkelso/natural-earth-vector/geojson). Run:

    python -I build_natural_earth.py <dir with the .geojson files> natural_earth.bin

Layers (lod 0 = drawn below zoom 3, 1 = from zoom 3, 2 = always):
    land / lakes            polygons   110m (lod 0) and 50m (lod 1)
    borders                 lines      110m (lod 0) and 50m (lod 1) admin-0 land boundaries
    country                 labels     ne_110m_admin_0_countries LABEL_X/LABEL_Y, MIN_LABEL, LABELRANK
    marine                  labels     ne_110m_geography_marine_polys (label point = pole of inaccessibility)
    ports                   labels     ne_10m_ports (min zoom from scalerank)

File (little-endian), zlib-compressed with Qt's qCompress framing (4-byte big-endian size + zlib):
    b"FNEM" u16 version=1 u16 layer_count
    layer: u8 kind (1 polygon, 2 line, 3 label) u8 lod char[8] id u32 feature_count
      kind 1/2 feature: u32 part_count, per part: u32 n, n x (i32 lon*1e5, i32 lat*1e5), each pair after the
                        first stored as the delta to the previous one
      kind 3 feature:   i32 lon*1e5 i32 lat*1e5 u8 min_zoom*10 u8 rank u16 n utf-8 name[n]
"""
import json
import math
import struct
import sys
import zlib

Q = 1e5


def load(d, name):
    with open("%s/%s.geojson" % (d, name), encoding="utf-8") as f:
        return json.load(f)["features"]


def parts_of(geom):
    t, c = geom["type"], geom["coordinates"]
    if t == "Polygon":
        return c
    if t == "MultiPolygon":
        return [ring for poly in c for ring in poly]
    if t == "LineString":
        return [c]
    if t == "MultiLineString":
        return c
    raise ValueError(t)


def pack_parts(parts):
    out = [struct.pack("<I", len(parts))]
    for part in parts:
        pts = [(int(round(x * Q)), int(round(y * Q))) for x, y in (p[:2] for p in part)]
        # drop consecutive duplicates (quantisation) but keep at least two points
        ded = [pts[0]] + [p for i, p in enumerate(pts[1:], 1) if p != pts[i - 1]]
        if len(ded) < 2:
            ded = pts[:2]
        out.append(struct.pack("<I", len(ded)))
        px, py = 0, 0
        buf = []
        for x, y in ded:
            buf.append(struct.pack("<ii", x - px, y - py))
            px, py = x, y
        out.append(b"".join(buf))
    return b"".join(out)


def geom_layer(kind, lod, lid, feats):
    body = [struct.pack("<BB8sI", kind, lod, lid.encode("ascii"), len(feats))]
    for f in feats:
        body.append(pack_parts(parts_of(f["geometry"])))
    return b"".join(body)


def label_layer(lod, lid, rows):
    body = [struct.pack("<BB8sI", 3, lod, lid.encode("ascii"), len(rows))]
    for lon, lat, minz, rank, name in rows:
        raw = name.encode("utf-8")
        body.append(struct.pack("<iiBBH", int(round(lon * Q)), int(round(lat * Q)),
                                max(0, min(255, int(round(minz * 10)))), max(0, min(255, int(rank))), len(raw)))
        body.append(raw)
    return b"".join(body)


def point_in_ring(x, y, ring):
    inside = False
    n = len(ring)
    j = n - 1
    for i in range(n):
        xi, yi = ring[i][0], ring[i][1]
        xj, yj = ring[j][0], ring[j][1]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-300) + xi:
            inside = not inside
        j = i
    return inside


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = 0.0 if dx == 0 and dy == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def label_point(geom):
    """Coarse pole of inaccessibility of the largest polygon (grid search)."""
    polys = geom["coordinates"] if geom["type"] == "MultiPolygon" else [geom["coordinates"]]

    def area(r):
        return abs(sum(r[i][0] * r[i - 1][1] - r[i - 1][0] * r[i][1] for i in range(len(r)))) / 2

    poly = max(polys, key=lambda p: area(p[0]))
    outer = poly[0]
    xs = [p[0] for p in outer]
    ys = [p[1] for p in outer]
    best, best_d = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2), -1.0
    steps = 40
    for i in range(steps + 1):
        for j in range(steps + 1):
            x = min(xs) + (max(xs) - min(xs)) * i / steps
            y = min(ys) + (max(ys) - min(ys)) * j / steps
            if not point_in_ring(x, y, outer) or any(point_in_ring(x, y, h) for h in poly[1:]):
                continue
            d = min(seg_dist(x, y, *outer[k - 1][:2], *outer[k][:2]) for k in range(1, len(outer), 3))
            if d > best_d:
                best, best_d = (x, y), d
    return best


def main(src, dst):
    layers = [
        geom_layer(1, 0, "land", load(src, "ne_110m_land")),
        geom_layer(1, 1, "land", load(src, "ne_50m_land")),
        geom_layer(1, 0, "lakes", load(src, "ne_110m_lakes")),
        geom_layer(1, 1, "lakes", [f for f in load(src, "ne_50m_lakes") if f["properties"].get("scalerank", 9) <= 5]),
        geom_layer(2, 0, "borders", load(src, "ne_110m_admin_0_boundary_lines_land")),
        geom_layer(2, 1, "borders", load(src, "ne_50m_admin_0_boundary_lines_land")),
    ]
    countries = []
    for f in load(src, "ne_110m_admin_0_countries"):
        p = f["properties"]
        countries.append((p["LABEL_X"], p["LABEL_Y"], float(p["MIN_LABEL"]), p["LABELRANK"], p["NAME"]))
    layers.append(label_layer(2, "country", countries))
    marine = []
    for f in load(src, "ne_110m_geography_marine_polys"):
        p = f["properties"]
        x, y = label_point(f["geometry"])
        name = p.get("label") or p["name"]
        marine.append((x, y, float(p["min_label"]), p["scalerank"], name.title() if name.isupper() else name))
    layers.append(label_layer(2, "marine", marine))
    ports = []
    for f in load(src, "ne_10m_ports"):
        p = f["properties"]
        x, y = f["geometry"]["coordinates"][:2]
        ports.append((x, y, max(2.0, p["scalerank"] - 1.0), p["scalerank"], p["name"]))
    layers.append(label_layer(2, "ports", ports))

    raw = b"FNEM" + struct.pack("<HH", 1, len(layers)) + b"".join(layers)
    blob = struct.pack(">I", len(raw)) + zlib.compress(raw, 9)
    with open(dst, "wb") as f:
        f.write(blob)
    print("%s: %d layers, %d bytes raw, %d bytes compressed" % (dst, len(layers), len(raw), len(blob)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
