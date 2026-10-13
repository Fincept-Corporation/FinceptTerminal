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

def snake_to_camel(snake_str: str) -> str:
    """Convert snake_case to camelCase"""
    components = snake_str.split('_')
    return components[0] + ''.join(x.title() for x in components[1:])


def convert_keys_to_camel(data: Any) -> Any:
    """Recursively convert dictionary keys from snake_case to camelCase"""
    if isinstance(data, dict):
        return {snake_to_camel(k): convert_keys_to_camel(v) for k, v in data.items()}
    elif isinstance(data, list):
        return [convert_keys_to_camel(item) for item in data]
    else:
        return data


def _sanitize_for_json(obj):
    """Recursively replace NaN/Infinity with None so json.dumps produces valid JSON."""
    import math
    if isinstance(obj, float):
        if math.isnan(obj) or math.isinf(obj):
            return None
        return obj
    if isinstance(obj, dict):
        return {k: _sanitize_for_json(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [_sanitize_for_json(v) for v in obj]
    return obj


def json_response(data: Any) -> str:
    """Convert data to JSON string for stdout output with camelCase keys"""
    camel_data = convert_keys_to_camel(data)
    sanitized = _sanitize_for_json(camel_data)
    return json.dumps(sanitized, default=str, ensure_ascii=False)


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
    import sys as _sys
    path = request.get('cancelFile') if isinstance(request, dict) else None
    if not path or not _os.path.exists(path):
        return
    try:
        out = _sys.__stdout__
        out.write(json.dumps({'success': False, 'error': 'Cancelled by user', 'cancelled': True}) + '\n')
        out.flush()
    finally:
        _os._exit(3)


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
    """Bars per year implied by a datetime index (used to annualise Sharpe/vol/CAGR).

    daily-like spacing: 252 when the index has weekend gaps (equities), 365 when it
    does not (crypto/FX 24x7); weekly 52; monthly 12; intraday: observed bars per
    calendar year spanned.
    """
    try:
        import numpy as np
        import pandas as pd
        if not isinstance(index, pd.DatetimeIndex):
            index = pd.DatetimeIndex(pd.to_datetime(list(index)))
        n = len(index)
        if n < 3:
            return 252.0
        secs = index.asi8.astype('float64')
        unit = str(index.dtype)
        if 'ns' in unit:
            secs = secs / 1e9
        elif 'us' in unit:
            secs = secs / 1e6
        elif 'ms' in unit:
            secs = secs / 1e3
        deltas = np.diff(secs)
        deltas = deltas[deltas > 0]
        if len(deltas) == 0:
            return 252.0
        med = float(np.median(deltas))
        if med < 1.0:
            return 252.0
        if med >= 25 * 86400:
            return 12.0
        if med >= 6 * 86400:
            return 52.0
        if med >= 20 * 3600:
            return 252.0 if bool(np.any(deltas > 1.5 * 86400)) else 365.0
        span_years = float(secs[-1] - secs[0]) / (365.25 * 86400.0)
        if span_years <= 0:
            return 252.0
        return float(max(1.0, (n - 1) / span_years))
    except Exception:
        return 252.0


def describe_annualization(ppy: float) -> str:
    """Human description of an annualisation basis for the assumptions block."""
    if abs(ppy - 252.0) < 0.5:
        return '252 bars/year (daily equity bars)'
    if abs(ppy - 365.0) < 0.5:
        return '365 bars/year (daily bars, 7-day market)'
    if abs(ppy - 52.0) < 0.5:
        return '52 bars/year (weekly bars)'
    if abs(ppy - 12.0) < 0.5:
        return '12 bars/year (monthly bars)'
    return f'{ppy:,.0f} bars/year (measured from the intraday bar spacing)'


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
                      notes: Optional[List[str]] = None) -> Dict[str, Any]:
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
    return out


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
