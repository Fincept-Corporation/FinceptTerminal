"""Shared plumbing for the Fincept data connectors (scripts/*_data.py). Not a connector itself.

Every connector prints one JSON payload and reports failure as {"error": "<plain message>"}.
These helpers keep those messages uniform and safe:

    missing_key(service, signup_url, env_var)
        The message for an absent API key. Return it BEFORE any network call, so the user is
        told what to do instead of seeing an upstream "HTTP 401".
    request_error(service, exc, env_var=None, secret=None)
        A plain message for a failed `requests` call (HTTP status, timeout, connection failure).
        It never contains the request URL, so a key passed as a query parameter cannot leak into
        the UI, the logs or an AI agent's context.
    scrub(text, *secrets)
        Mask the given secret values and key-like URL query parameters in any text.
    http_session(retries=1)
        A requests.Session that retries failed connections but never re-sends a request whose
        response timed out: `HTTPAdapter(max_retries=3)` turns a 30 s timeout into 2 minutes.
    parallel_map(fn, items, workers=8)
        {key: fn(value)} for a dict of independent requests, run concurrently.
    number(text) / numeric_columns(rows, skip=())
        Numbers for numeric text: one value, or every all-numeric column of a row list (identifier
        columns - ids, codes, CUSIPs, versions - and zero-padded codes stay text).
    iso_date(text, fmt) / epoch_iso(value)
        ISO dates at the source: strptime-format text -> YYYY-MM-DD (or YYYY-MM-DDTHH:MM:SSZ when the
        format has a time); Unix seconds/milliseconds -> YYYY-MM-DDTHH:MM:SSZ.

Envelope mode (one uniform result shape for every connector, opt-in per call):

    python fincept_connector.py run <script.py> <command> [args...]

runs the connector exactly as `python <script.py> <command> [args...]` would (same interpreter,
environment and stdin) and prints ONE JSON object:

    success  {"ok": true,
              "data": [row objects], "columns": [...], "types": {column: kind}      # one table
              -- or, when the result holds two or more tables --
              "data": <the object>, "tables": {name: {"columns", "types", "rows"}},
              "meta": {"source", "script", "command", "as_of" (ISO-8601 UTC), "count",
                       "truncated", "next"?, "extra": {"shape": <what was done to make rows —
                       see _tabulate()>, "rows_key"?, <the connector's other top-level fields>,
                       "omitted"?: [names of fields over 2 KB left out of meta - a raw pivot
                       kept next to "data" for an older caller; FINCEPT_CONNECTOR_RAW=1 has them]}},
              "raw": <the connector's own payload>     # only with FINCEPT_CONNECTOR_RAW=1
             }
             Every success carries either columns + a row list or tables: a caller never has
             to guess the shape. kind is number|integer|string|date|datetime|bool|object.
    failure  {"ok": false, "error": "<plain message>", "code": <one of ERROR_CODES>,
              "retry_after_s"?, "env_key"?, "env_keys"?, "docs_url"?}

Connectors themselves are unchanged, so every other caller keeps today's shapes. A connector may
put "code" (and "env_key", "docs_url", "retry_after_s") next to "error" itself; otherwise the code
is derived from the message ({"error": true, "message": ...} uses the message; a composite
{"success": false, <part>: {"error": ...}} reports its failed parts and is classified by the first).
See envelope() / classify_error(). Environment:
FINCEPT_CONNECTOR_CATALOG (path of data_connectors.json; searched for next to the scripts when
unset) and FINCEPT_CONNECTOR_TIMEOUT_S (kill a connector that runs longer; 0/unset = no limit).

Usage:
    import fincept_connector as fc
    if not API_KEY:
        return fc.missing_key("Finnhub", "https://finnhub.io/register", "FINNHUB_API_KEY")
    try:
        ...
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error("Finnhub", e, "FINNHUB_API_KEY", API_KEY)}
"""

from __future__ import annotations

import json
import re

__all__ = ["missing_key", "scrub", "http_error", "request_error", "http_session", "parallel_map",
           "number", "numeric_columns", "iso_date", "epoch_iso",
           "ERROR_CODES", "classify_error", "envelope", "run_enveloped"]

# The closed set of failure codes an enveloped result can carry.
ERROR_CODES = ("missing_key", "auth_failed", "rate_limited", "upstream_down", "bad_args",
               "no_data", "not_supported", "unknown")

# key=value pairs in URLs / messages whose value is a credential.
_KEY_PARAM = re.compile(
    r"(?i)\b((?:api[_-]?key|apikey|api[_-]?token|access[_-]?key|access[_-]?token|app[_-]?id|appid|"
    r"auth[_-]?token|token|key|secret|client[_-]?secret|password|signature)=)[^&#\s'\"<>]*"
)
_URL = re.compile(r"(?i)\b(?:https?://|with url: )\S+")


def missing_key(service: str, signup_url: str, env_var: str, what: str = "an API key") -> dict:
    """{"error": ...} telling the user which key is missing and where to get it.

    Also carries "code", "env_key" and "docs_url", so a caller can offer "add key" without
    reading the sentence."""
    return {"error": f"{service} needs {what}: get one at {signup_url} and save it as {env_var} "
                     f"in Settings > Credentials",
            "code": "missing_key", "env_key": env_var, "docs_url": signup_url}


def scrub(text, *secrets) -> str:
    """`text` with the secret values and any key-like query parameter values masked."""
    text = "" if text is None else str(text)
    for s in secrets:
        if s and len(str(s)) >= 4:
            text = text.replace(str(s), "***")
    return _KEY_PARAM.sub(r"\1***", text)


def _upstream_detail(response, secrets) -> str:
    """A short human-readable reason from an error response body, or ''."""
    try:
        body = response.json()
    except Exception:
        body = None
    msg = None
    if isinstance(body, dict):
        for k in ("message", "error_message", "Error Message", "error", "detail", "msg",
                  "Information", "Note", "description", "errorMessage", "status_message"):
            v = body.get(k)
            if isinstance(v, dict):
                v = v.get("message") or v.get("error_message") or v.get("detail")
            if isinstance(v, str) and v.strip():
                msg = v
                break
        if msg is None and isinstance(body.get("status"), dict):
            msg = body["status"].get("error_message")
        if msg is None and isinstance(body.get("errors"), list) and body["errors"]:
            first = body["errors"][0]
            msg = first.get("message") or first.get("detail") if isinstance(first, dict) else str(first)
    elif body is None:
        text = (getattr(response, "text", "") or "").strip()
        if text and not text.startswith("<") and len(text) < 300:
            msg = text
    if not msg:
        return ""
    # Upstream text may carry useful links (a signup page); key values are masked by scrub(), and
    # a "... for url: <our request>" echo is dropped.
    msg = re.sub(r"(?i)\s*(?:for|with) url: \S+", "", scrub(str(msg), *secrets)).strip()
    return msg[:200]


def http_error(service: str, response, env_var: str | None = None, secret: str | None = None) -> str:
    """Plain message for a non-2xx response. Never includes the request URL."""
    code = getattr(response, "status_code", 0) or 0
    if code == 401 and env_var:
        msg = f"{service} rejected the API key (HTTP 401): check {env_var} in Settings > Credentials"
    elif code == 403 and env_var:
        msg = (f"{service} refused the request (HTTP 403): the key saved as {env_var} in "
               f"Settings > Credentials is invalid or its plan does not include this data")
    elif code in (401, 403):
        msg = f"{service} refused access (HTTP {code})"
    elif code == 404:
        msg = f"{service}: not found (HTTP 404) - check the symbol / parameters"
    elif code == 429:
        msg = f"{service} is rate-limiting requests (HTTP 429) - wait a minute and try again"
    elif code >= 500:
        msg = f"{service} is down or overloaded (HTTP {code}) - try again later"
    else:
        msg = f"{service} returned HTTP {code}"
    detail = _upstream_detail(response, (secret,))
    return f"{msg}: {detail}" if detail else msg


def request_error(service: str, exc, env_var: str | None = None, secret: str | None = None) -> str:
    """Plain message for an exception raised by `requests`. Never includes the request URL."""
    import requests

    # requests >= 2.27: response.json() raises requests.exceptions.JSONDecodeError, which is a
    # RequestException (via InvalidJSONError), so it lands here rather than in `except ValueError`.
    if isinstance(exc, (getattr(requests.exceptions, "InvalidJSONError", ()), json.JSONDecodeError)):
        return f"{service} returned a response that is not JSON"
    response = getattr(exc, "response", None)
    if isinstance(exc, requests.exceptions.HTTPError) and response is not None:
        return http_error(service, response, env_var, secret)
    if isinstance(exc, requests.exceptions.Timeout):
        return f"{service} did not answer in time - it may be down or overloaded; try again later"
    if isinstance(exc, requests.exceptions.ConnectionError):
        reason = str(exc)
        m = re.search(r"Caused by \w+\((.*)\)\)?\s*$", reason)
        reason = m.group(1) if m else reason
        reason = re.sub(r"<[^>]+>:?\s*", "", reason)
        reason = _URL.sub("", scrub(reason, secret)).strip(" '\"()")
        reason += ")" * max(0, reason.count("(") - reason.count(")"))
        return f"Could not connect to {service}" + (f": {reason[:160]}" if reason else "")
    return f"{service} request failed: {_URL.sub('', scrub(exc, secret)).strip()[:200]}"


def http_session(retries: int = 1, backoff: float = 0.5, pool: int = 10):
    """requests.Session that retries connection failures `retries` times, never read timeouts."""
    import requests
    from urllib3.util.retry import Retry

    session = requests.Session()
    retry = Retry(total=retries, connect=retries, read=False, status=0, backoff_factor=backoff)
    adapter = requests.adapters.HTTPAdapter(pool_connections=pool, pool_maxsize=pool, max_retries=retry)
    session.mount("https://", adapter)
    session.mount("http://", adapter)
    return session


_NUMBER_TEXT = re.compile(r"^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$")
_LEADING_ZERO = re.compile(r"^[+-]?0\d")
# Columns that hold identifiers, not quantities: they stay text even when every value is digits.
_ID_COLUMN = re.compile(
    r"(?i)(?:^|_)(?:ids?|codes?|cusip|isin|cik|fips|zip|zipcode|postcode|naics|sic|lei|figi|sedol|version|uuid|hash|"
    r"phone|display)$|(?:Id|ID|Code|Cusip|Number)$")


def number(value):
    """A numeric string as an int (whole) or float; None for "" / None; anything else unchanged."""
    if isinstance(value, str):
        text = value.strip()
        if text == "":
            return None
        if _NUMBER_TEXT.match(text):
            f = float(text)
            if f.is_integer() and "." not in text and "e" not in text.lower():
                return int(text)
            return f
    return value


def numeric_columns(rows, skip=()):
    """Turn every column of `rows` whose non-empty values are all numeric strings into numbers, in
    place, and return `rows`. Identifier columns (ids, codes, CUSIPs, versions, `skip`) and codes
    with leading zeros ("007") stay text; a column with any non-numeric text is left alone."""
    if not isinstance(rows, list):
        return rows
    names = []
    for r in rows:
        if isinstance(r, dict):
            names += [k for k in r if k not in names]
    for name in names:
        if name in skip or _ID_COLUMN.search(str(name)):
            continue
        texts = [r.get(name) for r in rows if isinstance(r, dict) and r.get(name) not in (None, "")]
        if not texts or not all(isinstance(t, str) and _NUMBER_TEXT.match(t.strip()) for t in texts):
            continue
        if any(_LEADING_ZERO.match(t.strip()) for t in texts):
            continue
        for r in rows:
            if isinstance(r, dict) and name in r:
                r[name] = number(r[name])
    return rows


def iso_date(text, fmt):
    """`text` parsed with strptime `fmt` as an ISO date (YYYY-MM-DD), or as an ISO UTC date-time
    (YYYY-MM-DDTHH:MM:SSZ) when `fmt` has a time part; unchanged when it does not parse."""
    from datetime import datetime, timezone

    if not isinstance(text, str) or not text.strip():
        return text
    try:
        d = datetime.strptime(text.strip(), fmt)
    except ValueError:
        return text
    if any(x in fmt for x in ("%H", "%I", "%M", "%S")):
        if d.tzinfo is not None:
            d = d.astimezone(timezone.utc).replace(tzinfo=None)
        return d.strftime("%Y-%m-%dT%H:%M:%SZ")
    return d.date().isoformat()


def epoch_iso(value):
    """Unix seconds or milliseconds (number or numeric text) as an ISO UTC date-time ending in Z
    (milliseconds kept when present); anything else unchanged."""
    from datetime import datetime, timezone

    v = number(value) if isinstance(value, str) else value
    if not isinstance(v, (int, float)) or isinstance(v, bool):
        return value
    ms = abs(v) >= 1e11  # 1e11 s is the year 5138; 1e11 ms is 1973
    seconds = v / 1000.0 if ms else float(v)
    try:
        d = datetime.fromtimestamp(seconds, tz=timezone.utc)
    except (OverflowError, OSError, ValueError):
        return value
    if ms and int(v) % 1000:
        return d.strftime("%Y-%m-%dT%H:%M:%S.") + f"{int(v) % 1000:03d}Z"
    return d.strftime("%Y-%m-%dT%H:%M:%SZ")


def parallel_map(fn, items: dict, workers: int = 8) -> dict:
    """{key: fn(value)} for every item, evaluated concurrently (order of `items` preserved)."""
    if not items:
        return {}
    from concurrent.futures import ThreadPoolExecutor

    keys = list(items)
    with ThreadPoolExecutor(max_workers=max(1, min(workers, len(keys)))) as pool:
        values = list(pool.map(fn, (items[k] for k in keys)))
    return dict(zip(keys, values))


# ── Envelope ──────────────────────────────────────────────────────────────────────────────

_ENV_NAME = re.compile(r"\b[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*_(?:KEY|TOKEN|SECRET|ID|EMAIL|PASSWORD|USERNAME|USER)\b")
_LINK = re.compile(r"https?://[^\s'\"<>)]+")

# Ordered: the first matching rule wins. A missing key is checked before a rejected one, and both
# before the generic "required"/"invalid" wording of bad arguments.
_CODE_RULES = (
    ("missing_key", re.compile(
        r"(?i)needs (?:an?|your|the) .{0,40}?(?:key|token|app ?id|appname|account|credentials?|client ?id)"
        r"|(?:key|token|credentials?|app(?:lication)? id|client ?id|password|username)[^.]{0,60}"
        r"(?:not (?:set|configured|provided|found)|is required|are required|required\b|missing)"
        r"|environment variables? not set|save it as [A-Z_]+|requires? (?:an?|a free) (?:api )?key"
        r"|api key required|no api key")),
    ("auth_failed", re.compile(
        r"(?i)rejected the (?:api )?key|invalid (?:api )?key|key .{0,30}(?:is invalid|expired)"
        r"|http 40[13]\b|\b40[13]\b.{0,20}(?:unauthori[sz]ed|forbidden)|unauthori[sz]ed|forbidden"
        r"|refused (?:access|the request)|credentials rejected|authentication failed|access denied"
        r"|plan does not include")),
    ("rate_limited", re.compile(
        r"(?i)rate[- ]?limit|too many requests|http 429|\b429\b|quota (?:exceeded|reached)|throttl")),
    ("not_supported", re.compile(
        r"(?i)not supported|not implemented|no longer (?:available|offered|provided|resolves)"
        r"|has been (?:retired|withdrawn|discontinued)|is not available (?:without|from)|unsupported"
        r"|http 402\b|\b402 client error|payment required")),
    ("upstream_down", re.compile(
        r"(?i)down or overloaded|http 5\d\d|\b5\d\d\b.{0,20}(?:server|gateway|unavailable)"
        r"|did not answer in time|timed? ?out|could not connect|connection (?:refused|reset|aborted)"
        r"|not json|returned its html|bot protection|javascript challenge|cloudflare|maintenance"
        r"|temporarily unavailable|service unavailable|bad gateway|name or service not known"
        # a body that is not JSON (an HTML error page): json's own message
        r"|expecting value: line \d+ column \d+|json decode error|unterminated string starting at"
        # DNS / TCP failures as requests and urllib3 word them
        r"|nameresolutionerror|failed to resolve|getaddrinfo failed|max retries exceeded"
        r"|failed to establish a new connection|remote end closed connection|connection broken")),
    ("bad_args", re.compile(
        r"(?i)^usage\b|no command|missing arguments?|unknown (?:command|indicator|series|dataset|option)|invalid (?:command|argument|"
        r"parameter|date|symbol|value|json)|missing (?:argument|parameter)|\brequired\b|must be (?:a|an|one of)"
        r"|check the symbol / parameters|expected \d+ arg|not a valid"
        r"|^provide\b|please provide|must be provided|http 400\b|\b400 client error|bad request"
        r"|parameter value is not valid")),
    ("no_data", re.compile(
        r"(?i)no data|no results?|not found|no (?:records|rows|observations|series|matches)|empty (?:response|result)"
        r"|returned nothing|nothing (?:found|returned)|no recent|no \w+ data\b|published no\b"
        r"|\bno (?:\w+ ){1,2}found\b")),
)


def classify_error(message, hint=None) -> str:
    """The ERROR_CODES entry for a failure message. `hint` is a code the connector supplied."""
    if isinstance(hint, str) and hint in ERROR_CODES:
        return hint
    text = "" if message is None else str(message)
    for code, rule in _CODE_RULES:
        if rule.search(text):
            return code
    return "unknown"


def _last_json(text):
    """(payload, parsed) for what a connector printed: the whole output, else its last JSON line/block."""
    text = (text or "").strip()
    if not text:
        return None, False
    try:
        return json.loads(text), True
    except ValueError:
        pass
    for line in reversed(text.splitlines()):
        line = line.strip()
        if line[:1] in "{[":
            try:
                return json.loads(line), True
            except ValueError:
                continue
    for opener in "{[":
        at = text.rfind("\n" + opener)
        while at != -1:
            try:
                return json.loads(text[at + 1:]), True
            except ValueError:
                at = text.rfind("\n" + opener, 0, at)
    return None, False


_TABLE_KEYS = ("data", "rows", "records", "observations", "results", "items", "values", "series",
               "result", "response", "dataset", "list", "entries", "features", "_embedded")
_DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
_DATETIME = re.compile(r"^\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}(?::\d{2}(?:\.\d+)?)?(?:Z|[+-]\d{2}:?\d{2})?$")
_PERIOD_KEY = re.compile(r"^\d{4}(?:-?(?:Q[1-4]|\d{2})(?:-\d{2})?)?$")


def _is_scalar(value) -> bool:
    return value is None or isinstance(value, (str, int, float, bool))


def _is_rows(value) -> bool:
    return isinstance(value, list) and bool(value) and all(isinstance(r, dict) for r in value)


def _find_rows(value, depth=3):
    """(rows, path) for the one list of row objects reachable from `value` through
    conventional container keys (at most `depth` levels down), else (None, None)."""
    if _is_rows(value):
        return value, []
    if isinstance(value, dict) and depth > 0:
        for key in _TABLE_KEYS:
            if key in value:
                rows, path = _find_rows(value[key], depth - 1)
                if rows is not None:
                    return rows, [key] + path
        if len(value) == 1:
            # a container with one named member: HAL {"_embedded": {"instruments": [...]}}
            key, inner = next(iter(value.items()))
            if _is_rows(inner):
                return inner, [str(key)]
    return None, None


def _rows_of(value):
    """`value` as a list of row objects when it is one (directly or under conventional keys)."""
    rows, _ = _find_rows(value, 2)
    return rows


def _column_names(rows):
    names, seen = [], set()
    for row in rows:
        for key in row:
            if key not in seen:
                seen.add(key)
                names.append(key)
    return names


def _column_types(rows, names):
    """{column: "number" | "integer" | "string" | "date" | "datetime" | "bool" | "object"},
    inferred from the non-null values (a column with none is "string")."""
    kinds = {}
    for name in names:
        seen = set()
        for row in rows[:2000]:
            v = row.get(name)
            if v is None:
                continue
            if isinstance(v, bool):
                seen.add("bool")
            elif isinstance(v, int):
                seen.add("integer")
            elif isinstance(v, float):
                seen.add("number")
            elif isinstance(v, str):
                text = v.strip()
                seen.add("date" if _DATE.match(text) else "datetime" if _DATETIME.match(text) else "string")
            else:
                seen.add("object")
            if len(seen) > 2:
                break
        if not seen:
            kinds[name] = "string"
        elif len(seen) == 1:
            kinds[name] = seen.pop()
        elif seen == {"integer", "number"}:
            kinds[name] = "number"
        elif seen == {"date", "datetime"}:
            kinds[name] = "datetime"
        elif "object" in seen:
            kinds[name] = "object"
        else:
            kinds[name] = "string"
    return kinds


def _table(rows):
    names = _column_names(rows)
    return {"columns": names, "types": _column_types(rows, names), "rows": rows}


def _tabulate(data, nested=False):
    """(result, shape, info): `data` as ONE list of row objects, or — shape "tables" — as a
    {name: rows} dict when it holds several tables. (None, None, None) when there is nothing.

    shape says what was done, so a caller never has to guess:
      rows             already a list of row objects
      nested_table     the one table inside a wrapper object; info["rows_key"] is its path
                       ("observations", "result.data.rows", "rates") and info["inner_shape"]
                       how that value itself was turned into rows when it was not a row list
                       (single_record: the wrapper's only member is one record, {"quote": {..}})
      tables           two or more tables inside one object, by name
      column_oriented  parallel lists ({"dates": [...], "values": [...]}) zipped into rows
      dict_of_dicts    {key: {..}, ..} -> one row per entry, the entry's key in column "key"
      key_value        {name: number, ..} -> rows of {"key", "value"}
      list_of_values   [v, ..] -> rows of {"value"}
      list_of_lists    [[..], ..] -> rows with columns col_1..col_n (first row used as the
                       header when it is all strings and the next row is not)
      single_record    one object -> a one-row table (nested values stay as object cells)
    `nested` is True for a value found inside a wrapper: a map of numbers is then a
    key/value table whatever its keys are (at top level it must look like periods, because
    {"price": 1.5, "volume": 10} is a record).
    """
    if _is_rows(data):
        return data, "rows", {}
    if isinstance(data, list):
        if not data:
            return None, None, None
        if all(_is_scalar(v) for v in data):
            return [{"value": v} for v in data], "list_of_values", {}
        if all(isinstance(v, (list, tuple)) for v in data):
            width = max(len(v) for v in data)
            head = data[0]
            body = data
            if (len(data) > 1 and len(head) == width and all(isinstance(h, str) and h for h in head)
                    and not all(isinstance(c, str) for c in data[1])):
                names, body = [str(h) for h in head], data[1:]
            else:
                names = [f"col_{i + 1}" for i in range(width)]
            return [dict(zip(names, list(v) + [None] * (width - len(v)))) for v in body], "list_of_lists", {}
        return [{"value": v} for v in data], "list_of_values", {}
    if not isinstance(data, dict):
        return [{"value": data}], "single_record", {}
    if not data:
        return None, None, None

    scalars = {k: v for k, v in data.items() if _is_scalar(v)}

    # 1. the table under a conventional container key ("data", "results", "observations", ...)
    rows, path = _find_rows(data, 3)
    if rows is not None and path:
        info = {"rows_key": ".".join(path)}
        outer = {k: v for k, v in scalars.items() if k != path[0]}
        if outer:
            info["fields"] = outer
        return rows, "nested_table", info

    # 2. parallel lists
    lists = {k: v for k, v in data.items() if isinstance(v, list) and v and all(_is_scalar(x) for x in v)}
    if len(lists) >= 2 and len(lists) == len(data) - len(scalars) and len({len(v) for v in lists.values()}) == 1:
        n = len(next(iter(lists.values())))
        info = {"fields": scalars} if scalars else {}
        return [{k: v[i] for k, v in lists.items()} for i in range(n)], "column_oriented", info

    values = list(data.values())
    all_dicts = len(values) >= 2 and all(isinstance(v, dict) for v in values)

    # 3. a wrapper around one table, or around several
    if not all_dicts or any(_rows_of(v) is not None for v in values):
        subs = {}
        records = {}
        for key, value in data.items():
            if _is_scalar(value):
                continue
            sub, shape, info = _tabulate(value, nested=True)
            if sub is None:
                continue
            if shape == "single_record":
                if isinstance(value, dict):
                    records[str(key)] = sub
                continue
            if shape == "tables":
                for name, table_rows in sub.items():
                    subs[f"{key}.{name}"] = (table_rows, "rows", {})
            else:
                subs[str(key)] = (sub, shape, info)
        if len(subs) == 1:
            key, (sub, shape, info) = next(iter(subs.items()))
            out = {"rows_key": key + ("." + info["rows_key"] if info.get("rows_key") else "")}
            if shape not in ("rows", "nested_table"):
                out["inner_shape"] = shape
            elif info.get("inner_shape"):
                out["inner_shape"] = info["inner_shape"]
            fields = dict(info.get("fields") or {})
            fields.update(scalars)
            if fields:
                out["fields"] = fields
            return sub, "nested_table", out
        if len(subs) >= 2:
            info = {"fields": scalars} if scalars else {}
            return {name: sub for name, (sub, _, _) in subs.items()}, "tables", info
        if not subs and len(records) == 1 and len(data) - len(scalars) == 1:
            # {"symbol": "X", "quote": {...}}: the one record inside the wrapper is the row (it
            # used to be a one-row table whose only real cell was an object).
            key, rec = next(iter(records.items()))
            out = {"rows_key": key, "inner_shape": "single_record"}
            if scalars:
                out["fields"] = dict(scalars)
            return rec, "nested_table", out

    # 4. the object itself is the table
    if all_dicts:
        out = []
        for key, value in data.items():
            row = {"key": key}
            row.update({(k if k != "key" else "key_"): x for k, x in value.items()})
            out.append(row)
        return out, "dict_of_dicts", {}
    numeric = all(v is None or (isinstance(v, (int, float)) and not isinstance(v, bool)) for v in values)
    if numeric and ((nested and len(values) >= 2)
                    or (len(values) >= 3 and all(_PERIOD_KEY.match(str(k)) for k in data))):
        return [{"key": k, "value": v} for k, v in data.items()], "key_value", {}
    return [data], "single_record", {}


def _failure(message, hint=None, payload=None, entry=None) -> dict:
    payload = payload if isinstance(payload, dict) else {}
    entry = entry or {}
    # Drop an echoed request URL ("... for url: https://...") before anything else sees it.
    message = re.sub(r"(?i)\s*(?:for|with) url: \S+", "", scrub(message)).strip()
    message = message or "The connector failed without a message"
    code = classify_error(message, hint or payload.get("code"))
    broken = entry.get("broken")
    if broken and code not in ("missing_key", "bad_args"):
        # The catalogue records why this upstream cannot be reached; that is the real answer,
        # not the HTTP status of today's attempt.
        code = "not_supported"
        message = str(broken)
    out = {"ok": False, "error": message, "code": code}
    retry = payload.get("retry_after_s", payload.get("retry_after"))
    if isinstance(retry, (int, float)) and not isinstance(retry, bool) and retry >= 0:
        out["retry_after_s"] = int(retry)
    elif code == "rate_limited":
        out["retry_after_s"] = 60
    if code in ("missing_key", "auth_failed"):
        named = _ENV_NAME.findall(message)
        keys = entry.get("env_keys") or []
        env_key = payload.get("env_key") or (named[0] if named else (keys[0] if len(keys) == 1 else None))
        if env_key:
            out["env_key"] = env_key
        if len(keys) > 1:
            out["env_keys"] = keys
    link = payload.get("docs_url") or entry.get("docs_url")
    if not link and code in ("missing_key", "auth_failed"):
        found = _LINK.findall(message)
        link = found[0].rstrip(".,;") if found else None
    if link:
        out["docs_url"] = link
    return out


def _nested_errors(payload, depth=2):
    """[(part, message)] for the failed parts one or two levels inside a failed composite result."""
    out = []
    if not isinstance(payload, dict) or depth <= 0:
        return out
    for key, value in payload.items():
        if isinstance(value, dict):
            msg = value.get("error")
            if isinstance(msg, dict):
                msg = msg.get("message") or msg.get("detail")
            if isinstance(msg, str) and msg.strip():
                out.append((str(key), msg.strip()[:160]))
            else:
                out += [(f"{key}.{k}", m) for k, m in _nested_errors(value, depth - 1)]
    return out


_EXTRA_MAX_BYTES = 2048


def _small(value) -> bool:
    """True when a descriptive field is small enough to travel in meta.extra."""
    if _is_scalar(value):
        return True
    try:
        return len(json.dumps(value, default=str)) <= _EXTRA_MAX_BYTES
    except (TypeError, ValueError):
        return False


_PAGE_KEYS = ("next", "next_cursor", "cursor", "next_page", "next_page_token")


def envelope(payload, *, script="", command="", entry=None, parsed=True, exit_code=0, stderr="") -> dict:
    """Wrap one connector result (see the module docstring for the shape).

    `payload` is what the connector printed; `parsed` is False when its output was not JSON."""
    from datetime import datetime, timezone

    entry = entry or {}
    if not parsed:
        lines = (stderr or "").strip().splitlines()
        tail = scrub(lines[-1]) if lines else ""
        why = f": {tail[:200]}" if tail else ""
        if exit_code not in (0, None):
            return _failure(f"The connector exited with code {exit_code} and printed no result{why}",
                            hint="unknown", entry=entry)
        return _failure(f"The connector printed no result{why}", hint="unknown", entry=entry)

    if isinstance(payload, dict):
        err = payload.get("error")
        if err or payload.get("success") is False or payload.get("ok") is False:
            if isinstance(err, dict):
                err = err.get("message") or err.get("detail") or json.dumps(err)
            if not isinstance(err, str):
                err = None  # {"error": true, "message": "..."}: the flag is not the reason
            message = err or payload.get("message") or payload.get("detail")
            hint = None
            if not message:
                # A composite result ({"success": false, "parts": {"gdp": {"error": ...}, ...}}) says
                # why only in its parts: report those instead of a bare "failed".
                parts = _nested_errors(payload)
                if parts:
                    message = "Every part of the request failed - " + "; ".join(
                        f"{k}: {v}" for k, v in parts[:3]) + (f"; and {len(parts) - 3} more" if len(parts) > 3 else "")
                    hint = classify_error(parts[0][1])
            return _failure(str(message or "The connector reported a failure"), hint=hint, payload=payload,
                            entry=entry)

    extra = {}
    truncated = False
    cursor = None
    data = payload
    if isinstance(payload, dict):
        truncated = bool(payload.get("truncated"))
        for k in _PAGE_KEYS:
            if payload.get(k) not in (None, "", False):
                cursor = payload[k]
                break
        skip = ("success", "ok", "error", "truncated") + _PAGE_KEYS
        data = {k: v for k, v in payload.items() if k not in skip}
        if set(data) == {"data"} or ("data" in data and not _is_scalar(data["data"])):
            # {"data": ..., <descriptive fields>}: the payload is the "data" value. Fields too big to
            # be descriptive (a raw pivot kept next to its rows for an older caller) stay out of
            # meta and are named in extra.omitted; FINCEPT_CONNECTOR_RAW=1 returns them.
            extra = {k: v for k, v in data.items() if k != "data"}
            omitted = [k for k, v in extra.items() if not _small(v)]
            for k in omitted:
                extra.pop(k)
            if omitted:
                extra["omitted"] = omitted
            data = data["data"]

    if data is None or data == [] or data == {}:
        return _failure("The connector returned no data for this request", hint="no_data", entry=entry)

    out = {"ok": True}
    result, shape, info = _tabulate(data)
    if result is None:
        return _failure("The connector returned no data for this request", hint="no_data", entry=entry)
    if shape == "tables":
        # Several tables: each is offered by name; "data" stays the object they came from.
        out["data"] = data
        out["tables"] = {name: _table(rows) for name, rows in result.items()}
        count = sum(len(rows) for rows in result.values())
    else:
        table = _table(result)
        out["data"] = result
        out["columns"] = table["columns"]
        out["types"] = table["types"]
        count = len(result)
    extra["shape"] = shape
    for k, v in info.items():
        if k == "fields":
            extra.update({f: x for f, x in v.items() if f not in extra})
        else:
            extra[k] = v

    import os

    if os.environ.get("FINCEPT_CONNECTOR_RAW", "").strip() in ("1", "true", "yes"):
        out["raw"] = payload
    meta = {
        "source": entry.get("title") or (script[:-3] if script.endswith(".py") else script),
        "script": script,
        "command": command,
        "as_of": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "count": count,
        "truncated": truncated,
    }
    if cursor is not None:
        meta["next"] = cursor
    meta["extra"] = extra
    out["meta"] = meta
    return out


def _catalogue_entry(script):
    """The connector's row from resources/data_connectors.json when the file can be found."""
    import os

    here = os.path.dirname(os.path.abspath(__file__))
    # The host passes the resolved path (packaged layouts differ per platform); the directory
    # search is only the fallback for a bare run from a source or build tree.
    candidates = []
    explicit = os.environ.get("FINCEPT_CONNECTOR_CATALOG", "").strip()
    if explicit:
        candidates.append(explicit)
    candidates += [os.path.normpath(os.path.join(here, rel, "data_connectors.json"))
                   for rel in ("../resources", "../../resources", "..", "../Resources/resources", "../Resources")]
    for path in candidates:
        if os.path.isfile(path):
            try:
                with open(path, encoding="utf-8") as fh:
                    for row in json.load(fh).get("connectors", []):
                        if row.get("script") == script:
                            return row
            except (OSError, ValueError):
                pass
            break
    return {}


def _bind_child_lifetime():
    """Tie connector children to this wrapper so that killing the wrapper (cancel, time-out)
    never leaves the connector running.

    Windows: put this process in a job object with KILL_ON_JOB_CLOSE — children inherit the job
    and die when the last handle closes, i.e. when this process ends for any reason, including
    TerminateProcess. Returns the job handle, which the caller must keep referenced.
    POSIX: returns a preexec_fn that asks the kernel to send SIGTERM to the child when its
    parent dies (Linux; a no-op elsewhere — the signal handlers in run_enveloped cover an
    orderly stop there).
    """
    import os
    import sys

    if os.name == "nt":
        try:
            import ctypes
            from ctypes import wintypes

            k32 = ctypes.WinDLL("kernel32", use_last_error=True)

            class _Basic(ctypes.Structure):
                _fields_ = [("PerProcessUserTimeLimit", ctypes.c_int64),
                            ("PerJobUserTimeLimit", ctypes.c_int64),
                            ("LimitFlags", wintypes.DWORD),
                            ("MinimumWorkingSetSize", ctypes.c_size_t),
                            ("MaximumWorkingSetSize", ctypes.c_size_t),
                            ("ActiveProcessLimit", wintypes.DWORD),
                            ("Affinity", ctypes.c_size_t),
                            ("PriorityClass", wintypes.DWORD),
                            ("SchedulingClass", wintypes.DWORD)]

            class _Io(ctypes.Structure):
                _fields_ = [(n, ctypes.c_uint64) for n in
                            ("ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                             "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

            class _Extended(ctypes.Structure):
                _fields_ = [("BasicLimitInformation", _Basic), ("IoInfo", _Io),
                            ("ProcessMemoryLimit", ctypes.c_size_t), ("JobMemoryLimit", ctypes.c_size_t),
                            ("PeakProcessMemoryUsed", ctypes.c_size_t), ("PeakJobMemoryUsed", ctypes.c_size_t)]

            k32.CreateJobObjectW.restype = wintypes.HANDLE
            k32.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
            k32.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
            k32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
            k32.GetCurrentProcess.restype = wintypes.HANDLE
            job = k32.CreateJobObjectW(None, None)
            if not job:
                return None
            info = _Extended()
            info.BasicLimitInformation.LimitFlags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            if not k32.SetInformationJobObject(job, 9, ctypes.byref(info), ctypes.sizeof(info)):
                return None
            # Fails only when the host already placed us in a job that forbids nesting; the
            # host's own tree kill covers that case.
            k32.AssignProcessToJobObject(job, k32.GetCurrentProcess())
            return job
        except Exception:
            return None

    def _die_with_parent():  # pragma: no cover - runs in the child between fork and exec
        try:
            if sys.platform.startswith("linux"):
                import ctypes
                import signal

                ctypes.CDLL(None).prctl(1, signal.SIGTERM)  # PR_SET_PDEATHSIG
        except Exception:
            pass

    return _die_with_parent


def run_enveloped(argv) -> dict:
    """Run `<script.py> <command> [args...]` as a connector and return its envelope."""
    import os
    import subprocess
    import sys

    if not argv:
        return _failure("usage: fincept_connector.py run <script.py> <command> [args...]", hint="bad_args")
    here = os.path.dirname(os.path.abspath(__file__))
    script = argv[0].replace("\\", "/")
    target = os.path.normpath(os.path.join(here, script))
    try:
        inside = os.path.commonpath([here, target]) == here
    except ValueError:  # different drives on Windows
        inside = False
    if (not inside or not target.endswith(".py") or not os.path.isfile(target)
            or os.path.normcase(target) == os.path.normcase(os.path.abspath(__file__))):
        return _failure(f"Unknown connector script: {argv[0]}", hint="bad_args")
    command = argv[1] if len(argv) > 1 else ""
    entry = _catalogue_entry(script)
    env = dict(os.environ)
    env.setdefault("PYTHONIOENCODING", "utf-8")
    env["PYTHONPATH"] = here + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    try:
        limit = float(os.environ.get("FINCEPT_CONNECTOR_TIMEOUT_S", "0"))
    except ValueError:
        limit = 0.0
    import signal

    lifetime = _bind_child_lifetime()  # keep the reference: the job lives as long as we do
    kwargs = {"preexec_fn": lifetime} if callable(lifetime) else {}
    try:
        proc = subprocess.Popen([sys.executable, target] + list(argv[1:]), stdin=sys.stdin,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env, **kwargs)
    except OSError as exc:
        return _failure(f"Could not start the connector: {exc}", hint="unknown", entry=entry)

    def _stop(*_):
        # An orderly stop of the wrapper (terminate / Ctrl-Break / hang-up) stops the connector too.
        try:
            proc.kill()
        finally:
            os._exit(1)

    for name in ("SIGTERM", "SIGINT", "SIGBREAK", "SIGHUP"):
        sig = getattr(signal, name, None)
        if sig is not None:
            try:
                signal.signal(sig, _stop)
            except (ValueError, OSError):
                pass
    try:
        out, err = proc.communicate(timeout=limit if limit > 0 else None)
    except subprocess.TimeoutExpired:
        proc.kill()
        try:
            proc.communicate(timeout=5)
        except Exception:
            pass
        return _failure(f"{entry.get('title') or script} did not answer in time", hint="upstream_down",
                        entry=entry)
    stdout = out.decode("utf-8", "replace")
    stderr = err.decode("utf-8", "replace")
    payload, parsed = _last_json(stdout)
    return envelope(payload, script=script, command=command, entry=entry, parsed=parsed,
                    exit_code=proc.returncode, stderr=stderr)


def _emit(obj) -> None:
    """One JSON line on stdout; non-finite numbers become null."""
    import sys

    try:
        import fincept_json

        text = fincept_json.dumps(obj)
    except Exception:
        text = json.dumps(obj, default=str, ensure_ascii=False)
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    try:
        sys.stdout.write(text + "\n")
        sys.stdout.flush()
    except (BrokenPipeError, OSError):
        # The reader went away (cancelled run). Nothing to report and nobody to report it to:
        # leave quietly, without the interpreter's "Exception ignored ... BrokenPipeError" at exit.
        import os

        try:
            os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        except Exception:
            pass
        os._exit(0)


def _selftest() -> None:  # pragma: no cover - `python fincept_connector.py` runs it
    assert "SECRET123" not in scrub("for url: https://x.io/a?api_key=SECRET123&b=1", "SECRET123")
    assert scrub("https://x.io/a?apikey=abc&token=def") == "https://x.io/a?apikey=***&token=***"
    assert "series_key=GDP" in scrub("series_key=GDP")
    cases = {
        "FRED needs an API key: get one at https://fred.stlouisfed.org and save it as FRED_API_KEY "
        "in Settings > Credentials": "missing_key",
        "NOAA_CDO_TOKEN environment variable not set": "missing_key",
        "Finnhub rejected the API key (HTTP 401): check FINNHUB_API_KEY in Settings > Credentials": "auth_failed",
        "Semantic Scholar is rate-limiting requests (HTTP 429) - wait a minute and try again": "rate_limited",
        "Overpass API (overpass-api.de) is down or overloaded (HTTP 502) - try again later": "upstream_down",
        "World Bank did not answer in time - it may be down or overloaded; try again later": "upstream_down",
        "query required": "bad_args",
        "Unknown command: foo": "bad_args",
        "No command provided.": "bad_args",
        "No data found for symbols: ZZZZ": "no_data",
        "investment data is not supported: there is no open source for it": "not_supported",
        "something odd happened": "unknown",
        # found through the wrapper on the keyless connectors (2026-11-17)
        "Expecting value: line 1 column 1 (char 0)": "upstream_down",
        "Request failed: HTTPSConnectionPool(host='api.mnb.hu', port=443): Max retries exceeded with url: /x "
        "(Caused by NameResolutionError(\"Failed to resolve 'api.mnb.hu'\"))": "upstream_down",
        "Provide --name <group_name>. Available: ['monetary_policy']": "bad_args",
        "HTTP 400: 400 Client Error: Bad Request": "bad_args",
        "Either symbol or CIK must be provided": "bad_args",
        "HTTP 402: 402 Client Error: Payment Required": "not_supported",
        "No price data available for 2026-11-17. Treasury prices are published end-of-day": "no_data",
        "No economic events found": "no_data",
        "API message: The provided parameter value is not valid": "bad_args",
    }
    for message, want in cases.items():
        got = classify_error(message)
        assert got == want, (message, got, want)
    f = envelope(missing_key("FRED", "https://fred.stlouisfed.org/docs/api/api_key.html", "FRED_API_KEY"))
    assert f["ok"] is False and f["code"] == "missing_key" and f["env_key"] == "FRED_API_KEY" and f["docs_url"]
    ok = envelope([{"date": "2026-01-01", "value": 1.0}, {"date": "2026-01-02", "value": 2, "note": "x"}],
                  script="x_data.py", command="series")
    assert ok["ok"] and ok["columns"] == ["date", "value", "note"] and ok["meta"]["count"] == 2
    assert ok["types"] == {"date": "date", "value": "number", "note": "string"}
    assert ok["meta"]["extra"] == {"shape": "rows"} and "raw" not in ok
    wrapped = envelope({"success": True, "data": [{"a": 1}], "series_id": "GDP", "next_cursor": "p2"})
    assert wrapped["data"] == [{"a": 1}] and wrapped["meta"]["next"] == "p2"
    assert wrapped["meta"]["extra"] == {"series_id": "GDP", "shape": "rows"}
    rec = envelope({"price": 1.5, "symbol": "X", "ok_flag": True})
    assert rec["data"] == [{"price": 1.5, "symbol": "X", "ok_flag": True}]
    assert rec["meta"]["extra"]["shape"] == "single_record" and rec["types"]["ok_flag"] == "bool"
    assert envelope([])["code"] == "no_data" and envelope({"data": []})["code"] == "no_data"
    one = envelope({"series_id": "GDP", "observations": [{"date": "2026-01-01", "value": 1.0}], "count": 1})
    assert one["data"] == [{"date": "2026-01-01", "value": 1.0}] and one["columns"] == ["date", "value"]
    assert one["meta"]["extra"] == {"shape": "nested_table", "rows_key": "observations",
                                    "series_id": "GDP", "count": 1}
    deep = envelope({"result": {"data": {"rows": [{"x": 1}]}}})
    assert deep["data"] == [{"x": 1}] and deep["meta"]["extra"]["rows_key"] == "result.data.rows"
    colo = envelope({"dates": ["2026-01-01", "2026-01-02"], "values": [1, 2], "unit": "USD"})
    assert colo["data"] == [{"dates": "2026-01-01", "values": 1}, {"dates": "2026-01-02", "values": 2}]
    assert colo["meta"]["extra"] == {"shape": "column_oriented", "unit": "USD"}
    dod = envelope({"USA": {"gdp": 1.0}, "IND": {"gdp": 2.0, "key": "k"}})
    assert dod["data"] == [{"key": "USA", "gdp": 1.0}, {"key": "IND", "gdp": 2.0, "key_": "k"}]
    assert dod["meta"]["extra"]["shape"] == "dict_of_dicts"
    kv = envelope({"2021": 1.0, "2022": 2.0, "2023": None})
    assert kv["data"][0] == {"key": "2021", "value": 1.0} and kv["meta"]["extra"]["shape"] == "key_value"
    lol = envelope([["date", "close"], ["2026-01-01", 1.5]])
    assert lol["data"] == [{"date": "2026-01-01", "close": 1.5}] and lol["meta"]["extra"]["shape"] == "list_of_lists"
    assert envelope(["a", "b"])["data"] == [{"value": "a"}, {"value": "b"}]
    two = envelope({"a": [{"x": 1}], "b": {"rows": [{"y": 2}, {"y": 3, "z": "2026-01-01T10:00:00Z"}]}, "note": "n"})
    assert isinstance(two["data"], dict) and "columns" not in two and two["meta"]["count"] == 3
    assert two["tables"]["a"] == {"columns": ["x"], "types": {"x": "integer"}, "rows": [{"x": 1}]}
    assert two["tables"]["b"]["types"] == {"y": "integer", "z": "datetime"}
    assert two["meta"]["extra"]["shape"] == "tables"
    wrap = envelope({"query": "x", "papers": [{"id": 1}, {"id": 2}], "total": 2})
    assert wrap["data"] == [{"id": 1}, {"id": 2}]
    assert wrap["meta"]["extra"] == {"shape": "nested_table", "rows_key": "papers", "query": "x", "total": 2}
    fx = envelope({"base": "EUR", "date": "2026-11-13", "rates": {"USD": 1.1, "INR": 97.0}})
    assert fx["data"] == [{"key": "USD", "value": 1.1}, {"key": "INR", "value": 97.0}]
    assert fx["meta"]["extra"] == {"shape": "nested_table", "rows_key": "rates", "inner_shape": "key_value",
                                   "base": "EUR", "date": "2026-11-13"}
    cg = envelope({"bitcoin": {"usd": 1.0, "eur": 2.0}})
    assert cg["data"] == [{"key": "usd", "value": 1.0}, {"key": "eur", "value": 2.0}]
    assert cg["meta"]["extra"]["rows_key"] == "bitcoin"
    multi = envelope({"indicators": {"gdp": {"v": 1}, "cpi": {"v": 2}}, "years": [2020, 2021], "n": 2})
    assert set(multi["tables"]) == {"indicators", "years"} and multi["meta"]["extra"]["n"] == 2
    assert multi["tables"]["indicators"]["rows"] == [{"key": "gdp", "v": 1}, {"key": "cpi", "v": 2}]
    gone = envelope({"error": "HTTP 403: Forbidden for url: https://x.io/a?key=abc"},
                    entry={"broken": "Upstream needs a login"})
    assert gone["code"] == "not_supported" and gone["error"] == "Upstream needs a login"
    assert "x.io" not in envelope({"error": "HTTP 500 Server Error for url: https://x.io/a"})["error"]
    assert envelope(None, parsed=False, exit_code=1, stderr="Traceback\nKeyError: 'x'")["code"] == "unknown"
    assert envelope({"error": {"message": "HTTP 429 too many requests"}})["retry_after_s"] == 60
    # HAL paging envelope: the table is _embedded.<name>; links and page info are not rows
    hal = envelope({"success": True, "data": {"_links": {"self": {"href": "h"}, "next": {"href": "n"}},
                                              "page": {"totalElements": 2, "number": 1},
                                              "_embedded": {"instruments": [{"id": 1}, {"id": 2}]}}})
    assert hal["data"] == [{"id": 1}, {"id": 2}] and hal["meta"]["extra"]["rows_key"] == "_embedded.instruments", hal
    # the one record inside a wrapper is the row, not an object cell of a one-row table
    cur = envelope({"symbol": "VIX", "current": {"DATE": "2026-11-16", "CLOSE": 22.9}})
    assert cur["data"] == [{"DATE": "2026-11-16", "CLOSE": 22.9}] and cur["types"]["DATE"] == "date"
    assert cur["meta"]["extra"] == {"shape": "nested_table", "rows_key": "current", "inner_shape": "single_record",
                                    "symbol": "VIX"}, cur["meta"]["extra"]
    # typing helpers for connectors
    assert number("12") == 12 and number("-1.50") == -1.5 and number("") is None and number("1e3") == 1000.0
    assert number("12a") == "12a" and number(7) == 7
    nr = numeric_columns([{"year": "2020", "v": "1.5", "id": "42", "zip": "02134", "name": "x", "mix": "1"},
                          {"year": "2021", "v": "", "id": "43", "zip": "10001", "name": "y", "mix": "n/a"}])
    assert nr[0] == {"year": 2020, "v": 1.5, "id": "42", "zip": "02134", "name": "x", "mix": "1"}, nr
    assert nr[1]["v"] is None and nr[1]["year"] == 2021
    assert iso_date("17/11/2021", "%d/%m/%Y") == "2021-11-17" and iso_date("04-Jan-2011", "%d-%b-%Y") == "2011-01-04"
    assert iso_date("bad", "%d/%m/%Y") == "bad" and iso_date("2021-11-17 10:30", "%Y-%m-%d %H:%M") == "2021-11-17T10:30:00Z"
    assert epoch_iso(1700000000) == "2023-11-14T22:13:20Z" and epoch_iso("1700000000123") == "2023-11-14T22:13:20.123Z"
    assert epoch_iso(None) is None and epoch_iso("x") == "x"
    # a failed composite result names its failed parts and is classified by them
    parts = envelope({"success": False, "country": "x", "endpoints": {
        "gdp": {"success": False, "error": "HTTP error: 503 Server Error: Service Unavailable"},
        "cpi": {"success": False, "error": "Failed to process data"}}})
    assert parts["code"] == "upstream_down" and "gdp: HTTP error: 503" in parts["error"], parts
    assert envelope({"success": False})["error"] == "The connector reported a failure"
    flag = envelope({"error": True, "endpoint": "ipo", "message": "Nasdaq did not answer in time"})
    assert flag["error"] == "Nasdaq did not answer in time" and flag["code"] == "upstream_down", flag
    # a large raw object kept next to "data" for an older caller stays out of meta.extra
    pivot = {"USA": {str(y): float(y) for y in range(1950, 2030)}, "IND": {str(y): 1.0 for y in range(1950, 2030)}}
    big = envelope({"data": [{"country": "USA", "year": 2020, "value": 1.0}], "values": pivot, "unit": "%"})
    assert big["meta"]["extra"] == {"unit": "%", "omitted": ["values"], "shape": "rows"}, big["meta"]["extra"]
    print(json.dumps({"ok": True}))


if __name__ == "__main__":
    import sys as _sys

    if len(_sys.argv) > 1 and _sys.argv[1] == "run":
        _emit(run_enveloped(_sys.argv[2:]))
    else:
        _selftest()
