"""
OECD Development Cooperation Data Fetcher
Official Development Assistance (ODA) flows, multilateral aid, aid by sector, ODA/GNI and
donor/recipient lists from the OECD Development Assistance Committee (DAC) tables, via the
keyless OECD SDMX API (sdmx.oecd.org; the old stats.oecd.org SDMX-JSON service was retired).
"""
import sys
import json
import requests
from typing import Dict, Any

from sdmx_json import flatten, compact

BASE_URL = "https://sdmx.oecd.org/public/rest/data"
DAC2A = "OECD.DCD.FSD,DSD_DAC2@DF_DAC2A,"   # ODA disbursements to countries and regions
DAC1 = "OECD.DCD.FSD,DSD_DAC1@DF_DAC1,"     # flows by provider
DAC5 = "OECD.DCD.FSD,DSD_DAC1@DF_DAC5,"     # ODA by sector and provider
DATA_ACCEPT = "application/vnd.sdmx.data+json;version=1.0"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _code(v: str, default: str) -> str:
    """'all' -> wildcard; donor/recipient codes are ISO-3 or OECD aggregates (DAC, DPGC, ALLM ...)."""
    if not v:
        return default
    return "" if v.lower() == "all" else v.upper().replace(",", "+")


def _query(flow: str, key: str, start_year: str = None) -> Any:
    params = {"dimensionAtObservation": "AllDimensions", "startPeriod": start_year or "2015"}
    try:
        r = session.get(f"{BASE_URL}/{flow}/{key}", params=params, headers={"Accept": DATA_ACCEPT}, timeout=120)
        if r.status_code == 404:
            return {"error": "No data for this query (HTTP 404) - check donor/recipient/sector codes"}
        if r.status_code == 429:
            return {"error": "OECD API rate limit reached (HTTP 429) - retry in a minute"}
        r.raise_for_status()
        return r.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {e.response.text[:200]}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _series(flow: str, key: str, start_year: str, meta: Dict, max_series: int = 300) -> Any:
    raw = _query(flow, key, start_year)
    if isinstance(raw, dict) and "error" in raw:
        return {**meta, **raw}
    series = compact(flatten(raw))
    out = {**meta, "key": key, "unit": "USD millions", "series_count": len(series), "series": series[:max_series]}
    if len(series) > max_series:
        out["truncated"] = True
        out["note"] = f"{len(series)} series matched; only the first {max_series} returned - narrow the codes"
    return out


def get_oda_flows(donor: str = "DAC", recipient: str = "DPGC", start_year: str = None) -> Any:
    """Net ODA disbursements (constant USD) from donor(s) to recipient(s) - DAC Table 2A."""
    key = f"{_code(donor, 'DAC')}.{_code(recipient, 'DPGC')}.206.USD.Q"
    return _series(DAC2A, key, start_year, {"dataset": "DAC2A", "measure": "ODA disbursements, constant prices"})


def get_bilateral_aid(donor: str = "DAC", sector: str = "110+120+140+150+210+230+310+700",
                      start_year: str = None) -> Any:
    """Bilateral ODA commitments by sector (constant USD) - DAC Table 5.
    sector codes: 110 education, 120 health, 140 water, 150 government, 210 transport, 230 energy,
    310 agriculture, 700 humanitarian, 1000 all sectors."""
    key = f"{_code(donor, 'DAC')}.{_code(sector, '1000')}.528._Z._Z.USD.Q"
    return _series(DAC5, key, start_year, {"dataset": "DAC5", "measure": "Bilateral ODA commitments by sector"})


def get_multilateral_aid(donor: str = "DAC", start_year: str = None) -> Any:
    """Multilateral ODA, net disbursements (constant USD) - DAC Table 1."""
    key = f"{_code(donor, 'DAC')}._Z.2000._Z.1140.USD.Q"
    return _series(DAC1, key, start_year, {"dataset": "DAC1", "measure": "Multilateral ODA, net disbursements"})


def get_oda_gni(donor: str = "DAC", start_year: str = None) -> Any:
    """Total ODA (grant equivalent) and ODA as % of GNI per donor - DAC Table 1."""
    key = f"{_code(donor, 'DAC')}._Z.11010+11002._Z.1160.."
    res = _series(DAC1, key, start_year, {"dataset": "DAC1", "measure": "ODA grant equivalent and ODA/GNI"})
    res.pop("unit", None)
    return res


def _members(flow: str, key: str, dim: str) -> Any:
    # (detail=serieskeysonly returns malformed JSON on this API, so read one recent year instead)
    raw = _query(flow, key, start_year="2023")
    if isinstance(raw, dict) and "error" in raw:
        return raw
    seen = {}
    for s in flatten(raw):
        d = s["dimensions"].get(dim)
        if d and d["id"] not in seen:
            seen[d["id"]] = d["name"]
    items = [{"code": k, "name": v} for k, v in sorted(seen.items(), key=lambda kv: kv[1])]
    return {"count": len(items), "items": items}


def get_donors() -> Any:
    """Providers reporting ODA to the DAC (codes usable as `donor`)."""
    return _members(DAC1, "._Z.11010._Z.1160.USD.V", "DONOR")


def get_recipients() -> Any:
    """Countries/regions receiving DAC ODA (codes usable as `recipient`)."""
    return _members(DAC2A, "DAC..206.USD.V", "RECIPIENT")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "oda":
        donor = args[1] if len(args) > 1 else "DAC"
        recipient = args[2] if len(args) > 2 else "DPGC"
        start_year = args[3] if len(args) > 3 else None
        result = get_oda_flows(donor, recipient, start_year)
    elif command == "bilateral":
        donor = args[1] if len(args) > 1 else "DAC"
        sector = args[2] if len(args) > 2 else "110+120+140+150+210+230+310+700"
        start_year = args[3] if len(args) > 3 else None
        result = get_bilateral_aid(donor, sector, start_year)
    elif command == "multilateral":
        donor = args[1] if len(args) > 1 else "DAC"
        start_year = args[2] if len(args) > 2 else None
        result = get_multilateral_aid(donor, start_year)
    elif command == "oda_gni":
        donor = args[1] if len(args) > 1 else "DAC"
        start_year = args[2] if len(args) > 2 else None
        result = get_oda_gni(donor, start_year)
    elif command == "donors":
        result = get_donors()
    elif command == "recipients":
        result = get_recipients()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
