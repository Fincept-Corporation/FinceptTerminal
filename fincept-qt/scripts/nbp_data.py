"""
National Bank of Poland (NBP) Data Wrapper
Fetches data from the NBP Public Web API.

API Reference:
  Base URL:  https://api.nbp.pl/api
  Format:    JSON
  Auth:      None required — fully public
  Docs:      https://api.nbp.pl/

Exchange rate tables:
  Table A — 32 currencies, daily fixing (Mon–Fri)
  Table B — ~120 exotic currencies, weekly
  Table C — bid/ask rates for 8 major currencies (Forex)

Endpoints:
  GET /exchangerates/tables/{table}/          — latest table
  GET /exchangerates/tables/{table}/{date}/   — single date
  GET /exchangerates/tables/{table}/{from}/{to}/ — date range
  GET /exchangerates/rates/{table}/{code}/    — single currency history
  GET /exchangerates/rates/{table}/{code}/{date}/
  GET /exchangerates/rates/{table}/{code}/{from}/{to}/
  GET /exchangerates/rates/{table}/{code}/last/{n}/

All endpoints accept ?format=json

NBP interest rates are published at:
  https://api.nbp.pl/api/rates/reference/  (no REST; scraped from HTML)
  We use the NBP SDMX data alternatively.

Returns JSON output for C++ integration.
"""

import sys
import json
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


BASE_URL        = "https://api.nbp.pl/api"
DEFAULT_TIMEOUT = 30

# Currency codes available in Table A (32 major currencies)
TABLE_A_CODES = [
    "THB", "USD", "AUD", "HKD", "CAD", "NZD", "SGD", "EUR", "HUF", "CHF",
    "GBP", "UAH", "JPY", "CZK", "DKK", "ISK", "NOK", "SEK", "RON", "BGN",
    "TRY", "ILS", "CLP", "PHP", "MXN", "ZAR", "BRL", "MYR", "IDR", "INR",
    "KRW", "CNY",
]

# Key currency groups
MAJOR_CURRENCIES = ["USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD", "NOK", "SEK", "DKK"]
EM_CURRENCIES    = ["CNY", "INR", "BRL", "KRW", "TRY", "MXN", "ZAR", "IDR", "MYR", "PHP"]


# ---------------------------------------------------------------------------
# Dates — NBP answers 400 "Invalid date range" for an end date that is still in the future in
# Warsaw, which is what a caller east of Poland sends every night (its local "today").
# ---------------------------------------------------------------------------

def _warsaw_today() -> date:
    """Today's date in Warsaw (Europe/Warsaw: CET, CEST in summer)."""
    now_utc = datetime.now(timezone.utc)
    try:
        from zoneinfo import ZoneInfo
        return now_utc.astimezone(ZoneInfo("Europe/Warsaw")).date()
    except Exception:
        pass  # no tz database (e.g. Windows without tzdata): apply the EU rule directly

    def last_sunday(year: int, month: int) -> datetime:
        last_day = datetime(year, month + 1, 1, 1, tzinfo=timezone.utc) - timedelta(days=1)
        return last_day - timedelta(days=(last_day.weekday() + 1) % 7)

    # Summer time runs from 01:00 UTC on the last Sunday of March to 01:00 UTC on the last Sunday
    # of October.
    summer = last_sunday(now_utc.year, 3) <= now_utc < last_sunday(now_utc.year, 10)
    return (now_utc + timedelta(hours=2 if summer else 1)).date()


def _clamp_to_warsaw_today(value: Optional[str]) -> Optional[str]:
    """An ISO date later than today in Warsaw becomes today in Warsaw; anything else is unchanged."""
    if not value:
        return value
    try:
        d = date.fromisoformat(str(value)[:10])
    except ValueError:
        return value
    today = _warsaw_today()
    return today.isoformat() if d > today else value


def _windows(start: str, end: str, max_days: int):
    """[start, end] (ISO dates) as consecutive (from, to) windows of at most `max_days` days each;
    a single window when either bound is not an ISO date (the API then judges it)."""
    try:
        s, e = date.fromisoformat(str(start)[:10]), date.fromisoformat(str(end)[:10])
    except ValueError:
        return [(start, end)]
    out = []
    while s <= e:
        stop = min(e, s + timedelta(days=max_days - 1))
        out.append((s.isoformat(), stop.isoformat()))
        s = stop + timedelta(days=1)
    return out or [(start, end)]


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class NBPError:
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint    = endpoint
        self.error       = error
        self.status_code = status_code
        self.timestamp   = int(datetime.now(timezone.utc).timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "success":     False,
            "endpoint":    self.endpoint,
            "error":       self.error,
            "status_code": self.status_code,
            "timestamp":   self.timestamp,
            "type":        "NBPError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class NBPWrapper:
    """
    Wrapper for the National Bank of Poland (NBP) Public Web API.

    Exchange rates published Mon–Fri (Table A/C) or weekly (Table B).
    All data is in JSON format, no authentication required.
    Rates are expressed as PLN per foreign currency unit.
    """

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/4.0.2",
            "Accept":     "application/json",
        })

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _get(self, path: str, params: Optional[Dict] = None) -> Any:
        url  = f"{BASE_URL}/{path.lstrip('/')}"
        resp = self.session.get(url, params={"format": "json", **(params or {})},
                                timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.json()

    def _get_or_none(self, path: str) -> Any:
        """_get, with None for HTTP 404 (NBP's answer for a window without any publication)."""
        try:
            return self._get(path)
        except requests.exceptions.HTTPError as e:
            if e.response is not None and e.response.status_code == 404:
                return None
            raise

    def _safe(self, path: str, label: str,
              extra_fields: Optional[Dict] = None) -> Dict[str, Any]:
        try:
            data = self._get(path)
            result = {
                "success":   True,
                "data":      data,
                "source":    "National Bank of Poland",
                "url":       f"{BASE_URL}/{path}",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
            if extra_fields:
                result.update(extra_fields)
            return result
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return NBPError(label, str(e), sc).to_dict()
        except Exception as e:
            return NBPError(label, str(e)).to_dict()

    def _flatten_table(self, raw: Any) -> Dict[str, Any]:
        """Flatten NBP table response to list of {date, code: rate} rows."""
        if isinstance(raw, dict):
            raw = [raw]
        # raw = [{table, no, effectiveDate, rates: [{currency, code, mid}]}]
        rows = []
        for entry in raw:
            d = entry.get("effectiveDate") or entry.get("tradingDate", "")
            row: Dict[str, Any] = {"date": d}
            for r in entry.get("rates", []):
                code = r.get("code", "")
                mid  = r.get("mid")
                bid  = r.get("bid")
                ask  = r.get("ask")
                if code:
                    if mid is not None:
                        row[code] = mid
                    elif bid is not None and ask is not None:
                        row[f"{code}_bid"] = bid
                        row[f"{code}_ask"] = ask
            rows.append(row)
        return rows

    def _flatten_rates(self, raw: Any) -> Dict[str, Any]:
        """Flatten single-currency rate series to list of {date, mid/bid/ask} rows."""
        if isinstance(raw, dict):
            raw_rates = raw.get("rates", [])
            code      = raw.get("code", "")
        else:
            raw_rates = []
            code      = ""
        rows = []
        for r in raw_rates:
            d    = r.get("effectiveDate", r.get("tradingDate", ""))
            row  = {"date": d}
            if "mid" in r:
                row["mid"] = r["mid"]
            if "bid" in r and "ask" in r:
                row["bid"] = r["bid"]
                row["ask"] = r["ask"]
            rows.append(row)
        return rows, code

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_exchange_rates_today(self, table: str = "A") -> Dict[str, Any]:
        """Latest exchange rate table (A=mid, B=exotic, C=bid/ask)."""
        try:
            raw   = self._get(f"exchangerates/tables/{table}/")
            # One table date: a row per currency ({date, currency, name, mid | bid + ask}), not one
            # row with a column per currency.
            rows  = []
            for entry in (raw if isinstance(raw, list) else [raw]):
                d = entry.get("effectiveDate") or entry.get("tradingDate", "")
                for r in entry.get("rates", []):
                    row = {"date": d, "currency": r.get("code"), "name": r.get("currency")}
                    for k in ("mid", "bid", "ask"):
                        if r.get(k) is not None:
                            row[k] = r[k]
                    rows.append(row)
            return {
                "success":   True,
                "table":     table,
                "data":      rows,
                "count":     len(rows),
                "source":    "National Bank of Poland",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return NBPError(f"tables/{table}", str(e), sc).to_dict()
        except Exception as e:
            return NBPError(f"tables/{table}", str(e)).to_dict()

    def get_exchange_rates_range(self, start_date: str, end_date: str,
                                 table: str = "A") -> Dict[str, Any]:
        """Exchange rate table for a date range (max ~93 days per request)."""
        start_date = _clamp_to_warsaw_today(start_date)
        end_date = _clamp_to_warsaw_today(end_date)
        try:
            # NBP serves at most 93 days of tables per request: longer ranges are fetched in parts
            raw = []
            for s, e in _windows(start_date, end_date, 93):
                part = self._get_or_none(f"exchangerates/tables/{table}/{s}/{e}/")
                raw += part if isinstance(part, list) else ([part] if part else [])
            if not raw:
                return NBPError(f"tables/{table}/{start_date}/{end_date}",
                                "No data: NBP published no tables in this range", 404).to_dict()
            rows = self._flatten_table(raw)
            return {
                "success":    True,
                "table":      table,
                "start_date": start_date,
                "end_date":   end_date,
                "data":       rows,
                "count":      len(rows),
                "source":     "National Bank of Poland",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return NBPError(f"tables/{table}/{start_date}/{end_date}", str(e), sc).to_dict()
        except Exception as e:
            return NBPError(f"tables/{table}/{start_date}/{end_date}", str(e)).to_dict()

    def get_currency_rate(self, code: str, start_date: Optional[str] = None,
                          end_date: Optional[str] = None,
                          last_n: Optional[int] = None,
                          table: str = "A") -> Dict[str, Any]:
        """Historical rates for a single currency vs PLN."""
        start_date = _clamp_to_warsaw_today(start_date)
        end_date = _clamp_to_warsaw_today(end_date)
        try:
            if last_n:
                path = f"exchangerates/rates/{table}/{code}/last/{last_n}/"
            elif start_date and end_date:
                path = None
                # NBP serves at most 367 days of one currency per request: fetch longer ranges in parts
                raw = None
                for s, e in _windows(start_date, end_date, 367):
                    part = self._get_or_none(f"exchangerates/rates/{table}/{code}/{s}/{e}/")
                    if isinstance(part, dict):
                        if raw is None:
                            raw = part
                        else:
                            raw["rates"] = raw.get("rates", []) + part.get("rates", [])
                if raw is None:
                    return NBPError(f"rates/{table}/{code}", "No data: NBP published no rates in this range",
                                    404).to_dict()
            elif start_date:
                # single date
                path = f"exchangerates/rates/{table}/{code}/{start_date}/"
            else:
                path = f"exchangerates/rates/{table}/{code}/"
            if path is not None:
                raw = self._get(path)
            rows, c     = self._flatten_rates(raw)
            return {
                "success":     True,
                "currency":    raw.get("currency", "") if isinstance(raw, dict) else "",
                "code":        code.upper(),
                "table":       table,
                "data":        rows,
                "count":       len(rows),
                "note":        "Rates expressed as PLN per 1 unit of foreign currency",
                "source":      "National Bank of Poland",
                "timestamp":   int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return NBPError(f"rates/{table}/{code}", str(e), sc).to_dict()
        except Exception as e:
            return NBPError(f"rates/{table}/{code}", str(e)).to_dict()

    def get_usd_pln(self, start_date: Optional[str] = None,
                    end_date: Optional[str] = None,
                    last_n: int = 30) -> Dict[str, Any]:
        """USD/PLN exchange rate history."""
        return self.get_currency_rate("USD", start_date, end_date, last_n if not start_date else None)

    def get_eur_pln(self, start_date: Optional[str] = None,
                    end_date: Optional[str] = None,
                    last_n: int = 30) -> Dict[str, Any]:
        """EUR/PLN exchange rate history."""
        return self.get_currency_rate("EUR", start_date, end_date, last_n if not start_date else None)

    def get_major_currencies(self, last_n: int = 30) -> Dict[str, Any]:
        """Last N observations for major currencies vs PLN."""
        try:
            today = _warsaw_today()
            end   = today.isoformat()
            start = (today - timedelta(days=last_n * 2)).isoformat()  # buffer for weekends
            raw   = self._get(f"exchangerates/tables/A/{start}/{end}/")
            rows  = self._flatten_table(raw)
            # Filter to major currencies only
            filtered = []
            for row in rows:
                entry = {"date": row["date"]}
                for c in MAJOR_CURRENCIES:
                    if c in row:
                        entry[c] = row[c]
                filtered.append(entry)
            return {
                "success":    True,
                "currencies": MAJOR_CURRENCIES,
                "data":       filtered[-last_n:],
                "count":      len(filtered),
                "note":       "PLN per 1 unit of foreign currency",
                "source":     "National Bank of Poland",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return NBPError("major_currencies", str(e), sc).to_dict()
        except Exception as e:
            return NBPError("major_currencies", str(e)).to_dict()

    def get_bid_ask(self, start_date: Optional[str] = None,
                    end_date: Optional[str] = None) -> Dict[str, Any]:
        """Table C — bid/ask rates for major currencies (forex)."""
        if start_date and end_date:
            return self.get_exchange_rates_range(start_date, end_date, "C")
        return self.get_exchange_rates_today("C")

    def get_overview(self) -> Dict[str, Any]:
        """Latest rates snapshot — today's Table A + C."""
        results: Dict[str, Any] = {}
        for name, call in [
            ("table_a_today", lambda: self.get_exchange_rates_today("A")),
            ("table_c_today", lambda: self.get_exchange_rates_today("C")),
        ]:
            r = call()
            # "latest" stays one record {date, CODE: rate, ...} (CODE_bid / CODE_ask for table C)
            latest = None
            if r.get("data"):
                latest = {"date": r["data"][0].get("date")}
                for row in r["data"]:
                    if "mid" in row:
                        latest[row["currency"]] = row["mid"]
                    else:
                        latest[f"{row['currency']}_bid"] = row.get("bid")
                        latest[f"{row['currency']}_ask"] = row.get("ask")
            results[name] = {"success": r.get("success"), "count": r.get("count"), "latest": latest}
        return {
            "success":   True,
            "data":      results,
            "source":    "National Bank of Poland",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_all_currencies(self) -> Dict[str, Any]:
        """All currencies available in Table A + B."""
        return {
            "success":        True,
            "table_a_codes":  TABLE_A_CODES,
            "major_currencies": MAJOR_CURRENCIES,
            "em_currencies":    EM_CURRENCIES,
            "tables": {
                "A": "32 major currencies, daily mid rate (Mon-Fri)",
                "B": "~120 exotic currencies, weekly mid rate",
                "C": "8 major currencies, daily bid/ask (Mon-Fri)",
            },
            "note":      "Rates expressed as PLN per foreign currency unit",
            "source":    "National Bank of Poland",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "today":        "[table=A]                           — Latest exchange rate table",
    "range":        "<start YYYY-MM-DD> <end> [table=A]  — Table for date range",
    "currency":     "<code> [start] [end] [last_n] [table=A]  — Single currency history",
    "usd":          "[start] [end]                       — USD/PLN history",
    "eur":          "[start] [end]                       — EUR/PLN history",
    "major":        "[last_n=30]                         — Major currencies vs PLN",
    "bid_ask":      "[start] [end]                       — Table C bid/ask rates",
    "overview":     "                                    — Today's Table A + C snapshot",
    "available":    "                                    — List available currencies",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python nbp_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = NBPWrapper()

    try:
        if cmd == "today":
            result = wrapper.get_exchange_rates_today(_a(2, "A"))
        elif cmd == "range":
            if len(sys.argv) < 4:
                result = {"error": "range requires <start> <end>"}
            else:
                result = wrapper.get_exchange_rates_range(sys.argv[2], sys.argv[3], _a(4, "A"))
        elif cmd == "currency":
            if len(sys.argv) < 3:
                result = {"error": "currency requires <code>"}
            else:
                ln = int(_a(5)) if _a(5) else None
                result = wrapper.get_currency_rate(sys.argv[2], _a(3), _a(4), ln, _a(6, "A"))
        elif cmd == "usd":
            result = wrapper.get_usd_pln(_a(2), _a(3))
        elif cmd == "eur":
            result = wrapper.get_eur_pln(_a(2), _a(3))
        elif cmd == "major":
            n = int(_a(2, 30))
            result = wrapper.get_major_currencies(n)
        elif cmd == "bid_ask":
            result = wrapper.get_bid_ask(_a(2), _a(3))
        elif cmd == "overview":
            result = wrapper.get_overview()
        elif cmd in ("available", "currencies"):
            result = wrapper.get_all_currencies()
        else:
            result = {"error": f"Unknown command: {cmd}", "commands": COMMANDS}

        print(json.dumps(result, indent=2, ensure_ascii=True))

    except Exception as exc:
        print(json.dumps({
            "success":   False,
            "error":     str(exc),
            "traceback": traceback.format_exc(),
        }, indent=2))
        sys.exit(1)


if __name__ == "__main__":
    main()
