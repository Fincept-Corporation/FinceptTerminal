"""
FinRL Agent Training & Model Management
Wrapper around finrl.agents.stablebaselines3, finrl.train, and model management.

Covers:
- DRLAgent: Single-agent training (A2C, PPO, DDPG, SAC, TD3)
- DRLEnsembleAgent: Ensemble training with validation-based model selection
- Model saving/loading
- Hyperparameter configuration
- TensorBoard logging integration
- Training from the high-level finrl.train API

Usage (CLI):
    python finrl_agents.py --action list_algorithms
    python finrl_agents.py --action train --algorithm ppo --data_file train.csv --tickers AAPL MSFT --timesteps 50000
    python finrl_agents.py --action train --algorithm a2c --data_file train.csv --tickers AAPL MSFT --timesteps 100000 --tensorboard
    python finrl_agents.py --action train_highlevel --algorithm ppo --tickers AAPL MSFT --train_start 2018-01-01 --train_end 2022-12-31 --source yahoofinance
    python finrl_agents.py --action load_predict --algorithm ppo --model_path trained_models/ppo.zip --data_file test.csv --tickers AAPL MSFT
"""

import json
import sys
import os
import argparse
import numpy as np
import pandas as pd

from finrl.agents.stablebaselines3.models import DRLAgent, DRLEnsembleAgent, MODELS, MODEL_KWARGS
from finrl.config import (
    INDICATORS, TRAINED_MODEL_DIR, TENSORBOARD_LOG_DIR,
    A2C_PARAMS, PPO_PARAMS, DDPG_PARAMS, SAC_PARAMS, TD3_PARAMS,
)
from finrl.train import train as finrl_train

from stable_baselines3 import A2C, PPO, DDPG, SAC, TD3
from stable_baselines3.common.vec_env import DummyVecEnv

from finrl_environments import create_stock_trading_env


# ============================================================================
# Algorithm Registry
# ============================================================================

ALGORITHM_INFO = {
    "a2c": {
        "name": "Advantage Actor-Critic (A2C)",
        "description": "Synchronous version of A3C. Good baseline, fast training.",
        "best_for": "Simple strategies, quick prototyping",
        "default_params": A2C_PARAMS,
        "sb3_class": "A2C",
    },
    "ppo": {
        "name": "Proximal Policy Optimization (PPO)",
        "description": "Most stable and widely used algorithm. Clips policy updates.",
        "best_for": "General purpose, most reliable choice",
        "default_params": PPO_PARAMS,
        "sb3_class": "PPO",
    },
    "ddpg": {
        "name": "Deep Deterministic Policy Gradient (DDPG)",
        "description": "Off-policy, continuous action spaces. Uses replay buffer.",
        "best_for": "Continuous action spaces, deterministic policies",
        "default_params": DDPG_PARAMS,
        "sb3_class": "DDPG",
    },
    "sac": {
        "name": "Soft Actor-Critic (SAC)",
        "description": "Entropy-regularized off-policy. Explores efficiently.",
        "best_for": "Complex strategies needing exploration, robust performance",
        "default_params": SAC_PARAMS,
        "sb3_class": "SAC",
    },
    "td3": {
        "name": "Twin Delayed DDPG (TD3)",
        "description": "Improved DDPG with twin critics and delayed updates.",
        "best_for": "When DDPG is unstable, continuous actions",
        "default_params": TD3_PARAMS,
        "sb3_class": "TD3",
    },
}


# ============================================================================
# Training Functions
# ============================================================================

def train_agent(
    df: pd.DataFrame,
    algorithm: str = "ppo",
    stock_dim: int = None,
    hmax: int = 100,
    initial_amount: int = 1_000_000,
    buy_cost_pct: float = 0.001,
    sell_cost_pct: float = 0.001,
    reward_scaling: float = 1e-4,
    tech_indicator_list: list = None,
    total_timesteps: int = 50_000,
    policy: str = "MlpPolicy",
    model_kwargs: dict = None,
    policy_kwargs: dict = None,
    verbose: int = 1,
    seed: int = None,
    tensorboard_log: str = None,
    save_path: str = None,
    turbulence_threshold: float = None,
) -> dict:
    """
    Train a single DRL agent on stock trading environment.

    Returns dict with model info and training results.
    """
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)

    if stock_dim is None:
        if "tic" in df.columns:
            stock_dim = df["tic"].nunique()
        else:
            stock_dim = 1

    # Create environment
    env = create_stock_trading_env(
        df=df,
        stock_dim=stock_dim,
        hmax=hmax,
        initial_amount=initial_amount,
        buy_cost_pct=[buy_cost_pct] * stock_dim,
        sell_cost_pct=[sell_cost_pct] * stock_dim,
        reward_scaling=reward_scaling,
        tech_indicator_list=tech_indicator_list,
        turbulence_threshold=turbulence_threshold,
    )

    # Create agent
    agent = DRLAgent(env=env)

    # Get model with hyperparameters
    if model_kwargs is None:
        algo_key = algorithm.lower()
        if algo_key in MODEL_KWARGS:
            model_kwargs = MODEL_KWARGS[algo_key]
        else:
            model_kwargs = {}

    tb_log = tensorboard_log or TENSORBOARD_LOG_DIR if tensorboard_log is not None else None

    model = agent.get_model(
        model_name=algorithm.lower(),
        policy=policy,
        policy_kwargs=policy_kwargs,
        model_kwargs=model_kwargs,
        verbose=verbose,
        seed=seed,
        tensorboard_log=tb_log,
    )

    # Train model
    trained_model = agent.train_model(
        model=model,
        tb_log_name=algorithm.lower(),
        total_timesteps=total_timesteps,
    )

    # Save model
    if save_path is None:
        os.makedirs(TRAINED_MODEL_DIR, exist_ok=True)
        save_path = os.path.join(TRAINED_MODEL_DIR, f"{algorithm.lower()}_agent")

    trained_model.save(save_path)

    return {
        "status": "success",
        "algorithm": algorithm,
        "total_timesteps": total_timesteps,
        "stock_dim": stock_dim,
        "state_space": 1 + 2 * stock_dim + len(tech_indicator_list) * stock_dim,
        "action_space": stock_dim,
        "model_saved_to": save_path,
        "policy": policy,
        "model_kwargs": {k: str(v) for k, v in (model_kwargs or {}).items()},
    }


def train_highlevel(
    algorithm: str = "ppo",
    tickers: list = None,
    start_date: str = "2018-01-01",
    end_date: str = "2022-12-31",
    data_source: str = "yahoofinance",
    time_interval: str = "1D",
    indicators: list = None,
    drl_lib: str = "stable_baselines3",
    if_vix: bool = True,
    **kwargs,
) -> dict:
    """
    Train using FinRL's high-level train() API.
    Handles data download, preprocessing, and training in one call.
    """
    if tickers is None:
        tickers = ["AAPL", "MSFT", "GOOGL"]
    if indicators is None:
        indicators = list(INDICATORS)

    from finrl.meta.env_stock_trading.env_stocktrading import StockTradingEnv

    finrl_train(
        start_date=start_date,
        end_date=end_date,
        ticker_list=tickers,
        data_source=data_source,
        time_interval=time_interval,
        technical_indicator_list=indicators,
        drl_lib=drl_lib,
        env=StockTradingEnv,
        model_name=algorithm.lower(),
        if_vix=if_vix,
        **kwargs,
    )

    return {
        "status": "success",
        "algorithm": algorithm,
        "tickers": tickers,
        "date_range": f"{start_date} to {end_date}",
        "data_source": data_source,
        "drl_lib": drl_lib,
    }


# ============================================================================
# Prediction / Inference Functions
# ============================================================================

def predict_with_model(
    model_path: str,
    df: pd.DataFrame,
    algorithm: str = "ppo",
    stock_dim: int = None,
    hmax: int = 100,
    initial_amount: int = 1_000_000,
    buy_cost_pct: float = 0.001,
    sell_cost_pct: float = 0.001,
    reward_scaling: float = 1e-4,
    tech_indicator_list: list = None,
    deterministic: bool = True,
) -> dict:
    """Load a trained model and run predictions on test data."""
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)

    if stock_dim is None:
        if "tic" in df.columns:
            stock_dim = df["tic"].nunique()
        else:
            stock_dim = 1

    # Create test environment
    env = create_stock_trading_env(
        df=df,
        stock_dim=stock_dim,
        hmax=hmax,
        initial_amount=initial_amount,
        buy_cost_pct=[buy_cost_pct] * stock_dim,
        sell_cost_pct=[sell_cost_pct] * stock_dim,
        reward_scaling=reward_scaling,
        tech_indicator_list=tech_indicator_list,
        make_plots=False,
        initial=True,
    )

    # Load and predict
    account_value, actions = DRLAgent.DRL_prediction_load_from_file(
        model_name=algorithm.lower(),
        environment=env,
        cwd=os.path.dirname(model_path),
        deterministic=deterministic,
    )

    # Calculate metrics
    if isinstance(account_value, pd.DataFrame) and "account_value" in account_value.columns:
        values = account_value["account_value"].values
    elif isinstance(account_value, list):
        values = np.array(account_value)
    else:
        values = np.array([initial_amount])

    total_return = (values[-1] / values[0] - 1) * 100 if len(values) > 1 else 0
    daily_returns = np.diff(values) / values[:-1] if len(values) > 1 else np.array([0])
    sharpe = np.mean(daily_returns) / (np.std(daily_returns) + 1e-8) * np.sqrt(252)
    max_dd = np.min(values / np.maximum.accumulate(values) - 1) * 100 if len(values) > 1 else 0

    return {
        "status": "success",
        "algorithm": algorithm,
        "model_path": model_path,
        "initial_value": float(values[0]),
        "final_value": float(values[-1]),
        "total_return_pct": float(total_return),
        "sharpe_ratio": float(sharpe),
        "max_drawdown_pct": float(max_dd),
        "total_steps": len(values),
        "num_trades": int(np.sum(np.abs(np.array(actions)) > 0)) if actions is not None else 0,
    }


# ============================================================================
# Model Management
# ============================================================================

def list_saved_models(model_dir: str = None) -> dict:
    """List all saved models in the trained models directory."""
    if model_dir is None:
        model_dir = TRAINED_MODEL_DIR

    if not os.path.exists(model_dir):
        return {"models": [], "directory": model_dir, "count": 0}

    models = []
    for f in os.listdir(model_dir):
        fpath = os.path.join(model_dir, f)
        if os.path.isfile(fpath) and (f.endswith(".zip") or f.endswith(".pth") or f.endswith(".pt")):
            models.append({
                "filename": f,
                "path": fpath,
                "size_mb": round(os.path.getsize(fpath) / 1024 / 1024, 2),
                "algorithm": f.split("_")[0] if "_" in f else f.split(".")[0],
            })

    return {"models": models, "directory": model_dir, "count": len(models)}


def get_model_summary(algorithm: str) -> dict:
    """Get summary info about an algorithm."""
    key = algorithm.lower()
    if key not in ALGORITHM_INFO:
        return {"error": f"Unknown algorithm '{algorithm}'. Available: {list(ALGORITHM_INFO.keys())}"}
    return ALGORITHM_INFO[key]


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="FinRL Agent Training Manager")
    parser.add_argument("--action", required=True,
                        choices=["list_algorithms", "train", "train_highlevel",
                                 "load_predict", "list_models", "algorithm_info"])
    parser.add_argument("--algorithm", default="ppo",
                        choices=["a2c", "ppo", "ddpg", "sac", "td3"])
    parser.add_argument("--data_file", help="CSV training data file")
    parser.add_argument("--tickers", nargs="*", default=["AAPL", "MSFT", "GOOGL"])
    parser.add_argument("--stock_dim", type=int)
    parser.add_argument("--timesteps", type=int, default=50_000)
    parser.add_argument("--initial_amount", type=float, default=1_000_000)
    parser.add_argument("--hmax", type=int, default=100)
    parser.add_argument("--buy_cost", type=float, default=0.001)
    parser.add_argument("--sell_cost", type=float, default=0.001)
    parser.add_argument("--reward_scaling", type=float, default=1e-4)
    parser.add_argument("--policy", default="MlpPolicy")
    parser.add_argument("--seed", type=int)
    parser.add_argument("--tensorboard", action="store_true")
    parser.add_argument("--save_path")
    parser.add_argument("--model_path", help="Path to saved model for prediction")
    parser.add_argument("--model_dir", default=TRAINED_MODEL_DIR)
    parser.add_argument("--train_start", default="2018-01-01")
    parser.add_argument("--train_end", default="2022-12-31")
    parser.add_argument("--source", default="yahoofinance")
    parser.add_argument("--indicators", nargs="*")

    args = parser.parse_args()

    if args.action == "list_algorithms":
        result = {name: {k: v for k, v in info.items() if k != "default_params"}
                  for name, info in ALGORITHM_INFO.items()}
        result["_note"] = "Use --action algorithm_info --algorithm <name> for full details with params"

    elif args.action == "algorithm_info":
        result = get_model_summary(args.algorithm)

    elif args.action == "list_models":
        result = list_saved_models(args.model_dir)

    elif args.action == "train":
        if not args.data_file or not os.path.exists(args.data_file):
            result = {"error": "Provide valid --data_file CSV path for training data"}
            print(json.dumps(result, indent=2))
            return

        df = pd.read_csv(args.data_file)
        tb_log = TENSORBOARD_LOG_DIR if args.tensorboard else None

        result = train_agent(
            df=df,
            algorithm=args.algorithm,
            stock_dim=args.stock_dim,
            hmax=args.hmax,
            initial_amount=int(args.initial_amount),
            buy_cost_pct=args.buy_cost,
            sell_cost_pct=args.sell_cost,
            reward_scaling=args.reward_scaling,
            tech_indicator_list=args.indicators,
            total_timesteps=args.timesteps,
            policy=args.policy,
            seed=args.seed,
            tensorboard_log=tb_log,
            save_path=args.save_path,
        )

    elif args.action == "train_highlevel":
        result = train_highlevel(
            algorithm=args.algorithm,
            tickers=args.tickers,
            start_date=args.train_start,
            end_date=args.train_end,
            data_source=args.source,
            indicators=args.indicators,
        )

    elif args.action == "load_predict":
        if not args.model_path:
            result = {"error": "Provide --model_path to saved model"}
            print(json.dumps(result, indent=2))
            return
        if not args.data_file or not os.path.exists(args.data_file):
            result = {"error": "Provide valid --data_file CSV for test data"}
            print(json.dumps(result, indent=2))
            return

        df = pd.read_csv(args.data_file)
        result = predict_with_model(
            model_path=args.model_path,
            df=df,
            algorithm=args.algorithm,
            stock_dim=args.stock_dim,
            hmax=args.hmax,
            initial_amount=int(args.initial_amount),
            tech_indicator_list=args.indicators,
        )

    else:
        result = {"error": f"Unknown action: {args.action}"}

    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()
