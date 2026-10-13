"""
RSF Press Freedom Data Fetcher
Reporters Without Borders World Press Freedom Index: annual scores, ranks and the five context indicators for 180 countries, read from RSF's official yearly CSV files (no key).
"""
import io
import csv
import sys
import json
import requests
from datetime import datetime
from typing import Dict, Any, List

# rsf.org has no JSON API; the index is published as one semicolon-separated CSV per year.
CSV_URL = "https://rsf.org/sites/default/files/import_classement/{year}.csv"
FIRST_YEAR = 2013
# RSF zone labels are French; English names / aliases -> a distinctive fragment of the label.
ZONES = {"africa": "Afrique", "americas": "riques", "asia-pacific": "Asie", "asia": "Asie",
         "europe": "UE Balkans", "eu-balkans": "UE Balkans", "eeca": "EEAC",
         "eastern europe": "EEAC", "central asia": "EEAC", "middle east": "MENA", "mena": "MENA"}
ZONE_EN = {"Afrique": "Africa", "riques": "Americas", "Asie": "Asia-Pacific", "UE Balkans": "EU-Balkans",
           "EEAC": "Eastern Europe-Central Asia", "MENA": "Middle East-North Africa"}
CONTEXTS = ["Political Context", "Economic Context", "Legal Context", "Social Context", "Safety"]

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.headers.update({"User-Agent": "Mozilla/5.0 (FinceptTerminal/4.5; +https://fincept.in)"})


def _num(v: str):
    v = (v or "").strip().replace(",", ".")
    try:
        f = float(v)
        return int(f) if f.is_integer() and "." not in v else f
    except ValueError:
        return None


def _latest_year() -> int:
    y = datetime.now().year
    # the index is published each May
    return y if datetime.now().month >= 6 else y - 1


def _load(year: int) -> Any:
    try:
        r = session.get(CSV_URL.format(year=int(year)), timeout=30)
        if r.status_code == 404:
            return {"error": f"No RSF index file for {year} (available {FIRST_YEAR}-{_latest_year()})"}
        r.raise_for_status()
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    raw = r.content
    for enc in ("utf-8-sig", "cp1252", "latin-1"):
        try:
            text = raw.decode(enc)
            break
        except UnicodeDecodeError:
            continue
    rows = list(csv.DictReader(io.StringIO(text), delimiter=";"))
    if not rows or "ISO" not in rows[0]:
        return {"error": f"Unexpected RSF file layout for {year}"}
    out = []
    for row in rows:
        row = {(k or "").strip(): (v or "").strip() for k, v in row.items()}
        score_key = next((k for k in row if k.startswith("Score") and "N-1" not in k and "exaction" not in k
                          and "evolution" not in k), None)
        rank_key = "Rank" if "Rank" in row else "Rank N"
        rec = {"iso3": row.get("ISO"), "country": row.get("Country_EN") or row.get("EN_country"),
               "zone": next((v for k, v in ZONE_EN.items() if k in row.get("Zone", "")), row.get("Zone")),
               "rank": _num(row.get(rank_key, "")),
               "score": _num(row.get(score_key, "")) if score_key else None,
               "rank_previous": _num(row.get("Rank N-1", "")), "score_previous": _num(row.get("Score N-1", ""))}
        if row.get("Situation"):
            rec["situation"] = row.get("Situation")
        ctx = {c: _num(row.get(c, "")) for c in CONTEXTS if c in row}
        if ctx:
            rec["indicators"] = ctx
        out.append(rec)
    out.sort(key=lambda x: x["rank"] if isinstance(x["rank"], int) else 999)
    return out


def get_countries() -> Any:
    rows = _load(_latest_year())
    if isinstance(rows, dict):
        return rows
    return {"count": len(rows), "countries": [{"iso3": r["iso3"], "name": r["country"], "zone": r["zone"]} for r in rows]}


def get_index_scores(year: int = None) -> Any:
    year = year or _latest_year()
    rows = _load(year)
    if isinstance(rows, dict):
        return rows
    return {"year": year, "count": len(rows), "scale": "0-100 (100 = best)", "scores": rows}


def get_country_score(country: str, year: int = None) -> Any:
    year = year or _latest_year()
    rows = _load(year)
    if isinstance(rows, dict):
        return rows
    hit = next((r for r in rows if country.upper() in (str(r["iso3"]).upper(), str(r["country"]).upper())), None)
    return {"year": year, **hit} if hit else {"error": f"{country} not found in the {year} index (use ISO-3)"}


def get_rankings(year: int = None, region: str = None) -> Any:
    res = get_index_scores(year)
    if "error" in res:
        return res
    if region:
        want = ZONE_EN.get(ZONES.get(region.lower(), ""), region).lower()
        res["scores"] = [r for r in res["scores"] if want in str(r["zone"]).lower()]
        res["count"] = len(res["scores"])
        res["zone"] = region
    return res


def get_trend(country: str, start_year: int = 2015, end_year: int = None) -> Any:
    end_year = end_year or _latest_year()
    points: List[Dict] = []
    for y in range(max(FIRST_YEAR, int(start_year)), int(end_year) + 1):
        r = get_country_score(country, y)
        if "error" not in r:
            points.append({"year": y, "rank": r.get("rank"), "score": r.get("score")})
    if not points:
        return {"error": f"No RSF data for {country}"}
    return {"country": country.upper(), "trend": points}


def get_indicators(country: str, year: int = None) -> Any:
    r = get_country_score(country, year)
    if "error" in r:
        return r
    if not r.get("indicators"):
        return {"error": f"Context indicators are published from 2022 onwards (requested {r.get('year')})"}
    return {"country": r["country"], "iso3": r["iso3"], "year": r["year"], "score": r["score"],
            "indicators": r["indicators"]}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "countries":
        result = get_countries()
    elif command == "scores":
        year = int(args[1]) if len(args) > 1 else None
        result = get_index_scores(year)
    elif command == "country":
        country = args[1] if len(args) > 1 else "FRA"
        year = int(args[2]) if len(args) > 2 else None
        result = get_country_score(country, year)
    elif command == "rankings":
        year = int(args[1]) if len(args) > 1 else None
        region = args[2] if len(args) > 2 else None
        result = get_rankings(year, region)
    elif command == "trend":
        country = args[1] if len(args) > 1 else "FRA"
        start_year = int(args[2]) if len(args) > 2 else 2015
        end_year = int(args[3]) if len(args) > 3 else None
        result = get_trend(country, start_year, end_year)
    elif command == "indicators":
        country = args[1] if len(args) > 1 else "FRA"
        year = int(args[2]) if len(args) > 2 else None
        result = get_indicators(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
