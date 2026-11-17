# UniverseManager: LEAN add_universe overloads, daily selection, add/remove sequencing,
# Universe.UNCHANGED, schedules, minimum time in universe, pending removal of held
# securities, coarse + fine, ETF and user-defined universes, the universe_manager view.
# No network (fixture bars, saved info snapshot, static ETF holdings table).
import json
import math
import os
from datetime import datetime, timedelta

import pytest

from fincept_engine.enums import Resolution, SecurityType, Market
from fincept_engine.errors import NotSupportedByEngine
from fincept_engine.scheduling import DateRules, TimeRules
from fincept_engine.types import Symbol
from fincept_engine.universe import (Universe, UniverseSettings, FundamentalUniverse, ScheduledUniverse,
                                     ConstituentsUniverse, UniverseDefinitions, ETFConstituentData, CoarseFundamental)
from fincept_engine.universe_data import UniverseDataProvider
from fincept_engine.universe_selection import UniverseManager, SCHEDULE_NOTE

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


def fake_fetch(store):
    def fetch(tickers, start, end):
        return {t: {'bars': [b for b in store[t]['bars'] if start <= b[0][:10] < end],
                    'splits': [s for s in store[t]['splits'] if start <= s[0] < end]}
                for t in tickers if t in store}
    return fetch


class FakeSecurity:
    def __init__(self, symbol):
        self.symbol = symbol


class FakeAlgo:
    def __init__(self, start=datetime(2024, 1, 2), end=datetime(2024, 3, 28)):
        self.time = start
        self._start_date, self._end_date = start, end
        self.universe_settings = UniverseSettings()
        self.universe_settings.resolution = Resolution.DAILY
        self.securities = {}
        self.held = set()
        self.subscribed = []
        self.universe = UniverseDefinitions(self)


def make(tmp_path, infos=None, no_data=(), algo=None):
    algo = algo or FakeAlgo()
    calls = []

    def fetch_info(y):
        calls.append(y)
        return dict((infos if infos is not None else {'AAA': load_info()}).get(y, {}))

    provider = UniverseDataProvider(fetch_bars=fake_fetch(load_bars()), fetch_info=fetch_info,
                                    fetch_holdings=lambda y: None, pool=POOL, today=datetime(2024, 6, 30),
                                    cache_dir=str(tmp_path / 'cache'), info_workers=1)

    def subscribe(symbol, settings):
        k = str(symbol)
        if k in no_data:
            return None
        sec = algo.securities.get(k)
        if sec is None:
            sec = algo.securities[k] = FakeSecurity(symbol)
            algo.subscribed.append((k, settings))
        return sec

    m = UniverseManager(algo, provider, subscribe, lambda s: str(s) in algo.held)
    return algo, m, calls


def days(start, end):
    out, d = [], start
    while d <= end:
        if d.weekday() < 5 and d not in (datetime(2024, 1, 15), datetime(2024, 2, 19)):
            out.append(d)
        d += timedelta(days=1)
    return out


def run(m, start, end):
    """{day: (added tickers, removed tickers)} for the days with changes."""
    out = {}
    for d in days(start, end):
        a, r = m.select(d)
        if a or r:
            out[d] = ([str(s) for s in a], [str(s) for s in r])
    return out


def top_dollar_volume(n):
    def select(coarse):
        eq = [c for c in coarse if c.has_fundamental_data]
        return [c.symbol for c in sorted(eq, key=lambda c: c.dollar_volume, reverse=True)[:n]]
    return select


def test_coarse_universe_adds_and_removes_on_real_ranking(tmp_path):
    algo, m, _ = make(tmp_path)
    seen = []

    def select(coarse):
        seen.append(algo.time)
        return top_dollar_volume(1)(coarse)

    u = m.add_universe(select)
    assert isinstance(u, Universe) and u.universe_settings is algo.universe_settings
    changes = run(m, datetime(2024, 1, 2), datetime(2024, 2, 29))
    # BBB's volume doubles from the 2024-02-01 session: it leads from the 2024-02-02 selection on
    assert changes == {datetime(2024, 1, 2): (['AAA'], []), datetime(2024, 2, 2): (['BBB'], ['AAA'])}
    assert seen[0] == datetime(2024, 1, 2)          # the clock is at 00:00 of the selection day
    assert [str(s) for s in u.selected] == ['BBB'] and list(map(str, u.members)) == ['BBB']
    assert u.selected.count == 1 and u.members.count == 1


def test_warm_up_days_select_too(tmp_path):
    algo, m, _ = make(tmp_path, algo=FakeAlgo(start=datetime(2024, 1, 10)))
    m.add_universe(top_dollar_volume(1))
    a, r = m.select(datetime(2024, 1, 3))           # a warm-up day before the start date
    assert [str(s) for s in a] == ['AAA']


def test_unchanged_keeps_selection_and_skips_fine(tmp_path):
    algo, m, _ = make(tmp_path)
    fine_calls = []

    def coarse(c):
        if algo.time.date() <= datetime(2024, 1, 3).date():
            return [Symbol.create(t, SecurityType.EQUITY, Market.USA) for t in ('AAA', 'BBB', 'CCC')]
        return Universe.UNCHANGED

    def fine(f):
        fine_calls.append(algo.time)
        if algo.time.date() == datetime(2024, 1, 2).date():
            return [x.symbol for x in f if x.symbol.value in ('AAA', 'BBB')]
        return Universe.UNCHANGED

    m.add_universe(coarse, fine)
    changes = run(m, datetime(2024, 1, 2), datetime(2024, 1, 31))
    assert changes == {datetime(2024, 1, 2): (['AAA', 'BBB'], [])}
    assert fine_calls == [datetime(2024, 1, 2), datetime(2024, 1, 3)]   # skipped once coarse is UNCHANGED


def test_coarse_fine_fine_sees_only_selected_with_fundamentals(tmp_path):
    algo, m, calls = make(tmp_path)
    got = {}

    def coarse(c):
        return [x.symbol for x in c if x.symbol.value in ('AAA', 'CCC', 'SPY')]

    def fine(f):
        got['fine'] = sorted(x.symbol.value for x in f)
        ranked = sorted(f, key=lambda x: (not math.isnan(x.valuation_ratios.pe_ratio)), reverse=True)
        return [ranked[0].symbol]

    m.add_universe(coarse, fine)
    a, _ = m.select(datetime(2024, 1, 2))
    assert got['fine'] == ['AAA', 'CCC']          # SPY has no fundamental data: not passed to fine
    assert [str(s) for s in a] == ['AAA']
    assert sorted(calls) == ['AAA', 'CCC']          # one batch, only what the fine step touched


def test_async_coarse_fine_is_refused(tmp_path):
    algo, m, _ = make(tmp_path)
    algo.universe_settings.asynchronous = True
    with pytest.raises(ValueError):
        m.add_universe(lambda c: [], lambda f: [])
    m.add_universe(lambda f: [])                    # single-pass fundamental selection is fine


def test_identical_selection_reports_nothing(tmp_path):
    algo, m, _ = make(tmp_path)
    m.add_universe(lambda c: [x.symbol for x in c if x.symbol.value == 'CCC'])
    changes = run(m, datetime(2024, 1, 2), datetime(2024, 1, 31))
    assert changes == {datetime(2024, 1, 2): (['CCC'], [])}


def _alternating(algo, first_day, first, then):
    """Selection: `first` on first_day, then `then` plus CCC on every other day (so the
    selection keeps changing and LEAN re-evaluates removals)."""
    flip = {'n': 0}

    def select(t):
        if t.date() == first_day.date():
            return list(first)
        flip['n'] += 1
        return list(then) + (['CCC'] if flip['n'] % 2 else [])
    return select


def test_minimum_time_in_universe(tmp_path):
    algo, m, _ = make(tmp_path)
    settings = UniverseSettings(algo.universe_settings)
    settings.minimum_time_in_universe = timedelta(days=5)
    u = m.add_universe(SecurityType.EQUITY, 'min-time', Resolution.DAILY, Market.USA, settings,
                       _alternating(algo, datetime(2024, 1, 2), ['AAA'], ['BBB']))
    removed_on = None
    for d in days(datetime(2024, 1, 2), datetime(2024, 1, 12)):
        a, r = m.select(d)
        if 'AAA' in [str(s) for s in r]:
            removed_on = d
            break
        if d < datetime(2024, 1, 8):
            assert 'AAA' in [str(s) for s in u.members]
    assert removed_on == datetime(2024, 1, 8)      # added Jan 2; 5 days later is Sunday Jan 7


def test_default_minimum_time_is_one_day(tmp_path):
    algo, m, _ = make(tmp_path)
    assert algo.universe_settings.minimum_time_in_universe == timedelta(days=1)
    m.add_universe('one-day', _alternating(algo, datetime(2024, 1, 2), ['AAA'], ['BBB']))
    m.select(datetime(2024, 1, 2))
    a, r = m.select(datetime(2024, 1, 3))
    assert [str(s) for s in r] == ['AAA']


def test_held_security_is_pending_removal_until_flat(tmp_path):
    algo, m, _ = make(tmp_path)
    plan = {datetime(2024, 1, 2): ['AAA'], datetime(2024, 1, 3): ['BBB']}
    u = m.add_universe('held', lambda t: plan.get(datetime(t.year, t.month, t.day), ['BBB', 'CCC']))
    m.select(datetime(2024, 1, 2))
    algo.held.add('AAA')
    a, r = m.select(datetime(2024, 1, 3))
    assert [str(s) for s in r] == ['AAA'] and [str(s) for s in a] == ['BBB']   # reported removed once...
    assert m.is_member('AAA') and 'AAA' in u.members                              # ...but kept while held
    a, r = m.select(datetime(2024, 1, 4))
    assert r == [] and [str(s) for s in a] == ['CCC'] and m.is_member('AAA')      # not reported again
    algo.held.discard('AAA')
    a, r = m.select(datetime(2024, 1, 5))           # same selection (UNCHANGED): pending removals still checked
    assert a == [] and r == [] and not m.is_member('AAA')                         # flat: dropped silently


def test_reselected_pending_member_is_kept(tmp_path):
    algo, m, _ = make(tmp_path)
    plan = {datetime(2024, 1, 2): ['AAA'], datetime(2024, 1, 3): ['BBB'], datetime(2024, 1, 4): ['AAA', 'BBB']}
    m.add_universe('again', lambda t: plan.get(datetime(t.year, t.month, t.day), ['AAA', 'BBB']))
    m.select(datetime(2024, 1, 2))
    algo.held.add('AAA')
    m.select(datetime(2024, 1, 3))
    a, r = m.select(datetime(2024, 1, 4))           # re-selected while still held
    assert a == [] and r == [] and m.is_member('AAA')     # the pending removal is cancelled, no new event
    algo.held.discard('AAA')
    a, r = m.select(datetime(2024, 1, 5))
    assert a == [] and r == [] and m.is_member('AAA')     # a selected member stays when it goes flat


def test_schedule_month_start_selects_initially_and_on_rule(tmp_path):
    algo, m, _ = make(tmp_path, algo=FakeAlgo(start=datetime(2024, 1, 10)))
    when = []
    m.add_universe(DateRules(algo).month_start(), lambda f: when.append(algo.time) or [f[0].symbol])
    for d in days(datetime(2024, 1, 10), datetime(2024, 3, 28)):
        m.select(d)
    assert when == [datetime(2024, 1, 10), datetime(2024, 2, 1), datetime(2024, 3, 1)]


def test_schedule_on_a_date_has_no_initial_selection(tmp_path):
    algo, m, _ = make(tmp_path, algo=FakeAlgo(start=datetime(2024, 1, 10)))
    when = []
    settings = UniverseSettings(algo.universe_settings)
    settings.schedule.on(DateRules(algo).on(datetime(2024, 2, 15)))
    m.add_universe(FundamentalUniverse.usa(lambda f: when.append(algo.time) or [], settings))
    algo.universe_settings.schedule.on(DateRules(algo).month_end())
    m.add_universe(FundamentalUniverse.usa(lambda f: when.append(('end', algo.time)) or []))
    for d in days(datetime(2024, 1, 10), datetime(2024, 2, 29)):
        m.select(d)
    assert datetime(2024, 2, 15) in when and len([w for w in when if not isinstance(w, tuple)]) == 1
    assert [w[1] for w in when if isinstance(w, tuple)] == [datetime(2024, 1, 10), datetime(2024, 1, 31),
                                                            datetime(2024, 2, 29)]


def test_scheduled_universe_time_of_day(tmp_path):
    algo, m, _ = make(tmp_path)
    at1, at18 = [], []
    m.add_universe(ScheduledUniverse(DateRules(algo).every_day(), TimeRules(algo).at(1, 0),
                                     lambda t: at1.append((t, algo.time)) or ['SPY']))
    m.add_universe(ScheduledUniverse('UTC', DateRules(algo).every_day(), TimeRules(algo).at(18, 0),
                                     lambda t: at18.append(t) or ['CCC']))
    changes = run(m, datetime(2024, 1, 2), datetime(2024, 1, 4))
    assert at1[0] == (datetime(2024, 1, 2, 1, 0), datetime(2024, 1, 2, 1, 0))
    # an after-close selection runs before the next open, never before the first day
    assert at18 == [datetime(2024, 1, 2, 18, 0), datetime(2024, 1, 3, 18, 0)]
    assert changes[datetime(2024, 1, 2)] == (['SPY'], []) and changes[datetime(2024, 1, 3)] == (['CCC'], [])


def test_user_defined_universe_and_collapse_note(tmp_path):
    algo, m, _ = make(tmp_path)
    times = []
    u = m.add_universe('my_universe_name', Resolution.HOUR, lambda t: times.append(t) or ['SPY', 'IBM'])
    a, _ = m.select(datetime(2024, 1, 2))
    assert [str(s) for s in a] == ['SPY', 'IBM'] and times == [datetime(2024, 1, 2)]
    assert all(s.security_type == SecurityType.EQUITY for s in a)
    assert SCHEDULE_NOTE in m.notes()
    assert str(u.symbol) == 'MY_UNIVERSE_NAME'


def test_selection_results_accept_tickers_ids_and_generators(tmp_path):
    algo, m, _ = make(tmp_path)
    sym = Symbol.create('BBB', SecurityType.EQUITY, Market.USA)
    m.add_universe(lambda f: (x for x in ['AAA', sym, sym.id, 'AAA']))
    a, _ = m.select(datetime(2024, 1, 2))
    assert [str(s) for s in a] == ['AAA', 'BBB']
    algo2, m2, _ = make(tmp_path)
    m2.add_universe(lambda f: None)
    with pytest.raises(TypeError):
        m2.select(datetime(2024, 1, 2))


def test_symbol_without_data_is_skipped_and_noted(tmp_path):
    algo, m, _ = make(tmp_path, no_data=('ZZZZ',))
    m.add_universe('x', lambda t: ['ZZZZ', 'AAA'])
    a, _ = m.select(datetime(2024, 1, 2))
    assert [str(s) for s in a] == ['AAA']
    assert any('ZZZZ' in n for n in m.notes())


def test_two_universes_sharing_a_symbol(tmp_path):
    algo, m, _ = make(tmp_path)
    plan_b = {datetime(2024, 1, 2): ['AAA', 'BBB'], datetime(2024, 1, 3): ['BBB']}
    m.add_universe('a', lambda t: ['AAA'] if t < datetime(2024, 1, 4) else ['CCC'])
    m.add_universe('b', lambda t: plan_b.get(t, ['BBB', 'CCC']))
    assert m.select(datetime(2024, 1, 2)) == ([Symbol('AAA'), Symbol('BBB')], [])
    # b drops AAA while a still holds it: LEAN reports the removal anyway (per universe; see
    # UniverseSelection.ApplyUniverseSelection) and the security keeps its data through a
    a, r = m.select(datetime(2024, 1, 3))
    assert a == [] and [str(s) for s in r] == ['AAA']
    a, r = m.select(datetime(2024, 1, 4))          # a drops AAA too; CCC joins both, reported once
    assert [str(s) for s in a] == ['CCC'] and [str(s) for s in r] == ['AAA']


def test_etf_universe_with_filter(tmp_path):
    algo, m, _ = make(tmp_path)
    got = {}

    def filt(constituents):
        got['data'] = list(constituents)
        return [c.symbol for c in sorted(constituents, key=lambda c: c.weight, reverse=True)[:3]]

    u = m.add_universe(algo.universe.etf('SPY', algo.universe_settings, filt))
    a, _ = m.select(datetime(2024, 1, 2))
    assert len(a) == 3 and all(isinstance(c, ETFConstituentData) for c in got['data'])
    assert m.constituent_data('AAPL').weight > 0
    assert any('current top holdings' in n for n in m.notes())
    assert m.select(datetime(2024, 1, 3)) == ([], [])          # same holdings: unchanged


def test_universe_definitions_top_and_qc500(tmp_path):
    algo, m, _ = make(tmp_path)
    m.add_universe(algo.universe.top(2))
    a, _ = m.select(datetime(2024, 1, 2))
    assert [str(s) for s in a] == ['SPY', 'AAA']               # top dollar volume, ETFs included (LEAN)
    algo2, m2, _ = make(tmp_path)
    m2.add_universe(algo2.universe.dollar_volume.top(1))
    assert [str(s) for s in m2.select(datetime(2024, 1, 2))[0]] == ['SPY']
    assert algo.universe.qc_500.etf_symbol.value == 'SPY'
    assert algo.universe.unchanged is Universe.UNCHANGED


def test_manual_universe_subscribed_up_front_and_view(tmp_path):
    from fincept_engine.framework.selection import ManualUniverseSelectionModel
    algo, m, _ = make(tmp_path)
    spy = Symbol.create('SPY', SecurityType.EQUITY, Market.USA)
    m.set_universe_selection(ManualUniverseSelectionModel([spy]))
    m.add_universe_selection(ManualUniverseSelectionModel(Symbol.create('AAPL', SecurityType.EQUITY, Market.USA)))
    m.add_universe_selection(ManualUniverseSelectionModel(spy, Symbol.create('FB', SecurityType.EQUITY, Market.USA)))
    assert [k for k, _ in algo.subscribed] == ['SPY', 'AAPL', 'FB']        # before any selection
    assert m.select(datetime(2024, 1, 2)) == ([], [])                       # the runner reports them at start
    view = m.view()
    assert view.count == 3 and len(view) == 3
    assert view.active_securities.count == 3
    u = view.values()[0]
    assert view[u.symbol] is u and u.symbol in view


def test_custom_data_universes_are_refused(tmp_path):
    algo, m, _ = make(tmp_path)

    class MyData:
        def reader(self, *a):
            return None

    with pytest.raises(NotSupportedByEngine):
        m.add_universe(MyData, 'name', lambda d: [])
    with pytest.raises(NotSupportedByEngine):
        m.add_universe(ConstituentsUniverse(Symbol('X'), algo.universe_settings), lambda f: [])
    m.add_universe(CoarseFundamental)                       # LEAN: no selector = select everything
    a, _ = m.select(datetime(2024, 1, 2))
    assert sorted(str(s) for s in a) == ['AAA', 'BBB', 'CCC', 'SPY']


def test_fundamentals_api(tmp_path):
    algo, m, _ = make(tmp_path)
    algo.time = datetime(2024, 1, 3)
    f = m.fundamentals(Symbol.create('AAA', SecurityType.EQUITY, Market.USA))
    assert f.end_time == datetime(2024, 1, 3) and f.price > 0
    fs = m.fundamentals(['AAA', 'BBB'])
    assert [x.symbol.value for x in fs] == ['AAA', 'BBB']
