"""
Base Provider for Backtesting Engines
Abstract base class that all Python backtesting providers must implement
Mirrors the host IBacktestingProvider interface
"""

from abc import ABC, abstractmethod
from typing import Dict, Any, List, Optional
from dataclasses import dataclass, asdict
from datetime import datetime
import json


# ============================================================================
# Data Classes (Python equivalents of host interfaces)
# ============================================================================

@dataclass
class PerformanceMetrics:
    """Performance metrics for backtest results"""
    total_return: float
    annualized_return: float
    sharpe_ratio: float
    sortino_ratio: float
    max_drawdown: float
    win_rate: float
    loss_rate: float
    profit_factor: float
    volatility: float
    calmar_ratio: float
    total_trades: int
    winning_trades: int
    losing_trades: int
    average_win: float
    average_loss: float
    largest_win: float
    largest_loss: float
    average_trade_return: float
    expectancy: float
    alpha: Optional[float] = None
    beta: Optional[float] = None
    max_drawdown_duration: Optional[int] = None
    information_ratio: Optional[float] = None
    treynor_ratio: Optional[float] = None


@dataclass
class BacktestStatistics:
    """General statistics about the backtest"""
    start_date: str
    end_date: str
    initial_capital: float
    final_capital: float
    total_fees: float
    total_slippage: float
    total_trades: int
    winning_days: int
    losing_days: int
    average_daily_return: float
    best_day: float
    worst_day: float
    consecutive_wins: int
    consecutive_losses: int
    max_drawdown_date: Optional[str] = None
    recovery_time: Optional[int] = None


@dataclass
class Trade:
    """Single trade record"""
    id: str
    symbol: str
    entry_date: str
    side: str  # 'long' or 'short'
    quantity: float
    entry_price: float
    commission: float
    slippage: float
    exit_date: Optional[str] = None
    exit_price: Optional[float] = None
    pnl: Optional[float] = None
    pnl_percent: Optional[float] = None
    holding_period: Optional[int] = None
    exit_reason: Optional[str] = None  # 'signal', 'stop_loss', 'take_profit', 'time_limit'


@dataclass
class EquityPoint:
    """Single point on equity curve"""
    date: str
    equity: float
    returns: float
    drawdown: float
    benchmark: Optional[float] = None


@dataclass
class BacktestResult:
    """Complete backtest result"""
    id: str
    status: str  # 'completed', 'failed', 'running', 'cancelled'
    performance: PerformanceMetrics
    trades: List[Trade]
    equity: List[EquityPoint]
    statistics: BacktestStatistics
    logs: List[str]
    error: Optional[str] = None
    start_time: Optional[str] = None
    end_time: Optional[str] = None
    duration: Optional[int] = None
    charts: Optional[List[Dict[str, Any]]] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        return {
            'id': self.id,
            'status': self.status,
            'performance': asdict(self.performance),
            'trades': [asdict(t) for t in self.trades],
            'equity': [asdict(e) for e in self.equity],
            'statistics': asdict(self.statistics),
            'logs': self.logs,
            'error': self.error,
            'start_time': self.start_time,
            'end_time': self.end_time,
            'duration': self.duration,
            'charts': self.charts or []
        }


@dataclass
class OptimizationResult:
    """Optimization run result"""
    id: str
    status: str
    best_parameters: Dict[str, Any]
    best_result: BacktestResult
    all_results: List[Dict[str, Any]]
    iterations: int
    duration: int
    error: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        return {
            'id': self.id,
            'status': self.status,
            'best_parameters': self.best_parameters,
            'best_result': self.best_result.to_dict(),
            'all_results': self.all_results,
            'iterations': self.iterations,
            'duration': self.duration,
            'error': self.error
        }


@dataclass
class HistoricalData:
    """Historical price data"""
    symbol: str
    timeframe: str
    data: List[Dict[str, Any]]  # List of price bars


@dataclass
class IndicatorResult:
    """Technical indicator calculation result"""
    indicator: str
    symbol: str
    values: List[Dict[str, Any]]


# ============================================================================
# Base Provider Class
# ============================================================================

class BacktestingProviderBase(ABC):
    """
    Abstract base class for all backtesting providers

    All provider implementations (Lean, Backtrader, VectorBT, etc.)
    must inherit from this class and implement all abstract methods.

    This ensures platform independence - the terminal can work with
    any backtesting engine through this unified interface.
    """

    def __init__(self):
        self.config: Optional[Dict[str, Any]] = None

    # ========================================================================
    # Properties (must be defined in subclasses)
    # ========================================================================

    @property
    @abstractmethod
    def name(self) -> str:
        """Provider name (e.g., "QuantConnect Lean", "Backtrader")"""
        pass

    @property
    @abstractmethod
    def version(self) -> str:
        """Provider version"""
        pass

    @property
    @abstractmethod
    def capabilities(self) -> Dict[str, Any]:
        """
        Provider capabilities
        Example:
        {
            'backtesting': True,
            'optimization': True,
            'liveTrading': False,
            'research': False,
            'multiAsset': ['stocks', 'options', 'crypto'],
            'indicators': True,
            'customStrategies': True,
            'maxConcurrentBacktests': 5
        }
        """
        pass

    # ========================================================================
    # Abstract Methods (must be implemented by all providers)
    # ========================================================================

    @abstractmethod
    def initialize(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """
        Initialize the provider with configuration

        Args:
            config: Provider-specific configuration dictionary

        Returns:
            Dictionary with {'success': bool, 'message': str, 'error': Optional[str]}
        """
        pass

    @abstractmethod
    def test_connection(self) -> Dict[str, Any]:
        """
        Test connection to provider

        Returns:
            Dictionary with {'success': bool, 'message': str, 'error': Optional[str]}
        """
        pass

    @abstractmethod
    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """
        Run a backtest

        Args:
            request: Backtest request dictionary containing:
                - strategy: Strategy definition
                - startDate: Start date string
                - endDate: End date string
                - initialCapital: Initial capital amount
                - assets: List of asset selections
                - parameters: Strategy parameters
                - benchmark: Optional benchmark symbol

        Returns:
            Dictionary representation of BacktestResult
        """
        pass

    @abstractmethod
    def get_historical_data(self, request: Dict[str, Any]) -> List[Dict[str, Any]]:
        """
        Get historical price data

        Args:
            request: Data request dictionary containing:
                - symbols: List of symbols
                - startDate: Start date string
                - endDate: End date string
                - timeframe: Timeframe ('daily', 'minute', etc.)

        Returns:
            List of HistoricalData dictionaries
        """
        pass

    @abstractmethod
    def calculate_indicator(self, indicator_type: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """
        Calculate technical indicator

        Args:
            indicator_type: Type of indicator ('sma', 'ema', 'rsi', etc.)
            params: Indicator parameters (period, etc.)

        Returns:
            Dictionary representation of IndicatorResult
        """
        pass

    # ========================================================================
    # Optional Methods (providers can override if supported)
    # ========================================================================

    def optimize(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """
        Run parameter optimization (optional)

        Args:
            request: Optimization request dictionary

        Returns:
            Dictionary representation of OptimizationResult

        Raises:
            NotImplementedError if optimization not supported
        """
        raise NotImplementedError(f"Optimization not supported by {self.name}")

    def disconnect(self) -> None:
        """Clean up resources (no-op for subprocess-based providers)"""
        pass

    def validate_strategy(self, strategy: Dict[str, Any]) -> Dict[str, Any]:
        """
        Validate strategy before running (optional)

        Args:
            strategy: Strategy definition dictionary

        Returns:
            Dictionary with {'valid': bool, 'errors': List[str], 'warnings': List[str]}
        """
        errors = []
        warnings = []

        if not strategy.get('name'):
            errors.append('Strategy name is required')

        if not strategy.get('type'):
            errors.append('Strategy type is required')

        return {
            'valid': len(errors) == 0,
            'errors': errors,
            'warnings': warnings
        }

    # ========================================================================
    # Helper Methods
    # ========================================================================

    def _generate_id(self) -> str:
        """Generate unique ID for backtest/optimization runs"""
        import uuid
        return str(uuid.uuid4())

    def _current_timestamp(self) -> str:
        """Get current timestamp as ISO string"""
        return datetime.utcnow().isoformat() + 'Z'

    def _ensure_initialized(self) -> None:
        """No-op: Each subprocess is fresh, no persistent state needed"""
        pass

    def _ensure_connected(self) -> None:
        """No-op: Each subprocess is fresh, no persistent state needed"""
        pass

    def _create_success_result(self, message: str) -> Dict[str, Any]:
        """Create success result dictionary"""
        return {
            'success': True,
            'message': message
        }

    def _create_error_result(self, error: str) -> Dict[str, Any]:
        """Create error result dictionary"""
        return {
            'success': False,
            'message': 'Operation failed',
            'error': error
        }

    def _log(self, message: str) -> None:
        """Log message.

        Routed to stderr so the C++ host's `extract_json` (which scans stdout
        for the first '{' or '[') is never fooled by a leading bracket from
        a log line like "[Backtesting.py] Starting...". Stdout is reserved
        exclusively for the final JSON response.
        """
        import sys
        print(f"[{self.name}] {message}", file=sys.stderr)

    def _error(self, message: str, exception: Optional[Exception] = None) -> None:
        """Log error message"""
        error_msg = f"[{self.name}] ERROR: {message}"
        if exception:
            error_msg += f"\n{str(exception)}"
        print(error_msg, file=__import__('sys').stderr)


# ============================================================================
# Utility Functions
# ============================================================================

_PROCESS_T0 = __import__('time').time()

# Keys whose VALUE is a data map: its own keys are names chosen by the user or the data
# (strategy parameter ids, symbols) and must reach the screen unchanged - camel-casing them
# turned a parameter 'fast_period' into 'fastPeriod', which the strategy then did not know (B7).
_PROTECTED_MAP_KEYS = frozenset({
    'parameters', 'bestParameters', 'best_parameters', 'bestParams', 'best_params', 'params',
    'paramRanges', 'param_ranges', 'paramGrid', 'param_grid', 'parameterSensitivity',
    'parameter_sensitivity', 'parameterSpread', 'parameter_spread', 'paramStability',
    'param_stability', 'priceData', 'price_data', 'strategyParams', 'strategy_params',
    'symbolProvenance', 'perSymbol', 'per_symbol',
})


def snake_to_camel(snake_str: str) -> str:
    """Convert snake_case to camelCase ('fast_period' -> 'fastPeriod'; camelCase is unchanged)."""
    if '_' not in snake_str:
        return snake_str
    components = snake_str.split('_')
    return components[0] + ''.join(x.title() for x in components[1:])


def convert_keys_to_camel(data: Any, _protected: bool = False) -> Any:
    """Recursively convert FIELD names from snake_case to camelCase.

    Idempotent. The keys of data maps (see _PROTECTED_MAP_KEYS: parameters, priceData ...) are
    left as they are; values inside them are still converted."""
    if isinstance(data, dict):
        out = {}
        for k, v in data.items():
            nk = k if (_protected or not isinstance(k, str)) else snake_to_camel(k)
            out[nk] = convert_keys_to_camel(v, _protected=isinstance(k, str) and k in _PROTECTED_MAP_KEYS
                                            and isinstance(v, dict))
        return out
    if isinstance(data, list):
        return [convert_keys_to_camel(item) for item in data]
    return data


def _sanitize_for_json(obj):
    """Recursively replace NaN/Infinity with None (kept for callers; json_response does more)."""
    return to_jsonable(obj)


def to_jsonable(obj: Any) -> Any:
    """Strict conversion for the stdout JSON (B6): numpy ints / floats / bools -> numbers / bools
    (NaN and +-Infinity -> null), arrays and Series -> lists, Timestamps / datetimes / dates ->
    ISO strings ('YYYY-MM-DD' at midnight, else 'YYYY-MM-DD HH:MM:SS'), NaT -> null, Decimal ->
    float, sets and tuples -> lists, Path -> str, Enum -> value, dataclasses -> dicts. dict keys
    become strings (Timestamps as dates)."""
    import math
    import sys as _sys
    if obj is None or isinstance(obj, (str, bool)):
        return obj
    if isinstance(obj, int):
        return int(obj)
    if isinstance(obj, float):
        return obj if math.isfinite(obj) else None
    np_ = _sys.modules.get('numpy')
    if np_ is not None:
        if isinstance(obj, np_.bool_):
            return bool(obj)
        if isinstance(obj, np_.integer):
            return int(obj)
        if isinstance(obj, np_.floating):
            f = float(obj)
            return f if math.isfinite(f) else None
        if isinstance(obj, np_.ndarray):
            return [to_jsonable(v) for v in obj.tolist()]
        if isinstance(obj, np_.datetime64):
            if np_.isnat(obj):
                return None
            obj = __import__('pandas').Timestamp(obj)
    pd_ = _sys.modules.get('pandas')
    if pd_ is not None:
        if obj is pd_.NaT or obj is getattr(pd_, 'NA', object()):
            return None
        if isinstance(obj, pd_.Timestamp):
            return _iso(obj)
        if isinstance(obj, pd_.Timedelta):
            return obj.total_seconds()
        if isinstance(obj, (pd_.Series, pd_.Index)):
            return [to_jsonable(v) for v in obj.tolist()]
        if isinstance(obj, pd_.DataFrame):
            return to_jsonable(obj.to_dict(orient='records'))
    import datetime as _dt
    if isinstance(obj, _dt.datetime):
        return _iso(obj)
    if isinstance(obj, _dt.date):
        return obj.isoformat()
    if isinstance(obj, dict):
        return {(_key(k)): to_jsonable(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple, set, frozenset)):
        return [to_jsonable(v) for v in obj]
    from decimal import Decimal
    if isinstance(obj, Decimal):
        return float(obj) if obj.is_finite() else None
    import enum
    if isinstance(obj, enum.Enum):
        return to_jsonable(obj.value)
    import dataclasses
    if dataclasses.is_dataclass(obj) and not isinstance(obj, type):
        return to_jsonable(dataclasses.asdict(obj))
    from pathlib import PurePath
    if isinstance(obj, PurePath):
        return str(obj)
    return obj


def _iso(ts) -> str:
    if getattr(ts, 'tzinfo', None) is not None:
        ts = ts.replace(tzinfo=None)
    if ts.hour == 0 and ts.minute == 0 and ts.second == 0 and getattr(ts, 'microsecond', 0) == 0:
        return ts.strftime('%Y-%m-%d')
    return ts.strftime('%Y-%m-%d %H:%M:%S')


def _key(k) -> str:
    if isinstance(k, str):
        return k
    v = to_jsonable(k)
    return v if isinstance(v, str) else str(v)


def _last_resort(o):
    import sys as _sys
    print(f'[json] {type(o).__name__} serialised as text', file=_sys.stderr)
    return str(o)


ERROR_CODES = ('DATA', 'INPUT', 'ENGINE', 'MISSING_PACKAGE', 'CANCELLED', 'TIMEOUT', 'INTERNAL')


def classify_error(message: str, exc: Optional[BaseException] = None) -> str:
    """Contract R2 errorCode for a failure message / exception."""
    name = type(exc).__name__ if exc is not None else ''
    m = (message or '').lower()
    if 'cancelled by user' in m or name == 'CancelledError':
        return 'CANCELLED'
    if name in ('ModuleNotFoundError', 'ImportError') or 'no module named' in m or 'is not installed' in m:
        return 'MISSING_PACKAGE'
    if name in ('SyntheticDataRefused', 'PriceDataError') or any(s in m for s in (
            'could not be loaded', 'market data', 'no symbol', 'no bars', 'no data', 'yahoo', 'not enough data',
            'insufficient data', 'synthetic', 'price data', 'rate-limit', 'delisted')):
        return 'DATA'
    if 'timed out' in m or name == 'TimeoutError':
        return 'TIMEOUT'
    if name in ('ValueError', 'KeyError', 'TypeError') and exc is not None and not m.startswith('traceback'):
        return 'INPUT'
    if any(s in m for s in ('unknown command', 'unknown strategy', 'required', 'invalid', 'must be', 'no parameter',
                            'expected', 'unsupported', 'choose one of', 'too few', 'too large', 'needs at least')):
        return 'INPUT'
    return 'ENGINE'


def error_envelope(message: str, code: Optional[str] = None, *, exc: Optional[BaseException] = None,
                   traceback_text: Optional[str] = None, logs: Optional[List[str]] = None,
                   symbol: Optional[str] = None, **extra) -> Dict[str, Any]:
    """The contract R2 failure envelope: one plain sentence + errorCode + details."""
    msg, tb = _split_traceback(str(message or 'The command failed.'))
    out = {'success': False, 'error': msg, 'errorCode': code or classify_error(msg, exc),
           'details': {'traceback': traceback_text or tb, 'logs': list(logs or []), 'symbol': symbol}}
    out.update(extra)
    return out


def _split_traceback(message: str):
    """'... Traceback (most recent call last): ...' -> (plain last line, traceback text)."""
    if 'Traceback (most recent call last)' not in message:
        return message.strip(), None
    head, _, tb = message.partition('Traceback (most recent call last)')
    tb = 'Traceback (most recent call last)' + tb
    last = [ln.strip() for ln in tb.strip().splitlines() if ln.strip()]
    plain = head.strip().rstrip(':').strip() or (last[-1] if last else 'The command failed.')
    return plain, tb


def normalize_envelope(obj: Any) -> Any:
    """Contract R2: a failure is {'success': False, 'error', 'errorCode', 'details'} at the top
    level - also when an engine returned {'success': True, 'data': {'success': False, ...}}."""
    if not isinstance(obj, dict):
        return obj
    if obj.get('success') is True and isinstance(obj.get('data'), dict) and obj['data'].get('success') is False:
        inner = obj['data']
        env = error_envelope(inner.get('error') or inner.get('message') or 'The command failed.',
                             inner.get('errorCode'),
                             logs=inner.get('logs') if isinstance(inner.get('logs'), list) else None)
        for k in ('cancelled', 'timedOut'):
            if k in inner:
                env[k] = inner[k]
        return env
    if obj.get('success') is False:
        msg = obj.get('error') or obj.get('message') or 'The command failed.'
        plain, tb = _split_traceback(str(msg))
        out = dict(obj)
        out['error'] = plain
        out.setdefault('errorCode', 'CANCELLED' if obj.get('cancelled') else classify_error(plain))
        det = out.get('details') if isinstance(out.get('details'), dict) else {}
        det.setdefault('traceback', tb or obj.get('traceback'))
        det.setdefault('logs', obj.get('logs') if isinstance(obj.get('logs'), list) else [])
        det.setdefault('symbol', obj.get('symbol'))
        out['details'] = det
        return out
    return obj


def json_response(data: Any) -> str:
    """The single stdout line: camelCase field names (data-map keys untouched), strict JSON
    (NaN / Infinity -> null, numpy / pandas values converted, never `indent`), failures in the
    contract envelope. Uses scripts/fincept_json.py as the final boundary."""
    payload = data
    inner = payload.get('data') if isinstance(payload, dict) else None
    already_v2 = isinstance(inner, dict) and inner.get('schemaVersion') == 2
    if not already_v2:
        payload = convert_keys_to_camel(payload)
    payload = normalize_envelope(to_jsonable(payload))
    try:
        from base.progress import ensure_scripts_on_path
        ensure_scripts_on_path()
        import fincept_json
        return fincept_json.dumps(payload, default=_last_resort, separators=(',', ':'))
    except ImportError:
        return json.dumps(payload, default=_last_resort, ensure_ascii=False, allow_nan=False, separators=(',', ':'))


def emit_response(data: Any) -> str:
    """Print json_response(data) as the one stdout line (flushed) and return it."""
    import sys as _sys
    text = json_response(data)
    out = _sys.stdout
    try:
        out.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass
    out.write(text + '\n')
    out.flush()
    return text


def parse_json_input(json_str: str) -> Dict[str, Any]:
    """Parse JSON input from command line argument"""
    return json.loads(json_str)


# ============================================================================
# Run control shared by every provider
# ============================================================================

def read_cli_args(argv=None):
    """Return (command, json_args) from the CLI.

    The host (BacktestingService) sends `<command> --stdin` and writes the JSON
    payload to stdin: broker-injected candles easily exceed the ~32 KB Windows
    command-line limit, and argv is visible to other same-user processes.
    `<command> <json>` is still accepted for manual runs.
    """
    import sys as _sys
    argv = list(_sys.argv if argv is None else argv)
    if len(argv) < 2:
        raise ValueError('Usage: <provider>.py <command> <json_args | --stdin>')
    command = argv[1]
    if '--stdin' in argv[2:]:
        return command, _sys.stdin.read()
    if len(argv) < 3:
        raise ValueError('Usage: <provider>.py <command> <json_args | --stdin>')
    return command, argv[2]


def check_cancelled(request) -> None:
    """Exit the process immediately if the host cancelled this run.

    BacktestingService passes `cancelFile` (a temp path) with every run and
    creates that file when the user presses CANCEL. Long loops (optimisation,
    walk-forward windows) call this once per iteration so a cancelled run frees
    its Python slot instead of computing for minutes nobody will read.

    os._exit is deliberate: provider loops wrap iterations in broad
    `except Exception` handlers that would swallow a normal exception.
    """
    import os as _os
    path = request.get('cancelFile') if isinstance(request, dict) else None
    if not path or not _os.path.exists(path):
        return
    from base.progress import cancel_exit
    cancel_exit()


class SyntheticDataRefused(RuntimeError):
    """Real market data could not be loaded and the user did not allow synthetic data."""


def synthetic_allowed(request) -> bool:
    """True only when the user explicitly ticked 'Allow synthetic data' in the UI."""
    return bool(isinstance(request, dict) and request.get('allowSyntheticData', False))


def refuse_synthetic(symbols, reason) -> None:
    """Raise SyntheticDataRefused with an actionable message.

    Every provider used to fall back to random-walk prices whenever a download
    failed (typo'd ticker, network outage, rate limit) and still returned a
    'successful' backtest. A run on fake prices is now refused unless the user
    opted in explicitly.
    """
    syms = ', '.join(str(s) for s in (symbols or [])) or '(none)'
    raise SyntheticDataRefused(
        f'Market data for {syms} could not be loaded ({reason}). Refusing to run on synthetic '
        f'prices. Check the symbol (exchange suffix, e.g. RELIANCE.NS), the date range and your '
        f'network connection - or tick "Allow synthetic data" to test strategy mechanics on fake prices.'
    )


def periods_per_year(index) -> float:
    """Bars per year implied by a datetime index (annualisation basis). Delegates to
    base.metrics.infer_periods_per_year: 252 for daily bars without weekend dates, 365 with
    them (24x7 markets), 52 weekly, 12 monthly, intraday = bars per session day x 252 / 365.
    (B17: a short Mon-Thu daily series used to read 365, two bars 252 regardless.)"""
    try:
        from base.metrics import infer_periods_per_year
        return float(infer_periods_per_year(index))
    except Exception:
        return 252.0


def describe_annualization(ppy: float) -> str:
    """Human description of an annualisation basis for the assumptions block."""
    if abs(ppy - 252.0) < 0.5:
        return '252 bars/year (daily equity bars)'
    if abs(ppy - 365.0) < 0.5:
        return '365 bars/year (daily bars including weekends: 24x7 market or mixed calendar)'
    if abs(ppy - 52.0) < 0.5:
        return '52 bars/year (weekly bars)'
    if abs(ppy - 12.0) < 0.5:
        return '12 bars/year (monthly bars)'
    return f'{ppy:,.0f} bars/year (intraday: bars per session x sessions per year)'


_SURVIVORSHIP_NOTE = (
    'Backtests only the symbols entered. There is no universe selection, so no survivorship '
    'filter applies - but symbols that were later delisted cannot be tested, so a hand-picked '
    'list of current winners is itself survivorship-biased.'
)


def build_assumptions(engine: str, execution: str, request: Dict[str, Any], *,
                      costs: Optional[str] = None,
                      position_sizing: Optional[str] = None,
                      annualization: Optional[str] = None,
                      data_source: Optional[str] = None,
                      ignored: Optional[List[str]] = None,
                      notes: Optional[List[str]] = None,
                      fill_model: Optional[Dict[str, Any]] = None,
                      not_modelled: Optional[List[str]] = None) -> Dict[str, Any]:
    """The `assumptions` block every backtest result carries.

    It states how the numbers were produced so a reader can judge them: when
    fills happen relative to the signal bar, which costs were charged, how
    returns were annualised and which UI settings this engine ignored.
    """
    req = request if isinstance(request, dict) else {}
    commission = float(req.get('commission', 0) or 0)
    slippage = float(req.get('slippage', 0) or 0)
    if costs is None:
        costs = (f'commission {commission * 100:.3f}% of notional per fill; '
                 f'slippage {slippage * 100:.4f}% of price per fill')
    out = {
        'engine': engine,
        'execution': execution,
        'costs': costs,
        'data_source': data_source or 'Yahoo Finance (split/dividend-adjusted prices)',
        'survivorship': _SURVIVORSHIP_NOTE,
        'risk_free_rate': float(req.get('riskFreeRate', 0) or 0),
    }
    if position_sizing:
        out['position_sizing'] = position_sizing
    if annualization:
        out['annualization'] = annualization
    if ignored:
        out['ignored_settings'] = list(ignored)
    if notes:
        out['notes'] = list(notes)
    if fill_model:
        out['fill_model'] = dict(fill_model)
    if not_modelled is not None:
        out['not_modelled'] = list(not_modelled)
    return out


NOT_MODELLED_VOCABULARY = ('bid/ask spread', 'market impact', 'partial fills', 'volume limits', 'borrow cost',
                           'margin calls', 'cash interest', 'dividend withholding', 'FX conversion',
                           'trading halts', 'delisted symbols')


def capabilities_payload(engine: str, library: Optional[str], library_version: Optional[str], *,
                         installed: bool = True, commands: Optional[List[str]] = None,
                         fill_model: Optional[Dict[str, Any]] = None,
                         commission_models: Optional[List[str]] = None,
                         slippage_models: Optional[List[str]] = None, short_selling: bool = False,
                         leverage: bool = False, multi_symbol: bool = False, intraday: bool = False,
                         sizing_methods: Optional[List[str]] = None, stops: bool = False,
                         monte_carlo_methods: Optional[List[str]] = None,
                         optimize_methods: Optional[List[str]] = None,
                         walk_forward_modes: Optional[List[str]] = None,
                         not_modelled: Optional[List[str]] = None,
                         engine_implementation: Optional[str] = None,
                         fill_timings: Optional[List[str]] = None,
                         intervals: Optional[List[str]] = None,
                         request_keys: Optional[Dict[str, Any]] = None,
                         cost_presets: Optional[List[str]] = None) -> Dict[str, Any]:
    """Contract GET_CAPABILITIES data (camelCase at source).

    Optional blocks (emitted when given; the rebuilt screen reads them to enable / disable inputs):
      fillTimings  fill timings the engine executes (subset of next_open / next_close / same_close)
      intervals    bar intervals it accepts
      requestKeys  {'honoured': [request keys applied as sent], 'ignored': [request keys accepted but not
                   applied - each also produces an IGNORED_SETTING warning when set], 'notes': {key: text}}
      costPresets  base.costs presets accepted in costModel.preset
    """
    extra: Dict[str, Any] = {}
    if fill_timings is not None:
        extra['fillTimings'] = list(fill_timings)
    if intervals is not None:
        extra['intervals'] = list(intervals)
    if request_keys is not None:
        extra['requestKeys'] = {'honoured': list(request_keys.get('honoured') or []),
                                'ignored': list(request_keys.get('ignored') or []),
                                'notes': dict(request_keys.get('notes') or {})}
    if cost_presets is not None:
        extra['costPresets'] = list(cost_presets)
    return {
        **extra,
        'schemaVersion': 2, 'engine': engine, 'engineImplementation': engine_implementation,
        'library': library, 'libraryVersion': library_version, 'installed': bool(installed),
        'commands': list(commands or []), 'fillModel': fill_model,
        'commissionModels': list(commission_models or ['pct_notional']),
        'slippageModels': list(slippage_models or ['pct_price']), 'shortSelling': bool(short_selling),
        'leverage': bool(leverage), 'multiSymbol': bool(multi_symbol), 'intraday': bool(intraday),
        'sizingMethods': list(sizing_methods or ['percent']), 'stops': bool(stops),
        'monteCarloMethods': list(monte_carlo_methods or ['trade_bootstrap', 'trade_shuffle', 'returns_bootstrap',
                                                          'block_bootstrap']),
        'optimizeMethods': list(optimize_methods or ['grid', 'random']),
        'walkForwardModes': list(walk_forward_modes or ['rolling', 'anchored']),
        'notModelled': list(not_modelled if not_modelled is not None else NOT_MODELLED_VOCABULARY),
    }


def ignored_request_settings(request: Dict[str, Any], supported: List[str]) -> List[str]:
    """UI settings the user changed from their neutral value that this engine does not apply.

    supported: subset of {'commission', 'slippage', 'stops', 'sizing', 'short', 'leverage', 'interval'}.
    """
    req = request if isinstance(request, dict) else {}
    out = []
    if 'commission' not in supported and float(req.get('commission', 0) or 0) > 0:
        out.append('commission')
    if 'slippage' not in supported and float(req.get('slippage', 0) or 0) > 0:
        out.append('slippage')
    if 'stops' not in supported and (float(req.get('stopLoss', 0) or 0) > 0
                                     or float(req.get('takeProfit', 0) or 0) > 0):
        out.append('stop-loss/take-profit')
    sizing = str(req.get('positionSizing', 'percent') or 'percent')
    size_val = float(req.get('positionSizeValue', 100) or 100)
    if 'sizing' not in supported and not (sizing == 'percent' and size_val >= 100):
        out.append('position sizing (engine invests 100% of equity)')
    if 'short' not in supported and bool(req.get('allowShort', False)):
        out.append('short selling (long-only engine)')
    if 'leverage' not in supported and float(req.get('leverage', 1) or 1) != 1.0:
        out.append('leverage')
    if 'interval' not in supported and str(req.get('interval', '1d') or '1d') != '1d':
        out.append(f"interval {req.get('interval')} (engine uses daily bars)")
    return out
