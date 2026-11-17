# Framework universe selection models (fincept_engine.framework.selection) driven by the
# UniverseManager on fixture data: fundamental / coarse-fine subclasses (LEAN Python and C#
# constructor forms), QC500, EMA cross, ETF constituents, scheduled, custom, inception-date and
# liquid-ETF models, manual lists, option/future placeholders. No network.
import json
import os
from datetime import datetime, timedelta

import pytest

from fincept_engine.enums import Resolution, SecurityType, Market
from fincept_engine.errors import NotSupportedByEngine
from fincept_engine.scheduling import DateRules, TimeRules
from fincept_engine.types import Symbol
from fincept_engine.universe import Universe, UniverseSettings, UniverseDefinitions
from fincept_engine.universe_data import UniverseDataProvider
from fincept_engine.universe_selection import UniverseManager
from fincept_engine.framework.selection import (
    FundamentalUniverseSelectionModel, CoarseFundamentalUniverseSelectionModel, FineFundamentalUniverseSelectionModel,
    QC500UniverseSelectionModel, ETFConstituentsUniverseSelectionModel, EmaCrossUniverseSelectionModel,
    ScheduledUniverseSelectionModel, CustomUniverseSelectionModel, ManualUniverseSelectionModel,
    InceptionDateUniverseSelectionModel, LiquidETFUniverse, EnergyETFUniverse, OptionUniverseSelectionModel,
    FutureUniverseSelectionModel)
from fincept_engine.algorithm import UniverseSelectionModel

FIX = os.path.join(os.path.dirname(__file__), 'fixtures')
POOL = {'as_of': '2024-01-01', 'equities': ['AAA', 'BBB', 'CCC', 'DDD'], 'etfs': ['SPY']}


def load_bars():
    with open(os.path.join(FIX, 'universe_bars.json'), encoding='utf-8') as f:
        d = json.load(f)
    d.pop('_note', None)
    return d


def load_info():
    with open(os.path.join(FIX, 'universe_info_AAPL.json'), encoding='utf-8') as f:
        return json.load(f)['info']


class FakeSecurity:
    def __init__(self, symbol):
        self.symbol = symbol


class FakeAlgo:
    def __init__(self, start=datetime(2024, 1, 2), end=datetime(2024, 3, 28)):
        self.time = start
        self._start_date, self._end_date = start, end
        self.universe_settings = UniverseSettings()
        self.securities = {}
        self.universe = UniverseDefinitions(self)


def make(tmp_path, store=None, infos=None, no_data=()):
    algo = FakeAlgo()
    store = store or load_bars()

    def fetch(tickers, start, end):
        return {t: {'bars': [b for b in store[t]['bars'] if start <= b[0][:10] < end],
                    'splits': [s for s in store[t]['splits'] if start <= s[0] < end]} for t in tickers if t in store}

    provider = UniverseDataProvider(fetch_bars=fetch, fetch_info=lambda y: dict((infos or {}).get(y, {})),
                                    fetch_holdings=lambda y: None, pool=POOL, today=datetime(2024, 6, 30),
                                    cache_dir=str(tmp_path / 'cache'), info_workers=1)

    def subscribe(symbol, settings):
        if str(symbol) in no_data:
            return None
        return algo.securities.setdefault(str(symbol), FakeSecurity(symbol))

    return algo, UniverseManager(algo, provider, subscribe, lambda s: False)


def days(start, end):
    out, d = [], start
    while d <= end:
        if d.weekday() < 5 and d not in (datetime(2024, 1, 15), datetime(2024, 2, 19)):
            out.append(d)
        d += timedelta(days=1)
    return out


def tickers(symbols):
    return [str(s) for s in symbols]


def test_models_are_universe_selection_models():
    for cls in (FundamentalUniverseSelectionModel, QC500UniverseSelectionModel, ETFConstituentsUniverseSelectionModel,
                EmaCrossUniverseSelectionModel, CustomUniverseSelectionModel, ManualUniverseSelectionModel,
                LiquidETFUniverse, OptionUniverseSelectionModel):
        assert issubclass(cls, UniverseSelectionModel)


def test_fundamental_model_subclass_select(tmp_path):
    algo, m = make(tmp_path)

    class Top(FundamentalUniverseSelectionModel):
        def __init__(self, universe_settings=None):
            super().__init__(universe_settings)          # LEAN Python form: fundamental mode

        def select(self, algorithm, fundamental):
            eq = [f for f in fundamental if f.has_fundamental_data]
            return [f.symbol for f in sorted(eq, key=lambda f: f.dollar_volume, reverse=True)[:1]]

    m.set_universe_selection(Top())
    assert tickers(m.select(datetime(2024, 1, 2))[0]) == ['AAA']


def test_fundamental_model_with_one_argument_select(tmp_path):
    algo, m = make(tmp_path)

    class Model(FundamentalUniverseSelectionModel):
        def __init__(self):
            super().__init__(self.select)                 # C# (selector) form, select(fundamental)

        def select(self, fundamental):
            return [f.symbol for f in fundamental if f.symbol.value == 'CCC']

    m.add_universe_selection(Model())
    assert tickers(m.select(datetime(2024, 1, 2))[0]) == ['CCC']


def test_coarse_fine_subclass_and_function_models(tmp_path):
    algo, m = make(tmp_path, infos={'AAA': load_info()})
    seen = {}

    class CoarseFine(FundamentalUniverseSelectionModel):
        def __init__(self):
            super().__init__(True, None)

        def select_coarse(self, algorithm, coarse):
            seen['coarse'] = sorted(c.symbol.value for c in coarse)
            return [c.symbol for c in coarse]

        def select_fine(self, algorithm, fine):
            return [f.symbol for f in fine if f.market_cap > 0]

    m.add_universe_selection(CoarseFine())
    a, _ = m.select(datetime(2024, 1, 2))
    assert seen['coarse'] == ['AAA', 'BBB', 'CCC']        # fine filtering drops securities without fundamentals
    assert tickers(a) == ['AAA']                          # only AAA has a snapshot (market cap)

    algo2, m2 = make(tmp_path)
    m2.add_universe_selection(FineFundamentalUniverseSelectionModel(
        lambda c: [x.symbol for x in c if x.symbol.value in ('BBB', 'SPY')], lambda f: [x.symbol for x in f], None))
    assert tickers(m2.select(datetime(2024, 1, 2))[0]) == ['BBB']
    algo3, m3 = make(tmp_path)
    m3.add_universe_selection(CoarseFundamentalUniverseSelectionModel(lambda c: [x.symbol for x in c][:2]))
    assert tickers(m3.select(datetime(2024, 1, 2))[0]) == ['AAA', 'BBB']


def test_coarse_fine_model_with_async_settings_is_refused(tmp_path):
    algo, m = make(tmp_path)
    algo.universe_settings.asynchronous = True
    m.set_universe_selection(FineFundamentalUniverseSelectionModel(lambda c: [], lambda f: []))
    with pytest.raises(ValueError):
        m.select(datetime(2024, 1, 2))


def test_qc500_monthly_with_fine_criteria(tmp_path):
    algo, m = make(tmp_path, infos={'AAA': load_info(), 'BBB': {'quoteType': 'EQUITY', 'country': 'Germany',
                                                                'exchange': 'NYQ', 'marketCap': 9e10,
                                                                'currentPrice': 50.0, 'sector': 'Industrials'}})
    m.add_universe_selection(QC500UniverseSelectionModel())
    changes = {}
    for d in days(datetime(2024, 1, 2), datetime(2024, 2, 29)):
        a, r = m.select(d)
        if a or r:
            changes[d] = (tickers(a), tickers(r))
    # only AAA is a US company on NYSE/NASDAQ listed > 180 days with market cap > $500M
    assert changes == {datetime(2024, 1, 2): (['AAA'], [])}


def test_ema_cross_model_updates_with_daily_adjusted_prices(tmp_path):
    store = load_bars()
    for i, b in enumerate(store['CCC']['bars']):        # CCC trends up 3% a day
        p = 20.0 * (1.03 ** i)
        b[1:5] = [p, p, p, p]
        b[6] = p
    algo, m = make(tmp_path, store=store)
    model = EmaCrossUniverseSelectionModel(2, 5, 1)
    m.add_universe_selection(model)
    first = None
    for d in days(datetime(2024, 1, 2), datetime(2024, 1, 31)):
        a, _ = m.select(d)
        if a:
            first = (d, tickers(a))
            break
    # the slow EMA (5) needs 5 selections before anything can be chosen (no warm-up, as in LEAN)
    assert first == (datetime(2024, 1, 8), ['CCC'])
    assert model.averages[Symbol('CCC')].slow_ema.samples == 5


def test_etf_constituents_model_static_holdings(tmp_path):
    algo, m = make(tmp_path)
    m.add_universe_selection(ETFConstituentsUniverseSelectionModel(
        "SPY", algo.universe_settings, lambda cs: [c.symbol for c in sorted(cs, key=lambda c: c.weight)[-2:]]))
    a, _ = m.select(datetime(2024, 1, 2))
    assert sorted(tickers(a)) == ['AAPL', 'NVDA']


def test_scheduled_model_and_custom_model(tmp_path):
    algo, m = make(tmp_path)
    m.add_universe_selection(ScheduledUniverseSelectionModel(
        DateRules(algo).every(0, 1, 3), TimeRules(algo).every(timedelta(hours=12)),
        lambda dt: ['SPY'] if dt.weekday() in (0, 1) else ['IBM']))

    class MyCustom(CustomUniverseSelectionModel):
        def __init__(self):
            super().__init__("my-custom", None)

        def select(self, algorithm, date):
            return ["CCC"] if date.day % 7 == 0 else []

    m.add_universe_selection(MyCustom())
    log = {}
    for d in days(datetime(2024, 1, 2), datetime(2024, 1, 12)):
        a, r = m.select(d)
        log[d] = (sorted(tickers(a)), sorted(tickers(r)))
    assert log[datetime(2024, 1, 2)] == (['SPY'], [])          # Tuesday
    assert log[datetime(2024, 1, 3)] == ([], [])               # Wednesday: the scheduled model does not run
    assert log[datetime(2024, 1, 4)] == (['IBM'], ['SPY'])     # Thursday
    assert log[datetime(2024, 1, 8)] == (['SPY'], ['IBM'])     # Monday (the custom model's Jan 7 is a Sunday)
    assert log[datetime(2024, 1, 5)] == ([], [])


def test_inception_date_model(tmp_path):
    algo, m = make(tmp_path)
    m.add_universe_selection(InceptionDateUniverseSelectionModel(
        "spy-inception", {"SPY": datetime(2024, 1, 5), "AAA": datetime(2024, 1, 9)}))
    log = {d: tickers(m.select(d)[0]) for d in days(datetime(2024, 1, 2), datetime(2024, 1, 12))}
    assert log[datetime(2024, 1, 4)] == [] and log[datetime(2024, 1, 5)] == ['SPY']
    assert log[datetime(2024, 1, 9)] == ['AAA'] and log[datetime(2024, 1, 10)] == []


def test_liquid_etf_universe(tmp_path):
    algo, m = make(tmp_path, no_data=('TVIX', 'UGAZ', 'DGAZ', 'USLV', 'UGLD'))
    assert tickers(LiquidETFUniverse.ENERGY.inverse) == ['ERY', 'SCO', 'DGAZ']
    assert 'XLK' in LiquidETFUniverse.SP500_SECTORS and len(LiquidETFUniverse.SP_500_SECTORS) == 9
    assert str(LiquidETFUniverse.METALS).startswith('12 symbols: Long: GLD')
    m.set_universe_selection(LiquidETFUniverse())
    a, _ = m.select(datetime(2024, 1, 2))
    assert len(a) == 9 + 16 + 12 + 11 + 16 + 8 - 5            # every basket ETF incepted before 2024 ...
    assert any('TVIX' in n for n in m.notes())                 # ... except the delisted ones (noted)
    e = EnergyETFUniverse()
    assert isinstance(e, InceptionDateUniverseSelectionModel)


def test_manual_model_keeps_symbols_list():
    spy = Symbol.create('SPY', SecurityType.EQUITY, Market.USA)
    model = ManualUniverseSelectionModel([spy, 'AAPL'])
    assert model.symbols_list() == [spy, 'AAPL']
    algo = FakeAlgo()
    u = model.create_universes(algo)[0]
    assert tickers(u.select_symbols(algo.time)) == ['SPY', 'AAPL']


def test_option_and_future_chain_models_select_canonical_symbols(tmp_path):
    # The algorithm subscribes their chains (QCAlgorithm._derivative_universe_model); in the universe
    # manager they add no equity universe.
    algo, m = make(tmp_path)
    es = Symbol.create('ES', SecurityType.FUTURE, Market.CME)
    fut = FutureUniverseSelectionModel(timedelta(days=1), lambda dt: [es])
    opt = OptionUniverseSelectionModel(lambda dt: [])                   # (selector) form: daily refresh
    assert fut.select_chain_symbols(datetime(2024, 1, 2)) == [es]
    assert fut.refresh_interval == timedelta(days=1) and opt.refresh_interval == timedelta(days=1)
    assert fut.filter('the universe') == 'the universe'                 # LEAN default: unchanged
    m.add_universe_selection(fut)
    assert m.select(datetime(2024, 1, 2)) == ([], [])
