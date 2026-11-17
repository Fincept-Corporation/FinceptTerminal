# Universe data provider: coarse values as of a day (no look-ahead), raw price rebuild, the
# fundamentals snapshot mapping and missing values, caching, ETF holdings, disclosures.
# No network: bars come from tests/fixtures/universe_bars.json, fundamentals from
# tests/fixtures/universe_info_AAPL.json, ETF holdings from the static table.
import copy
import json
import math
import os
from datetime import datetime, timedelta

import pytest

from fincept_engine.errors import NotSupportedByEngine
from fincept_engine.universe import Fundamental, MorningstarSectorCode, MorningstarIndustryCode, NO_DATE
from fincept_engine.universe_data import (UniverseDataProvider, FUNDAMENTAL_DISCLOSURE, map_fundamentals,
                                          classify, build_series, lean_name)

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


def fake_fetch(store, calls=None):
    def fetch(tickers, start, end):
        if calls is not None:
            calls.append((tuple(tickers), start, end))
        out = {}
        for t in tickers:
            e = store.get(t)
            if e is None:
                continue
            out[t] = {'bars': [b for b in e['bars'] if start <= b[0][:10] < end],
                      'splits': [s for s in e['splits'] if start <= s[0] < end], 'dividends': []}
        return out
    return fetch


def make_provider(tmp_path, store=None, infos=None, holdings=None, **kw):
    calls = {'info': [], 'bars': []}
    infos = infos if infos is not None else {'AAA': load_info()}

    def fetch_info(y):
        calls['info'].append(y)
        return dict(infos.get(y, {}))

    def fetch_holdings(y):
        return (holdings or {}).get(y)

    p = UniverseDataProvider(fetch_bars=fake_fetch(store or load_bars(), calls['bars']), fetch_info=fetch_info,
                             fetch_holdings=fetch_holdings, pool=POOL, today=datetime(2024, 6, 30),
                             cache_dir=str(tmp_path / 'cache'), info_workers=1, **kw)
    return p, calls


def by_ticker(coarse):
    return {f.symbol.value: f for f in coarse}


def test_coarse_uses_previous_session_and_raw_prices(tmp_path):
    p, _ = make_provider(tmp_path)
    day = datetime(2024, 1, 3)
    c = by_ticker(p.coarse(day))
    assert set(c) == {'AAA', 'BBB', 'CCC', 'SPY'}           # DDD lists in February
    aaa = c['AAA']
    bars = load_bars()['AAA']['bars']
    prev = [b for b in bars if b[0][:10] < '2024-01-03'][-1]
    assert prev[0][:10] == '2024-01-02'
    assert aaa.time == datetime(2024, 1, 2) and aaa.end_time == datetime(2024, 1, 3)
    # Yahoo's split-adjusted close x the later 2:1 split = the raw price traded that day
    assert aaa.price == pytest.approx(prev[4] * 2)
    assert aaa.value == aaa.price
    assert aaa.volume == 1000000
    assert aaa.dollar_volume == pytest.approx(aaa.price * aaa.volume)
    assert aaa.split_factor == pytest.approx(0.5)
    assert aaa.price_factor == pytest.approx(0.98)
    assert aaa.adjusted_price == pytest.approx(prev[6])
    assert aaa.has_fundamental_data is True
    assert c['SPY'].has_fundamental_data is False
    assert type(aaa) is Fundamental


def test_coarse_has_no_look_ahead(tmp_path):
    day = datetime(2024, 2, 5)
    base, _ = make_provider(tmp_path / 'a')
    changed = load_bars()
    # change every bar from the selection day on: selection at day d sees only the session before d
    for e in changed.values():
        for b in e['bars']:
            if b[0][:10] >= day.strftime('%Y-%m-%d'):
                b[1:7] = [x * 10 for x in b[1:7]]
    other, _ = make_provider(tmp_path / 'b', store=changed)
    a, b = by_ticker(base.coarse(day)), by_ticker(other.coarse(day))
    assert set(a) == set(b)
    for t in a:
        for field in ('price', 'volume', 'dollar_volume', 'adjusted_price', 'time', 'has_fundamental_data'):
            assert getattr(a[t], field) == getattr(b[t], field), (t, field)
    # and the bars of the selection day itself do matter one day later
    a2, b2 = by_ticker(base.coarse(day + timedelta(days=1))), by_ticker(other.coarse(day + timedelta(days=1)))
    assert a2['AAA'].price != b2['AAA'].price


def test_listing_and_reference_days(tmp_path):
    p, _ = make_provider(tmp_path)
    assert 'DDD' not in by_ticker(p.coarse(datetime(2024, 2, 15)))       # first bar is on Feb 15
    assert 'DDD' in by_ticker(p.coarse(datetime(2024, 2, 16)))
    assert p.previous_session(datetime(2024, 1, 16)) == datetime(2024, 1, 12)   # Jan 15 holiday
    assert p.coarse(datetime(2023, 12, 1)) == []                                 # nothing known before the data


def test_pool_loaded_once_in_batches(tmp_path):
    p, calls = make_provider(tmp_path)
    p.prepare(datetime(2024, 1, 2), datetime(2024, 3, 28))
    p.coarse(datetime(2024, 1, 2))
    p.coarse(datetime(2024, 3, 1))
    assert len(calls['bars']) == 2                # one batched daily load + one call for later splits
    tickers, start, end = calls['bars'][0]
    assert set(tickers) == {'AAA', 'BBB', 'CCC', 'DDD', 'SPY'}
    assert start == '2023-01-01'                  # rounded to January 1 so runs share the cache
    assert end == '2024-04-01'                    # daily bars to the end of the run's last month
    tickers, start, end = calls['bars'][1]
    assert (start, end) == ('2024-04-01', '2024-07-01')   # split events after the window, up to "today"
    # AAA's 2:1 split of 2024-06-03 came from that tail: its raw price is rebuilt
    assert by_ticker(p.coarse(datetime(2024, 1, 3)))['AAA'].split_factor == pytest.approx(0.5)


def test_fundamentals_are_lazy_and_batched(tmp_path):
    p, calls = make_provider(tmp_path)
    coarse = p.coarse(datetime(2024, 1, 3))
    for f in coarse:
        f.__dict__['_batch'] = coarse
    c = by_ticker(coarse)
    _ = [(f.price, f.volume, f.dollar_volume, f.has_fundamental_data) for f in coarse]
    assert calls['info'] == []
    pe = c['AAA'].valuation_ratios.pe_ratio
    assert not math.isnan(pe)
    # one batch: every security with fundamental data in the selection, never the ETF
    assert sorted(calls['info']) == ['AAA', 'BBB', 'CCC']
    _ = c['BBB'].market_cap, c['CCC'].valuation_ratios.pe_ratio, c['SPY'].valuation_ratios.pe_ratio
    assert sorted(calls['info']) == ['AAA', 'BBB', 'CCC']


def test_fundamental_mapping_rescaled_to_the_selection_close(tmp_path):
    p, _ = make_provider(tmp_path)
    info = load_info()
    aaa = by_ticker(p.coarse(datetime(2024, 1, 3)))['AAA']
    close = aaa.price * aaa.split_factor                 # split-adjusted close (Yahoo's per-share units)
    r = close / info['currentPrice']
    vr = aaa.valuation_ratios
    assert vr.pe_ratio == pytest.approx(info['trailingPE'] * r)
    assert vr.pb_ratio == pytest.approx(info['priceToBook'] * r)
    assert vr.earning_yield == pytest.approx(1.0 / (info['trailingPE'] * r))
    assert aaa.market_cap == int(round(info['marketCap'] * r))
    assert aaa.company_profile.market_cap == aaa.market_cap
    ev = info['enterpriseValue'] + (info['marketCap'] * r - info['marketCap'])
    assert vr.ev_to_ebitda == pytest.approx(info['enterpriseToEbitda'] * ev / info['enterpriseValue'])
    assert vr.trailing_dividend_yield == pytest.approx(info['trailingAnnualDividendRate'] / close)
    ac = aaa.asset_classification
    assert ac.morningstar_sector_code == MorningstarSectorCode.TECHNOLOGY == 311
    assert ac.morningstar_industry_code == MorningstarIndustryCode.CONSUMER_ELECTRONICS
    assert ac.morningstar_industry_group_code == 31120 and ac.morningstar_economy_sphere_code == 3
    cr = aaa.company_reference
    assert (cr.country_id, cr.primary_exchange_id, cr.industry_template_code) == ('USA', 'NAS', 'N')
    assert aaa.security_reference.ipo_date == datetime(1980, 12, 12)
    assert aaa.company_profile.headquarter_city == 'Cupertino'
    assert aaa.operation_ratios.roe.one_year == pytest.approx(info['returnOnEquity'])
    assert aaa.operation_ratios.roe.value == pytest.approx(info['returnOnEquity'])
    assert aaa.earning_reports.basic_eps.twelve_months == pytest.approx(info['trailingEps'])
    assert aaa.financial_statements.income_statement.total_revenue.twelve_months == info['totalRevenue']
    assert aaa.financial_statements.balance_sheet.total_debt.three_months == info['totalDebt']
    # a period the snapshot does not report reads NaN
    assert math.isnan(aaa.financial_statements.balance_sheet.total_debt.twelve_months)


def test_missing_values_look_like_lean(tmp_path):
    p, calls = make_provider(tmp_path, infos={})        # Yahoo has nothing for any ticker
    c = by_ticker(p.coarse(datetime(2024, 1, 3)))
    bbb = c['BBB']
    assert math.isnan(bbb.valuation_ratios.pe_ratio)              # double -> NaN
    assert bbb.market_cap == 0                                     # long -> 0
    assert bbb.asset_classification.morningstar_sector_code == 0   # int -> 0
    assert bbb.company_reference.country_id is None                # string -> None
    assert bbb.security_reference.ipo_date == NO_DATE == datetime(1, 1, 1)   # DateTime -> MinValue
    assert bbb.company_reference.is_reit is False                  # bool -> False
    assert math.isnan(bbb.operation_ratios.roe.one_year) and math.isnan(bbb.operation_ratios.roe.value)
    assert not bbb.operation_ratios.roe.has_value
    # an ETF has no fundamental data: nothing is fetched, everything is missing
    spy = c['SPY']
    assert math.isnan(spy.valuation_ratios.pe_ratio) and spy.market_cap == 0
    assert 'SPY' not in calls['info']


def test_unmapped_lean_field_raises_not_supported(tmp_path):
    p, _ = make_provider(tmp_path)
    aaa = by_ticker(p.coarse(datetime(2024, 1, 3)))['AAA']
    with pytest.raises(NotSupportedByEngine) as e:
        _ = aaa.earning_ratios.equity_per_share_growth
    assert 'earning_ratios.equity_per_share_growth' in str(e.value)
    with pytest.raises(NotSupportedByEngine):
        _ = aaa.financial_statements.balance_sheet.total_assets
    with pytest.raises(NotSupportedByEngine):
        _ = aaa.valuation_ratios.forward_roa


def test_info_cache_ttl(tmp_path):
    p1, calls1 = make_provider(tmp_path)
    p1.prefetch_info(['AAA'])
    assert calls1['info'] == ['AAA']
    assert (tmp_path / 'cache' / 'info' / 'AAA.json').exists()
    p2, calls2 = make_provider(tmp_path)                 # same cache directory: served from disk
    assert p2.info('AAA')['symbol'] == 'AAPL'
    assert calls2['info'] == []
    p3, calls3 = make_provider(tmp_path, info_ttl_days=0)
    p3.info('AAA')
    assert calls3['info'] == ['AAA']


def test_info_failure_is_disclosed_not_invented(tmp_path):
    def boom(y):
        raise RuntimeError('network down')
    p = UniverseDataProvider(fetch_bars=fake_fetch(load_bars()), fetch_info=boom, fetch_holdings=lambda y: None,
                             pool=POOL, today=datetime(2024, 6, 30), cache_dir=str(tmp_path / 'c'), info_workers=1)
    aaa = by_ticker(p.coarse(datetime(2024, 1, 3)))['AAA']
    assert math.isnan(aaa.valuation_ratios.pe_ratio)
    assert any('could not be loaded' in n and 'AAA' in n for n in p.notes())


def test_etf_holdings_static_table_fallback(tmp_path):
    p, _ = make_provider(tmp_path)                        # fetch_holdings returns None
    cons = p.etf_constituents('SPY', datetime(2024, 1, 3))
    by = {c.symbol.value: c for c in cons}
    assert 'AAPL' in by and by['AAPL'].weight > 0
    assert by['AAPL'].shares_held is None and by['AAPL'].market_value is None   # not in the source: None
    assert by['AAPL'].time == datetime(2024, 1, 2)
    notes = ' '.join(p.notes())
    assert 'current top holdings' in notes and 'static table' in notes and '2026-11-17' in notes
    with pytest.raises(NotSupportedByEngine):
        p.etf_constituents('GDVD', datetime(2024, 1, 3))


def test_etf_holdings_live_then_cached(tmp_path):
    rows = [('BRK-B', 'Berkshire', 0.12), ('JPM', 'JPMorgan', 0.11)]
    p, _ = make_provider(tmp_path, holdings={'XLF': rows})
    cons = p.etf_constituents('XLF', datetime(2024, 1, 3))
    assert [c.symbol.value for c in cons] == ['BRK.B', 'JPM']          # LEAN spelling of class shares
    p2, _ = make_provider(tmp_path, holdings={})                        # cache hit, no fetch needed
    assert [c.symbol.value for c in p2.etf_constituents('XLF', datetime(2024, 1, 3))] == ['BRK.B', 'JPM']
    assert lean_name('BRK-B') == 'BRK.B' and lean_name('MSFT') == 'MSFT'


def test_disclosures(tmp_path):
    p, _ = make_provider(tmp_path)
    assert p.notes() == []
    coarse = p.coarse(datetime(2024, 1, 3))
    notes = p.notes()
    assert any('fixed pool of 4 liquid US stocks and 1 ETFs' in n and 'survivorship' in n for n in notes)
    assert FUNDAMENTAL_DISCLOSURE not in notes
    _ = by_ticker(coarse)['AAA'].valuation_ratios.pe_ratio
    assert FUNDAMENTAL_DISCLOSURE in p.notes()
    assert FUNDAMENTAL_DISCLOSURE == ("Fundamental filters use current values (Yahoo Finance snapshot), not values "
                                      "as of each backtest date; results have survivorship and look-ahead bias.")


def test_fundamentals_for_date_and_adjusted_bars(tmp_path):
    p, _ = make_provider(tmp_path)
    f = p.fundamental('AAA', datetime(2024, 1, 3, 9, 30))
    assert f.time == f.end_time == datetime(2024, 1, 3, 9, 30)
    assert f.price == pytest.approx(by_ticker(p.coarse(datetime(2024, 1, 3)))['AAA'].price)  # last complete bar
    bars = p.adjusted_daily('AAA')
    first = [b for b in bars if b['time'].startswith('2024-01-02')][0]
    raw = [b for b in load_bars()['AAA']['bars'] if b[0].startswith('2024-01-02')][0]
    assert first['close'] == pytest.approx(raw[6]) and first['open'] == pytest.approx(raw[1] * raw[6] / raw[4])


def test_classify_and_series_helpers():
    assert classify({'industry': 'Oil & Gas E&P'})[2] == MorningstarIndustryCode.OIL_AND_GAS_E_AND_P
    assert classify({'industry': 'Banks—Diversified'})[:3] == (103, 10320, 10320010)
    assert classify({'sector': 'Healthcare'}) == (206, 0, 0, 2)
    assert classify({}) == (0, 0, 0, 0)
    assert map_fundamentals({}, 10.0, 'X') == {}
    s = build_series('X', 'X', {'bars': [['2024-01-02 00:00:00', 10, 11, 9, 10, 100, 9.5],
                                         ['2024-01-03 00:00:00', 10, 11, 9, 10, 100, 9.5]],
                                'splits': [['2024-01-03', 2.0], ['2025-01-01', 3.0]]}, '2024-01-01', '2024-12-31')
    assert s.factor == [6.0, 3.0]        # splits strictly after each session
