"""
relationship_map_sources.py - the parts of relationship_map.py that are not Yahoo profile
calls (Fincept Terminal, Relationship Map tab):

  * subsidiaries from SEC EDGAR (no key): the company's CIK, its latest annual report
    (10-K, 20-F or 40-F) and the subsidiaries exhibit of that filing (EX-21 in a 10-K, EX-8 in
    a 20-F), found through the filing index page's "Type" column - a file-name match picks
    the wrong document (TSM's 20-F cover page);
  * the daily-return correlation set computed from one Yahoo price download;
  * the static tables: the curated industry peers (fallback only), the US sector SPDR ETFs
    and the benchmark index of each listing venue.

Every network request goes through the caller's `call(label, fn, timeout)` so the
relationship map's time budget applies; this module never decides on time limits itself.
Caches live under fincept_paths.cache_dir("relationship_map") only.
"""

import json
import os
import re
import threading
import time
from datetime import datetime, timezone

try:
    import fincept_paths
except ImportError:  # scripts dir not on sys.path (unusual launcher)
    import sys

    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_paths


# ── Static tables ────────────────────────────────────────────────────────────

# Curated US peer lists: the fallback when Yahoo's industry screen returns nothing (or does
# not answer). Keys are what "industry:<key>" reports.
INDUSTRY_PEERS = {
    "Technology": ["AAPL", "MSFT", "GOOGL", "META", "AMZN", "NVDA", "CRM", "ORCL", "ADBE"],
    "Software": ["MSFT", "CRM", "ORCL", "ADBE", "NOW", "INTU", "SNOW", "PLTR"],
    "Semiconductors": ["NVDA", "AMD", "INTC", "AVGO", "QCOM", "TXN", "MU", "AMAT"],
    "Electric Vehicles": ["TSLA", "RIVN", "LCID", "NIO", "XPEV", "LI"],
    "Auto Manufacturers": ["TSLA", "F", "GM", "TM", "HMC", "STLA"],
    "Banks": ["JPM", "BAC", "WFC", "C", "GS", "MS", "USB"],
    "Pharmaceuticals": ["JNJ", "PFE", "MRK", "ABBV", "LLY", "BMY", "AMGN"],
    "Oil & Gas": ["XOM", "CVX", "COP", "EOG", "SLB", "PSX"],
    "Retail": ["WMT", "COST", "TGT", "HD", "LOW", "AMZN"],
    "Payments": ["V", "MA", "PYPL", "XYZ", "FIS", "FI"],
    "Cloud Computing": ["AMZN", "MSFT", "GOOGL", "CRM", "SNOW", "NOW"],
    "Telecom": ["T", "VZ", "TMUS", "CMCSA"],
    "Healthcare": ["UNH", "CVS", "HCA", "MCK", "COR"],
    "Insurance": ["BRK-B", "MET", "PRU", "AFL", "AIG"],
    "Real Estate": ["AMT", "PLD", "CCI", "EQIX", "SPG"],
    "Consumer": ["PG", "KO", "PEP", "UL", "CL", "MCD", "SBUX"],
    "Airlines": ["DAL", "UAL", "AAL", "LUV", "ALK"],
    "Defense": ["LMT", "RTX", "NOC", "GD", "BA"],
    # Release 1 called this list "Energy" and matched it on the Energy SECTOR, so oil
    # companies were compared with electric utilities.
    "Utilities": ["NEE", "DUK", "SO", "AEP", "D"],
}

# Yahoo industry names (lower case, substring match) -> curated key. Industry matches always
# win over sector matches: release 1 walked one dict that matched the SECTOR "Technology"
# before the INDUSTRY "Semiconductors", so TSM / NVDA got Apple and Microsoft as peers.
_INDUSTRY_ALIASES = (
    ("Semiconductors", ("semiconductor",)),
    ("Software", ("software",)),
    ("Auto Manufacturers", ("auto manufacturers",)),
    ("Banks", ("banks",)),
    ("Pharmaceuticals", ("drug manufacturers", "pharmaceutical")),
    ("Oil & Gas", ("oil & gas",)),
    ("Retail", ("discount stores", "home improvement retail", "department stores", "internet retail")),
    ("Payments", ("credit services", "payment")),
    ("Telecom", ("telecom",)),
    ("Healthcare", ("healthcare plans", "medical care facilities", "medical distribution")),
    ("Insurance", ("insurance",)),
    ("Real Estate", ("reit", "real estate")),
    ("Consumer", ("household & personal products", "beverages", "restaurants", "packaged foods", "confectioners")),
    ("Airlines", ("airlines",)),
    ("Defense", ("aerospace & defense",)),
    ("Utilities", ("utilities",)),
    ("Technology", ("consumer electronics", "computer hardware", "communication equipment", "internet content")),
)

# Yahoo sector -> curated key, used only when no industry alias matched.
_SECTOR_FALLBACK = {
    "technology": "Technology",
    "healthcare": "Healthcare",
    "real estate": "Real Estate",
    "energy": "Oil & Gas",
    "utilities": "Utilities",
    "consumer defensive": "Consumer",
    "consumer cyclical": "Consumer",
}

GENERIC_PEERS = ["AAPL", "MSFT", "GOOGL", "AMZN", "META"]


def _norm_text(value):
    s = (value or "").replace("—", " - ").replace("–", " - ")
    return re.sub(r"\s+", " ", s).strip().lower()


def curated_peers(industry, sector, ticker):
    """(symbols, key) from the curated table - industry match first, then sector; ([], "")."""
    ind, sec = _norm_text(industry), _norm_text(sector)
    key = ""
    if ind:
        for k, aliases in _INDUSTRY_ALIASES:
            if any(a in ind for a in aliases):
                key = k
                break
    if not key and sec:
        key = _SECTOR_FALLBACK.get(sec, "")
    if not key:
        return [], ""
    own = (ticker or "").upper()
    return [p for p in INDUSTRY_PEERS[key] if p != own], key


# US sector SPDR ETFs by Yahoo sector.
SECTOR_ETFS = {
    "Technology": ("XLK", "Technology Select Sector SPDR"),
    "Financial Services": ("XLF", "Financial Select Sector SPDR"),
    "Healthcare": ("XLV", "Health Care Select Sector SPDR"),
    "Energy": ("XLE", "Energy Select Sector SPDR"),
    "Consumer Cyclical": ("XLY", "Consumer Discretionary Select Sector SPDR"),
    "Consumer Defensive": ("XLP", "Consumer Staples Select Sector SPDR"),
    "Industrials": ("XLI", "Industrial Select Sector SPDR"),
    "Basic Materials": ("XLB", "Materials Select Sector SPDR"),
    "Utilities": ("XLU", "Utilities Select Sector SPDR"),
    "Real Estate": ("XLRE", "Real Estate Select Sector SPDR"),
    "Communication Services": ("XLC", "Communication Services Select Sector SPDR"),
}

# Benchmark index by the Yahoo symbol suffix of the listing venue ("" = a US listing).
_INDEX_BY_SUFFIX = {
    "": ("^GSPC", "S&P 500"),
    "NS": ("^NSEI", "Nifty 50"), "BO": ("^NSEI", "Nifty 50"),
    "L": ("^FTSE", "FTSE 100"), "IL": ("^FTSE", "FTSE 100"),
    "T": ("^N225", "Nikkei 225"),
    "HK": ("^HSI", "Hang Seng"),
    "DE": ("^GDAXI", "DAX"), "F": ("^GDAXI", "DAX"),
    "PA": ("^FCHI", "CAC 40"),
    "TO": ("^GSPTSE", "S&P/TSX Composite"), "V": ("^GSPTSE", "S&P/TSX Composite"),
    "TW": ("^TWII", "TAIEX"), "TWO": ("^TWII", "TAIEX"),
    "KS": ("^KS11", "KOSPI"), "KQ": ("^KS11", "KOSPI"),
    "AX": ("^AXJO", "S&P/ASX 200"),
    "SW": ("^SSMI", "SMI"),
    "AS": ("^AEX", "AEX"),
    "MC": ("^IBEX", "IBEX 35"),
    "MI": ("FTSEMIB.MI", "FTSE MIB"),
    "SA": ("^BVSP", "Bovespa"),
    "SI": ("^STI", "Straits Times"),
    "MX": ("^MXX", "IPC Mexico"),
    "ST": ("^OMX", "OMX Stockholm 30"),
    "SS": ("000001.SS", "SSE Composite"), "SZ": ("399001.SZ", "SZSE Component"),
}


def listing_suffix(symbol):
    """The Yahoo exchange suffix of a symbol ("" for a US listing, "NS" for RELIANCE.NS)."""
    s = (symbol or "").upper()
    if s.startswith("^") or "=" in s:
        return None
    return s.split(".", 1)[1] if "." in s else ""


def is_us_listing(symbol):
    return listing_suffix(symbol) == ""


def benchmark_index(symbol):
    """(index symbol, name) of the listing venue; the S&P 500 when the venue is not mapped."""
    return _INDEX_BY_SUFFIX.get(listing_suffix(symbol) or "", _INDEX_BY_SUFFIX[""])


def sector_etf(symbol, sector, currency):
    """(ETF symbol, name) of a US-listed, USD-quoted company's sector; None otherwise."""
    if not is_us_listing(symbol) or (currency or "").upper() != "USD":
        return None
    return SECTOR_ETFS.get((sector or "").strip())


# ── Correlation ──────────────────────────────────────────────────────────────


def close_frame(downloaded, symbols):
    """The adjusted closes (one column per symbol) out of a yf.download() frame."""
    import pandas as pd

    df = downloaded
    if df is None or getattr(df, "empty", True):
        return pd.DataFrame()
    if isinstance(df.columns, pd.MultiIndex):
        level0 = list(df.columns.get_level_values(0))
        if "Close" in level0:
            closes = df["Close"]
        else:
            closes = df.xs("Close", axis=1, level=1)
    else:
        closes = df[["Close"]].rename(columns={"Close": symbols[0]}) if "Close" in df.columns else pd.DataFrame()
    if isinstance(closes, pd.Series):
        closes = closes.to_frame(symbols[0])
    closes = closes.copy()
    closes.columns = [str(c).upper() for c in closes.columns]
    return closes.sort_index()


def compute_correlation(closes, entries, min_obs=40):
    """The contract's "correlation" object.

    `closes`: adjusted closes, one column per symbol (dates of every venue, NaN where a venue
    did not trade). `entries`: [(symbol, role, label)], the company first. Daily simple returns
    per symbol on its own trading days; Pearson correlation on the pairwise-complete overlapping
    days, null when fewer than `min_obs` overlap. None when the company has no history.
    """
    import numpy as np
    import pandas as pd

    present = []
    for sym, role, label in entries:
        if sym in closes.columns:
            px = pd.to_numeric(closes[sym], errors="coerce")
            px = px[px > 0].dropna()
            if len(px) >= 2:
                present.append((sym, role, label, px))
    if not present or present[0][1] != "self":
        return None

    syms = [p[0] for p in present]
    prices = {p[0]: p[3] for p in present}
    rets = pd.DataFrame({s: (prices[s] / prices[s].shift(1) - 1.0).iloc[1:] for s in syms})
    rets = rets.replace([np.inf, -np.inf], np.nan)
    corr = rets.corr(method="pearson", min_periods=min_obs)
    counts = {s: int(rets[s].count()) for s in syms}

    n = len(syms)
    matrix = [[None] * n for _ in range(n)]
    for i in range(n):
        for j in range(i, n):
            if i == j:
                v = 1.0 if counts[syms[i]] >= min_obs else None
            else:
                raw = corr.iat[i, j]
                v = round(float(raw), 4) if raw is not None and np.isfinite(raw) else None
            matrix[i][j] = v
            matrix[j][i] = v

    me = syms[0]
    beta = {}
    for s in syms:
        if s == me:
            beta[s] = 1.0 if counts[me] >= min_obs else None
            continue
        pair = rets[[me, s]].dropna()
        if len(pair) >= min_obs:
            var = float(pair[s].var(ddof=1))
            cov = float(pair[me].cov(pair[s]))
            beta[s] = round(cov / var, 4) if var > 0 and np.isfinite(cov) else None
        else:
            beta[s] = None

    returns, spark = {}, {}
    for s in syms:
        px = prices[s]
        returns[s] = round(float(px.iloc[-1] / px.iloc[0] - 1.0), 6)
        weekly = px.resample("W-FRI").last().dropna().iloc[-53:]
        base = float(weekly.iloc[0]) if len(weekly) else 0.0
        spark[s] = [round(float(v) / base * 100.0, 2) for v in weekly] if base > 0 else []

    last = prices[me].index[-1]
    return {
        "window": "1y",
        "interval": "1d",
        "as_of": last.strftime("%Y-%m-%d") if hasattr(last, "strftime") else str(last)[:10],
        "observations": counts[me],
        "symbols": syms,
        "labels": {p[0]: p[2] or p[0] for p in present},
        "roles": {p[0]: p[1] for p in present},
        "matrix": matrix,
        "returns": returns,
        "beta": beta,
        "spark": spark,
    }


# ── SEC EDGAR subsidiaries ───────────────────────────────────────────────────

SEC_USER_AGENT = "Fincept Terminal support@fincept.in"
NOT_US_FILER_NOTE = "SEC Exhibit 21 covers US filers only"
ANNUAL_FORMS = ("10-K", "20-F", "40-F")
_SEC_GAP_S = 0.15            # spacing between SEC requests (SEC allows 10/s; stay well below)
_TICKERS_MAX_AGE_S = 7 * 86400
_FILINGS_MAX_AGE_S = 86400
_EXHIBIT_MAX_AGE_S = 30 * 86400
_PARSER_VERSION = 4          # part of the exhibit cache name: a parser change re-reads the exhibit
_MAX_SUBSIDIARIES = 500

_SEC_LOCK = threading.Lock()
_SEC_LAST = [0.0]


def _cache_path(*parts):
    return fincept_paths.cache_dir("relationship_map").joinpath(*parts)


def _read_cache(path, max_age_s):
    try:
        if time.time() - os.path.getmtime(path) > max_age_s:
            return None
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return None


def _write_cache(path, obj):
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        fincept_paths.atomic_write_text(path, json.dumps(obj, ensure_ascii=True))
    except Exception:
        pass  # a cache that cannot be written only costs a download next time


def _sec_session():
    import requests

    s = requests.Session()
    s.headers.update({"User-Agent": SEC_USER_AGENT, "Accept-Encoding": "gzip, deflate"})
    return s


def _sec_get(session, url, timeout):
    with _SEC_LOCK:
        gap = _SEC_GAP_S - (time.monotonic() - _SEC_LAST[0])
        if gap > 0:
            time.sleep(gap)
        _SEC_LAST[0] = time.monotonic()
    r = session.get(url, timeout=timeout)
    r.raise_for_status()
    return r


def _cik_table(session, call, timeout):
    """{TICKER: cik} from SEC's company_tickers.json, cached 7 days (a stale copy when SEC fails)."""
    path = _cache_path("sec_company_tickers.json")
    table = _read_cache(path, _TICKERS_MAX_AGE_S)
    if table is not None:
        return table
    try:
        raw = call("subsidiaries",
                   lambda: _sec_get(session, "https://www.sec.gov/files/company_tickers.json", timeout).json(),
                   timeout)
        table = {}
        for row in (raw or {}).values():
            t, c = str(row.get("ticker") or "").upper(), row.get("cik_str")
            if t and c and t not in table:
                table[t] = int(c)
        if table:
            _write_cache(path, table)
        return table
    except Exception:
        stale = _read_cache(path, float("inf"))
        if stale is None:
            raise
        return stale


def _annual_filings(cik, session, call, timeout):
    """[{form, filed, accession}] of the latest annual reports, newest first (cached a day)."""
    path = _cache_path("sec_filings", "%010d.json" % cik)
    cached = _read_cache(path, _FILINGS_MAX_AGE_S)
    if cached is not None:
        return cached

    def pick(block):
        forms = block.get("form") or []
        accs = block.get("accessionNumber") or []
        dates = block.get("filingDate") or []
        out = []
        for i, form in enumerate(forms):
            if form in ANNUAL_FORMS and i < len(accs):
                out.append({"form": form, "filed": dates[i] if i < len(dates) else "", "accession": accs[i]})
        return out

    base = "https://data.sec.gov/submissions/"
    sub = call("subsidiaries", lambda: _sec_get(session, base + "CIK%010d.json" % cik, timeout).json(), timeout)
    filings = (sub or {}).get("filings") or {}
    found = pick(filings.get("recent") or {})
    if not found:
        # Very active filers (banks file prospectuses daily) push the annual report out of
        # the "recent" block; the next page has it.
        for extra in (filings.get("files") or [])[:1]:
            name = extra.get("name")
            if name:
                found = pick(call("subsidiaries", lambda n=name: _sec_get(session, base + n, timeout).json(), timeout) or {})
    found.sort(key=lambda f: f.get("filed") or "", reverse=True)
    found = found[:3]
    _write_cache(path, found)
    return found


def _exhibit_url(index_html, form):
    """The subsidiaries exhibit of a filing, by the index page's Type column (EX-21* in a 10-K;
    EX-8* first, then EX-21*, in a 20-F / 40-F). None when the filing has none."""
    from urllib.parse import urljoin

    from bs4 import BeautifulSoup

    soup = BeautifulSoup(index_html, "lxml")
    docs = []
    for table in soup.find_all("table"):
        if "tableFile" not in (table.get("class") or []):
            continue
        for tr in table.find_all("tr"):
            tds = tr.find_all("td")
            if len(tds) < 4:
                continue
            a = tds[2].find("a")
            if a is None or not a.get("href"):
                continue
            href = a["href"]
            if href.startswith("/ix?doc="):
                href = href[len("/ix?doc="):]
            docs.append((tds[3].get_text(" ", strip=True).upper(), urljoin("https://www.sec.gov/", href)))
    prefixes = ("EX-21",) if form.startswith("10-K") else ("EX-8", "EX-21")
    for prefix in prefixes:
        for typ, url in docs:
            if typ.startswith(prefix):
                return url
    return None


_FOOTNOTE_TAIL = re.compile(r"(\s*(\(\s*[0-9a-z]{1,2}\s*\)|\[\s*[0-9]{1,2}\s*\]|\*+|†|‡|§|¹|²|³))+$")
_HEADER = re.compile(
    r"name of (the )?(subsidiar|company|entity|corporation|registrant)|jurisdiction|state or (other )?(country|jurisdiction)"
    r"|place of (incorporation|organi[sz]ation)|country of (incorporation|organi[sz]ation)"
    r"|state of (incorporation|organi[sz]ation)|(percent|percentage|%) (of )?(ownership|owned|voting|interest)"
    r"|organi[sz]ed under|under the laws of|incorporated (in|under)$"
    r"|^(as of |as at )?(january|february|march|april|may|june|july|august|september|october|november|december)"
    r" \d{1,2}, \d{4}"
    r"|^(name|names|subsidiary|subsidiaries|entity|entities|company|companies|legal name|entity name|domicile|"
    r"location|ownership|country|state)$",
    re.I,
)
_NOT_NAME_START = (
    "exhibit", "subsidiaries of", "list of", "significant subsidiaries", "pursuant to", "the following",
    "the names of", "certain subsidiaries", "omitted", "note:", "note ", "notes:", "notes ", "indicates", "*", "(1)",
    "(a)", "†", "page ", "as of ",
)
_ENTITY = re.compile(
    r"\b(inc|incorporated|llc|l\.l\.c|ltd|limited|corp|corporation|co|company|gmbh|ag|s\.?a|s\.?a\.?s|b\.?v|n\.?v|plc|"
    r"l\.?p|llp|pte|pty|k\.?k|s\.?r\.?l|s\.?p\.?a|s\.? de r\.?l|oy|asa|a/s|spa|sarl|s\.?a\.?r\.?l|holdings?|trust|"
    r"partners|bank|fund|group|kft|sp\. z o\.o|ulc|lp|lda|ltda|bhd|sdn)\b",
    re.I,
)


def _clean_cell(text):
    s = re.sub(r"\s+", " ", (text or "").replace("\xa0", " ")).strip()
    s = s.lstrip("•·-–— ").strip()
    s = _FOOTNOTE_TAIL.sub("", s).strip()
    return s.strip(" ,;:")


def _looks_like_name(name):
    if len(name) < 2 or len(name) > 200 or len(re.findall(r"[A-Za-z]", name)) < 2:
        return False
    low = name.lower()
    if low.startswith(_NOT_NAME_START):
        return False
    if re.search(r"\bsubsidiaries of\b|\blist of subsidiaries\b|\bsignificant subsidiaries\b", low):
        return False  # the exhibit's title ("Corsair Gaming, Inc. Subsidiaries of the Registrant")
    words = name.split()
    if len(words) > 25:
        return False  # a sentence (a footnote), not a name
    if name.endswith(".") and len(words) > 10 and not _ENTITY.search(name):
        return False
    return True


def _is_header(cells):
    return len(cells) <= 5 and any(_HEADER.search(c) for c in cells)


def _row_entry(cells):
    while cells and re.fullmatch(r"\(?\d{1,4}[.)]?", cells[0]):
        cells = cells[1:]  # a numbered list: "1." | name | jurisdiction
    if not cells or _is_header(cells):
        return None
    name = cells[0]
    if not _looks_like_name(name):
        return None
    jurisdiction = ""
    for c in cells[1:]:
        if re.fullmatch(r"[\d.,%\s()\-]+", c):
            continue  # an ownership percentage
        jurisdiction = c
        break
    return {"name": name, "jurisdiction": jurisdiction}


def _line_entry(line):
    s = _clean_cell(line)
    if not s or _is_header([s]) or not _looks_like_name(s):
        return None
    m = re.match(r"^(.+?)\s*\(([^()]{2,60})\)$", s)
    if m and _ENTITY.search(m.group(1)):
        return {"name": m.group(1).strip(" ,"), "jurisdiction": m.group(2).strip()}
    m = re.match(r"^(.+?)(?:\s{2,}|\t+|\s+[-–—]\s+)([A-Z][A-Za-z .,'&()-]{1,60})$", s)
    if m and _ENTITY.search(m.group(1)):
        return {"name": m.group(1).strip(" ,"), "jurisdiction": m.group(2).strip()}
    if _ENTITY.search(s):
        return {"name": s, "jurisdiction": ""}
    return None


def parse_subsidiaries(html):
    """[{name, jurisdiction}] out of an Exhibit 21 / 20-F exhibit 8 document: its table rows
    (header rows and rows that are not entity names skipped), else its text lines."""
    from bs4 import BeautifulSoup

    soup = BeautifulSoup(html, "lxml")
    rows = []
    for tr in soup.find_all("tr"):
        if tr.find("tr") is not None:
            continue  # a row holding a nested table: its inner rows are read on their own
        cells = [_clean_cell(td.get_text(" ", strip=True)) for td in tr.find_all(["td", "th"])]
        cells = [c for c in cells if c]
        if cells:
            entry = _row_entry(cells)
            if entry:
                rows.append(entry)
    if not rows:
        for line in soup.get_text("\n").splitlines():
            entry = _line_entry(line)
            if entry:
                rows.append(entry)
    out, seen = [], set()
    for r in rows:
        key = (r["name"].lower(), r["jurisdiction"].lower())
        if key in seen:
            continue
        seen.add(key)
        out.append(r)
        if len(out) >= _MAX_SUBSIDIARIES:
            break
    return out


_SIGNIFICANT_ONLY = re.compile(
    r"601\s*\(b\)\s*\(21\)\s*\(ii\)|subsidiaries[^.]{0,160}\b(are|have been) omitted|omitted[^.]{0,120}significant subsidiar",
    re.I,
)


def _filing_exhibit(cik, filing, session, call, timeout):
    """{form, filed, accession, url, exhibit, subsidiaries} of one filing (cached 30 days)."""
    acc = filing["accession"]
    acc_nd = acc.replace("-", "")
    path = _cache_path("sec_subsidiaries", "%d_%s_p%d.json" % (cik, acc_nd, _PARSER_VERSION))
    cached = _read_cache(path, _EXHIBIT_MAX_AGE_S)
    if cached is not None:
        return cached
    base = "https://www.sec.gov/Archives/edgar/data/%d/%s/" % (cik, acc_nd)
    index_url = base + acc + "-index.htm"
    index_html = call("subsidiaries", lambda: _sec_get(session, index_url, timeout).text, timeout)
    doc = _exhibit_url(index_html or "", filing["form"])
    res = {"form": filing["form"], "filed": filing.get("filed") or "", "accession": acc, "url": index_url,
           "exhibit": False, "significant_only": False, "subsidiaries": []}
    if doc:
        res["url"] = doc
        res["exhibit"] = True
        if not doc.lower().endswith(".pdf"):
            text = call("subsidiaries", lambda: _sec_get(session, doc, timeout).text, timeout) or ""
            res["subsidiaries"] = parse_subsidiaries(text)
            # Regulation S-K lets a filer leave out subsidiaries that are not significant.
            res["significant_only"] = bool(_SIGNIFICANT_ONLY.search(re.sub(r"<[^>]+>|&nbsp;|&#160;", " ", text)))
    _write_cache(path, res)
    return res


def fetch_subsidiaries(symbol, call, timeout):
    """Subsidiaries from the latest annual report on SEC EDGAR.

    -> {"state": "ok" | "empty" | "not_applicable", "subsidiaries": [...], "filing": {...},
        "note": sentence, "as_of": filing date}
    Network failures raise (the caller turns them into the section's timeout / error state).
    """
    out = {"state": "empty", "subsidiaries": [], "filing": {}, "note": "", "as_of": ""}
    if not is_us_listing(symbol):
        out.update(state="not_applicable", note=NOT_US_FILER_NOTE)
        return out
    session = _sec_session()
    try:
        table = _cik_table(session, call, timeout)
        sym = symbol.upper()
        cik = table.get(sym) or table.get(sym.replace(".", "-")) or table.get(sym.replace("-", "."))
        if not cik:
            out.update(state="not_applicable", note=NOT_US_FILER_NOTE)
            return out
        filings = _annual_filings(int(cik), session, call, timeout)
        if not filings:
            out["note"] = "SEC EDGAR lists no annual report (10-K, 20-F or 40-F) for this company"
            return out
        first = None
        for filing in filings[:2]:
            res = _filing_exhibit(int(cik), filing, session, call, timeout)
            first = first or res
            if res["subsidiaries"]:
                out.update(state="ok", subsidiaries=res["subsidiaries"], as_of=res["filed"],
                           filing={k: res[k] for k in ("form", "filed", "accession", "url")})
                notes = []
                if res is not first:
                    notes.append("From the %s filed %s: the latest annual report has no subsidiaries list." % (
                        res["form"], res["filed"]))
                if res.get("significant_only"):
                    notes.append("The filing lists significant subsidiaries only; smaller ones are omitted as "
                                 "Regulation S-K allows.")
                out["note"] = " ".join(notes)
                return out
        out["filing"] = {k: first[k] for k in ("form", "filed", "accession", "url")}
        out["as_of"] = first["filed"]
        if first["exhibit"]:
            out["note"] = "The subsidiaries exhibit of the %s filed %s could not be read" % (first["form"], first["filed"])
        else:
            out["note"] = "The %s filed %s has no subsidiaries exhibit" % (first["form"], first["filed"])
        return out
    finally:
        try:
            session.close()
        except Exception:
            pass


def utc_now_iso():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
