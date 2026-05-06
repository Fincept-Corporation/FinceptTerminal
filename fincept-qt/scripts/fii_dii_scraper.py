#!/usr/bin/env python3
"""
fii_dii_scraper.py — NSE cash-market FII/DII daily flows.

NSE publishes the daily institutional buy/sell numbers on
https://www.nseindia.com/api/fiidiiTradeReact . The endpoint refuses naked
HTTP clients; it requires a session cookie established by a prior visit to
https://www.nseindia.com/ plus a browser-like User-Agent. This script
handles the cookie dance via requests.Session and prints a single JSON
array on stdout — one row per category for "today" (NSE only returns one
day per call).

Output schema (single line on stdout, the C++ caller parses with
QJsonDocument::fromJson):

    [
      {
        "date":      "2026-05-06",   # ISO yyyy-mm-dd
        "fii_buy":   <float>,         # ₹ crore
        "fii_sell":  <float>,
        "fii_net":   <float>,
        "dii_buy":   <float>,
        "dii_sell":  <float>,
        "dii_net":   <float>
      }
    ]

If the upstream is unavailable or blocks the request, prints an empty
array `[]` so the caller has a clean parse path. Errors are logged to
stderr (which PythonRunner captures separately).
"""

import json
import re
import sys
from datetime import datetime
from typing import Any, Dict, List, Optional


def _make_session():
    import requests
    s = requests.Session()
    s.headers.update({
        "User-Agent": (
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/123.0 Safari/537.36"
        ),
        "Accept": "application/json,text/plain,*/*",
        "Accept-Language": "en-US,en;q=0.9",
        "Referer": "https://www.nseindia.com/reports/fii-dii",
        "Connection": "keep-alive",
    })
    # Seed cookies with a homepage visit; failing this still returns a
    # session but the API call will likely 401.
    try:
        s.get("https://www.nseindia.com/", timeout=8)
    except Exception as e:
        print(f"warn: homepage visit failed: {e}", file=sys.stderr)
    return s


def _parse_iso_date(raw: str) -> Optional[str]:
    """NSE returns dates in formats like '06-May-2026' — normalise to ISO."""
    if not raw:
        return None
    raw = raw.strip()
    for fmt in ("%d-%b-%Y", "%d-%B-%Y", "%d-%m-%Y", "%Y-%m-%d"):
        try:
            return datetime.strptime(raw, fmt).strftime("%Y-%m-%d")
        except Exception:
            continue
    # Fallback — strip any embedded time, keep first 10 chars
    return raw[:10] if len(raw) >= 10 else None


def _to_float(v: Any) -> float:
    if v is None:
        return 0.0
    if isinstance(v, (int, float)):
        return float(v)
    s = str(v).replace(",", "").strip()
    if not s or s in ("-", "--", "NA"):
        return 0.0
    try:
        return float(s)
    except Exception:
        return 0.0


def _category_key(name: str) -> Optional[str]:
    """Map NSE's category labels (FII/FPI, DII, etc.) to our prefix."""
    if not name:
        return None
    n = re.sub(r"[^A-Za-z]", "", name).upper()
    if "FII" in n or "FPI" in n:
        return "fii"
    if "DII" in n:
        return "dii"
    return None


def fetch_latest() -> List[Dict[str, Any]]:
    sess = _make_session()
    url = "https://www.nseindia.com/api/fiidiiTradeReact"
    try:
        r = sess.get(url, timeout=10)
        r.raise_for_status()
        data = r.json()
    except Exception as e:
        print(f"error: fii/dii API failed: {e}", file=sys.stderr)
        return []

    # Response shape: list of dicts, each with category, buyValue, sellValue,
    # netValue, date. Group by date.
    if not isinstance(data, list):
        print(f"warn: unexpected payload type {type(data).__name__}", file=sys.stderr)
        return []

    by_date: Dict[str, Dict[str, Any]] = {}
    for entry in data:
        if not isinstance(entry, dict):
            continue
        prefix = _category_key(entry.get("category", ""))
        if prefix is None:
            continue
        iso = _parse_iso_date(entry.get("date") or "")
        if not iso:
            continue
        rec = by_date.setdefault(iso, {
            "date": iso,
            "fii_buy": 0.0, "fii_sell": 0.0, "fii_net": 0.0,
            "dii_buy": 0.0, "dii_sell": 0.0, "dii_net": 0.0,
        })
        rec[f"{prefix}_buy"] = _to_float(entry.get("buyValue"))
        rec[f"{prefix}_sell"] = _to_float(entry.get("sellValue"))
        rec[f"{prefix}_net"] = _to_float(entry.get("netValue"))

    # Sort ascending by date (only one day in practice but be tolerant).
    return [by_date[k] for k in sorted(by_date.keys())]


def main():
    rows = fetch_latest()
    print(json.dumps(rows))


if __name__ == "__main__":
    main()
