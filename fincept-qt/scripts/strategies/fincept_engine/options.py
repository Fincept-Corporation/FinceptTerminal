# ============================================================================
# Fincept Terminal - Strategy Engine: options in the backtest (session extension)
#
# Ties the option modules (option_chain.py: listed expiries, strike ladders, chains, filters;
# option_model.py: volatility / rate / dividend inputs and model quotes; option_pricing.py)
# into the run:
#   * add_option / add_index_option / add_future_option subscribe a canonical option; before
#     each open the chain is generated from the underlying price known then, the strategy's
#     filter (set_filter) is applied and the selected contracts are subscribed (as LEAN's option
#     universe does); slice.option_chains[canonical] holds them with quotes at the close;
#   * each contract is a security priced with daily bars computed by the model from the real
#     underlying bars (open/high/low/close), with bid/ask from the spread rule; market orders buy
#     at the ask and sell at the bid of the fill reference price;
#   * at expiry (after the close of the last trading day) positions are settled: equity and
#     futures options in the money are exercised/assigned (shares or futures at the strike, two
#     order events like LEAN's exercise model, on_assignment_order_event for assignments), index
#     options are cash-settled at intrinsic value, out-of-the-money contracts expire at 0;
#   * option positions carry a margin requirement per underlying (see group_margin);
#   * the equity underlying of an option is priced split-adjusted but not dividend-adjusted, so
#     strikes compare with the price as traded and dividends are paid in cash.
# Every result that uses this carries the model disclosure (option_model.disclosure) and the
# flag uses_model_priced_derivatives.
# ============================================================================

import math
from datetime import datetime, timedelta, date
from typing import Dict, List

from .enums import SecurityType, OrderType, OrderStatus, OptionRight, DataNormalizationMode
from .errors import not_supported
from .session import SessionExtension
from .types import Symbol, DEFAULT_DATE

OPTION_TYPES = (SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION)
_CLOSE = timedelta(hours=16)
_OPEN = timedelta(hours=9, minutes=30)


def is_contract(symbol) -> bool:
    return (getattr(symbol, 'security_type', None) in OPTION_TYPES and not symbol.is_canonical()
            and symbol.id.date != DEFAULT_DATE)


def contract_multiplier(symbol) -> float:
    if symbol.security_type == SecurityType.FUTURE_OPTION:
        from .futures import spec
        try:
            return spec(symbol.underlying.id.symbol)[1]
        except Exception:
            return 1.0
    return 100.0


def canonical_multiplier(symbol) -> float:
    """Contract multiplier of an option symbol (canonical or contract)."""
    if symbol.security_type == SecurityType.FUTURE_OPTION and symbol.underlying is not None:
        from .futures import spec
        try:
            return spec(symbol.underlying.id.symbol)[1]
        except Exception:
            return 1.0
    return 100.0


def _mc_next(d):
    from . import market_calendar as mc
    return mc.add_trading_days(d, 1)


class _Col:
    """One column (open/high/low/close/volume/bid/ask) of a lazily priced option series."""
    __slots__ = ('_s', '_j')

    def __init__(self, series, j):
        self._s = series
        self._j = j

    def __len__(self):
        return len(self._s.dates)

    def __getitem__(self, i):
        if isinstance(i, slice):
            return [self._s._row(k)[self._j] for k in range(*i.indices(len(self._s.dates)))]
        if i < 0:
            i += len(self._s.dates)
        return self._s._row(i)[self._j]

    def __iter__(self):
        return (self._s._row(k)[self._j] for k in range(len(self._s.dates)))


class _OptionBars:
    """BarSeries-like daily bars of one option contract, priced by the model from the underlying's
    bars on first access of each day (a run only prices the days it reads)."""

    def __init__(self, sym, us, model, lo, hi):
        from .data import BAR_CLOSE
        self.symbol = str(sym).upper()
        self.yahoo = f"model ({getattr(us, 'yahoo', '')})"
        self.show_in_bars = False
        self.dividends, self.splits = [], []
        self._sym = sym
        self._us = us
        self._model = model
        import bisect
        a = bisect.bisect_left(us.dates, lo)
        b = bisect.bisect_right(us.dates, hi)
        self._ui = range(a, b)
        self.dates = us.dates[a:b]
        self._ends = us._ends[a:b]
        self._pos = None
        self._rows = {}
        self._call = int(sym.id.option_right) == int(OptionRight.CALL)
        self.open, self.high, self.low, self.close, self.volume = (_Col(self, 0), _Col(self, 1), _Col(self, 2),
                                                                   _Col(self, 3), _Col(self, 4))
        self.bid_close, self.ask_close = _Col(self, 5), _Col(self, 6)
        info = model.info(sym)

        def half_spread(px, info=info, model=model):
            b, a = model.bid_ask(info, px)
            return (a - b) / 2.0
        self._spread = half_spread

    def _row(self, i):
        r = self._rows.get(i)
        if r is None:
            us, m, sym = self._us, self._model, self._sym
            k = self._ui[i]
            d = us.dates[k]
            t_o, t_c = d + _OPEN, d + _CLOSE
            qo = m.quote(sym, us.open[k], t_o, 'open')
            qc = m.quote(sym, us.close[k], t_c, 'close')
            qh = m.quote(sym, us.high[k] if self._call else us.low[k], t_c, 'close').mid
            ql = m.quote(sym, us.low[k] if self._call else us.high[k], t_c, 'close').mid
            r = (qo.mid, max(qo.mid, qc.mid, qh, ql), min(qo.mid, qc.mid, qh, ql), qc.mid, 0.0, qc.bid, qc.ask)
            self._rows[i] = r
        return r

    def __len__(self):
        return len(self.dates)

    def count_known_at(self, t):
        import bisect
        return bisect.bisect_right(self._ends, t)

    def index_of_date(self, d):
        pos = self._pos
        if pos is None:
            pos = self._pos = {x: i for i, x in enumerate(self.dates)}
        return pos.get(d)

    def bar(self, i):
        o, h, l, c, v, b, a = self._row(i)
        return {'time': self.dates[i], 'open': o, 'high': h, 'low': l, 'close': c, 'volume': v,
                'bid_close': b, 'ask_close': a, 'half_spread': self._spread}


class OptionManager(SessionExtension):

    EAGER_LIMIT = 60     # selections up to this many contracts are subscribed (and announced) up front

    def __init__(self):
        self.model = None
        self.canonicals = {}        # key -> canonical option Security
        self.members = {}           # canonical key -> set of contract keys selected today
        self.used = False
        self.underlyings = set()    # underlying keys priced for options
        self.events = []            # exercise / assignment / expiry log for the notes
        self._indicators = []       # (indicator, symbol, kind)
        self._session = None
        self._chain_cache = {}
        self._warm_start = None
        self.member_symbols = {}    # canonical key -> {contract key: Symbol} selected today
        self.lazy = set()           # canonical keys whose (large) selection is subscribed on demand
        self.live = set()           # subscribed contract keys not yet expired

    # ------------------------------------------------------------------ set-up
    def attach_early(self, algo):
        """Used from initialize() (e.g. option_chain_provider) before the session exists."""
        from types import SimpleNamespace
        from .option_model import OptionModel
        self._early = True
        self._session = SimpleNamespace(algo=algo, data=algo._data, series={}, broker=algo._broker,
                                        start=algo._start_date, end=algo._end_date)
        if self.model is None:
            self.model = OptionModel(self._series_by_yahoo, self._dividends)
        algo._option_manager = self

    def after_initialize(self, session):
        self._session = session
        algo = session.algo
        from .option_model import OptionModel
        if self.model is None:
            self.model = OptionModel(self._series_by_yahoo, self._dividends)
        session.data.register_provider(is_contract, lambda s: self._contract_series(session, s))
        session.broker.group_margin = self.group_margin
        algo._option_manager = self
        for key, sec in list(algo.securities.items()):
            if sec.symbol.security_type in OPTION_TYPES:
                self.used = True
                self._prepare_security(session, sec)
        for ind, sym, kind in getattr(algo, '_pending_option_indicators', []):
            self.register_indicator(ind, sym, kind)
        algo._pending_option_indicators = []

    def _prepare_security(self, session, sec):
        algo = session.algo
        sym = sec.symbol
        sec.symbol_properties.contract_multiplier = canonical_multiplier(sym)
        if is_contract(sym) and not getattr(sec, 'is_delisted', False):
            self.live.add(str(sym).upper())
        if sym.is_canonical():
            self.canonicals[str(sym).upper()] = sec
        u = sym.underlying
        if u is not None:
            uk = str(u).upper()
            if u.security_type == SecurityType.EQUITY:
                session.data.set_mode(u, 'split_adjusted')
            if uk not in algo.securities and u.security_type != SecurityType.FUTURE:
                algo._add(u, getattr(sec, 'resolution', None), None)
            self.underlyings.add(uk)
        sec.underlying = algo.securities._securities.get(str(u).upper()) if u is not None else None
        if is_contract(sym) and not sec.price and self.model is not None:
            # a contract subscribed during the run gets the model quote known now (no look-ahead)
            t = algo.time
            S = self._underlying_price(u, t)
            if S:
                q = self.model.quote(sym, S, t, 'close')
                sec.update_price(q.mid, q.mid, q.mid, q.mid, 0.0)
                sec.bid_price, sec.ask_price = q.bid, q.ask

    def owns(self, security):
        return security.symbol.security_type in OPTION_TYPES

    @property
    def free_symbols(self):
        return False

    # ------------------------------------------------------------------ data
    def _series_by_yahoo(self, y):
        data = self._session.data
        st = SecurityType.INDEX if y.startswith('^') else (SecurityType.FUTURE if y.endswith('=F') else
                                                            SecurityType.EQUITY)
        # the underlying itself: the series the run prices it with
        for uk in self.underlyings:
            s = self._session.series.get(uk) or data._series.get(uk)
            if s is not None and getattr(s, 'yahoo', None) == y:
                return s
        return data.series(Symbol(y, st, 'usa'), st, 'usa')

    def _dividends(self, y):
        rt = getattr(self._session, '_actions', None)
        fn = getattr(self._session.data, '_actions_fetcher', None)
        if fn is None:
            return []
        try:
            acts = fn(y) or {}
        except Exception:
            return []
        return [(datetime.strptime(str(d)[:10], '%Y-%m-%d'), float(a)) for d, a in acts.get('dividends', [])]

    def underlying_series(self, sym):
        u = sym.underlying
        if u is None:
            return None
        session = self._session
        s = session.series.get(str(u).upper())
        if s is None:
            s = session.data.series(u, u.security_type, getattr(u, 'market', None))
        return s

    def _contract_series(self, session, sym):
        """Model-priced daily bars of one contract from the underlying's real bars (lazy per day)."""
        us = self.underlying_series(sym)
        if us is None or not len(us):
            return None
        exp = sym.id.date
        lo = max(session.data.fetch_start, exp - timedelta(days=3 * 365))
        hi = min(exp, session.end)
        s = _OptionBars(sym, us, self.model, lo, hi)
        if not len(s):
            return None
        self.used = True
        return s

    # ------------------------------------------------------------------ chains
    def _underlying_price(self, sym_u, t):
        s = self._session.series.get(str(sym_u).upper())
        if s is None:
            s = self._session.data.series(sym_u, sym_u.security_type, getattr(sym_u, 'market', None))
        if s is None:
            return 0.0
        n = s.count_known_at(t)
        return float(s.close[n - 1]) if n > 0 else 0.0

    def chain_symbols(self, canonical, t, price=None):
        from .option_chain import option_chain_symbols
        u = canonical.underlying
        S = price if price is not None else self._underlying_price(u, t)
        if not S:
            return [], S
        try:
            vh = self.model.atm_vol(canonical, t, at='close')
        except Exception:
            vh = None
        return option_chain_symbols(canonical, u, t, S, vh), S

    def _filter_of(self, sec):
        from .option_chain import set_filter_function, default_filter
        args = getattr(sec, '_filter_args', None)
        if not args:
            return default_filter()
        a, kw = args
        if not a and kw:
            a = tuple(kw.values())
        return set_filter_function(*a)

    def select(self, canonical_sec, t):
        from .option_chain import OptionFilterUniverse, ChainUnderlying, apply_filter
        canonical = canonical_sec.symbol
        syms, S = self.chain_symbols(canonical, t)
        if not syms:
            return []
        u = ChainUnderlying(canonical.underlying, S, t)
        uni = OptionFilterUniverse(syms, u, t, quote_fn=self.model.quote_fn(S, t, 'close'),
                                   security_type=canonical.security_type)
        res = apply_filter(self._filter_of(canonical_sec), uni)
        return [getattr(c, 'symbol', c) for c in res]

    def before_open(self, session, day, warming):
        if not self.canonicals:
            return [], []
        algo = session.algo
        added, removed = [], []
        self._expand_continuous_fop(session, day)
        for key, sec in list(self.canonicals.items()):
            if getattr(sec, '_removed', False):
                # the canonical was removed (remove_security, a universe dropping the underlying)
                for k in self.members.pop(key, set()):
                    h = algo.portfolio[k] if k in algo.portfolio else None
                    if h is None or not h.quantity:
                        removed.append(k)
                self.member_symbols.pop(key, None)
                del self.canonicals[key]
                continue
            if sec.symbol.security_type == SecurityType.FUTURE_OPTION and sec.symbol.underlying.is_canonical():
                continue    # options on a continuous future: chains come from the futures' contracts
            chosen = self.select(sec, day)
            keys = set()
            # A large selection (e.g. every call of SPX) is not subscribed up front: its contracts are
            # quoted in the chain and become securities when the strategy trades or reads one.
            lazy = len(chosen) > self.EAGER_LIMIT
            if lazy:
                self.lazy.add(key)
            syms = {}
            for sym in chosen:
                k = str(sym).upper()
                keys.add(k)
                syms[k] = sym
                if not lazy and k not in algo.securities:
                    s2 = algo._add(sym, getattr(sec, 'resolution', None), None)
                    self._prepare_security(session, s2)
                    added.append(k)
            self.member_symbols[key] = syms
            prev = self.members.get(key, set())
            for k in prev - keys:
                h = algo.portfolio[k] if k in algo.portfolio else None
                if h is None or not h.quantity:
                    removed.append(k)
            self.members[key] = keys
        return added, removed

    def _expand_continuous_fop(self, session, day):
        """add_future_option on a continuous future: each futures contract the future's filter selects
        today gets its own option chain (canonical option of that contract, the parent's option filter);
        contracts that leave the futures chain lose theirs."""
        algo = session.algo
        parents = [(k, s) for k, s in self.canonicals.items()
                   if s.symbol.security_type == SecurityType.FUTURE_OPTION and s.symbol.underlying.is_canonical()
                   and not getattr(s, '_removed', False)]
        if not parents:
            return
        fm = getattr(algo, '_futures_manager', None)
        if fm is None:
            from .futures import FuturesManager
            fm = next((h for h in session.extensions if isinstance(h, FuturesManager)), None)
        if fm is None:
            return
        children = getattr(self, '_fop_children', None)
        if children is None:
            children = self._fop_children = {}           # child canonical key -> parent key
        for pkey, psec in parents:
            fut_key = str(psec.symbol.underlying).upper()
            members = fm._chain_members.get(fut_key, set())
            wanted = set()
            for ck in members:
                cs = algo.securities._securities.get(ck)
                if cs is None or getattr(cs, 'is_delisted', False):
                    continue
                can = Symbol.create_canonical_option(cs.symbol, getattr(cs.symbol, 'market', None))
                k = str(can).upper()
                wanted.add(k)
                if k not in self.canonicals:
                    child = algo._add(can, getattr(psec, 'resolution', None), None)
                    child._filter_args = getattr(psec, '_filter_args', None)
                    for attr in ('price_model', '_option_exercise_model', '_option_assignment_model'):
                        if getattr(psec, attr, None) is not None:
                            setattr(child, attr, getattr(psec, attr))
                    self._prepare_security(session, child)
                    children[k] = pkey
            for k, parent in list(children.items()):
                if parent == pkey and k not in wanted:
                    cs = self.canonicals.get(k)
                    if cs is not None:
                        cs._removed = True
                    del children[k]

    def silent_security(self, key):
        """Canonical option securities this manager creates for continuous futures are internal."""
        return key in getattr(self, '_fop_children', {})

    def decorate_slice(self, session, sl, day):
        if not self.canonicals and not self.used:
            return
        from .option_chain import OptionChains, OptionChain, OptionContract, ChainUnderlying, expiry_datetime
        algo = session.algo
        if sl.option_chains is None:
            sl.option_chains = OptionChains(sl.time)
        t = sl.time
        for key, sec in self.canonicals.items():
            members = self.members.get(key)
            if not members:
                continue
            u = sec.symbol.underlying
            S = self._underlying_price(u, t)
            if not S:
                continue
            und = ChainUnderlying(u, S, t)
            qf = self.model.quote_fn(S, t, 'close')      # quotes computed when a contract is read
            pm = getattr(sec, 'price_model', None)
            allowed = getattr(pm, 'allowed_styles', None)
            contracts = []
            syms = self.member_symbols.get(key, {})
            for k in members:
                cs = algo.securities.get(k)
                sym = cs.symbol if cs is not None else syms.get(k)
                if sym is None or (cs is not None and getattr(cs, 'is_delisted', False)):
                    continue
                if expiry_datetime(sym) <= t:
                    continue        # expired at this moment (PM contracts at the close of their last day)
                oc = OptionContract(sym, t, und, quote_fn=qf)
                if allowed is not None and int(sym.id.option_style) not in allowed:
                    from .errors import ArgumentException
                    style = 'American' if int(sym.id.option_style) == 0 else 'European'
                    oc._model_error = ArgumentException(
                        f"{style} style options are not supported by option price model '{pm.name}'")
                contracts.append(oc)
            if contracts:
                sl.option_chains[sec.symbol] = OptionChain(sec.symbol, t, contracts, und, quote_fn=qf)
        # delistings of contracts, stamped as LEAN stamps them (WARNING at the expiry date, DELISTED the day
        # after). LEAN's daily slice at midnight carries the previous session's bar; this engine's slice is
        # that session's close, so the warning arrives with the last session before expiry and the
        # delisting with the last session on or before expiry (whose close is when the contract is
        # exercised or expires, see after_close).
        from .futures import Delisting
        from . import market_calendar as mc
        for k in list(self.live):
            cs = algo.securities._securities.get(k)
            if cs is None or getattr(cs, 'is_delisted', False):
                continue
            sym = cs.symbol
            exp = sym.id.date.date()
            nxt = mc.add_trading_days(day.date(), 1)
            exp_dt = datetime(exp.year, exp.month, exp.day)
            if nxt > exp:
                sl.delistings[sym] = Delisting(sym, exp_dt + timedelta(days=1), float(cs.price or 0.0), 1)
            elif nxt == exp:
                sl.delistings[sym] = Delisting(sym, exp_dt, float(cs.price or 0.0), 0)
        self._update_indicators(sl, t)

    def on_demand(self, key):
        """A chain contract the strategy reads or trades: subscribe it now. Returns the Security."""
        for syms in self.member_symbols.values():
            sym = syms.get(key)
            if sym is not None:
                algo = self._session.algo
                sec = algo._add(sym, None, None)
                self._prepare_security(self._session, sec)
                return sec
        return None

    # ------------------------------------------------------------------ expiry
    def after_close(self, session, day, warming):
        algo = session.algo
        for k in list(self.live):
            cs = algo.securities._securities.get(k)
            if cs is None:
                self.live.discard(k)
                continue
            if cs.symbol.id.date.date() > day.date() and                     _mc_next(day.date()) <= cs.symbol.id.date.date():
                continue        # not yet the last session on or before the expiry date
            self._expire(session, cs, day, warming)
            self.live.discard(k)

    def _expire(self, session, cs, day, warming):
        algo, broker = session.algo, session.broker
        sym = cs.symbol
        k = str(sym).upper()
        for tk in list(broker.book):
            if str(tk.symbol).upper() == k:
                broker._cancel(tk, 'Canceled: the option expired')
        h = algo.portfolio[k] if k in algo.portfolio else None
        cs.is_delisted = True
        cs.is_tradable = False
        if h is None or not h.quantity or warming:
            return
        S = self._underlying_price(sym.underlying, day + _CLOSE)
        K = sym.id.strike_price
        call = int(sym.id.option_right) == int(OptionRight.CALL)
        intrinsic = max(0.0, (S - K) if call else (K - S))
        itm = intrinsic > 0
        q = h.quantity
        assignment = itm and q < 0
        when = day + _CLOSE
        cash_settled = sym.security_type == SecurityType.INDEX_OPTION
        tag = (('Assigned' if assignment else 'Automatic Exercise') if itm else 'OTM') + f". Underlying: {S:g}"
        tk = algo._make_ticket(sym, -q, OrderType.OPTION_EXERCISE, tag=tag)
        tk._broker = broker
        tk.status = OrderStatus.SUBMITTED
        tk.created_time = when
        px = intrinsic if (itm and cash_settled) else 0.0
        algo.portfolio.process_fill(k, -q, px, 0.0)
        tk.average_fill_price = px
        tk.quantity_filled = -q
        tk.status = OrderStatus.FILLED
        tk.last_fill_time = when
        (broker.append_fill if hasattr(broker, 'append_fill') else broker.fills.append)(
            {'time': when.strftime('%Y-%m-%d 00:00:00'), 'symbol': k, 'quantity': -q, 'price': px,
             'raw_price': px, 'commission': 0.0, 'slippage': 0.0, 'type': 'OPTION_EXERCISE',
             'order_id': tk.order_id, 'tag': tag})
        ev = broker._emit(tk, OrderStatus.FILLED, px, -q, 0.0, tag, when=when, is_assignment=assignment)
        ev.is_in_the_money = itm
        if assignment:
            algo.on_assignment_order_event(ev)
        self.events.append(f"{k.strip()} {('assigned' if assignment else 'exercised') if itm else 'expired OTM'} "
                           f"{day:%Y-%m-%d}")
        if itm and not cash_settled:
            u = sym.underlying
            uk = str(u).upper()
            mult = contract_multiplier(sym) if sym.security_type != SecurityType.FUTURE_OPTION else 1.0
            uq = (q if call else -q) * mult
            if uk not in algo.securities:
                algo._add(u, None, None)
            utk = algo._make_ticket(u, uq, OrderType.OPTION_EXERCISE,
                                    tag='Option Assignment' if assignment else 'Option Exercise')
            utk._broker = broker
            utk.status = OrderStatus.SUBMITTED
            algo.portfolio.process_fill(uk, uq, K, 0.0)
            utk.average_fill_price = K
            utk.quantity_filled = uq
            utk.status = OrderStatus.FILLED
            utk.last_fill_time = when
            (broker.append_fill if hasattr(broker, 'append_fill') else broker.fills.append)(
                {'time': when.strftime('%Y-%m-%d 00:00:00'), 'symbol': uk, 'quantity': uq,
                 'price': K, 'raw_price': K, 'commission': 0.0, 'slippage': 0.0,
                 'type': 'OPTION_EXERCISE', 'order_id': utk.order_id, 'tag': utk.tag})
            uev = broker._emit(utk, OrderStatus.FILLED, K, uq, 0.0, utk.tag, when=when, is_assignment=assignment)
            uev.is_in_the_money = True
            if assignment:
                algo.on_assignment_order_event(uev)
            usec = algo.securities._securities.get(uk)
            if u.security_type == SecurityType.FUTURE and usec is not None and not u.is_canonical() \
                    and (getattr(usec, 'is_delisted', False) or u.id.date.date() <= day.date()):
                # the delivered futures contract expires today too: settled at its final price
                held = algo.portfolio[uk].quantity
                if held:
                    stk = algo._make_ticket(u, -held, OrderType.MARKET, tag='Liquidate from delisting')
                    stk._broker = broker
                    stk.status = OrderStatus.SUBMITTED
                    broker._fill(stk, uk, S, S, when)

    def exercise(self, session, symbol, quantity):
        """Manual early exercise (American contracts): settle now at the current underlying price."""
        algo = session.algo
        cs = algo.securities.get(str(symbol).upper())
        if cs is None or not is_contract(cs.symbol):
            raise ValueError(f"{symbol} is not a subscribed option contract")
        from .enums import OptionStyle
        if int(cs.symbol.id.option_style) == int(OptionStyle.EUROPEAN) and cs.symbol.id.date.date() > algo.time.date():
            from .orders import OrderResponseErrorCode
            raise ValueError("European options can only be exercised at expiry")
        h = algo.portfolio[str(symbol).upper()]
        if quantity > h.quantity or quantity <= 0:
            raise ValueError("Exercise quantity must be positive and not exceed the long position")
        saved = h.quantity
        # exercise the requested quantity: treat it as expiring now
        day = datetime(algo.time.year, algo.time.month, algo.time.day)
        h.quantity = quantity
        rest = saved - quantity
        try:
            was = cs.is_delisted
            self._expire(session, cs, day, False)
            cs.is_delisted = was
            cs.is_tradable = True
        finally:
            h.quantity += rest

    # ------------------------------------------------------------------ margin
    def group_margin(self, positions, price):
        """Margin of option positions with their underlying, per underlying:
        stock margin + min(sum of per-leg requirements, worst loss at the earliest expiry).
        Per-leg: a long option is paid in full (its value); a short option uncovered needs
        max(20% x S - out-of-the-money amount, 10% x S (calls) / 10% x K (puts)) x multiplier + its value
        (15% / 10% for index options). The worst loss is evaluated on intrinsic values over every strike,
        0 and the current price (bounded only when calls short are covered by long calls or shares)."""
        algo = self._session.algo
        groups = {}
        for t, q in positions.items():
            if not q:
                continue
            sec = algo.securities.get(t)
            if sec is None:
                continue
            sym = sec.symbol
            if is_contract(sym):
                groups.setdefault(str(sym.underlying).upper(), {'opts': [], 'stock': 0.0})['opts'].append((t, sym, q))
        if not groups:
            return 0.0, set()
        broker = self._session.broker
        total, covered = 0.0, set()
        for uk, g in groups.items():
            S = price(uk) if uk in algo.securities else self._underlying_price(g['opts'][0][1].underlying, algo.time)
            stock = positions.get(uk, 0.0) if uk in algo.securities else 0.0
            stock_kind = algo.securities[uk].symbol.security_type if uk in algo.securities else None
            if stock_kind == SecurityType.FUTURE:
                stock = 0.0   # futures underlyings keep their own futures margin
            legs = []
            naked = 0.0
            V_now = 0.0
            for t, sym, q in g['opts']:
                m = contract_multiplier(sym)
                p = price(t)
                K = sym.id.strike_price
                call = int(sym.id.option_right) == int(OptionRight.CALL)
                legs.append((K, call, q * m, sym.id.date))
                val = q * p * m
                V_now += val
                if q > 0:
                    naked += val
                else:
                    pct, floor = (0.15, 0.10) if sym.security_type == SecurityType.INDEX_OPTION else (0.20, 0.10)
                    otm = max(0.0, (K - S) if call else (S - K))
                    per = max(pct * S - otm, floor * (S if call else K))
                    naked += abs(q) * m * per + abs(val)
                covered.add(t)
            # worst loss at expiry (intrinsic values), counting shares held as covering calls
            net_calls = sum(qm for K, call, qm, _e in legs if call) + (stock if stock > 0 else 0.0)
            bounded = net_calls >= -1e-9 and stock >= 0
            req = naked
            if bounded:
                grid = sorted({0.0, S} | {K for K, _c, _q, _e in legs})
                worst = -float('inf')
                for x in grid + [grid[-1] * 1.5 + 1.0]:
                    v = sum(qm * (max(0.0, x - K) if call else max(0.0, K - x)) for K, call, qm, _e in legs)
                    v += stock * x
                    loss = (V_now + stock * S) - v
                    worst = max(worst, loss)
                excess = worst - stock * S
                req = min(naked, max(0.0, excess))
            if stock and uk in algo.securities:
                req += abs(stock * S * broker.multiplier(uk)) * broker.margin_fraction(uk)
                covered.add(uk)
            total += req
        return total, covered

    # ------------------------------------------------------------------ chain / indicators API
    def full_chain(self, canonical, t):
        """Every listed contract of `canonical` at t with quotes (no filter): algorithm.option_chain()."""
        from .option_chain import OptionChain, OptionContract, ChainUnderlying
        syms, S = self.chain_symbols(canonical, t)
        und = ChainUnderlying(canonical.underlying, S, t)
        qf = self.model.quote_fn(S, t, 'close')
        return OptionChain(canonical, t, [OptionContract(s, t, und, quote_fn=qf) for s in syms], und, quote_fn=qf)

    def register_indicator(self, indicator, symbol, kind):
        self._indicators.append((indicator, symbol, kind))

    def _update_indicators(self, sl, t):
        for ind, sym, kind in self._indicators:
            if sym is None or not is_contract(sym):
                continue
            if sym.id.date < datetime(t.year, t.month, t.day):
                continue
            S = self._underlying_price(sym.underlying, t)
            if not S:
                continue
            q = self.model.quote(sym, S, t, 'close')
            g = q.greeks
            v = {'iv': q.implied_volatility, 'delta': g.delta, 'gamma': g.gamma, 'vega': g.vega,
                 'theta': g.theta, 'rho': g.rho}.get(kind, 0.0)
            ind.update(t, float(v or 0.0))

    # ------------------------------------------------------------------ reporting
    def notes(self, session):
        if not self.used:
            return []
        out = []
        seen = set()
        for uk in sorted(self.underlyings):
            sec = session.algo.securities.get(uk)
            target = None
            for c in self.canonicals.values():
                if str(c.symbol.underlying).upper() == uk:
                    target = c.symbol
                    break
            if target is None:
                for k, s in session.algo.securities.items():
                    if is_contract(s.symbol) and str(s.symbol.underlying).upper() == uk:
                        target = s.symbol
                        break
            if target is None:
                continue
            try:
                d = self.model.disclosure(target)
            except Exception:
                d = None
            if d and d not in seen:
                seen.add(d)
                out.append(d)
        out.append("Option exercise and assignment happen at expiry only (in-the-money contracts: equity options "
                   "deliver shares at the strike, index options settle in cash at intrinsic value); early "
                   "assignment of American options before expiry is not simulated.")
        if self.events:
            out.append("Option expiries: " + "; ".join(self.events[:6]) + ("..." if len(self.events) > 6 else ""))
        return out

    def result_flags(self, session):
        if not self.used:
            return {}
        return {'uses_model_priced_derivatives': True,
                'model_priced_disclosures': self.notes(session)[:4]}
