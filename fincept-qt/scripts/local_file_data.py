"""Local File Reader
Reads a data file on this computer as a table: CSV / TSV (delimiter, encoding and header row
sniffed), JSON and NDJSON, Excel (.xlsx / .xlsm through openpyxl, .xls through xlrd), Parquet and
Arrow IPC / Feather (pyarrow).

Usage:
    python local_file_data.py read <path> [limit=1000] [sheet]
    python local_file_data.py schema <path> [sheet]
    python local_file_data.py sheets <path>

read    up to `limit` rows (1..100000). Files are streamed, never loaded whole (JSON documents
        excepted - they are parsed in one go up to 256 MB); `truncated` says rows were left out
        and `total_rows` gives the file's row count when it is cheap to know (Parquet / Arrow
        metadata, Excel sheet dimensions, text files up to 64 MB), else null.
schema  one row per column: name, type, null count and an example, from the first 1000 rows.
sheets  the worksheets of an Excel file (one row for any other format).
`sheet` is a worksheet name or 1-based number (Excel only; default the first sheet).

Values come back typed: whole numbers as integers, decimals as numbers (European "1.234,5" in
semicolon files too), true/false as booleans, dates as YYYY-MM-DD and date-times as
YYYY-MM-DDTHH:MM:SS (with Z when the file says the time is UTC or carries an offset). A text
column is only converted when every value in it converts, so codes such as "007" stay text, and
day/month order is decided per column (left as text when the column cannot tell).

Read-only by construction: the file is opened for reading and nothing else - nothing is written,
executed or imported from it, no URL is followed, and Windows network (UNC) and device paths are
refused (a network share mapped to a drive letter works). Failures are {"error", "code"} with code
bad_args (no path / not found / a folder / not readable / not a supported data file), no_data (an
empty file) or not_supported (the library for the format is not installed).
"""

import csv
import datetime as _dt
import io
import json
import math
import os
import re
import stat
import sys

DEFAULT_LIMIT = 1000
MAX_LIMIT = 100000
JSON_MAX_BYTES = 256 * 1024 * 1024
COUNT_MAX_BYTES = 64 * 1024 * 1024
SAMPLE_BYTES = 64 * 1024

TEXT_EXT = {".csv": None, ".tsv": "\t", ".tab": "\t", ".txt": None, ".psv": "|", ".dat": None}
JSON_EXT = {".json"}
NDJSON_EXT = {".ndjson", ".jsonl", ".jsonlines"}
XLSX_EXT = {".xlsx", ".xlsm"}
XLS_EXT = {".xls"}
PARQUET_EXT = {".parquet", ".parq", ".pq"}
ARROW_EXT = {".arrow", ".feather", ".ipc", ".arrows"}
WINDOWS_DEVICES = {"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$"} | {f"COM{i}" for i in range(1, 10)} | {
    f"LPT{i}" for i in range(1, 10)}
NULL_TOKENS = {"", "na", "n/a", "nan", "null", "none", "-", "--", "..", "#n/a"}


class ReaderError(Exception):
    def __init__(self, message, code="bad_args", **extra):
        super().__init__(message)
        self.code = code
        self.extra = extra


# ── path checks ──────────────────────────────────────────────────────────────

def check_path(raw):
    """(absolute path, size) of a regular local file the process can read, else ReaderError."""
    p = (raw or "").strip().strip('"').strip("'")
    if not p:
        raise ReaderError("Give the path of a data file (CSV, TSV, JSON, NDJSON, Excel, Parquet or Arrow/Feather)")
    if re.match(r"^[A-Za-z][A-Za-z0-9+.-]+://", p):
        raise ReaderError("Only files on this computer are read - URLs are not followed")
    if p.startswith(("\\\\", "//")):
        raise ReaderError("Network (UNC) and device paths are not read: copy the file to a local folder "
                          "or open it through a mapped drive letter")
    if os.name == "nt" and os.path.basename(p.replace("/", "\\")).split(".")[0].strip().upper() in WINDOWS_DEVICES:
        raise ReaderError(f"{os.path.basename(p)} is a Windows device name, not a file")
    p = os.path.abspath(os.path.expanduser(p))
    if p.startswith(("\\\\", "//")):
        raise ReaderError("Network (UNC) and device paths are not read")
    if not os.path.exists(p):
        raise ReaderError(f"No such file: {p}")
    if os.path.isdir(p):
        raise ReaderError(f"{p} is a folder - give the path of a file inside it")
    try:
        st = os.stat(p)
    except OSError as exc:
        raise ReaderError(f"Cannot read {p}: {exc.strerror or exc}")
    if not stat.S_ISREG(st.st_mode):
        raise ReaderError(f"{p} is not a regular file")
    try:
        with open(p, "rb") as fh:
            fh.read(1)
    except OSError as exc:
        raise ReaderError(f"Cannot read {p}: {exc.strerror or exc}")
    return p, st.st_size


def detect_format(path, size):
    ext = os.path.splitext(path)[1].lower()
    if ext in TEXT_EXT:
        return "csv"
    if ext in JSON_EXT:
        return "json"
    if ext in NDJSON_EXT:
        return "ndjson"
    if ext in XLSX_EXT:
        return "xlsx"
    if ext in XLS_EXT:
        return "xls"
    if ext in PARQUET_EXT:
        return "parquet"
    if ext in ARROW_EXT:
        return "arrow"
    with open(path, "rb") as fh:
        head = fh.read(8)
    if head[:4] == b"PAR1":
        return "parquet"
    if head[:6] == b"ARROW1":
        return "arrow"
    if head[:4] == b"PK\x03\x04":
        return "xlsx"
    if head[:4] == b"\xd0\xcf\x11\xe0":
        return "xls"
    raise ReaderError(f"{os.path.basename(path)} is not a supported data file: use .csv, .tsv, .txt, .json, "
                      ".ndjson/.jsonl, .xlsx/.xlsm/.xls, .parquet or .arrow/.feather", code="not_supported")


# ── value typing ─────────────────────────────────────────────────────────────

_INT = re.compile(r"^[+-]?(0|[1-9]\d*)$")
_LEADING_ZERO = re.compile(r"^[+-]?0\d")
_FLOAT = re.compile(r"^[+-]?(\d+\.\d*|\.\d+|\d+)([eE][+-]?\d+)?$")
_THOUSANDS = re.compile(r"^[+-]?\d{1,3}(,\d{3})+(\.\d+)?$")
_EURO = re.compile(r"^[+-]?(\d{1,3}(\.\d{3})+|\d+)(,\d+)?$")
_ISO_DATE = re.compile(r"^(\d{4})-(\d{2})-(\d{2})$")
_ISO_DT = re.compile(r"^(\d{4})-(\d{2})-(\d{2})[T ](\d{2}):(\d{2})(?::(\d{2})(\.\d+)?)?\s*(Z|[+-]\d{2}:?\d{2})?$")
_SLASH = re.compile(r"^(\d{1,2})[/.-](\d{1,2})[/.-](\d{4})(?:[ T](\d{1,2}):(\d{2})(?::(\d{2}))?)?$")
_YMD_SLASH = re.compile(r"^(\d{4})/(\d{1,2})/(\d{1,2})$")
_BOOL = {"true": True, "false": False}


def _iso_from_dt(d):
    if isinstance(d, _dt.datetime):
        if d.tzinfo is not None:
            d = d.astimezone(_dt.timezone.utc).replace(tzinfo=None)
            return d.isoformat(timespec="seconds") + "Z"
        if d.hour == d.minute == d.second == d.microsecond == 0:
            return d.date().isoformat()
        return d.isoformat(timespec="seconds")
    if isinstance(d, _dt.date):
        return d.isoformat()
    if isinstance(d, _dt.time):
        return d.isoformat(timespec="seconds")
    return d


def _valid_date(y, m, d):
    try:
        _dt.date(int(y), int(m), int(d))
        return True
    except ValueError:
        return False


def _column_converter(values, delimiter=None):
    """A function turning this column's text values into typed values, or None to leave it as text.
    `values` are the non-null strings of the column."""
    vals = [v.strip() for v in values]
    if not vals:
        return None
    low = [v.lower() for v in vals]
    if all(v in _BOOL for v in low):
        return lambda s: _BOOL[s.strip().lower()]
    numeric = all(_FLOAT.match(v) or _INT.match(v) for v in vals)
    if numeric and any(_LEADING_ZERO.match(v) for v in vals):
        return None  # codes such as "007" or ZIP codes: converting would lose the zeros
    if all(_INT.match(v) for v in vals):
        return lambda s: int(s.strip())
    if all(_FLOAT.match(v) or _INT.match(v) for v in vals):
        return lambda s: float(s.strip())
    if delimiter != "," and all(_THOUSANDS.match(v) or _INT.match(v) or _FLOAT.match(v) for v in vals) \
            and any(_THOUSANDS.match(v) for v in vals):
        return lambda s: float(s.strip().replace(",", ""))
    if delimiter == ";" and all(_EURO.match(v) for v in vals) and any("," in v for v in vals):
        return lambda s: float(s.strip().replace(".", "").replace(",", "."))
    if all(_ISO_DATE.match(v) and _valid_date(*_ISO_DATE.match(v).groups()) for v in vals):
        return lambda s: s.strip()
    if all(_ISO_DT.match(v) for v in vals):
        def iso_dt(s):
            m = _ISO_DT.match(s.strip())
            y, mo, d, hh, mi, ss, frac, tz = m.groups()
            base = _dt.datetime(int(y), int(mo), int(d), int(hh), int(mi), int(ss or 0))
            if tz:
                if tz == "Z":
                    return base.isoformat(timespec="seconds") + "Z"
                sign = 1 if tz[0] == "+" else -1
                t = tz[1:].replace(":", "")
                off = _dt.timedelta(hours=int(t[:2]), minutes=int(t[2:4] or 0)) * sign
                return (base - off).isoformat(timespec="seconds") + "Z"
            return _iso_from_dt(base)
        return iso_dt
    if all(_YMD_SLASH.match(v) and _valid_date(*_YMD_SLASH.match(v).groups()) for v in vals):
        return lambda s: "%04d-%02d-%02d" % tuple(int(x) for x in _YMD_SLASH.match(s.strip()).groups())
    ms = [_SLASH.match(v) for v in vals]
    if all(ms):
        first = [int(m.group(1)) for m in ms]
        second = [int(m.group(2)) for m in ms]
        if any(f > 12 for f in first) and all(s <= 12 for s in second):
            order = "dmy"
        elif any(s > 12 for s in second) and all(f <= 12 for f in first):
            order = "mdy"
        else:
            return None  # every value fits both day/month orders: leave the text alone

        def slash(s):
            m = _SLASH.match(s.strip())
            a, b, y, hh, mi, ss = m.groups()
            d, mo = (a, b) if order == "dmy" else (b, a)
            if hh is None:
                return "%04d-%02d-%02d" % (int(y), int(mo), int(d))
            return _iso_from_dt(_dt.datetime(int(y), int(mo), int(d), int(hh), int(mi), int(ss or 0)))
        if all(_valid_date(*((m.group(3), m.group(2), m.group(1)) if order == "dmy" else
                              (m.group(3), m.group(1), m.group(2)))) for m in ms):
            return slash
    return None


def type_rows(rows, columns, delimiter=None):
    """Convert text columns in place (a column converts only when every value converts)."""
    for c in columns:
        texts = []
        mixed = False
        for r in rows:
            v = r.get(c)
            if v is None:
                continue
            if not isinstance(v, str):
                mixed = True
                break
            if v.strip().lower() in NULL_TOKENS:
                continue
            texts.append(v)
        if mixed or not texts:
            continue
        conv = _column_converter(texts, delimiter)
        if conv is None:
            continue
        for r in rows:
            v = r.get(c)
            if isinstance(v, str):
                r[c] = None if v.strip().lower() in NULL_TOKENS else conv(v)
    return rows


def plain(v):
    """A JSON-safe scalar for a value from a binary format."""
    if v is None or isinstance(v, (bool, int, str)):
        return v
    if isinstance(v, float):
        return v if math.isfinite(v) else None
    if isinstance(v, (_dt.datetime, _dt.date, _dt.time)):
        return _iso_from_dt(v)
    if isinstance(v, _dt.timedelta):
        return v.total_seconds()
    try:
        import decimal
        if isinstance(v, decimal.Decimal):
            f = float(v)
            return f if math.isfinite(f) else None
    except ImportError:
        pass
    if isinstance(v, (bytes, bytearray)):
        return v.hex()
    if isinstance(v, dict):
        return {str(k): plain(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [plain(x) for x in v]
    if hasattr(v, "item"):  # numpy scalar
        try:
            return plain(v.item())
        except Exception:
            pass
    return str(v)


def unique_names(names):
    out, seen = [], {}
    for i, n in enumerate(names):
        n = ("" if n is None else str(n)).strip() or f"col_{i + 1}"
        if n in seen:
            seen[n] += 1
            n = f"{n}_{seen[n]}"
        else:
            seen[n] = 1
        out.append(n)
    return out


# ── text files ───────────────────────────────────────────────────────────────

def sniff_encoding(path):
    with open(path, "rb") as fh:
        raw = fh.read(SAMPLE_BYTES)
    if raw.startswith(b"\xef\xbb\xbf"):
        return "utf-8-sig", raw
    if raw.startswith((b"\xff\xfe\x00\x00", b"\x00\x00\xfe\xff")):
        return "utf-32", raw
    if raw.startswith((b"\xff\xfe", b"\xfe\xff")):
        return "utf-16", raw
    import codecs
    try:
        codecs.getincrementaldecoder("utf-8")().decode(raw, final=False)
        return "utf-8", raw
    except UnicodeDecodeError:
        pass
    try:
        raw.decode("cp1252")
        return "cp1252", raw
    except UnicodeDecodeError:
        return "latin-1", raw


def sniff_csv(path, sample, ext_delim):
    lines = [l for l in sample.splitlines() if l.strip()][:50]
    if not lines:
        raise ReaderError(f"{os.path.basename(path)} is empty", code="no_data")
    text = "\n".join(lines)
    delim = ext_delim
    if delim is None:
        try:
            delim = csv.Sniffer().sniff(text, delimiters=",;\t|").delimiter
        except csv.Error:
            counts = {d: min(l.count(d) for l in lines[:20]) for d in ",;\t|"}
            delim = max(counts, key=counts.get) if max(counts.values()) > 0 else ","
    rows = list(csv.reader(io.StringIO(text), delimiter=delim))
    header = True
    if len(rows) > 1:
        first, rest = rows[0], rows[1:]

        def numeric(s):
            s = s.strip()
            return bool(_INT.match(s) or _FLOAT.match(s) or _ISO_DATE.match(s) or _THOUSANDS.match(s))
        if any(numeric(c) for c in first if c.strip()):
            header = False
        elif not any(numeric(c) for r in rest for c in r if c.strip()):
            try:
                header = csv.Sniffer().has_header(text)
            except csv.Error:
                header = True
    return delim, header


def open_text(path, encoding):
    return open(path, "r", encoding=encoding, errors="replace", newline="")


def read_csv(path, size, limit):
    encoding, raw = sniff_encoding(path)
    try:
        sample = raw.decode(encoding if encoding != "utf-8" else "utf-8", errors="replace")
    except LookupError:
        sample = raw.decode("latin-1")
    ext_delim = TEXT_EXT.get(os.path.splitext(path)[1].lower())
    delim, header = sniff_csv(path, sample, ext_delim)
    csv.field_size_limit(min(sys.maxsize, 2 ** 31 - 1))
    rows, names, total, truncated = [], None, None, False
    with open_text(path, encoding) as fh:
        reader = csv.reader(fh, delimiter=delim)
        width = 0
        for rec in reader:
            if not any(c.strip() for c in rec):
                continue
            if names is None and header:
                names = unique_names(rec)
                continue
            if len(rows) < limit:
                rows.append(rec)
                width = max(width, len(rec))
                continue
            truncated = True
            break
        if truncated and size <= COUNT_MAX_BYTES:
            extra = 1 + sum(1 for rec in reader if any(c.strip() for c in rec))
            total = len(rows) + extra
        elif not truncated:
            total = len(rows)
    if names is None:
        names = [f"col_{i + 1}" for i in range(width)]
    elif width > len(names):
        names = unique_names(names + [f"col_{i + 1}" for i in range(len(names), width)])
    out = [{n: (rec[i] if i < len(rec) else None) for i, n in enumerate(names)} for rec in rows]
    type_rows(out, names, delim)
    info = {"format": "csv", "encoding": encoding.replace("-sig", ""), "delimiter": delim, "header": header}
    return names, out, total, truncated, info


def read_ndjson(path, size, limit):
    encoding, _ = sniff_encoding(path)
    rows, names, total, truncated, bad = [], [], None, False, 0
    seen = set()
    with open_text(path, encoding) as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            if len(rows) >= limit:
                truncated = True
                break
            try:
                obj = json.loads(line)
            except ValueError:
                bad += 1
                if bad > 5 and not rows:
                    raise ReaderError(f"{os.path.basename(path)} is not NDJSON (one JSON object per line)")
                continue
            if not isinstance(obj, dict):
                obj = {"value": obj}
            rows.append(obj)
            for k in obj:
                if k not in seen:
                    seen.add(k)
                    names.append(k)
        if truncated and size <= COUNT_MAX_BYTES:
            total = len(rows) + 1 + sum(1 for l in fh if l.strip())
        elif not truncated:
            total = len(rows)
    rows = [{n: plain(r.get(n)) for n in names} for r in rows]
    type_rows(rows, names)
    return names, rows, total, truncated, {"format": "ndjson", "encoding": encoding.replace("-sig", "")}


def _json_rows(doc):
    """(rows, rows_key) for the table a JSON document holds."""
    if isinstance(doc, list):
        if doc and all(isinstance(r, dict) for r in doc):
            return doc, None
        if doc and all(isinstance(r, list) for r in doc):
            head = doc[0]
            if len(doc) > 1 and all(isinstance(h, str) for h in head):
                names = unique_names(head)
                return [dict(zip(names, r)) for r in doc[1:]], None
            return [{f"col_{i + 1}": v for i, v in enumerate(r)} for r in doc], None
        return [{"value": v} for v in doc], None
    if isinstance(doc, dict):
        for k, v in doc.items():
            if isinstance(v, list) and v and all(isinstance(r, dict) for r in v):
                return v, k
        if doc and all(isinstance(v, dict) for v in doc.values()):
            return [dict({"key": k}, **v) for k, v in doc.items()], None
        return [doc], None
    return [{"value": doc}], None


def read_json(path, size, limit):
    if size > JSON_MAX_BYTES:
        raise ReaderError(f"{os.path.basename(path)} is {size // (1024 * 1024)} MB: JSON documents are read whole, "
                          f"up to {JSON_MAX_BYTES // (1024 * 1024)} MB - save it as NDJSON (one object per line) "
                          "to read it in parts")
    encoding, raw = sniff_encoding(path)
    if raw.lstrip()[:1] not in (b"{", b"[") and encoding.startswith("utf-8"):
        raise ReaderError(f"{os.path.basename(path)} does not contain a JSON document")
    with open_text(path, encoding) as fh:
        try:
            doc = json.load(fh)
        except ValueError as exc:
            # one object per line saved as .json
            fh.seek(0)
            first = fh.readline().strip()
            try:
                json.loads(first)
                return read_ndjson(path, size, limit)
            except ValueError:
                raise ReaderError(f"{os.path.basename(path)} is not valid JSON: {exc}")
    rows, key = _json_rows(doc)
    total = len(rows)
    truncated = total > limit
    rows = rows[:limit]
    names, seen = [], set()
    for r in rows:
        for k in r:
            if k not in seen:
                seen.add(k)
                names.append(k)
    rows = [{n: plain(r.get(n)) for n in names} for r in rows]
    type_rows(rows, names)
    info = {"format": "json", "encoding": encoding.replace("-sig", "")}
    if key:
        info["rows_key"] = key
    return names, rows, total, truncated, info


# ── Excel ────────────────────────────────────────────────────────────────────

def _need(module, fmt, package):
    try:
        return __import__(module)
    except ImportError:
        raise ReaderError(f"Reading {fmt} files needs the Python package '{package}', which is not installed",
                          code="not_supported", package=package)


def _pick_sheet(names, sheet):
    if sheet in (None, ""):
        return 0
    s = str(sheet).strip()
    if s in names:
        return names.index(s)
    low = [n.lower() for n in names]
    if s.lower() in low:
        return low.index(s.lower())
    if s.isdigit() and 1 <= int(s) <= len(names):
        return int(s) - 1
    raise ReaderError(f"No sheet '{s}': the workbook has {', '.join(names)}")


def _table_from_cells(cell_rows, limit):
    """Header detection + typing for rows of cell values (Excel)."""
    rows_iter = iter(cell_rows)
    first = None
    for r in rows_iter:
        if r is not None and any(c not in (None, "") for c in r):
            first = list(r)
            break
    if first is None:
        return [], [], 0, False, False
    header = all(isinstance(c, str) or c is None for c in first) and any(isinstance(c, str) for c in first)
    body, truncated, width = [], False, len(first)
    if not header:
        body.append(first)
    for r in rows_iter:
        if r is None or not any(c not in (None, "") for c in r):
            continue
        if len(body) >= limit:
            truncated = True
            break
        body.append(list(r))
        width = max(width, len(r))
    names = unique_names(first + [None] * (width - len(first))) if header else [f"col_{i + 1}" for i in range(width)]
    rows = [{n: plain(rec[i]) if i < len(rec) else None for i, n in enumerate(names)} for rec in body]
    while names and all(r.get(names[-1]) is None for r in rows) and names[-1].startswith("col_"):
        name = names.pop()
        for r in rows:
            r.pop(name, None)
    type_rows(rows, names)
    return names, rows, len(body), truncated, header


def read_xlsx(path, size, limit, sheet=None, list_only=False):
    openpyxl = _need("openpyxl", "Excel (.xlsx)", "openpyxl")
    try:
        wb = openpyxl.load_workbook(path, read_only=True, data_only=True)
    except Exception as exc:
        raise ReaderError(f"{os.path.basename(path)} could not be opened as an Excel workbook: {exc}")
    try:
        names = wb.sheetnames
        if list_only:
            out = []
            for i, n in enumerate(names):
                ws = wb[n]
                out.append({"sheet": n, "index": i + 1, "rows": ws.max_row, "columns": ws.max_column,
                            "visible": getattr(ws, "sheet_state", "visible") == "visible"})
            return out
        idx = _pick_sheet(names, sheet)
        ws = wb[names[idx]]
        max_row = ws.max_row
        cols, rows, n, truncated, header = _table_from_cells(ws.iter_rows(values_only=True), limit)
        total = None
        if truncated and max_row:
            total = max_row - (1 if header else 0)
        elif not truncated:
            total = n
        return cols, rows, total, truncated, {"format": "xlsx", "sheet": names[idx], "header": header}
    finally:
        wb.close()


def read_xls(path, size, limit, sheet=None, list_only=False):
    xlrd = _need("xlrd", "Excel 97-2003 (.xls)", "xlrd")
    try:
        book = xlrd.open_workbook(path, on_demand=True)
    except Exception as exc:
        raise ReaderError(f"{os.path.basename(path)} could not be opened as an .xls workbook: {exc}")
    try:
        names = book.sheet_names()
        if list_only:
            return [{"sheet": n, "index": i + 1, "rows": book.sheet_by_index(i).nrows,
                     "columns": book.sheet_by_index(i).ncols, "visible": True} for i, n in enumerate(names)]
        idx = _pick_sheet(names, sheet)
        sh = book.sheet_by_index(idx)

        def cells():
            for r in range(sh.nrows):
                out = []
                for c in range(sh.ncols):
                    cell = sh.cell(r, c)
                    if cell.ctype == xlrd.XL_CELL_DATE:
                        try:
                            out.append(xlrd.xldate.xldate_as_datetime(cell.value, book.datemode))
                        except Exception:
                            out.append(cell.value)
                    elif cell.ctype in (xlrd.XL_CELL_EMPTY, xlrd.XL_CELL_BLANK):
                        out.append(None)
                    elif cell.ctype == xlrd.XL_CELL_NUMBER and float(cell.value).is_integer():
                        out.append(int(cell.value))
                    elif cell.ctype == xlrd.XL_CELL_BOOLEAN:
                        out.append(bool(cell.value))
                    else:
                        out.append(cell.value)
                yield out
        cols, rows, n, truncated, header = _table_from_cells(cells(), limit)
        total = (sh.nrows - (1 if header else 0)) if truncated else n
        return cols, rows, total, truncated, {"format": "xls", "sheet": names[idx], "header": header}
    finally:
        book.release_resources()


# ── Parquet / Arrow ──────────────────────────────────────────────────────────

def _arrow_rows(batches, limit):
    rows, truncated = [], False
    for b in batches:
        for r in b.to_pylist():
            if len(rows) >= limit:
                truncated = True
                break
            rows.append({k: plain(v) for k, v in r.items()})
        if truncated:
            break
    return rows, truncated


def read_parquet(path, size, limit):
    _need("pyarrow", "Parquet", "pyarrow")
    import pyarrow.parquet as pq
    try:
        pf = pq.ParquetFile(path)
    except Exception as exc:
        raise ReaderError(f"{os.path.basename(path)} could not be opened as Parquet: {exc}")
    names = list(pf.schema_arrow.names)
    rows, truncated = _arrow_rows(pf.iter_batches(batch_size=max(1, min(limit, 65536))), limit)
    total = pf.metadata.num_rows
    return names, rows, total, truncated or total > len(rows), {
        "format": "parquet", "arrow_types": {f.name: str(f.type) for f in pf.schema_arrow}}


def read_arrow(path, size, limit):
    pa = _need("pyarrow", "Arrow/Feather", "pyarrow")
    import pyarrow.ipc as ipc
    source = pa.memory_map(path, "r")
    try:
        try:
            reader = ipc.open_file(source)
            total = sum(reader.get_batch(i).num_rows for i in range(reader.num_record_batches))
            batches = (reader.get_batch(i) for i in range(reader.num_record_batches))
            schema = reader.schema
            kind = "arrow"
        except Exception:
            source.seek(0)
            try:
                reader = ipc.open_stream(source)
            except Exception:
                try:
                    import pyarrow.feather as feather
                    table = feather.read_table(path)  # Feather v1
                except Exception as exc:
                    raise ReaderError(f"{os.path.basename(path)} could not be opened as Arrow/Feather: {exc}")
                rows, truncated = _arrow_rows(table.to_batches(), limit)
                return table.schema.names, rows, table.num_rows, truncated, {
                    "format": "feather", "arrow_types": {f.name: str(f.type) for f in table.schema}}
            total = None
            batches = reader
            schema = reader.schema
            kind = "arrow_stream"
        rows, truncated = _arrow_rows(batches, limit)
        return list(schema.names), rows, total, truncated, {
            "format": kind, "arrow_types": {f.name: str(f.type) for f in schema}}
    finally:
        source.close()


# ── commands ─────────────────────────────────────────────────────────────────

def _limit(raw):
    if raw in (None, ""):
        return DEFAULT_LIMIT
    try:
        n = int(str(raw).strip())
    except ValueError:
        raise ReaderError(f"limit must be a whole number, got '{raw}'")
    if n < 1:
        raise ReaderError("limit must be at least 1")
    return min(n, MAX_LIMIT)


def load(path_arg, limit, sheet=None):
    path, size = check_path(path_arg)
    fmt = detect_format(path, size)
    if size == 0:
        raise ReaderError(f"{os.path.basename(path)} is empty", code="no_data")
    if sheet not in (None, "") and fmt not in ("xlsx", "xls"):
        raise ReaderError("sheet only applies to Excel workbooks")
    if fmt == "csv":
        res = read_csv(path, size, limit)
    elif fmt == "ndjson":
        res = read_ndjson(path, size, limit)
    elif fmt == "json":
        res = read_json(path, size, limit)
    elif fmt == "xlsx":
        res = read_xlsx(path, size, limit, sheet)
    elif fmt == "xls":
        res = read_xls(path, size, limit, sheet)
    elif fmt == "parquet":
        res = read_parquet(path, size, limit)
    else:
        res = read_arrow(path, size, limit)
    return path, size, res


def cmd_read(path_arg, limit_arg=None, sheet=None):
    limit = _limit(limit_arg)
    path, size, (names, rows, total, truncated, info) = load(path_arg, limit, sheet)
    if not rows:
        raise ReaderError(f"{os.path.basename(path)} has no data rows", code="no_data")
    out = {"path": path, "file_size": size}
    out.update(info)
    out.update({"columns": names, "data": rows, "count": len(rows), "total_rows": total,
                "truncated": bool(truncated)})
    return out


def _kind(values):
    kinds = set()
    for v in values:
        if v is None:
            continue
        if isinstance(v, bool):
            kinds.add("bool")
        elif isinstance(v, int):
            kinds.add("integer")
        elif isinstance(v, float):
            kinds.add("number")
        elif isinstance(v, str):
            kinds.add("date" if _ISO_DATE.match(v) else "datetime" if _ISO_DT.match(v) else "string")
        else:
            kinds.add("object")
    if not kinds:
        return "string"
    if len(kinds) == 1:
        return kinds.pop()
    if kinds == {"integer", "number"}:
        return "number"
    if kinds == {"date", "datetime"}:
        return "datetime"
    return "object" if "object" in kinds else "string"


def cmd_schema(path_arg, sheet=None):
    path, size, (names, rows, total, truncated, info) = load(path_arg, DEFAULT_LIMIT, sheet)
    arrow_types = info.pop("arrow_types", {})
    cols = []
    for n in names:
        vals = [r.get(n) for r in rows]
        example = next((v for v in vals if v is not None), None)
        col = {"column": n, "type": _kind(vals), "nulls": sum(1 for v in vals if v is None),
               "example": example if not isinstance(example, (dict, list)) else json.dumps(example)[:200]}
        if n in arrow_types:
            col["source_type"] = arrow_types[n]
        cols.append(col)
    if not cols:
        raise ReaderError(f"{os.path.basename(path)} has no columns", code="no_data")
    out = {"path": path, "file_size": size}
    out.update(info)
    out.update({"data": cols, "rows_sampled": len(rows), "total_rows": total})
    return out


def cmd_sheets(path_arg):
    path, size = check_path(path_arg)
    fmt = detect_format(path, size)
    if size == 0:
        raise ReaderError(f"{os.path.basename(path)} is empty", code="no_data")
    if fmt == "xlsx":
        sheets = read_xlsx(path, size, 1, list_only=True)
    elif fmt == "xls":
        sheets = read_xls(path, size, 1, list_only=True)
    else:
        sheets = [{"sheet": os.path.splitext(os.path.basename(path))[0], "index": 1, "rows": None,
                   "columns": None, "visible": True}]
    return {"path": path, "format": fmt, "data": sheets}


COMMANDS = {
    "read": "<path> [limit=1000] [sheet]",
    "schema": "<path> [sheet]",
    "sheets": "<path>",
}


def main(argv=None):
    args = sys.argv[1:] if argv is None else argv
    if not args:
        return {"error": "Usage: python local_file_data.py <read|schema|sheets> <path> [...]", "code": "bad_args",
                "commands": COMMANDS}
    command = args[0]
    try:
        if command == "read":
            if len(args) < 2:
                raise ReaderError("Usage: read <path> [limit=1000] [sheet]")
            path = args[1]
            limit = args[2] if len(args) > 2 else "1000"
            sheet = args[3] if len(args) > 3 else ""
            return cmd_read(path, limit, sheet)
        if command == "schema":
            if len(args) < 2:
                raise ReaderError("Usage: schema <path> [sheet]")
            path = args[1]
            sheet = args[2] if len(args) > 2 else ""
            return cmd_schema(path, sheet)
        if command == "sheets":
            if len(args) < 2:
                raise ReaderError("Usage: sheets <path>")
            path = args[1]
            return cmd_sheets(path)
        return {"error": f"Unknown command: {command}", "code": "bad_args", "commands": COMMANDS}
    except ReaderError as exc:
        out = {"error": str(exc), "code": exc.code}
        out.update(exc.extra)
        return out
    except MemoryError:
        return {"error": "The file is too large to read with this limit - lower the limit", "code": "bad_args"}
    except Exception as exc:  # a malformed file must still answer in one JSON object
        return {"error": f"Could not read the file: {type(exc).__name__}: {exc}", "code": "unknown"}


def _emit(result):
    try:
        import fincept_json
        text = fincept_json.dumps(result, ensure_ascii=False)
    except Exception:
        text = json.dumps(result, ensure_ascii=False, default=str)
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    sys.stdout.write(text + "\n")
    sys.stdout.flush()


if __name__ == "__main__":
    _emit(main())
