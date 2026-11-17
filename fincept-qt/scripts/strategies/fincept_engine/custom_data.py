# ============================================================================
# Fincept Terminal - Strategy Engine: custom data (add_data with PythonData types)
#
# A custom type says where its data lives (get_source(config, date, is_live) ->
# SubscriptionDataSource) and how to read one line (reader(config, line, date, is_live)).
# The engine calls them like LEAN does:
#   * get_source is asked for every calendar day of the run (plus the history window);
#     each distinct source is fetched once: a remote file over HTTP (timeouts, cached on disk
#     under FINCEPT_DATA_DIR/strategy_engine/custom_data for 7 days), an object-store key the
#     strategy saved, or a local file path;
#   * every line goes through the strategy's reader; the points it returns are the data;
#   * a point reaches the strategy (slice[symbol], slice.get(Type), history, the security's
#     price) only once its end time has passed (no look-ahead).
# A source that is gone (HTTP error, an HTML page instead of data, an empty file) fails the run
# with a message naming the URL: no rows are ever invented.
# ============================================================================

import hashlib
import json
import os
import time as _time
from datetime import datetime, timedelta
from typing import Dict, List, Optional

from .errors import not_supported
from .session import SessionExtension
from .types import SymbolDict

_TTL_DAYS = 7
_MAX_SOURCES = 400
_TIMEOUT = 25


class SubscriptionTransportMedium:
    LOCAL_FILE = 0
    REMOTE_FILE = 1
    REST = 2
    STREAMING = 3
    OBJECT_STORE = 4
    LocalFile = 0
    RemoteFile = 1
    Rest = 2
    Streaming = 3
    ObjectStore = 4


class FileFormat:
    CSV = 0
    BINARY = 1
    ZIP_ENTRY_NAME = 2
    UNFOLDING_COLLECTION = 3
    INDEX = 4
    FOLDING_COLLECTION = 5
    Csv = 0
    Binary = 1
    ZipEntryName = 2
    UnfoldingCollection = 3
    Index = 4
    FoldingCollection = 5


class SubscriptionDataSource:
    def __init__(self, source='', transport_medium=SubscriptionTransportMedium.REMOTE_FILE,
                 format=FileFormat.CSV, headers=None, *args, **kwargs):
        # LEAN overloads: (source, transport) / (source, transport, format) / (source, format) ...
        self.source = str(source)
        self.transport_medium = transport_medium
        self.format = format
        self.headers = headers or kwargs.get('headers')
        for a in args:
            if isinstance(a, dict):
                self.headers = a

    def __repr__(self):
        return f"SubscriptionDataSource({self.source}, {self.transport_medium}, {self.format})"

    Source = property(lambda self: self.source)


def _cache_dir():
    from .yahoo_cache import cache_root
    d = cache_root() / 'custom_data'
    try:
        d.mkdir(parents=True, exist_ok=True)
    except OSError:
        return None
    return d


def fetch_url(url: str, headers=None) -> str:
    """Text of a remote file (cached on disk). Raises not_supported naming the URL when it is gone."""
    low = str(url or '').strip().lower()
    if not low.startswith(('http://', 'https://', 'ftp://')):
        raise not_supported(f"the local data file {url}",
                            "The strategy reads it from LEAN's data folder, which is not part of this engine.")
    d = _cache_dir()
    key = hashlib.sha1(url.encode('utf-8')).hexdigest()
    path = d / f"{key}.txt" if d is not None else None
    meta = d / f"{key}.json" if d is not None else None
    if path is not None and path.exists() and meta.exists():
        try:
            m = json.loads(meta.read_text(encoding='utf-8'))
            if _time.time() - float(m.get('fetched', 0)) < _TTL_DAYS * 86400:
                if m.get('error'):
                    raise not_supported(f"custom data from {url}", m['error'])
                return path.read_text(encoding='utf-8', errors='replace')
        except (OSError, ValueError):
            pass
    import urllib.request
    import urllib.error
    req = urllib.request.Request(url, headers={'User-Agent': 'FinceptTerminal/4 (support@fincept.in)',
                                               **(headers or {})})
    err = None
    text = None
    try:
        with urllib.request.urlopen(req, timeout=_TIMEOUT) as resp:
            raw = resp.read()
        text = raw.decode('utf-8', errors='replace')
        head = text.lstrip()[:200].lower()
        if not text.strip():
            err = "the source returned an empty file"
        elif head.startswith('<!doctype html') or head.startswith('<html'):
            err = "the source returned a web page instead of data (the file is gone or needs a login/API key)"
    except urllib.error.HTTPError as e:
        err = f"the source returned HTTP {e.code}"
    except Exception as e:  # DNS, timeout, TLS
        err = f"the source could not be reached ({type(e).__name__}: {e})"
    if meta is not None:
        try:
            if text is not None and err is None:
                path.write_text(text, encoding='utf-8')
            meta.write_text(json.dumps({'url': url, 'fetched': _time.time(), 'error': err}), encoding='utf-8')
        except OSError:
            pass
    if err:
        raise not_supported(f"custom data from {url}", err.capitalize() + '.')
    return text


class _Config:
    """What LEAN passes as `config` to get_source / reader."""
    def __init__(self, symbol, data_type, resolution):
        self.symbol = symbol
        self.type = data_type
        self.resolution = resolution
        self.data_time_zone = 'America/New_York'
        self.exchange_time_zone = 'America/New_York'
        self.is_custom_data = True
        self.increment = timedelta(days=1)
        self.mapped_symbol = str(symbol)
        self.tick_type = 'trade'

    Symbol = property(lambda self: self.symbol)
    Type = property(lambda self: self.type)
    Resolution = property(lambda self: self.resolution)
    Increment = property(lambda self: self.increment)
    DataTimeZone = property(lambda self: self.data_time_zone)
    ExchangeTimeZone = property(lambda self: self.exchange_time_zone)
    IsCustomData = property(lambda self: self.is_custom_data)
    MappedSymbol = property(lambda self: self.mapped_symbol)
    TickType = property(lambda self: self.tick_type)


class CustomFeed:
    """Points of one custom-data subscription, sorted by end time."""

    def __init__(self, symbol, points):
        self.symbol = symbol
        self.points = points            # [(end_time, point)]
        self._ends = [p[0] for p in points]

    def known_at(self, t):
        import bisect
        return bisect.bisect_right(self._ends, t)

    def new_between(self, t0, t1):
        import bisect
        lo = bisect.bisect_right(self._ends, t0)
        hi = bisect.bisect_right(self._ends, t1)
        return [p for _e, p in self.points[lo:hi]]


def _price_of(p):
    for name in ('close', 'Close', 'price', 'Price', 'value', 'Value'):
        try:
            v = getattr(p, name)
        except Exception:
            v = None
        if isinstance(v, (int, float)) and v == v and v != 0:
            return float(v)
    return 0.0


class CustomDataManager(SessionExtension):
    """Loads every add_data subscription before the run and delivers its points."""

    def __init__(self, object_store_reader=None):
        self.feeds: Dict[str, CustomFeed] = {}
        self.sources: Dict[str, List[str]] = {}
        self._price_series = {}
        self._last_t = None
        self._store = object_store_reader

    def owns(self, security):
        from .enums import SecurityType
        return security.symbol.security_type == SecurityType.BASE and str(security.symbol).upper() in self.feeds

    def after_initialize(self, session):
        algo = session.algo
        for key, (data_type, sec) in list(getattr(algo, '_custom_subscriptions', {}).items()):
            self.feeds[key] = self._load(session, data_type, sec)
        from .data import BarSeries
        algo._custom_manager = self
        session.data.register_provider(lambda sym: str(sym).upper() in self._price_series,
                                       lambda sym: self._price_series.get(str(sym).upper()))
        # a price series per subscription so the custom security can be traded and charted
        for key, feed in self.feeds.items():
            by_day = {}
            for end, p in feed.points:
                d = datetime(end.year, end.month, end.day)
                by_day[d] = _price_of(p)
            days = sorted(d for d, v in by_day.items() if v > 0)
            if not days:
                continue
            closes = [by_day[d] for d in days]
            bs = BarSeries.from_columns(key, f"custom ({', '.join(self.sources.get(key, [])[:1])})", days, closes,
                                        closes, closes, closes, [0.0] * len(days))
            bs.custom = True
            self._price_series[key] = bs

    def _load(self, session, data_type, sec):
        algo = session.algo
        cfg = _Config(sec.symbol, data_type, getattr(sec, 'resolution', None))
        proto = data_type()
        start = session.data.fetch_start
        end = session.end + timedelta(days=1)
        sources = {}
        d = start
        while d <= end:
            try:
                src = proto.get_source(cfg, d, False)
            except Exception as e:
                raise ValueError(f"{data_type.__name__}.get_source failed: {e}")
            if src is not None:
                k = (str(getattr(src, 'source', src)), int(getattr(src, 'transport_medium', 1) or 0))
                if k not in sources:
                    sources[k] = (d, src)
                    if len(sources) > _MAX_SOURCES:
                        raise not_supported(f"custom data {data_type.__name__} with one file per day",
                                            f"It names more than {_MAX_SOURCES} files (e.g. {k[0]}).")
            d += timedelta(days=1)
        key = str(sec.symbol).upper()
        self.sources[key] = [k[0] for k in sources]
        points = []
        for (src_name, medium), (d0, src) in sources.items():
            text = self._read(algo, src_name, medium, getattr(src, 'headers', None))
            fmt = int(getattr(src, 'format', 0) or 0)
            lines = [text] if fmt in (FileFormat.UNFOLDING_COLLECTION, FileFormat.INDEX) and \
                text.lstrip()[:1] in '[{' else text.splitlines()
            for line in lines:
                if not line.strip():
                    continue
                try:
                    obj = proto.reader(cfg, line, d0, False)
                except Exception:
                    obj = None   # LEAN skips lines its reader cannot parse (headers, comments)
                for p in self._unfold(obj):
                    if getattr(p, 'symbol', None) is None:
                        try:
                            p.symbol = sec.symbol
                        except Exception:
                            pass
                    t = getattr(p, 'time', None)
                    if not isinstance(t, datetime):
                        continue
                    e = getattr(p, 'end_time', None)
                    if not isinstance(e, datetime) or e < t:
                        e = t
                    points.append((e, p))
        points.sort(key=lambda x: x[0])
        if not points:
            raise not_supported(f"custom data {data_type.__name__}",
                                f"Its source ({', '.join(self.sources[key][:2])}) produced no data points.")
        return CustomFeed(sec.symbol, points)

    @staticmethod
    def _unfold(obj):
        if obj is None:
            return []
        data = getattr(obj, 'data', None)
        if isinstance(data, (list, tuple)) and data and not isinstance(obj, dict):
            return list(data)
        if isinstance(obj, (list, tuple)):
            return list(obj)
        return [obj]

    def _read(self, algo, src, medium, headers):
        if medium in (SubscriptionTransportMedium.REMOTE_FILE, SubscriptionTransportMedium.REST) or \
                src.startswith(('http://', 'https://')):
            return fetch_url(src, headers)
        if medium == SubscriptionTransportMedium.OBJECT_STORE:
            store = algo.object_store
            if store.contains_key(src):
                v = store.read(src)
                return v.decode('utf-8', errors='replace') if isinstance(v, bytes) else str(v)
            raise not_supported(f"custom data from object-store key '{src}'", "The key was never saved.")
        p = src
        if os.path.isfile(p):
            with open(p, 'r', encoding='utf-8', errors='replace') as f:
                return f.read()
        raise not_supported(f"custom data from the local file {src}",
                            "The file is not available (LEAN's data folder is not part of the engine).")

    # ---- per day ----
    def decorate_slice(self, session, sl, day):
        t1 = sl.time
        t0 = self._last_t if self._last_t is not None else datetime.min
        for key, feed in self.feeds.items():
            new = feed.new_between(t0, t1)
            if new:
                sec = session.algo.securities.get(key)
                sym = sec.symbol if sec is not None else feed.symbol
                sl.custom[sym] = new[-1]
                sl.has_data = True
                if sec is not None:
                    sec._custom_last = new[-1]
                    sec.cache._data[type(new[-1])] = list(new)
        self._last_t = t1

    def history(self, key, t, count=None, start=None):
        feed = self.feeds.get(str(key).upper())
        if feed is None:
            return []
        n = feed.known_at(t)
        pts = [p for _e, p in feed.points[:n]]
        if start is not None:
            pts = [p for e, p in feed.points[:n] if e >= start]
        if count is not None:
            pts = pts[-int(count):]
        return pts

    def silent_security(self, key):
        return False

    def notes(self, session):
        if not self.feeds:
            return []
        parts = []
        for key, srcs in self.sources.items():
            parts.append(f"{key} from {srcs[0]}" + (f" (+{len(srcs) - 1} more files)" if len(srcs) > 1 else ""))
        return ["Custom data read with the strategy's own reader: " + "; ".join(parts[:4]) + "."]

    def result_flags(self, session):
        if not self.feeds:
            return {}
        urls = []
        for srcs in self.sources.values():
            for u in srcs[:1]:
                if u not in urls:
                    urls.append(u)
        return {'uses_custom_data': True, 'custom_data_sources': urls[:20]}
