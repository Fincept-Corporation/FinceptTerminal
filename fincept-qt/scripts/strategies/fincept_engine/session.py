# ============================================================================
# Fincept Terminal - Strategy Engine: one backtest run (the day loop)
#
# One trading day of the loop (no look-ahead: a decision made with bar t's close can only
# fill at a later price):
#   0. before the open: universe selection with the data known at the previous close,
#      securities added/removed (on_securities_changed and the framework models)
#   1. scheduled events timed before the close run with the previous close known; their
#      market orders fill at today's open
#   2. orders queued earlier fill against today's bar (market: open; limit/stop: if the
#      bar's range reaches the price; market-on-close: close), with commission + slippage
#   3. today's bar is published: prices marked to the close, registered indicators and
#      consolidators updated, then on_data(slice) and the framework (alpha -> portfolio
#      construction -> risk -> execution); their orders fill from the next bar
#   4. events timed at/after the close, on_end_of_day, derivative expiries, margin check,
#      equity recorded
# Warm-up (set_warmup) replays the bars before the start date through indicators,
# consolidators and on_data with orders ignored. on_end_of_algorithm runs at the end.
#
# Securities can appear during the run (add_equity in on_data, universe selection,
# option and future contracts): each is priced from the moment it is subscribed, from
# the same DataSource that serves history() (so the loop and history() agree).
# ============================================================================

import inspect
import sys
from datetime import datetime, timedelta
from typing import Any, Dict, List, Optional

from .data import TRADABLE_TYPES, BAR_CLOSE, yahoo_ticker
from .enums import OrderStatus, OrderType, SecurityType
from .errors import not_supported
from .indicators import IndicatorBase, IndicatorDataPoint, feed_indicator
from .scheduling import MARKET_CLOSE, TradingCalendar
from .types import Slice, Symbol, TradeBar

CLOSE_DELTA = timedelta(hours=BAR_CLOSE.hour, minutes=BAR_CLOSE.minute)
OPEN_DELTA = timedelta(hours=9, minutes=30)
OPTION_TYPES = {SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION}


def call_optional(fn, *args):
    """Call a strategy hook that may or may not declare the optional arguments."""
    try:
        params = inspect.signature(fn).parameters
        n = sum(1 for p in params.values() if p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD))
        has_var = any(p.kind == p.VAR_POSITIONAL for p in params.values())
    except (TypeError, ValueError):
        return fn(*args)
    return fn(*args) if has_var or n >= len(args) else fn(*args[:n])


class BacktestSession:
    """Runs one initialized algorithm over the requested period and returns the result data."""

    def __init__(self, algorithm, data, broker, start: datetime, end: datetime, initial_cash: float,
                 user_symbols: List[str], max_symbols: int, mark=None):
        self.algo = algorithm
        self.data = data
        self.broker = broker
        self.start = start
        self.end = end
        self.initial_cash = initial_cash
        self.user_symbols = [str(x) for x in user_symbols]
        self.max_symbols = max_symbols
        self._mark = mark or (lambda name: None)
        # key -> BarSeries of every security being priced (grows during the run)
        self.series: Dict[str, Any] = {}
        self._resolved: Dict[str, bool] = {}      # key -> priced?
        self._announced = set()                   # keys already reported in on_securities_changed
        self.unpriceable: List[str] = []
        self.missing_other: List[str] = []
        self.skipped: List[str] = []
        self.notes: List[str] = []
        self.state = {'ind_len': -1, 'ind_index': {}, 'started': False, 'active_ids': set()}
        self.calendar = None
        self.extensions = []                      # per-day hooks of derivative/universe managers
        self._pending_added: List[str] = []
        self._pending_removed: List[str] = []
        self.filtered_bars = 0
        self._custom_bars = {}
        self._retired = set()
        self._n_resolved_seen = -1
        # on_bar(i, n, day, warming): called at the start of every day of the loop (the provider's
        # progress lines and cancel check); position_values: per equity point {key: signed value in
        # the account currency} of the positions held at the close (multi-asset weights)
        self.on_bar = None
        self.position_values: List[Dict[str, float]] = []
        algorithm._session = self
        algorithm._equity_points = []
        # The run's capital is the only starting money: balances the strategy set in other currencies
        # (set_cash("BTC", 1)) are dropped (noted in the result).
        self.dropped_balances = []
        book = algorithm.portfolio.cash_book
        for code, c in list(book.items()):
            if code != book.account_currency and c.amount:
                self.dropped_balances.append(f"{c.amount:g} {code}")
                c.set_amount(0.0)

    # ------------------------------------------------------------------ subscriptions

    @staticmethod
    def _norm(s: str) -> str:
        return str(s or '').strip().upper().replace('/', '')

    def _match_symbols(self, tradable, wanted):
        """User symbols may be the strategy's spelling (BTCUSD) or Yahoo's (BTC-USD)."""
        matched, unmatched = [], []
        for w in wanted:
            n = self._norm(w)
            hit = None
            for key, info in tradable.items():
                if n in (self._norm(key), self._norm(info['yahoo'])):
                    hit = key
                    break
            (matched if hit else unmatched).append(hit or w)
        return matched, unmatched

    def _classify(self, sec):
        """('yahoo', yahoo_ticker) | ('derived', None) | (None, None)."""
        st = sec.symbol.security_type
        key = str(sec.symbol).upper()
        if self.data.has_provider(st, sec.symbol):
            return 'derived', None
        y, _note = yahoo_ticker(key, st, getattr(sec.symbol, 'market', None))
        if st in TRADABLE_TYPES and y:
            return 'yahoo', y
        return None, None

    def apply_price_mode(self, sec):
        """RAW / SPLIT_ADJUSTED subscriptions (add_equity(..., data_normalization_mode=...), a security
        initializer's set_data_normalization_mode, subscription configs) load such bars."""
        if sec.symbol.security_type not in (SecurityType.EQUITY,):
            return
        mode = getattr(sec, '_data_normalization_mode', None)
        if mode is None:
            for c in self.algo.subscription_manager.get_subscription_data_configs(str(sec.symbol).upper()):
                m = getattr(c, '__dict__', {}).get('data_normalization_mode')
                if m is not None and int(m) != 1:
                    mode = m
        if mode is None:
            return
        m = int(mode)
        if m == 0:
            self.data.set_mode(sec.symbol, 'raw')
        elif m == 2:
            self.data.set_mode(sec.symbol, 'split_adjusted')

    def _price_new_securities(self) -> List[str]:
        """Resolve the series of securities subscribed since the last call. Returns the keys
        that became priced."""
        added = []
        secs = self.algo.securities._securities
        if len(secs) == self._n_resolved_seen:
            return added
        self._n_resolved_seen = len(secs)
        pending = [(k, s) for k, s in list(secs.items()) if k not in self._resolved]
        if not pending:
            return added
        for _k, s in pending:
            self.apply_price_mode(s)
        yahoo_items = []
        for key, sec in pending:
            kind, y = self._classify(sec)
            if kind == 'yahoo':
                yahoo_items.append((key, sec.symbol.security_type, getattr(sec.symbol, 'market', None)))
        if len(yahoo_items) > 1:
            self.data.preload(yahoo_items)
        for key, sec in pending:
            kind, _y = self._classify(sec)
            if kind is None:
                self._resolved[key] = False
                continue
            s = self.data.series(sec.symbol, sec.symbol.security_type, getattr(sec.symbol, 'market', None))
            self._resolved[key] = s is not None
            if s is not None:
                self.series[key] = s
                added.append(key)
            elif kind == 'yahoo':
                self.missing_other.append(key)
        return added

    # ------------------------------------------------------------------ set-up

    def prepare(self):
        """Decide what is priced and on which calendar; raise when nothing can run."""
        algo, data = self.algo, self.data
        for hook in self.extensions:
            hook.after_initialize(self)
        secs = list(algo.securities.values())
        tradable, unpriceable = {}, []
        derived = {}
        for s in secs:
            kind, y = self._classify(s)
            key = str(s.symbol).upper()
            if kind == 'yahoo':
                tradable[key] = {'security': s, 'type': s.symbol.security_type, 'yahoo': y}
            elif kind == 'derived':
                derived[key] = {'security': s, 'type': s.symbol.security_type, 'yahoo': key}
            elif kind is None and not any(h.owns(s) for h in self.extensions):
                unpriceable.append(f"{key} ({getattr(s.symbol.security_type, 'name', s.symbol.security_type)})")
        self.unpriceable = unpriceable
        wanted = self.user_symbols
        free_symbols = any(h.free_symbols for h in self.extensions)
        if not tradable and derived:
            # only derived instruments (custom data feeds, futures contracts): those are what it trades
            free_symbols = True
        if not tradable and not free_symbols:
            detail = f" It subscribes only {', '.join(unpriceable[:6])}." if unpriceable else ""
            raise ValueError("This strategy subscribes to no symbol the Fincept engine can price "
                             f"(daily Yahoo bars for equities, ETFs, indices, forex, crypto and common futures).{detail}")
        matched, unmatched = self._match_symbols(tradable, wanted)
        if not matched and not free_symbols:
            shown = [f"{k}" + (f" (Yahoo: {v['yahoo']})" if v['yahoo'] != k else '')
                     for k, v in list(tradable.items())[:8]]
            raise ValueError(
                f"This strategy trades {', '.join(shown)} (hard-coded in its initialize()); "
                f"it ignores {', '.join(wanted)}. Enter {next(iter(tradable))} as the symbol, "
                f"or pick a strategy that trades your instrument.")
        self.matched, self.unmatched = matched, unmatched
        self.tradable = tradable

        # price modes the strategy asked for (raw / split-adjusted), before any series is loaded
        for s in secs:
            self.apply_price_mode(s)
        # the user's symbols first, then the rest of what the strategy subscribed
        keys = list(dict.fromkeys(matched + list(tradable)))
        self.keys_all = keys
        print(f"Fetching data for {keys[:self.max_symbols]}...", file=sys.stderr)
        self.skipped = keys[self.max_symbols:]
        for k in self.skipped:
            self._resolved[k] = False
        load = keys[:self.max_symbols]
        data.preload([(k, tradable[k]['type'], getattr(tradable[k]['security'].symbol, 'market', None))
                      for k in load])
        for key in load:
            info = tradable[key]
            data.series(info['security'].symbol, info['type'], getattr(info['security'].symbol, 'market', None))
        missing_user = [k for k in matched if not data.has(k)]
        if missing_user:
            k = missing_user[0]
            raise ValueError(f"No historical data for {k} (Yahoo ticker {tradable[k]['yahoo']}) between "
                             f"{self.start:%Y-%m-%d} and {self.end:%Y-%m-%d} (Yahoo Finance returned nothing - "
                             f"check the symbol).")
        for k in load:
            self._resolved[k] = data.has(k)
            if data.has(k):
                self.series[k] = data.series(tradable[k]['security'].symbol)
            else:
                self.missing_other.append(k)
        # derived / other securities subscribed in initialize()
        self._price_new_securities()

        try:
            from .algorithm_imports import SymbolCache
            SymbolCache._known = {k: algo.securities[k].symbol for k in self.series if k in algo.securities}
        except Exception:
            pass
        self.raw_mode = [k for k in self.series
                         if any(getattr(c, '__dict__', {}).get('data_normalization_mode') == 0
                                for c in algo.subscription_manager.get_subscription_data_configs(k))]

        cal_series = [s for k, s in self.series.items() if self._resolved.get(k)]
        for h in self.extensions:
            cal_series.extend(h.calendar_series(self))
        all_days = sorted({d for s in cal_series for d in s.dates})
        days = [d for d in all_days if self.start <= d <= self.end]
        if not days:
            shown = ', '.join(matched) if matched else ', '.join(list(self.series)[:4]) or 'the subscribed symbols'
            raise ValueError(f"No historical data for {shown} between {self.start:%Y-%m-%d} and {self.end:%Y-%m-%d}.")
        self.all_days = all_days
        self.days = days
        self.calendar = TradingCalendar(all_days)

    # ------------------------------------------------------------------ per-day helpers

    def bars_on(self, d: datetime) -> Dict[str, Dict]:
        out = {}
        secs = self.algo.securities
        retired = self._retired
        for key, s in self.series.items():
            if key in retired:
                continue
            dates = s.dates
            if dates and dates[-1] < d:
                # a series that ended (expired contract): skip it from now on
                if key not in self.algo.portfolio._holdings or not self.algo.portfolio._holdings[key].quantity:
                    retired.add(key)
                continue
            i = s.index_of_date(d)
            if i is not None:
                sec = secs.get(key)
                if sec is not None and getattr(sec, '_removed', False):
                    h = self.algo.portfolio[key] if key in self.algo.portfolio else None
                    if h is None or not h.quantity:
                        continue
                out[key] = s.bar(i)
        return out

    def indicator_index(self):
        # register appends; unregister / re-register replace the list - either changes the key
        reg = self.algo._registered_indicators
        key = (id(reg), len(reg))
        if key != self.state['ind_len']:
            idx = {}
            for e in reg:
                for sym in e['symbols']:
                    idx.setdefault(sym, []).append(e)
            self.state['ind_index'] = idx
            self.state['ind_len'] = key
        return self.state['ind_index']

    def consolidators_for(self, key):
        algo = self.algo
        out, seen = [], set()
        for c in algo._consolidators:
            cons = c.get('consolidator') if isinstance(c, dict) else None
            if cons is not None and str(c.get('symbol', '')).upper() == key and id(cons) not in seen:
                seen.add(id(cons))
                out.append(cons)
        for cfg in algo.subscription_manager.subscriptions:
            cons = getattr(cfg, '__dict__', {}).get('consolidator')
            if cons is not None and str(cfg.symbol).upper() == key and id(cons) not in seen:
                seen.add(id(cons))
                out.append(cons)
        return out

    def make_bar(self, key, b, d):
        sec = self.algo.securities[key]
        bar = TradeBar(time=d, symbol=sec.symbol, open=b['open'], high=b['high'], low=b['low'],
                       close=b['close'], volume=b['volume'])
        bar.end_time = d + CLOSE_DELTA
        bar.period = timedelta(days=1)
        return bar

    def publish(self, d, bars_today):
        """Mark prices, update indicators and consolidators; return the slice."""
        algo = self.algo
        now = d + CLOSE_DELTA
        algo.time = now
        sl = Slice(now)
        prices = {}
        for key, b in bars_today.items():
            sec = algo.securities.get(key)
            if sec is None:
                continue
            bar = self.make_bar(key, b, d)
            custom = getattr(self.series.get(key), 'custom', False)
            flt = getattr(sec, 'data_filter', None)
            if flt is not None and hasattr(flt, 'filter'):
                try:
                    keep = flt.filter(sec, bar)
                except TypeError:
                    keep = True
                if not keep:
                    self.filtered_bars += 1
                    continue
            sec.update_price(b['close'], b['open'], b['high'], b['low'], b['volume'])
            if 'bid_close' in b:
                sec.bid_price, sec.ask_price = b['bid_close'], b['ask_close']
            else:
                sec.bid_price = sec.ask_price = b['close']
            sec._last_bar = bar
            vm = sec.volatility_model
            if vm is not None and type(vm).__name__ != 'NullVolatilityModel':
                vm.update(sec, bar)
            prices[key] = b['close']
            if custom:
                self._custom_bars[key] = bar      # the custom point itself goes into the slice
            else:
                sl.add(sec.symbol, bar)
        if prices:
            algo.portfolio.update_market_prices(prices)
        # LEAN order: prices are updated, then OnSecuritiesChanged, then dividends/splits and the
        # consolidators (so indicators created in on_securities_changed get this bar), then OnData.
        self.flush_changes()
        for h in self.extensions:
            h.decorate_slice(self, sl, d)
        sl._ensure_chains()
        algo.current_slice = sl
        idx = self.indicator_index()
        feed = list(sl.bars.items()) + [(k, b) for k, b in self._custom_bars.items()]
        self._custom_bars = {}
        for key, bar in feed:
            k = str(key).upper()
            for cons in self.consolidators_for(k):
                cons.update(bar)
            for e in idx.get(k, ()):
                sel = e['selector']
                ind = e['indicator']
                if sel is None:
                    feed_indicator(ind, bar)
                else:
                    v = sel(bar)
                    if isinstance(ind, IndicatorBase) and isinstance(v, (int, float)):
                        ind.update(bar.end_time, float(v))
                    else:
                        feed_indicator(ind, v if not isinstance(v, (int, float))
                                       else IndicatorDataPoint(float(v), bar.end_time))
        return sl

    def deliver(self, sl):
        """on_splits / on_dividends / on_delistings / on_symbol_changed_events, then on_data."""
        algo = self.algo
        from .algorithm import QCAlgorithm
        for coll, name in ((sl.dividends, 'on_dividends'), (sl.splits, 'on_splits'),
                           (sl.delistings, 'on_delistings'), (sl.symbol_changed_events, 'on_symbol_changed_events')):
            if len(coll):
                fn = getattr(algo, name, None)
                if fn is not None and getattr(fn, '__func__', None) is not getattr(QCAlgorithm, name, None):
                    fn(coll)
        if len(sl) or sl.has_data or len(sl.splits) or len(sl.dividends) or len(sl.delistings) \
                or len(sl.symbol_changed_events) or len(sl.option_chains) or len(sl.future_chains):
            algo.on_data(sl)

    def run_events(self, d, before_close: bool):
        algo = self.algo
        for when, ev in algo.schedule.due_on(d, self.calendar):
            if (when.time() < MARKET_CLOSE) != before_close:
                continue
            algo.time = when
            ev.callback()
            ev.fire_count += 1

    def queue_changes(self, added_keys=(), removed_keys=()):
        """Securities added/removed; reported at the next published bar (prices known)."""
        for k in added_keys:
            if k not in self._pending_added:
                self._pending_added.append(k)
        for k in removed_keys:
            if k in self._pending_added:
                self._pending_added.remove(k)
            elif k not in self._pending_removed:
                self._pending_removed.append(k)

    def flush_changes(self):
        added, removed = self._pending_added, self._pending_removed
        self._pending_added, self._pending_removed = [], []
        if added or removed:
            self.announce(added, removed)

    def announce(self, added_keys=(), removed_keys=()):
        """on_securities_changed + framework models for securities that became priced/removed."""
        from .algorithm import SecurityChanges
        algo = self.algo
        added = [algo.securities[k] for k in added_keys if k in algo.securities and k not in self._announced]
        for k in added_keys:
            self._announced.add(k)
        removed = [algo.securities[k] for k in removed_keys if k in algo.securities]
        for k in removed_keys:
            self._announced.discard(k)
        if not added and not removed:
            return
        changes = SecurityChanges(added=added, removed=removed)
        algo._fe_changes = changes
        call_optional(algo.on_securities_changed, changes)
        for model in (algo._alpha_model, algo._portfolio_construction,
                      algo._execution_model, algo._risk_management):
            if model is not None and hasattr(model, 'on_securities_changed'):
                model.on_securities_changed(algo, changes)

    def securities_added(self):
        keys = [k for k in self.series if self._resolved.get(k) and self._announce_ok(k)]
        self.queue_changes(keys)

    def _announce_ok(self, key):
        return not any(h.silent_security(key) for h in self.extensions)

    def end_of_day(self, bars_today):
        from .algorithm import QCAlgorithm
        algo = self.algo
        fn = getattr(algo, 'on_end_of_day', None)
        if fn is None or getattr(fn, '__func__', None) is QCAlgorithm.on_end_of_day:
            return
        for key in bars_today:
            sec = algo.securities.get(key)
            if sec is not None and not sec.symbol.security_type in OPTION_TYPES:
                call_optional(fn, sec.symbol)

    def new_day(self, d, warming: bool):
        """Before the open of day d (algorithm time = d 00:00): universes, newly subscribed securities."""
        self.algo.time = d
        added, removed = [], []
        gone = getattr(self.algo, '_removed_securities', None)
        if gone:
            removed.extend(gone)
            self.algo._removed_securities = []
        for h in self.extensions:
            a, r = h.before_open(self, d, warming)
            added.extend(a)
            removed.extend(r)
        newly = self._price_new_securities()
        if self.state['started']:
            self.queue_changes(list(dict.fromkeys(added + [k for k in newly if self._announce_ok(k)])), removed)

    # ------------------------------------------------------------------ the run

    def seed_prices(self, first_day):
        """Before the first session the price of every security is its last close before that
        session (known then), so a morning event on the first day can size and place orders, as
        LEAN's data would allow. on_data is not called for these bars."""
        algo = self.algo
        prices = {}
        for key, s in self.series.items():
            sec = algo.securities.get(key)
            if sec is None or sec.price:
                continue
            n = s.count_known_at(first_day)
            if n <= 0:
                continue
            i = n - 1
            sec.update_price(s.close[i], s.open[i], s.high[i], s.low[i], s.volume[i])
            sec.bid_price = sec.ask_price = s.close[i]
            sec._last_bar = self.make_bar(key, s.bar(i), s.dates[i])
            prices[key] = s.close[i]
        if prices:
            algo.portfolio.update_market_prices(prices)

    def run(self) -> Dict[str, Any]:
        algo, broker = self.algo, self.broker
        self.prepare()
        days, all_days = self.days, self.all_days
        start = self.start

        # ---- warm-up ----
        warm_days = []
        pre = [d for d in all_days if d < start]
        if algo._warmup_bars:
            warm_days = pre[-algo._warmup_bars:]
        elif algo._warmup_timedelta is not None:
            warm_days = [d for d in pre if d >= start - algo._warmup_timedelta]
        auto_warm = bool(getattr(algo.settings, 'automatic_indicator_warm_up', False))
        if auto_warm and not warm_days:
            self.auto_warm_indicators()

        on_bar = self.on_bar
        if warm_days:
            algo._is_warming_up = True
            self.securities_added()
            self.state['started'] = True
            for wi, d in enumerate(warm_days):
                if on_bar is not None:
                    on_bar(wi, len(warm_days), d, True)
                algo._next_fill_date = d
                self.new_day(d, True)
                bars_today = self.bars_on(d)
                self.run_events(d, True)
                sl = self.publish(d, bars_today)
                self.deliver(sl)
                self.framework_step(sl)
                self.run_events(d, False)
                for h in self.extensions:
                    h.after_close(self, d, True)
            algo._is_warming_up = False
        # LEAN calls on_warmup_finished once, with or without a warm-up period.
        algo.on_warmup_finished()
        self.seed_prices(days[0])

        # ---- main loop ----
        equity_curve = []
        stopped_early = None
        n_days = len(days)
        for di, d in enumerate(days):
            if on_bar is not None:
                on_bar(di, n_days, d, False)
            if algo._quit_requested:
                stopped_early = getattr(algo, '_quit_message', '') or 'quit() called'
                break
            # 0. before the open (universe selection sees the previous close; orders fill today)
            algo._next_fill_date = d
            self.new_day(d, False)
            bars_today = self.bars_on(d)
            # 1. events before the close: they know only the previous close; fill at today's open
            self.run_events(d, True)
            # 2. fills on today's bar
            algo.time = d + OPEN_DELTA
            broker.process_bar(d, bars_today)
            # 3. today's bar
            algo._next_fill_date = d + timedelta(days=1)
            if not self.state['started']:
                self.securities_added()
                self.state['started'] = True
            bars_today = self.bars_on(d)  # securities subscribed by morning events
            sl = self.publish(d, bars_today)
            self.deliver(sl)
            self.framework_step(sl)
            # 4. after the close
            self.run_events(d, False)
            algo.time = d + CLOSE_DELTA
            self.end_of_day(bars_today)
            # fill timing 'same_close': market orders decided on this bar fill at its close
            broker.fill_same_close(d, bars_today)
            for h in self.extensions:
                h.after_close(self, d, False)
            self.margin_interest(d)
            broker.accrue_borrow(d + CLOSE_DELTA)
            mcm = getattr(algo.portfolio, 'margin_call_model', None)
            if not getattr(mcm, 'is_null', False):
                broker.check_margin(d + CLOSE_DELTA)
            tpv = algo.portfolio.total_portfolio_value
            algo._equity_points.append(tpv)
            equity_curve.append({'time': d.strftime('%Y-%m-%d 00:00:00'), 'equity': tpv})
            self._snap_positions()
            if algo._quit_requested:
                stopped_early = getattr(algo, '_quit_message', '') or 'quit() called'
                self.fill_after_quit(d, equity_curve)
                break

        if not equity_curve:  # stopped before the first day (quit() in initialize)
            equity_curve.append({'time': days[0].strftime('%Y-%m-%d 00:00:00'),
                                 'equity': algo.portfolio.total_portfolio_value})
            self._snap_positions()
        algo.on_end_of_algorithm()
        return self.results(equity_curve, warm_days, stopped_early)

    def _traded_bars(self, shown, days, max_symbols: int = 300, max_rows: int = 150_000):
        """Bars of the TRADED instruments that are not in the result's bars (model-priced option contracts,
        derived futures contracts) over the days they were held - for each trade's MAE / MFE and exposure."""
        import bisect
        span: Dict[str, list] = {}
        for f in self.broker.fills:
            k = str(f.get('symbol', ''))
            d = str(f.get('time', ''))[:10]
            if k in shown or k not in self.series or not d:
                continue
            s = span.setdefault(k, [d, d])
            s[0], s[1] = min(s[0], d), max(s[1], d)
        held = {t for t, h in self.algo.portfolio._held() if h.quantity}
        last = days[-1] if days else None
        out, rows = {}, 0
        for k, (a, b) in list(span.items())[:max_symbols]:
            s = self.series.get(k)
            try:
                lo = datetime.strptime(a, '%Y-%m-%d')
                hi = last if (k in held and last is not None) else datetime.strptime(b, '%Y-%m-%d')
                i0, i1 = bisect.bisect_left(s.dates, lo), bisect.bisect_right(s.dates, hi)
                if i1 <= i0 or rows + (i1 - i0) > max_rows:
                    continue
                rng = range(i0, i1)
                out[k] = {'dates': [s.dates[i].strftime('%Y-%m-%d') for i in rng],
                          'open': [float(s.open[i]) for i in rng], 'high': [float(s.high[i]) for i in rng],
                          'low': [float(s.low[i]) for i in rng], 'close': [float(s.close[i]) for i in rng],
                          'volume': [float(s.volume[i] or 0.0) for i in rng]}
                rows += i1 - i0
            except Exception:
                continue
        return out

    def _snap_positions(self):
        """Signed value (account currency) of every position held at this close."""
        pf = self.algo.portfolio
        snap = {}
        for t, h in pf._held():
            q = h.quantity
            if not q:
                continue
            try:
                snap[t] = float(h.holdings_value) * (pf._rate(pf.quote_currency(t)) or 1.0)
            except Exception:
                continue
        self.position_values.append(snap)

    def fill_after_quit(self, d, equity_curve):
        """Orders the strategy placed before quit() (e.g. closing its positions) fill at the next open,
        as they would in LEAN; nothing else runs that day."""
        from .enums import OrderType as _OT
        broker = self.broker
        pending = [tk for tk in broker.book if tk.order_type in (_OT.MARKET, _OT.MARKET_ON_OPEN, _OT.COMBO_MARKET)]
        if not pending:
            return
        nxt = [x for x in self.days if x > d][:1]
        if not nxt:
            return
        day = nxt[0]
        self.algo.time = day + OPEN_DELTA
        broker.process_bar(day, self.bars_on(day))
        equity_curve.append({'time': day.strftime('%Y-%m-%d 00:00:00'),
                             'equity': self.algo.portfolio.total_portfolio_value})
        self._snap_positions()

    def margin_interest(self, d):
        """Margin interest models the strategy set (security.set_margin_interest_rate_model): applied
        once per day after the close."""
        algo = self.algo
        from .models import MarginInterestRateParameters
        for sec in list(algo.securities.values()):
            model = getattr(sec, 'margin_interest_rate_model', None)
            fn = getattr(model, 'apply_margin_interest_rate', None)
            if fn is None or getattr(model, '_is_null', False):
                continue
            fn(MarginInterestRateParameters(sec, d + CLOSE_DELTA))

    # ------------------------------------------------------------------ results

    def results(self, equity_curve, warm_days, stopped_early) -> Dict[str, Any]:
        algo, broker, data = self.algo, self.broker, self.data
        initial_cash = self.initial_cash
        days = self.days
        final_equity = equity_curve[-1]['equity'] if equity_curve else initial_cash
        total_return = ((final_equity - initial_cash) / initial_cash) * 100 if initial_cash else 0.0
        orders = list(algo.transactions._orders.values())
        open_at_end = [t for t in orders if t.status in (OrderStatus.SUBMITTED, OrderStatus.PARTIALLY_FILLED,
                                                         OrderStatus.UPDATE_SUBMITTED)]
        open_positions = []
        last_day = days[-1].strftime('%Y-%m-%d 00:00:00')
        for ticker, h in algo.portfolio.get_holdings_summary().items():
            open_positions.append({'symbol': ticker, 'quantity': h['quantity'], 'avg_price': h['avg_price'],
                                   'market_price': h['market_price'], 'time': last_day})
        rejected = {}
        for _t, reason in broker.rejected:
            rejected[reason.split(':')[0]] = rejected.get(reason.split(':')[0], 0) + 1
        notes = list(data.notes) + list(algo._engine_notes)
        for h in self.extensions:
            for n in h.notes(self):
                if n not in notes:
                    notes.append(n)
        if self.skipped:
            notes.append(f"Only the first {self.max_symbols} of the strategy's {len(self.keys_all)} symbols were "
                         f"loaded (skipped: {', '.join(self.skipped[:6])}{'...' if len(self.skipped) > 6 else ''}).")
        if self.missing_other:
            miss = list(dict.fromkeys(self.missing_other))
            notes.append(f"No Yahoo data for {', '.join(miss[:6])}{'...' if len(miss) > 6 else ''}; "
                         f"the strategy ran without it.")
        if self.unpriceable:
            notes.append(f"No data source for {', '.join(self.unpriceable[:6])}; never priced or filled.")
        if self.unmatched:
            notes.append(f"Ignored symbol(s) the strategy does not trade: {', '.join(self.unmatched)}.")
        extra = [k for k in self.tradable if k not in self.matched and k in self.series]
        if extra:
            notes.append(f"The strategy also trades {', '.join(extra[:8])} (hard-coded in its code); "
                         f"their data was loaded too.")
        if self.dropped_balances:
            notes.append("The run's capital replaced the strategy's own starting balances ("
                         + ", ".join(self.dropped_balances) + ").")
        sd = algo._strategy_dates
        if sd.get('start') or sd.get('end'):
            notes.append("The strategy's own start/end dates were replaced by the requested period.")
        if stopped_early:
            notes.append(f"The strategy stopped itself early (quit): {stopped_early}.")
        if self.filtered_bars:
            notes.append(f"{self.filtered_bars} bar(s) were dropped by the strategy's security data filter.")
        from .enums import Resolution
        intraday = sorted({k for k, s in algo.securities._securities.items()
                           if isinstance(getattr(s, 'resolution', None), int)
                           and int(s.resolution) in (int(Resolution.SECOND), int(Resolution.MINUTE),
                                                     int(Resolution.HOUR), int(Resolution.TICK))
                           and k in self.series})
        if intraday:
            notes.append(f"The strategy subscribes {', '.join(intraday[:5])}{'...' if len(intraday) > 5 else ''} at "
                         f"intraday resolution; this engine steps through daily bars (free intraday history covers "
                         f"only recent weeks), so its logic ran once per trading day at the close.")
        if broker.model_notes:
            by = {}
            for t, what in sorted(broker.model_notes):
                by.setdefault(what, []).append(t)
            for what, ts in by.items():
                notes.append(f"{what.capitalize()} of {', '.join(ts[:5])} came from the strategy's own {what} model, "
                             f"not the run's settings.")
        bm = getattr(algo.brokerage_model, 'brokerage_name', None)
        if bm and bm not in ('DEFAULT', 'Default'):
            notes.append(f"The strategy selects the {bm} brokerage model: its order rules are not modelled and its "
                         f"fee schedule is not applied (the run's commission rate is).")
        for t, (asked, cap) in broker.leverage_capped.items():
            notes.append(f"{t}: the strategy asked for {asked:g}x leverage; the engine allows at most {cap:g}x.")

        shown = {k: s for k, s in self.series.items() if getattr(s, 'show_in_bars', True)}
        out = {
            'open_positions': open_positions,
            'unfilled_orders': len(open_at_end),
            'unfilled_order_types': sorted({t.order_type.name for t in open_at_end}),
            'unsimulated_order_types': sorted(broker.unsimulated_types),
            'non_market_orders': sum(1 for t in orders if t.order_type != OrderType.MARKET),
            'orders_submitted': len(orders),
            'orders_filled': len(broker.fills),
            'orders_rejected': len(broker.rejected),
            'rejection_reasons': rejected,
            'warmup_orders_ignored': broker.ignored_warmup,
            'margin_calls': broker.margin_calls,
            'total_fees': broker.total_fees,
            'total_slippage': broker.total_slippage,
            'total_borrow': getattr(broker, 'total_borrow', 0.0),
            'carry_open': {k: dict(v) for k, v in (getattr(broker, 'carry', None) or {}).items() if v},
            'corporate_events': list(getattr(broker, 'corporate_events', None) or []),
            'fill_timing': getattr(broker, 'fill_timing', 'next_open'),
            'commission_rate': broker.commission_rate,
            'slippage_rate': broker.slippage_rate,
            'warmup_bars': len(warm_days),
            'symbols': list(shown.keys()),
            'symbol_map': {k: s.yahoo for k, s in shown.items()},
            'symbol_mapping': [dict(v) for k, v in getattr(data, 'mapping', {}).items() if k in self.series],
            'position_values': self.position_values,
            'run_days': [d.strftime('%Y-%m-%d') for d in days],
            # the last session before the first day (where the starting capital stands when orders placed in
            # initialize() fill at the first bar's open)
            'previous_session': next((d.strftime('%Y-%m-%d') for d in reversed(self.all_days or []) if d < days[0]),
                                     None) if days else None,
            'security_types': {k: str(getattr(getattr(getattr(self.algo.securities.get(k), 'symbol', None),
                                                      'security_type', None), 'name', '') or '')
                               for k in shown},
            'price_modes': {k: data.mode_of(k) for k in shown},
            'notes': notes,
            'performance': {
                'total_return': total_return,
                'final_equity': final_equity,
                'initial_cash': initial_cash,
                'total_trades': len(broker.fills),
            },
            'trades': broker.fills,
            'equity': equity_curve,
            'bars': {k: {'dates': [d.strftime('%Y-%m-%d') for d in s.dates],
                         'open': s.open, 'high': s.high, 'low': s.low, 'close': s.close,
                         'volume': s.volume} for k, s in shown.items()},
        }
        out['traded_bars'] = self._traded_bars(shown, days)
        flags = {}
        for h in self.extensions:
            flags.update(h.result_flags(self) or {})
        out.update(flags)
        if algo._runtime_statistics:
            out['runtime_statistics'] = dict(algo._runtime_statistics)
        return out

    # ------------------------------------------------------------------ helpers

    def auto_warm_indicators(self):
        """settings.automatic_indicator_warm_up: seed each registered indicator with the
        bars before the start date it needs."""
        algo = self.algo
        for e in algo._registered_indicators:
            ind = e['indicator']
            n = int(getattr(ind, 'warm_up_period', 0) or getattr(ind, 'period', 0) or 0)
            for sym in e['symbols'][:1]:
                s = self.series.get(sym)
                if s is None or n <= 0:
                    continue
                hi = s.count_known_at(self.start)
                for i in range(max(0, hi - n), hi):
                    bar = TradeBar(time=s.dates[i], symbol=algo.securities[sym].symbol, open=s.open[i],
                                   high=s.high[i], low=s.low[i], close=s.close[i], volume=s.volume[i])
                    bar.end_time = s._ends[i]
                    feed_indicator(ind, e['selector'](bar) if e['selector'] else bar)

    def framework_step(self, sl):
        """alpha.update -> insights -> portfolio construction -> risk -> execution."""
        algo = self.algo
        state = self.state
        alpha = algo._alpha_model
        pcm = algo._portfolio_construction
        if alpha is None and pcm is None and not algo._pending_insights:
            return
        now = algo.utc_time
        new = list(alpha.update(algo, sl) or []) if alpha is not None else []
        new += algo._pending_insights
        algo._pending_insights = []
        for ins in new:
            if callable(ins.period) and not isinstance(ins.period, timedelta):
                close = ins.period(now)          # an Expiry function
                period = close - now
            else:
                period = ins.period if isinstance(ins.period, timedelta) else timedelta(days=float(ins.period or 1))
            ins.period = period
            ins.generated_time_utc = now
            ins.close_time_utc = now + period
            if not getattr(ins, 'source_model', None) and alpha is not None:
                ins.source_model = getattr(alpha, 'name', type(alpha).__name__)
        if new:
            algo.insights.add_range(new)
            handlers = algo.insights_generated
            if handlers._handlers:
                from .algorithm_imports import GeneratedInsightsCollection
                handlers(algo, GeneratedInsightsCollection(*new))
        if pcm is None:
            return
        latest = {}
        for ins in algo.insights:
            if ins.close_time_utc > now:
                latest[str(ins.symbol).upper()] = ins
        ids = {id(i) for i in latest.values()}
        changed = bool(new) or ids != state['active_ids']
        state['active_ids'] = ids
        targets = []
        if changed:
            targets = [t for t in (pcm.create_targets(algo, list(latest.values())) or []) if t is not None]
            covered = {str(t.symbol).upper() for t in targets}
            for ticker, h in algo.portfolio.items():
                if h.quantity and ticker not in latest and ticker not in covered:
                    from .types import PortfolioTarget
                    targets.append(PortfolioTarget(algo.securities[ticker].symbol, 0))
        risk = algo._risk_management
        if risk is not None:
            # LEAN: the risk model returns overrides; they replace the targets of their symbols
            from .algorithm import merge_risk_targets
            targets = merge_risk_targets(risk.manage_risk(algo, list(targets)), targets)
        if not targets:
            return
        dedup = {}
        for t in targets:
            dedup[str(t.symbol).upper()] = t
        execution = algo._execution_model
        if execution is None:
            from .algorithm import ImmediateExecutionModel
            execution = ImmediateExecutionModel()
        execution.execute(algo, list(dedup.values()))


class SessionExtension:
    """Base for the per-run managers (universes, option chains, futures, custom data)."""
    free_symbols = False   # True: the strategy chooses its securities; the user's symbols are not required

    def after_initialize(self, session):
        pass

    def owns(self, security) -> bool:
        return False

    def calendar_series(self, session):
        return []

    def before_open(self, session, day, warming):
        return [], []

    def decorate_slice(self, session, sl, day):
        pass

    def after_close(self, session, day, warming):
        pass

    def silent_security(self, key) -> bool:
        return False

    def notes(self, session):
        return []

    def result_flags(self, session):
        return {}
