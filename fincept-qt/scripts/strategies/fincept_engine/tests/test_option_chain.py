import bisect
import math
from datetime import date, datetime, timedelta

import pytest

from fincept_engine import option_chain as oc
from fincept_engine import option_model as om
from fincept_engine import option_pricing as op
from fincept_engine.enums import Market, OptionRight, OptionStyle, SecurityType
from fincept_engine.errors import NotSupportedByEngine
from fincept_engine.types import Symbol

CALL, PUT = OptionRight.CALL, OptionRight.PUT


def eq(index=False, ticker='XYZ'):
    return Symbol.create(ticker, SecurityType.INDEX if index else SecurityType.EQUITY, Market.USA)


# ---------------------------------------------------------------------------
# Expiry calendars
# ---------------------------------------------------------------------------

def _dates(exps):
    return [e.date.date() for e in exps]


def test_goog_2015_christmas_weekly_and_monthlies():
    exps = oc.listed_expiries('GOOG', datetime(2015, 12, 24), SecurityType.OPTION)
    by_date = {e.date.date(): e for e in exps}
    assert by_date[date(2015, 12, 24)].kind == oc.KIND_WEEKLY          # Friday 25th is Christmas
    assert by_date[date(2016, 1, 15)].kind == oc.KIND_MONTHLY
    assert by_date[date(2017, 1, 20)].kind == oc.KIND_LEAPS
    assert all(e.settlement == 'PM' for e in exps)
    assert _dates(exps) == sorted(_dates(exps))


def test_good_friday_moves_monthly_to_thursday():
    exps = oc.listed_expiries('XYZ', datetime(2022, 3, 1), SecurityType.OPTION)
    assert date(2022, 4, 14) in _dates(exps) and date(2022, 4, 15) not in _dates(exps)
    spx = {e.date.date(): e for e in oc.listed_expiries('SPX', datetime(2022, 3, 1), SecurityType.INDEX_OPTION)}
    assert spx[date(2022, 4, 14)].settlement == 'AM'
    assert spx[date(2022, 4, 14)].last_trading_date == date(2022, 4, 13)


def test_pre_2015_standard_monthlies_are_saturday_dated():
    exps = {e.date.date(): e for e in oc.listed_expiries('AAPL', datetime(2014, 6, 4), SecurityType.OPTION)}
    june = exps[date(2014, 6, 21)]
    assert june.kind == oc.KIND_MONTHLY and june.last_trading_date == date(2014, 6, 20)
    assert date(2014, 7, 3) in exps          # weekly moved to Thursday (July 4th holiday)
    assert oc.equity_monthly_expiry(2015, 1) == (date(2015, 1, 17), date(2015, 1, 16))
    assert oc.equity_monthly_expiry(2015, 2) == (date(2015, 2, 20), date(2015, 2, 20))
    # April 2014: third Friday was Good Friday -> Saturday date, last trading Thursday
    assert oc.equity_monthly_expiry(2014, 4) == (date(2014, 4, 19), date(2014, 4, 17))


def test_spx_root_lists_only_am_monthlies():
    exps = oc.listed_expiries('SPX', datetime(2021, 1, 4), SecurityType.INDEX_OPTION)
    assert exps and all(e.kind in (oc.KIND_MONTHLY, oc.KIND_LEAPS) and e.settlement == 'AM' for e in exps)
    assert all(e.date.weekday() == 4 for e in exps)


def test_spxw_programs_by_date():
    early = oc.listed_expiries('SPXW', datetime(2016, 1, 4), SecurityType.INDEX_OPTION)
    assert {e.date.weekday() for e in early if e.kind == oc.KIND_DAILY} == set()
    mid = oc.listed_expiries('SPXW', datetime(2021, 1, 4), SecurityType.INDEX_OPTION)
    assert {e.date.weekday() for e in mid if e.kind == oc.KIND_DAILY} == {0, 2}
    late = oc.listed_expiries('SPXW', datetime(2023, 3, 1), SecurityType.INDEX_OPTION)
    kinds = {e.date.date(): e.kind for e in late}
    assert {e.date.weekday() for e in late if e.kind == oc.KIND_DAILY} == {0, 1, 2, 3}
    assert kinds[date(2023, 3, 17)] == oc.KIND_MONTHLY_PM
    assert kinds[date(2023, 3, 31)] == oc.KIND_QUARTER_END
    assert kinds[date(2023, 4, 28)] == oc.KIND_MONTH_END
    # listing horizons: no Friday weekly further than 42 days, no daily further than 21 days
    for e in late:
        days = (e.date.date() - date(2023, 3, 1)).days
        if e.kind == oc.KIND_WEEKLY:
            assert days <= 42
        if e.kind == oc.KIND_DAILY:
            assert days <= 21


def test_vix_expiry_rule():
    assert oc.vix_monthly_expiry(2023, 1) == date(2023, 1, 18)
    assert oc.vix_monthly_expiry(2023, 2) == date(2023, 2, 15)
    assert oc.vix_monthly_expiry(2023, 3) == date(2023, 3, 22)
    assert oc.vix_monthly_expiry(2024, 6) == date(2024, 6, 18)      # Wednesday June 19 2024 was Juneteenth
    vix = oc.listed_expiries('VIX', datetime(2020, 1, 2), SecurityType.INDEX_OPTION)
    assert _dates(vix)[:3] == [date(2020, 1, 22), date(2020, 2, 19), date(2020, 3, 18)]
    assert all(e.settlement == 'AM' and e.last_trading_date == e.date.date() - timedelta(days=1) for e in vix)
    vixw = oc.listed_expiries('VIXW', datetime(2020, 1, 2), SecurityType.INDEX_OPTION)
    assert all(e.date.weekday() == 2 for e in vixw)
    assert not set(_dates(vixw)) & set(_dates(vix))


def test_future_option_expiries():
    gc = Symbol.create_future('GC', 'comex', datetime(2020, 12, 29))
    assert oc.future_option_expiry('GC', date(2020, 12, 29)) == date(2020, 11, 24)
    # the December contract carries the serial November options too (they exercise into December futures)
    og = oc.listed_expiries('OG', datetime(2020, 6, 1), SecurityType.FUTURE_OPTION, gc)
    assert _dates(og) == [date(2020, 10, 27), date(2020, 11, 24)]
    assert oc.future_option_expiry('CL', date(2020, 12, 21)) == date(2020, 12, 16)
    es = Symbol.create_future('ES', 'cme', datetime(2020, 6, 19))
    e = oc.listed_expiries('ES', datetime(2020, 1, 2), SecurityType.FUTURE_OPTION, es)[0]
    assert e.date.date() == date(2020, 6, 19) and e.settlement == 'AM'
    with pytest.raises(ValueError):
        oc.listed_expiries('OG', datetime(2020, 6, 1), SecurityType.FUTURE_OPTION, Symbol.create_future('GC'))


def test_is_standard_matches_lean():
    canon = Symbol.create_canonical_option(eq())
    sym = lambda d: oc.contract_symbol(canon, CALL, 100, d)
    assert oc.is_standard(sym(datetime(2023, 3, 17)))       # 3rd Friday
    assert not oc.is_standard(sym(datetime(2023, 3, 10)))   # Friday weekly
    assert not oc.is_standard(sym(datetime(2023, 3, 15)))   # Wednesday of the 3rd week
    assert oc.is_standard(sym(datetime(2022, 4, 14)))       # Thursday (Good Friday week)
    assert oc.is_standard(sym(datetime(2014, 6, 21)))       # pre-2015 Saturday
    assert oc.is_weekly(sym(datetime(2023, 3, 10)))
    spx = eq(True, 'SPX')
    spxw = Symbol.create_canonical_option(spx, 'SPXW', Market.USA)
    assert oc.is_standard(oc.contract_symbol(spxw, PUT, 4000, datetime(2023, 3, 17)))
    assert not oc.is_standard(oc.contract_symbol(spxw, PUT, 4000, datetime(2022, 4, 14)))
    assert oc.is_standard(oc.contract_symbol(Symbol.create_canonical_option(spx), PUT, 4000, datetime(2023, 3, 10)))


def test_expiry_datetime_and_settlement():
    spx = eq(True, 'SPX')
    am = oc.contract_symbol(Symbol.create_canonical_option(spx), CALL, 4000, datetime(2023, 3, 17))
    assert oc.settlement_type(am) == 'AM'
    assert oc.expiry_datetime(am) == datetime(2023, 3, 17, 9, 30)
    assert oc.last_trading_date(am) == date(2023, 3, 16)
    pm = oc.contract_symbol(Symbol.create_canonical_option(spx, 'SPXW', Market.USA), CALL, 4000,
                            datetime(2023, 11, 24))
    assert oc.expiry_datetime(pm) == datetime(2023, 11, 24, 13, 0)     # early close after Thanksgiving
    sat = oc.contract_symbol(Symbol.create_canonical_option(eq()), CALL, 100, datetime(2014, 6, 21))
    assert oc.expiry_datetime(sat) == datetime(2014, 6, 20, 16, 0)
    assert oc.time_to_expiry(sat, datetime(2014, 6, 20, 16, 0)) == 0.0
    assert oc.time_to_expiry(sat, datetime(2014, 6, 19, 16, 0)) == pytest.approx(1 / 365)


# ---------------------------------------------------------------------------
# Strikes and chain generation
# ---------------------------------------------------------------------------

def test_strike_increments_by_product_and_level():
    assert oc.strike_increment(750, oc.EQUITY_STRIKE_TIERS) == 5.0
    assert oc.strike_increment(30, oc.EQUITY_STRIKE_TIERS) == 1.0
    assert oc.strike_increment(8, oc.EQUITY_STRIKE_TIERS) == 0.5
    assert oc.strike_increment(400, oc.ETF_STRIKE_TIERS) == 1.0
    assert oc.strike_increment(4000, oc.INDEX_STRIKE_TIERS['SPX']) == 5.0
    assert oc.strike_increment(15000, oc.INDEX_STRIKE_TIERS['NDX']) == 25.0
    assert oc.strike_increment(15, oc.INDEX_STRIKE_TIERS['VIX']) == 0.5
    assert oc.strike_increment(1850, None) == 10.0     # generic level grid: [1000, 2500) step 10
    assert oc.strike_increment(0.92, None) == 0.005


def test_strike_ladder_fixed_grid_and_band():
    a = oc.strike_ladder(100.0, 30 / 365, 0.1, oc.EQUITY_STRIKE_TIERS)    # 4*0.1*sqrt(30/365) < 15%
    b = oc.strike_ladder(103.0, 30 / 365, 0.1, oc.EQUITY_STRIKE_TIERS)
    common = set(a) & set(b)
    assert len(common) > 10
    for k in a + b:
        inc = oc.strike_increment(k, oc.EQUITY_STRIKE_TIERS)
        assert abs(k / inc - round(k / inc)) < 1e-9
    assert min(a) >= 85.0 - 1e-9 and max(a) <= 115.0 + 1e-9            # 15% minimum band
    assert 100.0 in a
    wide = oc.strike_ladder(100.0, 2.0, 0.3, oc.EQUITY_STRIKE_TIERS)    # 4*0.3*sqrt(2) = 170% -> 90% cap
    assert min(wide) < 70 and max(wide) > 130
    assert len([k for k in wide if k < 100]) <= 40 and len([k for k in wide if k > 100]) <= 40


def test_chain_symbols_deterministic_and_stable():
    goog = eq(ticker='GOOG')
    canon = Symbol.create_canonical_option(goog)
    t = datetime(2015, 12, 24, 9, 30)
    s1 = oc.option_chain_symbols(canon, goog, t, 750.0, 0.3)
    s2 = oc.option_chain_symbols(canon, goog, t, 750.0, 0.3)
    assert [x.value for x in s1] == [x.value for x in s2]
    assert all(a is b for a, b in zip(s1, s2))
    assert s1[0].value == 'GOOG  151224C00650000'
    assert 'GOOG  151224C00745000' in {x.value for x in s1}
    assert all(x.id.option_style == OptionStyle.AMERICAN and x.canonical == canon for x in s1)
    nxt = oc.option_chain_symbols(canon, goog, t + timedelta(days=4), 760.0, 0.3)
    shared = {x.value: x for x in s1} .keys() & {x.value for x in nxt}
    assert len(shared) > 100
    first = {x.value: x for x in s1}
    assert all(first[v] is y for y in nxt for v in [y.value] if v in shared)
    spec = oc.option_chain_spec(canon, goog, t, 750.0, 0.3)
    assert sum(2 * len(ks) for _, ks in spec) == len(s1)


def test_index_chain_keeps_target_name_and_style():
    spx = eq(True, 'SPX')
    spxw = Symbol.create_canonical_option(spx, 'SPXW', Market.USA)
    syms = oc.option_chain_symbols(spxw, spx, datetime(2021, 1, 4, 9, 30), 3700.0, 0.2)
    assert syms and all(s.value.startswith('SPXW') and s.id.option_style == OptionStyle.EUROPEAN for s in syms)
    assert all(s.canonical.value == '?SPXW' and s.underlying == spx for s in syms)
    assert all(s.id.strike_price % 5 == 0 for s in syms)


def test_future_option_chain():
    gc = Symbol.create_future('GC', 'comex', datetime(2020, 12, 29))
    canon = Symbol.create_canonical_option(gc)
    syms = oc.option_chain_symbols(canon, gc, datetime(2020, 6, 1, 9, 30), 1750.0, 0.18)
    assert syms and all(s.value.startswith('OG') and s.security_type == SecurityType.FUTURE_OPTION for s in syms)
    assert {s.id.date.date() for s in syms} == {date(2020, 10, 27), date(2020, 11, 24)}


# ---------------------------------------------------------------------------
# OptionFilterUniverse on a hand-built chain
# ---------------------------------------------------------------------------

XYZ = eq()
CANON = Symbol.create_canonical_option(XYZ)
EXPIRIES = [datetime(2023, 3, 3), datetime(2023, 3, 10), datetime(2023, 3, 17), datetime(2023, 4, 21)]
STRIKES = [95, 97.5, 100, 102.5, 105, 107.5, 110]
T0 = datetime(2023, 3, 1)


def contracts():
    return [oc.contract_symbol(CANON, r, k, e) for e in EXPIRIES for k in STRIKES for r in (CALL, PUT)]


def fake_quote(sym):
    k = sym.id.strike_price
    return oc.OptionQuote(sym, T0, 1.0, 0.95, 1.05, 0.2 + k / 1000,
                          oc.Greeks(delta=k / 1000, gamma=0.001, vega=0.1, theta=-1.0, rho=0.05), 102.3)


def universe(price=102.3, quote_fn=fake_quote, t=T0, items=None):
    return oc.OptionFilterUniverse(items if items is not None else contracts(), price, t, quote_fn=quote_fn)


def picked(u):
    return sorted((s.id.date.date(), s.id.strike_price, int(s.id.option_right)) for s in u.symbols)


def strikes_of(u):
    return sorted({s.id.strike_price for s in u.symbols})


def test_relative_strikes():
    assert strikes_of(universe().strikes(-1, 1)) == [100, 102.5]
    assert strikes_of(universe().strikes(0, 1)) == [102.5, 105]
    assert strikes_of(universe().strikes(1, 2)) == [102.5, 105]     # first two strikes above 102.3
    assert strikes_of(universe(price=100).strikes(-2, 2)) == [95, 97.5, 100, 102.5, 105]
    assert strikes_of(universe(price=100).strikes(-10, 10)) == STRIKES
    assert universe(price=111).strikes(-1, 1).count == 0
    assert strikes_of(universe().strikes([95, 110])) == [95, 110]
    assert strikes_of(universe().strikes_above(105)) == [107.5, 110]
    assert strikes_of(universe().strikes_below(97.5)) == [95]


def test_expiration_filters():
    assert {s.id.date for s in universe().expiration(0, 7).symbols} == {EXPIRIES[0]}
    u = universe().expiration(timedelta(days=5), timedelta(days=20))
    assert {s.id.date for s in u.symbols} == {EXPIRIES[1], EXPIRIES[2]}
    # Saturday universe: reference moves to Monday 2023-03-06
    u = universe(t=datetime(2023, 3, 4)).expiration(0, 7)
    assert {s.id.date for s in u.symbols} == {EXPIRIES[1]}
    assert {s.id.date for s in universe().expiration([date(2023, 4, 21)]).symbols} == {EXPIRIES[3]}
    assert {s.id.date for s in universe().expiring_after(datetime(2023, 3, 10)).symbols} == set(EXPIRIES[2:])
    assert {s.id.date for s in universe().expiring_before(datetime(2023, 3, 10)).symbols} == {EXPIRIES[0]}
    assert universe(t=datetime(2023, 3, 3, 9, 30)).zero_dte().count == len(STRIKES) * 2


def test_weekly_standard_type_filters_are_lazy_like_lean():
    u = universe().weeklys_only()
    assert u.count == len(contracts())          # not applied yet
    u.apply_types_filter()
    assert {s.id.date for s in u.symbols} == {EXPIRIES[0], EXPIRIES[1]}
    u = universe().standards_only().apply_types_filter()
    assert {s.id.date for s in u.symbols} == {EXPIRIES[2], EXPIRIES[3]}
    u = universe().include_weeklys().apply_types_filter()
    assert u.count == len(contracts())
    assert {s.id.date for s in universe().standards_only().front_month().symbols} == {EXPIRIES[2]}
    with pytest.raises(RuntimeError):
        universe().front_month().standards_only()


def test_front_back_months_and_rights():
    assert {s.id.date for s in universe().front_month().symbols} == {EXPIRIES[0]}
    assert {s.id.date for s in universe().back_month().symbols} == {EXPIRIES[1]}
    assert {s.id.date for s in universe().back_months().symbols} == set(EXPIRIES[1:])
    assert {s.id.date for s in universe().farthest_expiration().symbols} == {EXPIRIES[3]}
    assert {s.id.option_right for s in universe().calls_only().symbols} == {CALL}
    assert {s.id.option_right for s in universe().puts_only().symbols} == {PUT}
    otm = universe().otm()
    assert all((s.id.strike_price > 102.3) == (s.id.option_right == CALL) for s in otm.symbols)
    itm = universe().itm()
    assert all((s.id.strike_price < 102.3) == (s.id.option_right == CALL) for s in itm.symbols)
    assert strikes_of(universe().atm()) == [102.5]           # 100 is 2.3 away > 2% of 102.3
    assert strikes_of(universe().atm(3)) == [100, 102.5, 105]


def test_contracts_func_gets_option_contracts_with_greeks():
    seen = []

    def selector(items):
        seen.extend(items)
        return [c for c in items if c.Greeks.Delta > 0.104 and c.ImpliedVolatility < 0.31
                and c.OpenInterest == 0]

    u = universe().contracts(selector)
    assert all(isinstance(c, oc.OptionContract) for c in seen)
    assert strikes_of(u) == [105, 107.5]
    # a list of symbols (or contracts) restricts the universe
    keep = [c for c in universe()][:3]
    assert universe().contracts(keep).count == 3
    assert universe().contracts([keep[0].symbol]).symbols == [keep[0].symbol]
    assert universe().where(lambda c: c.strike == 100).count == 8
    assert universe().where_contains([keep[1].symbol.value]).symbols == [keep[1].symbol]
    assert len(list(universe())) == len(contracts())


def test_greek_and_iv_filters_use_the_pricing_callback():
    assert strikes_of(universe().delta(0.1, 0.105)) == [100, 102.5, 105]
    assert strikes_of(universe().d(0.1, 0.105).iv(0.2, 0.3025)) == [100, 102.5]
    assert universe().g(0, 1).v(0, 1).t(-2, 0).r(0, 1).count == len(contracts())
    with pytest.raises(NotSupportedByEngine):
        universe(quote_fn=None).delta(0, 1)


def test_open_interest_and_volume_are_not_invented():
    assert universe().oi(0, 10).count == len(contracts())         # 0 is inside the range
    with pytest.raises(NotSupportedByEngine):
        universe().open_interest(100, 500)
    with pytest.raises(NotSupportedByEngine):
        universe().volume(1, 1000)
    c = next(iter(universe()))
    assert c.open_interest == 0 and c.volume == 0


def test_future_option_greek_filters_unsupported_like_lean():
    gc = Symbol.create_future('GC', 'comex', datetime(2020, 12, 29))
    syms = oc.option_chain_symbols(Symbol.create_canonical_option(gc), gc, datetime(2020, 6, 1), 1750.0, 0.18)
    with pytest.raises(RuntimeError):
        oc.OptionFilterUniverse(syms, 1750.0, datetime(2020, 6, 1), quote_fn=fake_quote).delta(0, 1)


def test_strategy_filters():
    assert picked(universe().straddle(10)) == [(date(2023, 3, 17), 102.5, 0), (date(2023, 3, 17), 102.5, 1)]
    assert picked(universe().strangle(10, 5, -5)) == [(date(2023, 3, 17), 97.5, 1), (date(2023, 3, 17), 107.5, 0)]
    assert picked(universe().naked_call(10, 0)) == [(date(2023, 3, 17), 102.5, 0)]
    assert picked(universe().naked_put(0, -5)) == [(date(2023, 3, 3), 97.5, 1)]
    assert picked(universe().call_spread(10, 5)) == [(date(2023, 3, 17), 97.5, 0), (date(2023, 3, 17), 107.5, 0)]
    assert picked(universe().put_spread(10, 5, 0)) == [(date(2023, 3, 17), 102.5, 1), (date(2023, 3, 17), 107.5, 1)]
    assert picked(universe().iron_condor(10, 5, 10)) == [
        (date(2023, 3, 17), 95, 1), (date(2023, 3, 17), 97.5, 1), (date(2023, 3, 17), 107.5, 0),
        (date(2023, 3, 17), 110, 0)]
    assert picked(universe().call_butterfly(10, 5)) == [
        (date(2023, 3, 17), 97.5, 0), (date(2023, 3, 17), 102.5, 0), (date(2023, 3, 17), 107.5, 0)]
    assert picked(universe().put_butterfly(10, 5)) == [
        (date(2023, 3, 17), 97.5, 1), (date(2023, 3, 17), 102.5, 1), (date(2023, 3, 17), 107.5, 1)]
    assert picked(universe().iron_butterfly(10, 5)) == [
        (date(2023, 3, 17), 97.5, 1), (date(2023, 3, 17), 102.5, 0), (date(2023, 3, 17), 102.5, 1),
        (date(2023, 3, 17), 107.5, 0)]
    assert picked(universe().box_spread(10, 5)) == [
        (date(2023, 3, 17), 97.5, 0), (date(2023, 3, 17), 97.5, 1), (date(2023, 3, 17), 107.5, 0),
        (date(2023, 3, 17), 107.5, 1)]
    assert picked(universe().jelly_roll(0, 10, 30)) == [
        (date(2023, 3, 17), 102.5, 0), (date(2023, 3, 17), 102.5, 1), (date(2023, 4, 21), 102.5, 0),
        (date(2023, 4, 21), 102.5, 1)]
    assert picked(universe().call_calendar_spread(0, 10, 30)) == [(date(2023, 3, 17), 102.5, 0),
                                                                   (date(2023, 4, 21), 102.5, 0)]
    assert picked(universe().put_calendar_spread(-5, 0, 7)) == [(date(2023, 3, 3), 97.5, 1),
                                                                 (date(2023, 3, 10), 97.5, 1)]
    assert picked(universe().call_ladder(10, 7, 2, -3)) == [
        (date(2023, 3, 17), 100, 0), (date(2023, 3, 17), 105, 0), (date(2023, 3, 17), 110, 0)]
    assert picked(universe().protective_collar(10, 5, -5)) == [(date(2023, 3, 17), 97.5, 1),
                                                                (date(2023, 3, 17), 107.5, 0)]
    assert picked(universe().conversion(10, 0)) == [(date(2023, 3, 17), 102.5, 0), (date(2023, 3, 17), 102.5, 1)]
    assert universe().straddle(60).count == 0            # no expiry 60+ days out
    assert universe(price=None).straddle(10).count == 0  # LEAN: no underlying -> empty


def test_strategy_filter_argument_validation():
    with pytest.raises(ValueError):
        universe().strangle(30, 0, -5)
    with pytest.raises(ValueError):
        universe().strangle(30, 5, 5)
    with pytest.raises(ValueError):
        universe().iron_condor(30, 10, 5)
    with pytest.raises(ValueError):
        universe().call_spread(30, -5, 5)
    with pytest.raises(ValueError):
        universe().jelly_roll(0, 60, 30)
    with pytest.raises(ValueError):
        universe().call_ladder(30, 1, 2, 3)
    with pytest.raises(ValueError):
        universe().box_spread(30, 0)
    with pytest.raises(ValueError):
        universe().atm(-1)


def test_pascal_case_aliases():
    u = universe()
    assert u.StandardsOnly().Strikes(-1, 1).Expiration(0, 30).CallsOnly() is u
    assert u.OnlyApplyFilterAtMarketOpen() is u
    assert universe().IncludeWeeklys().FrontMonth().count == len(STRIKES) * 2


def test_apply_filter_and_legacy_set_filter():
    # filter returning a list of contracts (BasicTemplateOptionsFilterUniverseAlgorithm style)
    def list_filter(u):
        u = u.weeklys_only().expiration(0, 1)
        return [c for c in u if c.symbol.id.option_right != PUT and -5 < u.underlying.price - c.symbol.id.strike_price < 5]

    r = oc.apply_filter(list_filter, universe(t=datetime(2023, 3, 3)))
    assert picked(r) == [(date(2023, 3, 3), k, 0) for k in (97.5, 100, 102.5, 105)]   # |102.3 - K| < 5

    # filter returning None after in-place calls (CoarseFineOptionUniverseChainRegressionAlgorithm style)
    def none_filter(u):
        u.include_weeklys().front_month()

    assert {s.id.date for s in oc.apply_filter(none_filter, universe()).symbols} == {EXPIRIES[0]}
    f = oc.set_filter_function(-1, 1, 0, 10)
    assert picked(oc.apply_filter(f, universe())) == [
        (d.date(), k, r) for d in EXPIRIES[:2] for k in (100, 102.5) for r in (0, 1)]
    f = oc.set_filter_function(timedelta(days=15), timedelta(days=60))
    assert {s.id.date for s in oc.apply_filter(f, universe()).symbols} == {EXPIRIES[2], EXPIRIES[3]}
    assert strikes_of(oc.apply_filter(oc.set_filter_function(0, 1), universe())) == [102.5, 105]
    assert picked(oc.apply_filter(oc.default_filter(), universe())) == [
        (d.date(), k, r) for d in EXPIRIES[:3] for k in (100, 102.5) for r in (0, 1)]
    with pytest.raises(TypeError):
        oc.apply_filter(lambda u: 5, universe())


# ---------------------------------------------------------------------------
# OptionChain / OptionChains
# ---------------------------------------------------------------------------

def make_chain(t=T0):
    u = oc.apply_filter(lambda x: x.strikes(-1, 1), universe(t=t))
    return oc.OptionChain(CANON, t, u, oc.ChainUnderlying(XYZ, 102.3, t), quote_fn=fake_quote)


def test_option_chain_shape():
    chain = make_chain()
    assert len(chain) == 16 and chain.count == 16
    keys = [(c.expiry, c.strike, int(c.right)) for c in chain]
    assert keys == sorted(keys)
    first = next(iter(chain))
    assert chain.contracts[first.symbol] is first
    assert chain.contracts[first.symbol.value.lower()] is first
    assert first.symbol in chain and chain.contains_key(first.symbol.value)
    assert chain.underlying.price == chain.underlying.value == chain.underlying.close == 102.3
    assert chain.underlying.symbol == XYZ and chain.symbol == CANON
    assert first.bid_price == 0.95 and first.ask_price == 1.05 and first.last_price == 1.0
    assert first.underlying_last_price == 102.3 and first.UnderlyingLastPrice == 102.3
    assert first.days_to_expiry == 2 and first.DaysToExpiry == 2
    assert first.ID == first.symbol.id and first.UnderlyingSymbol == XYZ
    calls = chain.calls_only()
    assert isinstance(calls, oc.OptionChain) and len(calls) == 8 and len(chain) == 16
    assert len(chain.straddle(10)) == 2
    assert len(chain.where(lambda c: c.strike == 100)) == 8
    # contracts whose expiry date is before the chain time are dropped
    late = make_chain(t=datetime(2023, 3, 6))
    assert {c.expiry for c in late} == set(EXPIRIES[1:])


def test_option_chain_data_frame():
    pd = pytest.importorskip('pandas')
    chain = make_chain()
    df = chain.data_frame
    assert df.index.name == 'symbol' and len(df) == 16
    for col in ('expiry', 'strike', 'right', 'impliedvolatility', 'delta', 'gamma', 'vega', 'theta', 'rho',
                'openinterest', 'lastprice', 'bidprice', 'askprice', 'underlyinglastprice'):
        assert col in df.columns
    sel = df.loc[(df.expiry <= T0 + timedelta(days=10)) & (df.delta < 0.101)]
    assert set(sel.strike) == {100.0}
    best = sel.loc[sel.expiry.idxmax()].name
    assert isinstance(best, Symbol) and best.id.date == EXPIRIES[1]


def test_option_chains_lookup_and_iteration():
    chain = make_chain()
    spx = eq(True, 'SPX')
    spxw = Symbol.create_canonical_option(spx, 'SPXW', Market.USA)
    w = oc.OptionChain(spxw, T0, [], 4000.0)
    chains = oc.OptionChains(T0)
    chains[CANON] = chain
    chains.add(w)
    assert chains[CANON] is chain and chains.get(CANON) is chain
    assert chains.get(XYZ) is chain                    # underlying -> default canonical
    assert chains.get('?xyz') is chain and chains.get('XYZ') is chain
    assert chains.get(next(iter(chain)).symbol) is chain
    assert chains.get_value(spxw) is w and chains.get(spx) is None      # SPX maps to ?SPX, not ?SPXW
    assert chains.get('SPXW  230317C04000000') is w
    assert chains.get_value('MISSING') is None
    with pytest.raises(KeyError):
        chains['MISSING']
    assert chains.contains_key(XYZ) and XYZ in chains and 'NOPE' not in chains
    assert chains.try_get_value(CANON) == (True, chain) and chains.try_get_value('NOPE') == (False, None)
    assert chains.count == 2 and len(chains) == 2
    kvps = list(chains)
    assert [k.key for k in kvps] == [CANON, spxw] and kvps[0].value is chain and kvps[0].Value is chain
    for k, v in chains:
        assert chains[k] is v
    assert list(chains.values())[0] is chain and list(chains.keys())[1] == spxw


def test_option_chains_data_frame():
    pytest.importorskip('pandas')
    chains = oc.OptionChains(T0)
    chains[CANON] = make_chain()
    df = chains.data_frame
    assert list(df.index.names) == ['canonical', 'symbol'] and len(df) == 16
    canonicals = df.index.get_level_values('canonical')
    assert all(c.underlying == XYZ for c in canonicals)
    assert df.iloc[0].name[1].id.date == EXPIRIES[0]


def test_greeks_are_lazy():
    calls = []

    def compute():
        calls.append(1)
        return (0.5, 0.01, 0.2, -36.5, 0.1, 3.0)

    g = oc.Greeks.lazy(compute)
    assert not calls
    assert g.delta == 0.5 and g.Delta == 0.5 and g.theta_per_day == pytest.approx(-0.1) and g.lambda_ == 3.0
    assert g.ThetaPerDay == pytest.approx(-0.1) and g.Lambda == 3.0
    _ = g.gamma, g.vega, g.rho
    assert len(calls) == 1


# ---------------------------------------------------------------------------
# OptionModel: market inputs, quotes, no look-ahead
# ---------------------------------------------------------------------------

class FakeSeries:
    def __init__(self, dates, opens, closes):
        self.dates = list(dates)
        self.open = list(opens)
        self.close = list(closes)
        self.high = [max(o, c) for o, c in zip(opens, closes)]
        self.low = [min(o, c) for o, c in zip(opens, closes)]
        self._ends = [d + timedelta(hours=16) for d in self.dates]

    def count_known_at(self, t):
        return bisect.bisect_right(self._ends, t)


SESSIONS = [d for d in (datetime(2022, 1, 3) + timedelta(days=i) for i in range(560)) if d.weekday() < 5]


def make_series(vix_open=None, extra=None):
    n = len(SESSIONS)
    data = {
        '^VIX9D': FakeSeries(SESSIONS, [15.0] * n, [15.5] * n),
        '^VIX': FakeSeries(SESSIONS, vix_open or [19.0 + (i % 5) for i in range(n)], [20.0 + (i % 7) for i in range(n)]),
        '^VIX3M': FakeSeries(SESSIONS, [22.0] * n, [22.0] * n),
        '^IRX': FakeSeries(SESSIONS, [5.0] * n, [5.0] * n),
        '^GSPC': FakeSeries(SESSIONS, [4000.0] * n, [4000.0 + 10 * math.sin(i) for i in range(n)]),
        'SPY': FakeSeries(SESSIONS, [400.0] * n, [400.0] * n),
        'XYZ': FakeSeries(SESSIONS, [100.0 + i * 0.05 for i in range(n)],
                          [100.0 * math.exp(0.02 * math.sin(i * 1.7)) for i in range(n)]),
    }
    data.update(extra or {})
    return data


DIVS = {'SPY': [(datetime(2022, 3, 18), 1.0), (datetime(2022, 6, 17), 1.5), (datetime(2022, 9, 16), 1.5),
                (datetime(2022, 12, 16), 2.0), (datetime(2023, 3, 17), 2.0)],
        'XYZ': [(datetime(2022, 8, 1), 0.5), (datetime(2023, 2, 1), 0.5)]}


def model(series=None):
    s = series or make_series()
    return om.OptionModel(s.get, lambda tk: DIVS.get(tk, []))


def test_rate_from_irx():
    m = model()
    r = m.rate(datetime(2023, 1, 5, 16), 'close')
    assert r == pytest.approx(-math.log(1 - 0.05 * 91 / 360) * 365 / 91)
    assert om.OptionModel(lambda tk: None).rate(datetime(2023, 1, 5)) == 0.0


def test_dividend_yield_trailing_and_known_at_t():
    m = model()
    spx = eq(True, 'SPX')
    # at 2023-01-05: ex-dates within the last 365 days: 2022-03-18 .. 2022-12-16 -> 6.0 / 400
    assert m.dividend_yield(spx, datetime(2023, 1, 5, 16)) == pytest.approx(6.0 / 400)
    # 2023-03-17 dividend not known on 2023-03-16; 2022-03-18 drops out after 2023-03-18
    assert m.dividend_yield(spx, datetime(2023, 3, 16, 16)) == pytest.approx(6.0 / 400)
    assert m.dividend_yield(spx, datetime(2023, 3, 17, 16)) == pytest.approx(8.0 / 400)
    assert m.dividend_yield(spx, datetime(2023, 3, 20, 16)) == pytest.approx(7.0 / 400)
    gc = Symbol.create_future('GC', 'comex', datetime(2023, 12, 27))
    assert m.dividend_yield(gc, datetime(2023, 3, 20, 16)) == 0.0


def test_vol_index_open_vs_close_no_look_ahead():
    m = model()
    s = make_series()['^VIX']
    spx = eq(True, 'SPX')
    i = SESSIONS.index(datetime(2023, 1, 5))
    T = 30 / 365
    assert m.atm_vol(spx, datetime(2023, 1, 5, 9, 30), T, 'open') == pytest.approx(s.open[i] / 100)
    assert m.atm_vol(spx, datetime(2023, 1, 5, 9, 0), T, 'open') == pytest.approx(s.close[i - 1] / 100)
    assert m.atm_vol(spx, datetime(2023, 1, 5, 16, 0), T, 'close') == pytest.approx(s.close[i] / 100)
    assert m.atm_vol(spx, datetime(2023, 1, 5, 12, 0), T, 'close') == pytest.approx(s.close[i - 1] / 100)


def test_term_structure_interpolates_in_total_variance():
    m = model()
    spx = eq(True, 'SPX')
    t = datetime(2023, 1, 5, 16)
    v9, v30, v93 = 0.155, m.atm_vol(spx, t, 30 / 365), 0.22
    assert m.atm_vol(spx, t, 9 / 365) == pytest.approx(v9)
    assert m.atm_vol(spx, t, 5 / 365) == pytest.approx(v9)            # flat before the first point
    assert m.atm_vol(spx, t, 200 / 365) == pytest.approx(v93)         # flat after the last (no ^VIX6M)
    T = 60 / 365
    w = v30 ** 2 * 30 / 365 + (v93 ** 2 * 93 / 365 - v30 ** 2 * 30 / 365) * (60 - 30) / (93 - 30)
    assert m.atm_vol(spx, t, T) == pytest.approx(math.sqrt(w / T))


def test_realized_vol_for_equities():
    m = model()
    xyz_s = make_series()['XYZ']
    t = datetime(2023, 1, 5, 16)
    n = xyz_s.count_known_at(t)
    closes = xyz_s.close[n - 31:n]
    rets = [math.log(b / a) for a, b in zip(closes, closes[1:])]
    mu = sum(rets) / len(rets)
    rv = math.sqrt(sum((x - mu) ** 2 for x in rets) / (len(rets) - 1) * 252)
    assert m.atm_vol(XYZ, t) == pytest.approx(max(rv, m.vol_floor))
    flat = make_series(extra={'XYZ': FakeSeries(SESSIONS, [100.0] * len(SESSIONS), [100.0] * len(SESSIONS))})
    assert model(flat).atm_vol(XYZ, t) == pytest.approx(0.10)        # floor
    short = make_series(extra={'XYZ': FakeSeries(SESSIONS[:5], [100.0] * 5, [100.0] * 5)})
    m2 = model(short)
    assert m2.atm_vol(XYZ, datetime(2022, 1, 7, 16)) == pytest.approx(0.30)
    assert m2.fallbacks and 'default 30% volatility' in m2.disclosure(XYZ)


def test_quote_prices_spreads_and_greeks():
    m = model()
    t = datetime(2023, 1, 5, 9, 30)
    c = oc.contract_symbol(CANON, CALL, 100, datetime(2023, 2, 17))
    q = m.quote(c, 101.0, t, 'open')
    T = oc.time_to_expiry(c, t)
    r, qy = m.rate(t, 'open'), m.dividend_yield(XYZ, t, 'open')
    F = 101.0 * math.exp((r - qy) * T)
    sigma = m.skewed_vol(m.info(c), m.atm_vol(XYZ, t, T, 'open'), 100, F, T)
    assert q.implied_volatility == pytest.approx(sigma)
    assert q.mid == pytest.approx(op.bs_price(101.0, 100, T, r, qy, sigma, True))
    half = 0.5 * max(0.05, 0.03 * q.mid)
    assert (q.bid, q.ask) == (pytest.approx(q.mid - half), pytest.approx(q.mid + half))
    assert q.greeks._v is None                                         # lazy
    d, g, v, th, rho = op.bs_greeks(101.0, 100, T, r, qy, sigma, True)
    assert (q.greeks.delta, q.greeks.vega, q.greeks.theta) == (pytest.approx(d), pytest.approx(v), pytest.approx(th))
    assert q.greeks.lambda_ == pytest.approx(d * 101.0 / q.mid)
    # implied vol of the mid recovers the model vol
    assert op.implied_volatility(q.mid, 101.0, 100, T, r, qy, True) == pytest.approx(sigma, rel=1e-6)
    # deep OTM: bid floored at 0
    far = m.quote(oc.contract_symbol(CANON, CALL, 300, datetime(2023, 1, 13)), 101.0, t, 'open')
    assert far.bid == 0.0 and far.ask == pytest.approx(far.mid + 0.025)
    # expired: intrinsic, no spread
    exp = m.quote(oc.contract_symbol(CANON, PUT, 105, datetime(2023, 1, 6)), 101.0, datetime(2023, 1, 6, 16), 'close')
    assert exp.mid == exp.bid == exp.ask == pytest.approx(4.0) and exp.greeks.delta == -1.0


def test_skew_rule_and_index_spread():
    m = model()
    spx = eq(True, 'SPX')
    canon = Symbol.create_canonical_option(spx)
    t = datetime(2023, 1, 5, 16)
    ivs = [m.quote(oc.contract_symbol(canon, PUT, k, datetime(2023, 2, 17)), 4000.0, t).implied_volatility
           for k in (3400, 3800, 4000, 4200)]
    assert ivs[0] > ivs[1] > ivs[2] > ivs[3]                          # equity-index put skew
    q = m.quote(oc.contract_symbol(canon, CALL, 4000, datetime(2023, 2, 17)), 4000.0, t)
    half = 0.5 * max(0.10, 0.02 * q.mid)
    assert q.ask - q.mid == pytest.approx(half)


def test_black76_for_future_options_and_vix_forward():
    gc = Symbol.create_future('GC', 'comex', datetime(2023, 12, 27))
    canon = Symbol.create_canonical_option(gc)
    n = len(SESSIONS)
    m = model(make_series(extra={'^GVZ': FakeSeries(SESSIONS, [16.0] * n, [16.0] * n),
                                 '^VIX': FakeSeries(SESSIONS, [18.0] * n, [18.0] * n)}))
    t = datetime(2023, 3, 1, 16)
    c = oc.contract_symbol(canon, CALL, 1900, datetime(2023, 11, 27))
    q = m.quote(c, 1850.0, t)
    T = oc.time_to_expiry(c, t)
    sigma = m.skewed_vol(m.info(c), 0.16, 1900, 1850.0, T)
    assert q.model == op.MODEL_BLACK76 and q.forward == 1850.0
    assert q.mid == pytest.approx(op.black76_price(1850.0, 1900, T, m.rate(t), sigma, True))
    assert q.ask - q.mid == pytest.approx(0.5 * max(0.2, 0.02 * q.mid))
    assert '^GVZ' in m.disclosure(c)
    # VIX options: Black-76 on a forward above spot when the S&P term structure is upward sloping
    vix = eq(True, 'VIX')
    vc = Symbol.create_canonical_option(vix)
    vq = m.quote(oc.contract_symbol(vc, CALL, 20, datetime(2023, 4, 19)), 19.0, t)
    assert vq.model == op.MODEL_BLACK76 and vq.forward > 19.0
    flat = make_series(extra={'^VIX9D': FakeSeries(SESSIONS, [20.0] * len(SESSIONS), [20.0] * len(SESSIONS)),
                              '^VIX': FakeSeries(SESSIONS, [20.0] * len(SESSIONS), [20.0] * len(SESSIONS)),
                              '^VIX3M': FakeSeries(SESSIONS, [20.0] * len(SESSIONS), [20.0] * len(SESSIONS))})
    assert model(flat).vix_forward(t, 0.2, 19.0) == pytest.approx(19.0)


def _pipeline(series, t, at='open'):
    """What the integrator does each day: price known at t -> vol hint -> chain -> quotes."""
    goog = eq(ticker='XYZ')
    canon = Symbol.create_canonical_option(goog)
    m = om.OptionModel(series.get, lambda tk: DIVS.get(tk, []))
    s = series['XYZ']
    n = s.count_known_at(t)
    price = s.open[n] if at == 'open' and s.dates[n].date() == t.date() else s.close[n - 1]
    syms = oc.option_chain_symbols(canon, goog, t, price, m.atm_vol(goog, t, at=at))
    quotes = [m.quote(x, price, t, at) for x in syms[::7]]
    spx = eq(True, 'SPX')
    spx_q = m.quote(oc.contract_symbol(Symbol.create_canonical_option(spx), PUT, 3900, datetime(2023, 2, 17)),
                    4000.0, t, at)
    return ([x.value for x in syms], [(q.mid, q.bid, q.ask, q.implied_volatility, q.greeks.delta) for q in quotes],
            (spx_q.mid, spx_q.implied_volatility))


def _mutate_after(series, cut: datetime, keep_open_of_cut_day=True):
    out = {}
    for tk, s in series.items():
        o, c = list(s.open), list(s.close)
        for i, d in enumerate(s.dates):
            if d.date() > cut.date():
                o[i] *= 1.37
                c[i] *= 0.61
            elif d.date() == cut.date():
                c[i] *= 0.61                          # the close of the quote day is not known at the open
                if not keep_open_of_cut_day:
                    o[i] *= 1.37
        out[tk] = FakeSeries(s.dates, o, c)
    return out


def test_no_look_ahead_at_open():
    series = make_series()
    t = datetime(2023, 1, 5, 9, 30)
    base = _pipeline(series, t, 'open')
    assert base == _pipeline(_mutate_after(series, t), t, 'open')
    assert base != _pipeline(_mutate_after(series, t, keep_open_of_cut_day=False), t, 'open')   # sanity


def test_no_look_ahead_at_close():
    series = make_series()
    t = datetime(2023, 1, 5, 16, 0)
    base = _pipeline(series, t, 'close')
    changed = {}
    for tk, s in series.items():
        o, c = list(s.open), list(s.close)
        for i, d in enumerate(s.dates):
            if d.date() > t.date():
                o[i] *= 1.37
                c[i] *= 0.61
        changed[tk] = FakeSeries(s.dates, o, c)
    assert base == _pipeline(changed, t, 'close')


def test_disclosure_text():
    m = model()
    spx = eq(True, 'SPX')
    text = m.disclosure(spx)
    assert text.startswith("Option prices are model-derived, not historical market quotes")
    for part in ('^VIX9D/^VIX/^VIX3M', '^IRX', 'SPY (proxy)', 'max($0.10, 2% of mid)', 'Open interest and volume',
                 'European'):
        assert part in text
    assert m.vol_source(spx)['indexes'] == [('^VIX9D', 9), ('^VIX', 30), ('^VIX3M', 93)]
    assert m.vol_source(XYZ)['indexes'] == [] and m.vol_source(XYZ)['fallback'] == 'XYZ'
    t2 = m.disclosure(XYZ, include_chain=False)
    assert 'realised volatility of XYZ' in t2 and 'early-exercise' in t2 and 'max($0.05, 3% of mid)' in t2
    assert 'Option chains are generated' not in t2
