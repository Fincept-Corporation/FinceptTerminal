#!/usr/bin/env python3
"""
Fincept Terminal - Strategy Runner for the Fincept Engine (QCAlgorithm-style strategies)

Loads a strategy from strategies/_registry.py and runs it over real daily bars. Engine-specific
(owned by the fincept engine although it sits in base/): the provider is
Analytics/backtesting/fincept/fincept_provider.py.

DATA. Every bar comes from base.price_store through fincept_engine.price_feed (the OHLCV cache
all engines share: exchange session dates, zero Yahoo requests when warm). The three hooks the
engine calls - fetch_historical_data (split + dividend adjusted), fetch_split_adjusted (split
adjusted + the adjusted close) and fetch_actions (dividends / splits) - keep their names and
formats (end dates EXCLUSIVE); tests override them with synthetic feeds.

One trading day of the loop (no look-ahead: a decision made with bar t's close can only
fill at a later price):
  1. scheduled events timed before the close run with the previous close known; their
     market orders fill at today's open
  2. orders queued earlier fill against today's bar (market: open - or close with
     fill_timing 'next_close'; limit/stop: if the bar's range reaches the price;
     market-on-close: close), with commission + slippage
  3. today's bar is published: prices marked to the close, registered indicators and
     consolidators updated, then on_data(slice) and the framework (alpha -> portfolio
     construction -> risk -> execution); their orders fill from the next bar (fill_timing
     'same_close': market orders placed now fill at this close)
  4. events timed at/after the close, on_end_of_day, borrow fees, margin check, equity recorded
Warm-up (set_warmup) replays the bars before the start date through indicators,
consolidators and on_data with orders ignored. on_end_of_algorithm runs at the end.
"""

import sys
import json
import math
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, Any, List, Optional

# Add strategies directory to path
STRATEGIES_DIR = Path(__file__).resolve().parent.parent.parent.parent / "strategies"
if str(STRATEGIES_DIR) not in sys.path:
    sys.path.insert(0, str(STRATEGIES_DIR))

from fincept_engine import QCAlgorithm, Symbol, TradeBar, Slice
from fincept_engine.enums import Resolution, SecurityType, OrderType, OrderStatus
from fincept_engine.errors import NotSupportedByEngine, not_supported
from fincept_engine.data import DataSource, TRADABLE_TYPES, yahoo_ticker, BAR_CLOSE
from fincept_engine.broker import BacktestBroker
from fincept_engine.session import BacktestSession, call_optional as _call_optional
from fincept_engine import price_feed
from _registry import STRATEGY_REGISTRY
from _loader import resolve_strategy_path

MAX_SYMBOLS = 120         # securities fetched up front (strategies with long hard-coded lists)
_OPTION_TYPES = {SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION}
_CLOSE_DELTA = timedelta(hours=BAR_CLOSE.hour, minutes=BAR_CLOSE.minute)
_OPEN_DELTA = timedelta(hours=9, minutes=30)
_CODE_CACHE: Dict[str, Any] = {}   # strategy file -> compiled code (optimise trials re-run one strategy)


def _sig_round(x: float, digits: int = 8) -> float:
    """Round to `digits` significant digits (drops Yahoo float32 noise, keeps tiny prices)."""
    if not x or x != x or math.isinf(x):
        return x
    return round(x, max(0, digits - 1 - int(math.floor(math.log10(abs(x))))))


class FinceptStrategyRunner:
    """Runs registry strategies over real daily bars (see module docstring)."""

    def __init__(self):
        self.strategy_registry = STRATEGY_REGISTRY
        self.strategies_dir = STRATEGIES_DIR
        self.phase = None  # optional dict the provider's watchdog reads ('initialize' / 'run')

    def _mark(self, name):
        if isinstance(self.phase, dict):
            import time
            self.phase['name'] = name
            self.phase['since'] = time.monotonic()

    def get_strategy_info(self, strategy_id: str) -> Optional[Dict[str, str]]:
        return self.strategy_registry.get(strategy_id)

    def strategy_source(self, strategy_id: str) -> str:
        info = self.get_strategy_info(strategy_id)
        if not info:
            raise ValueError(f"Strategy {strategy_id} not found in registry")
        strategy_path = resolve_strategy_path(self.strategies_dir, info['path'])
        if not strategy_path.exists():
            raise FileNotFoundError(f"Strategy file not found: {strategy_path}")
        with open(strategy_path, 'r', encoding='utf-8') as f:
            return f.read()

    def load_strategy_class(self, strategy_id: str):
        """Dynamically load strategy class from file (the compiled code is cached per process; the
        class is created anew for every run so no state leaks between runs)."""
        info = self.get_strategy_info(strategy_id)
        if not info:
            raise ValueError(f"Strategy {strategy_id} not found in registry")

        # Containment check - the code below exec()s whatever it reads, so the
        # resolved path must stay under strategies_dir. The check lives in
        # strategies/_loader; raises StrategyPathError (a ValueError) if it escapes.
        strategy_path = resolve_strategy_path(self.strategies_dir, info['path'])
        if not strategy_path.exists():
            raise FileNotFoundError(f"Strategy file not found: {strategy_path}")
        key = str(strategy_path)
        code = _CODE_CACHE.get(key)
        if code is None:
            with open(strategy_path, 'r', encoding='utf-8') as f:
                code = compile(f.read(), key, 'exec')
            _CODE_CACHE[key] = code

        namespace = {
            'QCAlgorithm': QCAlgorithm,
            'Symbol': Symbol,
            'TradeBar': TradeBar,
            'Slice': Slice,
            'Resolution': Resolution,
            'SecurityType': SecurityType,
        }
        exec(code, namespace)

        # Only classes DEFINED by the strategy code count (the star import drags in other
        # QCAlgorithm subclasses); the last one defined wins.
        own_module = namespace.get('__name__', 'builtins')
        defined = [obj for obj in namespace.values()
                   if isinstance(obj, type) and issubclass(obj, QCAlgorithm) and obj is not QCAlgorithm
                   and getattr(obj, '__module__', None) == own_module]
        strategy_class = defined[-1] if defined else None
        if strategy_class is None:
            for name, obj in namespace.items():
                if isinstance(obj, type) and issubclass(obj, QCAlgorithm) and obj is not QCAlgorithm:
                    strategy_class = obj
                    break
        if not strategy_class:
            raise ValueError(f"No QCAlgorithm subclass found in {info['path']}")
        return strategy_class

    # ------------------------------------------------------------------ data (base.price_store)

    def fetch_historical_data(self, symbols: List[str], start_date: str,
                              end_date: str, resolution: str = 'daily') -> Dict[str, List[Dict]]:
        """Daily OHLCV, split- and dividend-adjusted, keyed by the Yahoo ticker requested, from the
        shared price store. end_date is EXCLUSIVE; tickers the store cannot serve are absent."""
        return price_feed.default_feed().daily_dicts(list(symbols), start_date, end_date, 'adj')

    def _fetch_for_source(self, tickers, start, end_exclusive):
        return self.fetch_historical_data(tickers, start, end_exclusive, 'daily')

    def fetch_split_adjusted(self, symbols, start_date, end_date):
        """Daily bars adjusted for splits only (dividends not subtracted) plus the adjusted close
        ('adj_close'): option underlyings and strategies that ask for raw prices. end EXCLUSIVE."""
        return price_feed.default_feed().daily_dicts(list(symbols), start_date, end_date, 'raw')

    def fetch_actions(self, yahoo_ticker: str, start: str = None, end: str = None) -> Dict[str, list]:
        """Dividends (split basis and as paid) and splits of a ticker in [start, end). Default start:
        the run's first loaded day; default end: the store's last bar (what turns split-adjusted bars
        back into prices as traded)."""
        rng = getattr(self, '_data_range', None)
        s = start or (rng[0] if rng else '2000-01-01')
        return price_feed.default_feed().actions(yahoo_ticker, s, end)

    # ------------------------------------------------------------------ symbols

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
                if n in (key, self._norm(info['yahoo'])):
                    hit = key
                    break
            (matched if hit else unmatched).append(hit or w)
        return matched, unmatched

    # ------------------------------------------------------------------ run

    def execute_strategy(self, strategy_id: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Run a strategy.

        params: symbols, start_date, end_date (YYYY-MM-DD, inclusive), initial_cash,
                commission (fraction of notional per fill), slippage (fraction of price) - or
                cost_model (base.costs.CostModel), fill_timing ('next_open' | 'next_close' |
                'same_close'), strategy_params (dict, exposed through get_parameter()),
                on_bar (callable(i, n, day, warming): progress / cancel), auto_warm_indicators
                (bool: seed registered indicators with the bars before the start).
        """
        try:
            return self._execute(strategy_id, params)
        except NotSupportedByEngine as e:
            return {'success': False, 'error': str(e), 'not_supported': True, 'feature': e.feature}
        except Exception as e:
            import traceback
            msg = f"{type(e).__name__}: {e}" if not str(e) else str(e)
            where = self._strategy_frame(e)
            if where:   # the strategy's own code raised: say so and where
                msg = f"The strategy's code raised {type(e).__name__}: {e} ({where})"
            return {'success': False, 'error': msg, 'traceback': traceback.format_exc(), 'exception': type(e).__name__,
                    'in_strategy_code': bool(where)}

    def _strategy_frame(self, exc) -> Optional[str]:
        """'<file> line N, in <function>' of the innermost traceback frame inside a strategy file (not the engine)."""
        import traceback
        try:
            frames = traceback.extract_tb(exc.__traceback__)
        except Exception:
            return None
        root = str(self.strategies_dir)
        engine = str(self.strategies_dir / 'fincept_engine')
        for fr in reversed(frames):
            fn = str(fr.filename)
            if fn.startswith(root) and not fn.startswith(engine):
                return f"{Path(fn).name} line {fr.lineno}, in {fr.name}()"
        return None

    def _execute(self, strategy_id: str, params: Dict[str, Any]) -> Dict[str, Any]:
        symbols = params.get('symbols')
        start_date = params.get('start_date')
        end_date = params.get('end_date')
        initial_cash = params.get('initial_cash')
        strategy_params = params.get('strategy_params') or {}
        if not symbols:
            raise ValueError("symbols parameter is required")
        if not start_date:
            raise ValueError("start_date parameter is required")
        if not end_date:
            raise ValueError("end_date parameter is required")
        if initial_cash is None:
            raise ValueError("initial_cash parameter is required")
        start = datetime.strptime(str(start_date)[:10], '%Y-%m-%d')
        end = datetime.strptime(str(end_date)[:10], '%Y-%m-%d')
        if end < start:
            raise ValueError(f"end date {end_date} is before start date {start_date}")
        initial_cash = float(initial_cash)
        price_feed.default_feed().begin_run()

        strategy_class = self.load_strategy_class(strategy_id)
        algorithm = strategy_class()
        self._alias_pascal_handlers(algorithm)

        # The run's dates win over the strategy's hard-coded set_start_date/set_end_date.
        algorithm._forced_start, algorithm._forced_end = start, end
        algorithm._start_date, algorithm._end_date = start, end
        algorithm.time = start
        data = DataSource(start, end, self._fetch_for_source, raw_fetcher=self.fetch_split_adjusted,
                          actions_fetcher=self.fetch_actions)
        self._data_range = (data.fetch_start.strftime('%Y-%m-%d'), (end + timedelta(days=1)).strftime('%Y-%m-%d'))
        algorithm._data = data
        broker = BacktestBroker(algorithm, params.get('commission', 0.0), params.get('slippage', 0.0),
                                cost_model=params.get('cost_model'),
                                fill_timing=params.get('fill_timing') or 'next_open')
        algorithm._broker = broker
        algorithm.portfolio._broker = broker
        algorithm._next_fill_date = start
        if strategy_params:
            algorithm.set_parameters({str(k): _param_text(v) for k, v in strategy_params.items()})

        self._mark('initialize')
        if hasattr(algorithm, 'initialize'):
            algorithm.initialize()
        elif hasattr(algorithm, 'Initialize'):
            algorithm.Initialize()
        self._mark('run')

        # The user's capital wins over the strategy's set_cash().
        algorithm.set_cash(initial_cash)
        if params.get('auto_warm_indicators') and not algorithm._warmup_bars and algorithm._warmup_timedelta is None:
            try:
                algorithm.settings.automatic_indicator_warm_up = True
            except Exception:
                pass

        # ---- what the strategy subscribed that the engine cannot run ----
        if algorithm._custom_data:
            raise not_supported("custom data feeds", f"It subscribes {', '.join(algorithm._custom_data[:4])} "
                                f"through add_data() with a type that has no reader.")

        session = BacktestSession(algorithm, data, broker, start, end, initial_cash, [str(x) for x in symbols],
                                  MAX_SYMBOLS, mark=self._mark)
        session.on_bar = params.get('on_bar')
        self._add_extensions(session)
        out = session.run()
        out['performance']['strategy_id'] = strategy_id
        out['performance']['strategy_name'] = self.get_strategy_info(strategy_id)['name']
        feed = price_feed.default_feed()
        out['price_feed'] = {'provenance': feed.provenance(), 'errors': dict(feed.errors),
                             'network_calls': feed.network_calls}
        return {'success': True, 'data': out}

    def _add_extensions(self, session):
        """Per-run managers: corporate actions, the cash book's conversion rates."""
        from fincept_engine.corporate_actions import CorporateActions
        from fincept_engine.currency import CashBookRates
        rng = self._data_range
        session.extensions.append(CorporateActions(lambda y: self.fetch_actions(y, rng[0], rng[1])))
        session.extensions.append(CashBookRates())
        types = {s.symbol.security_type for s in session.algo.securities.values()}
        if SecurityType.FUTURE in types or any(
                getattr(getattr(s.symbol, 'underlying', None), 'security_type', None) == SecurityType.FUTURE
                for s in session.algo.securities.values()):
            from fincept_engine.futures import FuturesManager
            early_fm = getattr(session.algo, '_futures_manager', None)
            session.extensions.append(early_fm if early_fm is not None else FuturesManager())
        if session.algo._universe_manager.universes() or session.algo._universe_manager._models or \
                getattr(session.algo, '_derivative_models', None) or getattr(session.algo, '_universe_option_filters', None):
            from fincept_engine.universe_ext import UniverseExtension
            session.extensions.insert(0, UniverseExtension(self))
        if getattr(session.algo, '_custom_subscriptions', None):
            from fincept_engine.custom_data import CustomDataManager
            session.extensions.insert(0, CustomDataManager())
        early = getattr(session.algo, '_option_manager', None)
        if early is not None:
            session.extensions.append(early)          # created during initialize()
        elif types & _OPTION_TYPES:
            from fincept_engine.options import OptionManager
            mgr = OptionManager()
            session.extensions.append(mgr)
            session._pending_option_indicators = getattr(session.algo, '_pending_option_indicators', [])

    _HANDLERS = (('initialize', 'Initialize'), ('on_data', 'OnData'), ('on_order_event', 'OnOrderEvent'),
                 ('on_end_of_algorithm', 'OnEndOfAlgorithm'), ('on_securities_changed', 'OnSecuritiesChanged'),
                 ('on_warmup_finished', 'OnWarmupFinished'), ('on_end_of_day', 'OnEndOfDay'))

    @classmethod
    def _alias_pascal_handlers(cls, algorithm):
        """A strategy that defines only the PascalCase handler (OnData, Initialize, ...) gets it
        called: the engine calls the snake_case name, whose base version is a no-op."""
        t = type(algorithm)
        for snake, pascal in cls._HANDLERS:
            mine_snake = getattr(t, snake, None) is not getattr(QCAlgorithm, snake, None)
            p = getattr(t, pascal, None)
            mine_pascal = p is not None and p is not getattr(QCAlgorithm, pascal, None)
            if mine_pascal and not mine_snake:
                setattr(algorithm, snake, getattr(algorithm, pascal))

    def list_strategies(self) -> List[Dict[str, Any]]:
        """All registry entries with their catalogue status (supported / reason / requires)."""
        strategies = []
        for strategy_id, info in self.strategy_registry.items():
            row = {'id': strategy_id, 'name': info['name'], 'category': info['category'], 'path': info['path']}
            for k in ('supported', 'reason', 'requires'):
                if k in info:
                    row[k] = info[k]
            strategies.append(row)
        return strategies


def _param_text(v) -> str:
    """get_parameter() values are strings (LEAN); integral floats from an optimiser grid as '20', not '20.0'."""
    if isinstance(v, bool):
        return str(v).lower()
    if isinstance(v, float) and v.is_integer():
        return str(int(v))
    return str(v)


def main():
    """CLI entry point for testing."""
    if len(sys.argv) < 2:
        print("Usage: python fincept_strategy_runner.py <command> [args]")
        print("Commands:")
        print("  list - List all strategies")
        print("  run <strategy_id> <params_json> - Run a strategy")
        sys.exit(1)

    runner = FinceptStrategyRunner()
    command = sys.argv[1]

    if command == 'list':
        strategies = runner.list_strategies()
        print(json.dumps({'success': True, 'data': strategies, 'count': len(strategies)}, indent=2))
    elif command == 'run':
        if len(sys.argv) < 4:
            print("Usage: python fincept_strategy_runner.py run <strategy_id> <params_json>")
            sys.exit(1)
        result = runner.execute_strategy(sys.argv[2], json.loads(sys.argv[3]))
        print(json.dumps(result, indent=2, default=str))
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)


if __name__ == '__main__':
    main()
