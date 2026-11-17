"""Parameter mapping shared by the akshare_*.py connector scripts.

The AkShare Data Explorer and Asia Markets screens send the boxes the user filled as named values:

    python akshare_stocks_historical.py stock_zh_a_hist symbol=600519 start_date=2024-01-01 period=weekly

The endpoint functions do not share one positional order (stock_zh_a_hist takes symbol, period,
start_date, ...; stock_zt_pool_em takes only a date; stock_board_industry_cons_em takes a board
name), so each value is applied by parameter name, and only where the endpoint really has a
parameter for it. A value the endpoint has no parameter for is left out and reported back in
"ignored_params" instead of being forced into the wrong parameter. Dates typed as YYYY-MM-DD or
YYYYMMDD are converted to the format of the parameter's own default (akshare mixes 20240101,
2024-01-01, "2024-01-01 09:30:00", 202401 and 2024).

Bare positional values (manual CLI use: `<endpoint> 000001 20240101`) are still accepted and are
passed in the endpoint's own parameter order.

The second half of this module is the call/output plumbing every akshare_*.py script shares:
safe_call() turns an akshare call into {"success", "data", "columns", "count"} or a plain-English
{"success": false, "error"}, and run_cli() prints that as the one JSON payload on stdout (anything
akshare or tqdm prints goes to stderr; NaN/Infinity/NaT become null via fincept_json).
"""

import inspect
import json
import re
import sys
import time

import fincept_json

# Screen field -> endpoint parameter names it may fill, in order of preference. `stock` comes
# before `symbol` because the endpoints that have both use `symbol` for the report type.
_FIELD_TARGETS = {
    "symbol": ("stock", "symbol", "fund_code"),
    "start_date": ("start_date", "start_day", "date", "trade_date"),
    "end_date": ("end_date", "end_day"),
    "period": ("period",),
}

# How the screens label each field (used in messages the user reads).
_FIELD_LABELS = {"symbol": "SYMBOL", "start_date": "START", "end_date": "END", "period": "PERIOD"}

# The screen's PERIOD choices (daily / weekly / monthly), spelled the way the endpoint's own
# `period` default is spelled. Endpoints whose `period` means something else (minutes "1"/"5",
# a range "全部") never receive it.
_PERIOD_LONG = {"daily": "daily", "weekly": "weekly", "monthly": "monthly"}
_PERIOD_SHORT = {"daily": "day", "weekly": "week", "monthly": "month"}

_DAY_RE = re.compile(r"^(\d{4})[-/]?(\d{2})[-/]?(\d{2})$")


def split_screen_args(args):
    """Split argv into (positional values, {field: value}). Only `field=value` items whose field is
    one the screens send are treated as named; anything else stays positional."""
    positional, named = [], {}
    for raw in args:
        key, sep, value = str(raw).partition("=")
        key = key.strip()
        if sep and key in _FIELD_TARGETS:
            value = value.strip()
            if value:
                named[key] = value
        else:
            positional.append(raw)
    return positional, named


def screen_values(args):
    """{field: value} for scripts that read the values themselves. Positional values are taken
    in the screens' historical order (symbol, start_date, end_date)."""
    positional, named = split_screen_args(args)
    values = dict(named)
    for field, value in zip(("symbol", "start_date", "end_date"), positional):
        if str(value).strip():
            values.setdefault(field, str(value).strip())
    return values


def _date_for(value, default):
    """`value` in the format of the parameter's default (YYYYMMDD when the default says nothing)."""
    m = _DAY_RE.match(value.strip())
    if not m:
        return value.strip()  # not a full day (e.g. "2024" for a year parameter) - pass as typed
    y, mo, d = m.groups()
    if isinstance(default, str):
        if re.fullmatch(r"\d{4}-\d{2}-\d{2}", default):
            return f"{y}-{mo}-{d}"
        if re.fullmatch(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}(:\d{2})?", default):
            return f"{y}-{mo}-{d}{default[10:]}"  # keep the default's time of day
        if re.fullmatch(r"\d{6}", default):
            return y + mo
        if re.fullmatch(r"\d{4}", default):
            return y
    return y + mo + d


def _period_for(value, default):
    """The PERIOD choice in the spelling of the parameter's default, or None if it does not apply."""
    if default in _PERIOD_LONG.values():
        return _PERIOD_LONG.get(value.strip().lower())
    if default in _PERIOD_SHORT.values():
        return _PERIOD_SHORT.get(value.strip().lower())
    return None


def _parameters(func):
    try:
        return inspect.signature(func).parameters
    except (TypeError, ValueError):
        return None


def map_screen_args(func, named):
    """({param: value} to call `func` with, [labels of fields it has no parameter for])."""
    params = _parameters(func) or {}
    kwargs, ignored = {}, []
    for field, value in named.items():
        target = next((p for p in _FIELD_TARGETS[field] if p in params and p not in kwargs), None)
        if target is None:
            ignored.append(_FIELD_LABELS[field])
            continue
        default = params[target].default
        if default is inspect.Parameter.empty:
            default = None
        if field == "period":
            mapped = _period_for(value, default) if isinstance(default, str) else None
            if mapped is None:
                ignored.append(_FIELD_LABELS[field])
                continue
            kwargs[target] = mapped
        elif field in ("start_date", "end_date"):
            kwargs[target] = _date_for(value, default)
        else:
            kwargs[target] = value
    return kwargs, ignored


def _needs_message(name, missing):
    labels = []
    for p in missing:
        field = next((f for f, targets in _FIELD_TARGETS.items() if p in targets), None)
        labels.append(f"{p} ({_FIELD_LABELS[field]} box)" if field else p)
    return f"{name} needs a value for: {', '.join(labels)}"


def missing_param(name, missing):
    """The failure for an endpoint called without a required value, nothing fetched:
    {"success": false, "data": [], "error": <sentence naming the screen boxes>,
     "error_type": "missing_param", "missing": [<param>, ...], "code": "bad_args"} — the screen
    can point at the empty box(es) instead of parsing the sentence."""
    return {"success": False, "data": [], "error": _needs_message(name, missing),
            "error_type": "missing_param", "missing": list(missing), "code": "bad_args"}


def call_with_screen_args(func, name, args):
    """Call endpoint function `func` with the screen's arguments (see module docstring).

    Returns the function's result dict, with "ignored_params" added when a value the user typed
    does not apply to this endpoint, or {"success": False, "error": ...} when the endpoint needs a
    value that was not given (or more values were given than it takes)."""
    positional, named = split_screen_args(args)
    params = _parameters(func)
    kwargs, ignored = map_screen_args(func, named) if named else ({}, [])

    if params is not None:
        positional_params = [p for p in params.values()
                             if p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD)]
        if len(positional) > len(positional_params) and not any(
                p.kind == p.VAR_POSITIONAL for p in params.values()):
            return {"success": False, "data": [],
                    "error": f"{name} takes at most {len(positional_params)} value(s) "
                             f"({', '.join(p.name for p in positional_params) or 'none'}), got {len(positional)}"}
        # Positional values fill the first parameters; drop a named value for one of those (passing
        # both would be a TypeError).
        for p in positional_params[:len(positional)]:
            kwargs.pop(p.name, None)
        filled = {p.name for p in positional_params[:len(positional)]} | set(kwargs)
        missing = [p.name for p in params.values()
                   if p.default is inspect.Parameter.empty
                   and p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD, p.KEYWORD_ONLY)
                   and p.name not in filled]
        if missing:
            return missing_param(name, missing)

    result = func(*positional, **kwargs)
    if ignored and isinstance(result, dict):
        result.setdefault("ignored_params", ignored)
    return result


# ── Calling akshare ──────────────────────────────────────────────────────────

def frame_records(df):
    """(rows, column names) of a DataFrame for the payload.

    datetime64 columns become text exactly as before (astype(str): "2024-01-02" or
    "2024-01-02 09:30:00"), with a missing cell as None instead of the string "NaT". NaN and
    Infinity cells are left for emit(), which writes them as null."""
    import pandas as pd

    df = df.copy()
    for i, dtype in enumerate(df.dtypes):
        if pd.api.types.is_datetime64_any_dtype(dtype):
            col = df.iloc[:, i]
            df.isetitem(i, col.astype(str).where(col.notna(), None))
    return df.to_dict(orient="records"), [str(c) for c in df.columns]


def to_result(obj, key_column="table"):
    """The success payload for whatever an akshare function returned. An empty frame is an explicit
    empty result (success, no rows), not an error."""
    import pandas as pd

    if isinstance(obj, dict) and not obj:
        obj = None  # e.g. a position ranking asked for a non-trading day
    if isinstance(obj, dict) and obj and all(isinstance(v, pd.DataFrame) for v in obj.values()):
        # {contract/variety: frame} (position rankings, warehouse receipts): one table, keyed by a column
        obj = pd.concat([v.assign(**{key_column: k}) for k, v in obj.items()], ignore_index=True)
    if isinstance(obj, pd.Series):
        obj = obj.to_frame().reset_index()
    if isinstance(obj, pd.DataFrame):
        rows, columns = frame_records(obj)
        return {"success": True, "data": rows, "columns": columns, "count": len(rows)}
    if obj is None:
        return {"success": True, "data": [], "count": 0}
    if isinstance(obj, (list, tuple)):
        rows = [v if isinstance(v, dict) else {"value": v} for v in obj]
        result = {"success": True, "data": rows, "count": len(rows)}
        if rows and all(not isinstance(v, dict) for v in obj):
            result["columns"] = ["value"]
        return result
    if isinstance(obj, dict) and all(isinstance(v, (list, tuple)) for v in obj.values()):
        # A dict of lists is a table, not one record (it came back as a single row whose cells
        # were whole lists): equal-length lists under several keys are its columns; otherwise
        # each key is a group ({"沪深300指数": ["io2611", ...]} -> one row per contract).
        lists = {str(k): list(v) for k, v in obj.items()}
        sizes = {len(v) for v in lists.values()}
        if len(lists) > 1 and len(sizes) == 1 and sizes.pop() > 1:
            names = list(lists)
            rows = [dict(zip(names, values)) for values in zip(*lists.values())]
            return {"success": True, "data": rows, "columns": names, "count": len(rows)}
        rows = [{"name": k, "value": item} for k, items in lists.items() for item in items]
        return {"success": True, "data": rows, "columns": ["name", "value"], "count": len(rows)}
    if isinstance(obj, dict):
        return {"success": True, "data": obj, "count": 1}
    return {"success": True, "data": str(obj), "count": 1}


def _exception_chain(exc):
    seen, stack = set(), [exc]
    while stack:
        e = stack.pop()
        if e is None or id(e) in seen:
            continue
        seen.add(id(e))
        yield e
        stack.extend(a for a in getattr(e, "args", ()) if isinstance(a, BaseException))
        stack.extend((e.__cause__, e.__context__))


def _class_names(exc):
    return {c.__name__ for c in type(exc).__mro__}


def _host(exc):
    """The host a network error was about, when it can be told."""
    for e in _exception_chain(exc):
        url = getattr(getattr(e, "request", None), "url", None)
        if url:
            m = re.match(r"^[a-z]+://([^/:?#]+)", str(url))
            if m:
                return m.group(1)
        m = re.search(r"host='([^']+)'", str(e))
        if m:
            return m.group(1)
    return None


def _closed_without_answer(exc):
    """The server hung up instead of answering: on these sites that means 'refused'."""
    from http.client import RemoteDisconnected

    return any(isinstance(e, (RemoteDisconnected, ConnectionResetError)) for e in _exception_chain(exc))


_REFUSED = ("{who} is refusing requests from this network ({why}) - it is blocking or rate-limiting "
            "this IP. Try again later.")


def describe_error(exc):
    """A plain-English message for a failed akshare call (no traceback text)."""
    names = _class_names(exc)
    text = str(exc).strip()
    host = _host(exc)
    who = host or "The data source"

    if "StalledError" in names:
        return (f"The data source did not finish answering within {BUDGET_SECONDS} s - it is sending data "
                "very slowly or not at all, which is usually how a site throttles a network. Try again later.")
    if "Push2RateLimitError" in names:
        return ("Eastmoney (push2.eastmoney.com) is rate-limiting this IP: it closed the connection without "
                "answering. The block usually lifts after about 5 minutes (40+ minutes if it is hit again); "
                "do not query Eastmoney quote endpoints until then.")
    if "DceBlockedError" in names:
        return ("The Dalian Commodity Exchange site (dce.com.cn) is refusing automated requests (HTTP 412 "
                "anti-bot check); DCE data cannot be fetched until the exchange lifts the block.")
    if "RateLimitError" in names:
        return f"The data source is rate-limiting requests from this network: {text}"
    if "InvalidParameterError" in names:
        return f"Invalid parameter: {text}"
    status = getattr(exc, "status_code", None) or getattr(getattr(exc, "response", None), "status_code", None)
    if status in (401, 403, 412, 418, 429, 451, 456):
        return _REFUSED.format(who=who, why=f"HTTP {status}")

    timeout_types = (TimeoutError,)
    try:
        import requests

        timeout_types += (requests.exceptions.Timeout,)
        connection_types = (requests.exceptions.ConnectionError, requests.exceptions.ChunkedEncodingError,
                            ConnectionError)
        http_error = requests.exceptions.HTTPError
    except ImportError:
        connection_types, http_error = (ConnectionError,), ()
    if isinstance(exc, timeout_types) or "timed out" in text.lower():
        return f"{who} did not answer in time (network timeout). Try again later."
    if http_error and isinstance(exc, http_error):
        return f"{who} answered with an error (HTTP {status}). Try again later."
    if isinstance(exc, connection_types) or {"RequestsError", "CurlError"} & names:
        if _closed_without_answer(exc):
            return _REFUSED.format(who=who, why="it closed the connection without answering")
        if any(k in text for k in ("NameResolution", "getaddrinfo", "Name or service not known")):
            return f"Could not resolve {who} - check the internet connection."
        return f"Could not connect to {who} (network error). Check the internet connection or try again later."

    if isinstance(exc, json.JSONDecodeError) or "Expecting value: line 1 column 1" in text:
        return ("The data source answered with an empty or non-JSON page - it is probably refusing or "
                "rate-limiting requests from this network, or it changed its format. Try again later.")
    if "BadZipFile" in names:
        return ("The data source sent a web page instead of the expected file - it is probably refusing "
                "automated requests from this network. Try again later.")
    if isinstance(exc, KeyError):
        return (f"The data source's answer has no field {text} - the request was refused or the site "
                "changed its format.")
    if isinstance(exc, ValueError) and ("Length mismatch" in text or "Expected axis" in text):
        return ("The data source changed its table layout and the installed akshare cannot parse it "
                "(column mismatch).")
    if isinstance(exc, (AttributeError, TypeError)) and "'NoneType' object" in text:
        return (f"The data source returned an empty page - it may be down, blocking requests, or have "
                f"changed ({text}).")
    text = text or type(exc).__name__
    return text if len(text) <= 500 else text[:497] + "..."


def _is_network(exc):
    """A failure of the site or the connection (unreachable, refused, rate-limited, timed out)."""
    if {"RateLimitError", "APIError", "NetworkError", "RequestsError", "CurlError"} & _class_names(exc):
        return True
    try:
        import requests

        network = (requests.exceptions.RequestException, ConnectionError, TimeoutError)
    except ImportError:
        network = (ConnectionError, TimeoutError)
    return isinstance(exc, network)


def _worth_retry(exc):
    """One more try only helps a dropped connection or a timeout. Never a rate limit or a refusal
    (retrying extends the block), a bad parameter, or a parse error."""
    if {"RateLimitError", "APIError", "InvalidParameterError", "StalledError"} & _class_names(exc):
        return False
    try:
        import requests

        transient = (requests.exceptions.ConnectionError, requests.exceptions.Timeout,
                     requests.exceptions.ChunkedEncodingError, ConnectionError, TimeoutError)
    except ImportError:
        transient = (ConnectionError, TimeoutError)
    return isinstance(exc, transient) and not _closed_without_answer(exc)


# Wall-clock budget for one script run. requests' timeout is per read, so a site that trickles
# bytes (SHFE did, 2026-11-16) never times out, and the host kills the script after 300 s with no
# explanation. run_cli() sets the deadline; each akshare call runs in a worker thread bounded by it.
# 270 s leaves the host's 300 s limit room for interpreter start-up and printing the answer.
BUDGET_SECONDS = 270
_DEADLINE = [None]


class StalledError(TimeoutError):
    """The data source did not finish answering within the script's budget."""


def _bounded(func, args, kwargs):
    if _DEADLINE[0] is None:
        return func(*args, **kwargs)
    remaining = _DEADLINE[0] - time.monotonic()
    if remaining <= 0:
        raise StalledError()
    box = {}

    def work():
        try:
            box["value"] = func(*args, **kwargs)
        except BaseException as exc:  # noqa: BLE001 - re-raised in the caller's thread
            box["error"] = exc

    import threading

    worker = threading.Thread(target=work, name="akshare-call", daemon=True)
    worker.start()
    worker.join(remaining)
    if worker.is_alive():
        raise StalledError()
    if "error" in box:
        raise box["error"]
    return box.get("value")


def safe_call(func, *args, _retries=2, _key_column="table", **kwargs):
    """Call an akshare function -> {"success", "data", "columns", "count"} or
    {"success": False, "error": <plain message>, "data": []}. Never raises."""
    last = None
    for attempt in range(max(1, _retries)):
        try:
            return to_result(_bounded(func, args, kwargs), key_column=_key_column)
        except Exception as exc:  # noqa: BLE001 - every failure becomes a JSON error
            last = exc
            if attempt + 1 < _retries and _worth_retry(exc):
                time.sleep(1 + attempt)
                continue
            break
    result = {"success": False, "error": describe_error(last), "data": []}
    if _is_network(last):
        result["error_type"] = "network"
    return result


# ── Default dates that never expire ──────────────────────────────────────────

def china_today():
    """Today's date at the Chinese exchanges (UTC+8), whatever the machine's time zone."""
    from datetime import datetime, timedelta, timezone

    return datetime.now(timezone(timedelta(hours=8))).date()


def recent_weekdays(count=7, fmt="%Y%m%d"):
    """The last `count` weekdays up to today (China time), newest first. Exchange holidays are not
    known here: a holiday simply comes back empty and the next older day is tried."""
    from datetime import timedelta

    out, day = [], china_today()
    while len(out) < count:
        if day.weekday() < 5:
            out.append(day.strftime(fmt))
        day -= timedelta(days=1)
    return out


def recent_period_ends(months=(3, 6, 9, 12), count=4, fmt="%Y%m%d"):
    """The last `count` report-period ends before today (quarter ends by default), newest first."""
    import calendar
    from datetime import date

    today = china_today()
    out, year = [], today.year
    while len(out) < count:
        for m in sorted(months, reverse=True):
            end = date(year, m, calendar.monthrange(year, m)[1])
            if end < today and len(out) < count:
                out.append(end.strftime(fmt))
        year -= 1
    return out


_DATE_COLUMNS = ("日期", "date", "交易日期", "净值日期", "公告日期", "变动日期", "实施方案公告日期", "除权日", "时间")


def between_dates(df, start_date="", end_date="", column=None):
    """Rows of `df` from start_date to end_date (inclusive; YYYYMMDD or YYYY-MM-DD), for akshare
    history functions that no longer take a date range. Unchanged when no bound is given or the
    frame has no date column."""
    import pandas as pd

    if not (start_date or end_date) or not isinstance(df, pd.DataFrame) or df.empty:
        return df
    column = column or next((c for c in _DATE_COLUMNS if c in df.columns), None)
    if column is None:
        return df
    when = pd.to_datetime(df[column].astype(str), errors="coerce")
    keep = when.notna()
    if start_date:
        keep &= when >= pd.Timestamp(start_date)
    if end_date:
        keep &= when < pd.Timestamp(end_date) + pd.Timedelta(days=1)
    return df[keep]


def call_first_with_rows(func, param, candidates, *args, **kwargs):
    """safe_call(func, ..., param=<candidate>) for each candidate (newest first) until one returns
    rows: the newest trading day or report period that has data. Stops at a network failure or a
    refusal - asking the same blocked site again only extends the block."""
    result = {"success": True, "data": [], "count": 0}
    for value in candidates:
        result = safe_call(func, *args, **{param: value}, **kwargs)
        if result.get("success") and result.get("count"):
            return result
        if not result.get("success") and result.get("error_type") == "network":
            return result
    return result


# Xueqiu answers anonymous requests for stock data with a login wall (code 400016 or an HTML page,
# checked 2026-11-16); its fund endpoints still answer. akshare's functions take a `token`, which
# the screens have no way to supply.
XUEQIU_LOGIN = ("{} needs a logged-in Xueqiu (xueqiu.com) session: Xueqiu stopped serving stock data to "
                "anonymous requests, and this screen cannot supply a login token.")


def unavailable(message):
    """Answer for an endpoint that cannot work as the screen calls it (paid key, login token):
    a plain error, without calling akshare."""
    return {"success": False, "error": message, "error_type": "unavailable", "data": []}


def usage_error(message):
    return {"success": False, "error": message, "error_type": "usage", "data": []}


# ── The one JSON payload on stdout ───────────────────────────────────────────

def _iso_default(obj):
    """Dates as the class-based scripts always wrote them (datetime.isoformat)."""
    iso = getattr(obj, "isoformat", None)
    if callable(iso):
        return iso()
    return str(obj)


def emit(result, *, ascii=False, iso_dates=False, stream=None):
    """Write `result` as one line of strict JSON (NaN/Infinity/NaT -> null, numpy scalars -> numbers)."""
    text = fincept_json.dumps(result, ensure_ascii=ascii, default=_iso_default if iso_dates else str)
    out = stream or sys.stdout
    out.flush()
    buffer = getattr(out, "buffer", None)
    if buffer is not None:  # always UTF-8, whatever the console encoding is
        buffer.write((text + "\n").encode("utf-8"))
        buffer.flush()
    else:
        out.write(text + "\n")
        out.flush()
    return text


def run_cli(handler, *, ascii=False, iso_dates=False):
    """Run handler() -> result dict and print the result as the script's one JSON line.

    While the handler runs, sys.stdout points at stderr, so progress bars and prints from akshare
    cannot reach the host's JSON parser. Any exception becomes {"success": false, "error": ...}.
    Returns the process exit code (1 for a usage error, else 0)."""
    real_stdout = sys.stdout
    sys.stdout = sys.stderr
    _DEADLINE[0] = time.monotonic() + BUDGET_SECONDS
    try:
        result = handler()
    except Exception as exc:  # noqa: BLE001
        result = {"success": False, "error": describe_error(exc), "data": []}
    finally:
        sys.stdout = real_stdout
        _DEADLINE[0] = None
    if not isinstance(result, dict):
        result = to_result(result)
    result.setdefault("timestamp", int(time.time()))
    emit(result, ascii=ascii, iso_dates=iso_dates)
    code = 1 if result.get("error_type") == "usage" else 0
    import threading

    if any(t.name == "akshare-call" and t.is_alive() for t in threading.enumerate()):
        # A stalled request is still reading: the payload is out, so leave now rather than wait for
        # it (or let it print after the JSON).
        import os

        sys.stderr.flush()
        os._exit(code)
    return code


def endpoint_listing(endpoints):
    """get_all_endpoints answer for an ENDPOINTS = {name: {"func", "desc", "category"}} table."""
    categories = {}
    for name, info in endpoints.items():
        categories.setdefault(info.get("category", "Other"), []).append(name)
    return {"success": True,
            "data": {"available_endpoints": list(endpoints), "total_count": len(endpoints),
                     "categories": categories}}


def run_endpoint_table(endpoints, retired=None, argv=None):
    """main() of the scripts built on an ENDPOINTS table:
    `<script> get_all_endpoints` | `<script> <endpoint> [field=value ...]`.

    `retired` maps names that are no longer listed to the message answered when one is still
    called (an old saved screen state, a manual run)."""
    argv = sys.argv[1:] if argv is None else argv

    def handler():
        if not argv:
            return usage_error("No endpoint specified")
        name, args = argv[0], argv[1:]
        if name == "get_all_endpoints":
            return endpoint_listing(endpoints)
        if name in endpoints:
            return call_with_screen_args(endpoints[name]["func"], name, args)
        if retired and name in retired:
            return unavailable(retired[name])
        return {"success": False, "error": f"Unknown endpoint: {name}", "data": []}

    return run_cli(handler)


def run_wrapper_cli(wrapper, listing, *, aliases=None, retired=None, argv=None, ascii=False, iso_dates=False):
    """main() of the class-based scripts: `get_all_endpoints` -> listing(); `<endpoint>` -> the
    wrapper method aliases[endpoint], get_<endpoint>, or <endpoint> (first that exists)."""
    argv = sys.argv[1:] if argv is None else argv

    def handler():
        if not argv:
            script = sys.argv[0].replace("\\", "/").rsplit("/", 1)[-1] if sys.argv else "akshare script"
            return usage_error(f"Usage: python {script} <endpoint> [field=value ...]")
        endpoint, args = argv[0], argv[1:]
        if endpoint == "get_all_endpoints":
            return listing()
        if retired and endpoint in retired:
            return unavailable(retired[endpoint])
        candidates = [(aliases or {}).get(endpoint)]
        candidates += [endpoint] if endpoint.startswith("get_") else [f"get_{endpoint}", endpoint]
        for name in candidates:
            if name and not name.startswith("_") and name != listing.__name__:
                method = getattr(wrapper, name, None)
                if callable(method):
                    return call_with_screen_args(method, endpoint, args)
        return {"success": False, "error": f"Unknown endpoint: {endpoint}", "data": []}

    return run_cli(handler, ascii=ascii, iso_dates=iso_dates)
