"""
FinRL Trading Environments
Wrapper around all FinRL gymnasium environments.

Covers:
- StockTradingEnv: Standard stock trading with buy/sell/hold actions
- StockTradingEnvCashPenalty: Penalizes holding excess cash
- StockTradingEnvStopLoss: Built-in stop-loss and profit-taking
- StockTradingEnvNP: NumPy-based high-performance environment
- StockPortfolioEnv: Portfolio allocation with weight-based actions
- PortfolioOptimizationEnv: Advanced portfolio optimization
- CryptoEnv: Multi-cryptocurrency trading
- BitcoinEnv: Single BTC trading with CCXT data

Usage (CLI):
    python finrl_environments.py --action list_envs
    python finrl_environments.py --action create --env stock_trading --data_file train.csv --tickers AAPL MSFT --initial_amount 1000000
    python finrl_environments.py --action info --env stock_trading --stock_dim 30
    python finrl_environments.py --action create --env crypto --data_file crypto.csv
    python finrl_environments.py --action create --env portfolio --data_file train.csv --tickers AAPL MSFT GOOGL
    python finrl_environments.py --action create --env cash_penalty --data_file train.csv
    python finrl_environments.py --action create --env stop_loss --data_file train.csv
"""

import json
import sys
import os
import argparse
import numpy as np
import pandas as pd

from finrl.meta.env_stock_trading.env_stocktrading import (
    StockTradingEnv,
)
from finrl.meta.env_stock_trading.env_stocktrading_cashpenalty import (
    StockTradingEnvCashpenalty,
)
from finrl.meta.env_stock_trading.env_stocktrading_stoploss import (
    StockTradingEnvStopLoss,
)
from finrl.meta.env_stock_trading.env_stocktrading_np import (
    StockTradingEnv as StockTradingEnvNP,
)
from finrl.meta.env_portfolio_allocation.env_portfolio import (
    StockPortfolioEnv,
)
from finrl.meta.env_portfolio_optimization.env_portfolio_optimization import (
    PortfolioOptimizationEnv,
)
from finrl.meta.env_cryptocurrency_trading.env_multiple_crypto import (
    CryptoEnv,
)
from finrl.meta.env_cryptocurrency_trading.env_btc_ccxt import (
    BitcoinEnv,
)
from finrl.config import INDICATORS

from stable_baselines3.common.vec_env import DummyVecEnv


# ============================================================================
# Environment Registry
# ============================================================================

ENVIRONMENT_REGISTRY = {
    "stock_trading": {
        "class": "StockTradingEnv",
        "description": "Standard stock trading with discrete buy/sell/hold per stock",
        "action_type": "continuous [-hmax, hmax] per stock",
        "features": ["Technical indicators", "Turbulence threshold", "Make plots", "State memory"],
    },
    "cash_penalty": {
        "class": "StockTradingEnvCashpenalty",
        "description": "Stock trading that penalizes holding too much cash",
        "action_type": "continuous or discrete",
        "features": ["Cash penalty", "Random start", "Patient mode", "Multi-process support"],
    },
    "stop_loss": {
        "class": "StockTradingEnvStopLoss",
        "description": "Stock trading with automatic stop-loss and profit-taking",
        "action_type": "continuous or discrete",
        "features": ["Stop-loss", "Profit-loss ratio", "Cash penalty", "Random start"],
    },
    "stock_np": {
        "class": "StockTradingEnvNP",
        "description": "High-performance NumPy-based stock trading (for ElegantRL)",
        "action_type": "continuous with sigmoid",
        "features": ["NumPy arrays", "Turbulence", "High performance"],
    },
    "portfolio": {
        "class": "StockPortfolioEnv",
        "description": "Portfolio allocation with weight-based actions (softmax normalized)",
        "action_type": "portfolio weights [0, 1] summing to 1",
        "features": ["Softmax normalization", "Covariance matrix state", "Lookback window"],
    },
    "portfolio_optimization": {
        "class": "PortfolioOptimizationEnv",
        "description": "Advanced portfolio optimization with time windows",
        "action_type": "portfolio weights",
        "features": ["Commission fees", "Time windows", "Multiple normalization modes"],
    },
    "crypto": {
        "class": "CryptoEnv",
        "description": "Multi-cryptocurrency trading environment",
        "action_type": "continuous per crypto",
        "features": ["Multiple cryptocurrencies", "Lookback period", "CCXT integration"],
    },
    "bitcoin": {
        "class": "BitcoinEnv",
        "description": "Bitcoin single-asset trading with CCXT data",
        "action_type": "continuous [-1, 1]",
        "features": ["Sub-minute data", "Cumulative return tracking"],
    },
}


# ============================================================================
# Environment Factory Functions
# ============================================================================

def create_stock_trading_env(
    df: pd.DataFrame,
    stock_dim: int,
    hmax: int = 100,
    initial_amount: int = 1_000_000,
    buy_cost_pct: list = None,
    sell_cost_pct: list = None,
    reward_scaling: float = 1e-4,
    tech_indicator_list: list = None,
    turbulence_threshold: float = None,
    risk_indicator_col: str = "turbulence",
    make_plots: bool = False,
    print_verbosity: int = 10,
    day: int = 0,
    initial: bool = True,
    num_stock_shares: list = None,
    vectorize: bool = False,
) -> StockTradingEnv:
    """Create standard StockTradingEnv."""
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)
    if buy_cost_pct is None:
        buy_cost_pct = [0.001] * stock_dim
    if sell_cost_pct is None:
        sell_cost_pct = [0.001] * stock_dim
    if num_stock_shares is None:
        num_stock_shares = [0] * stock_dim

    state_space = 1 + 2 * stock_dim + len(tech_indicator_list) * stock_dim
    action_space = stock_dim

    env = StockTradingEnv(
        df=df,
        stock_dim=stock_dim,
        hmax=hmax,
        initial_amount=initial_amount,
        num_stock_shares=num_stock_shares,
        buy_cost_pct=buy_cost_pct,
        sell_cost_pct=sell_cost_pct,
        reward_scaling=reward_scaling,
        state_space=state_space,
        action_space=action_space,
        tech_indicator_list=tech_indicator_list,
        turbulence_threshold=turbulence_threshold,
        risk_indicator_col=risk_indicator_col,
        make_plots=make_plots,
        print_verbosity=print_verbosity,
        day=day,
        initial=initial,
    )

    if vectorize:
        return DummyVecEnv([lambda: env])
    return env


def create_cash_penalty_env(
    df: pd.DataFrame,
    hmax: int = 10,
    initial_amount: float = 1_000_000,
    buy_cost_pct: float = 0.003,
    sell_cost_pct: float = 0.003,
    cash_penalty_proportion: float = 0.1,
    daily_info_cols: list = None,
    discrete_actions: bool = False,
    random_start: bool = True,
    patient: bool = False,
    turbulence_threshold: float = None,
    print_verbosity: int = 10,
    currency: str = "$",
    vectorize: bool = False,
) -> StockTradingEnvCashpenalty:
    """Create cash-penalty stock trading environment."""
    if daily_info_cols is None:
        daily_info_cols = ["open", "close", "high", "low", "volume"]

    env = StockTradingEnvCashpenalty(
        df=df,
        buy_cost_pct=buy_cost_pct,
        sell_cost_pct=sell_cost_pct,
        hmax=hmax,
        discrete_actions=discrete_actions,
        turbulence_threshold=turbulence_threshold,
        print_verbosity=print_verbosity,
        initial_amount=initial_amount,
        daily_information_cols=daily_info_cols,
        cash_penalty_proportion=cash_penalty_proportion,
        random_start=random_start,
        patient=patient,
        currency=currency,
    )

    if vectorize:
        return DummyVecEnv([lambda: env])
    return env


def create_stop_loss_env(
    df: pd.DataFrame,
    hmax: int = 10,
    initial_amount: float = 1_000_000,
    buy_cost_pct: float = 0.003,
    sell_cost_pct: float = 0.003,
    stoploss_penalty: float = 0.9,
    profit_loss_ratio: float = 2.0,
    cash_penalty_proportion: float = 0.1,
    daily_info_cols: list = None,
    discrete_actions: bool = False,
    random_start: bool = True,
    patient: bool = False,
    turbulence_threshold: float = None,
    print_verbosity: int = 10,
    currency: str = "$",
    vectorize: bool = False,
) -> StockTradingEnvStopLoss:
    """Create stop-loss stock trading environment."""
    if daily_info_cols is None:
        daily_info_cols = ["open", "close", "high", "low", "volume"]

    env = StockTradingEnvStopLoss(
        df=df,
        buy_cost_pct=buy_cost_pct,
        sell_cost_pct=sell_cost_pct,
        hmax=hmax,
        discrete_actions=discrete_actions,
        stoploss_penalty=stoploss_penalty,
        profit_loss_ratio=profit_loss_ratio,
        turbulence_threshold=turbulence_threshold,
        print_verbosity=print_verbosity,
        initial_amount=initial_amount,
        daily_information_cols=daily_info_cols,
        cash_penalty_proportion=cash_penalty_proportion,
        random_start=random_start,
        patient=patient,
        currency=currency,
    )

    if vectorize:
        return DummyVecEnv([lambda: env])
    return env


def create_portfolio_env(
    df: pd.DataFrame,
    stock_dim: int,
    hmax: int = 100,
    initial_amount: int = 1_000_000,
    transaction_cost_pct: float = 0.001,
    reward_scaling: float = 1e-4,
    tech_indicator_list: list = None,
    turbulence_threshold: float = None,
    lookback: int = 252,
    vectorize: bool = False,
) -> StockPortfolioEnv:
    """Create portfolio allocation environment."""
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)

    state_space = stock_dim
    action_space = stock_dim

    env = StockPortfolioEnv(
        df=df,
        stock_dim=stock_dim,
        hmax=hmax,
        initial_amount=initial_amount,
        transaction_cost_pct=transaction_cost_pct,
        reward_scaling=reward_scaling,
        state_space=state_space,
        action_space=action_space,
        tech_indicator_list=tech_indicator_list,
        turbulence_threshold=turbulence_threshold,
        lookback=lookback,
    )

    if vectorize:
        return DummyVecEnv([lambda: env])
    return env


def create_portfolio_optimization_env(
    df: pd.DataFrame,
    initial_amount: float = 1_000_000,
    features: list = None,
    time_window: int = 1,
    commission_fee_pct: float = 0.0,
    commission_fee_model: str = "trf",
    normalize_df: str = "by_previous_time",
    reward_scaling: float = 1.0,
    return_last_action: bool = False,
    tics_in_portfolio: str = "all",
    vectorize: bool = False,
) -> PortfolioOptimizationEnv:
    """Create advanced portfolio optimization environment."""
    if features is None:
        features = ["close", "high", "low"]

    env = PortfolioOptimizationEnv(
        df=df,
        initial_amount=initial_amount,
        features=features,
        time_window=time_window,
        comission_fee_pct=commission_fee_pct,
        comission_fee_model=commission_fee_model,
        normalize_df=normalize_df,
        reward_scaling=reward_scaling,
        return_last_action=return_last_action,
        tics_in_portfolio=tics_in_portfolio,
    )

    if vectorize:
        return DummyVecEnv([lambda: env])
    return env


def create_crypto_env(
    config: dict,
    lookback: int = 1,
    initial_capital: float = 1_000_000,
    buy_cost_pct: float = 0.001,
    sell_cost_pct: float = 0.001,
    gamma: float = 0.99,
) -> CryptoEnv:
    """Create multi-cryptocurrency trading environment."""
    return CryptoEnv(
        config=config,
        lookback=lookback,
        initial_capital=initial_capital,
        buy_cost_pct=buy_cost_pct,
        sell_cost_pct=sell_cost_pct,
        gamma=gamma,
    )


def create_bitcoin_env(
    price_ary=None,
    tech_ary=None,
    time_frequency: int = 15,
    initial_account: float = 1_000_000,
    max_stock: float = 100.0,
    transaction_fee_percent: float = 0.001,
    mode: str = "train",
    gamma: float = 0.99,
) -> BitcoinEnv:
    """Create Bitcoin trading environment."""
    return BitcoinEnv(
        price_ary=price_ary,
        tech_ary=tech_ary,
        time_frequency=time_frequency,
        initial_account=initial_account,
        max_stock=max_stock,
        transaction_fee_percent=transaction_fee_percent,
        mode=mode,
        gamma=gamma,
    )


# ============================================================================
# Environment Info
# ============================================================================

def get_env_info(env_type: str, stock_dim: int = 30, indicators: list = None) -> dict:
    """Get computed environment dimensions."""
    if indicators is None:
        indicators = list(INDICATORS)

    if env_type in ("stock_trading", "stock_np"):
        state_space = 1 + 2 * stock_dim + len(indicators) * stock_dim
        action_space = stock_dim
    elif env_type in ("portfolio", "portfolio_optimization"):
        state_space = stock_dim
        action_space = stock_dim
    elif env_type == "crypto":
        state_space = stock_dim * (1 + len(indicators))
        action_space = stock_dim
    elif env_type == "bitcoin":
        state_space = 1 + 1 + len(indicators)
        action_space = 1
    else:
        state_space = 1 + 2 * stock_dim + len(indicators) * stock_dim
        action_space = stock_dim

    return {
        "env_type": env_type,
        "env_info": ENVIRONMENT_REGISTRY.get(env_type, {}),
        "stock_dim": stock_dim,
        "state_space": state_space,
        "action_space": action_space,
        "num_indicators": len(indicators),
        "indicators": indicators,
    }


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="FinRL Environment Manager")
    parser.add_argument("--action", required=True,
                        choices=["list_envs", "create", "info"])
    parser.add_argument("--env", default="stock_trading",
                        choices=list(ENVIRONMENT_REGISTRY.keys()))
    parser.add_argument("--data_file", help="CSV data file path")
    parser.add_argument("--tickers", nargs="*")
    parser.add_argument("--stock_dim", type=int, default=30)
    parser.add_argument("--initial_amount", type=float, default=1_000_000)
    parser.add_argument("--hmax", type=int, default=100)
    parser.add_argument("--buy_cost", type=float, default=0.001)
    parser.add_argument("--sell_cost", type=float, default=0.001)
    parser.add_argument("--reward_scaling", type=float, default=1e-4)
    parser.add_argument("--turbulence_threshold", type=float, default=None)
    parser.add_argument("--indicators", nargs="*")

    args = parser.parse_args()

    if args.action == "list_envs":
        result = {name: info for name, info in ENVIRONMENT_REGISTRY.items()}

    elif args.action == "info":
        result = get_env_info(args.env, args.stock_dim, args.indicators)

    elif args.action == "create":
        if not args.data_file or not os.path.exists(args.data_file):
            result = {"error": "Provide valid --data_file CSV path"}
            print(json.dumps(result, indent=2))
            return

        df = pd.read_csv(args.data_file)
        stock_dim = args.stock_dim
        if args.tickers:
            stock_dim = len(args.tickers)
        elif "tic" in df.columns:
            stock_dim = df["tic"].nunique()

        indicators = args.indicators or list(INDICATORS)

        if args.env == "stock_trading":
            env = create_stock_trading_env(
                df=df, stock_dim=stock_dim, hmax=args.hmax,
                initial_amount=int(args.initial_amount),
                tech_indicator_list=indicators,
                turbulence_threshold=args.turbulence_threshold,
                reward_scaling=args.reward_scaling,
            )
        elif args.env == "cash_penalty":
            env = create_cash_penalty_env(
                df=df, hmax=args.hmax, initial_amount=args.initial_amount,
                buy_cost_pct=args.buy_cost, sell_cost_pct=args.sell_cost,
            )
        elif args.env == "stop_loss":
            env = create_stop_loss_env(
                df=df, hmax=args.hmax, initial_amount=args.initial_amount,
                buy_cost_pct=args.buy_cost, sell_cost_pct=args.sell_cost,
            )
        elif args.env == "portfolio":
            env = create_portfolio_env(
                df=df, stock_dim=stock_dim, hmax=args.hmax,
                initial_amount=int(args.initial_amount),
                tech_indicator_list=indicators,
            )
        elif args.env == "portfolio_optimization":
            env = create_portfolio_optimization_env(
                df=df, initial_amount=args.initial_amount,
            )
        else:
            result = {"error": f"Environment '{args.env}' requires array-based config, use Python API"}
            print(json.dumps(result, indent=2))
            return

        obs_shape = env.observation_space.shape if hasattr(env.observation_space, 'shape') else "N/A"
        act_shape = env.action_space.shape if hasattr(env.action_space, 'shape') else "N/A"

        result = {
            "status": "created",
            "env_type": args.env,
            "observation_space": str(obs_shape),
            "action_space": str(act_shape),
            "stock_dim": stock_dim,
            "data_rows": len(df),
        }

    else:
        result = {"error": f"Unknown action: {args.action}"}

    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()
