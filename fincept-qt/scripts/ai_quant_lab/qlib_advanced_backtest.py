"""
AI Quant Lab - Advanced Backtest Module
Realistic backtesting with market microstructure:
- Order book simulation
- Market impact modeling
- Realistic slippage
- Transaction costs
- Execution algorithms (TWAP, VWAP, Limit orders)
- Liquidity constraints
"""

import json
import sys
from typing import Dict, List, Any, Optional, Union, Tuple
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

try:
    import pandas as pd
    import numpy as np
    from collections import deque
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False
    pd = None
    np = None


class OrderBook:
    """
    Simplified order book simulation.

    Models bid-ask spread and depth.
    """

    def __init__(self,
                 initial_mid_price: float,
                 spread_bps: float = 10.0,
                 depth: float = 1000000):
        """
        Args:
            initial_mid_price: Initial mid price
            spread_bps: Bid-ask spread in basis points
            depth: Liquidity depth at best bid/ask
        """
        self.mid_price = initial_mid_price
        self.spread_bps = spread_bps
        self.depth = depth

        # Calculate bid-ask
        spread = initial_mid_price * (spread_bps / 10000)
        self.best_bid = initial_mid_price - spread / 2
        self.best_ask = initial_mid_price + spread / 2

        # Order book levels (price -> quantity)
        self.bids = {}
        self.asks = {}

        self._initialize_book()

    def _initialize_book(self):
        """Initialize order book with realistic depth"""
        # 5 levels on each side
        tick_size = self.mid_price * 0.001  # 0.1% tick size

        for i in range(5):
            bid_price = self.best_bid - i * tick_size
            ask_price = self.best_ask + i * tick_size

            # Depth decreases with distance from mid
            level_depth = self.depth * (1 - i * 0.15)

            self.bids[bid_price] = level_depth
            self.asks[ask_price] = level_depth

    def get_execution_price(self,
                           side: str,
                           quantity: float,
                           aggressive: bool = True) -> Tuple[float, float]:
        """
        Get execution price for an order.

        Args:
            side: 'buy' or 'sell'
            quantity: Order quantity
            aggressive: If True, crosses spread (market order)

        Returns:
            (avg_execution_price, total_cost)
        """
        if side == 'buy':
            book = sorted(self.asks.items())  # Ascending price
            base_price = self.best_ask if aggressive else self.best_bid
        else:
            book = sorted(self.bids.items(), reverse=True)  # Descending price
            base_price = self.best_bid if aggressive else self.best_ask

        if not aggressive:
            # Passive order, no immediate execution
            return base_price, base_price * quantity

        # Walk through order book
        remaining = quantity
        total_cost = 0.0

        for price, available in book:
            if remaining <= 0:
                break

            fill_qty = min(remaining, available)
            total_cost += price * fill_qty
            remaining -= fill_qty

        # If not enough liquidity, use last price + penalty
        if remaining > 0:
            penalty_price = book[-1][0] * 1.01 if side == 'buy' else book[-1][0] * 0.99
            total_cost += penalty_price * remaining

        avg_price = total_cost / quantity
        return avg_price, total_cost

    def update_price(self, new_mid_price: float):
        """Update order book after price movement"""
        self.mid_price = new_mid_price
        spread = new_mid_price * (self.spread_bps / 10000)
        self.best_bid = new_mid_price - spread / 2
        self.best_ask = new_mid_price + spread / 2
        self._initialize_book()


class MarketImpactModel:
    """
    Market impact model based on academic research.

    Implements square-root impact law.
    """

    def __init__(self,
                 impact_coefficient: float = 0.1,
                 temporary_decay_rate: float = 0.5):
        """
        Args:
            impact_coefficient: Market impact sensitivity
            temporary_decay_rate: Rate of temporary impact decay
        """
        self.impact_coef = impact_coefficient
        self.decay_rate = temporary_decay_rate

    def calculate_impact(self,
                        order_size: float,
                        adv: float,  # Average daily volume
                        volatility: float,
                        participation_rate: Optional[float] = None) -> Dict[str, float]:
        """
        Calculate permanent and temporary market impact.

        Uses square-root impact model:
        Impact = σ * (size / ADV)^0.5

        Args:
            order_size: Order size in shares
            adv: Average daily volume
            volatility: Daily volatility
            participation_rate: Order size as % of volume

        Returns:
            Dict with permanent and temporary impact
        """
        if adv <= 0 or order_size <= 0:
            return {"permanent": 0.0, "temporary": 0.0, "total": 0.0}

        # Participation rate
        if participation_rate is None:
            participation_rate = order_size / adv

        # Square-root impact
        base_impact = volatility * np.sqrt(participation_rate)

        # Permanent impact (50% of total)
        permanent_impact = self.impact_coef * base_impact * 0.5

        # Temporary impact (50% of total, decays)
        temporary_impact = self.impact_coef * base_impact * 0.5

        total_impact = permanent_impact + temporary_impact

        return {
            "permanent": float(permanent_impact),
            "temporary": float(temporary_impact),
            "total": float(total_impact),
            "participation_rate": float(participation_rate),
            "base_impact_bps": float(base_impact * 10000)
        }

    def calculate_optimal_execution_time(self,
                                        order_size: float,
                                        adv: float,
                                        max_participation: float = 0.10) -> int:
        """
        Calculate optimal execution time to minimize impact.

        Args:
            order_size: Total order size
            adv: Average daily volume
            max_participation: Max participation rate (e.g., 0.10 = 10%)

        Returns:
            Number of time periods (intervals) to execute over
        """
        participation = order_size / adv

        if participation <= max_participation:
            return 1  # Execute immediately

        # Split into chunks
        num_periods = int(np.ceil(participation / max_participation))
        return num_periods


class SlippageModel:
    """
    Realistic slippage model.

    Combines bid-ask spread, market impact, and volatility.
    """

    def __init__(self,
                 base_spread_bps: float = 10.0,
                 volatility_factor: float = 0.5,
                 size_penalty_factor: float = 0.1):
        """
        Args:
            base_spread_bps: Base bid-ask spread
            volatility_factor: How much volatility affects slippage
            size_penalty_factor: Penalty for large orders
        """
        self.base_spread = base_spread_bps
        self.vol_factor = volatility_factor
        self.size_factor = size_penalty_factor

    def calculate_slippage(self,
                          order_size: float,
                          adv: float,
                          volatility: float,
                          order_type: str = "market") -> Dict[str, float]:
        """
        Calculate expected slippage.

        Args:
            order_size: Order size
            adv: Average daily volume
            volatility: Daily volatility
            order_type: 'market', 'limit', or 'midpoint'

        Returns:
            Slippage in basis points and dollars
        """
        # Base spread cost
        if order_type == "market":
            spread_cost = self.base_spread / 2  # Cross full spread
        elif order_type == "limit":
            spread_cost = 0  # Provide liquidity
        else:  # midpoint
            spread_cost = self.base_spread / 4

        # Volatility component
        vol_cost = self.vol_factor * volatility * 10000  # Convert to bps

        # Size component (square root of participation)
        participation = order_size / adv if adv > 0 else 1.0
        size_cost = self.size_factor * np.sqrt(participation) * 10000

        # Total slippage
        total_slippage_bps = spread_cost + vol_cost + size_cost

        return {
            "total_bps": float(total_slippage_bps),
            "spread_bps": float(spread_cost),
            "volatility_bps": float(vol_cost),
            "size_bps": float(size_cost),
            "participation_rate": float(participation)
        }


class ExecutionAlgorithm:
    """
    Execution algorithms for optimal trade execution.
    """

    def __init__(self, impact_model: MarketImpactModel):
        self.impact_model = impact_model

    def twap(self,
             total_quantity: float,
             num_intervals: int,
             interval_minutes: int = 5) -> List[Dict[str, Any]]:
        """
        Time-Weighted Average Price (TWAP) algorithm.

        Splits order equally across time intervals.

        Args:
            total_quantity: Total order size
            num_intervals: Number of execution intervals
            interval_minutes: Minutes per interval

        Returns:
            List of child orders
        """
        slice_size = total_quantity / num_intervals

        orders = []
        for i in range(num_intervals):
            orders.append({
                "interval": i,
                "quantity": slice_size,
                "execution_time": i * interval_minutes,
                "strategy": "TWAP",
                "aggressive": True  # Market orders
            })

        return orders

    def vwap(self,
             total_quantity: float,
             volume_profile: List[float]) -> List[Dict[str, Any]]:
        """
        Volume-Weighted Average Price (VWAP) algorithm.

        Allocates order proportional to expected volume.

        Args:
            total_quantity: Total order size
            volume_profile: Expected volume distribution (sums to 1.0)

        Returns:
            List of child orders
        """
        if abs(sum(volume_profile) - 1.0) > 0.01:
            # Normalize
            volume_profile = [v / sum(volume_profile) for v in volume_profile]

        orders = []
        for i, vol_pct in enumerate(volume_profile):
            orders.append({
                "interval": i,
                "quantity": total_quantity * vol_pct,
                "volume_participation": vol_pct,
                "strategy": "VWAP",
                "aggressive": True
            })

        return orders

    def implementation_shortfall(self,
                                 total_quantity: float,
                                 adv: float,
                                 urgency: float = 0.5) -> List[Dict[str, Any]]:
        """
        Implementation Shortfall algorithm.

        Balances market impact vs. timing risk.

        Args:
            total_quantity: Total order size
            adv: Average daily volume
            urgency: Urgency level (0=passive, 1=aggressive)

        Returns:
            Optimal execution schedule
        """
        # Calculate optimal execution rate based on urgency
        max_participation = 0.05 + urgency * 0.15  # 5-20%

        num_intervals = self.impact_model.calculate_optimal_execution_time(
            total_quantity, adv, max_participation
        )

        # Front-load if urgent
        if urgency > 0.7:
            # Execute 50% immediately, rest over time
            immediate_qty = total_quantity * 0.5
            remaining_qty = total_quantity * 0.5

            orders = [{
                "interval": 0,
                "quantity": immediate_qty,
                "strategy": "IS-Aggressive",
                "aggressive": True
            }]

            # Split remaining
            slice_size = remaining_qty / (num_intervals - 1) if num_intervals > 1 else remaining_qty
            for i in range(1, num_intervals):
                orders.append({
                    "interval": i,
                    "quantity": slice_size,
                    "strategy": "IS-Passive",
                    "aggressive": False
                })
        else:
            # Use TWAP-like distribution
            orders = self.twap(total_quantity, num_intervals)
            for order in orders:
                order["strategy"] = "IS-Balanced"

        return orders

    def pov(self,
            total_quantity: float,
            target_participation: float = 0.10,
            max_participation: float = 0.20) -> Dict[str, Any]:
        """
        Percentage of Volume (POV) algorithm.

        Maintains target participation rate.

        Args:
            total_quantity: Total order size
            target_participation: Target participation rate
            max_participation: Maximum allowed participation

        Returns:
            POV execution parameters
        """
        return {
            "strategy": "POV",
            "total_quantity": total_quantity,
            "target_participation": target_participation,
            "max_participation": max_participation,
            "description": f"Maintain {target_participation*100:.1f}% of volume"
        }


class AdvancedBacktestEngine:
    """
    Advanced backtesting engine with realistic execution simulation.
    """

    def __init__(self,
                 initial_capital: float = 1000000,
                 commission_bps: float = 5.0,
                 min_commission: float = 1.0):
        """
        Args:
            initial_capital: Starting capital
            commission_bps: Commission in basis points
            min_commission: Minimum commission per trade
        """
        self.initial_capital = initial_capital
        self.commission_bps = commission_bps
        self.min_commission = min_commission

        # Models
        self.impact_model = MarketImpactModel()
        self.slippage_model = SlippageModel()
        self.execution_algo = ExecutionAlgorithm(self.impact_model)

        # State
        self.cash = initial_capital
        self.positions = {}
        self.order_books = {}
        self.trade_history = []
        self.pnl_history = []

    def simulate_order(self,
                      symbol: str,
                      side: str,
                      quantity: float,
                      current_price: float,
                      adv: float,
                      volatility: float,
                      execution_strategy: str = "market",
                      urgency: float = 0.5) -> Dict[str, Any]:
        """
        Simulate realistic order execution.

        Args:
            symbol: Instrument symbol
            side: 'buy' or 'sell'
            quantity: Order quantity
            current_price: Current market price
            adv: Average daily volume
            volatility: Daily volatility
            execution_strategy: 'market', 'twap', 'vwap', 'is'
            urgency: Urgency level for IS algorithm

        Returns:
            Execution results
        """
        # Get or create order book
        if symbol not in self.order_books:
            self.order_books[symbol] = OrderBook(current_price)

        order_book = self.order_books[symbol]
        order_book.update_price(current_price)

        # Calculate market impact
        impact = self.impact_model.calculate_impact(
            quantity, adv, volatility
        )

        # Calculate slippage
        slippage = self.slippage_model.calculate_slippage(
            quantity, adv, volatility,
            order_type="market" if execution_strategy == "market" else "limit"
        )

        # Execute based on strategy
        if execution_strategy == "market":
            # Immediate execution
            exec_price, total_cost = order_book.get_execution_price(
                side, quantity, aggressive=True
            )

            # Add market impact
            if side == 'buy':
                exec_price *= (1 + impact['total'])
            else:
                exec_price *= (1 - impact['total'])

            executions = [{
                "quantity": quantity,
                "price": exec_price,
                "timestamp": datetime.now(),
                "strategy": "market"
            }]

        elif execution_strategy == "twap":
            # TWAP execution
            num_intervals = 10
            child_orders = self.execution_algo.twap(quantity, num_intervals)

            executions = []
            cumulative_impact = 0.0

            for child in child_orders:
                # Each slice experiences less impact
                child_qty = child["quantity"]
                child_impact = self.impact_model.calculate_impact(
                    child_qty, adv, volatility
                )

                exec_price = current_price * (1 + cumulative_impact)
                if side == 'buy':
                    exec_price *= (1 + child_impact['total'])
                else:
                    exec_price *= (1 - child_impact['total'])

                executions.append({
                    "quantity": child_qty,
                    "price": exec_price,
                    "timestamp": datetime.now(),
                    "strategy": "twap",
                    "interval": child["interval"]
                })

                cumulative_impact += child_impact['permanent']

        else:  # Implementation Shortfall
            child_orders = self.execution_algo.implementation_shortfall(
                quantity, adv, urgency
            )

            executions = []
            cumulative_impact = 0.0

            for child in child_orders:
                child_qty = child["quantity"]
                child_impact = self.impact_model.calculate_impact(
                    child_qty, adv, volatility
                )

                exec_price = current_price * (1 + cumulative_impact)
                if side == 'buy':
                    exec_price *= (1 + child_impact['total'])
                else:
                    exec_price *= (1 - child_impact['total'])

                executions.append({
                    "quantity": child_qty,
                    "price": exec_price,
                    "timestamp": datetime.now(),
                    "strategy": "is",
                    "aggressive": child["aggressive"]
                })

                cumulative_impact += child_impact['permanent']

        # Calculate total execution
        total_qty = sum(e["quantity"] for e in executions)
        total_cost = sum(e["quantity"] * e["price"] for e in executions)
        avg_price = total_cost / total_qty if total_qty > 0 else current_price

        # Commission
        commission = max(
            self.min_commission,
            total_cost * (self.commission_bps / 10000)
        )

        # Total slippage (realized)
        realized_slippage = abs(avg_price - current_price) / current_price * 10000

        return {
            "success": True,
            "symbol": symbol,
            "side": side,
            "quantity": total_qty,
            "avg_price": avg_price,
            "total_cost": total_cost,
            "commission": commission,
            "market_impact_bps": impact['total'] * 10000,
            "expected_slippage_bps": slippage['total_bps'],
            "realized_slippage_bps": realized_slippage,
            "execution_strategy": execution_strategy,
            "num_executions": len(executions),
            "executions": executions
        }

    def calculate_transaction_costs(self,
                                    trades: List[Dict[str, Any]]) -> Dict[str, Any]:
        """
        Calculate total transaction costs for a set of trades.

        Args:
            trades: List of trade executions

        Returns:
            Breakdown of transaction costs
        """
        total_commission = sum(t.get("commission", 0) for t in trades)
        total_slippage = sum(
            t.get("total_cost", 0) * t.get("realized_slippage_bps", 0) / 10000
            for t in trades
        )
        total_impact = sum(
            t.get("total_cost", 0) * t.get("market_impact_bps", 0) / 10000
            for t in trades
        )

        total_costs = total_commission + total_slippage + total_impact
        total_traded = sum(t.get("total_cost", 0) for t in trades)

        return {
            "total_costs": total_costs,
            "commission": total_commission,
            "slippage": total_slippage,
            "market_impact": total_impact,
            "total_traded_value": total_traded,
            "cost_bps": (total_costs / total_traded * 10000) if total_traded > 0 else 0,
            "num_trades": len(trades)
        }


def run_backtest(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Run a multi-strategy backtest using market data fetched via yfinance.

    Expected params keys (all optional with defaults):
        dataset_config.instruments  : comma-separated tickers, e.g. "AAPL,MSFT"
        dataset_config.start_date   : "YYYY-MM-DD"
        dataset_config.end_date     : "YYYY-MM-DD"
        strategy_config.type        : "topk_dropout" | "weight_based" | "enhanced_indexing"
        strategy_config.topk        : int (default 10)
        portfolio_config.initial_capital : float (default 1_000_000)
        portfolio_config.benchmark  : str ticker (default "SPY")
    """
    if not PANDAS_AVAILABLE:
        return {"success": False, "error": "pandas/numpy not available"}

    try:
        import yfinance as yf
    except ImportError:
        return {"success": False, "error": "yfinance not installed. Run: pip install yfinance"}

    # ── Parse params ────────────────────────────────────────────────────────
    dataset_cfg   = params.get("dataset_config", {})
    strategy_cfg  = params.get("strategy_config", {})
    portfolio_cfg = params.get("portfolio_config", {})

    raw_instruments = dataset_cfg.get("instruments", "AAPL,MSFT,GOOG,AMZN")
    tickers    = [t.strip().upper() for t in raw_instruments.split(",") if t.strip()]
    start_date = dataset_cfg.get("start_date", "2020-01-01")
    end_date   = dataset_cfg.get("end_date",   "2024-01-01")

    strategy_type = strategy_cfg.get("type", "topk_dropout")
    topk          = int(strategy_cfg.get("topk", min(10, len(tickers))))

    initial_capital = float(portfolio_cfg.get("initial_capital", 1_000_000))
    benchmark_ticker = portfolio_cfg.get("benchmark", "SPY") or "SPY"

    # ── Fetch price data ─────────────────────────────────────────────────────
    all_tickers = list(set(tickers + [benchmark_ticker]))
    raw = yf.download(all_tickers, start=start_date, end=end_date,
                      auto_adjust=True, progress=False)

    if raw.empty:
        return {"success": False, "error": "No data returned from yfinance. Check tickers and date range."}

    # Handle single vs multi ticker response
    if isinstance(raw.columns, pd.MultiIndex):
        close = raw["Close"].dropna(how="all")
        volume = raw["Volume"].dropna(how="all")
    else:
        close = raw[["Close"]].rename(columns={"Close": tickers[0]}).dropna()
        volume = raw[["Volume"]].rename(columns={"Volume": tickers[0]}).dropna()

    # Drop benchmark from strategy tickers
    strat_tickers = [t for t in tickers if t in close.columns]
    if not strat_tickers:
        return {"success": False, "error": f"None of the requested tickers had data: {tickers}"}

    topk = min(topk, len(strat_tickers))

    # ── Simple momentum strategy signal ──────────────────────────────────────
    # Rank tickers by trailing 20-day return, hold top-K equally weighted
    returns       = close[strat_tickers].pct_change()
    momentum_20d  = close[strat_tickers].pct_change(20)

    portfolio_returns = []
    dates = returns.index[20:]  # skip warmup

    for dt in dates:
        row_mom = momentum_20d.loc[dt].dropna()
        if row_mom.empty:
            portfolio_returns.append(0.0)
            continue

        if strategy_type == "topk_dropout":
            selected = row_mom.nlargest(topk).index.tolist()
        elif strategy_type == "weight_based":
            # weight proportional to momentum score (long only)
            pos = row_mom[row_mom > 0]
            selected = pos.nlargest(topk).index.tolist()
        else:  # enhanced_indexing — equal weight all
            selected = row_mom.index.tolist()

        if not selected:
            portfolio_returns.append(0.0)
            continue

        day_ret = returns.loc[dt, selected].mean()
        portfolio_returns.append(float(day_ret) if not pd.isna(day_ret) else 0.0)

    # ── Benchmark returns ────────────────────────────────────────────────────
    bm_col = benchmark_ticker if benchmark_ticker in close.columns else None
    if bm_col:
        bm_returns = close[bm_col].pct_change().loc[dates].fillna(0.0).tolist()
    else:
        bm_returns = [0.0] * len(dates)

    # ── Compute equity curve & metrics ───────────────────────────────────────
    port_series = pd.Series(portfolio_returns, index=dates)
    bm_series   = pd.Series(bm_returns,        index=dates)

    port_cum = (1 + port_series).cumprod() * initial_capital
    bm_cum   = (1 + bm_series).cumprod()   * initial_capital

    # Annualised return
    n_years = len(dates) / 252
    final_val   = float(port_cum.iloc[-1]) if not port_cum.empty else initial_capital
    total_ret   = (final_val - initial_capital) / initial_capital
    ann_ret     = (1 + total_ret) ** (1 / n_years) - 1 if n_years > 0 else 0.0

    # Volatility & Sharpe
    ann_vol = float(port_series.std() * np.sqrt(252))
    sharpe  = ann_ret / ann_vol if ann_vol > 0 else 0.0

    # Max drawdown
    running_max = port_cum.cummax()
    drawdown    = (port_cum - running_max) / running_max
    max_dd      = float(drawdown.min())

    # Calmar
    calmar = ann_ret / abs(max_dd) if max_dd != 0 else 0.0

    # Win rate
    win_rate = float((port_series > 0).mean())

    # Transaction cost estimate (using AdvancedBacktestEngine)
    engine = AdvancedBacktestEngine(initial_capital=initial_capital)
    avg_price   = float(close[strat_tickers].iloc[-1].mean())
    avg_vol_val = float(volume[strat_tickers].iloc[-20:].mean().mean()) if strat_tickers[0] in volume.columns else 1e6
    sample_execution = engine.simulate_order(
        strat_tickers[0], "buy", 1000, avg_price, avg_vol_val,
        float(port_series.std()), "market"
    )

    # Equity curve (downsample to ~100 points for display)
    step = max(1, len(port_cum) // 100)
    equity_curve = [
        {"date": str(d.date()), "portfolio": round(float(v), 2),
         "benchmark": round(float(b), 2)}
        for d, v, b in zip(port_cum.index[::step],
                           port_cum.values[::step],
                           bm_cum.values[::step])
    ]

    return {
        "success": True,
        "strategy": strategy_type,
        "tickers": strat_tickers,
        "start_date": start_date,
        "end_date": end_date,
        "metrics": {
            "initial_capital":    round(initial_capital, 2),
            "final_value":        round(final_val, 2),
            "total_return_pct":   round(total_ret * 100, 2),
            "annualised_return":  round(ann_ret * 100, 2),
            "annualised_vol":     round(ann_vol * 100, 2),
            "sharpe_ratio":       round(sharpe, 3),
            "max_drawdown_pct":   round(max_dd * 100, 2),
            "calmar_ratio":       round(calmar, 3),
            "win_rate_pct":       round(win_rate * 100, 2),
            "trading_days":       len(dates),
        },
        "execution_cost_estimate": {
            "commission_bps":        round(sample_execution.get("market_impact_bps", 0), 2),
            "expected_slippage_bps": round(sample_execution.get("expected_slippage_bps", 0), 2),
        },
        "equity_curve": equity_curve,
    }


def optimize_portfolio(params: Dict[str, Any]) -> Dict[str, Any]:
    """Placeholder for portfolio optimisation — returns not-yet-implemented."""
    return {"success": False, "error": "optimize_portfolio not yet implemented"}


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "check_status":
            result = {
                "success": True,
                "pandas_available": PANDAS_AVAILABLE,
                "features": [
                    "Order book simulation",
                    "Market impact modeling",
                    "Slippage calculation",
                    "TWAP execution",
                    "VWAP execution",
                    "Implementation Shortfall",
                    "POV execution",
                    "Transaction cost analysis",
                    "run_backtest (yfinance momentum strategy)",
                ]
            }
            print(json.dumps(result))

        elif command == "run_backtest":
            raw = sys.argv[2] if len(sys.argv) > 2 else "{}"
            params = json.loads(raw)
            result = run_backtest(params)
            print(json.dumps(result))

        elif command == "optimize_portfolio":
            raw = sys.argv[2] if len(sys.argv) > 2 else "{}"
            params = json.loads(raw)
            result = optimize_portfolio(params)
            print(json.dumps(result))

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
