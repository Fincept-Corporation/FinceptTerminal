"""
Portfolio Metrics for Alpha Arena

Provides advanced portfolio analytics including:
- Sharpe Ratio
- Max Drawdown
- Win Rate
- Profit Factor
- Risk-adjusted returns
"""

import math
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field
from datetime import datetime

from alpha_arena.types.models import PortfolioState, TradeResult
from alpha_arena.utils.logging import get_logger

logger = get_logger("portfolio_metrics")


@dataclass
class PortfolioMetrics:
    """Comprehensive portfolio metrics"""

    # Basic metrics
    total_return: float = 0.0
    total_return_pct: float = 0.0
    total_pnl: float = 0.0

    # Trade statistics
    total_trades: int = 0
    winning_trades: int = 0
    losing_trades: int = 0
    win_rate: float = 0.0

    # P&L statistics
    gross_profit: float = 0.0
    gross_loss: float = 0.0
    profit_factor: float = 0.0
    average_win: float = 0.0
    average_loss: float = 0.0
    largest_win: float = 0.0
    largest_loss: float = 0.0
    expectancy: float = 0.0

    # Risk metrics
    sharpe_ratio: float = 0.0
    sortino_ratio: float = 0.0
    max_drawdown: float = 0.0
    max_drawdown_pct: float = 0.0
    current_drawdown: float = 0.0
    current_drawdown_pct: float = 0.0

    # Volatility
    daily_volatility: float = 0.0
    annualized_volatility: float = 0.0

    # Time-based
    avg_trade_duration: float = 0.0  # seconds
    time_in_market_pct: float = 0.0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "total_return": self.total_return,
            "total_return_pct": self.total_return_pct,
            "total_pnl": self.total_pnl,
            "total_trades": self.total_trades,
            "winning_trades": self.winning_trades,
            "losing_trades": self.losing_trades,
            "win_rate": self.win_rate,
            "gross_profit": self.gross_profit,
            "gross_loss": self.gross_loss,
            "profit_factor": self.profit_factor,
            "average_win": self.average_win,
            "average_loss": self.average_loss,
            "largest_win": self.largest_win,
            "largest_loss": self.largest_loss,
            "expectancy": self.expectancy,
            "sharpe_ratio": self.sharpe_ratio,
            "sortino_ratio": self.sortino_ratio,
            "max_drawdown": self.max_drawdown,
            "max_drawdown_pct": self.max_drawdown_pct,
            "current_drawdown": self.current_drawdown,
            "current_drawdown_pct": self.current_drawdown_pct,
            "daily_volatility": self.daily_volatility,
            "annualized_volatility": self.annualized_volatility,
        }

    def to_prompt_context(self) -> str:
        """Convert to text for LLM context"""
        lines = [
            "PORTFOLIO PERFORMANCE:",
            f"  Total Return: ${self.total_return:+,.2f} ({self.total_return_pct:+.2f}%)",
            f"  Sharpe Ratio: {self.sharpe_ratio:.2f}",
            f"  Max Drawdown: {self.max_drawdown_pct:.2f}%",
            f"  Win Rate: {self.win_rate:.1f}% ({self.winning_trades}W / {self.losing_trades}L)",
            f"  Profit Factor: {self.profit_factor:.2f}",
            f"  Expectancy: ${self.expectancy:+,.2f}",
        ]
        return "\n".join(lines)

    def get_risk_assessment(self) -> str:
        """Get risk assessment based on metrics"""
        if self.sharpe_ratio >= 2.0 and self.max_drawdown_pct < 10:
            return "excellent"
        elif self.sharpe_ratio >= 1.0 and self.max_drawdown_pct < 20:
            return "good"
        elif self.sharpe_ratio >= 0.5 and self.max_drawdown_pct < 30:
            return "moderate"
        elif self.sharpe_ratio >= 0 and self.max_drawdown_pct < 50:
            return "risky"
        else:
            return "very_risky"


class PortfolioAnalyzer:
    """
    Analyzes portfolio performance and calculates metrics.

    Tracks:
    - Portfolio value history for drawdown/volatility
    - Trade history for win rate/profit factor
    - Returns for Sharpe/Sortino ratios
    """

    def __init__(self, initial_capital: float = 10000.0, risk_free_rate: float = 0.05):
        self.initial_capital = initial_capital
        self.risk_free_rate = risk_free_rate  # Annual risk-free rate

        # History tracking
        self._portfolio_values: List[float] = [initial_capital]
        self._returns: List[float] = []
        self._trades: List[Dict[str, Any]] = []
        self._peak_value: float = initial_capital
        self._timestamps: List[datetime] = [datetime.now()]

    def record_value(self, value: float, timestamp: Optional[datetime] = None):
        """Record a portfolio value snapshot"""
        ts = timestamp or datetime.now()

        if self._portfolio_values:
            prev_value = self._portfolio_values[-1]
            if prev_value > 0:
                ret = (value - prev_value) / prev_value
                self._returns.append(ret)

        self._portfolio_values.append(value)
        self._timestamps.append(ts)

        # Update peak for drawdown
        if value > self._peak_value:
            self._peak_value = value

    def record_trade(
        self,
        pnl: float,
        entry_price: float,
        exit_price: float,
        quantity: float,
        side: str,
        entry_time: Optional[datetime] = None,
        exit_time: Optional[datetime] = None,
    ):
        """Record a completed trade"""
        self._trades.append({
            "pnl": pnl,
            "entry_price": entry_price,
            "exit_price": exit_price,
            "quantity": quantity,
            "side": side,
            "entry_time": entry_time or datetime.now(),
            "exit_time": exit_time or datetime.now(),
        })

    def calculate_metrics(self) -> PortfolioMetrics:
        """Calculate all portfolio metrics"""
        metrics = PortfolioMetrics()

        if not self._portfolio_values:
            return metrics

        current_value = self._portfolio_values[-1]

        # Basic returns
        metrics.total_return = current_value - self.initial_capital
        metrics.total_return_pct = ((current_value / self.initial_capital) - 1) * 100
        metrics.total_pnl = metrics.total_return

        # Trade statistics
        metrics.total_trades = len(self._trades)

        if self._trades:
            winning = [t for t in self._trades if t["pnl"] > 0]
            losing = [t for t in self._trades if t["pnl"] < 0]

            metrics.winning_trades = len(winning)
            metrics.losing_trades = len(losing)
            metrics.win_rate = (len(winning) / len(self._trades)) * 100 if self._trades else 0

            # P&L statistics
            metrics.gross_profit = sum(t["pnl"] for t in winning)
            metrics.gross_loss = abs(sum(t["pnl"] for t in losing))
            metrics.profit_factor = metrics.gross_profit / metrics.gross_loss if metrics.gross_loss > 0 else float('inf')

            metrics.average_win = metrics.gross_profit / len(winning) if winning else 0
            metrics.average_loss = metrics.gross_loss / len(losing) if losing else 0

            metrics.largest_win = max((t["pnl"] for t in winning), default=0)
            metrics.largest_loss = abs(min((t["pnl"] for t in losing), default=0))

            # Expectancy = (Win% * Avg Win) - (Loss% * Avg Loss)
            win_pct = metrics.win_rate / 100
            loss_pct = 1 - win_pct
            metrics.expectancy = (win_pct * metrics.average_win) - (loss_pct * metrics.average_loss)

        # Volatility and risk metrics
        if len(self._returns) >= 2:
            metrics.daily_volatility = self._calculate_std(self._returns)
            metrics.annualized_volatility = metrics.daily_volatility * math.sqrt(252)

            # Sharpe Ratio
            metrics.sharpe_ratio = self._calculate_sharpe_ratio()

            # Sortino Ratio
            metrics.sortino_ratio = self._calculate_sortino_ratio()

        # Drawdown
        dd, dd_pct = self._calculate_max_drawdown()
        metrics.max_drawdown = dd
        metrics.max_drawdown_pct = dd_pct

        # Current drawdown
        if self._peak_value > 0:
            metrics.current_drawdown = self._peak_value - current_value
            metrics.current_drawdown_pct = (metrics.current_drawdown / self._peak_value) * 100

        return metrics

    def _calculate_std(self, values: List[float]) -> float:
        """Calculate standard deviation"""
        if len(values) < 2:
            return 0.0

        mean = sum(values) / len(values)
        variance = sum((x - mean) ** 2 for x in values) / (len(values) - 1)
        return math.sqrt(variance)

    def _calculate_sharpe_ratio(self) -> float:
        """
        Calculate Sharpe Ratio.

        Sharpe = (Portfolio Return - Risk-Free Rate) / Portfolio Volatility
        """
        if len(self._returns) < 2:
            return 0.0

        # Annualized return
        total_return = (self._portfolio_values[-1] / self.initial_capital) - 1
        periods = len(self._returns)
        annualized_return = ((1 + total_return) ** (252 / periods)) - 1 if periods > 0 else 0

        # Daily risk-free rate
        daily_rf = self.risk_free_rate / 252

        # Excess returns
        excess_returns = [r - daily_rf for r in self._returns]

        # Average excess return (annualized)
        avg_excess = sum(excess_returns) / len(excess_returns) if excess_returns else 0
        avg_excess_annual = avg_excess * 252

        # Volatility (annualized)
        volatility = self._calculate_std(self._returns) * math.sqrt(252)

        if volatility == 0:
            return 0.0

        return avg_excess_annual / volatility

    def _calculate_sortino_ratio(self) -> float:
        """
        Calculate Sortino Ratio.

        Similar to Sharpe but only uses downside volatility.
        """
        if len(self._returns) < 2:
            return 0.0

        daily_rf = self.risk_free_rate / 252

        # Downside returns only (negative returns)
        downside_returns = [r for r in self._returns if r < daily_rf]

        if not downside_returns:
            return float('inf')  # No downside = infinite Sortino

        # Downside deviation
        downside_deviation = self._calculate_std(downside_returns) * math.sqrt(252)

        if downside_deviation == 0:
            return float('inf')

        # Average excess return (annualized)
        avg_return = sum(self._returns) / len(self._returns) * 252
        excess_return = avg_return - self.risk_free_rate

        return excess_return / downside_deviation

    def _calculate_max_drawdown(self) -> tuple[float, float]:
        """Calculate maximum drawdown in absolute and percentage terms"""
        if len(self._portfolio_values) < 2:
            return 0.0, 0.0

        max_dd = 0.0
        max_dd_pct = 0.0
        peak = self._portfolio_values[0]

        for value in self._portfolio_values:
            if value > peak:
                peak = value

            dd = peak - value
            dd_pct = (dd / peak) * 100 if peak > 0 else 0

            if dd > max_dd:
                max_dd = dd
                max_dd_pct = dd_pct

        return max_dd, max_dd_pct

    def get_equity_curve(self) -> List[Dict[str, Any]]:
        """Get equity curve data for charting"""
        return [
            {
                "timestamp": ts.isoformat(),
                "value": val,
                "return_pct": ((val / self.initial_capital) - 1) * 100,
            }
            for ts, val in zip(self._timestamps, self._portfolio_values)
        ]

    def get_drawdown_curve(self) -> List[Dict[str, Any]]:
        """Get drawdown curve data for charting"""
        curve = []
        peak = self._portfolio_values[0] if self._portfolio_values else 0

        for ts, val in zip(self._timestamps, self._portfolio_values):
            if val > peak:
                peak = val

            dd_pct = ((peak - val) / peak) * 100 if peak > 0 else 0
            curve.append({
                "timestamp": ts.isoformat(),
                "drawdown_pct": dd_pct,
            })

        return curve

    def reset(self, initial_capital: Optional[float] = None):
        """Reset the analyzer"""
        if initial_capital:
            self.initial_capital = initial_capital

        self._portfolio_values = [self.initial_capital]
        self._returns = []
        self._trades = []
        self._peak_value = self.initial_capital
        self._timestamps = [datetime.now()]


# Per-model analyzers registry
_analyzers: Dict[str, PortfolioAnalyzer] = {}


def get_analyzer(model_name: str, initial_capital: float = 10000.0) -> PortfolioAnalyzer:
    """Get or create a portfolio analyzer for a model"""
    if model_name not in _analyzers:
        _analyzers[model_name] = PortfolioAnalyzer(initial_capital=initial_capital)
    return _analyzers[model_name]


def reset_analyzer(model_name: str):
    """Reset a model's analyzer"""
    if model_name in _analyzers:
        del _analyzers[model_name]


def reset_all_analyzers():
    """Reset all analyzers"""
    _analyzers.clear()
