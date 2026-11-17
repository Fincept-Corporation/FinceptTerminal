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
import os
import sys
from typing import Dict, List, Any, Optional, Union, Tuple
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import fincept_json

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


_STRATEGY_LABELS = {
    "topk_dropout": "Top-K dropout (20-day momentum)",
    "weight_based": "Momentum-weighted top-K (positive momentum only)",
    "enhanced_indexing": "Equal weight (all names)",
}


def _topk_dropout_targets(score: "pd.Series", held: List[str], topk: int, n_drop: int) -> List[str]:
    """Qlib TopkDropoutStrategy book update (method_sell='bottom', method_buy='top').

    Keeps the current book and only swaps out up to ``n_drop`` names per rebalance:
    the held names ranked worst among (held + the best non-held candidates) are sold
    and replaced by the best-ranked names not held.
    """
    score = score.dropna()
    held = [h for h in held if h in score.index]
    last = score.loc[held].sort_values(ascending=False).index.tolist() if held else []
    candidates = score.drop(index=last, errors="ignore").sort_values(ascending=False)
    today = candidates.index[:max(0, n_drop + topk - len(last))].tolist()
    comb = score.loc[last + today].sort_values(ascending=False).index.tolist()
    sell = [n for n in last if n in comb[-n_drop:]] if n_drop > 0 else []
    buy = today[:len(sell) + topk - len(last)]
    return [n for n in last if n not in sell] + buy


def run_backtest(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Long-only daily backtest on Yahoo Finance prices, signal = trailing 20-day return.

    params:
        dataset_config.instruments  : "AAPL,MSFT,..." (required)
        dataset_config.start_date / end_date : "YYYY-MM-DD" (default: the last 3 years)
        strategy_config.type        : "topk_dropout" | "weight_based" | "enhanced_indexing"
            topk_dropout       hold `topk` names equally weighted; each rebalance replaces at
                               most `n_drop` of them (Qlib TopK-Dropout) on the momentum ranking
            weight_based       the top `topk` names with positive momentum, weighted by their
                               momentum score (cash when none is positive)
            enhanced_indexing  equal weight of every instrument (an equal-weight benchmark; the
                               name is kept for compatibility -- there is no tracking-error model)
        strategy_config.topk / n_drop / rebalance_days (default 10 / 1 / 5)
        portfolio_config.initial_capital (default 1e6), benchmark (default "SPY"),
            commission_bps / slippage_bps charged per unit of one-way turnover (default 5 / 5),
            risk_free_rate (annual, default 0.02)

    Positions are chosen from the momentum as of the previous close and earn the next
    day's return (no look-ahead); weights drift with prices between rebalances; costs
    are turnover x (commission + slippage) and are deducted from the day's return, so
    every metric is NET of costs (gross figures are reported next to them).
    """
    if not PANDAS_AVAILABLE:
        return {"success": False, "error": "pandas/numpy not available"}

    try:
        import fincept_yahoo as fy  # the shared Yahoo layer: bar cache, limiter, back-off
        fy.yf()
    except ImportError:
        return {"success": False, "error": "yfinance is not installed"}

    # ── Parse params ────────────────────────────────────────────────────────
    dataset_cfg   = params.get("dataset_config") or {}
    strategy_cfg  = params.get("strategy_config") or {}
    portfolio_cfg = params.get("portfolio_config") or {}

    raw_instruments = dataset_cfg.get("instruments") or ""
    if isinstance(raw_instruments, (list, tuple)):
        raw_instruments = ",".join(str(t) for t in raw_instruments)
    tickers = list(dict.fromkeys(t.strip().upper() for t in str(raw_instruments).split(",") if t.strip()))
    if not tickers:
        return {"success": False, "error": "dataset_config.instruments is empty -- give at least one ticker"}

    today = datetime.now().date()
    end_date = str(dataset_cfg.get("end_date") or today.isoformat())
    start_date = str(dataset_cfg.get("start_date") or (today - timedelta(days=3 * 365)).isoformat())

    strategy_type = str(strategy_cfg.get("type") or "topk_dropout")
    if strategy_type not in _STRATEGY_LABELS:
        return {"success": False,
                "error": f"Unknown strategy '{strategy_type}'. Use one of: {', '.join(_STRATEGY_LABELS)}"}
    topk = int(strategy_cfg.get("topk", 10))
    n_drop = int(strategy_cfg.get("n_drop", 1))
    rebalance_days = int(strategy_cfg.get("rebalance_days", 5))
    if topk < 1 or n_drop < 0 or rebalance_days < 1:
        return {"success": False, "error": "topk and rebalance_days must be >= 1 and n_drop >= 0"}

    initial_capital = float(portfolio_cfg.get("initial_capital", 1_000_000))
    benchmark_ticker = (str(portfolio_cfg.get("benchmark") or "SPY")).strip().upper()
    commission_bps = float(portfolio_cfg.get("commission_bps", 5.0))
    slippage_bps = float(portfolio_cfg.get("slippage_bps", 5.0))
    rf_annual = float(portfolio_cfg.get("risk_free_rate", 0.02))
    if initial_capital <= 0 or commission_bps < 0 or slippage_bps < 0:
        return {"success": False, "error": "initial_capital must be > 0 and costs >= 0"}
    cost_rate = (commission_bps + slippage_bps) / 10_000.0

    # ── Fetch price data ─────────────────────────────────────────────────────
    all_tickers = list(dict.fromkeys(tickers + [benchmark_ticker]))
    # One long download per symbol shared by every script; later runs slice it locally.
    raw = fy.download(all_tickers, start=start_date, end=end_date, auto_adjust=True)

    if raw is None or raw.empty:
        return {"success": False, "error": "No data returned from Yahoo Finance. Check tickers and date range."}

    if isinstance(raw.columns, pd.MultiIndex):
        close = raw["Close"].dropna(how="all")
    else:
        close = raw[["Close"]].rename(columns={"Close": all_tickers[0]}).dropna()

    strat_tickers = [t for t in tickers if t in close.columns and close[t].notna().sum() > 25]
    missing = [t for t in tickers if t not in strat_tickers]
    if not strat_tickers:
        return {"success": False, "error": f"None of the requested tickers had enough price data: {tickers}"}

    topk = min(topk, len(strat_tickers))
    n_drop = min(n_drop, topk)

    prices = close[strat_tickers]
    returns = prices.pct_change()
    # Momentum as of the PREVIOUS close: positions chosen on it earn today's return.
    momentum = prices.pct_change(20).shift(1)
    dates = returns.index[21:]
    if len(dates) < 20:
        return {"success": False,
                "error": f"Only {len(dates)} trading days after the 20-day warm-up; widen the date range"}

    # ── Simulate ─────────────────────────────────────────────────────────────
    weights = pd.Series(0.0, index=strat_tickers)    # drifted weights going into the day
    held: List[str] = []
    gross_rets, net_rets, turnovers = [], [], []
    n_rebalances = 0
    for i, dt in enumerate(dates):
        cost = 0.0
        if i % rebalance_days == 0:
            score = momentum.loc[dt].dropna()
            target = pd.Series(0.0, index=strat_tickers)
            if not score.empty:
                if strategy_type == "topk_dropout":
                    held = _topk_dropout_targets(score, held, topk, n_drop)
                    if held:
                        target.loc[held] = 1.0 / len(held)
                elif strategy_type == "weight_based":
                    pos = score[score > 0].nlargest(topk)
                    if not pos.empty:
                        target.loc[pos.index] = pos / pos.sum()
                else:
                    avail = score.index.tolist()
                    target.loc[avail] = 1.0 / len(avail)
            turnover = float((target - weights).abs().sum())
            cost = turnover * cost_rate
            turnovers.append(turnover)
            weights = target
            n_rebalances += 1
        day = returns.loc[dt, strat_tickers].fillna(0.0)
        gross = float((weights * day).sum())
        gross_rets.append(gross)
        net_rets.append(gross - cost)
        # Drift: each holding grows with its own return; cash (1 - sum w) earns nothing.
        grown = weights * (1.0 + day)
        total = float(grown.sum() + (1.0 - weights.sum()))
        weights = grown / total if total > 0 else grown

    # ── Benchmark ────────────────────────────────────────────────────────────
    # yf.download keeps an all-NaN column for a symbol it could not resolve.
    bm_col = (benchmark_ticker
              if benchmark_ticker in close.columns and close[benchmark_ticker].notna().sum() > 1
              else None)
    bm_series = (close[bm_col].pct_change().reindex(dates).fillna(0.0) if bm_col
                 else pd.Series(np.nan, index=dates))

    port_series = pd.Series(net_rets, index=dates)
    gross_series = pd.Series(gross_rets, index=dates)
    port_cum = (1 + port_series).cumprod() * initial_capital
    bm_cum = (1 + bm_series).cumprod() * initial_capital if bm_col else None

    def stats(r: pd.Series) -> Dict[str, Any]:
        n_years = len(r) / 252.0
        total = float((1 + r).prod() - 1)
        ann = (1 + total) ** (1 / n_years) - 1 if n_years > 0 and total > -1 else None
        vol = float(r.std(ddof=1) * np.sqrt(252)) if len(r) > 1 else None
        excess = r - rf_annual / 252.0
        sd = float(excess.std(ddof=1))
        sharpe = float(excess.mean() / sd * np.sqrt(252)) if sd > 0 else None
        cum = (1 + r).cumprod()
        peak = cum.cummax().clip(lower=1.0)
        mdd = float((cum / peak - 1).min())
        calmar = (ann / abs(mdd)) if ann is not None and mdd < 0 else None
        return {"total": total, "ann": ann, "vol": vol, "sharpe": sharpe, "mdd": mdd, "calmar": calmar}

    def pct(v, nd=2):
        return None if v is None else round(v * 100, nd)

    def rnd(v, nd=3):
        return None if v is None else round(v, nd)

    net = stats(port_series)
    gross = stats(gross_series)
    bm = stats(bm_series) if bm_col else None
    final_val = float(port_cum.iloc[-1])
    n_years = len(dates) / 252.0
    total_turnover = float(sum(turnovers))
    total_cost_pct = float(sum(t * cost_rate for t in turnovers))

    # Equity curve (downsampled to ~100 points for display, always ending on the last day)
    step = max(1, len(port_cum) // 100)
    idx = list(range(0, len(port_cum), step))
    if idx[-1] != len(port_cum) - 1:
        idx.append(len(port_cum) - 1)
    equity_curve = [
        {"date": str(port_cum.index[k].date()),
         "portfolio": round(float(port_cum.iloc[k]), 2),
         "benchmark": round(float(bm_cum.iloc[k]), 2) if bm_cum is not None else None}
        for k in idx
    ]

    warnings_out = []
    if bm_col is None:
        warnings_out.append(f"Benchmark '{benchmark_ticker}' returned no price data from Yahoo Finance, so there "
                            "is no benchmark line. Use a Yahoo symbol such as SPY.")
    if missing:
        warnings_out.append(f"No usable price data for: {', '.join(missing)} (left out).")
    if strategy_type != "enhanced_indexing" and topk >= len(strat_tickers):
        warnings_out.append(f"Top-K ({topk}) covers the whole universe of {len(strat_tickers)} names, so the "
                            "ranking selects nothing -- this is an equal-weight portfolio.")

    return {
        "success": True,
        "strategy": strategy_type,
        "strategy_label": _STRATEGY_LABELS[strategy_type],
        "tickers": strat_tickers,
        "benchmark": benchmark_ticker if bm_col else None,
        **({"warning": " ".join(warnings_out)} if warnings_out else {}),
        "start_date": str(dates[0].date()),
        "end_date": str(dates[-1].date()),
        "parameters": {
            "topk": topk, "n_drop": n_drop if strategy_type == "topk_dropout" else None,
            "rebalance_days": rebalance_days, "signal": "20-day trailing return (as of the previous close)",
            "risk_free_rate": rf_annual,
        },
        "metrics": {
            "net_of_costs":       True,
            "initial_capital":    round(initial_capital, 2),
            "final_value":        round(final_val, 2),
            "total_return_pct":   pct(net["total"]),
            "annualised_return":  pct(net["ann"]),
            "annualised_vol":     pct(net["vol"]),
            "sharpe_ratio":       rnd(net["sharpe"]),
            "max_drawdown_pct":   pct(net["mdd"]),
            "calmar_ratio":       rnd(net["calmar"]),
            "win_rate_pct":       round(float((port_series > 0).mean()) * 100, 2),
            "trading_days":       len(dates),
            "gross_total_return_pct": pct(gross["total"]),
            "gross_sharpe_ratio": rnd(gross["sharpe"]),
            "benchmark_total_return_pct": pct(bm["total"]) if bm else None,
            "benchmark_sharpe_ratio": rnd(bm["sharpe"]) if bm else None,
        },
        # What the costs actually were in this run (they used to be one hypothetical
        # 1000-share order with a volatility term that put AAPL at ~100 bps).
        "costs": {
            "commission_bps": commission_bps,
            "slippage_bps": slippage_bps,
            "rebalances": n_rebalances,
            "avg_turnover_per_rebalance_pct": round(total_turnover / n_rebalances * 100, 2) if n_rebalances else None,
            "annual_turnover_pct": round(total_turnover / n_years * 100, 1) if n_years > 0 else None,
            "total_cost_drag_pct": round(total_cost_pct * 100, 3),
        },
        "equity_curve": equity_curve,
    }


def optimize_portfolio(params: Dict[str, Any]) -> Dict[str, Any]:
    """Placeholder for portfolio optimisation — returns not-yet-implemented."""
    return {"success": False, "error": "optimize_portfolio not yet implemented"}


def _json_safe(obj):
    """Replace NaN / +-Infinity with None before json.dumps (they are not valid JSON)."""
    if isinstance(obj, float):
        return obj if obj == obj and obj not in (float('inf'), float('-inf')) else None
    if isinstance(obj, dict):
        return {k: _json_safe(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [_json_safe(v) for v in obj]
    return obj


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        fincept_json.emit({"success": False, "error": "No command specified"})
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
            fincept_json.emit(result)

        elif command == "run_backtest":
            raw = sys.argv[2] if len(sys.argv) > 2 else "{}"
            params = json.loads(raw)
            result = run_backtest(params)
            fincept_json.emit(_json_safe(result))

        elif command == "optimize_portfolio":
            raw = sys.argv[2] if len(sys.argv) > 2 else "{}"
            params = json.loads(raw)
            result = optimize_portfolio(params)
            fincept_json.emit(result)

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            fincept_json.emit(result)

    except Exception as e:
        fincept_json.emit({"success": False, "error": str(e)})
        sys.exit(1)


if __name__ == "__main__":
    main()
