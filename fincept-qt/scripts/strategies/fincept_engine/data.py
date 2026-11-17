# ============================================================================
# Fincept Terminal - Strategy Engine Market Data
# Daily bars for the backtest loop and for self.history(): one store, real prices.
#
# The runner owns a DataSource. Every price a strategy can see - the bars of the
# loop, history() requests, indicator warm-up - comes from it, sliced so that no
# bar ends after the algorithm's clock (no look-ahead).
# ============================================================================

import bisect
from datetime import datetime, timedelta, time as dt_time
from typing import Callable, Dict, List, Optional

from .enums import SecurityType, Market

# A daily bar covers the trading day and is complete at the close. Bars are stamped
# with the session date (00:00); the moment the bar becomes known is that date at
# BAR_CLOSE. Scheduled events before BAR_CLOSE see only the previous day's bar.
BAR_CLOSE = dt_time(16, 0)
BAR_OPEN = dt_time(9, 30)
DAILY = timedelta(days=1)

# Calendar days fetched before the backtest start so history(), warm-up and
# indicator seeding have real bars to read (~500 trading days).
DEFAULT_LOOKBACK_DAYS = 730

_INDEX_MAP = {
    'SPX': '^GSPC', 'GSPC': '^GSPC', 'SPXW': '^GSPC', 'NDX': '^NDX', 'VIX': '^VIX', 'DJI': '^DJI',
    'DJIA': '^DJI', 'RUT': '^RUT', 'NIFTY50': '^NSEI', 'NIFTY': '^NSEI', 'NSEI': '^NSEI',
    'BANKNIFTY': '^NSEBANK', 'NIFTYBANK': '^NSEBANK', 'SENSEX': '^BSESN', 'N225': '^N225',
    'NIKKEI225': '^N225', 'FTSE': '^FTSE', 'UKX': '^FTSE', 'DAX': '^GDAXI', 'HSI': '^HSI',
    'STOXX50E': '^STOXX50E', 'SX5E': '^STOXX50E',
}

# Futures roots Yahoo carries as continuous front-month series (ROOT=F).
_FUTURE_ROOTS = {
    'ES', 'NQ', 'YM', 'RTY', 'MES', 'MNQ', 'MYM', 'M2K', 'GC', 'SI', 'HG', 'PL', 'PA', 'MGC', 'SIL',
    'CL', 'NG', 'HO', 'RB', 'BZ', 'MCL', 'ZC', 'ZW', 'ZS', 'ZM', 'ZL', 'ZO', 'KE', 'ZN', 'ZB', 'ZF',
    'ZT', 'UB', '6E', '6B', '6J', '6A', '6C', '6S', '6N', 'LE', 'GF', 'HE', 'SB', 'CT', 'KC', 'CC',
    'OJ', 'LBS', 'BTC', 'ETH', 'MBT', 'VX', 'DC',
}

# CFD tickers priced from the closest Yahoo series (a disclosed proxy).
_CFD_PROXY = {
    'XAUUSD': 'GC=F', 'XAGUSD': 'SI=F', 'SPX500USD': '^GSPC', 'NAS100USD': '^NDX',
    'US30USD': '^DJI', 'DE30EUR': '^GDAXI', 'UK100GBP': '^FTSE', 'JP225USD': '^N225',
    'WTICOUSD': 'CL=F', 'BCOUSD': 'BZ=F', 'NATGASUSD': 'NG=F', 'XCUUSD': 'HG=F',
}

# Crypto quote currencies, longest first so 'USDT' wins over 'USD'.
_CRYPTO_QUOTES = ['USDT', 'USDC', 'BUSD', 'TUSD', 'USD', 'EUR', 'GBP', 'JPY', 'BTC', 'ETH',
                  'BNB', 'AUD', 'CAD', 'CHF', 'KRW', 'TRY', 'BRL', 'INR']
_STABLE_TO_USD = {'USDT', 'USDC', 'BUSD', 'TUSD'}

_INDIA_MARKETS = {Market.INDIA, Market.NSE, 'india', 'nse'}
_BSE_MARKETS = {Market.BSE, 'bse'}

# Security types the engine can price from Yahoo daily bars.
TRADABLE_TYPES = {SecurityType.EQUITY, SecurityType.FOREX, SecurityType.CRYPTO, SecurityType.INDEX,
                  SecurityType.FUTURE, SecurityType.CFD, SecurityType.CRYPTO_FUTURE}


# US tickers that changed while the company stayed the same: Yahoo keeps the whole history under the
# new ticker (LEAN's map files do the same mapping). Only renames, never mergers or delistings.
_US_TICKER_CHANGES = {
    'FB': ('META', 'Meta Platforms changed its ticker from FB to META in June 2022'),
    'ANTM': ('ELV', 'Anthem changed its ticker from ANTM to ELV in June 2022'),
    'FISV': ('FI', 'Fiserv changed its ticker from FISV to FI in June 2023'),
    'RE': ('EG', 'Everest Re changed its ticker from RE to EG in July 2023'),
    'PKI': ('RVTY', 'PerkinElmer changed its ticker from PKI to RVTY in May 2023'),
}


def yahoo_ticker(ticker: str, security_type=SecurityType.EQUITY, market: str = Market.USA):
    """Yahoo Finance spelling of a strategy symbol, plus an optional disclosure note.

    Returns (yahoo_ticker or None, note or None). None means the security type has no
    Yahoo daily series (options, custom data).
    """
    t = str(ticker or '').strip().upper()
    if not t:
        return None, None
    st = security_type if security_type is not None else SecurityType.EQUITY
    mkt = str(market or '').lower()
    if st in (SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION, SecurityType.BASE):
        return None, None
    # Already a Yahoo spelling (BTC-USD, EURUSD=X, ^GSPC, ES=F, SBIN.NS).
    if any(c in t for c in ('=', '^')) or t.endswith(('.NS', '.BO')):
        return t, None
    if st == SecurityType.EQUITY:
        if mkt in _INDIA_MARKETS:
            return f"{t}.NS", None
        if mkt in _BSE_MARKETS:
            return f"{t}.BO", None
        ch = _US_TICKER_CHANGES.get(t)
        if ch is not None and ch[0]:
            return ch[0], f"{t} priced from Yahoo {ch[0]} ({ch[1]})"
        return t.replace('.', '-'), None  # BRK.B -> BRK-B
    if st == SecurityType.FOREX:
        return f"{t}=X", None
    if st in (SecurityType.CRYPTO, SecurityType.CRYPTO_FUTURE):
        if '-' in t:
            return t, None
        for q in _CRYPTO_QUOTES:
            if t.endswith(q) and len(t) > len(q):
                base = t[:-len(q)]
                note = None
                quote = q
                if q in _STABLE_TO_USD:
                    quote = 'USD'
                    note = f"{t} priced from Yahoo {base}-USD ({q} treated as USD)"
                if st == SecurityType.CRYPTO_FUTURE:
                    note = (f"{t} (crypto future) priced from the spot series {base}-{quote}; "
                            f"funding and basis are not modelled")
                return f"{base}-{quote}", note
        return t, None
    if st == SecurityType.INDEX:
        mapped = _INDEX_MAP.get(t)
        return (mapped or f"^{t}"), None
    if st == SecurityType.FUTURE:
        root = t.lstrip('/').split('=')[0]
        if root in _FUTURE_ROOTS:
            return f"{root}=F", (f"continuous future {t}: Yahoo's front-month series {root}=F "
                                 f"(rolled by Yahoo, not back-adjusted)")
        return None, None
    if st == SecurityType.CFD:
        proxy = _CFD_PROXY.get(t)
        if proxy:
            return proxy, f"CFD {t} priced from {proxy} as a proxy"
        return None, None
    return t, None


def _unsplit(bars, actions):
    """Split-adjusted bars -> prices as traded: multiply by every split ratio after the bar's date."""
    splits = sorted((str(d)[:10], float(r)) for d, r in (actions or {}).get('splits', []) if float(r) > 0)
    if not splits:
        return bars
    out = []
    for b in bars:
        day = str(b['time'])[:10]
        f = 1.0
        for d, r in splits:
            if d > day:
                f *= r
        if f != 1.0:
            b = dict(b)
            for k in ('open', 'high', 'low', 'close'):
                b[k] = b[k] * f
            b['volume'] = (b.get('volume') or 0.0) / f
        out.append(b)
    return out


class BarSeries:
    """Daily OHLCV bars of one symbol, oldest first, stamped with the session date."""

    __slots__ = ('symbol', 'yahoo', 'dates', 'open', 'high', 'low', 'close', 'volume', '_ends', '_pos',
                 'dividends', 'splits', 'show_in_bars', '__dict__')

    def __init__(self, symbol: str, yahoo: str, bars: List[Dict]):
        self.symbol = symbol
        self.yahoo = yahoo
        self.dividends = []   # [(datetime, amount)] ex-dates, when the source reported them
        self.splits = []      # [(datetime, ratio)]
        self.show_in_bars = True
        rows = []
        for b in bars or []:
            try:
                d = b.get('_dt')   # the price feed passes the parsed session date along
                if d is None:
                    d = datetime.strptime(str(b['time'])[:10], '%Y-%m-%d')
                elif d.hour or d.minute or d.second:
                    d = datetime(d.year, d.month, d.day)
                o, h, l, c = float(b['open']), float(b['high']), float(b['low']), float(b['close'])
                v = float(b.get('volume') or 0.0)
            except (KeyError, TypeError, ValueError):
                continue
            if not (c > 0) or c != c:  # drop empty / NaN rows Yahoo sometimes returns
                continue
            if not (o > 0) or o != o:
                o = c
            h = max(h, o, c) if h == h else max(o, c)
            l = min(l, o, c) if l == l else min(o, c)
            rows.append((d, o, h, l, c, v if v == v else 0.0))
        rows.sort(key=lambda r: r[0])
        dedup = []
        for r in rows:
            if dedup and dedup[-1][0] == r[0]:
                dedup[-1] = r
            else:
                dedup.append(r)
        self.dates = [r[0] for r in dedup]
        self.open = [r[1] for r in dedup]
        self.high = [r[2] for r in dedup]
        self.low = [r[3] for r in dedup]
        self.close = [r[4] for r in dedup]
        self.volume = [r[5] for r in dedup]
        self._ends = [d + timedelta(hours=BAR_CLOSE.hour, minutes=BAR_CLOSE.minute) for d in self.dates]
        self._pos = None

    @classmethod
    def from_columns(cls, symbol: str, label: str, dates, open_, high, low, close, volume):
        """A series built from ready columns (derived instruments); dates sorted, unique."""
        s = cls.__new__(cls)
        s.symbol, s.yahoo = symbol, label
        s.dividends, s.splits, s.show_in_bars = [], [], True
        s.dates, s.open, s.high, s.low, s.close, s.volume = (list(dates), list(open_), list(high), list(low),
                                                             list(close), list(volume))
        s._ends = [d + timedelta(hours=BAR_CLOSE.hour, minutes=BAR_CLOSE.minute) for d in s.dates]
        s._pos = None
        return s

    def __len__(self):
        return len(self.dates)

    def count_known_at(self, t: datetime) -> int:
        """Number of bars complete at time t (bar end <= t)."""
        return bisect.bisect_right(self._ends, t)

    def index_of_date(self, d: datetime) -> Optional[int]:
        pos = self._pos
        if pos is None:
            pos = self._pos = {x: i for i, x in enumerate(self.dates)}
        return pos.get(d)

    def bar(self, i: int) -> Dict:
        return {'time': self.dates[i], 'open': self.open[i], 'high': self.high[i],
                'low': self.low[i], 'close': self.close[i], 'volume': self.volume[i]}


class DataSource:
    """Lazily fetched, cached daily bars keyed by the STRATEGY's symbol name.

    fetcher(yahoo_tickers: list, start 'YYYY-MM-DD', end 'YYYY-MM-DD' exclusive) ->
    {yahoo_ticker: [ {time, open, high, low, close, volume}, ... ]}
    """

    def __init__(self, start: datetime, end: datetime, fetcher: Callable,
                 lookback_days: int = DEFAULT_LOOKBACK_DAYS, raw_fetcher: Callable = None,
                 actions_fetcher: Callable = None):
        self.start = start
        self.end = end
        self.fetch_start = start - timedelta(days=lookback_days)
        self._fetcher = fetcher
        # Price modes other than adjusted (LEAN DataNormalizationMode): key -> 'raw' | 'split_adjusted'.
        # raw_fetcher returns Yahoo's split-adjusted, not dividend-adjusted bars; RAW additionally
        # undoes later splits with the split ratios Yahoo reports (actions_fetcher).
        self._raw_fetcher = raw_fetcher
        self._actions_fetcher = actions_fetcher
        self._modes: Dict[str, str] = {}
        self._series: Dict[str, Optional[BarSeries]] = {}
        self.notes: List[str] = []
        self.failed: Dict[str, str] = {}  # symbol -> yahoo ticker that returned nothing
        # strategy symbol -> {'requested', 'used', 'reason'} when the series is read under another
        # ticker (a rename such as FB -> META, a proxy, or only Yahoo's spelling of the same instrument)
        self.mapping: Dict[str, Dict[str, Optional[str]]] = {}
        # Derived instruments (option/future contracts, custom data): providers claim a symbol
        # and build its series; tried before Yahoo. provider(symbol) -> BarSeries | None.
        self._providers: List = []
        self._raw: Dict[str, Optional[BarSeries]] = {}

    def set_mode(self, symbol, mode: str):
        """Price mode of a symbol before its series is first loaded: 'raw' or 'split_adjusted'
        (default: split- and dividend-adjusted)."""
        key = str(symbol).upper()
        if mode in ('raw', 'split_adjusted') and self._raw_fetcher is not None:
            if key in self._series and self._modes.get(key) != mode:
                self._series.pop(key, None)
            self._modes[key] = mode

    def mode_of(self, symbol) -> str:
        return self._modes.get(str(symbol).upper(), 'adjusted')

    def register_provider(self, claims, build):
        """claims(symbol) -> bool; build(symbol) -> BarSeries-like or None."""
        self._providers.append((claims, build))

    def has_provider(self, security_type=None, symbol=None) -> bool:
        return any(claims(symbol) for claims, _ in self._providers) if symbol is not None else False

    def resolve(self, ticker: str, security_type=None, market=None):
        y, note = yahoo_ticker(ticker, security_type, market)
        return y, note

    def has(self, symbol: str) -> bool:
        return self._series.get(str(symbol).upper()) is not None

    def series(self, symbol, security_type=None, market=None) -> Optional[BarSeries]:
        """Bars for `symbol` (a Symbol or ticker). Fetched once; None if unavailable."""
        key = str(symbol).upper()
        if key in self._series:
            return self._series[key]
        if isinstance(symbol, object) and not isinstance(symbol, str):
            for claims, build in self._providers:
                if claims(symbol):
                    s = build(symbol)
                    self._series[key] = s
                    return s
        if security_type is None:
            security_type = getattr(symbol, 'security_type', SecurityType.EQUITY)
        if market is None:
            market = getattr(symbol, 'market', Market.USA)
        y, note = yahoo_ticker(key, security_type, market)
        if not y:
            self._series[key] = None
            return None
        raw = {}
        pre = getattr(self, '_prefetched', None)
        mode = self._modes.get(key)
        try:
            if mode is not None:
                raw = self._raw_fetcher([y], self.fetch_start.strftime('%Y-%m-%d'),
                                        (self.end + timedelta(days=1)).strftime('%Y-%m-%d')) or {}
                if mode == 'raw' and self._actions_fetcher is not None and raw.get(y):
                    raw = {y: _unsplit(raw[y], self._actions_fetcher(y))}
            elif pre is not None and y in pre:
                raw = {y: pre.pop(y)}
            else:
                raw = self._fetcher([y], self.fetch_start.strftime('%Y-%m-%d'),
                                    (self.end + timedelta(days=1)).strftime('%Y-%m-%d')) or {}
        except Exception as e:  # network / provider failure: treated as "no data"
            self.failed[key] = f"{y} ({e})"
            self._series[key] = None
            return None
        bars = raw.get(y)
        if bars is None and len(raw) == 1:
            bars = next(iter(raw.values()))
        s = BarSeries(key, y, bars or [])
        if len(s) == 0:
            self.failed[key] = y
            self._series[key] = None
            return None
        if y != key:
            self.mapping[key] = {'requested': key, 'used': y, 'reason': note}
        if note and note not in self.notes:
            self.notes.append(note)
        if mode is not None:
            n2 = (f"{key}: {'raw (as traded)' if mode == 'raw' else 'split-adjusted, not dividend-adjusted'} "
                  f"prices; dividends are paid in cash")
            if n2 not in self.notes:
                self.notes.append(n2)
        self._series[key] = s
        return s

    def preload(self, items):
        """items: iterable of (key, security_type, market): one batched fetch for every symbol not
        loaded yet (the fetcher downloads in batches and caches), then the series are built."""
        want = {}
        for key, st, mkt in items:
            k = str(key).upper()
            if k in self._series:
                continue
            y, _n = yahoo_ticker(k, st if st is not None else SecurityType.EQUITY, mkt or Market.USA)
            if y:
                want.setdefault(y, []).append((k, st, mkt))
        if len(want) > 1:
            try:
                raw = self._fetcher(list(want), self.fetch_start.strftime('%Y-%m-%d'),
                                    (self.end + timedelta(days=1)).strftime('%Y-%m-%d')) or {}
            except Exception:
                raw = None
            if raw is not None:
                self._prefetched = getattr(self, '_prefetched', {})
                self._prefetched.update(raw)
                for y in want:
                    self._prefetched.setdefault(y, [])
        for y, entries in want.items():
            for k, st, mkt in entries:
                self.series(k, st, mkt)

    def loaded(self) -> Dict[str, BarSeries]:
        return {k: v for k, v in self._series.items() if v is not None}
