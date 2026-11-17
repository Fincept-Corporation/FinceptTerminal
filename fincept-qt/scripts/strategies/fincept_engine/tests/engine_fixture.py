# Offline backtests for the engine unit tests: the real runner, session, broker and extensions,
# fed with small synthetic daily bars instead of Yahoo (no network, no disk cache).
import os
import sys
from datetime import datetime, timedelta

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.abspath(os.path.join(_HERE, '..', '..', '..'))
for _p in (os.path.join(_SCRIPTS, 'strategies'), os.path.join(_SCRIPTS, 'Analytics', 'backtesting', 'base')):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from fincept_engine import market_calendar as mc  # noqa: E402

VIX_TICKERS = ('^VIX9D', '^VIX', '^VIX3M', '^VIX6M')


def trading_days(start, end):
    return [datetime(d.year, d.month, d.day) for d in mc.trading_days(start, end)]


def bar(o, h=None, l=None, c=None):
    """(open, high, low, close) with defaults: a flat bar at `o`."""
    c = o if c is None else c
    h = max(o, c) if h is None else h
    l = min(o, c) if l is None else l
    return (float(o), float(h), float(l), float(c))


class Feed:
    """Synthetic Yahoo: {yahoo ticker: price | fn(datetime) -> price or (o, h, l, c)}. The CBOE
    volatility indexes default to a flat `vix` and ^IRX to a flat `irx` (percent)."""

    def __init__(self, prices, vix=20.0, irx=0.0):
        self.prices = dict(prices)
        for t in VIX_TICKERS:
            self.prices.setdefault(t, vix)
        self.prices.setdefault('^IRX', irx)
        self.requested = []

    def bars(self, ticker, start, end_exclusive):
        spec = self.prices.get(ticker)
        if spec is None:
            return None
        rows = []
        for d in trading_days(start, end_exclusive - timedelta(days=1)):
            v = spec(d) if callable(spec) else spec
            o, h, l, c = bar(v) if isinstance(v, (int, float)) else bar(*v)
            rows.append({'time': d.strftime('%Y-%m-%d 00:00:00'), 'open': o, 'high': h, 'low': l,
                         'close': c, 'volume': 1_000_000.0})
        return rows

    def __call__(self, tickers, start, end, mode='adj'):
        s = datetime.strptime(str(start)[:10], '%Y-%m-%d')
        e = datetime.strptime(str(end)[:10], '%Y-%m-%d')
        out = {}
        for t in tickers:
            self.requested.append(t)
            rows = self.bars(t, s, e)
            if rows is None:
                continue
            if mode == 'raw':
                for r in rows:
                    r['adj_close'] = r['close']
            out[t] = rows
        return out


def run(cls, start, end, feed, cash=100_000.0, symbols=('SPY',), commission=0.0, slippage=0.0):
    """Run a QCAlgorithm subclass through the real runner on synthetic bars. Returns the runner's
    result dict ({'success': ..., 'data': ...} or the error envelope)."""
    import fincept_strategy_runner as R

    class _Runner(R.FinceptStrategyRunner):
        def load_strategy_class(self, strategy_id):
            return cls

        def get_strategy_info(self, strategy_id):
            return {'name': cls.__name__, 'path': f'{cls.__name__}.py', 'category': 'test'}

        def fetch_historical_data(self, symbols, start_date, end_date, resolution='daily'):
            return feed(symbols, start_date, end_date, 'adj')

        def fetch_split_adjusted(self, symbols, start_date, end_date):
            return feed(symbols, start_date, end_date, 'raw')

        def fetch_actions(self, yahoo_ticker, start=None, end=None):
            return {'dividends': [], 'splits': []}

    return _Runner().execute_strategy('TEST', {
        'symbols': list(symbols), 'start_date': start, 'end_date': end, 'initial_cash': cash,
        'commission': commission, 'slippage': slippage})


def ok(result):
    assert result.get('success'), (result.get('error'), (result.get('traceback') or '')[-2000:])
    return result['data']
