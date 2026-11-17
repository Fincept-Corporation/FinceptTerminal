from datetime import date, datetime

import pytest

from fincept_engine import option_strategies as ost
from fincept_engine.enums import Market, OptionRight, OptionStyle, SecurityType
from fincept_engine.option_strategies import Leg, OptionStrategies, OptionStrategy
from fincept_engine.types import Symbol

C, P = OptionRight.CALL, OptionRight.PUT
GOOG = Symbol.create('GOOG', SecurityType.EQUITY, Market.USA)
CANON = Symbol.create_canonical_option(GOOG)
EXP = datetime(2016, 1, 15)
FAR = datetime(2016, 2, 19)


def legs(strategy):
    return [(int(l.right), l.strike, l.quantity, l.expiration) for l in strategy.option_legs]


def und(strategy):
    return [(l.symbol, l.quantity) for l in strategy.underlying_legs]


def test_leg_create():
    leg = Leg.create(GOOG, 5)
    assert (leg.symbol, leg.quantity, leg.order_price) == (GOOG, 5, None)
    leg = Leg.Create(GOOG, -3, 101.5)
    assert (leg.Symbol, leg.Quantity, leg.OrderPrice) == (GOOG, -3, 101.5)


def test_covered_protective_collar_conversion():
    s = OptionStrategies.covered_call(CANON, 750, EXP)
    assert s.name == "Covered Call" and legs(s) == [(0, 750, -1, EXP)] and und(s) == [(GOOG, 100)]
    s = OptionStrategies.protective_call(CANON, 750, EXP)
    assert s.name == "Protective Call" and legs(s) == [(0, 750, 1, EXP)] and und(s) == [(GOOG, -100)]
    s = OptionStrategies.covered_put(CANON, 750, EXP)
    assert s.name == "Covered Put" and legs(s) == [(1, 750, -1, EXP)] and und(s) == [(GOOG, -100)]
    s = OptionStrategies.protective_put(CANON, 750, EXP)
    assert s.name == "Protective Put" and legs(s) == [(1, 750, 1, EXP)] and und(s) == [(GOOG, 100)]
    s = OptionStrategies.protective_collar(CANON, 760, 740, EXP)
    assert s.name == "Protective Collar" and legs(s) == [(0, 760, -1, EXP), (1, 740, 1, EXP)]
    assert und(s) == [(GOOG, 100)]
    s = OptionStrategies.conversion(CANON, 750, EXP)
    assert s.name == "Conversion" and legs(s) == [(0, 750, -1, EXP), (1, 750, 1, EXP)] and und(s) == [(GOOG, 100)]
    s = OptionStrategies.reverse_conversion(CANON, 750, EXP)
    assert s.name == "Reverse Conversion" and legs(s) == [(0, 750, 1, EXP), (1, 750, -1, EXP)]
    assert und(s) == [(GOOG, -100)]
    with pytest.raises(ValueError):
        OptionStrategies.protective_collar(CANON, 740, 760, EXP)


def test_naked_and_vertical_spreads():
    assert legs(OptionStrategies.naked_call(CANON, 750, EXP)) == [(0, 750, -1, EXP)]
    assert legs(OptionStrategies.naked_put(CANON, 750, EXP)) == [(1, 750, -1, EXP)]
    s = OptionStrategies.bear_call_spread(CANON, 740, 760, EXP)
    assert s.name == "Bear Call Spread" and legs(s) == [(0, 740, -1, EXP), (0, 760, 1, EXP)]
    s = OptionStrategies.bear_put_spread(CANON, 760, 740, EXP)
    assert s.name == "Bear Put Spread" and legs(s) == [(1, 760, 1, EXP), (1, 740, -1, EXP)]
    s = OptionStrategies.bull_call_spread(CANON, 740, 760, EXP)
    assert s.name == "Bull Call Spread" and legs(s) == [(0, 740, 1, EXP), (0, 760, -1, EXP)]
    s = OptionStrategies.bull_put_spread(CANON, 760, 740, EXP)
    assert s.name == "Bull Put Spread" and legs(s) == [(1, 760, -1, EXP), (1, 740, 1, EXP)]
    for bad in (lambda: OptionStrategies.bear_call_spread(CANON, 760, 740, EXP),
                lambda: OptionStrategies.bear_put_spread(CANON, 740, 760, EXP),
                lambda: OptionStrategies.bull_call_spread(CANON, 760, 760, EXP),
                lambda: OptionStrategies.bull_put_spread(CANON, 740, 760, EXP)):
        with pytest.raises(ValueError):
            bad()


def test_straddles_strangles():
    s = OptionStrategies.straddle(CANON, 750, EXP)
    assert s.name == "Straddle" and legs(s) == [(0, 750, 1, EXP), (1, 750, 1, EXP)]
    s = OptionStrategies.short_straddle(CANON, 750, EXP)
    assert s.name == "Short Straddle" and legs(s) == [(0, 750, -1, EXP), (1, 750, -1, EXP)]
    s = OptionStrategies.strangle(CANON, 760, 740, EXP)
    assert s.name == "Strangle" and legs(s) == [(0, 760, 1, EXP), (1, 740, 1, EXP)]
    s = OptionStrategies.short_strangle(CANON, 760, 740, EXP)
    assert s.name == "Short Strangle" and legs(s) == [(0, 760, -1, EXP), (1, 740, -1, EXP)]
    with pytest.raises(ValueError):
        OptionStrategies.strangle(CANON, 740, 760, EXP)


def test_butterflies():
    for f, name, right in ((OptionStrategies.call_butterfly, "Butterfly Call", 0),
                           (OptionStrategies.butterfly_call, "Butterfly Call", 0),
                           (OptionStrategies.put_butterfly, "Butterfly Put", 1),
                           (OptionStrategies.butterfly_put, "Butterfly Put", 1)):
        s = f(CANON, 760, 750, 740, EXP)
        assert s.name == name and legs(s) == [(right, 760, 1, EXP), (right, 750, -2, EXP), (right, 740, 1, EXP)]
    s = OptionStrategies.short_butterfly_call(CANON, 760, 750, 740, EXP)
    assert s.name == "Short Butterfly Call" and [l[2] for l in legs(s)] == [-1, 2, -1]
    s = OptionStrategies.short_butterfly_put(CANON, 760, 750, 740, EXP)
    assert s.name == "Short Butterfly Put" and [l[2] for l in legs(s)] == [-1, 2, -1]
    assert legs(OptionStrategies.call_butterfly(CANON, 10.45, 10.3, 10.15, EXP))[0][1] == 10.45   # float spacing
    with pytest.raises(ValueError):
        OptionStrategies.call_butterfly(CANON, 765, 750, 740, EXP)
    with pytest.raises(ValueError):
        OptionStrategies.put_butterfly(CANON, 740, 750, 760, EXP)


def test_calendar_spreads_and_jelly_roll():
    s = OptionStrategies.call_calendar_spread(CANON, 750, EXP, FAR)
    assert s.name == "Call Calendar Spread" and legs(s) == [(0, 750, -1, EXP), (0, 750, 1, FAR)]
    s = OptionStrategies.short_call_calendar_spread(CANON, 750, EXP, FAR)
    assert s.name == "Short Call Calendar Spread" and legs(s) == [(0, 750, 1, EXP), (0, 750, -1, FAR)]
    s = OptionStrategies.put_calendar_spread(CANON, 750, EXP, FAR)
    assert s.name == "Put Calendar Spread" and legs(s) == [(1, 750, -1, EXP), (1, 750, 1, FAR)]
    s = OptionStrategies.short_put_calendar_spread(CANON, 750, EXP, FAR)
    assert s.name == "Short Put Calendar Spread" and legs(s) == [(1, 750, 1, EXP), (1, 750, -1, FAR)]
    s = OptionStrategies.jelly_roll(CANON, 750, EXP, FAR)
    assert s.name == "Jelly Roll"
    assert legs(s) == [(0, 750, -1, EXP), (0, 750, 1, FAR), (1, 750, 1, EXP), (1, 750, -1, FAR)]
    s = OptionStrategies.short_jelly_roll(CANON, 750, EXP, FAR)
    assert s.name == "Short Jelly Roll" and [l[2] for l in legs(s)] == [1, -1, -1, 1]
    with pytest.raises(ValueError):
        OptionStrategies.call_calendar_spread(CANON, 750, FAR, EXP)
    with pytest.raises(ValueError):
        OptionStrategies.put_calendar_spread(CANON, 750, EXP, EXP)
    # date objects are accepted and normalised to datetimes
    assert legs(OptionStrategies.call_calendar_spread(CANON, 750, date(2016, 1, 15), date(2016, 2, 19)))[1][3] == FAR


def test_iron_condor_and_butterfly():
    s = OptionStrategies.iron_condor(CANON, 730, 740, 760, 770, EXP)
    assert s.name == "Iron Condor"
    assert legs(s) == [(1, 730, 1, EXP), (1, 740, -1, EXP), (0, 760, -1, EXP), (0, 770, 1, EXP)]
    s = OptionStrategies.short_iron_condor(CANON, 730, 740, 760, 770, EXP)
    assert s.name == "Short Iron Condor" and [l[2] for l in legs(s)] == [-1, 1, 1, -1]
    s = OptionStrategies.iron_butterfly(CANON, 740, 750, 760, EXP)
    assert s.name == "Iron Butterfly"
    assert legs(s) == [(1, 740, 1, EXP), (1, 750, -1, EXP), (0, 750, -1, EXP), (0, 760, 1, EXP)]
    s = OptionStrategies.short_iron_butterfly(CANON, 740, 750, 760, EXP)
    assert s.name == "Short Iron Butterfly" and [l[2] for l in legs(s)] == [-1, 1, 1, -1]
    with pytest.raises(ValueError):
        OptionStrategies.iron_condor(CANON, 740, 730, 760, 770, EXP)
    with pytest.raises(ValueError):
        OptionStrategies.iron_butterfly(CANON, 740, 750, 765, EXP)


def test_box_spreads():
    s = OptionStrategies.box_spread(CANON, 760, 740, EXP)
    assert s.name == "Box Spread"
    assert legs(s) == [(1, 760, 1, EXP), (1, 740, -1, EXP), (0, 740, 1, EXP), (0, 760, -1, EXP)]
    s = OptionStrategies.short_box_spread(CANON, 760, 740, EXP)
    assert s.name == "Short Box Spread" and [l[2] for l in legs(s)] == [-1, 1, -1, 1]
    with pytest.raises(ValueError):
        OptionStrategies.box_spread(CANON, 740, 760, EXP)


def test_ladders_and_backspreads():
    s = OptionStrategies.bear_call_ladder(CANON, 740, 750, 760, EXP)
    assert s.name == "Bear Call Ladder" and legs(s) == [(0, 740, -1, EXP), (0, 750, 1, EXP), (0, 760, 1, EXP)]
    s = OptionStrategies.bull_call_ladder(CANON, 740, 750, 760, EXP)
    assert s.name == "Bull Call Ladder" and [l[2] for l in legs(s)] == [1, -1, -1]
    s = OptionStrategies.bear_put_ladder(CANON, 760, 750, 740, EXP)
    assert s.name == "Bear Put Ladder" and legs(s) == [(1, 760, 1, EXP), (1, 750, -1, EXP), (1, 740, -1, EXP)]
    s = OptionStrategies.bull_put_ladder(CANON, 760, 750, 740, EXP)
    assert s.name == "Bull Put Ladder" and [l[2] for l in legs(s)] == [-1, 1, 1]
    s = OptionStrategies.call_backspread(CANON, 740, 760, EXP)
    assert s.name == "Call Backspread" and legs(s) == [(0, 740, -1, EXP), (0, 760, 2, EXP)]
    s = OptionStrategies.put_backspread(CANON, 760, 740, EXP)
    assert s.name == "Put Backspread" and legs(s) == [(1, 760, -1, EXP), (1, 740, 2, EXP)]
    assert [l[2] for l in legs(OptionStrategies.short_call_backspread(CANON, 740, 760, EXP))] == [1, -2]
    assert [l[2] for l in legs(OptionStrategies.short_put_backspread(CANON, 760, 740, EXP))] == [1, -2]
    for bad in (lambda: OptionStrategies.bear_call_ladder(CANON, 750, 740, 760, EXP),
                lambda: OptionStrategies.bear_put_ladder(CANON, 740, 750, 760, EXP),
                lambda: OptionStrategies.call_backspread(CANON, 760, 740, EXP),
                lambda: OptionStrategies.put_backspread(CANON, 740, 760, EXP)):
        with pytest.raises(ValueError):
            bad()


def test_validation_of_canonical_and_expiration():
    contract = Symbol.create_option(GOOG, Market.USA, OptionStyle.AMERICAN, C, 750, EXP)
    with pytest.raises(ValueError, match="canonicalOption must contain canonical option symbol"):
        OptionStrategies.straddle(contract, 750, EXP)
    with pytest.raises(ValueError, match="canonicalOption"):
        OptionStrategies.straddle(GOOG, 750, EXP)
    with pytest.raises(ValueError, match="expiration must contain expiration date"):
        OptionStrategies.naked_call(CANON, 750, datetime.max)
    with pytest.raises(ValueError):
        OptionStrategies.naked_call(CANON, 750, None)


def test_option_leg_symbols_and_resolution():
    s = OptionStrategies.iron_condor(CANON, 730, 740, 760, 770, EXP)
    expected = Symbol.create_option(GOOG, Market.USA, OptionStyle.AMERICAN, P, 730, EXP)
    assert s.option_legs[0].symbol == expected and s.option_legs[0].symbol.value == 'GOOG  160115P00730000'
    assert s.canonical_option == CANON and s.underlying == GOOG
    assert ost.resolve_legs(s, 2) == [(s.option_legs[0].symbol, 2), (s.option_legs[1].symbol, -2),
                                      (s.option_legs[2].symbol, -2), (s.option_legs[3].symbol, 2)]
    cc = OptionStrategies.covered_call(CANON, 750, EXP)
    assert ost.resolve_legs(cc, -1) == [(cc.option_legs[0].symbol, 1), (GOOG, -100)]
    combo = ost.strategy_to_legs(cc, 3)
    assert [(l.symbol, l.quantity) for l in combo] == [(cc.option_legs[0].symbol, -3), (GOOG, 300)]


def test_index_option_strategy_keeps_target_and_style():
    spx = Symbol.create('SPX', SecurityType.INDEX, Market.USA)
    spxw = Symbol.create_canonical_option(spx, 'SPXW', Market.USA)
    s = OptionStrategies.bull_call_spread(spxw, 3700, 3750, datetime(2021, 1, 8))
    sym = s.option_legs[0].symbol
    assert sym.value == 'SPXW  210108C03700000' and sym.id.option_style == OptionStyle.EUROPEAN
    assert sym.canonical == spxw and sym.underlying == spx
    vix = Symbol.create('VIX', SecurityType.INDEX, Market.USA)
    vixw = Symbol.create_canonical_option(vix, 'VIXW', Market.USA)
    s = OptionStrategies.bear_call_spread(vixw, 20, 25, datetime(2020, 2, 5))
    assert s.option_legs[1].symbol.value == 'VIXW  200205C00025000'


def test_future_option_covered_call_uses_one_future():
    gc = Symbol.create_future('GC', 'comex', datetime(2020, 12, 29))
    og = Symbol.create_canonical_option(gc)
    s = OptionStrategies.covered_call(og, 1900, datetime(2020, 11, 24))
    assert und(s) == [(gc, 1)] and s.option_legs[0].symbol.value.startswith('OG')
    ost.set_contract_multiplier_provider(lambda c: 50)
    try:
        assert und(OptionStrategies.covered_call(og, 1900, datetime(2020, 11, 24))) == [(gc, 50)]
    finally:
        ost.set_contract_multiplier_provider(None)


def test_option_strategy_create_and_pascal_case():
    a = Symbol.create_option(GOOG, Market.USA, OptionStyle.AMERICAN, C, 750, EXP)
    b = Symbol.create_option(GOOG, Market.USA, OptionStyle.AMERICAN, C, 760, EXP)
    s = OptionStrategy.create("Custom", [ost.OptionLegData.create(1, a), ost.OptionLegData.create(-1, b),
                                          ost.UnderlyingLegData.create(10, GOOG)])
    assert s.Name == "Custom" and s.CanonicalOption == CANON and s.Underlying == GOOG
    assert [(l.Symbol, l.Quantity, l.Strike, l.Right, l.Expiration) for l in s.OptionLegs] == [
        (a, 1, 750, C, EXP), (b, -1, 760, C, EXP)]
    assert [(l.symbol, l.quantity) for l in s.UnderlyingLegs] == [(GOOG, 10)]
    assert ost.UnderlyingLegData.create(5, 101.0).order_price == 101.0
    with pytest.raises(ValueError):
        OptionStrategy.create("Bad", [Leg.create(GOOG, 1)])
    assert OptionStrategies.IronCondor is OptionStrategies.iron_condor
    s = OptionStrategies.BullPutSpread(CANON, 760, 740, EXP)
    assert s.Name == "Bull Put Spread"
    assert 'Strike: 760' in repr(s.option_legs[0])
