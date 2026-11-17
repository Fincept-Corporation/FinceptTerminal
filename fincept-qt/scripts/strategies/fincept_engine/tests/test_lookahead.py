# No look-ahead for derived data: perturb every bar after a cut date (underlying, volatility index,
# rates, universe pool) and check that nothing the strategy saw or decided up to the cut changes.
# Same idea as the engine's correctness suite, for an options strategy and a universe strategy.
import math
import random
from datetime import datetime, timedelta

from AlgorithmImports import *  # noqa: F401,F403
from engine_fixture import Feed, run, ok, trading_days
from fincept_engine.universe_data import UniverseDataProvider

CUT = datetime(2022, 3, 15)
START, END = '2022-01-03', '2022-05-31'


def perturb(fn, seed):
    """fn(d) with every bar after CUT multiplied by a random walk (deterministic per seed)."""
    rnd = random.Random(seed)
    days = trading_days(CUT + timedelta(days=1), datetime(2022, 12, 31))
    walk, f = {}, 1.0
    for d in days:
        f *= 1 + rnd.gauss(0, 0.03)
        walk[d] = f

    def g(d):
        v = fn(d)
        k = walk.get(d, 1.0)
        return tuple(x * k for x in v) if isinstance(v, tuple) else v * k
    return g


def spy(d):
    x = (d - datetime(2021, 1, 1)).days
    c = 400 + 20 * math.sin(x / 9.0) + 0.05 * x
    return (c - 1, c + 2, c - 3, c)


def vix(d):
    return 20 + 5 * math.sin((d - datetime(2021, 1, 1)).days / 13.0)


def irx(d):
    return 0.5 + 0.01 * (d - datetime(2021, 1, 1)).days / 30.0


class OptionProbe(QCAlgorithm):
    def initialize(self):
        self.set_start_date(2022, 1, 3)
        self.set_end_date(2022, 5, 31)
        self.set_cash(1_000_000)
        self.spy = self.add_equity('SPY', Resolution.DAILY).symbol
        opt = self.add_option('SPY', Resolution.DAILY)
        opt.set_filter(-2, 2, 0, 40)
        self.opt = opt.symbol
        self.seen = []
        type(self).last = self

    def on_data(self, data):
        chain = data.option_chains.get(self.opt)
        if not chain:
            return
        calls = sorted([c for c in chain if c.right == OptionRight.CALL],
                       key=lambda c: (c.expiry, abs(c.strike - chain.underlying.price)))
        if not calls:
            return
        c = calls[0]
        self.seen.append((self.time, str(c.symbol), round(c.bid_price, 9), round(c.ask_price, 9),
                          round(c.implied_volatility, 9), round(c.greeks.delta, 9)))
        if self.time.weekday() == 0 and not self.portfolio.invested:
            self.market_order(c.symbol, 1)
        elif self.time.weekday() == 4 and self.portfolio.invested:
            self.liquidate()


def _fills_upto(d, cut):
    return [(f['time'][:10], f['symbol'], f['quantity'], round(f['price'], 9)) for f in d['trades']
            if f['time'][:10] <= cut.strftime('%Y-%m-%d')]


def test_option_chain_quotes_and_decisions_ignore_bars_after_the_cut():
    base = Feed({'SPY': spy, '^VIX': vix, '^VIX9D': vix, '^VIX3M': vix, '^VIX6M': vix, '^IRX': irx})
    pert = Feed({'SPY': perturb(spy, 1), '^VIX': perturb(vix, 2), '^VIX9D': perturb(vix, 3),
                 '^VIX3M': perturb(vix, 4), '^VIX6M': perturb(vix, 5), '^IRX': perturb(irx, 6)})
    a = ok(run(OptionProbe, START, END, base, cash=1_000_000))
    seen_a = OptionProbe.last.seen
    b = ok(run(OptionProbe, START, END, pert, cash=1_000_000))
    seen_b = OptionProbe.last.seen
    cut_t = CUT + timedelta(hours=16)
    before_a = [s for s in seen_a if s[0] <= cut_t]
    before_b = [s for s in seen_b if s[0] <= cut_t]
    assert len(before_a) > 40
    assert before_a == before_b                              # chain, quotes, IV and greeks up to the cut
    assert [s for s in seen_a if s[0] > cut_t] != [s for s in seen_b if s[0] > cut_t]
    assert len(_fills_upto(a, CUT)) >= 4
    assert _fills_upto(a, CUT) == _fills_upto(b, CUT)
    assert [p['equity'] for p in a['equity'] if p['time'][:10] <= '2022-03-15'] == \
        [p['equity'] for p in b['equity'] if p['time'][:10] <= '2022-03-15']


# ---------------------------------------------------------------- universe
POOL = {'as_of': '2024-01-01', 'equities': ['AAA', 'BBB', 'CCC', 'DDD'], 'etfs': []}


def stock(phase, vol):
    def fn(d):
        x = (d - datetime(2021, 1, 1)).days
        c = 50 + 10 * math.sin(x / 11.0 + phase)
        return (c, c + 1, c - 1, c, vol * (1.5 + math.sin(x / 7.0 + 2 * phase)))
    return fn


STOCKS = {'AAA': stock(0.0, 1e6), 'BBB': stock(1.0, 1.2e6), 'CCC': stock(2.0, 0.9e6), 'DDD': stock(3.0, 1.1e6)}


class _UniFeed(Feed):
    """Feed with volumes: price functions return (o, h, l, c, volume)."""

    def bars(self, ticker, start, end_exclusive):
        spec = self.prices.get(ticker)
        if spec is None or ticker not in STOCKS and ticker not in ('SPY',):
            return super().bars(ticker, start, end_exclusive)
        rows = []
        for d in trading_days(start, end_exclusive - timedelta(days=1)):
            o, h, l, c, v = spec(d)
            rows.append({'time': d.strftime('%Y-%m-%d 00:00:00'), 'open': o, 'high': h, 'low': l,
                         'close': c, 'volume': v})
        return rows


def _pool_fetch(feed):
    def fetch(tickers, start, end):
        s = datetime.strptime(start[:10], '%Y-%m-%d')
        e = datetime.strptime(end[:10], '%Y-%m-%d')
        out = {}
        for t in tickers:
            rows = feed.bars(t, s, e)
            if rows:
                out[t] = {'bars': [[r['time'][:10], r['open'], r['high'], r['low'], r['close'], r['volume'],
                                    r['close']] for r in rows], 'splits': []}
        return out
    return fetch


def _perturb5(fn, seed):
    g = perturb(lambda d: fn(d)[:4], seed)
    rnd = random.Random(seed + 100)
    vol = {}

    def h(d):
        o, hi, lo, c = g(d)
        v = fn(d)[4]
        if d > CUT:
            v = vol.setdefault(d, v * rnd.uniform(0.2, 3.0))
        return (o, hi, lo, c, v)
    return h


class UniverseProbe(QCAlgorithm):
    provider_fetch = None
    tmp = None

    def initialize(self):
        self.set_start_date(2022, 1, 3)
        self.set_end_date(2022, 5, 31)
        self.set_cash(100000)
        self.universe_settings.resolution = Resolution.DAILY
        self._universe_manager.provider = UniverseDataProvider(
            fetch_bars=type(self).provider_fetch, fetch_info=lambda y: {}, fetch_holdings=lambda y: None,
            pool=POOL, today=datetime(2024, 6, 30), cache_dir=type(self).tmp, info_workers=1)
        self.add_universe(self.coarse)
        self.picks = []
        type(self).last = self

    def coarse(self, coarse):
        top = sorted(coarse, key=lambda c: c.dollar_volume, reverse=True)[:2]
        self.picks.append((self.time, sorted(str(c.symbol.value) for c in top)))
        return [c.symbol for c in top]

    def on_securities_changed(self, changes):
        for s in changes.removed_securities:
            if s.invested:
                self.liquidate(s.symbol)
        for s in changes.added_securities:
            self.set_holdings(s.symbol, 0.45)


def test_universe_selection_and_trades_ignore_bars_after_the_cut(tmp_path):
    def feed_of(prices):
        f = _UniFeed(prices)
        return f

    base_prices = dict(STOCKS)
    base_prices['SPY'] = lambda d: spy(d) + (1e6,)
    pert_prices = {k: _perturb5(v, i) for i, (k, v) in enumerate(STOCKS.items())}
    pert_prices['SPY'] = _perturb5(lambda d: spy(d) + (1e6,), 9)
    results = []
    for n, prices in enumerate((base_prices, pert_prices)):
        feed = feed_of(prices)
        UniverseProbe.provider_fetch = staticmethod(_pool_fetch(feed))
        UniverseProbe.tmp = str(tmp_path / f'u{n}')
        d = ok(run(UniverseProbe, START, END, feed, symbols=('SPY',)))
        results.append((d, list(UniverseProbe.last.picks)))
    (a, picks_a), (b, picks_b) = results
    cut_t = CUT + timedelta(hours=16)
    pa = [p for p in picks_a if p[0] <= cut_t]
    pb = [p for p in picks_b if p[0] <= cut_t]
    assert len(pa) > 40 and len({tuple(p[1]) for p in pa}) > 1   # the selection changes over time
    assert pa == pb
    assert [p for p in picks_a if p[0] > cut_t] != [p for p in picks_b if p[0] > cut_t]
    assert len(_fills_upto(a, CUT)) >= 4
    assert _fills_upto(a, CUT) == _fills_upto(b, CUT)
    assert a.get('uses_universe_selection') is True
