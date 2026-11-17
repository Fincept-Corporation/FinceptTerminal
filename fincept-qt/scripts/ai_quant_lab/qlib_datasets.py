"""
AI Quant Lab - local qlib market datasets.

Registry
    Every qlib dataset on this machine: the public qlib datasets that ``qlib_service.py
    download_data`` puts in <FINCEPT_DATA_DIR>/qlib_data/<region>_data (a frozen snapshot -- the
    US one ends on 2020-11-10; one that only exists in qlib's own ~/.qlib/qlib_data/<region>_data,
    where earlier releases downloaded it, is read there in place) and the datasets built here
    from Yahoo Finance daily bars under <FINCEPT_DATA_DIR>/qlib_data/<dataset id>/. ``dataset_info`` / ``list_datasets`` report
    each one's calendar range, instrument count and last update, and pick the default: the
    one whose calendar ends latest.

Builder
    ``build_dataset`` / ``update_dataset`` write qlib's on-disk layout from Yahoo bars:
        calendars/day.txt                 one trading day per line (the benchmark's days)
        instruments/all.txt               SYMBOL<TAB>first day<TAB>last day
        instruments/<universe>.txt        the universe members (no benchmarks)
        features/<symbol>/<field>.day.bin little-endian float32: [calendar index of the
                                          first value, value, value, ...]
    Prices are adjusted, with qlib's ``factor`` field = adjusted / raw price, so
    ``$close / $factor`` is the price that actually traded that day, and volume is
    raw volume / factor (qlib's convention). Yahoo's daily Open/High/Low/Close/Volume are
    split-adjusted, not raw: the raw bar is rebuilt from the split events in the same
    download (every split after a bar's date up to the fetch is in the response).

    Raw downloads are cached per ticker under <FINCEPT_DATA_DIR>/qlib_data/_yahoo_cache/
    (<TICKER>.csv + <TICKER>.json), shared by every dataset. A build only requests what the
    cache lacks, so an interrupted build resumes where it stopped, and an update only asks
    for the bars after the last cached day. When a dividend or split has happened since,
    Yahoo's adjusted history moves by one constant ratio; the cached adjusted closes are
    rescaled by the ratio measured on the overlapping days (or the ticker is re-downloaded
    when the overlap does not agree).

Nothing heavy is imported at module level: dataset_info runs every time a panel opens.
"""

from __future__ import annotations

import bisect
import json
import os
import re
import shutil
import sys
import threading
import time
from collections import deque
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

HERE = Path(__file__).resolve().parent
UNIVERSE_FILE = HERE / "qlib_universes.json"

# The public datasets qlib_service.py download_data installs: (id, region, name, default
# market). Each one is read from public_dataset_dir(region).
_PUBLIC = (
    ("qlib_us", "us", "Public qlib US (Yahoo snapshot)", "sp500"),
    ("qlib_cn", "cn", "Public qlib CN", "csi300"),
)
FIELDS = ("open", "high", "low", "close", "volume", "factor", "change")
COMMANDS = ("list_datasets", "dataset_info", "build_dataset", "update_dataset")
SAMPLE_INSTRUMENTS = 5   # dataset_info's sample_instruments: what the panels prefill

# Yahoo politeness: few parallel requests with a pause after each, exponential back-off
# on HTTP 429, and a time budget well inside the terminal's 60-minute script watchdog.
_WORKERS = 3
_PAUSE_S = 0.35
_RATE_LIMIT_DELAYS = (15, 30, 60, 120, 240)
_TIME_BUDGET_S = 40 * 60
_MISSING_RECHECK_S = 24 * 3600
# Fewer bars than this in the dataset window is a Yahoo stub (a delisted symbol answering with
# its last days), not a listing: such a ticker is skipped and reported.
_MIN_BARS = 20
_ID_RE = re.compile(r"^[a-z0-9][a-z0-9_]{0,47}$")
_TICKER_RE = re.compile(r"^\^?[A-Z0-9][A-Z0-9.\-=]{0,14}$")
_WIN_RESERVED = {"CON", "PRN", "AUX", "NUL", *(f"COM{i}" for i in range(10)), *(f"LPT{i}" for i in range(10))}


def log(msg: str) -> None:
    """Progress / diagnostics: stderr only (stdout carries exactly one JSON payload)."""
    print(f"[qlib_datasets] {msg}", file=sys.stderr, flush=True)


# ── Paths and small readers ──────────────────────────────────────────────────

def _fincept_paths():
    """scripts/fincept_paths.py (the scripts root is on sys.path whenever the terminal starts a
    script; added here for a module imported some other way)."""
    root = str(HERE.parent)
    if root not in sys.path:
        sys.path.append(root)
    import fincept_paths
    return fincept_paths


def data_root() -> Path:
    """<FINCEPT_DATA_DIR>/qlib_data (run by hand: the edition's default data root)."""
    return _fincept_paths().data_root() / "qlib_data"


def legacy_qlib_dir() -> Path:
    """qlib's own default data folder, ~/.qlib/qlib_data: where download_data put the public
    datasets in earlier releases (and where qlib's CLI puts them). Read in place only: never
    written, and never copied (multi-GB downloads)."""
    return Path.home() / ".qlib" / "qlib_data"


def _existing_or_new(name: str, present) -> Path:
    """<data root>/qlib_data/<name> when `present`; else ~/.qlib/qlib_data/<name> when that is
    `present`; else the former (where a new download goes)."""
    new = data_root() / name
    if present(new):
        return new
    legacy = legacy_qlib_dir() / name
    return legacy if present(legacy) else new


def public_download_dir(region: str) -> Path:
    """Where download_data puts a new public qlib dataset: <data root>/qlib_data/<region>_data."""
    return data_root() / f"{region}_data"


def public_dataset_dir(region: str) -> Path:
    """Where the public qlib dataset of `region` is read from: public_download_dir(region) when
    it holds one, else ~/.qlib/qlib_data/<region>_data when that does (read in place)."""
    return _existing_or_new(f"{region}_data", lambda p: (p / "calendars" / "day.txt").is_file())


def qlib_data_file(name: str) -> Path:
    """A file kept next to the public datasets (qlib's HIST stock2concept.npy / stock_index.npy):
    <data root>/qlib_data/<name>, else ~/.qlib/qlib_data/<name> when only that one exists."""
    return _existing_or_new(name, lambda p: p.is_file())


def _cache_dir() -> Path:
    return data_root() / "_yahoo_cache"


def _read_lines(path: Path) -> List[str]:
    try:
        with path.open("r", encoding="utf-8") as fh:
            return [ln.strip() for ln in fh if ln.strip()]
    except OSError:
        return []


def _calendar(path: Path) -> List[str]:
    return [ln[:10] for ln in _read_lines(path / "calendars" / "day.txt")]


def _markets(path: Path) -> List[str]:
    try:
        return sorted(p.stem for p in (path / "instruments").glob("*.txt"))
    except OSError:
        return []


def instrument_names(path: Path, market: str = "all") -> List[str]:
    """Lower-case instrument codes listed in instruments/<market>.txt."""
    return [ln.split("\t")[0].strip().lower() for ln in _read_lines(path / "instruments" / f"{market.lower()}.txt")]


def _load_json(path: Path) -> Dict[str, Any]:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def _write_json_atomic(path: Path, obj: Dict[str, Any]) -> None:
    tmp = path.with_name(path.name + f".tmp{os.getpid()}")
    tmp.write_text(json.dumps(obj, indent=2, default=str), encoding="utf-8")
    os.replace(tmp, path)


def load_universes() -> Dict[str, Any]:
    return _load_json(UNIVERSE_FILE)


def _ny_now() -> datetime:
    try:
        from zoneinfo import ZoneInfo
        return datetime.now(ZoneInfo("America/New_York")).replace(tzinfo=None)
    except Exception:
        # No tz database (a Windows Python without tzdata): US Eastern standard time is
        # close enough to decide whether today's session has closed.
        return datetime.utcnow() - timedelta(hours=5)


def last_complete_session() -> date:
    """The last weekday whose US session has closed (17:00 New York). A bar for today taken
    while the market is open is a partial bar and is never stored."""
    now = _ny_now()
    d = now.date() if (now.weekday() < 5 and now.hour >= 17) else now.date() - timedelta(days=1)
    while d.weekday() >= 5:
        d -= timedelta(days=1)
    return d


def _parse_date(value: Any, what: str) -> Optional[date]:
    if value in (None, ""):
        return None
    try:
        return date.fromisoformat(str(value)[:10])
    except ValueError:
        raise ValueError(f"{what} must be YYYY-MM-DD (got '{value}')")


def _dir_size(path: Path) -> int:
    total = 0
    for root, _dirs, files in os.walk(path):
        for f in files:
            try:
                total += os.path.getsize(os.path.join(root, f))
            except OSError:
                pass
    return total


# ── Registry ─────────────────────────────────────────────────────────────────

def suggested_dates(cal: List[str]) -> Dict[str, List[str]]:
    """Default windows inside a calendar: test = the last year, valid = the year before,
    train = up to five years before that (after a 60-day warm-up for the 60-day factor
    windows); recent = the last ~3 months. A short calendar is split 60/20/20."""
    n = len(cal)
    if n == 0:
        return {}

    def shift(day: str, days: int) -> str:
        return (date.fromisoformat(day) - timedelta(days=days)).isoformat()

    warm = min(60, max(0, n // 10))
    out: Dict[str, List[str]] = {"recent": [cal[max(0, n - 63)], cal[-1]], "full": [cal[0], cal[-1]]}
    ts = bisect.bisect_right(cal, shift(cal[-1], 365))
    vs = bisect.bisect_right(cal, shift(cal[ts], 365)) if ts < n else n
    tr = max(warm, bisect.bisect_left(cal, shift(cal[vs], 5 * 365))) if vs < n else warm
    if n >= 3 * 252 and vs - tr >= 252 and ts - vs >= 120:
        out.update(train=[cal[tr], cal[vs - 1]], valid=[cal[vs], cal[ts - 1]], test=[cal[ts], cal[-1]])
    else:
        span = n - warm
        a = warm + int(span * 0.6)
        b = warm + int(span * 0.8)
        a, b = max(a, warm + 1), max(b, warm + 2)
        if b >= n:
            return out
        out.update(train=[cal[warm], cal[a - 1]], valid=[cal[a], cal[b - 1]], test=[cal[b], cal[-1]])
    out["last_year"] = list(out["test"])
    return out


def _info(ds_id: str, path: Path, source: str, region: str, name: str, default_market: str,
          meta: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    cal = _calendar(path)
    if not cal or not (path / "features").is_dir():
        return None
    markets = _markets(path)
    if default_market not in markets:
        default_market = "all"
    members = instrument_names(path, default_market)
    if source == "yahoo":
        updated = meta.get("updated_at")
    else:
        try:
            updated = datetime.fromtimestamp((path / "calendars" / "day.txt").stat().st_mtime).isoformat(timespec="seconds")
        except OSError:
            updated = None
    end = date.fromisoformat(cal[-1])
    latest = last_complete_session()
    info = {
        "id": ds_id,
        "name": name,
        "source": source,
        "region": region,
        "path": str(path),
        "provider_uri": path.as_posix(),
        "calendar_start": cal[0],
        "calendar_end": cal[-1],
        "trading_days": len(cal),
        "instruments": len(_read_lines(path / "instruments" / "all.txt")),
        "markets": markets,
        "default_market": default_market,
        "universe_size": len(members),
        # Five: the cross-sectional views (Live Signals > Factor Analysis) need at least five
        # names, and the panels prefill them from this sample.
        "sample_instruments": members[:SAMPLE_INSTRUMENTS],
        "benchmark": meta.get("benchmark"),
        "updated_at": updated,
        "age_days": (date.today() - end).days,
        "can_update": source == "yahoo",
        "up_to_date": source == "yahoo" and end >= latest,
        "latest_session": latest.isoformat(),
        "defaults": suggested_dates(cal),
    }
    if source == "yahoo":
        info.update({
            "universe": meta.get("universe"),
            "universe_label": meta.get("universe_label"),
            "universe_as_of": meta.get("universe_as_of"),
            "start_date": meta.get("start_date"),
            "skipped": meta.get("skipped", []),
            "late_start": meta.get("late_start", []),
            "size_bytes": meta.get("size_bytes"),
        })
    return info


def list_datasets() -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    root = data_root()
    if root.is_dir():
        for d in sorted(root.iterdir()):
            if not d.is_dir() or d.name.startswith("_") or "." in d.name:
                continue
            meta = _load_json(d / "dataset.json")
            if meta.get("source") != "yahoo":
                continue
            info = _info(d.name, d, "yahoo", meta.get("region", "us"), meta.get("name") or d.name,
                         meta.get("universe") or "all", meta)
            if info:
                out.append(info)
    for ds_id, region, name, market in _PUBLIC:
        info = _info(ds_id, public_dataset_dir(region), "qlib_public", region, name, market, {})
        if info:
            out.append(info)
    return out


def _freshest(infos: List[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
    if not infos:
        return None
    return max(infos, key=lambda i: (i["calendar_end"], i["source"] == "yahoo", i["instruments"]))


def resolve(dataset_id: Optional[str]) -> Tuple[Optional[Dict[str, Any]], Optional[str]]:
    """(info, error). No id: the freshest dataset; (None, None) when there is none at all."""
    infos = list_datasets()
    if dataset_id:
        for i in infos:
            if i["id"] == dataset_id:
                return i, None
        known = ", ".join(i["id"] for i in infos) or "none"
        return None, f"Dataset '{dataset_id}' was not found (available: {known})."
    return _freshest(infos), None


def no_dataset_message() -> str:
    return ("No qlib market dataset is available yet. Build one from Yahoo Finance with BUILD FROM YAHOO in the "
            "dataset bar at the top of the panel (US large caps from 2016 take a few minutes), or download the "
            "public qlib US dataset (1999-2020) with DOWNLOAD QLIB DATASET (Model Library > Browse).")


def window_message(info: Dict[str, Any], start: Optional[str], end: Optional[str]) -> str:
    hint = ""
    if info.get("source") == "qlib_public":
        hint = (" The public qlib dataset is a frozen snapshot; for recent dates build a Yahoo Finance dataset "
                "(BUILD FROM YAHOO in the dataset bar) and select it.")
    elif info.get("can_update") and not info.get("up_to_date"):
        hint = " Use UPDATE TO TODAY in the dataset bar to add the latest bars."
    return (f"The {info['name']} dataset ({info['id']}) covers {info['calendar_start']} to {info['calendar_end']}; "
            f"the requested window {start or '...'} -> {end or '...'} lies outside it. Pick dates inside that range."
            + hint)


def check_window(info: Dict[str, Any], start: Optional[str], end: Optional[str]):
    """Clamp [start, end] to the dataset calendar. Returns (start, end, warning, error)."""
    cs, ce = info["calendar_start"], info["calendar_end"]
    s, e = (start or cs)[:10], (end or ce)[:10]
    if s > ce or e < cs or s > e:
        return s, e, None, window_message(info, start, end)
    notes = []
    if s < cs:
        notes.append(f"start {s} moved to the first day of the {info['id']} dataset, {cs}")
        s = cs
    if e > ce:
        notes.append(f"end {e} moved to the last day of the {info['id']} dataset, {ce}")
        e = ce
    return s, e, ("; ".join(notes) or None), None


def brief(info: Dict[str, Any]) -> Dict[str, Any]:
    """The dataset block attached to qlib_service results."""
    return {k: info.get(k) for k in ("id", "name", "source", "calendar_start", "calendar_end", "instruments")}


def dataset_info(params: Dict[str, Any]) -> Dict[str, Any]:
    infos = list_datasets()
    want = params.get("dataset") or None
    chosen = None
    if want:
        chosen = next((i for i in infos if i["id"] == want), None)
    default = _freshest(infos)
    chosen = chosen or default
    uni = load_universes()
    universes = [{"id": k, "label": v.get("label", k), "description": v.get("description", ""),
                  "count": len(v.get("tickers", []))} for k, v in uni.get("universes", {}).items()]
    universes.append({"id": "custom", "label": "Custom tickers",
                      "description": "Your own comma-separated Yahoo symbols", "count": 0})
    out = {
        "success": True,
        "available": chosen is not None,
        "default": default["id"] if default else None,
        "selected": chosen["id"] if chosen else None,
        "requested": want,
        "requested_found": (not want) or any(i["id"] == want for i in infos),
        "dataset": chosen,
        # Full entries: the panel switches datasets (and their default dates) without asking again.
        "datasets": infos,
        "universes": universes,
        "universe_as_of": uni.get("as_of"),
        "default_build": {"universe": uni.get("default_universe", "us_large"),
                          "start_date": uni.get("default_start_date", "2016-01-01"), "region": "us"},
        "latest_session": last_complete_session().isoformat(),
        "data_root": str(data_root()),
    }
    if chosen is None:
        out["message"] = no_dataset_message()
    return out


# ── Yahoo cache ──────────────────────────────────────────────────────────────

def _sym_file(sym: str) -> str:
    base = sym.upper()
    return ("_" + base) if base in _WIN_RESERVED else base


def _feature_dir_name(sym: str) -> str:
    """qlib's code_to_fname, lower-cased like its storage layer."""
    base = sym.upper()
    return (("_qlib_" + base) if base in _WIN_RESERVED else base).lower()


class _Cache:
    COLS = ["open", "high", "low", "close", "adj_close", "volume", "dividends", "splits"]

    def __init__(self) -> None:
        self.dir = _cache_dir()
        self.dir.mkdir(parents=True, exist_ok=True)

    def meta(self, sym: str) -> Dict[str, Any]:
        return _load_json(self.dir / f"{_sym_file(sym)}.json")

    def load(self, sym: str):
        import pandas as pd
        p = self.dir / f"{_sym_file(sym)}.csv"
        if not p.exists():
            return None
        df = pd.read_csv(p, index_col=0, parse_dates=[0])
        df.index.name = "date"
        return df

    def save(self, sym: str, df, meta: Dict[str, Any]) -> None:
        p = self.dir / f"{_sym_file(sym)}.csv"
        if df is not None:
            tmp = p.with_name(p.name + f".tmp{os.getpid()}")
            df[self.COLS].to_csv(tmp, index_label="date", float_format="%.6f")
            os.replace(tmp, p)
        _write_json_atomic(self.dir / f"{_sym_file(sym)}.json", meta)


def _raw_frame(df, cutoff: date):
    """Yahoo history (auto_adjust=False, actions=True) -> raw daily bars + Yahoo's adjusted
    close. Splits are applied over the WHOLE response before cutting it at `cutoff`, so a
    split that took effect after the cutoff still un-adjusts the bars before it."""
    import numpy as np
    import pandas as pd
    if df is None or len(df) == 0:
        return None
    idx = pd.DatetimeIndex(df.index)
    if idx.tz is not None:
        idx = idx.tz_localize(None)
    idx = idx.normalize()
    splits = pd.Series(df["Stock Splits"].values if "Stock Splits" in df else 0.0, index=idx, dtype=float).fillna(0.0)
    ratio = splits.where(splits > 0, 1.0)
    after = ratio[::-1].cumprod()[::-1].shift(-1, fill_value=1.0)   # product of splits strictly after each bar
    out = pd.DataFrame(index=idx)
    for col, src in (("open", "Open"), ("high", "High"), ("low", "Low"), ("close", "Close")):
        out[col] = pd.to_numeric(pd.Series(df[src].values, index=idx), errors="coerce") * after
    out["adj_close"] = pd.to_numeric(pd.Series(df["Adj Close"].values, index=idx), errors="coerce")
    out["volume"] = pd.to_numeric(pd.Series(df["Volume"].values, index=idx), errors="coerce") / after
    out["dividends"] = pd.Series(df["Dividends"].values if "Dividends" in df else 0.0, index=idx, dtype=float).fillna(0.0)
    out["splits"] = splits
    out = out[~out.index.duplicated(keep="last")].sort_index()
    out = out[out.index <= pd.Timestamp(cutoff)]
    out = out[np.isfinite(out["close"]) & (out["close"] > 0)]
    out.index.name = "date"
    return out


def _merge_tail(old, new):
    """Append freshly downloaded bars to the cached ones. Returns (merged, ratio) or
    (None, reason) when the overlap disagrees (the caller re-downloads the ticker)."""
    import numpy as np
    import pandas as pd
    overlap = old.index.intersection(new.index)
    if len(overlap) == 0:
        return None, "no overlapping bars to align the adjustment"
    rc = (new.loc[overlap, "close"] / old.loc[overlap, "close"]).replace([np.inf, -np.inf], np.nan).dropna()
    if rc.empty or float((rc - 1).abs().max()) > 2e-3:
        return None, "raw prices changed on overlapping days"
    ra = (new.loc[overlap, "adj_close"] / old.loc[overlap, "adj_close"]).replace([np.inf, -np.inf], np.nan).dropna()
    if ra.empty:
        return None, "no adjusted closes on overlapping days"
    r = float(ra.median())
    if float((ra / r - 1).abs().max()) > 2e-3:
        return None, "the adjustment changed unevenly across overlapping days"
    keep = old[old.index < new.index.min()].copy()
    keep["adj_close"] = keep["adj_close"] * r
    return pd.concat([keep, new]).sort_index(), r


def _fincept_yahoo():
    """scripts/fincept_yahoo.py: the shared Yahoo layer (session, limiter / back-off / breaker,
    the bar cache every script reads)."""
    _fincept_paths()  # puts the scripts root on sys.path
    import fincept_yahoo
    return fincept_yahoo


class _Fetcher:
    """Thread pool over Ticker.history frames from fincept_yahoo's shared bar cache (one long
    download per symbol shared with every other script; a later call fetches only the missing
    tail), with pacing and HTTP 429 back-off. The CSV cache above keeps its own format."""

    def __init__(self, cutoff: date, deadline: float) -> None:
        self.fy = _fincept_yahoo()
        self.fy.yf()  # configured yfinance: shared session, tz cache, limiter
        self.cutoff = cutoff
        self.deadline = deadline
        self.lock = threading.Lock()
        self.backoff_until = 0.0
        self.requests = 0
        self.rate_limited = 0

    def _history(self, sym: str, start: date):
        # End beyond today so the frame carries every split up to now (see _raw_frame).
        end = date.today() + timedelta(days=2)
        try:
            df = self.fy.ticker_history(sym, start=start.isoformat(), end=end.isoformat(), interval="1d",
                                        auto_adjust=False, actions=True)
        except Exception as e:
            if getattr(e, "code", "") == "RATE_LIMITED":  # the bar cache's PriceDataError
                from yfinance.exceptions import YFRateLimitError
                raise YFRateLimitError() from e
            raise
        if df is None or len(df) == 0:
            # Yahoo has no symbol / no bars in the window (yfinance's raise_errors=True raised here).
            raise LookupError(f"{sym}: no data found on Yahoo Finance from {start.isoformat()}")
        return _raw_frame(df, self.cutoff)

    def run(self, jobs: List[Tuple[str, date]], on_done) -> None:
        from yfinance import exceptions as yfe
        missing_types = tuple(t for t in (getattr(yfe, "YFTickerMissingError", None),
                                          getattr(yfe, "YFTzMissingError", None),
                                          getattr(yfe, "YFPricesMissingError", None),
                                          getattr(yfe, "YFInvalidPeriodError", None)) if t)
        rate_type = getattr(yfe, "YFRateLimitError", None)
        queue = deque(jobs)
        attempts: Dict[str, int] = {}

        def worker():
            while True:
                with self.lock:
                    if not queue:
                        return
                    sym, start = queue.popleft()
                while True:
                    wait = self.backoff_until - time.time()
                    if wait <= 0:
                        break
                    time.sleep(min(wait, 1.0))
                if time.time() > self.deadline:
                    on_done(sym, None, "pending", "time budget reached; run the build again to resume")
                    continue
                try:
                    with self.lock:
                        self.requests += 1
                    df = self._history(sym, start)
                    on_done(sym, df, "ok", "")
                except Exception as e:  # noqa: BLE001 - classified below
                    msg = str(e).strip() or type(e).__name__
                    n = attempts[sym] = attempts.get(sym, 0) + 1
                    if rate_type is not None and isinstance(e, rate_type) or "Too Many Requests" in msg or " 429" in msg:
                        with self.lock:
                            self.rate_limited += 1
                            delay = _RATE_LIMIT_DELAYS[min(n, len(_RATE_LIMIT_DELAYS)) - 1]
                            self.backoff_until = max(self.backoff_until, time.time() + delay)
                        log(f"{sym}: Yahoo rate limit, backing off {delay}s (attempt {n})")
                        if n < len(_RATE_LIMIT_DELAYS):
                            with self.lock:
                                queue.append((sym, start))
                        else:
                            on_done(sym, None, "failed", "Yahoo kept rate-limiting; run the build again later")
                    elif isinstance(e, missing_types) or any(s in msg.lower() for s in
                                                             ("delisted", "no data found", "not found", "404")):
                        on_done(sym, None, "missing", msg[:200])
                    elif n < 3:
                        log(f"{sym}: {msg[:160]} -- retrying")
                        time.sleep(3 * n)
                        with self.lock:
                            queue.append((sym, start))
                    else:
                        on_done(sym, None, "failed", msg[:200])
                time.sleep(_PAUSE_S)

        threads = [threading.Thread(target=worker, daemon=True) for _ in range(min(_WORKERS, max(1, len(jobs))))]
        for t in threads:
            t.start()
        for t in threads:
            t.join()


def _refresh_cache(symbols: List[str], start: date, cutoff: date, progress_label: str) -> Dict[str, Any]:
    """Bring the cache of every symbol to [start, cutoff]. Returns per-symbol status and
    request statistics. Only what the cache lacks is downloaded."""
    cache = _Cache()
    status: Dict[str, Dict[str, Any]] = {}
    jobs: List[Tuple[str, date]] = []
    kinds: Dict[str, str] = {}
    full_start: Dict[str, date] = {}
    now = time.time()
    for sym in symbols:
        m = cache.meta(sym)
        if m.get("status") == "missing" and now - float(m.get("checked_ts", 0)) < _MISSING_RECHECK_S:
            status[sym] = {"status": "missing", "reason": m.get("reason", "no Yahoo data"), "cached": True}
            continue
        have_csv = (cache.dir / f"{_sym_file(sym)}.csv").exists()
        if m.get("status") == "ok" and have_csv and m.get("start_requested", "9999") <= start.isoformat():
            if m.get("fetched_through", "") >= cutoff.isoformat():
                status[sym] = {"status": "ok", "cached": True}
                continue
            last = date.fromisoformat(m.get("last") or m["fetched_through"])
            jobs.append((sym, last - timedelta(days=10)))
            kinds[sym] = "tail"
            full_start[sym] = date.fromisoformat(m["start_requested"])
        else:
            jobs.append((sym, start))
            kinds[sym] = "full"
            full_start[sym] = start

    stats = {"requests": 0, "rate_limited": 0, "rescaled": [], "refetched": [], "bars_added": 0}
    if not jobs:
        return {"status": status, **stats}

    log(f"{progress_label}: {len(jobs)} of {len(symbols)} ticker(s) to download "
        f"({sum(1 for k in kinds.values() if k == 'tail')} incremental)")
    fetcher = _Fetcher(cutoff, time.time() + _TIME_BUDGET_S)
    done_lock = threading.Lock()
    counter = {"n": 0}
    second_pass: List[Tuple[str, date]] = []

    def on_done(sym, df, st, reason):
        with done_lock:
            counter["n"] += 1
            if counter["n"] % 20 == 0 or counter["n"] == len(jobs):
                log(f"{progress_label}: {counter['n']}/{len(jobs)} downloaded")
        meta_now = {"checked_ts": time.time(), "checked_at": datetime.now().isoformat(timespec="seconds")}
        if st != "ok":
            if st == "missing" and kinds[sym] == "tail":
                # A ticker that answered before and now has nothing new: keep its history.
                cache.save(sym, None, {**cache.meta(sym), "fetched_through": cutoff.isoformat(), **meta_now})
                status[sym] = {"status": "ok", "cached": False, "note": "no new bars"}
                return
            if kinds[sym] == "tail":
                # The cached history is still good; the dataset keeps it, ending earlier.
                status[sym] = {"status": "ok", "stale": True, "reason": reason}
                return
            if st == "missing":
                cache.save(sym, None, {"status": "missing", "reason": reason, **meta_now})
            status[sym] = {"status": st, "reason": reason}
            return
        if kinds[sym] == "tail":
            old = cache.load(sym)
            if df is None or df.empty:
                cache.save(sym, None, {**cache.meta(sym), "fetched_through": cutoff.isoformat(), **meta_now})
                status[sym] = {"status": "ok", "cached": False, "note": "no new bars"}
                return
            merged, ratio = _merge_tail(old, df) if old is not None else (None, "cache file missing")
            if merged is None:
                log(f"{sym}: {ratio}; downloading its full history again")
                with done_lock:
                    second_pass.append((sym, full_start[sym]))
                    stats["refetched"].append(sym)
                return
            added = int((merged.index > old.index.max()).sum())
            with done_lock:
                stats["bars_added"] += added
                if abs(ratio - 1.0) > 1e-6:
                    stats["rescaled"].append({"ticker": sym, "ratio": round(ratio, 8)})
            df = merged
        else:
            if df is None or df.empty:
                cache.save(sym, None, {"status": "missing", "reason": "no bars in the requested window", **meta_now})
                status[sym] = {"status": "missing", "reason": "no bars in the requested window"}
                return
            with done_lock:
                stats["bars_added"] += len(df)
        start_req = full_start[sym].isoformat()
        cache.save(sym, df, {"status": "ok", "start_requested": start_req, "first": df.index.min().date().isoformat(),
                             "last": df.index.max().date().isoformat(), "fetched_through": cutoff.isoformat(),
                             "adj_basis": cutoff.isoformat(), "rows": int(len(df)), **meta_now})
        status[sym] = {"status": "ok", "cached": False}

    fetcher.run(jobs, on_done)
    if second_pass:
        kinds.update({s: "full" for s, _ in second_pass})
        fetcher.run(second_pass, on_done)
    stats["requests"] = fetcher.requests
    stats["rate_limited"] = fetcher.rate_limited
    return {"status": status, **stats}


# ── Writing the qlib layout ──────────────────────────────────────────────────

def _write_qlib(target: Path, symbols: List[str], members: List[str], universe_id: str,
                start: date, end: date, benchmark_order: List[str]) -> Dict[str, Any]:
    import numpy as np
    import pandas as pd
    cache = _Cache()
    s_ts, e_ts = pd.Timestamp(start), pd.Timestamp(end)
    frames = {}
    for sym in symbols:
        df = cache.load(sym)
        if df is not None:
            df = df[(df.index >= s_ts) & (df.index <= e_ts)]
            if not df.empty:
                frames[sym] = df
    bench = next((b for b in benchmark_order if b in frames), None)
    if bench is not None:
        cal_idx = frames[bench].index
    else:
        cal_idx = pd.DatetimeIndex(sorted(set().union(*[set(f.index) for f in frames.values()]))) if frames else []
    cal_idx = pd.DatetimeIndex(cal_idx).sort_values().unique()
    if len(cal_idx) == 0:
        raise RuntimeError("No bars at all in the requested window; nothing to write.")

    (target / "calendars").mkdir(parents=True, exist_ok=True)
    (target / "instruments").mkdir(parents=True, exist_ok=True)
    (target / "features").mkdir(parents=True, exist_ok=True)
    pos = {d: i for i, d in enumerate(cal_idx)}
    spans: Dict[str, Tuple[str, str]] = {}
    no_bars: List[Tuple[str, int]] = []
    for sym, df in frames.items():
        df = df[df.index.isin(cal_idx)]
        if len(df) < _MIN_BARS:
            no_bars.append((sym, len(df)))
            continue
        raw_close = df["close"]
        factor = (df["adj_close"] / raw_close).replace([np.inf, -np.inf], np.nan)
        factor = factor.where(factor > 0).ffill().bfill().fillna(1.0)
        cols = {
            "open": df["open"].where(df["open"] > 0) * factor,
            "high": df["high"].where(df["high"] > 0) * factor,
            "low": df["low"].where(df["low"] > 0) * factor,
            "close": raw_close * factor,
            "volume": df["volume"].where(df["volume"] >= 0) / factor,
            "factor": factor,
        }
        first, last = pos[df.index[0]], pos[df.index[-1]]
        span = cal_idx[first:last + 1]
        frame = pd.DataFrame(cols).reindex(span)
        frame["change"] = frame["close"].pct_change(fill_method=None)
        fdir = target / "features" / _feature_dir_name(sym)
        fdir.mkdir(parents=True, exist_ok=True)
        for field in FIELDS:
            np.hstack([[first], frame[field].to_numpy(dtype="float64")]).astype("<f4").tofile(
                str(fdir / f"{field}.day.bin"))
        spans[sym] = (span[0].date().isoformat(), span[-1].date().isoformat())

    with (target / "calendars" / "day.txt").open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(d.date().isoformat() + "\n" for d in cal_idx))
    with (target / "instruments" / "all.txt").open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(f"{s}\t{a}\t{b}\n" for s, (a, b) in sorted(spans.items())))
    with (target / "instruments" / f"{universe_id}.txt").open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(f"{s}\t{spans[s][0]}\t{spans[s][1]}\n" for s in sorted(members) if s in spans))
    cal_start = cal_idx[0].date().isoformat()
    late = sorted(({"ticker": s, "first_day": a} for s, (a, _b) in spans.items() if a > cal_start and s in members),
                  key=lambda r: r["first_day"])
    return {"calendar_start": cal_start, "calendar_end": cal_idx[-1].date().isoformat(),
            "trading_days": int(len(cal_idx)), "written": sorted(spans), "no_bars": no_bars, "late_start": late,
            "benchmark": bench}


class _Lock:
    """One build / update of a dataset at a time. An OS file lock, so a run that is killed
    (the terminal's watchdog, the app closing) releases it with its process: a retry is never
    blocked by a stale lock file. The (empty) lock file itself stays."""

    def __init__(self, path: Path) -> None:
        self.path = path
        self.fh = None

    def __enter__(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.fh = open(self.path, "a+b")
        try:
            if os.name == "nt":
                import msvcrt
                self.fh.seek(0)
                msvcrt.locking(self.fh.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.fh.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            self.fh.close()
            self.fh = None
            raise RuntimeError("Another build or update of this dataset is running. Wait for it to finish.")
        return self

    def __exit__(self, *exc):
        if self.fh is None:
            return
        try:
            if os.name == "nt":
                import msvcrt
                self.fh.seek(0)
                msvcrt.locking(self.fh.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.fh.fileno(), fcntl.LOCK_UN)
        except OSError:
            pass
        self.fh.close()
        self.fh = None


def _swap_in(staging: Path, target: Path) -> None:
    """Replace `target` with `staging` (two renames, then the old copy is removed)."""
    old = target.with_name(target.name + f".old{os.getpid()}")
    for attempt in range(5):
        try:
            if target.exists():
                os.replace(target, old)
            os.replace(staging, target)
            break
        except OSError as e:
            if attempt == 4:
                if old.exists() and not target.exists():
                    os.replace(old, target)
                raise RuntimeError(f"Could not replace {target} ({e}); is a qlib task reading it? Try again.")
            time.sleep(1.0)
    shutil.rmtree(old, ignore_errors=True)


def _normalize_tickers(raw: Any) -> List[str]:
    if isinstance(raw, str):
        raw = re.split(r"[,\s;]+", raw)
    out: List[str] = []
    for t in raw or []:
        s = str(t).strip().upper()
        if s and s not in out:
            out.append(s)
    return out


def _materialize(ds_id: str, meta: Dict[str, Any], start: date, end: date, cutoff: date,
                 label: str, t0: float, stats: Dict[str, Any]) -> Dict[str, Any]:
    """Write the dataset from the cache into a staging dir and swap it in."""
    root = data_root()
    target = root / ds_id
    # Leftovers of a run that was killed mid-write (the caller holds the dataset lock, so no
    # other run is using them).
    for leftover in list(root.glob(f"{ds_id}.staging*")) + list(root.glob(f"{ds_id}.old*")):
        shutil.rmtree(leftover, ignore_errors=True)
    staging = root / f"{ds_id}.staging{os.getpid()}"
    symbols = meta["tickers"]
    ok = [s for s in symbols if stats["status"].get(s, {}).get("status") == "ok"]
    members = [s for s in meta["members"] if s in ok]
    uni = load_universes()
    written = _write_qlib(staging, ok, members, meta["universe"], start, min(end, cutoff),
                          [meta.get("benchmark") or "SPY"] + list(uni.get("benchmarks", [])))
    skipped = [{"ticker": s, "status": v.get("status"), "reason": v.get("reason", "")}
               for s, v in stats["status"].items() if v.get("status") != "ok"]
    skipped += [{"ticker": s, "status": "stale", "reason": "kept its cached bars; refresh failed: " + v.get("reason", "")}
                for s, v in stats["status"].items() if v.get("stale")]
    skipped += [{"ticker": s, "status": "no_bars", "reason": f"only {n} bar(s) from Yahoo inside the dataset window"}
                for s, n in written["no_bars"]]
    meta.update({
        "calendar_start": written["calendar_start"], "calendar_end": written["calendar_end"],
        "trading_days": written["trading_days"], "instrument_count": len(written["written"]),
        "member_count": sum(1 for m in meta["members"] if m in written["written"]),
        "benchmark": written["benchmark"], "skipped": skipped, "late_start": written["late_start"],
        "built_through": cutoff.isoformat(), "updated_at": datetime.now().isoformat(timespec="seconds"),
    })
    meta["name"] = f"Yahoo Finance - {meta.get('universe_label') or meta['universe']} ({meta['member_count']} names)"
    _write_json_atomic(staging / "dataset.json", meta)
    meta["size_bytes"] = _dir_size(staging)
    _write_json_atomic(staging / "dataset.json", meta)
    _swap_in(staging, target)
    log(f"{label}: wrote {len(written['written'])} instrument(s), {written['trading_days']} trading days "
        f"{written['calendar_start']} -> {written['calendar_end']} in {time.time() - t0:.1f}s")
    return meta


def build_dataset(params: Dict[str, Any]) -> Dict[str, Any]:
    t0 = time.time()
    region = str(params.get("region") or "us").lower()
    if region != "us":
        return {"success": False, "error": f"The Yahoo Finance builder covers US listings only (region 'us'); got '{region}'."}
    uni = load_universes()
    universes = uni.get("universes", {})
    universe = str(params.get("universe") or uni.get("default_universe", "us_large")).lower()
    if universe == "custom":
        tickers = _normalize_tickers(params.get("tickers"))
        bad = [t for t in tickers if not _TICKER_RE.match(t)]
        if bad:
            return {"success": False, "error": f"Not Yahoo symbols: {', '.join(bad[:10])}"}
        if not tickers:
            return {"success": False, "error": "A custom universe needs at least one ticker (comma-separated Yahoo symbols)."}
        if len(tickers) > 500:
            return {"success": False, "error": f"A custom universe is limited to 500 tickers (got {len(tickers)})."}
        label, as_of = "Custom", None
    elif universe in universes:
        tickers = [t.upper() for t in universes[universe]["tickers"]]
        label, as_of = universes[universe].get("label", universe), uni.get("as_of")
    else:
        known = ", ".join(list(universes) + ["custom"])
        return {"success": False, "error": f"Unknown universe '{universe}' (choose one of: {known})."}

    try:
        start = _parse_date(params.get("start_date") or uni.get("default_start_date", "2016-01-01"), "start_date")
        end = _parse_date(params.get("end_date"), "end_date")
    except ValueError as e:
        return {"success": False, "error": str(e)}
    cutoff = last_complete_session()
    end = min(end or cutoff, cutoff)
    if start < date(1990, 1, 1):
        return {"success": False, "error": "start_date must be 1990-01-01 or later."}
    if (end - start).days < 120:
        return {"success": False, "error": f"The window {start} -> {end} is too short; give the dataset at least "
                                           f"six months (a year or more for model training)."}

    ds_id = str(params.get("dataset") or f"yahoo_{universe}").lower()
    if not _ID_RE.match(ds_id) or ds_id.startswith("qlib_"):
        return {"success": False, "error": f"Invalid dataset id '{ds_id}' (lower-case letters, digits, underscores)."}
    if ds_id in {f"{p[1]}_data" for p in _PUBLIC}:
        # <data root>/qlib_data/<region>_data is where download_data puts the public dataset.
        return {"success": False, "error": f"Dataset id '{ds_id}' is reserved for the public qlib dataset; choose another id."}
    benchmarks = [b.upper() for b in uni.get("benchmarks", ["SPY"])]
    symbols = tickers + [b for b in benchmarks if b not in tickers]
    members = [t for t in tickers if t not in benchmarks] or list(tickers)
    root = data_root()
    root.mkdir(parents=True, exist_ok=True)
    previous = _load_json(root / ds_id / "dataset.json")

    with _Lock(root / f"_{ds_id}.lock"):
        stats = _refresh_cache(symbols, start, cutoff, f"build {ds_id}")
        if not any(v.get("status") == "ok" for s, v in stats["status"].items() if s in members):
            reasons = sorted({v.get("reason", "") for v in stats["status"].values()})[:3]
            return {"success": False,
                    "error": "Yahoo Finance returned no data for any ticker of the universe"
                             + (f" ({'; '.join(r for r in reasons if r)})" if any(reasons) else "") + "."}
        meta = {
            "id": ds_id, "source": "yahoo", "region": "us", "universe": universe, "universe_label": label,
            "universe_as_of": as_of, "name": f"Yahoo Finance - {label} ({len(members)} names)",
            "tickers": symbols, "members": members, "benchmark": uni.get("default_benchmark", "SPY"),
            "start_date": start.isoformat(), "end_date": params.get("end_date") or None,
            "created_at": previous.get("created_at") or datetime.now().isoformat(timespec="seconds"),
        }
        meta = _materialize(ds_id, meta, start, end, cutoff, f"build {ds_id}", t0, stats)

    pending = [s["ticker"] for s in meta["skipped"] if s["status"] in ("failed", "pending", "stale")]
    result = {
        "success": True,
        "command": "build_dataset",
        "dataset": brief({**meta, "instruments": meta["instrument_count"]}),
        "dataset_id": ds_id,
        "universe": universe,
        "universe_label": label,
        "universe_as_of": as_of,
        "calendar_start": meta["calendar_start"],
        "calendar_end": meta["calendar_end"],
        "trading_days": meta["trading_days"],
        "instruments": meta["instrument_count"],
        "members": meta["member_count"],
        "requested": len(symbols),
        "benchmark": meta["benchmark"],
        "skipped": meta["skipped"],
        "late_start": meta["late_start"],
        "requests": stats["requests"],
        "from_cache": sum(1 for v in stats["status"].values() if v.get("cached")),
        "rate_limited": stats["rate_limited"],
        "seconds": round(time.time() - t0, 1),
        "size_bytes": meta["size_bytes"],
        "path": str(root / ds_id),
        "message": (f"Built {ds_id}: {meta['member_count']} of {len(members)} {label} names + benchmark, "
                    f"{meta['calendar_start']} to {meta['calendar_end']} ({meta['trading_days']} trading days)"),
    }
    if pending:
        result["warning"] = (f"{len(pending)} ticker(s) could not be downloaded this time ({', '.join(pending[:8])}"
                             f"{'...' if len(pending) > 8 else ''}); run the build again to fetch them.")
    return result


def update_dataset(params: Dict[str, Any]) -> Dict[str, Any]:
    t0 = time.time()
    root = data_root()
    want = params.get("dataset") or None
    infos = list_datasets()
    if want:
        target = next((i for i in infos if i["id"] == want), None)
        if target is None:
            return {"success": False, "error": f"Dataset '{want}' was not found."}
        if target["source"] != "yahoo":
            return {"success": False,
                    "error": f"{target['name']} is a frozen public snapshot ({target['calendar_start']} to "
                             f"{target['calendar_end']}) and cannot be updated. Build a Yahoo Finance dataset "
                             f"(BUILD FROM YAHOO) for current data."}
        ids = [want]
    else:
        ids = [i["id"] for i in infos if i["source"] == "yahoo"]
        if not ids:
            return {"success": False, "error": "There is no Yahoo-built dataset to update. " + no_dataset_message()}
    cutoff = last_complete_session()
    reports = []
    for ds_id in ids:
        meta = _load_json(root / ds_id / "dataset.json")
        prev_end = meta.get("calendar_end")
        prev_days = int(meta.get("trading_days") or 0)
        if meta.get("built_through", "") >= cutoff.isoformat() and not params.get("force"):
            reports.append({"dataset_id": ds_id, "already_up_to_date": True, "previous_end": prev_end,
                            "calendar_end": prev_end, "new_trading_days": 0, "requests": 0,
                            "latest_session": cutoff.isoformat()})
            continue
        with _Lock(root / f"_{ds_id}.lock"):
            start = date.fromisoformat(meta["start_date"])
            stats = _refresh_cache(meta["tickers"], start, cutoff, f"update {ds_id}")
            meta["end_date"] = None
            meta = _materialize(ds_id, meta, start, cutoff, cutoff, f"update {ds_id}", t0, stats)
        reports.append({
            "dataset_id": ds_id, "already_up_to_date": False, "previous_end": prev_end,
            "calendar_end": meta["calendar_end"], "trading_days": meta["trading_days"],
            "new_trading_days": meta["trading_days"] - prev_days, "requests": stats["requests"],
            "bars_added": stats["bars_added"], "rescaled": stats["rescaled"], "refetched": stats["refetched"],
            "rate_limited": stats["rate_limited"], "instruments": meta["instrument_count"],
            "skipped": [s for s in meta["skipped"] if s["status"] in ("failed", "pending", "stale")],
            "latest_session": cutoff.isoformat(),
        })
    first = reports[0]
    all_current = all(r["already_up_to_date"] for r in reports)
    msg = ("Already up to date through " + cutoff.isoformat()) if all_current else "; ".join(
        f"{r['dataset_id']}: {r['previous_end']} -> {r['calendar_end']} (+{r['new_trading_days']} day(s), "
        f"{r['requests']} request(s))" for r in reports if not r["already_up_to_date"])
    out = {
        "success": True,
        "command": "update_dataset",
        "dataset_id": first["dataset_id"],
        "updates": reports,
        "already_up_to_date": all_current,
        "calendar_end": first["calendar_end"],
        "requests": sum(r["requests"] for r in reports),
        "seconds": round(time.time() - t0, 1),
        "message": msg,
    }
    info = next((i for i in list_datasets() if i["id"] == first["dataset_id"]), None)
    if info:
        out["dataset"] = brief(info)
    pending = [s["ticker"] for r in reports for s in r.get("skipped", [])]
    if pending:
        out["warning"] = (f"{len(pending)} ticker(s) could not be refreshed ({', '.join(pending[:8])}); "
                          f"run the update again later.")
    return out


def run_command(command: str, params: Dict[str, Any]) -> Dict[str, Any]:
    try:
        if command in ("list_datasets", "dataset_info"):
            out = dataset_info(params)
            out["command"] = command
            return out
        if command == "build_dataset":
            return build_dataset(params)
        if command == "update_dataset":
            return update_dataset(params)
        return {"success": False, "error": f"Unknown dataset command: {command}"}
    except Exception as e:  # noqa: BLE001 - one JSON answer whatever happened
        import traceback
        traceback.print_exc(file=sys.stderr)
        return {"success": False, "error": f"{command} failed: {e}"}


if __name__ == "__main__":
    sys.path.insert(0, str(HERE.parent))
    import fincept_json
    _cmd = sys.argv[1] if len(sys.argv) > 1 else "dataset_info"
    _params = json.loads(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].strip() else {}
    _fd = os.dup(1)
    os.dup2(2, 1)               # yfinance / pandas chatter must not reach stdout
    _res = run_command(_cmd, _params)
    sys.stdout.flush()
    os.dup2(_fd, 1)
    fincept_json.emit(_res)
