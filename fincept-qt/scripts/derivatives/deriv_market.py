"""Free market data for the Derivatives tab (no API key, nothing paid).

Commands (one JSON object on stdout, always exit code 0 for handled outcomes):

  deriv_market.py chain <SYMBOL> [--source auto|cboe|yahoo] [--max-expiries N]
      Listed option chain, DELAYED quotes. Primary source: Cboe's public delayed-quote
      feed (cdn.cboe.com, about 15 minutes delayed, no key); fallback: Yahoo Finance
      through yfinance (delayed; one request per expiry, so at most N expiries).
  deriv_market.py treasury_curve [--year YYYY]
      U.S. Treasury daily par yield curve (CMT), home.treasury.gov, public domain.
  deriv_market.py futures_curve <ROOT> [--months N]
      Futures contract months of a root (CL, GC, ES ...) from Yahoo Finance.

Output shapes (compact on purpose: a 30k-contract SPX chain stays near 2 MB):

  chain ok:   {"ok": true, "kind": "chain", "symbol", "source": "cboe"|"yahoo",
               "source_label", "delayed": true, "fetched_at" (UTC ISO),
               "underlying": {"price", "bid", "ask", "prev_close", "change", "change_pct",
                              "iv30" (percent, Cboe only), "currency", "quote_time"
                              (exchange local "YYYY-MM-DDTHH:MM:SS"), "quote_tz",
                              "market_state" (Yahoo only)},
               "expirations": ["YYYY-MM-DD", ...],
               "columns": ["e","k","c","bid","ask","last","vol","oi","iv","delta","gamma","theta","vega"],
               "rows": [[expiry index, strike, 1 call / 0 put, ...], ...]   (null = not quoted),
               "count", "notes": [...], "fallback_reason": "...", "partial": {"loaded", "total"}}
  failure:    {"ok": false, "status": "no_options"|"feed_unavailable"|"bad_symbol"|"bad_request",
               "message": "<plain sentence>", "symbol"}
  The key "error" is deliberately not used: the terminal's runner treats it as a failed
  run and drops the status, which the screen needs to pick the right empty state.
"""

import contextlib
import json
import math
import os
import re
import sys
import time
from datetime import date, datetime, timedelta, timezone

_SCRIPTS_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
if _SCRIPTS_DIR not in sys.path:
    sys.path.append(_SCRIPTS_DIR)
try:
    import fincept_json
except ImportError:  # pragma: no cover - run from an unusual folder
    fincept_json = None


def quiet_stdout():
    """Library prints (yfinance's failed-symbol lines) go to stderr: stdout carries one JSON object.

    fincept_json's guard is thread-safe and re-entrant (yf.download runs worker threads);
    contextlib.redirect_stdout is the fallback when the shared module is not on the path.
    """
    if fincept_json is not None:
        return fincept_json.stdout_to_stderr()
    return contextlib.redirect_stdout(sys.stderr)  # pragma: no cover


UA ={"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal)", "Accept": "application/json"}
CBOE_URL = "https://cdn.cboe.com/api/global/delayed_quotes/options/{}.json"
# Index option roots Cboe lists under a leading underscore.
CBOE_INDEX = {"SPX", "XSP", "NDX", "RUT", "VIX", "DJX", "OEX", "XEO", "MRUT", "MXEF", "MXEA", "SPEU", "NDXE", "RUTE"}
# Yahoo index tickers whose listed options trade under another root.
YAHOO_TO_CBOE = {"GSPC": "SPX", "SPX": "SPX", "NDX": "NDX", "RUT": "RUT", "VIX": "VIX", "DJI": "DJX", "XSP": "XSP",
                 "OEX": "OEX", "XEO": "XEO"}
CHAIN_COLUMNS = ["e", "k", "c", "bid", "ask", "last", "vol", "oi", "iv", "delta", "gamma", "theta", "vega"]
OCC = re.compile(r"^(?P<root>.+?)(?P<exp>\d{6})(?P<cp>[CP])(?P<strike>\d{8})$")


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), allow_nan=False))
    sys.stdout.write("\n")
    sys.stdout.flush()


def fail(status, message, **extra):
    out = {"ok": False, "status": status, "message": message}
    out.update(extra)
    return out


def num(v, digits=4):
    """A finite float rounded to `digits`, else None."""
    if v is None or isinstance(v, bool):
        return None
    try:
        f = float(v)
    except (TypeError, ValueError):
        return None
    if not math.isfinite(f):
        return None
    return round(f, digits)


def whole(v):
    f = num(v, 0)
    return None if f is None else int(f)


def now_utc_iso():
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


# ── Cboe ────────────────────────────────────────────────────────────────────


def cboe_base(symbol):
    s = symbol.strip().upper()
    if s.startswith("^"):
        s = YAHOO_TO_CBOE.get(s[1:], s[1:])
    return s


def cboe_chain(symbol):
    import requests

    base = cboe_base(symbol)
    if "." in base and base.split(".")[-1] not in ("A", "B", "C"):
        # Exchange suffixes (RELIANCE.NS, VOD.L): no US-listed options.
        return fail("no_options", f"No US-listed options for {symbol}. Options are available for US-listed "
                                  "stocks, ETFs and indices.", symbol=symbol)
    if not base or not re.match(r"^[A-Z][A-Z0-9./-]{0,11}$", base):
        return fail("bad_symbol", f"'{symbol}' is not a symbol Cboe lists.", symbol=symbol)
    candidates = (["_" + base, base] if base in CBOE_INDEX else [base, "_" + base])
    body = None
    last_status = None
    for cand in candidates:
        try:
            r = requests.get(CBOE_URL.format(cand), headers=UA, timeout=25)
        except requests.exceptions.RequestException as e:
            return fail("feed_unavailable", f"Cboe delayed quotes could not be reached ({type(e).__name__}).",
                        symbol=symbol)
        last_status = r.status_code
        if r.status_code == 200:
            try:
                body = r.json()
            except ValueError:
                return fail("feed_unavailable", "Cboe returned an unreadable reply.", symbol=symbol)
            break
        if r.status_code not in (403, 404):
            return fail("feed_unavailable", f"Cboe delayed quotes answered HTTP {r.status_code}.", symbol=symbol)
    if body is None:
        # The CDN answers 403 / 404 for symbols it has no options for.
        return fail("no_options", f"Cboe lists no options for {base} (HTTP {last_status}).", symbol=symbol)

    data = body.get("data") if isinstance(body, dict) else None
    if not isinstance(data, dict) or not data.get("options"):
        return fail("no_options", f"Cboe lists no options for {base}.", symbol=symbol)

    qt = str(data.get("last_trade_time") or "")
    try:
        quote_dt = datetime.strptime(qt[:19], "%Y-%m-%dT%H:%M:%S")
    except ValueError:
        quote_dt = None
    quote_date = quote_dt.date() if quote_dt else date.today()
    after_close = bool(quote_dt and (quote_dt.hour, quote_dt.minute) >= (16, 0))

    allowed = {base, base + "W", base + "P"}
    by_key = {}
    dropped_roots = {}
    expired = 0
    dupes = 0
    for o in data["options"]:
        m = OCC.match(str(o.get("option", "")))
        if not m:
            continue
        root = m.group("root")
        if root not in allowed:
            dropped_roots[root] = dropped_roots.get(root, 0) + 1
            continue
        try:
            exp = datetime.strptime(m.group("exp"), "%y%m%d").date()
        except ValueError:
            continue
        if exp < quote_date or (exp == quote_date and after_close):
            expired += 1
            continue
        strike = int(m.group("strike")) / 1000.0
        is_call = m.group("cp") == "C"
        key = (exp, strike, is_call)
        oi = whole(o.get("open_interest")) or 0
        if key in by_key:
            dupes += 1
            if oi <= (by_key[key][1] or 0):
                continue
        bid, ask = num(o.get("bid")), num(o.get("ask"))
        iv = num(o.get("iv"), 6)
        by_key[key] = (o, oi, bid, ask, iv)

    if not by_key:
        return fail("no_options", f"Cboe lists no current options for {base}.", symbol=symbol)

    expirations = sorted({k[0] for k in by_key})
    exp_index = {e: i for i, e in enumerate(expirations)}
    rows = []
    for (exp, strike, is_call) in sorted(by_key):
        o, oi, bid, ask, iv = by_key[(exp, strike, is_call)]
        rows.append([
            exp_index[exp], strike, 1 if is_call else 0,
            bid if bid and bid > 0 else None,
            ask if ask and ask > 0 else None,
            num(o.get("last_trade_price")) or None,
            whole(o.get("volume")) or 0,
            oi,
            iv if iv and iv > 0 else None,
            num(o.get("delta"), 5), num(o.get("gamma"), 6), num(o.get("theta"), 5), num(o.get("vega"), 5),
        ])

    notes = []
    if dropped_roots:
        notes.append("Left out {} contracts with non-standard roots ({}).".format(
            sum(dropped_roots.values()), ", ".join(sorted(dropped_roots))))
    if dupes:
        notes.append(f"Merged {dupes} contracts listed under two roots (AM / PM settlement); kept the one with more open interest.")
    if expired:
        notes.append(f"Dropped {expired} contracts that had already expired at the quote time.")

    price = num(data.get("current_price"))
    if not price:
        b, a = num(data.get("bid")), num(data.get("ask"))
        price = round((b + a) / 2, 4) if b and a else num(data.get("close"))
    return {
        "ok": True, "kind": "chain", "symbol": base, "source": "cboe",
        "source_label": "Cboe delayed quotes", "delayed": True, "fetched_at": now_utc_iso(),
        "underlying": {
            "price": price, "bid": num(data.get("bid")), "ask": num(data.get("ask")),
            "prev_close": num(data.get("prev_day_close")), "change": num(data.get("price_change")),
            "change_pct": num(data.get("price_change_percent")), "iv30": num(data.get("iv30")),
            "currency": "USD", "quote_time": qt[:19], "quote_tz": "America/New_York",
        },
        "expirations": [e.isoformat() for e in expirations],
        "columns": CHAIN_COLUMNS, "rows": rows, "count": len(rows), "notes": notes,
    }


# ── Yahoo (fallback) ────────────────────────────────────────────────────────


def yahoo_chain(symbol, max_expiries, budget_s=45.0):
    try:
        import yfinance as yf
    except Exception:
        return fail("feed_unavailable", "The Yahoo Finance library is not installed.", symbol=symbol)
    s = symbol.strip().upper()
    if s in YAHOO_TO_CBOE.values() and not s.startswith("^"):
        s = "^" + s  # Yahoo lists index options under ^SPX, ^NDX ...
    started = time.monotonic()
    try:
        t = yf.Ticker(s)
        with quiet_stdout():
            expiries = list(t.options or [])
    except Exception as e:
        return fail("feed_unavailable", f"Yahoo Finance could not be reached ({type(e).__name__}).", symbol=symbol)
    if not expiries:
        return fail("no_options", f"Yahoo Finance lists no options for {symbol}.", symbol=symbol)
    today = date.today()
    expiries = [e for e in expiries if e >= today.isoformat()]
    total = len(expiries)
    wanted = expiries[:max(1, max_expiries)]
    rows = []
    loaded = []
    underlying = {}
    for e in wanted:
        if time.monotonic() - started > budget_s:
            break
        try:
            with quiet_stdout():
                ch = t.option_chain(e)
        except Exception:
            continue
        if not underlying and isinstance(getattr(ch, "underlying", None), dict):
            u = ch.underlying
            ts = u.get("regularMarketTime")
            qt = ""
            if isinstance(ts, (int, float)):
                qt = datetime.fromtimestamp(ts, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
            underlying = {
                "price": num(u.get("regularMarketPrice")), "bid": num(u.get("bid")), "ask": num(u.get("ask")),
                "prev_close": num(u.get("regularMarketPreviousClose")),
                "change": num(u.get("regularMarketChange")), "change_pct": num(u.get("regularMarketChangePercent")),
                "currency": u.get("currency") or "USD", "quote_time": qt, "quote_tz": "UTC",
                "market_state": u.get("marketState") or "",
            }
        idx = len(loaded)
        loaded.append(e)
        for frame, is_call in ((ch.calls, 1), (ch.puts, 0)):
            if frame is None:
                continue
            for rec in frame.to_dict("records"):
                strike = num(rec.get("strike"))
                if strike is None:
                    continue
                bid, ask = num(rec.get("bid")), num(rec.get("ask"))
                iv = num(rec.get("impliedVolatility"), 6)
                rows.append([idx, strike, is_call, bid if bid and bid > 0 else None, ask if ask and ask > 0 else None,
                             num(rec.get("lastPrice")) or None, whole(rec.get("volume")) or 0,
                             whole(rec.get("openInterest")) or 0, iv if iv and iv > 1e-4 else None,
                             None, None, None, None])
    if not rows:
        return fail("feed_unavailable", f"Yahoo Finance returned no option quotes for {symbol}.", symbol=symbol)
    notes = ["Yahoo Finance quotes are delayed; bid and ask are often zero outside market hours."]
    out = {
        "ok": True, "kind": "chain", "symbol": s.lstrip("^"), "source": "yahoo",
        "source_label": "Yahoo Finance (delayed)", "delayed": True, "fetched_at": now_utc_iso(),
        "underlying": underlying, "expirations": loaded, "columns": CHAIN_COLUMNS, "rows": rows,
        "count": len(rows), "notes": notes,
    }
    if len(loaded) < total:
        out["partial"] = {"loaded": len(loaded), "total": total}
    return out


def cmd_chain(args):
    if not args:
        return fail("bad_request", "Usage: chain <SYMBOL> [--source auto|cboe|yahoo] [--max-expiries N]")
    symbol = args[0]
    source = "auto"
    max_exp = 12
    i = 1
    while i < len(args):
        if args[i] == "--source" and i + 1 < len(args):
            source = args[i + 1].lower()
            i += 2
        elif args[i] == "--max-expiries" and i + 1 < len(args):
            try:
                max_exp = max(1, min(40, int(args[i + 1])))
            except ValueError:
                pass
            i += 2
        else:
            i += 1
    if source == "yahoo":
        return yahoo_chain(symbol, max_exp)
    primary = cboe_chain(symbol)
    if primary.get("ok") or source == "cboe" or primary.get("status") == "bad_symbol":
        return primary
    fallback = yahoo_chain(symbol, max_exp)
    if fallback.get("ok"):
        fallback["fallback_reason"] = primary.get("message", "Cboe delayed quotes were unavailable.")
        return fallback
    if primary.get("status") == "no_options" and fallback.get("status") == "no_options":
        return fail("no_options", f"No listed options found for {symbol}. Options are available for US-listed "
                                  "stocks, ETFs and indices.", symbol=symbol)
    return fail("feed_unavailable", "{} {}".format(primary.get("message", ""), fallback.get("message", "")).strip(),
                symbol=symbol)


# ── U.S. Treasury par curve ─────────────────────────────────────────────────

TREASURY_CSV = ("https://home.treasury.gov/resource-center/data-chart-center/interest-rates/"
                "daily-treasury-rates.csv/{y}/all?type=daily_treasury_yield_curve&field_tdr_date_value={y}&page&_format=csv")
TENOR_YEARS = {"1 Mo": 1 / 12, "1.5 Month": 1.5 / 12, "2 Mo": 2 / 12, "3 Mo": 0.25, "4 Mo": 4 / 12, "6 Mo": 0.5,
               "1 Yr": 1.0, "2 Yr": 2.0, "3 Yr": 3.0, "5 Yr": 5.0, "7 Yr": 7.0, "10 Yr": 10.0, "20 Yr": 20.0,
               "30 Yr": 30.0}


def cmd_treasury(args):
    import csv
    import io

    import requests

    year = date.today().year
    if "--year" in args:
        try:
            year = int(args[args.index("--year") + 1])
        except (ValueError, IndexError):
            pass
    rows = []
    last_err = ""
    for y in (year, year - 1):
        try:
            r = requests.get(TREASURY_CSV.format(y=y), headers={"User-Agent": UA["User-Agent"]}, timeout=25)
        except requests.exceptions.RequestException as e:
            last_err = type(e).__name__
            continue
        if r.status_code != 200:
            last_err = f"HTTP {r.status_code}"
            continue
        reader = csv.DictReader(io.StringIO(r.text))
        for rec in reader:
            try:
                d = datetime.strptime(rec.get("Date", ""), "%m/%d/%Y").date()
            except ValueError:
                continue
            pts = []
            for col, yrs in TENOR_YEARS.items():
                v = num(rec.get(col))
                if v is not None:
                    pts.append((yrs, col, v))
            if len(pts) >= 4:
                rows.append((d, sorted(pts)))
        if rows:
            break
    if not rows:
        return fail("feed_unavailable", f"The U.S. Treasury yield curve could not be loaded ({last_err or 'no data'}).")
    rows.sort(key=lambda x: x[0], reverse=True)
    latest = rows[0]
    out = {
        "ok": True, "kind": "treasury_curve", "date": latest[0].isoformat(),
        "source_label": "U.S. Treasury daily par yield curve (CMT)", "fetched_at": now_utc_iso(),
        "tenors": [p[0] for p in latest[1]], "labels": [p[1] for p in latest[1]], "yields": [p[2] for p in latest[1]],
        "units": "percent, bond-equivalent (semi-annual) par yields",
    }
    if len(rows) > 1:
        prev = dict((p[1], p[2]) for p in rows[1][1])
        out["prev_date"] = rows[1][0].isoformat()
        out["prev_yields"] = [prev.get(p[1]) for p in latest[1]]
    return out


# ── Futures contract months (Yahoo) ─────────────────────────────────────────

MONTH_CODES = "FGHJKMNQUVXZ"
FUTURES_ROOTS = {
    # root: (Yahoo exchange suffix, listed month codes, name, quote unit)
    "CL": ("NYM", MONTH_CODES, "WTI crude oil", "USD per barrel"),
    "BZ": ("NYM", MONTH_CODES, "Brent crude oil", "USD per barrel"),
    "NG": ("NYM", MONTH_CODES, "Natural gas", "USD per MMBtu"),
    "RB": ("NYM", MONTH_CODES, "RBOB gasoline", "USD per gallon"),
    "HO": ("NYM", MONTH_CODES, "Heating oil", "USD per gallon"),
    "GC": ("CMX", "GJMQVZ", "Gold", "USD per troy ounce"),
    "SI": ("CMX", "HKNUZ", "Silver", "USD per troy ounce"),
    "HG": ("CMX", "HKNUZ", "Copper", "USD per pound"),
    "ZC": ("CBT", "HKNUZ", "Corn", "US cents per bushel"),
    "ZS": ("CBT", "FHKNQUX", "Soybeans", "US cents per bushel"),
    "ZW": ("CBT", "HKNUZ", "Wheat", "US cents per bushel"),
    "ES": ("CME", "HMUZ", "E-mini S&P 500", "index points"),
    "NQ": ("CME", "HMUZ", "E-mini Nasdaq-100", "index points"),
    "YM": ("CBT", "HMUZ", "E-mini Dow", "index points"),
    "RTY": ("CME", "HMUZ", "E-mini Russell 2000", "index points"),
    "ZN": ("CBT", "HMUZ", "10-year T-note", "points of par"),
    "ZB": ("CBT", "HMUZ", "30-year T-bond", "points of par"),
    "6E": ("CME", "HMUZ", "Euro FX", "USD per EUR"),
    "6J": ("CME", "HMUZ", "Japanese yen", "USD per JPY"),
    "BTC": ("CME", MONTH_CODES, "Bitcoin", "USD per BTC"),
}


def cmd_futures(args):
    if not args:
        return fail("bad_request", "Usage: futures_curve <ROOT> [--months N]")
    root = args[0].strip().upper().replace("=F", "")
    if root not in FUTURES_ROOTS:
        return fail("bad_symbol", "Futures curves are available for: " + ", ".join(sorted(FUTURES_ROOTS)) + ".",
                    symbol=root)
    n = 10
    if "--months" in args:
        try:
            n = max(2, min(24, int(args[args.index("--months") + 1])))
        except (ValueError, IndexError):
            pass
    try:
        import yfinance as yf
    except Exception:
        return fail("feed_unavailable", "The Yahoo Finance library is not installed.")
    suffix, codes, name, unit = FUTURES_ROOTS[root]
    today = date.today()
    contracts = []
    y, m = today.year, today.month
    while len(contracts) < n + 2 and y <= today.year + 6:
        if MONTH_CODES[m - 1] in codes:
            contracts.append((f"{root}{MONTH_CODES[m - 1]}{str(y)[-2:]}.{suffix}", y, m))
        m += 1
        if m > 12:
            m, y = 1, y + 1
    symbols = [c[0] for c in contracts] + [f"{root}=F"]
    try:
        with quiet_stdout():  # yfinance prints failed symbols to stdout
            frame = yf.download(symbols, period="5d", interval="1d", progress=False, group_by="ticker",
                                auto_adjust=False, threads=True)
    except Exception as e:
        return fail("feed_unavailable", f"Yahoo Finance could not be reached ({type(e).__name__}).", symbol=root)

    def last_of(sym):
        try:
            sub = frame[sym]
        except Exception:
            return None
        sub = sub.dropna(subset=["Close"]) if "Close" in sub else None
        if sub is None or sub.empty:
            return None
        row = sub.iloc[-1]
        prev = sub["Close"].iloc[-2] if len(sub) > 1 else None
        return {"date": str(sub.index[-1].date()), "last": num(row.get("Close")), "prev": num(prev),
                "volume": whole(row.get("Volume")) or 0}

    out_rows = []
    missing = []
    for sym, yy, mm in contracts:
        info = last_of(sym)
        if not info or not info["last"]:
            missing.append(sym)
            continue
        info.update({"symbol": sym, "month": f"{yy:04d}-{mm:02d}"})
        out_rows.append(info)
        if len(out_rows) >= n:
            break
    if not out_rows:
        return fail("feed_unavailable", f"Yahoo Finance returned no prices for {root} contract months.", symbol=root)
    front = last_of(f"{root}=F")
    return {
        "ok": True, "kind": "futures_curve", "root": root, "name": name, "unit": unit,
        "source_label": "Yahoo Finance (delayed)", "fetched_at": now_utc_iso(),
        "continuous": front, "contracts": out_rows, "missing": missing,
        "note": "Nominal maturity = the middle of the contract month (for roll yield).",
    }


def main(argv):
    if not argv:
        emit(fail("bad_request", "Usage: deriv_market.py chain|treasury_curve|futures_curve ..."))
        return
    cmd, rest = argv[0], argv[1:]
    try:
        if cmd == "chain":
            emit(cmd_chain(rest))
        elif cmd == "treasury_curve":
            emit(cmd_treasury(rest))
        elif cmd == "futures_curve":
            emit(cmd_futures(rest))
        else:
            emit(fail("bad_request", f"Unknown command '{cmd}'."))
    except Exception as e:  # never a traceback instead of JSON
        emit(fail("feed_unavailable", f"Unexpected failure: {type(e).__name__}: {e}"))


if __name__ == "__main__":
    main(sys.argv[1:])
