# ============================================================================
# Fincept Terminal - Strategy Engine: futures contracts, chains and continuous mapping
#
# Free data has only one series per root: Yahoo's continuous front-month ROOT=F. Individual
# contracts are derived from it:
#   * listed expiries follow each root's exchange calendar (quarterly third Fridays for
#     equity indices, CL's 25th-of-prior-month rule, ...; table below);
#   * contract price on day t = continuous price(t) x exp(r(t) x (T - T_front(t)) / 365):
#     the same price path as the continuous series, plus a cost-of-carry spread at the
#     3-month T-bill rate (^IRX) for contracts expiring after the current front contract;
#   * no volume or open interest (none is available per contract): they are 0.
# Every result that uses a derived contract carries the disclosure text below.
#
# Contract multipliers and initial margins come from a small table of exchange specs
# (margin as a fraction of notional, a simplification of the exchanges' fixed dollar
# margins). Futures are margined: fills move no cash except fees; profit is realised into
# cash when a position is reduced (portfolio.py).
#
# A continuous contract (add_future) maps to one contract at a time (security.mapped) by
# DataMappingMode: LAST_TRADING_DAY (roll on the expiry), FIRST_DAY_MONTH (roll at the start
# of the expiry month), OPEN_INTEREST / OPEN_INTEREST_ANNUAL (approximated: roll 5 trading
# days before expiry); contract_depth_offset picks a later contract. A roll produces a
# SymbolChangedEvent. Expired contracts are delisted: a WARNING the day before, DELISTED on
# the expiry day, and open positions are settled at the final price.
# ============================================================================

import math
from datetime import date, datetime, timedelta
from typing import Dict, List, Optional

from .enums import SecurityType, OrderType, OrderStatus
from .errors import not_supported
from .session import SessionExtension
from .types import Symbol, SymbolDict, TradeBar, DEFAULT_DATE

DISCLOSURE = ("Individual futures contracts are derived from Yahoo's continuous front-month series (same price "
              "path; later expiries add a cost-of-carry spread at the ^IRX T-bill rate); contract volume and open "
              "interest are not available (0); open-interest roll dates are approximated as 5 trading days before "
              "expiry; margins are a fixed fraction of notional from a table of exchange specs.")

_MONTHS = 'FGHJKMNQUVXZ'


def _cycle(codes: str):
    return [_MONTHS.index(c) + 1 for c in codes]


ALL = 'FGHJKMNQUVXZ'

# root: (description, multiplier, initial margin / notional, contract months, expiry rule, contracts listed)
SPECS = {
    'ES': ('E-mini S&P 500', 50.0, 0.06, 'HMUZ', 'third_friday', 5),
    'MES': ('Micro E-mini S&P 500', 5.0, 0.06, 'HMUZ', 'third_friday', 5),
    'NQ': ('E-mini Nasdaq-100', 20.0, 0.07, 'HMUZ', 'third_friday', 5),
    'MNQ': ('Micro E-mini Nasdaq-100', 2.0, 0.07, 'HMUZ', 'third_friday', 5),
    'YM': ('E-mini Dow', 5.0, 0.06, 'HMUZ', 'third_friday', 5),
    'MYM': ('Micro E-mini Dow', 0.5, 0.06, 'HMUZ', 'third_friday', 5),
    'RTY': ('E-mini Russell 2000', 50.0, 0.07, 'HMUZ', 'third_friday', 5),
    'M2K': ('Micro E-mini Russell 2000', 5.0, 0.07, 'HMUZ', 'third_friday', 5),
    'GC': ('Gold', 100.0, 0.07, 'GJMQVZ', 'third_last_business_day', 6),
    'MGC': ('Micro Gold', 10.0, 0.07, 'GJMQVZ', 'third_last_business_day', 6),
    'SI': ('Silver', 5000.0, 0.09, 'HKNUZ', 'third_last_business_day', 5),
    'SIL': ('Micro Silver', 1000.0, 0.09, 'HKNUZ', 'third_last_business_day', 5),
    'HG': ('Copper', 25000.0, 0.08, 'HKNUZ', 'third_last_business_day', 5),
    'PL': ('Platinum', 50.0, 0.08, 'FJNV', 'third_last_business_day', 4),
    'PA': ('Palladium', 100.0, 0.10, 'HMUZ', 'third_last_business_day', 4),
    'CL': ('Crude Oil WTI', 1000.0, 0.10, ALL, 'cl', 12),
    'MCL': ('Micro Crude Oil WTI', 100.0, 0.10, ALL, 'cl', 12),
    'BZ': ('Brent Crude', 1000.0, 0.10, ALL, 'last_business_day_two_months_prior', 12),
    'NG': ('Natural Gas', 10000.0, 0.12, ALL, 'three_business_days_before_month', 12),
    'HO': ('Heating Oil', 42000.0, 0.10, ALL, 'last_business_day_prior_month', 12),
    'RB': ('RBOB Gasoline', 42000.0, 0.10, ALL, 'last_business_day_prior_month', 12),
    'ZC': ('Corn', 50.0, 0.06, 'HKNUZ', 'business_day_before_15th', 5),
    'ZS': ('Soybeans', 50.0, 0.06, 'FHKNQUX', 'business_day_before_15th', 7),
    'ZW': ('Chicago Wheat', 50.0, 0.07, 'HKNUZ', 'business_day_before_15th', 5),
    'KE': ('KC HRW Wheat', 50.0, 0.07, 'HKNUZ', 'business_day_before_15th', 5),
    'ZM': ('Soybean Meal', 100.0, 0.06, 'FHKNQUVZ', 'business_day_before_15th', 8),
    'ZL': ('Soybean Oil', 600.0, 0.06, 'FHKNQUVZ', 'business_day_before_15th', 8),
    'ZO': ('Oats', 50.0, 0.08, 'HKNUZ', 'business_day_before_15th', 5),
    'ZN': ('10-Year T-Note', 1000.0, 0.025, 'HMUZ', 'seventh_business_day_before_last', 3),
    'ZB': ('30-Year T-Bond', 1000.0, 0.04, 'HMUZ', 'seventh_business_day_before_last', 3),
    'UB': ('Ultra T-Bond', 1000.0, 0.05, 'HMUZ', 'seventh_business_day_before_last', 3),
    'ZF': ('5-Year T-Note', 1000.0, 0.015, 'HMUZ', 'last_business_day', 3),
    'ZT': ('2-Year T-Note', 2000.0, 0.008, 'HMUZ', 'last_business_day', 3),
    '6E': ('Euro FX', 125000.0, 0.03, 'HMUZ', 'two_business_days_before_third_wednesday', 4),
    '6B': ('British Pound', 62500.0, 0.03, 'HMUZ', 'two_business_days_before_third_wednesday', 4),
    '6J': ('Japanese Yen', 12500000.0, 0.03, 'HMUZ', 'two_business_days_before_third_wednesday', 4),
    '6A': ('Australian Dollar', 100000.0, 0.04, 'HMUZ', 'two_business_days_before_third_wednesday', 4),
    '6C': ('Canadian Dollar', 100000.0, 0.03, 'HMUZ', 'business_day_before_third_wednesday', 4),
    '6S': ('Swiss Franc', 125000.0, 0.04, 'HMUZ', 'two_business_days_before_third_wednesday', 4),
    '6N': ('New Zealand Dollar', 100000.0, 0.04, 'HMUZ', 'two_business_days_before_third_wednesday', 4),
    'VX': ('Cboe VIX', 1000.0, 0.15, ALL, 'vix', 8),
    'BTC': ('Bitcoin', 5.0, 0.35, ALL, 'last_friday', 6),
    'MBT': ('Micro Bitcoin', 0.1, 0.35, ALL, 'last_friday', 6),
    'ETH': ('Ether', 50.0, 0.35, ALL, 'last_friday', 6),
    'LE': ('Live Cattle', 400.0, 0.05, 'GJMQVZ', 'last_business_day', 6),
    'GF': ('Feeder Cattle', 500.0, 0.05, 'FHJKQUVX', 'last_thursday', 8),
    'HE': ('Lean Hogs', 400.0, 0.06, 'GJKMNQVZ', 'tenth_business_day', 8),
    'SB': ('Sugar No. 11', 1120.0, 0.08, 'HKNV', 'last_business_day_prior_month', 4),
    'CT': ('Cotton No. 2', 500.0, 0.07, 'HKNVZ', 'seventeenth_business_day_before_end', 5),
    'KC': ('Coffee C', 375.0, 0.09, 'HKNUZ', 'eighth_business_day_before_last', 5),
    'CC': ('Cocoa', 10.0, 0.10, 'HKNUZ', 'eleventh_business_day_before_last', 5),
    'DC': ('Class III Milk', 2000.0, 0.05, ALL, 'dairy', 12),
}


def spec(root: str):
    s = SPECS.get(str(root).upper().lstrip('/'))
    if s is None:
        raise not_supported(f"futures contracts of {root}",
                            f"The engine has exchange calendars and specs for {', '.join(sorted(SPECS))}.")
    return s


# ---------------------------------------------------------------- expiry calendar

def _mc():
    from . import market_calendar as mc
    return mc


def _shift_month(year, month, n):
    m = month - 1 + n
    return year + m // 12, m % 12 + 1


def expiry_for(root: str, year: int, month: int) -> date:
    """Last trading day of `root`'s contract for (year, month)."""
    mc = _mc()
    rule = spec(root)[4]
    if rule == 'third_friday':
        d = mc.third_friday(year, month)
        return d if mc.is_trading_day(d) else mc.previous_trading_day(d)
    if rule == 'third_last_business_day':
        return mc.nth_last_business_day(year, month, 3)
    if rule == 'last_business_day':
        return mc.last_business_day(year, month)
    if rule == 'cl':
        y, m = _shift_month(year, month, -1)
        d25 = date(y, m, 25)
        n = 3 if mc.is_trading_day(d25) else 4
        d = d25
        while n:
            d -= timedelta(days=1)
            if mc.is_trading_day(d):
                n -= 1
        return d
    if rule == 'last_business_day_two_months_prior':
        y, m = _shift_month(year, month, -2)
        return mc.last_business_day(y, m)
    if rule == 'three_business_days_before_month':
        return mc.add_trading_days(date(year, month, 1), -3)
    if rule == 'last_business_day_prior_month':
        y, m = _shift_month(year, month, -1)
        return mc.last_business_day(y, m)
    if rule == 'business_day_before_15th':
        return mc.previous_trading_day(date(year, month, 15))
    if rule == 'seventh_business_day_before_last':
        return mc.add_trading_days(mc.last_business_day(year, month), -7)
    if rule == 'two_business_days_before_third_wednesday':
        return mc.add_trading_days(mc.third_wednesday(year, month), -2)
    if rule == 'business_day_before_third_wednesday':
        return mc.add_trading_days(mc.third_wednesday(year, month), -1)
    if rule == 'vix':
        y, m = _shift_month(year, month, 1)
        d = mc.third_friday(y, m) - timedelta(days=30)
        return d if mc.is_trading_day(d) else mc.previous_trading_day(d)
    if rule == 'last_friday':
        y, m = _shift_month(year, month, 1)
        d = date(y, m, 1) - timedelta(days=1)
        while d.weekday() != 4:
            d -= timedelta(days=1)
        return d if mc.is_trading_day(d) else mc.previous_trading_day(d)
    if rule == 'last_thursday':
        y, m = _shift_month(year, month, 1)
        d = date(y, m, 1) - timedelta(days=1)
        while d.weekday() != 3:
            d -= timedelta(days=1)
        return d if mc.is_trading_day(d) else mc.previous_trading_day(d)
    if rule == 'tenth_business_day':
        return mc.nth_business_day(year, month, 10)
    if rule == 'seventeenth_business_day_before_end':
        return mc.add_trading_days(mc.last_business_day(year, month), -17)
    if rule == 'eighth_business_day_before_last':
        return mc.add_trading_days(mc.last_business_day(year, month), -8)
    if rule == 'eleventh_business_day_before_last':
        return mc.add_trading_days(mc.last_business_day(year, month), -11)
    if rule == 'dairy':
        # the business day before the USDA announces the month's class price (approximated as the
        # first Wednesday of the following month; the real date follows the USDA calendar)
        y, m = _shift_month(year, month, 1)
        d = date(y, m, 1)
        while d.weekday() != 2:
            d += timedelta(days=1)
        return mc.previous_trading_day(d)
    raise not_supported(f"the expiry calendar of {root}")


def contract_expiries(root: str, as_of: date, count: Optional[int] = None) -> List[date]:
    """The listed contracts' expiries (last trading days) after `as_of`, nearest first."""
    s = spec(root)
    months = _cycle(s[3])
    n = count or s[5]
    out = []
    y, m = as_of.year, as_of.month
    guard = 0
    while len(out) < n and guard < 60:
        if m in months:
            e = expiry_for(root, y, m)
            if e > as_of:
                out.append(e)
        y, m = _shift_month(y, m, 1)
        guard += 1
    return sorted(out)


def contract_symbol(root: str, expiry: date, market: str = 'cme') -> Symbol:
    return Symbol.create_future(root, market, datetime(expiry.year, expiry.month, expiry.day))


# ---------------------------------------------------------------- LEAN-shaped objects

class FutureMarginModel:
    """Initial/maintenance margin per contract = a fixed fraction of the contract's notional value
    (exchange spec table). LEAN uses the exchanges' fixed dollar margins; this is a disclosed
    simplification."""

    def __init__(self, fraction: float = 0.1, security=None):
        self.fraction = float(fraction)
        self._security = security

    def margin_fraction(self, security):
        return self.fraction

    def _per_contract(self, maintenance=False):
        sec = self._security
        if sec is None:
            return 0.0
        mult = float(getattr(sec.symbol_properties, 'contract_multiplier', 1.0) or 1.0)
        v = float(sec.price or 0.0) * mult * self.fraction
        return v * (0.9 if maintenance else 1.0)

    @property
    def initial_overnight_margin_requirement(self):
        return self._per_contract()

    @property
    def maintenance_overnight_margin_requirement(self):
        return self._per_contract(True)

    @property
    def initial_intraday_margin_requirement(self):
        return self._per_contract() * 0.5

    @property
    def maintenance_intraday_margin_requirement(self):
        return self._per_contract(True) * 0.5

    def get_leverage(self, security):
        return 1.0 / self.fraction


class FuturesContract:
    def __init__(self, symbol, underlying_symbol, expiry, bar=None, time=None):
        self.symbol = symbol
        self.underlying_symbol = underlying_symbol
        self.expiry = expiry
        self.time = time
        self.last_price = float(bar['close']) if bar else 0.0
        self.bid_price = self.ask_price = self.last_price
        self.bid_size = self.ask_size = 0.0
        self.volume = 0.0
        self.open_interest = 0.0
        self.mark_price = self.last_price

    @property
    def id(self):
        return self.symbol.id

    def __repr__(self):
        return f"{self.symbol}"

    Symbol = property(lambda self: self.symbol)
    Expiry = property(lambda self: self.expiry)
    LastPrice = property(lambda self: self.last_price)


class FuturesChain:
    """One root's contracts on a day (iterable of FuturesContract)."""

    def __init__(self, canonical, time, contracts):
        self.symbol = canonical
        self.time = time
        self.contracts = SymbolDict()
        for c in contracts:
            self.contracts[c.symbol] = c
        self.underlying = None
        self.trade_bars = SymbolDict()
        self.quote_bars = SymbolDict()
        self.ticks = SymbolDict()

    def __iter__(self):
        return iter(list(self.contracts.values()))

    def __len__(self):
        return len(self.contracts)

    def __bool__(self):
        return len(self.contracts) > 0

    @property
    def filtered_contracts(self):
        return [c.symbol for c in self]

    @property
    def data_frame(self):
        import pandas as pd
        rows = [{'expiry': c.expiry, 'lastprice': c.last_price, 'bidprice': c.bid_price, 'askprice': c.ask_price,
                 'openinterest': c.open_interest, 'volume': c.volume} for c in self]
        return pd.DataFrame(rows, index=pd.Index([c.symbol for c in self], name='symbol'))

    Contracts = property(lambda self: self.contracts)


class FutureFilterUniverse:
    """LEAN's future contract filter: expiration windows, front/back month, custom selectors."""

    def __init__(self, contracts, as_of, underlying_price=0.0):
        # contracts: list of (Symbol, expiry date)
        self._all = list(contracts)
        self._cur = list(contracts)
        self.local_time = as_of
        self.underlying = None
        self._at_open = False

    def _days(self, x):
        if isinstance(x, timedelta):
            return x.days
        return int(x)

    def expiration(self, min_expiry, max_expiry):
        lo = self.local_time.date() + timedelta(days=self._days(min_expiry))
        hi = self.local_time.date() + timedelta(days=self._days(max_expiry))
        self._cur = [(s, e) for s, e in self._cur if lo <= e <= hi]
        return self

    def front_month(self):
        if self._cur:
            first = min(e for _s, e in self._cur)
            self._cur = [(s, e) for s, e in self._cur if e == first]
        return self

    def back_months(self):
        if self._cur:
            first = min(e for _s, e in self._cur)
            self._cur = [(s, e) for s, e in self._cur if e != first]
        return self

    def back_month(self):
        exps = sorted({e for _s, e in self._cur})
        if len(exps) > 1:
            self._cur = [(s, e) for s, e in self._cur if e == exps[1]]
        else:
            self._cur = []
        return self

    def standards_only(self):
        return self

    def include_weeklys(self):
        return self

    def weeklys_only(self):
        self._cur = []
        return self

    def expiration_cycle(self, months):
        ms = set(int(m) for m in months)
        self._cur = [(s, e) for s, e in self._cur if e.month in ms]
        return self

    def only_apply_filter_at_market_open(self):
        self._at_open = True
        return self

    def contracts(self, selector):
        syms = [s for s, _e in self._cur]
        chosen = selector(syms) if callable(selector) else selector
        keep = {str(x) for x in (chosen or [])}
        self._cur = [(s, e) for s, e in self._cur if str(s) in keep]
        return self

    def where(self, predicate):
        self._cur = [(s, e) for s, e in self._cur if predicate(s)]
        return self

    def __iter__(self):
        return iter([s for s, _e in self._cur])

    def __len__(self):
        return len(self._cur)

    def symbols(self):
        return [s for s, _e in self._cur]

    Expiration = expiration
    FrontMonth = front_month
    BackMonths = back_months
    Contracts = contracts


class Delisting:
    def __init__(self, symbol=None, time=None, price=0.0, type_=0):
        self.symbol = symbol
        self.time = time
        self.end_time = time
        self.price = float(price)
        self.value = float(price)
        self.type = type_

    def __str__(self):
        return f"{self.symbol} {'Warning' if self.type == 0 else 'Delisted'}"


class SymbolChangedEvent:
    def __init__(self, symbol=None, time=None, old_symbol='', new_symbol=''):
        self.symbol = symbol
        self.time = time
        self.end_time = time
        self.old_symbol = old_symbol
        self.new_symbol = new_symbol
        self.value = 0.0

    def __str__(self):
        return f"{self.time} {self.symbol}: {self.old_symbol} -> {self.new_symbol}"

    OldSymbol = property(lambda self: self.old_symbol)
    NewSymbol = property(lambda self: self.new_symbol)


# ---------------------------------------------------------------- session extension

class FuturesManager(SessionExtension):
    """Continuous futures (mapping, rolls), contract universes (chains), derived contract prices,
    expiries."""

    OI_ROLL_DAYS = 5

    def __init__(self):
        self.canonicals = {}         # key -> Security (continuous)
        self.used = False
        self._continuous = {}        # root -> BarSeries of ROOT=F
        self._rate = None            # ^IRX series
        self._mapped = {}            # canonical key -> mapped contract Symbol
        self._pending_changes = []   # SymbolChangedEvent queued for the slice
        self._expiring = set()
        self._settled = []
        self._chain_members = {}     # canonical key -> set of contract keys selected today

    # ---- set-up ----
    def is_contract(self, symbol) -> bool:
        return (getattr(symbol, 'security_type', None) == SecurityType.FUTURE
                and not symbol.is_canonical() and symbol.id.date != DEFAULT_DATE)

    def after_initialize(self, session):
        algo = session.algo
        session.data.register_provider(self.is_contract, lambda sym: self._contract_series(session, sym))
        for key, sec in list(algo.securities.items()):
            st = sec.symbol.security_type
            if st != SecurityType.FUTURE:
                continue
            self.used = True
            if sec.symbol.is_canonical():
                self.canonicals[key] = sec
            self.configure(sec)

    def configure(self, sec):
        """Contract multiplier, margin model of a future (canonical or contract)."""
        root = sec.symbol.id.symbol
        try:
            s = spec(root)
        except Exception:
            return
        sec.symbol_properties.contract_multiplier = s[1]
        sec.symbol_properties.description = s[0]
        if not isinstance(getattr(sec, 'buying_power_model', None), FutureMarginModel):
            sec.buying_power_model = FutureMarginModel(s[2], sec)
            sec.margin_model = sec.buying_power_model

    def owns(self, security):
        return self.is_contract(security.symbol)

    def calendar_series(self, session):
        roots = list(self.canonicals)
        for sec in list(session.algo.securities.values()):
            if self.is_contract(sec.symbol) and sec.symbol.id.symbol not in roots:
                roots.append(sec.symbol.id.symbol)
        return [s for s in (self._cont(session, k) for k in roots) if s is not None]

    # ---- data ----
    def _root(self, symbol):
        return symbol.id.symbol

    def _cont(self, session, root_or_key):
        root = str(root_or_key).upper().lstrip('/')
        if root in self._continuous:
            return self._continuous[root]
        sym = Symbol(f"{root}=F", SecurityType.FUTURE, 'cme')
        s = session.data.series(sym, SecurityType.FUTURE, 'cme')
        self._continuous[root] = s
        return s

    def _irx(self, session):
        if self._rate is None:
            sym = Symbol('^IRX', SecurityType.INDEX, 'usa')
            self._rate = session.data.series(sym, SecurityType.INDEX, 'usa') or False
        return self._rate or None

    def _rate_at(self, session, i_date):
        s = self._irx(session)
        if s is None:
            return 0.0
        n = s.count_known_at(i_date + timedelta(hours=16))
        if n <= 0:
            return 0.0
        y = s.close[n - 1] / 100.0
        return math.log(1 + y) if y > -0.99 else 0.0

    def _contract_series(self, session, sym):
        """Daily bars of one contract, derived from the continuous series (see module notes)."""
        from .data import BarSeries
        root = self._root(sym)
        cont = self._cont(session, root)
        if cont is None:
            return None
        exp = sym.id.date.date()
        s = spec(root)
        listing = exp - timedelta(days=int(400 * max(1, s[5]) / (12 / max(1, len(s[3])))))
        dates, o, h, l, c, v = [], [], [], [], [], []
        front_cache = {}
        for i, d in enumerate(cont.dates):
            dd = d.date()
            if dd < listing or dd > exp:
                continue
            if dd not in front_cache:
                ex = contract_expiries(root, dd - timedelta(days=1), 1)
                front_cache[dd] = ex[0] if ex else exp
            front = front_cache[dd]
            tau = max(0, (exp - front).days) / 365.0
            f = math.exp(self._rate_at(session, d) * tau) if tau > 0 else 1.0
            dates.append(d)
            o.append(cont.open[i] * f)
            h.append(cont.high[i] * f)
            l.append(cont.low[i] * f)
            c.append(cont.close[i] * f)
            v.append(0.0)
        if not dates:
            return None
        bs = BarSeries.from_columns(str(sym).upper(), f"{root}=F (derived)", dates, o, h, l, c, v)
        bs.show_in_bars = False
        self.used = True
        return bs

    # ---- mapping ----
    def _mapped_for(self, sec, day: date):
        root = sec.symbol.id.symbol
        mode = getattr(sec, '_data_mapping_mode', None)
        mode = 2 if mode is None else int(mode)
        depth = int(getattr(sec, '_contract_depth_offset', 0) or 0)
        exps = contract_expiries(root, day - timedelta(days=1), depth + 3)
        mc = _mc()
        if mode == 0:        # last trading day: hold the contract through its expiry day
            live = [e for e in exps if e >= day]
        elif mode == 1:      # first day of the expiry month
            live = [e for e in exps if (e.year, e.month) > (day.year, day.month)]
        else:                # open interest (approximation): roll N trading days before expiry
            live = [e for e in exps if mc.add_trading_days(e, -self.OI_ROLL_DAYS) > day]
        if len(live) <= depth:
            return None
        e = live[depth]
        return contract_symbol(root, e, sec.symbol.market)

    # ---- per day ----
    def before_open(self, session, day, warming):
        if not self.canonicals:
            return [], []
        algo = session.algo
        added, removed = [], []
        d = day.date()
        for key, sec in list(self.canonicals.items()):
            # the mapped contract (continuous futures)
            m = self._mapped_for(sec, d)
            if m is not None:
                old = self._mapped.get(key)
                if old is not None and str(old) != str(m):
                    self._pending_changes.append(SymbolChangedEvent(sec.symbol, day, str(old), str(m)))
                self._mapped[key] = m
                sec._mapped = m
                if str(m).upper() not in algo.securities:
                    internal = self._subscribe(session, m, sec)
                    internal._fe_internal = True      # the continuous future's own subscription
            # the contract universe (set_filter)
            members = set()
            for sym in self._filtered(sec, d):
                k = str(sym).upper()
                members.add(k)
                if k not in algo.securities:
                    self._subscribe(session, sym, sec)
                    added.append(k)
            prev = self._chain_members.get(key, set())
            for k in prev - members:
                h = algo.portfolio[k] if k in algo.portfolio else None
                if h is None or not h.quantity:
                    removed.append(k)
            self._chain_members[key] = members
        return added, removed

    def _subscribe(self, session, sym, canonical_sec):
        algo = session.algo
        res = getattr(canonical_sec, 'resolution', None)
        sec = algo._add(sym, res, None)
        self.configure(sec)
        return sec

    def _filtered(self, sec, d: date):
        root = sec.symbol.id.symbol
        exps = contract_expiries(root, d - timedelta(days=1))
        listed = [(contract_symbol(root, e, sec.symbol.market), e) for e in exps]
        args = getattr(sec, '_filter_args', None)
        if args is None:
            return []   # LEAN's default: no contract universe until set_filter is called
        a, kw = args
        uni = FutureFilterUniverse(listed, datetime(d.year, d.month, d.day))
        if a and callable(a[0]):
            res = a[0](uni)
            uni = res if isinstance(res, FutureFilterUniverse) else uni
        elif a:
            lo, hi = (a[0], a[1]) if len(a) >= 2 else (0, a[0])
            uni.expiration(lo, hi)
        return uni.symbols()

    def decorate_slice(self, session, sl, day):
        if not self.canonicals and not self.used:
            return
        from .chains import FuturesChains
        algo = session.algo
        if sl.future_chains is None:
            sl.future_chains = FuturesChains()
        bars = sl.bars
        for key, sec in self.canonicals.items():
            members = self._chain_members.get(key, set())
            contracts = []
            for k in sorted(members):
                s2 = algo.securities.get(k)
                b = bars.get(k)
                if s2 is None or b is None:
                    continue
                contracts.append(FuturesContract(s2.symbol, sec.symbol, s2.symbol.id.date,
                                                 {'close': b.close}, sl.time))
            if contracts:
                sl.future_chains[sec.symbol] = FuturesChain(sec.symbol, sl.time, contracts)
        for ev in self._pending_changes:
            sl.symbol_changed_events[ev.symbol] = ev
            ev.time = ev.end_time = sl.time
        self._pending_changes = []
        # delistings, stamped as LEAN stamps them (WARNING at the expiry date, DELISTED the day after): the
        # warning arrives with the last session before expiry, the delisting with the expiry session
        for k, s2 in list(algo.securities.items()):
            if not self.is_contract(s2.symbol) or getattr(s2, 'is_delisted', False):
                continue
            if getattr(s2, '_fe_internal', False) and not algo.portfolio[k].quantity                     and k not in self._chain_members.get(str(s2.symbol.canonical).upper(), set()):
                continue      # a continuous future's mapped contract: internal, no events for it
            exp = s2.symbol.id.date.date()
            ser = session.series.get(k)
            if ser is None:
                continue
            i = ser.index_of_date(day)
            if i is None:
                continue
            nxt = ser.dates[i + 1].date() if i + 1 < len(ser.dates) else None
            exp_dt = datetime(exp.year, exp.month, exp.day)
            if day.date() == exp:
                sl.delistings[s2.symbol] = Delisting(s2.symbol, exp_dt + timedelta(days=1), ser.close[i], 1)
                self._expiring.add(k)
            elif nxt is not None and nxt == exp or (nxt is None and exp > day.date() and
                                                    _mc().add_trading_days(day.date(), 1) == exp):
                sl.delistings[s2.symbol] = Delisting(s2.symbol, exp_dt, ser.close[i], 0)

    def after_close(self, session, day, warming):
        """Settle expired contracts at their final price; they stop trading."""
        if not self._expiring:
            return
        algo, broker = session.algo, session.broker
        for k in sorted(self._expiring):
            sec = algo.securities.get(k)
            if sec is None:
                continue
            for tk in list(broker.book):
                if str(tk.symbol).upper() == k:
                    broker._cancel(tk, 'Canceled: the contract expired')
            h = algo.portfolio[k] if k in algo.portfolio else None
            if h is not None and h.quantity and not warming:
                px = float(sec.price or 0.0)
                tk = algo._make_ticket(sec.symbol, -h.quantity, OrderType.MARKET, tag='Liquidate from delisting')
                tk._broker = broker
                tk.status = OrderStatus.SUBMITTED
                broker._fill(tk, k, px, px, day + timedelta(hours=16))
                self._settled.append(f"{k} {day:%Y-%m-%d}")
            sec.is_delisted = True
            sec.is_tradable = False
        self._expiring = set()

    def silent_security(self, key):
        return False

    def notes(self, session):
        if not self.used:
            return []
        out = [DISCLOSURE]
        if self._settled:
            out.append("Expired futures positions were settled at the final price: " + ", ".join(self._settled[:5])
                       + ("..." if len(self._settled) > 5 else ""))
        return out

    def result_flags(self, session):
        return {'uses_derived_futures_contracts': True} if self.used else {}
