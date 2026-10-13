# IMF (International Monetary Fund) Data Wrapper
# Keyless IMF data: World Economic Outlook series (IMF DataMapper API), international reserves
# (IRFCL), direction of trade (IMTS) and any other dataflow of the IMF SDMX 3.0 API
# (api.imf.org — the old dataservices.imf.org SDMX_JSON service was retired).

import sys
import json
import requests
from typing import Dict, Any, List, Optional
from datetime import datetime

from sdmx_json import flatten

SDMX_BASE = "https://api.imf.org/external/sdmx"
DATAMAPPER = "https://www.imf.org/external/datamapper/api/v1"
DATA_ACCEPT = "application/vnd.sdmx.data+json;version=2.0.0"
STRUCT_ACCEPT = "application/vnd.sdmx.structure+json;version=1.0.0"

session = requests.Session()
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})

# ISO-2 / common names -> ISO-3 (the IMF APIs key countries by ISO-3).
ISO2_TO_ISO3 = {
    "US": "USA", "GB": "GBR", "UK": "GBR", "CN": "CHN", "JP": "JPN", "DE": "DEU", "FR": "FRA", "IN": "IND",
    "IT": "ITA", "CA": "CAN", "KR": "KOR", "RU": "RUS", "BR": "BRA", "AU": "AUS", "ES": "ESP", "MX": "MEX",
    "ID": "IDN", "NL": "NLD", "SA": "SAU", "TR": "TUR", "CH": "CHE", "PL": "POL", "SE": "SWE", "BE": "BEL",
    "AR": "ARG", "IE": "IRL", "AT": "AUT", "NO": "NOR", "IL": "ISR", "AE": "ARE", "EG": "EGY", "ZA": "ZAF",
    "DK": "DNK", "SG": "SGP", "MY": "MYS", "PH": "PHL", "TH": "THA", "NG": "NGA", "PK": "PAK", "CL": "CHL",
    "FI": "FIN", "RO": "ROU", "CZ": "CZE", "PT": "PRT", "IQ": "IRQ", "PE": "PER", "GR": "GRC", "NZ": "NZL",
    "QA": "QAT", "DZ": "DZA", "HU": "HUN", "KZ": "KAZ", "KW": "KWT", "MA": "MAR", "UA": "UKR", "SK": "SVK",
    "EC": "ECU", "VN": "VNM", "BD": "BGD", "AO": "AGO", "KE": "KEN", "OM": "OMN", "LK": "LKA", "LU": "LUX",
    "PA": "PAN", "UY": "URY", "CR": "CRI", "LT": "LTU", "SI": "SVN", "BG": "BGR", "HR": "HRV", "RS": "SRB",
    "CO": "COL", "HK": "HKG", "TW": "TWN", "IS": "ISL", "EE": "EST", "LV": "LVA", "CY": "CYP", "MT": "MLT",
    "GH": "GHA", "ET": "ETH", "TZ": "TZA", "JO": "JOR", "LB": "LBN", "TN": "TUN", "BO": "BOL", "PY": "PRY",
    "VE": "VEN", "IR": "IRN", "MM": "MMR", "KH": "KHM", "NP": "NPL", "MN": "MNG", "GE": "GEO", "AM": "ARM",
    "AZ": "AZE", "BY": "BLR", "UZ": "UZB", "ZM": "ZMB", "ZW": "ZWE", "MZ": "MOZ", "SN": "SEN", "CI": "CIV",
    "CM": "CMR", "DO": "DOM", "GT": "GTM", "HN": "HND", "SV": "SLV", "NI": "NIC", "JM": "JAM", "TT": "TTO",
}
NAME_TO_ISO3 = {
    "united_states": "USA", "usa": "USA", "united_kingdom": "GBR", "china": "CHN", "japan": "JPN",
    "germany": "DEU", "france": "FRA", "india": "IND", "italy": "ITA", "canada": "CAN", "south_korea": "KOR",
    "russia": "RUS", "brazil": "BRA", "australia": "AUS", "spain": "ESP", "mexico": "MEX", "indonesia": "IDN",
    "netherlands": "NLD", "saudi_arabia": "SAU", "turkey": "TUR", "switzerland": "CHE", "south_africa": "ZAF",
    "singapore": "SGP", "argentina": "ARG", "sweden": "SWE", "norway": "NOR", "poland": "POL",
    "euro_area": "G163", "world": "G001",
}

# IRFCL (International Reserves and Foreign Currency Liquidity) indicator presets, US dollars.
IRFCL_INDICATORS = {
    "IRFCLDT1_IRFCL65_USD": "Official reserve assets",
    "IRFCLDT1_IRFCLCDCFC_USD": "Official reserve currency and deposits (convertible foreign currencies)",
    "IRFCLDT1_IRFCL32_USD": "Official reserve securities",
    "IRFCLDT1_IRFCL56_USD": "Official reserve assets: gold (incl. gold deposits / swapped)",
    "IRFCLDT1_IRFCL56V_FTO": "Gold volume (fine troy ounces)",
    "IRFCLDT1_IRFCL57_USD": "IMF reserve position",
    "IRFCLDT1_IRFCL58_USD": "Other reserve assets",
    "IRFCLDT1_IRFCL59_USD": "Official reserve assets: financial derivatives",
    "IRFCLDT1_IRFCL34_USD": "Other foreign currency assets",
    "IRFCLDT1_IRFCL54_USD": "Official reserve assets and other foreign currency assets",
}
IRFCL_PRESETS = {
    "irfcl_top_lines": ["IRFCLDT1_IRFCL54_USD", "IRFCLDT1_IRFCL65_USD", "IRFCLDT1_IRFCLCDCFC_USD",
                        "IRFCLDT1_IRFCL32_USD", "IRFCLDT1_IRFCL56_USD", "IRFCLDT1_IRFCL57_USD",
                        "IRFCLDT1_IRFCL58_USD", "IRFCLDT1_IRFCL34_USD"],
    "reserve_assets": ["IRFCLDT1_IRFCL65_USD", "IRFCLDT1_IRFCLCDCFC_USD", "IRFCLDT1_IRFCL32_USD",
                       "IRFCLDT1_IRFCL56_USD", "IRFCLDT1_IRFCL57_USD", "IRFCLDT1_IRFCL58_USD"],
    "gold_reserves": ["IRFCLDT1_IRFCL56_USD", "IRFCLDT1_IRFCL56V_FTO"],
    "derivative_assets": ["IRFCLDT1_IRFCL59_USD"],
}
TRADE_INDICATORS = {
    "exports": ["XG_FOB_USD"],
    "imports": ["MG_CIF_USD"],
    "balance": ["TBG_USD"],
    "all": ["XG_FOB_USD", "MG_CIF_USD", "TBG_USD"],
}
TRADE_TITLES = {
    "XG_FOB_USD": "Exports of goods, FOB, US dollars",
    "MG_CIF_USD": "Imports of goods, CIF, US dollars",
    "MG_FOB_USD": "Imports of goods, FOB, US dollars",
    "TBG_USD": "Trade balance, goods, US dollars",
}
FREQUENCY_MAP = {"annual": "A", "yearly": "A", "a": "A", "quarter": "Q", "quarterly": "Q", "q": "Q",
                 "month": "M", "monthly": "M", "m": "M"}
# World Economic Outlook headline indicators (DataMapper codes).
WEO_CORE = {
    "NGDP_RPCH": "Real GDP growth (annual % change)",
    "NGDPD": "GDP, current prices (billions of US dollars)",
    "NGDPDPC": "GDP per capita, current prices (US dollars)",
    "PCPIPCH": "Inflation rate, average consumer prices (annual % change)",
    "LUR": "Unemployment rate (%)",
    "BCA_NGDPD": "Current account balance (% of GDP)",
    "GGXWDG_NGDP": "General government gross debt (% of GDP)",
    "GGXCNL_NGDP": "General government net lending/borrowing (% of GDP)",
}


def _err(endpoint: str, message: str) -> Dict[str, Any]:
    return {"error": message, "endpoint": endpoint, "timestamp": datetime.now().isoformat()}


def _iso3(country: str) -> str:
    c = (country or "").strip()
    if not c:
        return c
    if c.lower() in NAME_TO_ISO3:
        return NAME_TO_ISO3[c.lower()]
    if len(c) == 2:
        return ISO2_TO_ISO3.get(c.upper(), c.upper())
    return c.upper()


def _countries(countries: Optional[str], default: str = "USA") -> List[str]:
    if not countries:
        return [default]
    if countries.lower() == "all":
        return ["*"]
    return [_iso3(c) for c in countries.replace("+", ",").split(",") if c.strip()]


def _get_json(url: str, params: Dict = None, accept: str = "application/json", timeout: int = 90) -> Any:
    r = session.get(url, params=params, headers={"Accept": accept}, timeout=timeout)
    if r.status_code == 404:
        return {"__nodata__": True}
    r.raise_for_status()
    return r.json()


def _sdmx_data(flow: str, key: str, start: Optional[str], end: Optional[str], last_n: Optional[int] = None) -> Any:
    agency, _, fid = flow.partition("/") if "/" in flow else ("IMF.STA", "", flow)
    # The API's c[TIME_PERIOD] filter is unreliable (ignored, or empty with a range), so the
    # period window is applied client-side below.
    params = {}
    if last_n and not start and not end:
        params["lastNObservations"] = last_n
    url = f"{SDMX_BASE}/3.0/data/dataflow/{agency}/{fid}/+/{key}"
    raw = _get_json(url, params=params, accept=DATA_ACCEPT)
    if isinstance(raw, dict) and raw.get("__nodata__"):
        return []
    series = flatten(raw)
    if start or end:
        for s in series:
            s["observations"] = [o for o in s["observations"]
                                 if (not start or _period(o["period"]) >= start)
                                 and (not end or _period(o["period"])[:len(end)] <= end)]
        series = [s for s in series if s["observations"]]
    return series


def _period(p: str) -> str:
    # IMF SDMX 3.0 periods look like 2026-M01 / 2025-Q3 / 2025
    return p.replace("-M", "-") if isinstance(p, str) else p


def get_economic_indicators(countries: Optional[str] = None, symbols: Optional[str] = None,
                            frequency: str = "monthly", start_date: Optional[str] = None,
                            end_date: Optional[str] = None, sector: str = "all") -> Dict[str, Any]:
    """International reserves (IRFCL). symbols: preset (irfcl_top_lines, reserve_assets, gold_reserves,
    derivative_assets) or comma-separated IRFCL indicator codes."""
    try:
        sym = (symbols or "irfcl_top_lines").strip()
        codes = IRFCL_PRESETS.get(sym.lower()) or [s.strip() for s in sym.replace("+", ",").split(",") if s.strip()]
        freq = FREQUENCY_MAP.get((frequency or "monthly").lower(), "M")
        ctry = "+".join(_countries(countries))
        key = f"{ctry}.{'+'.join(codes)}.*.{freq}"
        series = _sdmx_data("IMF.STA/IRFCL", key, start_date, end_date, last_n=None if start_date else 24)
        rows = []
        for s in series:
            d = s["dimensions"]
            ind = d.get("INDICATOR", {}).get("id")
            for o in s["observations"]:
                if o["value"] is None:
                    continue
                rows.append({"date": _period(o["period"]), "country": d.get("COUNTRY", {}).get("id"),
                             "symbol": ind, "title": IRFCL_INDICATORS.get(ind, ind),
                             "sector": d.get("SECTOR", {}).get("id"), "value": o["value"]})
        if not rows:
            return _err("economic_indicators", "No data found for the specified parameters")
        rows.sort(key=lambda r: (r["country"] or "", r["symbol"] or "", r["date"] or ""))
        return {"data": rows, "count": len(rows), "source": "IMF IRFCL (api.imf.org)",
                "parameters": {"countries": ctry, "symbols": codes, "frequency": freq,
                               "start_date": start_date, "end_date": end_date}}
    except Exception as e:
        return _err("economic_indicators", str(e))


def get_direction_of_trade(countries: Optional[str] = None, counterparts: Optional[str] = None,
                           direction: str = "all", frequency: str = "annual",
                           start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
    """Bilateral merchandise trade (IMF IMTS, formerly DOTS). counterparts default: World."""
    try:
        codes = TRADE_INDICATORS.get((direction or "all").lower(), TRADE_INDICATORS["all"])
        freq = FREQUENCY_MAP.get((frequency or "annual").lower(), "A")
        ctry = "+".join(_countries(countries))
        cpty = "+".join(_countries(counterparts, default="G001"))
        key = f"{ctry}.{'+'.join(codes)}.{cpty}.{freq}"
        series = _sdmx_data("IMF.STA/IMTS", key, start_date, end_date, last_n=None if start_date else 10)
        rows = []
        for s in series:
            d = s["dimensions"]
            ind = d.get("INDICATOR", {}).get("id")
            for o in s["observations"]:
                if o["value"] is None:
                    continue
                rows.append({"date": _period(o["period"]), "country": d.get("COUNTRY", {}).get("id"),
                             "counterpart": d.get("COUNTERPART_COUNTRY", {}).get("id"),
                             "symbol": ind, "title": TRADE_TITLES.get(ind, ind), "value": o["value"]})
        if not rows:
            return _err("direction_of_trade", "No data found for the specified parameters")
        rows.sort(key=lambda r: (r["country"] or "", r["counterpart"] or "", r["symbol"] or "", r["date"] or ""))
        return {"data": rows, "count": len(rows), "source": "IMF IMTS (api.imf.org)",
                "parameters": {"countries": ctry, "counterparts": cpty, "direction": direction,
                               "frequency": freq, "start_date": start_date, "end_date": end_date}}
    except Exception as e:
        return _err("direction_of_trade", str(e))


def _weo_indicator(indicator: str, iso3s: List[str], start: Optional[int], end: Optional[int]) -> Dict:
    raw = _get_json(f"{DATAMAPPER}/{indicator}", timeout=60)
    values = (raw or {}).get("values", {}).get(indicator, {})
    out = {}
    for c in iso3s:
        series = values.get(c, {})
        pts = [{"year": int(y), "value": v} for y, v in series.items()
               if (start is None or int(y) >= start) and (end is None or int(y) <= end)]
        pts.sort(key=lambda p: p["year"])
        out[c] = pts
    return out


def get_weo(indicator: str = "NGDP_RPCH", countries: Optional[str] = None,
            start_year: Optional[str] = None, end_year: Optional[str] = None) -> Dict[str, Any]:
    """One World Economic Outlook indicator (incl. IMF projections) for one or more countries."""
    try:
        iso3s = [c for c in _countries(countries) if c != "*"]
        data = _weo_indicator(indicator.upper(), iso3s, int(start_year) if start_year else None,
                              int(end_year) if end_year else None)
        if not any(data.values()):
            return _err("weo", f"No WEO data for {indicator} / {','.join(iso3s)}")
        return {"indicator": indicator.upper(), "title": WEO_CORE.get(indicator.upper()), "data": data,
                "source": "IMF World Economic Outlook (DataMapper API)"}
    except Exception as e:
        return _err("weo", str(e))


def get_available_indicators(query: Optional[str] = None) -> Dict[str, Any]:
    """WEO/DataMapper indicators (code, label, unit) plus the IRFCL and trade presets."""
    try:
        raw = _get_json(f"{DATAMAPPER}/indicators", timeout=60)
        inds = []
        for code, meta in (raw or {}).get("indicators", {}).items():
            if not code or not isinstance(meta, dict):
                continue
            label = meta.get("label") or ""
            if query and query.lower() not in (code + " " + label + " " + (meta.get("description") or "")).lower():
                continue
            inds.append({"symbol": code, "label": label, "unit": meta.get("unit"), "dataset": meta.get("dataset"),
                         "source": meta.get("source")})
        return {"indicators": inds, "count": len(inds),
                "irfcl_presets": IRFCL_PRESETS, "irfcl_indicators": IRFCL_INDICATORS,
                "trade_indicators": TRADE_TITLES}
    except Exception as e:
        return _err("available_indicators", str(e))


def get_dataflows(query: Optional[str] = None) -> Dict[str, Any]:
    """All IMF SDMX dataflows (agency/id + name), optionally filtered."""
    try:
        raw = _get_json(f"{SDMX_BASE}/2.1/dataflow/all/all/latest", accept=STRUCT_ACCEPT)
        flows = []
        for f in (raw or {}).get("data", {}).get("dataflows", []):
            fid = f.get("id", "")
            if "VINTAGE" in fid:
                continue
            name = f.get("name") if isinstance(f.get("name"), str) else (f.get("names") or {}).get("en", "")
            if query and query.lower() not in (fid + " " + name).lower():
                continue
            flows.append({"flow": f"{f.get('agencyID')}/{fid}", "name": name})
        return {"dataflows": flows, "count": len(flows)}
    except Exception as e:
        return _err("dataflows", str(e))


def get_data(flow: str, key: str = "*", start: Optional[str] = None, end: Optional[str] = None,
             max_series: int = 200) -> Dict[str, Any]:
    """Generic IMF SDMX 3.0 query: flow 'IMF.STA/CPI', key with '*' wildcards and '+' for OR."""
    try:
        series = _sdmx_data(flow, key, start, end, last_n=None if start else 12)
        out = [{"key": s["key"], "observations": [{"period": _period(o["period"]), "value": o["value"]}
                                                  for o in s["observations"]]} for s in series]
        res = {"flow": flow, "key": key, "series_count": len(out), "series": out[:max_series]}
        if len(out) > max_series:
            res["truncated"] = True
            res["note"] = f"{len(out)} series matched; only the first {max_series} returned - narrow the key"
        return res
    except Exception as e:
        return _err("data", str(e))


def get_comprehensive_economic_data(country: Optional[str] = None, start_date: Optional[str] = None,
                                    end_date: Optional[str] = None) -> Dict[str, Any]:
    """Headline WEO indicators for one country (history + IMF projections)."""
    try:
        iso3 = _iso3(country or "USA")
        start = int(str(start_date)[:4]) if start_date else datetime.now().year - 10
        end = int(str(end_date)[:4]) if end_date else datetime.now().year + 2
        out = {}
        for code, title in WEO_CORE.items():
            try:
                pts = _weo_indicator(code, [iso3], start, end).get(iso3, [])
            except Exception as e:  # one failing indicator must not sink the rest
                pts = []
                out[code] = {"title": title, "error": str(e)}
                continue
            out[code] = {"title": title, "data": pts}
        if not any(v.get("data") for v in out.values()):
            return _err("comprehensive_economic_data", f"No WEO data for {iso3}")
        return {"country": iso3, "indicators": out, "source": "IMF World Economic Outlook (DataMapper API)",
                "parameters": {"country": country, "start_date": start, "end_date": end}}
    except Exception as e:
        return _err("comprehensive_economic_data", str(e))


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    usage = {
        "error": "Usage: python imf_data.py <command> [args...]",
        "commands": [
            "economic_indicators [countries] [symbols] [frequency] [start_date] [end_date]",
            "direction_of_trade [countries] [counterparts] [direction] [frequency] [start_date] [end_date]",
            "weo [indicator] [countries] [start_year] [end_year]",
            "available_indicators [query]",
            "comprehensive_economic_data [country] [start_date] [end_date]",
            "dataflows [query]",
            "data <flow> [key] [start] [end]",
        ],
    }
    if not args:
        print(json.dumps(usage))
        sys.exit(1)

    command = args[0]

    def arg(i, default=None):
        return args[i] if len(args) > i and args[i] != "" else default

    try:
        if command == "economic_indicators":
            result = get_economic_indicators(arg(1), arg(2), arg(3, "monthly"), arg(4), arg(5), arg(6, "all"))
        elif command == "direction_of_trade":
            result = get_direction_of_trade(arg(1), arg(2), arg(3, "all"), arg(4, "annual"), arg(5), arg(6))
        elif command == "weo":
            result = get_weo(arg(1, "NGDP_RPCH"), arg(2), arg(3), arg(4))
        elif command == "available_indicators":
            result = get_available_indicators(arg(1))
        elif command == "comprehensive_economic_data":
            result = get_comprehensive_economic_data(arg(1), arg(2), arg(3))
        elif command == "dataflows":
            result = get_dataflows(arg(1))
        elif command == "data":
            if not arg(1):
                result = {"error": "Usage: data <flow e.g. IMF.STA/CPI> [key] [start] [end]"}
            else:
                result = get_data(arg(1), arg(2, "*"), arg(3), arg(4))
        else:
            result = _err(command, f"Unknown command: {command}")
        print(json.dumps(result))
    except Exception as e:
        print(json.dumps(_err(command, str(e))))


if __name__ == "__main__":
    main()
