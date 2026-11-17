# ============================================================================
# Fincept Terminal - Strategy Engine Option Model (market inputs + quotes)
#
# Model quotes of option contracts from real data, with data access injected:
#   get_series(yahoo_ticker) -> None or a series with .dates (session dates at
#       00:00), .open/.high/.low/.close lists and count_known_at(t) (bars
#       complete at t; the bar of date d completes at d 16:00);
#   get_dividends(yahoo_ticker) -> [(datetime, amount), ...] (ex-dates).
#
# No look-ahead: a quote at the open of day d (at='open', t >= d 09:30) may
# use day d's OPEN of a volatility index and closes up to d-1 for everything
# else; a quote at the close (t >= d 16:00) may use day d's close. Nothing
# later is ever read (count_known_at(t) is the guard).
# ============================================================================

import bisect
import math
from datetime import datetime, time as dt_time, timedelta

from .enums import OptionRight, SecurityType
from .option_chain import Greeks, OptionQuote, time_to_expiry, CHAIN_DISCLOSURE
from .option_pricing import (MODEL_BLACK76, MODEL_BSM, EUROPEAN_EXERCISE_DISCLOSURE, GREEK_UNITS,
                             option_price, option_price_and_greeks, intrinsic_value)
from .types import Symbol

_OPTION_TYPES = (SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION)
_OPEN = dt_time(9, 30)
_YEAR_DAYS = 365.0

# Volatility index family of each underlying / option root / Yahoo ticker.
VOL_FAMILY = {
    'SPX': 'VIX', 'SPXW': 'VIX', 'XSP': 'VIX', 'SPY': 'VIX', '^GSPC': 'VIX', 'GSPC': 'VIX', 'ES': 'VIX',
    'MES': 'VIX', 'VOO': 'VIX', 'IVV': 'VIX', 'OEX': 'VIX', 'XEO': 'VIX',
    'NDX': 'VXN', 'NDXP': 'VXN', 'NQX': 'VXN', 'QQQ': 'VXN', '^NDX': 'VXN', 'NQ': 'VXN', 'MNQ': 'VXN',
    'RUT': 'RVX', 'RUTW': 'RVX', 'IWM': 'RVX', '^RUT': 'RVX', 'RTY': 'RVX', 'M2K': 'RVX',
    'DIA': 'VXD', 'DJX': 'VXD', '^DJI': 'VXD', 'YM': 'VXD', 'MYM': 'VXD',
    'GLD': 'GVZ', 'GC': 'GVZ', 'MGC': 'GVZ',
    'USO': 'OVX', 'CL': 'OVX', 'MCL': 'OVX',
    'VIX': 'VVIX', 'VIXW': 'VVIX', '^VIX': 'VVIX',
}
# Yahoo tickers of each family with their horizon in calendar days (term structure where it exists).
VOL_INDEX_TICKERS = {
    'VIX': (('^VIX9D', 9), ('^VIX', 30), ('^VIX3M', 93), ('^VIX6M', 183)),
    'VXN': (('^VXN', 30),),
    'RVX': (('^RVX', 30),),
    'VXD': (('^VXD', 30),),
    'GVZ': (('^GVZ', 30),),
    'OVX': (('^OVX', 30),),
    'VVIX': (('^VVIX', 30),),
}
# Index options take their dividend yield from the proxy ETF.
DIVIDEND_PROXY = {'SPX': 'SPY', 'SPXW': 'SPY', 'XSP': 'SPY', 'OEX': 'OEF', 'XEO': 'OEF', 'NDX': 'QQQ',
                  'NDXP': 'QQQ', 'NQX': 'QQQ', 'RUT': 'IWM', 'RUTW': 'IWM', 'DJX': 'DIA', '^GSPC': 'SPY',
                  '^NDX': 'QQQ', '^RUT': 'IWM', '^DJI': 'DIA'}
INDEX_ROOTS = frozenset({'SPX', 'SPXW', 'XSP', 'OEX', 'XEO', 'NDX', 'NDXP', 'NQX', 'RUT', 'RUTW', 'DJX', 'VIX',
                         'VIXW'})
RATE_TICKER = '^IRX'

# Skew rule: sigma(K) = sigma_ATM * clip(1 - b*x + c*x^2, SKEW_MIN, SKEW_MAX), x = ln(K/F) / sqrt(T).
SKEW_PARAMS = {
    'index': (1.0, 0.6),       # S&P/Nasdaq/Russell/Dow indexes, their ETFs and futures
    'equity': (0.4, 0.4),
    'vix': (-0.5, 0.3),        # VIX options: upside (call) skew
    'metal': (0.1, 0.4),
    'energy': (0.3, 0.4),
    'future': (0.2, 0.4),
}
SKEW_MIN, SKEW_MAX = 0.5, 3.0
_INDEX_FAMILIES = {'VIX', 'VXN', 'RVX', 'VXD'}

# Spread rule: full spread = max(min, pct * mid); bid = mid - spread/2 floored at 0, ask = mid + spread/2.
SPREAD_RULES = {'equity': (0.05, 0.03), 'index': (0.10, 0.02), 'future': (None, 0.02)}
# Futures options: the minimum full spread is two ticks of the option.
FUTURE_OPTION_TICK = {'ES': 0.05, 'MES': 0.05, 'NQ': 0.05, 'MNQ': 0.05, 'YM': 1.0, 'MYM': 1.0, 'RTY': 0.05,
                      'M2K': 0.05, 'GC': 0.10, 'MGC': 0.10, 'SI': 0.001, 'HG': 0.0005, 'CL': 0.01, 'MCL': 0.01,
                      'NG': 0.001, 'ZB': 1 / 64, 'UB': 1 / 64, 'ZN': 1 / 64, 'ZF': 1 / 128, 'ZT': 1 / 256,
                      'ZC': 0.125, 'ZS': 0.125, 'ZW': 0.125, 'KE': 0.125, 'ZL': 0.005, 'ZM': 0.05,
                      '6E': 0.00005, '6B': 0.0001, '6A': 0.00005, '6C': 0.00005, '6S': 0.00005,
                      'LE': 0.025, 'HE': 0.025}
DEFAULT_FUTURE_OPTION_TICK = 0.01


def _desc_spread(kind, tick=None):
    mn, pct = SPREAD_RULES[kind]
    if kind == 'future':
        return (f"bid/ask = model mid -/+ half of max(2 option ticks ({2 * (tick or DEFAULT_FUTURE_OPTION_TICK):g}), "
                f"{pct:.0%} of mid), bid floored at 0, no tick rounding")
    return f"bid/ask = model mid -/+ half of max(${mn:.2f}, {pct:.0%} of mid), bid floored at 0, no tick rounding"


class UnderlyingInfo:
    """How one underlying is modelled (see OptionModel.info)."""

    __slots__ = ('key', 'kind', 'root', 'option_root', 'yahoo', 'family', 'vol_tickers', 'dividend_ticker',
                 'model', 'skew', 'spread_kind', 'tick', 'american')

    def __repr__(self):
        return (f"UnderlyingInfo({self.key}: kind={self.kind}, yahoo={self.yahoo}, family={self.family}, "
                f"model={self.model})")


class _State:
    """Market inputs of one underlying at one moment (cached per (underlying, t, at))."""
    __slots__ = ('rate', 'q', 'points', 'realized', 'vix_points', 'sources')


class OptionModel:
    """Market inputs for option pricing (volatility, rate, dividend yield) and model quotes.

    get_series(yahoo_ticker) and get_dividends(yahoo_ticker) are injected (no data access here).
    Every quote is reproducible from data known at its time (see module header)."""

    def __init__(self, get_series, get_dividends=None, realized_window: int = 30, vol_floor: float = 0.10,
                 default_vol: float = 0.30, max_stale_days: int = 10, ticker_map: dict = None):
        self._get_series = get_series
        self._get_dividends = get_dividends
        self.realized_window = int(realized_window)
        self.vol_floor = float(vol_floor)
        self.default_vol = float(default_vol)
        self.max_stale_days = int(max_stale_days)
        self._ticker_map = {str(k).upper(): v for k, v in (ticker_map or {}).items()}
        self._series_cache = {}
        self._div_cache = {}
        self._info_cache = {}
        self._state_cache = {}
        self._rate_cache = {}
        self._q_cache = {}
        self._rv_cache = {}
        self.fallbacks = {}      # underlying key -> {reason: count}

    # ------------------------------------------------------------------ data
    def _series(self, ticker):
        if ticker is None:
            return None
        if ticker not in self._series_cache:
            s = self._get_series(ticker)
            if s is not None and not getattr(s, 'dates', None):
                s = None
            self._series_cache[ticker] = s
        return self._series_cache[ticker]

    def _known_value(self, ticker, t: datetime, at: str, use_open: bool):
        """(value, bar date) of `ticker` known at t, or None. With use_open and at='open', day d's open
        counts once t >= d 09:30; otherwise the last complete bar's close. Values older than
        max_stale_days are treated as missing."""
        s = self._series(ticker)
        if s is None:
            return None
        n = s.count_known_at(t)
        if use_open and at == 'open' and t.time() >= _OPEN and n < len(s.dates) and s.dates[n].date() == t.date():
            v = s.open[n]
            if v is not None and v == v and v > 0:
                return v, s.dates[n]
        if n <= 0:
            return None
        d = s.dates[n - 1]
        if (t.date() - d.date()).days > self.max_stale_days:
            return None
        v = s.close[n - 1]
        if v is None or v != v:
            return None
        return v, d

    def _note(self, key, reason):
        r = self.fallbacks.setdefault(key, {})
        r[reason] = r.get(reason, 0) + 1

    # ------------------------------------------------------------- underlying
    def info(self, underlying) -> UnderlyingInfo:
        """How `underlying` (a contract Symbol, canonical option, underlying Symbol or ticker) is modelled."""
        opt_root = None
        if isinstance(underlying, Symbol):
            if underlying.security_type in _OPTION_TYPES:
                opt_root = underlying.id.symbol
                st_opt = underlying.security_type
                u = underlying.underlying
                if u is None:
                    raise ValueError(f"OptionModel: option symbol {underlying} has no underlying")
            else:
                u = underlying
                st_opt = None
            ckey = (u.value, int(u.security_type), opt_root)
        else:
            s = str(underlying).strip().upper()
            if s.startswith('?'):
                opt_root = s[1:]
                s = s[1:]
            ckey = (s, None, opt_root)
            u = None
            st_opt = None
        info = self._info_cache.get(ckey)
        if info is not None:
            return info
        info = self._build_info(u, ckey[0], opt_root, st_opt)
        self._info_cache[ckey] = info
        return info

    def _build_info(self, u, ticker, opt_root, st_opt):
        from .data import yahoo_ticker
        info = UnderlyingInfo()
        if u is not None:
            st = u.security_type
            root = u.id.symbol
        else:
            root = ticker
            if ticker.endswith('=F'):
                root, st = ticker[:-2], SecurityType.FUTURE
            elif ticker in INDEX_ROOTS or ticker.startswith('^'):
                st = SecurityType.INDEX
            else:
                st = SecurityType.EQUITY
        if root in ('SPXW', 'XSP'):
            root = 'SPX' if root == 'SPXW' else root
        info.root = root
        info.option_root = opt_root or root
        info.key = f"{info.option_root}" if opt_root else root
        mapped = self._ticker_map.get(root)
        if st == SecurityType.FUTURE:
            info.kind = 'future'
            info.yahoo = mapped or f"{root}=F"
        elif st == SecurityType.INDEX:
            info.kind = 'vix' if root == 'VIX' else 'index'
            info.yahoo = mapped or yahoo_ticker(root, SecurityType.INDEX)[0]
        else:
            info.kind = 'equity'
            info.yahoo = mapped or yahoo_ticker(root, SecurityType.EQUITY)[0]
        fam = (VOL_FAMILY.get(info.option_root) or VOL_FAMILY.get(root) or VOL_FAMILY.get(info.yahoo or ''))
        info.family = fam
        info.vol_tickers = VOL_INDEX_TICKERS.get(fam, ()) if fam else ()
        if info.kind == 'equity':
            info.dividend_ticker = info.yahoo
        elif info.kind == 'index':
            proxy = DIVIDEND_PROXY.get(info.option_root) or DIVIDEND_PROXY.get(root)
            info.dividend_ticker = yahoo_ticker(proxy, SecurityType.EQUITY)[0] if proxy else None
        else:
            info.dividend_ticker = None
        info.model = MODEL_BLACK76 if info.kind in ('future', 'vix') else MODEL_BSM
        if info.kind == 'vix':
            info.skew = SKEW_PARAMS['vix']
        elif fam in _INDEX_FAMILIES:
            info.skew = SKEW_PARAMS['index']
        elif fam == 'GVZ' or root in ('GC', 'SI', 'MGC', 'GLD', 'SLV', 'HG'):
            info.skew = SKEW_PARAMS['metal']
        elif fam == 'OVX' or root in ('CL', 'MCL', 'NG', 'USO', 'BZ', 'HO', 'RB'):
            info.skew = SKEW_PARAMS['energy']
        elif info.kind == 'future':
            info.skew = SKEW_PARAMS['future']
        else:
            info.skew = SKEW_PARAMS['equity']
        if st_opt == SecurityType.FUTURE_OPTION or (st_opt is None and info.kind == 'future'):
            info.spread_kind = 'future'
        elif st_opt == SecurityType.INDEX_OPTION or (st_opt is None and info.kind in ('index', 'vix')):
            info.spread_kind = 'index'
        else:
            info.spread_kind = 'equity'
        info.tick = FUTURE_OPTION_TICK.get(root, DEFAULT_FUTURE_OPTION_TICK)
        info.american = info.kind in ('equity', 'future') or info.option_root == 'OEX'
        return info

    def vol_source(self, underlying) -> dict:
        """Where `underlying`'s volatility comes from: {'family', 'indexes': [(yahoo ticker, days), ...] that
        the data source has, 'fallback': realised-volatility ticker, 'skew': (b, c), 'model'}."""
        info = underlying if isinstance(underlying, UnderlyingInfo) else self.info(underlying)
        return {'family': info.family,
                'indexes': [(tk, days) for tk, days in info.vol_tickers if self._series(tk) is not None],
                'fallback': info.yahoo, 'skew': info.skew, 'model': info.model}

    # ------------------------------------------------------------------ rate
    def rate(self, t: datetime, at: str = 'close') -> float:
        """Continuously compounded risk-free rate from ^IRX known at t (13-week T-bill discount yield in
        percent: price = 1 - y*91/360, r = -ln(price) * 365/91). The last known value is carried over
        missing days; 0.0 when ^IRX is unavailable (counted in fallbacks)."""
        key = (t, at)
        r = self._rate_cache.get(key)
        if r is None:
            kv = self._known_value(RATE_TICKER, t, 'close', False)
            if kv is None:
                self._note('rate', f"{RATE_TICKER} unavailable: rate 0 used")
                r = 0.0
            else:
                y = kv[0] / 100.0
                p = 1.0 - y * 91.0 / 360.0
                r = -math.log(p) * _YEAR_DAYS / 91.0 if p > 0 else 0.0
            if len(self._rate_cache) > 50000:
                self._rate_cache.clear()
            self._rate_cache[key] = r
        return r

    # ------------------------------------------------------------- dividends
    def _dividends(self, ticker):
        d = self._div_cache.get(ticker)
        if d is None:
            rows = []
            if self._get_dividends is not None and ticker:
                for when, amt in (self._get_dividends(ticker) or []):
                    try:
                        a = float(amt)
                    except (TypeError, ValueError):
                        continue
                    if a > 0:
                        rows.append((when if isinstance(when, datetime) else datetime(when.year, when.month,
                                                                                       when.day), a))
            rows.sort()
            dates = [r[0].date() for r in rows]
            cum = [0.0]
            for _, a in rows:
                cum.append(cum[-1] + a)
            d = self._div_cache[ticker] = (dates, cum)
        return d

    def dividend_yield(self, underlying, t: datetime, at: str = 'close', price: float = None) -> float:
        """Trailing-12-month cash dividends with ex-date <= t's date, divided by the price known at t (the
        last complete close of the dividend series, else `price`). Index options use the proxy ETF
        (SPX->SPY, NDX->QQQ, RUT->IWM, DJX->DIA); futures and VIX options: 0. Capped at 25%."""
        info = underlying if isinstance(underlying, UnderlyingInfo) else self.info(underlying)
        tk = info.dividend_ticker
        if not tk:
            return 0.0
        key = (tk, t, at)
        q = self._q_cache.get(key)
        if q is not None:
            return q
        dates, cum = self._dividends(tk)
        q = 0.0
        if dates:
            d1 = t.date()
            hi = bisect.bisect_right(dates, d1)
            lo = bisect.bisect_right(dates, d1 - timedelta(days=365))
            total = cum[hi] - cum[lo]
            if total > 0:
                kv = self._known_value(tk, t, at, False)
                p = kv[0] if kv is not None else (price if (info.kind == 'equity' and price) else None)
                if p and p > 0:
                    q = min(total / p, 0.25)
                else:
                    self._note(info.yahoo, f"no {tk} price for the dividend yield: 0 used")
        if len(self._q_cache) > 50000:
            self._q_cache.clear()
        self._q_cache[key] = q
        return q

    # ------------------------------------------------------------ volatility
    def _term_points(self, tickers, t, at):
        pts = []
        for tk, days in tickers:
            kv = self._known_value(tk, t, at, True)
            if kv is not None and kv[0] > 0:
                pts.append((days / _YEAR_DAYS, kv[0] / 100.0))
        pts.sort()
        return pts

    def realized_vol(self, ticker, t: datetime, at: str = 'close'):
        """Close-to-close realised volatility (sample std of log returns * sqrt(252)) over the last
        `realized_window` returns complete at t; None with fewer than 10 returns."""
        s = self._series(ticker)
        if s is None:
            return None
        n = s.count_known_at(t)
        key = (ticker, n)
        if key in self._rv_cache:
            return self._rv_cache[key]
        closes = [c for c in s.close[max(0, n - self.realized_window - 1):n] if c and c > 0]
        rv = None
        if len(closes) >= 11:
            rets = [math.log(b / a) for a, b in zip(closes, closes[1:])]
            m = sum(rets) / len(rets)
            var = sum((x - m) ** 2 for x in rets) / (len(rets) - 1)
            rv = math.sqrt(var * 252.0)
        self._rv_cache[key] = rv
        return rv

    def _state(self, info: UnderlyingInfo, t: datetime, at: str) -> _State:
        key = (info.key, info.yahoo, t, at)
        st = self._state_cache.get(key)
        if st is not None:
            return st
        st = _State()
        st.rate = self.rate(t, at)
        st.q = self.dividend_yield(info, t, at) if info.model == MODEL_BSM else 0.0
        st.points = self._term_points(info.vol_tickers, t, at) if info.vol_tickers else []
        st.realized = None
        st.sources = None
        if not st.points:
            rv = self.realized_vol(info.yahoo, t, at)
            if rv is None:
                self._note(info.yahoo, f"fewer than 10 daily returns of {info.yahoo}: default "
                                     f"{self.default_vol:.0%} volatility used")
                rv = self.default_vol
            elif info.vol_tickers:
                self._note(info.yahoo, f"{'/'.join(tk for tk, _ in info.vol_tickers)} unavailable: "
                                     f"{self.realized_window}-day realised volatility of {info.yahoo} used")
            st.realized = max(rv, self.vol_floor)
        st.vix_points = self._term_points(VOL_INDEX_TICKERS['VIX'], t, at) if info.kind == 'vix' else []
        if len(self._state_cache) > 20000:
            self._state_cache.clear()
        self._state_cache[key] = st
        return st

    @staticmethod
    def _interp_vol(points, T):
        """Interpolate (T_i, sigma_i) linearly in total variance; flat beyond the ends."""
        if len(points) == 1 or T <= points[0][0]:
            return points[0][1]
        if T >= points[-1][0]:
            return points[-1][1]
        for (t0, s0), (t1, s1) in zip(points, points[1:]):
            if t0 <= T <= t1:
                w0, w1 = s0 * s0 * t0, s1 * s1 * t1
                w = w0 + (w1 - w0) * (T - t0) / (t1 - t0)
                return math.sqrt(max(w, 1e-12) / T)
        return points[-1][1]

    def _atm(self, st: _State, T: float) -> float:
        if st.points:
            return max(self._interp_vol(st.points, max(T, 1e-6)), self.vol_floor * 0.5)
        return st.realized

    def atm_vol(self, underlying, t: datetime, T: float = 30 / 365.0, at: str = 'close') -> float:
        """At-the-money volatility for horizon T (years) known at t: the family's CBOE volatility index
        (term structure interpolated in total variance where it exists), else realised volatility."""
        info = underlying if isinstance(underlying, UnderlyingInfo) else self.info(underlying)
        return self._atm(self._state(info, t, at), T)

    def skewed_vol(self, info: UnderlyingInfo, atm: float, K: float, F: float, T: float) -> float:
        """sigma(K) = atm * clip(1 - b*x + c*x^2, SKEW_MIN, SKEW_MAX) with x = ln(K/F)/sqrt(T)."""
        if K <= 0 or F <= 0:
            return atm
        b, c = info.skew
        x = math.log(K / F) / math.sqrt(max(T, 1.0 / _YEAR_DAYS))
        m = 1.0 - b * x + c * x * x
        return atm * min(max(m, SKEW_MIN), SKEW_MAX)

    def vix_forward(self, t: datetime, T: float, spot: float, at: str = 'close', st: _State = None) -> float:
        """VIX forward for expiry T: sqrt of the S&P 500 forward variance between T and T+30 days from the
        ^VIX9D/^VIX/^VIX3M/^VIX6M term structure, scaled so that T=0 gives `spot` (no convexity adjustment).
        `spot` itself when fewer than two term points are known."""
        pts = st.vix_points if st is not None else self._term_points(VOL_INDEX_TICKERS['VIX'], t, at)
        if len(pts) < 2 or T <= 0:
            return spot
        tau = 30.0 / _YEAR_DAYS
        s0 = self._interp_vol(pts, tau)
        w_a = self._interp_vol(pts, T) ** 2 * T
        w_b = self._interp_vol(pts, T + tau) ** 2 * (T + tau)
        fwd = math.sqrt(max(w_b - w_a, 1e-12) / tau)
        return spot * fwd / s0 if s0 > 0 else spot

    # ----------------------------------------------------------------- quotes
    def quote(self, contract_symbol, underlying_price: float, t: datetime, at: str = 'close') -> OptionQuote:
        """Model quote of an option contract at t from the underlying price known at t (the futures price for
        futures options, spot VIX for VIX options). mid = Black-Scholes-Merton (Black-76 for futures and VIX
        options) at the skewed volatility; bid/ask from the spread rule; greeks are computed on first access.
        At or after expiry the quote is the intrinsic value with no spread."""
        if at not in ('open', 'close'):
            raise ValueError("OptionModel.quote: at must be 'open' or 'close'")
        if contract_symbol.security_type not in _OPTION_TYPES or contract_symbol.is_canonical():
            raise ValueError(f"OptionModel.quote: {contract_symbol} is not an option contract")
        info = self.info(contract_symbol)
        S = float(underlying_price)
        K = contract_symbol.id.strike_price
        is_call = int(contract_symbol.id.option_right) == int(OptionRight.CALL)
        T = time_to_expiry(contract_symbol, t)
        st = self._state(info, t, at)
        r = st.rate
        q = st.q
        model = info.model
        if T <= 0.0:
            mid = intrinsic_value(S, K, is_call)
            g = Greeks.lazy(lambda: (_expired_delta(S, K, is_call), 0.0, 0.0, 0.0, 0.0, 0.0))
            return OptionQuote(contract_symbol, t, mid, mid, mid, 0.0, g, S, at, S, 0.0, r, q, model, None)
        if info.kind == 'vix':
            F = self.vix_forward(t, T, S, at, st)
        elif model == MODEL_BLACK76:
            F = S
        else:
            F = S * math.exp((r - q) * T)
        atm = self._atm(st, T)
        sigma = self.skewed_vol(info, atm, K, F, T)
        und = F if model == MODEL_BLACK76 else S
        mid = option_price(model, und, K, T, r, q, sigma, is_call)
        bid, ask = self.bid_ask(info, mid)

        def compute(und=und, K=K, T=T, r=r, q=q, sigma=sigma, is_call=is_call, model=model, mid=mid):
            p, d, gm, v, th, rh = option_price_and_greeks(model, und, K, T, r, q, sigma, is_call)
            lam = d * und / p if p > 1e-12 else 0.0
            return d, gm, v, th, rh, lam

        return OptionQuote(contract_symbol, t, mid, bid, ask, sigma, Greeks.lazy(compute), S, at, F, T, r, q,
                           model, atm)

    def bid_ask(self, info: UnderlyingInfo, mid: float):
        mn, pct = SPREAD_RULES[info.spread_kind]
        if info.spread_kind == 'future':
            mn = 2.0 * info.tick
        half = 0.5 * max(mn, pct * mid)
        return max(mid - half, 0.0), mid + half

    def quotes(self, contract_symbols, underlying_price: float, t: datetime, at: str = 'close') -> dict:
        """{symbol: OptionQuote} for many contracts of one underlying at one moment."""
        return {s: self.quote(s, underlying_price, t, at) for s in contract_symbols}

    def quote_fn(self, underlying_price: float, t: datetime, at: str = 'close'):
        """A callable(symbol) -> OptionQuote bound to one moment, for OptionContract / OptionFilterUniverse."""
        return lambda s: self.quote(s, underlying_price, t, at)

    # ------------------------------------------------------------- disclosure
    def disclosure(self, underlying, include_chain: bool = True) -> str:
        """The text every backtest result using model prices for `underlying` must carry."""
        info = underlying if isinstance(underlying, UnderlyingInfo) else self.info(underlying)
        if info.model == MODEL_BLACK76:
            if info.kind == 'vix':
                model = (f"Black-76 on a VIX forward derived from real {info.yahoo} prices (S&P 500 forward variance "
                         f"from ^VIX9D/^VIX/^VIX3M/^VIX6M between expiry and expiry + 30 days, scaled to spot VIX, no "
                         f"convexity adjustment; spot VIX when the term structure is missing)")
            else:
                model = f"Black-76 on real {info.yahoo} futures prices (continuous front-month series)"
        else:
            model = f"Black-Scholes-Merton on real {info.yahoo} prices"
        names = [tk for tk, _ in info.vol_tickers]
        avail = [tk for tk in names if self._series(tk) is not None]
        if avail:
            if len(avail) > 1:
                vol = (f"at-the-money volatility from the CBOE {'/'.join(avail)} indexes (term structure "
                       f"interpolated in total variance, flat beyond the ends)")
            else:
                vol = f"at-the-money volatility from the CBOE {avail[0]} index level"
            if info.kind == 'vix':
                vol = vol.replace("at-the-money volatility", "at-the-money volatility of VIX")
        else:
            vol = (f"at-the-money volatility = {self.realized_window}-day close-to-close realised volatility of "
                   f"{info.yahoo}, floored at {self.vol_floor:.0%}")
        b, c = info.skew
        skew = (f"skew rule sigma(K) = sigma_ATM * clip(1 {'-' if b >= 0 else '+'} {abs(b):g}*x + {c:g}*x^2, "
                f"{SKEW_MIN:g}, {SKEW_MAX:g}), x = ln(K/F)/sqrt(T)")
        rate = (f"risk-free rate from the 13-week T-bill discount yield ({RATE_TICKER}) converted to a "
                f"continuously compounded rate")
        if info.model == MODEL_BLACK76:
            div = "no dividend yield (futures/forward pricing)"
        elif info.dividend_ticker and info.dividend_ticker != info.yahoo:
            div = (f"dividend yield = trailing-12-month cash dividends of {info.dividend_ticker} (proxy) / its "
                   f"price")
        else:
            div = f"dividend yield = trailing-12-month cash dividends of {info.yahoo} / price"
        exercise = ("European exercise (these index options are European-style)." if not info.american else
                    EUROPEAN_EXERCISE_DISCLOSURE)
        spread = _desc_spread(info.spread_kind, info.tick if info.spread_kind == 'future' else None)
        text = (f"Option prices are model-derived, not historical market quotes: {model}, with {vol}, a {skew}, "
                f"the {rate} and {div}; {spread}. {exercise} No look-ahead: quotes at the open use that day's "
                f"open of the volatility index and closes up to the previous day; quotes at the close use that "
                f"day's close. {GREEK_UNITS}")
        fb = {}
        for k in (info.yahoo, 'rate'):
            for reason, n in self.fallbacks.get(k, {}).items():
                fb[reason] = fb.get(reason, 0) + n
        if fb:
            text += " Fallbacks used: " + "; ".join(f"{reason} ({n} quote times)" for reason, n in sorted(fb.items())) + "."
        if include_chain:
            text += " " + CHAIN_DISCLOSURE
        return text


def _expired_delta(S, K, is_call):
    if is_call:
        return 1.0 if S > K else (0.5 if S == K else 0.0)
    return -1.0 if S < K else (-0.5 if S == K else 0.0)
