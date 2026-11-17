# ============================================================================
# Fincept Terminal - Strategy Engine Option Strategies
# LEAN's OptionStrategies factory (Common/Securities/Option/OptionStrategies.cs),
# OptionStrategy with its option/underlying legs, and Orders.Leg.
# Argument validation raises ValueError (LEAN: ArgumentException) with LEAN's
# messages.
# ============================================================================

from datetime import date, datetime

from .enums import OptionRight, SecurityType
from .option_chain import contract_symbol
from .types import Symbol

_OPTION_TYPES = (SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION)


def _to_dt(x):
    if isinstance(x, datetime):
        return x
    if isinstance(x, date):
        return datetime(x.year, x.month, x.day)
    return x


class Leg:
    """LEAN Orders.Leg: a symbol, a quantity (per unit of the combo) and an optional limit price."""

    __slots__ = ('symbol', 'quantity', 'order_price')

    def __init__(self, symbol=None, quantity: int = 0, order_price=None):
        self.symbol = symbol
        self.quantity = int(quantity)
        self.order_price = order_price

    @staticmethod
    def create(symbol, quantity: int, order_price=None) -> 'Leg':
        return Leg(symbol, quantity, order_price)

    Create = create
    Symbol = property(lambda self: self.symbol)
    Quantity = property(lambda self: self.quantity)
    OrderPrice = property(lambda self: self.order_price)

    def __repr__(self):
        return f"Leg({self.symbol}, {self.quantity}{'' if self.order_price is None else f', @{self.order_price}'})"


class OptionLegData(Leg):
    """An option leg: right, strike, expiration and quantity; `symbol` is filled by OptionStrategy."""

    __slots__ = ('right', 'strike', 'expiration')

    def __init__(self, right=OptionRight.CALL, strike: float = 0.0, expiration=None, quantity: int = 0,
                 symbol=None, order_price=None):
        super().__init__(symbol, quantity, order_price)
        self.right = right
        self.strike = float(strike)
        self.expiration = _to_dt(expiration)

    @staticmethod
    def create(quantity: int, symbol, order_price=None) -> 'OptionLegData':
        return OptionLegData(symbol.id.option_right, symbol.id.strike_price, symbol.id.date, quantity, symbol,
                             order_price)

    Create = create
    Right = property(lambda self: self.right)
    Strike = property(lambda self: self.strike)
    Expiration = property(lambda self: self.expiration)

    def __repr__(self):
        exp = self.expiration.strftime('%Y%m%d') if self.expiration is not None else '?'
        return f"Leg: {self.quantity}. Right: {OptionRight(int(self.right)).name.title()}. Strike: {self.strike:g}. Expiration: {exp}"


class UnderlyingLegData(Leg):
    """An underlying leg (shares / future contracts)."""

    __slots__ = ()

    @staticmethod
    def create(quantity: int, symbol=None, order_price=None) -> 'UnderlyingLegData':
        """create(quantity, symbol[, order_price]) or create(quantity[, order_price]) (LEAN overloads)."""
        if symbol is not None and not isinstance(symbol, (Symbol, str)):
            symbol, order_price = None, symbol
        return UnderlyingLegData(symbol, quantity, order_price)

    Create = create

    def __repr__(self):
        return f"Leg: {self.quantity}. {self.symbol}" if self.symbol is not None else ''


class OptionStrategy:
    """LEAN OptionStrategy: name, canonical option, underlying, option legs and underlying legs."""

    OptionLegData = OptionLegData
    UnderlyingLegData = UnderlyingLegData

    def __init__(self, name: str = None, canonical_option=None, option_legs=None, underlying_legs=None):
        self.name = name
        self.canonical_option = canonical_option
        self.underlying = canonical_option.underlying if canonical_option is not None else None
        self.option_legs = list(option_legs or [])
        self.underlying_legs = list(underlying_legs or [])
        if name is not None:
            self.set_symbols()

    def set_symbols(self):
        """Fill missing canonical/underlying from the legs and the option legs' contract Symbols."""
        if self.canonical_option is None:
            if self.underlying is None:
                leg = self.underlying_legs[0] if self.underlying_legs else None
                if leg is None or leg.symbol is None:
                    return
                self.underlying = leg.symbol
            self.canonical_option = Symbol.create_canonical_option(self.underlying)
        for leg in self.option_legs:
            if leg.symbol is None:
                leg.symbol = contract_symbol(self.canonical_option, leg.right, leg.strike, leg.expiration,
                                             self.underlying)

    @staticmethod
    def create(name: str, legs) -> 'OptionStrategy':
        underlying_legs, option_legs = [], []
        canonical = None
        for leg in legs:
            if isinstance(leg, UnderlyingLegData):
                underlying_legs.append(leg)
            elif isinstance(leg, OptionLegData):
                option_legs.append(leg)
                if canonical is None:
                    canonical = leg.symbol.canonical
            else:
                raise ValueError(f"Invalid leg type: {type(leg).__name__}")
        return OptionStrategy(name, canonical, option_legs, underlying_legs)

    Create = create
    SetSymbols = set_symbols
    Name = property(lambda self: self.name)
    CanonicalOption = property(lambda self: self.canonical_option)
    Underlying = property(lambda self: self.underlying)
    OptionLegs = property(lambda self: self.option_legs)
    UnderlyingLegs = property(lambda self: self.underlying_legs)

    def __repr__(self):
        return f"OptionStrategy({self.name}, {self.canonical_option}, {len(self.option_legs)} option legs, " \
               f"{len(self.underlying_legs)} underlying legs)"


def resolve_legs(strategy: OptionStrategy, quantity: int = 1) -> list:
    """[(Symbol, quantity)] of the strategy bought `quantity` times (negative sells): option legs as
    contract Symbols (Symbol.create_option from the canonical's underlying, target name and style),
    then underlying legs."""
    strategy.set_symbols()
    out = []
    for leg in strategy.option_legs:
        out.append((leg.symbol, leg.quantity * quantity))
    for leg in strategy.underlying_legs:
        sym = leg.symbol if leg.symbol is not None else strategy.underlying
        out.append((sym, leg.quantity * quantity))
    return out


def strategy_to_legs(strategy: OptionStrategy, quantity: int = 1) -> list:
    """The strategy as combo-order Legs (Leg.create(symbol, leg quantity * quantity, leg order price))."""
    resolved = resolve_legs(strategy, quantity)
    prices = [leg.order_price for leg in strategy.option_legs] + [leg.order_price for leg in strategy.underlying_legs]
    return [Leg.create(sym, qty, p) for (sym, qty), p in zip(resolved, prices)]


# ---------------------------------------------------------------------------
# Contract multipliers: "1 lot of the underlying" in covered/protective strategies
# ---------------------------------------------------------------------------

def _default_contract_multiplier(canonical) -> int:
    """Equity and index options deliver 100 units. A futures option delivers one future contract, so its
    underlying lot is 1 (LEAN uses the symbol-properties multiplier of the futures option, which is the
    future's point value; that is not a number of futures)."""
    if canonical.security_type == SecurityType.FUTURE_OPTION:
        return 1
    return 100


_multiplier_provider = _default_contract_multiplier


def set_contract_multiplier_provider(func):
    """Override how many underlying units one option delivers: func(canonical_option) -> int; None resets."""
    global _multiplier_provider
    _multiplier_provider = func or _default_contract_multiplier


def _multiplier(canonical) -> int:
    return int(_multiplier_provider(canonical))


# ---------------------------------------------------------------------------
# Validation (LEAN: CheckCanonicalOptionSymbol / CheckExpirationDate)
# ---------------------------------------------------------------------------

def _check_canonical(canonical, strategy_name):
    if (not isinstance(canonical, Symbol) or not canonical.has_underlying
            or canonical.id.strike_price != 0.0):
        raise ValueError(f"{strategy_name}: canonicalOption must contain canonical option symbol")


def _check_expiration(expiration, strategy_name, parameter_name):
    if expiration is None or expiration == datetime.max or expiration == datetime.min \
            or not isinstance(expiration, (datetime, date)):
        raise ValueError(f"{strategy_name}: expiration must contain expiration date ({parameter_name})")


def _eq(a, b) -> bool:
    return abs(float(a) - float(b)) <= 1e-9 * max(1.0, abs(float(a)), abs(float(b)))


def _leg(right, strike, quantity, expiration):
    return OptionLegData(right, strike, expiration, quantity)


def _invert(strategy: OptionStrategy, name: str) -> OptionStrategy:
    strategy.name = name
    for leg in strategy.option_legs + strategy.underlying_legs:
        leg.quantity *= -1
    return strategy


_C, _P = OptionRight.CALL, OptionRight.PUT


class OptionStrategies:
    """LEAN OptionStrategies: factory methods of the popular option strategies (snake_case and PascalCase)."""

    # ---- underlying + option ----
    @staticmethod
    def covered_call(canonical_option, strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "CoveredCall")
        _check_expiration(expiration, "CoveredCall", "expiration")
        exp = _to_dt(expiration)
        return OptionStrategy("Covered Call", canonical_option, [_leg(_C, strike, -1, exp)],
                              [UnderlyingLegData(canonical_option.underlying, _multiplier(canonical_option))])

    @staticmethod
    def protective_call(canonical_option, strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.covered_call(canonical_option, strike, expiration), "Protective Call")

    @staticmethod
    def covered_put(canonical_option, strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "CoveredPut")
        _check_expiration(expiration, "CoveredPut", "expiration")
        exp = _to_dt(expiration)
        return OptionStrategy("Covered Put", canonical_option, [_leg(_P, strike, -1, exp)],
                              [UnderlyingLegData(canonical_option.underlying, -_multiplier(canonical_option))])

    @staticmethod
    def protective_put(canonical_option, strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.covered_put(canonical_option, strike, expiration), "Protective Put")

    @staticmethod
    def protective_collar(canonical_option, call_strike, put_strike, expiration) -> OptionStrategy:
        if call_strike < put_strike:
            raise ValueError("ProtectiveCollar: callStrike must be greater than putStrike (callStrike, putStrike)")
        covered_call = OptionStrategies.covered_call(canonical_option, call_strike, expiration)
        protective_put = OptionStrategies.protective_put(canonical_option, put_strike, expiration)
        return OptionStrategy("Protective Collar", canonical_option,
                              covered_call.option_legs + protective_put.option_legs, covered_call.underlying_legs)

    @staticmethod
    def conversion(canonical_option, strike, expiration) -> OptionStrategy:
        s = OptionStrategies.protective_collar(canonical_option, strike, strike, expiration)
        s.name = "Conversion"
        return s

    @staticmethod
    def reverse_conversion(canonical_option, strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.conversion(canonical_option, strike, expiration), "Reverse Conversion")

    # ---- single option ----
    @staticmethod
    def naked_call(canonical_option, strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "NakedCall")
        _check_expiration(expiration, "NakedCall", "expiration")
        return OptionStrategy("Naked Call", canonical_option, [_leg(_C, strike, -1, _to_dt(expiration))])

    @staticmethod
    def naked_put(canonical_option, strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "NakedPut")
        _check_expiration(expiration, "NakedPut", "expiration")
        return OptionStrategy("Naked Put", canonical_option, [_leg(_P, strike, -1, _to_dt(expiration))])

    # ---- vertical spreads ----
    @staticmethod
    def bear_call_spread(canonical_option, leg1_strike, leg2_strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "BearCallSpread")
        _check_expiration(expiration, "BearCallSpread", "expiration")
        if leg1_strike >= leg2_strike:
            raise ValueError("BearCallSpread: leg1Strike must be less than leg2Strike (leg1Strike, leg2Strike)")
        exp = _to_dt(expiration)
        return OptionStrategy("Bear Call Spread", canonical_option,
                              [_leg(_C, leg1_strike, -1, exp), _leg(_C, leg2_strike, 1, exp)])

    @staticmethod
    def bear_put_spread(canonical_option, leg1_strike, leg2_strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "BearPutSpread")
        _check_expiration(expiration, "BearPutSpread", "expiration")
        if leg1_strike <= leg2_strike:
            raise ValueError("BearPutSpread: leg1Strike must be greater than leg2Strike (leg1Strike, leg2Strike)")
        exp = _to_dt(expiration)
        return OptionStrategy("Bear Put Spread", canonical_option,
                              [_leg(_P, leg1_strike, 1, exp), _leg(_P, leg2_strike, -1, exp)])

    @staticmethod
    def bull_call_spread(canonical_option, leg1_strike, leg2_strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "BullCallSpread")
        _check_expiration(expiration, "BullCallSpread", "expiration")
        if leg1_strike >= leg2_strike:
            raise ValueError("BullCallSpread: leg1Strike must be less than leg2Strike (leg1Strike, leg2Strike)")
        exp = _to_dt(expiration)
        return OptionStrategy("Bull Call Spread", canonical_option,
                              [_leg(_C, leg1_strike, 1, exp), _leg(_C, leg2_strike, -1, exp)])

    @staticmethod
    def bull_put_spread(canonical_option, leg1_strike, leg2_strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "BullPutSpread")
        _check_expiration(expiration, "BullPutSpread", "expiration")
        if leg1_strike <= leg2_strike:
            raise ValueError("BullPutSpread: leg1Strike must be greater than leg2Strike (leg1Strike, leg2Strike)")
        exp = _to_dt(expiration)
        return OptionStrategy("Bull Put Spread", canonical_option,
                              [_leg(_P, leg1_strike, -1, exp), _leg(_P, leg2_strike, 1, exp)])

    # ---- straddles / strangles ----
    @staticmethod
    def straddle(canonical_option, strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "Straddle")
        _check_expiration(expiration, "Straddle", "expiration")
        exp = _to_dt(expiration)
        return OptionStrategy("Straddle", canonical_option, [_leg(_C, strike, 1, exp), _leg(_P, strike, 1, exp)])

    @staticmethod
    def short_straddle(canonical_option, strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.straddle(canonical_option, strike, expiration), "Short Straddle")

    @staticmethod
    def strangle(canonical_option, call_leg_strike, put_leg_strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "Strangle")
        _check_expiration(expiration, "Strangle", "expiration")
        if call_leg_strike <= put_leg_strike:
            raise ValueError("Strangle: callLegStrike must be greater than putLegStrike (callLegStrike, putLegStrike)")
        exp = _to_dt(expiration)
        return OptionStrategy("Strangle", canonical_option,
                              [_leg(_C, call_leg_strike, 1, exp), _leg(_P, put_leg_strike, 1, exp)])

    @staticmethod
    def short_strangle(canonical_option, call_leg_strike, put_leg_strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.strangle(canonical_option, call_leg_strike, put_leg_strike, expiration),
                       "Short Strangle")

    # ---- butterflies ----
    @staticmethod
    def _butterfly(right, name, check_name, canonical_option, higher, middle, lower, expiration):
        _check_canonical(canonical_option, check_name)
        _check_expiration(expiration, check_name, "expiration")
        if higher <= middle or lower >= middle or not _eq(higher - middle, middle - lower):
            label = "ButterflyCall" if right == _C else "ButterflyPut"
            raise ValueError(f"{label}: upper and lower strikes must both be equidistant from the middle strike "
                             f"(higherStrike, middleStrike, lowerStrike)")
        exp = _to_dt(expiration)
        return OptionStrategy(name, canonical_option, [_leg(right, higher, 1, exp), _leg(right, middle, -2, exp),
                                                       _leg(right, lower, 1, exp)])

    @staticmethod
    def call_butterfly(canonical_option, higher_strike, middle_strike, lower_strike, expiration) -> OptionStrategy:
        return OptionStrategies._butterfly(_C, "Butterfly Call", "CallButterfly", canonical_option, higher_strike,
                                           middle_strike, lower_strike, expiration)

    @staticmethod
    def butterfly_call(canonical_option, higher_strike, middle_strike, lower_strike, expiration) -> OptionStrategy:
        return OptionStrategies.call_butterfly(canonical_option, higher_strike, middle_strike, lower_strike,
                                               expiration)

    @staticmethod
    def short_butterfly_call(canonical_option, higher_strike, middle_strike, lower_strike,
                             expiration) -> OptionStrategy:
        return _invert(OptionStrategies.butterfly_call(canonical_option, higher_strike, middle_strike, lower_strike,
                                                       expiration), "Short Butterfly Call")

    @staticmethod
    def put_butterfly(canonical_option, higher_strike, middle_strike, lower_strike, expiration) -> OptionStrategy:
        return OptionStrategies._butterfly(_P, "Butterfly Put", "PutButterfly", canonical_option, higher_strike,
                                           middle_strike, lower_strike, expiration)

    @staticmethod
    def butterfly_put(canonical_option, higher_strike, middle_strike, lower_strike, expiration) -> OptionStrategy:
        return OptionStrategies.put_butterfly(canonical_option, higher_strike, middle_strike, lower_strike,
                                              expiration)

    @staticmethod
    def short_butterfly_put(canonical_option, higher_strike, middle_strike, lower_strike,
                            expiration) -> OptionStrategy:
        return _invert(OptionStrategies.butterfly_put(canonical_option, higher_strike, middle_strike, lower_strike,
                                                      expiration), "Short Butterfly Put")

    # ---- calendar spreads ----
    @staticmethod
    def _calendar(right, name, check_name, canonical_option, strike, near_expiration, far_expiration):
        _check_canonical(canonical_option, check_name)
        _check_expiration(near_expiration, check_name, "nearExpiration")
        _check_expiration(far_expiration, check_name, "farExpiration")
        near, far = _to_dt(near_expiration), _to_dt(far_expiration)
        if near >= far:
            raise ValueError(f"{check_name}: near expiration must be less than far expiration "
                             f"(nearExpiration, farExpiration)")
        return OptionStrategy(name, canonical_option, [_leg(right, strike, -1, near), _leg(right, strike, 1, far)])

    @staticmethod
    def call_calendar_spread(canonical_option, strike, near_expiration, far_expiration) -> OptionStrategy:
        return OptionStrategies._calendar(_C, "Call Calendar Spread", "CallCalendarSpread", canonical_option, strike,
                                          near_expiration, far_expiration)

    @staticmethod
    def short_call_calendar_spread(canonical_option, strike, near_expiration, far_expiration) -> OptionStrategy:
        return _invert(OptionStrategies.call_calendar_spread(canonical_option, strike, near_expiration,
                                                             far_expiration), "Short Call Calendar Spread")

    @staticmethod
    def put_calendar_spread(canonical_option, strike, near_expiration, far_expiration) -> OptionStrategy:
        return OptionStrategies._calendar(_P, "Put Calendar Spread", "PutCalendarSpread", canonical_option, strike,
                                          near_expiration, far_expiration)

    @staticmethod
    def short_put_calendar_spread(canonical_option, strike, near_expiration, far_expiration) -> OptionStrategy:
        return _invert(OptionStrategies.put_calendar_spread(canonical_option, strike, near_expiration,
                                                            far_expiration), "Short Put Calendar Spread")

    # ---- iron condors / butterflies ----
    @staticmethod
    def iron_condor(canonical_option, long_put_strike, short_put_strike, short_call_strike, long_call_strike,
                    expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "IronCondor")
        _check_expiration(expiration, "IronCondor", "expiration")
        if long_put_strike >= short_put_strike or short_put_strike > short_call_strike \
                or short_call_strike >= long_call_strike:
            raise ValueError("IronCondor: strike prices must be in ascending order "
                             "(longPutStrike, shortPutStrike, shortCallStrike, longCallStrike)")
        exp = _to_dt(expiration)
        return OptionStrategy("Iron Condor", canonical_option, [
            _leg(_P, long_put_strike, 1, exp), _leg(_P, short_put_strike, -1, exp),
            _leg(_C, short_call_strike, -1, exp), _leg(_C, long_call_strike, 1, exp)])

    @staticmethod
    def short_iron_condor(canonical_option, short_put_strike, long_put_strike, long_call_strike, short_call_strike,
                          expiration) -> OptionStrategy:
        return _invert(OptionStrategies.iron_condor(canonical_option, short_put_strike, long_put_strike,
                                                    long_call_strike, short_call_strike, expiration),
                       "Short Iron Condor")

    @staticmethod
    def iron_butterfly(canonical_option, otm_put_strike, atm_strike, otm_call_strike, expiration) -> OptionStrategy:
        if not _eq(atm_strike - otm_put_strike, otm_call_strike - atm_strike):
            raise ValueError("IronButterfly: intervals between exercise prices must be equal")
        s = OptionStrategies.iron_condor(canonical_option, otm_put_strike, atm_strike, atm_strike, otm_call_strike,
                                         expiration)
        s.name = "Iron Butterfly"
        return s

    @staticmethod
    def short_iron_butterfly(canonical_option, otm_put_strike, atm_strike, otm_call_strike,
                             expiration) -> OptionStrategy:
        return _invert(OptionStrategies.iron_butterfly(canonical_option, otm_put_strike, atm_strike, otm_call_strike,
                                                       expiration), "Short Iron Butterfly")

    # ---- boxes / jelly rolls ----
    @staticmethod
    def box_spread(canonical_option, higher_strike, lower_strike, expiration) -> OptionStrategy:
        if higher_strike <= lower_strike:
            raise ValueError("BoxSpread: strike prices must be in descending order, higherStrike, lowerStrike")
        bear_put = OptionStrategies.bear_put_spread(canonical_option, higher_strike, lower_strike, expiration)
        bull_call = OptionStrategies.bull_call_spread(canonical_option, lower_strike, higher_strike, expiration)
        return OptionStrategy("Box Spread", canonical_option, bear_put.option_legs + bull_call.option_legs)

    @staticmethod
    def short_box_spread(canonical_option, higher_strike, lower_strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.box_spread(canonical_option, higher_strike, lower_strike, expiration),
                       "Short Box Spread")

    @staticmethod
    def jelly_roll(canonical_option, strike, near_expiration, far_expiration) -> OptionStrategy:
        call_cal = OptionStrategies.call_calendar_spread(canonical_option, strike, near_expiration, far_expiration)
        short_put_cal = OptionStrategies.short_put_calendar_spread(canonical_option, strike, near_expiration,
                                                                   far_expiration)
        return OptionStrategy("Jelly Roll", canonical_option, call_cal.option_legs + short_put_cal.option_legs)

    @staticmethod
    def short_jelly_roll(canonical_option, strike, near_expiration, far_expiration) -> OptionStrategy:
        return _invert(OptionStrategies.jelly_roll(canonical_option, strike, near_expiration, far_expiration),
                       "Short Jelly Roll")

    # ---- ladders / backspreads ----
    @staticmethod
    def bear_call_ladder(canonical_option, lower_strike, middle_strike, higher_strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "BearCallLadder")
        _check_expiration(expiration, "BearCallLadder", "expiration")
        if lower_strike >= middle_strike or lower_strike >= higher_strike or middle_strike >= higher_strike:
            raise ValueError("BearCallLadder: strike prices must be in ascending order "
                             "(lowerStrike, middleStrike, higherStrike)")
        exp = _to_dt(expiration)
        return OptionStrategy("Bear Call Ladder", canonical_option, [
            _leg(_C, lower_strike, -1, exp), _leg(_C, middle_strike, 1, exp), _leg(_C, higher_strike, 1, exp)])

    @staticmethod
    def bear_put_ladder(canonical_option, higher_strike, middle_strike, lower_strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "BearPutLadder")
        _check_expiration(expiration, "BearPutLadder", "expiration")
        if higher_strike <= middle_strike or higher_strike <= lower_strike or middle_strike <= lower_strike:
            raise ValueError("BearPutLadder: strike prices must be in descending order "
                             "(higherStrike, middleStrike, lowerStrike)")
        exp = _to_dt(expiration)
        return OptionStrategy("Bear Put Ladder", canonical_option, [
            _leg(_P, higher_strike, 1, exp), _leg(_P, middle_strike, -1, exp), _leg(_P, lower_strike, -1, exp)])

    @staticmethod
    def bull_call_ladder(canonical_option, lower_strike, middle_strike, higher_strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.bear_call_ladder(canonical_option, lower_strike, middle_strike, higher_strike,
                                                         expiration), "Bull Call Ladder")

    @staticmethod
    def bull_put_ladder(canonical_option, higher_strike, middle_strike, lower_strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.bear_put_ladder(canonical_option, higher_strike, middle_strike, lower_strike,
                                                        expiration), "Bull Put Ladder")

    @staticmethod
    def call_backspread(canonical_option, lower_strike, higher_strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "CallBackspread")
        _check_expiration(expiration, "CallBackspread", "expiration")
        if lower_strike >= higher_strike:
            raise ValueError("CallBackspread: strike prices must be in ascending order, lowerStrike, higherStrike")
        exp = _to_dt(expiration)
        return OptionStrategy("Call Backspread", canonical_option,
                              [_leg(_C, lower_strike, -1, exp), _leg(_C, higher_strike, 2, exp)])

    @staticmethod
    def put_backspread(canonical_option, higher_strike, lower_strike, expiration) -> OptionStrategy:
        _check_canonical(canonical_option, "PutBackspread")
        _check_expiration(expiration, "PutBackspread", "expiration")
        if higher_strike <= lower_strike:
            raise ValueError("PutBackspread: strike prices must be in descending order, higherStrike, lowerStrike")
        exp = _to_dt(expiration)
        return OptionStrategy("Put Backspread", canonical_option,
                              [_leg(_P, higher_strike, -1, exp), _leg(_P, lower_strike, 2, exp)])

    @staticmethod
    def short_call_backspread(canonical_option, lower_strike, higher_strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.call_backspread(canonical_option, lower_strike, higher_strike, expiration),
                       "Short Call Backspread")

    @staticmethod
    def short_put_backspread(canonical_option, higher_strike, lower_strike, expiration) -> OptionStrategy:
        return _invert(OptionStrategies.put_backspread(canonical_option, higher_strike, lower_strike, expiration),
                       "Short Put Backspread")


FACTORY_METHODS = (
    'covered_call', 'protective_call', 'covered_put', 'protective_put', 'protective_collar', 'conversion',
    'reverse_conversion', 'naked_call', 'naked_put', 'bear_call_spread', 'bear_put_spread', 'bull_call_spread',
    'bull_put_spread', 'straddle', 'short_straddle', 'strangle', 'short_strangle', 'call_butterfly',
    'butterfly_call', 'short_butterfly_call', 'put_butterfly', 'butterfly_put', 'short_butterfly_put',
    'call_calendar_spread', 'short_call_calendar_spread', 'put_calendar_spread', 'short_put_calendar_spread',
    'iron_condor', 'short_iron_condor', 'iron_butterfly', 'short_iron_butterfly', 'box_spread', 'short_box_spread',
    'jelly_roll', 'short_jelly_roll', 'bear_call_ladder', 'bear_put_ladder', 'bull_call_ladder', 'bull_put_ladder',
    'call_backspread', 'put_backspread', 'short_call_backspread', 'short_put_backspread')

for _n in FACTORY_METHODS:
    setattr(OptionStrategies, ''.join(w.capitalize() for w in _n.split('_')), OptionStrategies.__dict__[_n])
    # LEAN's Python API also exposes them upper-case (OptionStrategies.STRADDLE in LEAN's own examples)
    setattr(OptionStrategies, _n.upper(), OptionStrategies.__dict__[_n])
del _n
