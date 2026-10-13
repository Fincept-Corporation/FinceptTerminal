"""Shared SDMX-JSON helpers for the SDMX data connectors (abs, bis_stats, oecd_*, unicef, imf ...).

Not a connector itself. `flatten(msg)` turns an SDMX-JSON data message (1.0 or 2.0, series or
AllDimensions layout) into plain series:

    [{"key": "M1.GPM.20.AUS.Q",
      "dimensions": {"MEASURE": {"id": "M1", "name": "Chain volume measures"}, ...},
      "observations": [{"period": "2024-Q1", "value": 123.4}, ...]}, ...]
"""
from typing import Any, Dict, List, Optional


def _name(v: Dict) -> str:
    n = v.get("name")
    if isinstance(n, dict):
        n = n.get("en") or next(iter(n.values()), "")
    if not n and isinstance(v.get("names"), dict):
        n = v["names"].get("en") or next(iter(v["names"].values()), "")
    return n or v.get("id", v.get("value", ""))


def _num(v: Any) -> Any:
    if v is None:
        return None
    try:
        f = float(v)
    except (TypeError, ValueError):
        return v
    return int(f) if f.is_integer() and abs(f) < 1e15 and not isinstance(v, float) else f


def _parts(msg: Dict):
    body = msg.get("data", msg) if isinstance(msg.get("data"), dict) else msg
    datasets = body.get("dataSets") or []
    structure = body.get("structure")
    if structure is None:
        structs = body.get("structures") or []
        structure = structs[0] if structs else {}
    return datasets, structure.get("dimensions", {}) or {}


def flatten(msg: Dict, max_series: Optional[int] = None) -> List[Dict]:
    """Flatten an SDMX-JSON data message into a list of series (see module docstring)."""
    if not isinstance(msg, dict):
        return []
    datasets, dims = _parts(msg)
    if not datasets:
        return []
    ds = datasets[0]
    sdims = dims.get("series", []) or []
    odims = dims.get("observation", []) or []

    def decode(dim_list, key_str):
        out = {}
        idx = key_str.split(":") if key_str != "" else []
        for d, i in zip(dim_list, idx):
            vals = d.get("values", [])
            try:
                v = vals[int(i)]
            except (ValueError, IndexError):
                v = {"id": i}
            vid = v.get("id", v.get("value", ""))
            out[d.get("id", "")] = {"id": vid, "name": _name(v) or vid}
        return out

    result: List[Dict] = []
    if "series" in ds:
        time_dim = odims[0] if odims else {"values": []}
        tvals = time_dim.get("values", [])
        for skey, s in ds["series"].items():
            dmap = decode(sdims, skey)
            obs = []
            for okey, ov in (s.get("observations") or {}).items():
                try:
                    tv = tvals[int(okey.split(":")[0])]
                    period = tv.get("id", tv.get("value"))
                except (ValueError, IndexError):
                    period = okey
                obs.append({"period": period, "value": _num(ov[0] if isinstance(ov, list) and ov else ov)})
            obs.sort(key=lambda o: str(o["period"]))
            result.append({"key": ".".join(v["id"] for v in dmap.values()), "dimensions": dmap,
                           "observations": obs})
            if max_series and len(result) >= max_series:
                break
    elif "observations" in ds:
        # AllDimensions layout: every dimension (incl. TIME_PERIOD) is an observation dimension.
        tpos = next((i for i, d in enumerate(odims) if d.get("id") in ("TIME_PERIOD", "TIME")), len(odims) - 1)
        groups: Dict[str, Dict] = {}
        for okey, ov in ds["observations"].items():
            parts = okey.split(":")
            series_parts = parts[:tpos] + parts[tpos + 1:]
            series_dims = odims[:tpos] + odims[tpos + 1:]
            gk = ":".join(series_parts)
            if gk not in groups:
                dmap = decode(series_dims, gk)
                groups[gk] = {"key": ".".join(v["id"] for v in dmap.values()), "dimensions": dmap,
                              "observations": []}
            try:
                tv = odims[tpos].get("values", [])[int(parts[tpos])]
                period = tv.get("id", tv.get("value"))
            except (ValueError, IndexError):
                period = parts[tpos] if tpos < len(parts) else None
            groups[gk]["observations"].append(
                {"period": period, "value": _num(ov[0] if isinstance(ov, list) and ov else ov)})
        for g in groups.values():
            g["observations"].sort(key=lambda o: str(o["period"]))
            result.append(g)
            if max_series and len(result) >= max_series:
                break
    return result


def compact(series: List[Dict]) -> List[Dict]:
    """Same series with dimension labels collapsed to 'name' strings (smaller payload for MCP/UI)."""
    out = []
    for s in series:
        out.append({"key": s["key"],
                     "labels": {k: (v["name"] or v["id"]) for k, v in s["dimensions"].items()},
                     "observations": s["observations"]})
    return out
