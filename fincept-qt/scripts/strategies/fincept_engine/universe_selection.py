# ============================================================================
# Fincept Terminal - Strategy Engine: universe selection (LEAN UniverseSelection)
#
# UniverseManager owns the universes of one run. The runner calls select(day) once per
# trading day before the open (algorithm time = day 00:00; also on warm-up days) and gets
# back the symbols added to and removed from the algorithm, which it reports through
# on_securities_changed. Semantics follow LEAN's UniverseSelection / Universe /
# PendingRemovalsManager:
#   * fundamental / coarse(+fine) / ETF / user-defined universes select every trading day
#     with the data known at the previous close; a universe whose settings carry a
#     schedule (universe_settings.schedule.on(rule), add_universe(date_rule, fn)) selects
#     on the rule's days, plus once at the first day when the rule fired within the past
#     year (LEAN moves the subscription start back to the last scheduled date);
#     ScheduledUniverse selects on its date rule at its time of day (times after the close
#     run before the next open, with the clock at the scheduled time)
#   * Universe.UNCHANGED keeps the selection; an identical selection is "unchanged" too;
#     coarse UNCHANGED skips the fine step
#   * a deselected member stays until universe_settings.minimum_time_in_universe has passed
#   * a deselected member that is held / has open orders is reported removed once but stays
#     a member ("pending removal") until it is flat; re-selecting it cancels the removal
#   * a symbol is reported added when it joins its first universe and removed when it
#     leaves its last one (a security in two universes is not reported twice)
# Intraday selection schedules collapse to one selection per trading day (disclosed).
# ============================================================================

import bisect
import inspect
from datetime import datetime, timedelta, time as dt_time

from .enums import Market, Resolution, SecurityType
from .errors import not_supported
from .types import Symbol, SecurityIdentifier
from .universe import (UNCHANGED, Universe, UniverseSettings, FundamentalUniverseFactory, CoarseFundamentalUniverse,
                       FineFundamentalFilteredUniverse, FundamentalFilteredUniverse, ETFConstituentsUniverse,
                       UserDefinedUniverse, ScheduledUniverse, ManualUniverse, ConstituentsUniverse, Member,
                       Fundamental, CoarseFundamental, FineFundamental, FundamentalUniverse, ETFConstituentData,
                       UniverseDefinitions, _CountDict, _CountSet, _time_in_universe_ok, _rule_matches,
                       _is_date_rule)

MIDNIGHT = dt_time(0, 0)
MARKET_OPEN = dt_time(9, 30)
MARKET_CLOSE = dt_time(16, 0)

SCHEDULE_NOTE = ("Universe selection runs once per trading day before the open; intraday selection schedules "
                 "and selection times during the session collapse to that one selection.")

_FUNDAMENTAL_TYPES = ('CoarseFundamental', 'FineFundamental', 'Fundamental', 'FundamentalUniverse', 'Fundamentals')
_PERIODIC_RULES = ('every_day', 'month_start', 'month_end', 'week_start', 'week_end', 'year_start', 'year_end')


def _day(x) -> datetime:
    return datetime(x.year, x.month, x.day)


def _key(symbol) -> str:
    return str(getattr(symbol, 'value', symbol)).upper()


def _positional_count(fn):
    try:
        params = inspect.signature(fn).parameters.values()
    except (TypeError, ValueError):
        return None
    if any(p.kind == p.VAR_POSITIONAL for p in params):
        return None
    return sum(1 for p in params if p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD))


def call_flexible(fn, *args):
    """Call fn with the trailing args it accepts: model.select(algorithm, data) or select(data)."""
    n = _positional_count(fn)
    if n is None or n >= len(args):
        return fn(*args)
    return fn(*args[len(args) - n:]) if n > 0 else fn()


class _Changes:
    """Added / removed symbols of one step; an add and a remove of the same symbol cancel."""

    def __init__(self):
        self.added, self.removed = [], []

    def add(self, sym):
        k = _key(sym)
        for i, s in enumerate(self.removed):
            if _key(s) == k:
                del self.removed[i]
                return
        if all(_key(s) != k for s in self.added):
            self.added.append(sym)

    def remove(self, sym):
        k = _key(sym)
        for i, s in enumerate(self.added):
            if _key(s) == k:
                del self.added[i]
                return
        if all(_key(s) != k for s in self.removed):
            self.removed.append(sym)


class UniverseManager:
    """The universes of one run and their daily selection.

    algorithm: the QCAlgorithm (universe_settings, time, end date are read; time is set to each
               selection's time while selectors run)
    provider:  universe_data.UniverseDataProvider (created on first use when None)
    subscribe: subscribe(symbol, universe_settings) -> Security | None - adds a selected security
               to the algorithm (None: it cannot be priced; it is skipped and noted)
    is_held:   is_held(symbol) -> bool - holdings or open orders (deselected held members stay
               until flat)
    calendar:  optional scheduling.TradingCalendar of the run (date rules); built from the NYSE
               calendar when not given
    """

    def __init__(self, algorithm, provider=None, subscribe=None, is_held=None, calendar=None):
        self.algorithm = algorithm
        self._provider = provider
        self._subscribe = subscribe
        self._is_held = is_held
        self.calendar = calendar
        self._own_calendar = None
        self._universes = {}          # universe symbol value -> Universe (insertion order)
        self._models = []             # [{'model', 'keys', 'created'}]
        self._post_initialized = False
        self._first_day = None
        self._prev_day = None
        self._pending = []            # [(universe key, symbol key)] deselected but held members
        self._securities = {}         # symbol key -> security subscribed through a universe
        self._constituents = {}       # symbol key -> latest ETFConstituentData
        self._skipped = []            # selected symbols subscribe() could not add
        self._collapsed = False
        self._prepared = False        # pool bars loaded for the run
        self._user_defined = None     # universe of securities added outside universes (optional)

    # ------------------------------------------------------------------ plumbing
    @property
    def provider(self):
        if self._provider is None:
            from .universe_data import UniverseDataProvider
            self._provider = UniverseDataProvider()
        return self._provider

    @provider.setter
    def provider(self, value):
        """Inject a configured UniverseDataProvider (before the first selection)."""
        self._provider = value
        self._prepared = False

    def _settings_default(self):
        s = getattr(self.algorithm, 'universe_settings', None)
        return s if s is not None else UniverseSettings()

    def _set_time(self, t):
        try:
            self.algorithm.time = t
        except Exception:  # noqa: BLE001 - a read-only clock is fine
            pass

    def _held(self, symbol) -> bool:
        if self._is_held is None:
            return False
        try:
            return bool(self._is_held(symbol))
        except Exception:  # noqa: BLE001
            return False

    def _calendar(self):
        if self.calendar is not None:
            return self.calendar
        algo = self.algorithm
        start = self._first_day or _day(getattr(algo, '_start_date', None) or datetime.now())
        end = getattr(algo, '_end_date', None) or getattr(algo, 'end_date', None) or start
        end = max(_day(end), start)
        cal = self._own_calendar
        if cal is not None and cal.days and cal.days[0] <= start - timedelta(days=400) and end <= cal.days[-1]:
            return cal
        from .scheduling import TradingCalendar
        lo, hi = start - timedelta(days=800), end + timedelta(days=40)
        try:
            from .market_calendar import trading_days
            days = list(trading_days(lo, hi))
        except Exception:  # noqa: BLE001 - weekday fallback
            days, d = [], lo
            while d <= hi:
                if d.weekday() < 5:
                    days.append(d)
                d += timedelta(days=1)
        self._own_calendar = TradingCalendar(days)
        return self._own_calendar

    def _previous_trading_day(self, day):
        days = self._calendar().days
        i = bisect.bisect_left(days, day) - 1
        return days[i] if i >= 0 else None

    # ------------------------------------------------------------------ adding universes
    def add_universe(self, *args, **kwargs):
        """LEAN add_universe overloads. Returns the Universe."""
        args = list(args)
        if not args:
            for k in ('universe', 'selector', 'coarse_selector', 'fine_selector', 'pySelector'):
                if kwargs.get(k) is not None:
                    args.append(kwargs.pop(k))
        if not args:
            raise ValueError("add_universe() needs a universe or a selection function")
        first = args[0]
        if self._is_model(first):
            self.add_universe_selection(first)
            return first
        if isinstance(first, Universe):
            if len(args) > 1 and callable(args[1]):
                return self._register(FundamentalFilteredUniverse(first, args[1]))
            return self._register(first)
        if _is_date_rule(first) and len(args) > 1 and callable(args[1]):
            settings = UniverseSettings(self._settings_default())
            settings.schedule.on(first)
            return self._register(FundamentalUniverseFactory(Market.USA, settings, args[1]))
        if isinstance(first, type):
            return self._add_typed(first, args[1:])
        if isinstance(first, SecurityType):
            # (security_type, name, resolution, market, universe_settings, selector(datetime))
            rest = args[1:]
            name = next((a for a in rest if isinstance(a, str)), None)
            markets = [a for a in rest if isinstance(a, str)]
            return self._add_user_defined(name, rest, security_type=first,
                                          market=markets[1] if len(markets) > 1 else Market.USA)
        if isinstance(first, str):
            return self._add_user_defined(first, args[1:])
        if callable(first):
            if len(args) > 1 and callable(args[1]):
                coarse = CoarseFundamentalUniverse(self._settings_default(), first)
                return self._register(FineFundamentalFilteredUniverse(coarse, args[1]))
            return self._register(FundamentalUniverse.usa(first))
        raise ValueError(f"QCAlgorithm.add_universe: {first!r} is not a valid argument.")

    def _add_typed(self, data_type, rest):
        name = getattr(data_type, '__name__', str(data_type))
        if name in _FUNDAMENTAL_TYPES:
            selector = next((a for a in rest if callable(a) and not isinstance(a, type)), None)
            settings = next((a for a in rest if isinstance(a, UniverseSettings)), None)
            return self._register(FundamentalUniverseFactory(Market.USA, settings, selector))
        if name in ('ETFConstituentData', 'ETFConstituentUniverse'):
            raise not_supported("an ETF constituents universe added by data type",
                                "Use self.universe.etf(ticker, ...) instead.")
        raise not_supported(f"a custom data universe ({name})",
                            "Universes built from custom data files are not available yet.")

    def _add_user_defined(self, name, rest, security_type=SecurityType.EQUITY, market=Market.USA):
        resolution = next((a for a in rest if isinstance(a, Resolution)), Resolution.DAILY)
        settings = next((a for a in rest if isinstance(a, UniverseSettings)), None)
        selector = next((a for a in rest if callable(a) and not isinstance(a, (type, UniverseSettings))), None)
        if selector is None:
            raise ValueError(f"add_universe('{name}', ...) needs a selection function of the date/time")
        if int(resolution) < int(Resolution.DAILY):
            self._collapsed = True
        u = UserDefinedUniverse(name=name, security_type=security_type, market=market, resolution=resolution,
                                universe_settings=settings or self._settings_default(), selector=selector)
        return self._register(u)

    def _register(self, u):
        if isinstance(u, ConstituentsUniverse) or isinstance(getattr(u, 'universe', None), ConstituentsUniverse):
            raise not_supported("a constituents-file universe (ConstituentsUniverse)",
                                "It reads custom universe data files; a custom-data module will come later.")
        if u.universe_settings is None:
            u.universe_settings = self._settings_default()
        if isinstance(u, UserDefinedUniverse) and u.interval < timedelta(days=1):
            self._collapsed = True            # intraday refresh -> one selection a day (disclosed)
        key = _key(u.symbol)
        if key in self._universes:            # LEAN UniverseManager.TryAdd: first one wins
            return self._universes[key]
        self._universes[key] = u
        if isinstance(u, ManualUniverse):
            self._add_manual_members(u)
        return u

    def _add_manual_members(self, u):
        """A fixed list is subscribed up front (the runner reports it with the other initial
        securities); its members never change."""
        t = getattr(self.algorithm, 'time', None)
        syms = self._normalize(u.select_symbols(t), SecurityType.EQUITY, Market.USA)
        u._selected_order = syms
        u.selected = _CountSet(syms)
        u._previous = frozenset(_key(s) for s in syms)
        for s in syms:
            sec = self._security_for(s, u)
            if sec is not None:
                u.securities[_key(s)] = Member(t, sec)

    # ------------------------------------------------------------------ framework models
    @staticmethod
    def _is_model(x) -> bool:
        if isinstance(x, (Universe, type)) or not hasattr(x, 'create_universes') and not hasattr(x, 'CreateUniverses'):
            return False
        return True

    @staticmethod
    def _manual_symbols(model):
        fn = getattr(model, 'symbols_list', None)
        return list(fn()) if callable(fn) else None

    def add_universe_selection(self, model):
        """LEAN add_universe_selection: the model's universes are created after initialize()
        (post_initialize / first select); a manual list is subscribed right away."""
        entry = {'model': model, 'keys': [], 'created': False}
        self._models.append(entry)
        if self._post_initialized or self._manual_symbols(model) is not None:
            self._create(entry)

    def set_universe_selection(self, model):
        """LEAN set_universe_selection: replaces the models added so far."""
        for entry in self._models:
            for k in entry['keys']:
                self._universes.pop(k, None)
        self._models = []
        self.add_universe_selection(model)

    def _create(self, entry):
        model = entry['model']
        manual = self._manual_symbols(model)
        if manual is not None:
            universes = [ManualUniverse(manual, getattr(model, 'universe_settings', None)
                                        or getattr(model, '_universe_settings', None))]
        else:
            fn = getattr(model, 'create_universes', None) or getattr(model, 'CreateUniverses')
            universes = list(fn(self.algorithm) or [])
        keys = []
        for u in universes:
            if not isinstance(u, Universe):
                raise not_supported(f"the universe selection model {type(model).__name__}",
                                    f"Its create_universes() returned {type(u).__name__}, not a Universe.")
            u = self._register(u)
            keys.append(_key(u.symbol))
        entry['keys'], entry['created'] = keys, True

    def post_initialize(self):
        """Create the universes of the framework models (LEAN FrameworkPostInitialize). Call it after
        initialize(); select() calls it if it was not called."""
        self._post_initialized = True
        for entry in self._models:
            if not entry['created']:
                self._create(entry)

    def _refresh_models(self, day):
        from .algorithm import UniverseSelectionModel as _Base
        base_fn = getattr(_Base, 'get_next_refresh_time_utc', None)
        for entry in self._models:
            m = entry['model']
            fn = getattr(type(m), 'get_next_refresh_time_utc', None)
            if fn is None or fn is base_fn or not entry['created']:
                continue
            try:
                when = m.get_next_refresh_time_utc()
            except Exception:  # noqa: BLE001
                continue
            if when is None or when > day:
                continue
            new_keys = []
            for u in list(m.create_universes(self.algorithm) or []):
                if isinstance(u, Universe):
                    new_keys.append(_key(self._register(u).symbol))
            for k in entry['keys']:
                if k not in new_keys and k in self._universes:
                    self._universes[k].dispose()
            entry['keys'] = new_keys

    # ------------------------------------------------------------------ selection
    def select(self, now):
        """Run the universes due before the open of `now`'s day. Returns (added, removed) symbols."""
        day = _day(now)
        if not self._post_initialized:
            self.post_initialize()
        if self._first_day is None:
            self._first_day = day
        self._refresh_models(day)
        changes = _Changes()
        for key in list(self._universes):
            u = self._universes.get(key)
            if u is None:
                continue
            for t in self._selection_times(u, day):
                self._apply(u, t, day, changes)
            if u.dispose_requested and not u.securities:
                del self._universes[key]
        self._prev_day = day
        return list(changes.added), list(changes.removed)

    def _selection_times(self, u, day):
        kind = u._kind
        if kind == 'manual':
            return []
        if u.dispose_requested:
            return [day]
        if kind == 'scheduled':
            return self._scheduled_times(u, day)
        sched = getattr(u.universe_settings, 'schedule', None)
        if kind in ('fundamental', 'coarse', 'coarse_fine', 'filtered') and sched is not None \
                and getattr(sched, 'initialized', False):
            rule = sched.date_rule
            if day == self._first_day:
                return [day] if self._fired_within_year(rule, day) else []
            return [day] if _rule_matches(rule, day, self._calendar()) else []
        return [day]

    def _fired_within_year(self, rule, day):
        rt = getattr(rule, 'rule_type', None)
        if rt in _PERIODIC_RULES or (rt == 'every' and getattr(rule, 'days', None)):
            return True
        if rt == 'on':
            lo = day - timedelta(days=365)
            return any(lo <= _day(d) <= day for d in (getattr(rule, 'dates', None) or []))
        cal = self._calendar()
        days = cal.days
        hi = bisect.bisect_right(days, day)
        lo = bisect.bisect_left(days, day - timedelta(days=365))
        return any(_rule_matches(rule, d, cal) for d in days[lo:hi])

    @staticmethod
    def _time_of_day(time_rule):
        if time_rule is None or getattr(time_rule, 'rule_type', None) == 'every':
            return MIDNIGHT                   # LEAN every(interval) starts at midnight
        t = getattr(time_rule, 'time', None)
        return t if isinstance(t, dt_time) else MIDNIGHT

    def _scheduled_times(self, u, day):
        cal = self._calendar()
        tod = self._time_of_day(u.time_rule)
        if getattr(u.time_rule, 'rule_type', None) == 'every' or MARKET_OPEN <= tod < MARKET_CLOSE:
            self._collapsed = True
        out = []
        if tod >= MARKET_CLOSE and self._prev_day is not None:
            prev = self._previous_trading_day(day)
            if prev is not None and prev >= self._first_day and _rule_matches(u.date_rule, prev, cal):
                out.append(datetime.combine(prev.date(), tod))
        if tod < MARKET_CLOSE and _rule_matches(u.date_rule, day, cal):
            out.append(datetime.combine(day.date(), tod))
        return out

    def _apply(self, u, t, day, changes):
        """LEAN UniverseSelection.ApplyUniverseSelection for one universe at time t."""
        self._set_time(t)
        result = [] if u.dispose_requested else self._select(u, t, day)
        if result is UNCHANGED:
            self._check_pending(u, {_key(s) for s in u.selected})
            return
        u._selected_order = result
        u.selected = _CountSet(result)
        sel = {_key(s) for s in result}
        self._check_pending(u, sel)
        uk = _key(u.symbol)
        minimum = getattr(u.universe_settings, 'minimum_time_in_universe', None)
        for sk in sorted(u.securities):
            if sk in sel:
                continue
            m = u.securities[sk]
            if not u.dispose_requested and not _time_in_universe_ok(t - m.added, minimum):
                continue
            pending = (uk, sk) in self._pending
            sym = getattr(m.security, 'symbol', None) or sk
            if not pending:
                # LEAN announces the removal even when another universe still selects the security
                # (UniverseSelection.ApplyUniverseSelection: "not checking if other universes have this
                # security still selected"); the security keeps its data while another universe has it.
                changes.remove(sym)
            if self._held(sym):
                if not pending:
                    self._pending.append((uk, sk))
            else:
                del u.securities[sk]
                if pending:
                    self._pending.remove((uk, sk))
        for s in result:
            sk = _key(s)
            if sk in u.securities:
                continue
            sec = self._security_for(s, u)
            if sec is None:
                continue
            elsewhere = self._member_elsewhere(sk, u)
            u.securities[sk] = Member(t, sec)
            if not elsewhere:
                changes.add(getattr(sec, 'symbol', None) or s)

    def _check_pending(self, u, selected_keys):
        for uk, sk in list(self._pending):
            v = self._universes.get(uk)
            m = v.securities.get(sk) if v is not None else None
            if m is None:
                self._pending.remove((uk, sk))
                continue
            safe = not self._held(getattr(m.security, 'symbol', None) or sk)
            if safe or (v is u and sk in selected_keys):
                if safe:
                    del v.securities[sk]
                self._pending.remove((uk, sk))

    def _member_elsewhere(self, sk, u) -> bool:
        for k, v in self._universes.items():
            if v is u or sk not in v.securities:
                continue
            if (k, sk) not in self._pending:
                return True
        return False

    def _security_for(self, sym, u):
        sk = _key(sym)
        sec = self._securities.get(sk)
        if sec is not None:
            return sec
        if self._subscribe is None:
            raise RuntimeError("UniverseManager was created without a subscribe callback")
        sec = self._subscribe(sym, u.universe_settings or self._settings_default())
        if sec is None:
            if sk not in self._skipped:
                self._skipped.append(sk)
            return None
        self._securities[sk] = sec
        return sec

    # ------------------------------------------------------------------ the data of each universe type
    def _run_end(self, day):
        algo = self.algorithm
        end = getattr(algo, '_end_date', None) or getattr(algo, 'end_date', None)
        return max(_day(end), day) if end is not None else day

    def _coarse(self, day):
        p = self.provider
        if not self._prepared:
            p.prepare(day, self._run_end(day))       # the whole run's bars in one batched load
            self._prepared = True
        data = p.coarse(day)
        for f in data:
            f.__dict__['_batch'] = data
        return data

    def _select(self, u, t, day):
        kind = u._kind
        if kind in ('fundamental', 'coarse'):
            return self._perform(u, u.select_symbols(t, self._coarse(day)))
        if kind in ('coarse_fine', 'filtered'):
            inner = u.universe
            data = self._data_for(inner, t, day)
            res = inner.select_symbols(t, data)
            if res is UNCHANGED:
                return UNCHANGED
            syms = self._normalize(res, SecurityType.EQUITY, Market.USA)
            wanted = {_key(s) for s in syms}
            if data and isinstance(data[0], CoarseFundamental):
                fine, seen = [], set()
                for f in data:
                    k = _key(f.symbol)
                    if k in wanted and k not in seen and f.has_fundamental_data:
                        seen.add(k)
                        fine.append(f)
            else:
                fine = [self.provider.fundamental(s, t) for s in syms]
            for f in fine:
                f.__dict__['_batch'] = fine
            return self._perform(u.fine_fundamental_universe, u.fine_fundamental_universe.select_symbols(t, fine))
        if kind == 'etf':
            return self._perform(u, u.select_symbols(t, self._data_for(u, t, day)))
        if kind in ('user', 'scheduled'):
            st = getattr(u, 'security_type_of_members', SecurityType.EQUITY)
            mk = getattr(u, 'market_of_members', Market.USA)
            return self._perform(u, u.select_symbols(t), st, mk)
        return self._perform(u, u.select_symbols(t, None))

    def _data_for(self, u, t, day):
        kind = u._kind
        if kind in ('fundamental', 'coarse'):
            return self._coarse(day)
        if kind == 'etf':
            data = self.provider.etf_constituents(u.etf_symbol, day)
            for c in data:
                self._constituents[_key(c.symbol)] = c
            return data
        return None

    def _perform(self, u, res, security_type=SecurityType.EQUITY, market=Market.USA):
        """LEAN Universe.PerformSelection: UNCHANGED, or the selection when it differs from the last one."""
        if res is UNCHANGED:
            return UNCHANGED
        syms = self._normalize(res, security_type, market)
        cur = frozenset(_key(s) for s in syms)
        prev = u._previous if u._previous is not None else frozenset()
        u._previous = cur
        if cur == prev:
            return UNCHANGED
        return syms

    def _normalize(self, res, security_type=SecurityType.EQUITY, market=Market.USA):
        if res is None:
            raise TypeError("The universe selection function returned None; return a list of symbols "
                            "(or Universe.UNCHANGED).")
        if isinstance(res, (str, Symbol, SecurityIdentifier)):
            res = [res]
        out, seen = [], set()
        equity_usa = security_type == SecurityType.EQUITY and str(market).lower() == Market.USA
        for x in res:
            if isinstance(x, Symbol):
                s = x
            elif isinstance(x, SecurityIdentifier):
                s = Symbol.create(x.symbol, x.security_type, x.market)
            elif hasattr(x, 'symbol') and isinstance(getattr(x, 'symbol', None), Symbol):
                s = x.symbol
            else:
                t = str(x).strip().upper()
                s = self.provider.symbol(t) if (equity_usa and self._provider is not None) else \
                    Symbol.create(t, security_type, market)
            k = _key(s)
            if k not in seen:
                seen.add(k)
                out.append(s)
        return out

    # ------------------------------------------------------------------ queries for the runner
    def is_member(self, symbol) -> bool:
        k = _key(symbol)
        return any(k in u.securities for u in self._universes.values())

    def active_symbols(self):
        """Keys of every current member of every universe (pending removals included)."""
        out = {}
        for u in self._universes.values():
            for k, m in u.securities.items():
                out.setdefault(k, getattr(m.security, 'symbol', None) or k)
        return out

    def active_securities(self):
        out = _CountDict()
        for u in self._universes.values():
            for k, m in u.securities.items():
                sym = getattr(m.security, 'symbol', None) or k
                out.setdefault(sym, m.security)
        return out

    def constituent_data(self, symbol):
        """The latest ETFConstituentData seen for a symbol (security.cache data), or None."""
        return self._constituents.get(_key(symbol))

    def fundamentals(self, symbols, as_of=None):
        """algorithm.fundamentals(symbol | [symbols]): Fundamental objects as of `as_of` (default the
        algorithm time), from the last bar complete then and the fundamentals snapshot."""
        t = as_of or getattr(self.algorithm, 'time', None) or datetime.now()
        if isinstance(symbols, (str, Symbol)):
            return self.provider.fundamental(symbols, t)
        out = [self.provider.fundamental(s, t) for s in symbols]
        for f in out:
            f.__dict__['_batch'] = out
        return out

    def register_manual_security(self, security):
        """Optional: put a security added with add_equity & co. into the user-defined universe (as
        LEAN does) so universe_manager counts it; it never produces changes."""
        if self._user_defined is None:
            self._user_defined = ManualUniverse([], self._settings_default(),
                                                symbol=Symbol("QC-UNIVERSE-USERDEFINED-USA-EQUITY",
                                                              SecurityType.BASE, Market.USA))
            self._universes[_key(self._user_defined.symbol)] = self._user_defined
        sym = getattr(security, 'symbol', security)
        k = _key(sym)
        self._user_defined.securities.setdefault(k, Member(getattr(self.algorithm, 'time', None), security))
        self._securities.setdefault(k, security)

    def universes(self):
        return list(self._universes.values())

    def notes(self):
        """Disclosures that apply to this run (pool, fundamentals snapshot, ETF holdings, timing)."""
        out = list(self._provider.notes()) if self._provider is not None else []
        if self._collapsed:
            out.append(SCHEDULE_NOTE)
        if self._skipped:
            out.append(f"Selected but without price data, so not added: {', '.join(self._skipped[:8])}"
                       f"{'...' if len(self._skipped) > 8 else ''}.")
        return out

    def view(self):
        return UniverseManagerView(self)


class UniverseManagerView:
    """algorithm.universe_manager: universes keyed by universe symbol (LEAN UniverseManager)."""

    def __init__(self, manager):
        self._m = manager

    def _find(self, key):
        return self._m._universes.get(_key(key))

    def __getitem__(self, key):
        u = self._find(key)
        if u is None:
            raise KeyError(key)
        return u

    def get(self, key, default=None):
        u = self._find(key)
        return u if u is not None else default

    def __contains__(self, key):
        return self._find(key) is not None

    contains_key = __contains__
    ContainsKey = __contains__

    def __iter__(self):
        return iter([u.symbol for u in self._m._universes.values()])

    def __len__(self):
        return len(self._m._universes)

    def keys(self):
        return [u.symbol for u in self._m._universes.values()]

    def values(self):
        return list(self._m._universes.values())

    def items(self):
        return [(u.symbol, u) for u in self._m._universes.values()]

    @property
    def count(self):
        return len(self._m._universes)

    Count = count

    @property
    def active_securities(self):
        return self._m.active_securities()

    ActiveSecurities = active_securities

    def __repr__(self):
        return f"UniverseManager({', '.join(str(u.symbol) for u in self._m._universes.values())})"
