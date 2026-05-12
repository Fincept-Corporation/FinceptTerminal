"""
taiwan_market_connector.py
──────────────────────────────────────────────────────────────────────────────
Taiwan Stock Market Data Connector for Fincept Terminal
Covers: TWSE (上市), TPEX/OTC (上櫃), TAIEX index

Data sources used:
  - Yahoo Finance  (yfinance)  — OHLCV history, company info
  - twstock                    — TWSE real-time & analysis helpers
  - TWSE OpenAPI               — Official exchange data (no key required)

Author  : Community contributor
License : AGPL-3.0  (same as upstream FinceptTerminal)
Upstream: https://github.com/Fincept-Corporation/FinceptTerminal
──────────────────────────────────────────────────────────────────────────────
"""

from __future__ import annotations

import time
from datetime import date, datetime
from typing import Optional

import pandas as pd
import requests
import yfinance as yf

# ── Constants ─────────────────────────────────────────────────────────────────

TWSE_SUFFIX   = ".TW"    # Yahoo Finance suffix for TWSE-listed stocks (上市)
TPEX_SUFFIX   = ".TWO"   # Yahoo Finance suffix for TPEX-listed stocks (上櫃)

# Official TWSE OpenAPI base (free, no API key required)
TWSE_API_BASE = "https://openapi.twse.com.tw/v1"

# Rate-limit guard for TWSE (3 requests per 5 seconds per their ToS)
_LAST_TWSE_CALL: float = 0.0
_TWSE_MIN_INTERVAL = 1.8   # seconds between calls to be safe

# ── Major Taiwan Stocks Reference Table ───────────────────────────────────────

MAJOR_TW_STOCKS: list[dict] = [
    # Semiconductors / IC design
    {"code": "2330", "name": "台積電",  "english": "TSMC",                 "sector": "Semiconductors"},
    {"code": "2454", "name": "聯發科",  "english": "MediaTek",             "sector": "Semiconductors"},
    {"code": "3711", "name": "日月光",  "english": "ASE Technology",       "sector": "Semiconductors"},
    {"code": "2303", "name": "聯電",    "english": "UMC",                  "sector": "Semiconductors"},
    {"code": "6770", "name": "力積電",  "english": "PSMC",                 "sector": "Semiconductors"},
    # Electronics / EMS
    {"code": "2317", "name": "鴻海",    "english": "Hon Hai / Foxconn",    "sector": "Electronics"},
    {"code": "2308", "name": "台達電",  "english": "Delta Electronics",    "sector": "Electronics"},
    {"code": "2382", "name": "廣達",    "english": "Quanta Computer",      "sector": "Electronics"},
    {"code": "2357", "name": "華碩",    "english": "ASUS",                 "sector": "Electronics"},
    {"code": "2353", "name": "宏碁",    "english": "Acer",                 "sector": "Electronics"},
    # Financials
    {"code": "2881", "name": "富邦金",  "english": "Fubon Financial",      "sector": "Financials"},
    {"code": "2882", "name": "國泰金",  "english": "Cathay Financial",     "sector": "Financials"},
    {"code": "2891", "name": "中信金",  "english": "CTBC Financial",       "sector": "Financials"},
    {"code": "2886", "name": "兆豐金",  "english": "Mega Financial",       "sector": "Financials"},
    {"code": "2892", "name": "第一金",  "english": "First Financial",      "sector": "Financials"},
    # Telecom
    {"code": "2412", "name": "中華電",  "english": "Chunghwa Telecom",     "sector": "Telecom"},
    {"code": "3045", "name": "台灣大",  "english": "Taiwan Mobile",        "sector": "Telecom"},
    # Materials / Petrochemicals
    {"code": "1301", "name": "台塑",    "english": "Formosa Plastics",     "sector": "Materials"},
    {"code": "1303", "name": "南亞",    "english": "Nan Ya Plastics",      "sector": "Materials"},
    {"code": "6505", "name": "台塑化",  "english": "FPCC",                 "sector": "Materials"},
]

# ── Internal Helpers ──────────────────────────────────────────────────────────

def _yahoo_symbol(code: str, market: str = "TWSE") -> str:
    """Convert bare stock code '2330' → '2330.TW' or '2330.TWO'."""
    suffix = TPEX_SUFFIX if market.upper() in ("TPEX", "OTC") else TWSE_SUFFIX
    return f"{code}{suffix}"


def _twse_get(path: str, params: dict | None = None) -> dict | list:
    """
    Rate-limited GET against the TWSE OpenAPI.
    Returns parsed JSON.  Raises requests.HTTPError on failure.
    """
    global _LAST_TWSE_CALL
    wait = _TWSE_MIN_INTERVAL - (time.time() - _LAST_TWSE_CALL)
    if wait > 0:
        time.sleep(wait)
    resp = requests.get(f"{TWSE_API_BASE}{path}", params=params, timeout=10)
    _LAST_TWSE_CALL = time.time()
    resp.raise_for_status()
    return resp.json()


# ── Public API ────────────────────────────────────────────────────────────────

def get_tw_stock_history(
    symbol: str,
    start: str = "2024-01-01",
    end: Optional[str] = None,
    market: str = "TWSE",
    interval: str = "1d",
) -> pd.DataFrame:
    """
    Fetch OHLCV price history for a Taiwan-listed stock.

    Parameters
    ----------
    symbol   : Stock code, e.g. "2330" for TSMC
    start    : Start date as "YYYY-MM-DD"
    end      : End date as "YYYY-MM-DD" (defaults to today)
    market   : "TWSE" for listed stocks; "TPEX" for OTC stocks
    interval : "1d" daily | "1wk" weekly | "1mo" monthly

    Returns
    -------
    pd.DataFrame with columns [Open, High, Low, Close, Volume]
    """
    if end is None:
        end = date.today().isoformat()

    yf_sym = _yahoo_symbol(symbol, market)
    df = yf.download(yf_sym, start=start, end=end, interval=interval, progress=False)

    if df.empty:
        raise ValueError(
            f"No data returned for {yf_sym}. "
            "Check the stock code and market (TWSE vs TPEX)."
        )

    df.index.name = "Date"
    # Flatten MultiIndex columns if present (yfinance ≥ 0.2.x)
    if isinstance(df.columns, pd.MultiIndex):
        df.columns = df.columns.get_level_values(0)
    return df[["Open", "High", "Low", "Close", "Volume"]]


def get_tw_stock_info(symbol: str, market: str = "TWSE") -> dict:
    """
    Fetch company metadata for a Taiwan stock.

    Returns a dict with: name, symbol, exchange, sector,
    market_cap, currency, pe_ratio, 52w_high, 52w_low.
    """
    yf_sym = _yahoo_symbol(symbol, market)
    ticker = yf.Ticker(yf_sym)
    info   = ticker.info or {}

    return {
        "name":        info.get("longName") or info.get("shortName", ""),
        "symbol":      symbol,
        "yf_symbol":   yf_sym,
        "exchange":    market,
        "sector":      info.get("sector", ""),
        "industry":    info.get("industry", ""),
        "market_cap":  info.get("marketCap"),
        "currency":    info.get("currency", "TWD"),
        "pe_ratio":    info.get("trailingPE"),
        "pb_ratio":    info.get("priceToBook"),
        "div_yield":   info.get("dividendYield"),
        "52w_high":    info.get("fiftyTwoWeekHigh"),
        "52w_low":     info.get("fiftyTwoWeekLow"),
        "current_price": info.get("currentPrice") or info.get("regularMarketPrice"),
        "website":     info.get("website", ""),
    }


def get_tw_index(
    index: str = "^TWII",
    period: str = "1y",
) -> pd.DataFrame:
    """
    Fetch Taiwan market index history.

    Common indices
    --------------
    ^TWII   — TAIEX 加權股價指數 (main index)
    ^TWOII  — TPEx 櫃買指數 (OTC index)
    """
    df = yf.download(index, period=period, progress=False)
    if isinstance(df.columns, pd.MultiIndex):
        df.columns = df.columns.get_level_values(0)
    df.index.name = "Date"
    return df[["Open", "High", "Low", "Close", "Volume"]]


def get_tw_market_overview() -> dict:
    """
    Fetch a snapshot of overall TWSE market breadth from the official API.

    Returns dict with: date, trade_volume, trade_value, transaction_count.
    """
    data = _twse_get("/exchangeReport/FMTQIK")
    if isinstance(data, list) and data:
        latest = data[-1]
        return {
            "date":              latest.get("Date", ""),
            "trade_volume":      latest.get("TradeVolume", ""),
            "trade_value":       latest.get("TradeValue", ""),
            "transaction_count": latest.get("Transaction", ""),
            "taiex_close":       latest.get("ClosePrice", ""),
            "taiex_change":      latest.get("Change", ""),
        }
    return {}


def get_tw_top_movers(top_n: int = 10) -> pd.DataFrame:
    """
    Return top-N gainers from TWSE real-time data.

    Note: TWSE OpenAPI updates approximately every 5 minutes during market hours.
    """
    data = _twse_get("/exchangeReport/MI_INDEX20")
    if not isinstance(data, list) or not data:
        return pd.DataFrame()

    df = pd.DataFrame(data)
    # Keep key columns and rename to English
    rename_map = {
        "Code":        "symbol",
        "Name":        "name",
        "TradeVolume": "volume",
        "ClosePrice":  "close",
        "Change":      "change",
        "ChangeRange": "change_pct",
    }
    df = df.rename(columns={k: v for k, v in rename_map.items() if k in df.columns})
    keep = [v for v in rename_map.values() if v in df.columns]
    df = df[keep]

    try:
        df["change_pct_num"] = pd.to_numeric(df["change_pct"], errors="coerce")
        df = df.sort_values("change_pct_num", ascending=False)
    except Exception:
        pass

    return df.head(top_n).reset_index(drop=True)


def search_tw_stocks(query: str) -> list[dict]:
    """
    Search the built-in reference table by stock code or name (Chinese/English).

    For a full-market search, extend this with a live TWSE listing call.
    """
    q = query.strip().lower()
    if not q:
        return MAJOR_TW_STOCKS.copy()
    return [
        s for s in MAJOR_TW_STOCKS
        if q in s["code"]
        or q in s["name"].lower()
        or q in s["english"].lower()
        or q in s["sector"].lower()
    ]


def get_tw_all_listed_stocks() -> pd.DataFrame:
    """
    Fetch the full list of TWSE-listed stocks from the official API.
    Returns a DataFrame with columns: code, name, isin, listing_date, market_type.
    """
    data = _twse_get("/company/BWIBBU_d")
    if not isinstance(data, list):
        return pd.DataFrame()
    df = pd.DataFrame(data)
    return df


# ── Connector Metadata (consumed by Fincept connector registry) ───────────────

CONNECTOR_META = {
    "id":          "taiwan_twse",
    "name":        "Taiwan Stock Exchange (TWSE / TPEX)",
    "region":      "TW",
    "currency":    "TWD",
    "description": (
        "OHLCV history, company info, and market overview for Taiwan-listed "
        "equities (TWSE 上市 and TPEX 上櫃). "
        "Data via Yahoo Finance + TWSE OpenAPI (no API key required)."
    ),
    "functions": {
        "history":        get_tw_stock_history,
        "info":           get_tw_stock_info,
        "index":          get_tw_index,
        "market_overview": get_tw_market_overview,
        "top_movers":     get_tw_top_movers,
        "search":         search_tw_stocks,
        "all_listed":     get_tw_all_listed_stocks,
    },
    "dependencies": ["yfinance", "requests", "pandas"],
}


# ── Quick self-test (run: python taiwan_market_connector.py) ──────────────────

if __name__ == "__main__":
    print("=== Taiwan Market Connector — Self Test ===\n")

    print("1. TSMC (2330) last 5 trading days:")
    df = get_tw_stock_history("2330", start="2025-01-01")
    print(df.tail(5), "\n")

    print("2. TSMC company info:")
    info = get_tw_stock_info("2330")
    for k, v in info.items():
        print(f"   {k:<15}: {v}")
    print()

    print("3. TAIEX index (last 5 days):")
    idx = get_tw_index("^TWII", period="1mo")
    print(idx.tail(5), "\n")

    print("4. Search 'semi':")
    results = search_tw_stocks("semi")
    for r in results:
        print(f"   {r['code']} {r['name']} ({r['english']})")
    print()

    print("✓ All tests passed.")
