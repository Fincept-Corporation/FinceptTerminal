"""
FinRL Portfolio Optimization with DRL
Wrapper around finrl.meta.env_portfolio_allocation, env_portfolio_optimization,
and finrl.agents.portfolio_optimization.

Covers:
- Portfolio allocation environment (softmax weight-based)
- Portfolio optimization environment (advanced, with time windows)
- DRL-based portfolio management training
- Mean-variance comparison
- Portfolio weight analysis
- Rebalancing strategies

Usage (CLI):
    python finrl_portfolio.py --action list_modes
    python finrl_portfolio.py --action train_allocation --data_file train.csv --tickers AAPL MSFT GOOGL AMZN --algorithm ppo --timesteps 50000
    python finrl_portfolio.py --action train_optimization --data_file train.csv --tickers AAPL MSFT GOOGL --time_window 5
    python finrl_portfolio.py --action backtest --model_path trained_models/portfolio_ppo.zip --data_file test.csv --tickers AAPL MSFT GOOGL
    python finrl_portfolio.py --action analyze_weights --weights_file results/portfolio_weights.csv
"""

import json
import sys
import os
import argparse
import numpy as np
import pandas as pd

from finrl.agents.stablebaselines3.models import DRLAgent
from finrl.config import INDICATORS, TRAINED_MODEL_DIR, RESULTS_DIR
from stable_baselines3.common.vec_env import DummyVecEnv

from finrl_environments import (
    create_portfolio_env,
    create_portfolio_optimization_env,
)


# ============================================================================
# Portfolio Mode Registry
# ============================================================================

PORTFOLIO_MODES = {
    "allocation": {
        "description": "Portfolio allocation using softmax-normalized weights",
        "action_type": "Continuous weights -> softmax normalized to sum to 1",
        "env_class": "StockPortfolioEnv",
        "features": [
            "Covariance matrix in state",
            "Lookback window (default 252 days)",
            "Turbulence threshold support",
            "Transaction cost support",
        ],
    },
    "optimization": {
        "description": "Advanced portfolio optimization with time windows and commission models",
        "action_type": "Portfolio weights with commission fee models",
        "env_class": "PortfolioOptimizationEnv",
        "features": [
            "Time window for multi-period state",
            "Multiple normalization modes (by_previous_time, by_COLUMN, by_fist_time)",
            "Commission fee models (trf = transaction remainder factor, wvm = worth vector model)",
            "Custom feature selection (close, high, low, volume, etc.)",
        ],
    },
}


# ============================================================================
# Training Functions
# ============================================================================

def train_portfolio_allocation(
    df: pd.DataFrame,
    stock_dim: int = None,
    hmax: int = 100,
    initial_amount: int = 1_000_000,
    transaction_cost_pct: float = 0.001,
    reward_scaling: float = 1e-4,
    tech_indicator_list: list = None,
    turbulence_threshold: float = None,
    lookback: int = 252,
    algorithm: str = "ppo",
    total_timesteps: int = 50_000,
    policy: str = "MlpPolicy",
    save_path: str = None,
    verbose: int = 1,
) -> dict:
    """Train a DRL agent for portfolio allocation."""
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)
    if stock_dim is None:
        stock_dim = df["tic"].nunique() if "tic" in df.columns else 1

    # Create environment
    env = create_portfolio_env(
        df=df,
        stock_dim=stock_dim,
        hmax=hmax,
        initial_amount=initial_amount,
        transaction_cost_pct=transaction_cost_pct,
        reward_scaling=reward_scaling,
        tech_indicator_list=tech_indicator_list,
        turbulence_threshold=turbulence_threshold,
        lookback=lookback,
    )

    # Wrap for SB3
    env_train, _ = env.get_sb_env()

    # Train
    agent = DRLAgent(env=env_train)
    from finrl.agents.stablebaselines3.models import MODEL_KWARGS
    model_kwargs = MODEL_KWARGS.get(algorithm.lower(), {})

    model = agent.get_model(
        model_name=algorithm.lower(),
        policy=policy,
        model_kwargs=model_kwargs,
        verbose=verbose,
    )
    trained_model = agent.train_model(
        model=model,
        tb_log_name=f"portfolio_{algorithm}",
        total_timesteps=total_timesteps,
    )

    # Save
    if save_path is None:
        os.makedirs(TRAINED_MODEL_DIR, exist_ok=True)
        save_path = os.path.join(TRAINED_MODEL_DIR, f"portfolio_{algorithm}")
    trained_model.save(save_path)

    return {
        "status": "success",
        "mode": "allocation",
        "algorithm": algorithm,
        "stock_dim": stock_dim,
        "lookback": lookback,
        "total_timesteps": total_timesteps,
        "model_saved_to": save_path,
    }


def train_portfolio_optimization(
    df: pd.DataFrame,
    initial_amount: float = 1_000_000,
    features: list = None,
    time_window: int = 5,
    commission_fee_pct: float = 0.001,
    commission_fee_model: str = "trf",
    normalize_df: str = "by_previous_time",
    reward_scaling: float = 1.0,
    algorithm: str = "ppo",
    total_timesteps: int = 50_000,
    policy: str = "MlpPolicy",
    save_path: str = None,
    verbose: int = 1,
) -> dict:
    """Train a DRL agent for advanced portfolio optimization."""
    if features is None:
        features = ["close", "high", "low"]

    # Create environment
    env = create_portfolio_optimization_env(
        df=df,
        initial_amount=initial_amount,
        features=features,
        time_window=time_window,
        commission_fee_pct=commission_fee_pct,
        commission_fee_model=commission_fee_model,
        normalize_df=normalize_df,
        reward_scaling=reward_scaling,
    )

    # Wrap for SB3
    env_train, _ = env.get_sb_env()

    # Train
    agent = DRLAgent(env=env_train)
    from finrl.agents.stablebaselines3.models import MODEL_KWARGS
    model_kwargs = MODEL_KWARGS.get(algorithm.lower(), {})

    model = agent.get_model(
        model_name=algorithm.lower(),
        policy=policy,
        model_kwargs=model_kwargs,
        verbose=verbose,
    )
    trained_model = agent.train_model(
        model=model,
        tb_log_name=f"portfolio_opt_{algorithm}",
        total_timesteps=total_timesteps,
    )

    # Save
    if save_path is None:
        os.makedirs(TRAINED_MODEL_DIR, exist_ok=True)
        save_path = os.path.join(TRAINED_MODEL_DIR, f"portfolio_opt_{algorithm}")
    trained_model.save(save_path)

    stock_dim = df["tic"].nunique() if "tic" in df.columns else 1

    return {
        "status": "success",
        "mode": "optimization",
        "algorithm": algorithm,
        "stock_dim": stock_dim,
        "time_window": time_window,
        "features": features,
        "commission_model": commission_fee_model,
        "total_timesteps": total_timesteps,
        "model_saved_to": save_path,
    }


# ============================================================================
# Backtesting
# ============================================================================

def backtest_portfolio(
    model_path: str,
    df: pd.DataFrame,
    mode: str = "allocation",
    algorithm: str = "ppo",
    stock_dim: int = None,
    initial_amount: int = 1_000_000,
    tech_indicator_list: list = None,
    features: list = None,
    time_window: int = 5,
    lookback: int = 252,
    deterministic: bool = True,
    save_results: bool = True,
    results_dir: str = None,
    tickers: list = None,
) -> dict:
    """Backtest a trained portfolio agent."""
    if results_dir is None:
        results_dir = RESULTS_DIR
    if stock_dim is None:
        stock_dim = df["tic"].nunique() if "tic" in df.columns else 1

    if mode == "allocation":
        if tech_indicator_list is None:
            tech_indicator_list = list(INDICATORS)
        env = create_portfolio_env(
            df=df, stock_dim=stock_dim,
            initial_amount=initial_amount,
            tech_indicator_list=tech_indicator_list,
            lookback=lookback,
        )
    else:
        if features is None:
            features = ["close", "high", "low"]
        env = create_portfolio_optimization_env(
            df=df, initial_amount=float(initial_amount),
            features=features, time_window=time_window,
        )

    # Load model
    from stable_baselines3 import A2C, PPO, DDPG, SAC, TD3
    MODEL_MAP = {"a2c": A2C, "ppo": PPO, "ddpg": DDPG, "sac": SAC, "td3": TD3}
    model_class = MODEL_MAP.get(algorithm.lower(), PPO)
    model = model_class.load(model_path)

    # Run backtest
    obs = env.reset()
    account_values = [initial_amount]
    all_weights = []

    done = False
    while not done:
        action, _states = model.predict(obs, deterministic=deterministic)
        obs, reward, done, info = env.step(action)

        # Collect weights (softmax of actions)
        weights = np.exp(action) / np.sum(np.exp(action))
        all_weights.append(weights.tolist() if hasattr(weights, 'tolist') else list(weights))

    # Get account memory
    if hasattr(env, 'save_asset_memory'):
        df_account = env.save_asset_memory()
        if isinstance(df_account, pd.DataFrame) and "account_value" in df_account.columns:
            account_values = df_account["account_value"].values.tolist()

    # Metrics
    values = np.array(account_values, dtype=float)
    total_return = (values[-1] / values[0] - 1) * 100 if len(values) > 1 else 0
    daily_returns = np.diff(values) / values[:-1] if len(values) > 1 else np.array([0])
    sharpe = np.mean(daily_returns) / (np.std(daily_returns) + 1e-8) * np.sqrt(252)
    max_dd = np.min(values / np.maximum.accumulate(values) - 1) * 100 if len(values) > 1 else 0

    result = {
        "status": "success",
        "mode": mode,
        "algorithm": algorithm,
        "stock_dim": stock_dim,
        "initial_value": float(values[0]),
        "final_value": float(values[-1]),
        "total_return_pct": round(float(total_return), 4),
        "sharpe_ratio": round(float(sharpe), 4),
        "max_drawdown_pct": round(float(max_dd), 4),
        "num_rebalances": len(all_weights),
    }

    # Weight analysis
    if all_weights:
        weights_arr = np.array(all_weights)
        avg_weights = np.mean(weights_arr, axis=0)
        ticker_names = tickers[:stock_dim] if tickers else [f"Stock_{i}" for i in range(stock_dim)]
        result["average_weights"] = {
            ticker_names[i]: round(float(avg_weights[i]), 4)
            for i in range(min(len(avg_weights), len(ticker_names)))
        }
        result["weight_turnover"] = round(float(np.mean(np.sum(np.abs(np.diff(weights_arr, axis=0)), axis=1))), 4) if len(weights_arr) > 1 else 0

    # Save
    if save_results:
        os.makedirs(results_dir, exist_ok=True)
        if all_weights:
            pd.DataFrame(all_weights, columns=tickers[:stock_dim] if tickers else None).to_csv(
                os.path.join(results_dir, f"portfolio_weights_{algorithm}.csv"), index=False)
        result["results_saved_to"] = results_dir

    return result


# ============================================================================
# Weight Analysis
# ============================================================================

def analyze_portfolio_weights(weights_df: pd.DataFrame, tickers: list = None) -> dict:
    """Analyze portfolio weight history."""
    weights = weights_df.values
    num_periods, num_assets = weights.shape

    if tickers is None:
        tickers = list(weights_df.columns) if all(not str(c).isdigit() for c in weights_df.columns) else [f"Asset_{i}" for i in range(num_assets)]

    avg_weights = np.mean(weights, axis=0)
    std_weights = np.std(weights, axis=0)
    min_weights = np.min(weights, axis=0)
    max_weights = np.max(weights, axis=0)

    # Concentration: Herfindahl index
    hhi = np.mean([np.sum(w ** 2) for w in weights])

    # Turnover
    turnover = np.mean(np.sum(np.abs(np.diff(weights, axis=0)), axis=1)) if num_periods > 1 else 0

    per_asset = []
    for i in range(num_assets):
        per_asset.append({
            "ticker": tickers[i] if i < len(tickers) else f"Asset_{i}",
            "avg_weight_pct": round(float(avg_weights[i]) * 100, 2),
            "std_weight_pct": round(float(std_weights[i]) * 100, 2),
            "min_weight_pct": round(float(min_weights[i]) * 100, 2),
            "max_weight_pct": round(float(max_weights[i]) * 100, 2),
        })

    return {
        "num_periods": num_periods,
        "num_assets": num_assets,
        "avg_concentration_hhi": round(float(hhi), 4),
        "effective_num_assets": round(1.0 / (hhi + 1e-8), 2),
        "avg_turnover": round(float(turnover), 4),
        "per_asset_stats": per_asset,
    }


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="FinRL Portfolio Optimization")
    parser.add_argument("--action", required=True,
                        choices=["list_modes", "train_allocation", "train_optimization",
                                 "backtest", "analyze_weights"])
    parser.add_argument("--data_file", help="CSV data file")
    parser.add_argument("--tickers", nargs="*")
    parser.add_argument("--algorithm", default="ppo")
    parser.add_argument("--timesteps", type=int, default=50_000)
    parser.add_argument("--initial_amount", type=float, default=1_000_000)
    parser.add_argument("--lookback", type=int, default=252)
    parser.add_argument("--time_window", type=int, default=5)
    parser.add_argument("--features", nargs="*", default=["close", "high", "low"])
    parser.add_argument("--commission_model", default="trf", choices=["trf", "wvm"])
    parser.add_argument("--commission_pct", type=float, default=0.001)
    parser.add_argument("--model_path", help="Trained model path")
    parser.add_argument("--mode", default="allocation", choices=["allocation", "optimization"])
    parser.add_argument("--weights_file", help="CSV with portfolio weights")
    parser.add_argument("--save_path")
    parser.add_argument("--results_dir", default=RESULTS_DIR)
    parser.add_argument("--indicators", nargs="*")

    args = parser.parse_args()

    if args.action == "list_modes":
        result = PORTFOLIO_MODES

    elif args.action == "train_allocation":
        if not args.data_file or not os.path.exists(args.data_file):
            result = {"error": "Provide valid --data_file"}
        else:
            df = pd.read_csv(args.data_file)
            result = train_portfolio_allocation(
                df=df, algorithm=args.algorithm,
                total_timesteps=args.timesteps,
                initial_amount=int(args.initial_amount),
                lookback=args.lookback,
                tech_indicator_list=args.indicators,
                save_path=args.save_path,
            )

    elif args.action == "train_optimization":
        if not args.data_file or not os.path.exists(args.data_file):
            result = {"error": "Provide valid --data_file"}
        else:
            df = pd.read_csv(args.data_file)
            result = train_portfolio_optimization(
                df=df, algorithm=args.algorithm,
                total_timesteps=args.timesteps,
                initial_amount=args.initial_amount,
                features=args.features,
                time_window=args.time_window,
                commission_fee_pct=args.commission_pct,
                commission_fee_model=args.commission_model,
                save_path=args.save_path,
            )

    elif args.action == "backtest":
        if not args.model_path or not args.data_file:
            result = {"error": "Provide --model_path and --data_file"}
        elif not os.path.exists(args.data_file):
            result = {"error": f"Data file not found: {args.data_file}"}
        else:
            df = pd.read_csv(args.data_file)
            result = backtest_portfolio(
                model_path=args.model_path,
                df=df, mode=args.mode,
                algorithm=args.algorithm,
                initial_amount=int(args.initial_amount),
                tech_indicator_list=args.indicators,
                features=args.features,
                time_window=args.time_window,
                lookback=args.lookback,
                tickers=args.tickers,
                results_dir=args.results_dir,
            )

    elif args.action == "analyze_weights":
        if not args.weights_file or not os.path.exists(args.weights_file):
            result = {"error": "Provide valid --weights_file"}
        else:
            weights_df = pd.read_csv(args.weights_file)
            result = analyze_portfolio_weights(weights_df, args.tickers)

    else:
        result = {"error": f"Unknown action: {args.action}"}

    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()
