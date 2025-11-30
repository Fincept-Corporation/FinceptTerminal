"""
Base Provider for Backtesting Engines
Abstract base class that all Python backtesting providers must implement
Mirrors the TypeScript IBacktestingProvider interface
"""

from abc import ABC, abstractmethod
from typing import Dict, Any, List, Optional
from dataclasses import dataclass, asdict
from datetime import datetime
import json


# ============================================================================
# Data Classes (Python equivalents of TypeScript interfaces)
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
        self.initialized = False
        self.connected = False
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
        """Clean up resources and disconnect"""
        self.connected = False
        self.initialized = False

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
        """Check if provider is initialized, raise error if not"""
        if not self.initialized:
            raise RuntimeError(f"{self.name} is not initialized. Call initialize() first.")

    def _ensure_connected(self) -> None:
        """Check if provider is connected, raise error if not"""
        self._ensure_initialized()
        if not self.connected:
            raise RuntimeError(f"{self.name} is not connected. Call test_connection() first.")

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
        """Log message"""
        print(f"[{self.name}] {message}")

    def _error(self, message: str, exception: Optional[Exception] = None) -> None:
        """Log error message"""
        error_msg = f"[{self.name}] ERROR: {message}"
        if exception:
            error_msg += f"\n{str(exception)}"
        print(error_msg, file=__import__('sys').stderr)


# ============================================================================
# Utility Functions
# ============================================================================

def json_response(data: Any) -> str:
    """Convert data to JSON string for stdout output"""
    return json.dumps(data, default=str, ensure_ascii=False)


def parse_json_input(json_str: str) -> Dict[str, Any]:
    """Parse JSON input from command line argument"""
    return json.loads(json_str)
