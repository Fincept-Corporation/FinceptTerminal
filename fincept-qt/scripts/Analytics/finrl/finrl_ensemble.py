"""
FinRL Ensemble Strategies
Wrapper around finrl.agents.stablebaselines3.models.DRLEnsembleAgent
and finrl.applications.stock_trading.ensemble_stock_trading.

Covers:
- Ensemble strategy training (A2C + PPO + DDPG + SAC + TD3)
- Validation-window based model selection
- Rolling-window ensemble with rebalancing
- Turbulence-based strategy switching
- Sharpe-ratio-based algorithm selection

Usage (CLI):
    python finrl_ensemble.py --action list_strategies
    python finrl_ensemble.py --action train --data_file data.csv --tickers AAPL MSFT GOOGL --rebalance_window 63 --validation_window 63
    python finrl_ensemble.py --action backtest --data_file test.csv --model_dir trained_models --tickers AAPL MSFT GOOGL
    python finrl_ensemble.py --action compare_single_vs_ensemble --data_file test.csv --model_dir trained_models --tickers AAPL MSFT
"""

import json
import sys
import os
import argparse
import numpy as np
import pandas as pd

from finrl.agents.stablebaselines3.models import DRLEnsembleAgent, DRLAgent
from finrl.config import (
    INDICATORS, TRAINED_MODEL_DIR, RESULTS_DIR,
    A2C_PARAMS, PPO_PARAMS, DDPG_PARAMS, SAC_PARAMS, TD3_PARAMS,
)
from finrl.meta.preprocessor.preprocessors import data_split


# ============================================================================
# Ensemble Strategy Registry
# ============================================================================

ENSEMBLE_STRATEGIES = {
    "validation_sharpe": {
        "description": "Select best algorithm per rebalance window based on validation Sharpe ratio",
        "algorithms_used": ["A2C", "PPO", "DDPG", "SAC", "TD3"],
        "method": "Train all 5 algorithms, validate on recent window, pick best Sharpe",
        "advantages": [
            "Adapts to changing market conditions",
            "Automatically selects best-performing algorithm",
            "Built-in validation prevents overfitting",
        ],
    },
    "turbulence_switching": {
        "description": "Switch between aggressive/defensive algorithms based on turbulence",
        "algorithms_used": ["PPO (normal)", "A2C (high turbulence)"],
        "method": "Use turbulence index to toggle between algorithms",
        "advantages": [
            "Risk-aware strategy switching",
            "Protects against market crashes",
        ],
    },
    "weighted_average": {
        "description": "Weighted average of all algorithm predictions",
        "algorithms_used": ["A2C", "PPO", "DDPG", "SAC", "TD3"],
        "method": "Combine actions from all algorithms with Sharpe-ratio weights",
        "advantages": [
            "Diversification of strategies",
            "Smoothed trading signals",
        ],
    },
}


# ============================================================================
# Ensemble Training
# ============================================================================

def train_ensemble(
    df: pd.DataFrame,
    train_start: str,
    train_end: str,
    test_start: str,
    test_end: str,
    tickers: list = None,
    hmax: int = 100,
    initial_amount: float = 1_000_000,
    buy_cost_pct: float = 0.001,
    sell_cost_pct: float = 0.001,
    reward_scaling: float = 1e-4,
    tech_indicator_list: list = None,
    rebalance_window: int = 63,
    validation_window: int = 63,
    a2c_params: dict = None,
    ppo_params: dict = None,
    ddpg_params: dict = None,
    sac_params: dict = None,
    td3_params: dict = None,
    timesteps_dict: dict = None,
    print_verbosity: int = 10,
) -> dict:
    """
    Train an ensemble of DRL agents with rolling validation.

    Uses DRLEnsembleAgent which trains A2C, PPO, DDPG, SAC, TD3 and selects
    the best per rebalance window using validation Sharpe ratio.
    """
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)
    if tickers is None:
        tickers = df["tic"].unique().tolist() if "tic" in df.columns else ["AAPL"]

    stock_dim = len(tickers)
    state_space = 1 + 2 * stock_dim + len(tech_indicator_list) * stock_dim
    action_space = stock_dim

    if a2c_params is None: a2c_params = A2C_PARAMS
    if ppo_params is None: ppo_params = PPO_PARAMS
    if ddpg_params is None: ddpg_params = DDPG_PARAMS
    if sac_params is None: sac_params = SAC_PARAMS
    if td3_params is None: td3_params = TD3_PARAMS

    if timesteps_dict is None:
        timesteps_dict = {
            "a2c": 10_000,
            "ppo": 10_000,
            "ddpg": 10_000,
            "sac": 10_000,
            "td3": 10_000,
        }

    # Build train/test periods
    train_period = (train_start, train_end)
    val_test_period = (test_start, test_end)

    try:
        ensemble_agent = DRLEnsembleAgent(
            df=df,
            train_period=train_period,
            val_test_period=val_test_period,
            rebalance_window=rebalance_window,
            validation_window=validation_window,
            stock_dim=stock_dim,
            hmax=hmax,
            initial_amount=initial_amount,
            buy_cost_pct=[buy_cost_pct] * stock_dim,
            sell_cost_pct=[sell_cost_pct] * stock_dim,
            reward_scaling=reward_scaling,
            state_space=state_space,
            action_space=action_space,
            tech_indicator_list=tech_indicator_list,
            print_verbosity=print_verbosity,
        )

        df_summary = ensemble_agent.run_ensemble_strategy(
            A2C_model_kwargs=a2c_params,
            PPO_model_kwargs=ppo_params,
            DDPG_model_kwargs=ddpg_params,
            SAC_model_kwargs=sac_params,
            TD3_model_kwargs=td3_params,
            timesteps_dict=timesteps_dict,
        )

        # Save results
        os.makedirs(RESULTS_DIR, exist_ok=True)
        if isinstance(df_summary, pd.DataFrame):
            df_summary.to_csv(os.path.join(RESULTS_DIR, "ensemble_results.csv"), index=False)

        return {
            "status": "success",
            "strategy": "validation_sharpe",
            "rebalance_window": rebalance_window,
            "validation_window": validation_window,
            "algorithms": ["a2c", "ppo", "ddpg", "sac", "td3"],
            "stock_dim": stock_dim,
            "train_period": train_period,
            "test_period": val_test_period,
            "timesteps_per_algo": timesteps_dict,
            "results_saved_to": RESULTS_DIR,
        }

    except Exception as e:
        return {"error": str(e), "status": "failed"}


# ============================================================================
# Turbulence-Based Switching
# ============================================================================

def turbulence_ensemble(
    df_test: pd.DataFrame,
    models: dict,
    stock_dim: int,
    hmax: int = 100,
    initial_amount: int = 1_000_000,
    tech_indicator_list: list = None,
    turbulence_threshold: float = 150.0,
    normal_algo: str = "ppo",
    defensive_algo: str = "a2c",
) -> dict:
    """
    Ensemble strategy that switches algorithms based on turbulence.
    Uses aggressive algo in normal conditions, defensive in high turbulence.
    """
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)

    from finrl_environments import create_stock_trading_env
    from stable_baselines3 import A2C, PPO, DDPG, SAC, TD3

    MODEL_MAP = {"a2c": A2C, "ppo": PPO, "ddpg": DDPG, "sac": SAC, "td3": TD3}

    # Load models
    loaded_models = {}
    for algo, path in models.items():
        model_class = MODEL_MAP.get(algo.lower(), PPO)
        loaded_models[algo] = model_class.load(path)

    # Create environment
    env = create_stock_trading_env(
        df=df_test, stock_dim=stock_dim, hmax=hmax,
        initial_amount=initial_amount,
        tech_indicator_list=tech_indicator_list,
    )

    obs, _ = env.reset()
    account_values = [initial_amount]
    algo_used = []
    done = False

    while not done:
        # Check turbulence
        turbulence = 0
        if hasattr(env, 'turbulence'):
            turbulence = env.turbulence
        elif "turbulence" in df_test.columns:
            day = min(env.day if hasattr(env, 'day') else 0, len(df_test) - 1)
            turbulence = df_test.iloc[day].get("turbulence", 0)

        # Select algorithm
        if turbulence > turbulence_threshold:
            current_algo = defensive_algo
        else:
            current_algo = normal_algo

        algo_used.append(current_algo)

        # Get action
        model = loaded_models.get(current_algo, loaded_models.get(normal_algo))
        action, _ = model.predict(obs, deterministic=True)
        obs, reward, done, truncated, info = env.step(action)

    # Get final account values
    if hasattr(env, 'save_asset_memory'):
        df_account = env.save_asset_memory()
        if isinstance(df_account, pd.DataFrame) and "account_value" in df_account.columns:
            account_values = df_account["account_value"].tolist()

    values = np.array(account_values, dtype=float)
    total_return = (values[-1] / values[0] - 1) * 100 if len(values) > 1 else 0

    from collections import Counter
    algo_counts = Counter(algo_used)

    return {
        "status": "success",
        "strategy": "turbulence_switching",
        "turbulence_threshold": turbulence_threshold,
        "normal_algo": normal_algo,
        "defensive_algo": defensive_algo,
        "initial_value": float(values[0]),
        "final_value": float(values[-1]),
        "total_return_pct": round(float(total_return), 4),
        "algo_usage": dict(algo_counts),
        "algo_switches": sum(1 for i in range(1, len(algo_used)) if algo_used[i] != algo_used[i - 1]),
        "total_steps": len(algo_used),
    }


# ============================================================================
# Weighted Average Ensemble
# ============================================================================

def weighted_average_actions(
    models: dict,
    obs: np.ndarray,
    weights: dict = None,
    deterministic: bool = True,
) -> np.ndarray:
    """Compute weighted average of actions from multiple models."""
    if weights is None:
        weights = {k: 1.0 / len(models) for k in models}

    total_weight = sum(weights.values())
    weighted_action = None

    for algo_name, model in models.items():
        action, _ = model.predict(obs, deterministic=deterministic)
        w = weights.get(algo_name, 0) / total_weight
        if weighted_action is None:
            weighted_action = action * w
        else:
            weighted_action += action * w

    return weighted_action


# ============================================================================
# Comparison: Single vs Ensemble
# ============================================================================

def compare_single_vs_ensemble(
    df_test: pd.DataFrame,
    model_dir: str,
    algorithms: list = None,
    stock_dim: int = None,
    initial_amount: int = 1_000_000,
    tech_indicator_list: list = None,
    tickers: list = None,
) -> dict:
    """Compare individual algorithms against ensemble approaches."""
    if algorithms is None:
        algorithms = ["a2c", "ppo", "sac"]
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)
    if stock_dim is None:
        stock_dim = df_test["tic"].nunique() if "tic" in df_test.columns else 1

    from finrl_backtest import backtest_agent

    results = {}
    for algo in algorithms:
        # Find model
        for ext in [".zip", "", "_agent.zip", "_agent"]:
            path = os.path.join(model_dir, f"{algo}{ext}")
            if os.path.exists(path):
                try:
                    metrics = backtest_agent(
                        model_path=path, df=df_test, algorithm=algo,
                        stock_dim=stock_dim, initial_amount=initial_amount,
                        tech_indicator_list=tech_indicator_list,
                        save_results=False, tickers=tickers,
                    )
                    results[algo] = metrics
                except Exception as e:
                    results[algo] = {"error": str(e)}
                break

    # Find best single
    valid = {k: v for k, v in results.items() if "sharpe_ratio" in v}
    if valid:
        best_single = max(valid.items(), key=lambda x: x[1]["sharpe_ratio"])
        results["_comparison"] = {
            "best_single_algorithm": best_single[0],
            "best_single_sharpe": best_single[1]["sharpe_ratio"],
            "best_single_return": best_single[1]["total_return_pct"],
            "num_algorithms_tested": len(algorithms),
        }

    return results


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="FinRL Ensemble Strategies")
    parser.add_argument("--action", required=True,
                        choices=["list_strategies", "train", "backtest",
                                 "compare_single_vs_ensemble"])
    parser.add_argument("--data_file", help="CSV data file")
    parser.add_argument("--tickers", nargs="*")
    parser.add_argument("--algorithms", nargs="*", default=["a2c", "ppo", "sac"])
    parser.add_argument("--train_start", default="2018-01-01")
    parser.add_argument("--train_end", default="2022-06-30")
    parser.add_argument("--test_start", default="2022-07-01")
    parser.add_argument("--test_end", default="2023-12-31")
    parser.add_argument("--rebalance_window", type=int, default=63)
    parser.add_argument("--validation_window", type=int, default=63)
    parser.add_argument("--initial_amount", type=float, default=1_000_000)
    parser.add_argument("--hmax", type=int, default=100)
    parser.add_argument("--timesteps", type=int, default=10_000)
    parser.add_argument("--model_dir", default=TRAINED_MODEL_DIR)
    parser.add_argument("--indicators", nargs="*")

    args = parser.parse_args()

    if args.action == "list_strategies":
        result = ENSEMBLE_STRATEGIES

    elif args.action == "train":
        if not args.data_file or not os.path.exists(args.data_file):
            result = {"error": "Provide valid --data_file"}
        else:
            df = pd.read_csv(args.data_file)
            ts = args.timesteps
            result = train_ensemble(
                df=df,
                train_start=args.train_start,
                train_end=args.train_end,
                test_start=args.test_start,
                test_end=args.test_end,
                tickers=args.tickers,
                hmax=args.hmax,
                initial_amount=args.initial_amount,
                tech_indicator_list=args.indicators,
                rebalance_window=args.rebalance_window,
                validation_window=args.validation_window,
                timesteps_dict={a: ts for a in ["a2c", "ppo", "ddpg", "sac", "td3"]},
            )

    elif args.action == "compare_single_vs_ensemble":
        if not args.data_file or not os.path.exists(args.data_file):
            result = {"error": "Provide valid --data_file"}
        else:
            df = pd.read_csv(args.data_file)
            result = compare_single_vs_ensemble(
                df_test=df,
                model_dir=args.model_dir,
                algorithms=args.algorithms,
                initial_amount=int(args.initial_amount),
                tech_indicator_list=args.indicators,
                tickers=args.tickers,
            )

    else:
        result = {"error": f"Unknown action: {args.action}"}

    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()
