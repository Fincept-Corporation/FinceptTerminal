"""
FinRL Backtesting & Evaluation
Wrapper around finrl.test and performance evaluation utilities.

Covers:
- Backtest trained RL agents on test data
- Performance metrics (Sharpe, Sortino, Calmar, max drawdown)
- Account value tracking
- Action/trade analysis
- Multi-algorithm comparison
- High-level finrl.test API

Usage (CLI):
    python finrl_backtest.py --action backtest --algorithm ppo --model_path trained_models/ppo_agent.zip --data_file test.csv --tickers AAPL MSFT
    python finrl_backtest.py --action backtest_highlevel --algorithm ppo --tickers AAPL MSFT --test_start 2023-01-01 --test_end 2023-12-31
    python finrl_backtest.py --action compare --model_dir trained_models --data_file test.csv --algorithms a2c ppo sac
    python finrl_backtest.py --action metrics --account_values_file results/account_values.csv
"""

import json
import sys
import os
import argparse
import numpy as np
import pandas as pd

from finrl.agents.stablebaselines3.models import DRLAgent
from finrl.test import test as finrl_test
from finrl.config import (
    INDICATORS, TRAINED_MODEL_DIR, RESULTS_DIR,
    TEST_START_DATE, TEST_END_DATE,
)

from finrl_environments import create_stock_trading_env


# ============================================================================
# Performance Metrics
# ============================================================================

def calculate_metrics(account_values: np.ndarray, risk_free_rate: float = 0.02) -> dict:
    """Calculate comprehensive performance metrics from account value series."""
    if len(account_values) < 2:
        return {"error": "Need at least 2 data points"}

    values = np.array(account_values, dtype=float)
    daily_returns = np.diff(values) / values[:-1]

    # Basic returns
    total_return = (values[-1] / values[0] - 1) * 100
    annualized_return = ((values[-1] / values[0]) ** (252 / len(daily_returns)) - 1) * 100

    # Risk metrics
    daily_vol = np.std(daily_returns) * 100
    annual_vol = daily_vol * np.sqrt(252)

    # Sharpe ratio (annualized)
    excess_return = np.mean(daily_returns) - risk_free_rate / 252
    sharpe = excess_return / (np.std(daily_returns) + 1e-8) * np.sqrt(252)

    # Sortino ratio
    downside_returns = daily_returns[daily_returns < 0]
    downside_std = np.std(downside_returns) if len(downside_returns) > 0 else 1e-8
    sortino = excess_return / (downside_std + 1e-8) * np.sqrt(252)

    # Maximum drawdown
    running_max = np.maximum.accumulate(values)
    drawdowns = (values - running_max) / running_max
    max_drawdown = np.min(drawdowns) * 100
    max_drawdown_duration = 0
    current_dd_duration = 0
    for i in range(len(values)):
        if values[i] < running_max[i]:
            current_dd_duration += 1
            max_drawdown_duration = max(max_drawdown_duration, current_dd_duration)
        else:
            current_dd_duration = 0

    # Calmar ratio
    calmar = annualized_return / (abs(max_drawdown) + 1e-8) if max_drawdown != 0 else 0

    # Win rate
    winning_days = np.sum(daily_returns > 0)
    losing_days = np.sum(daily_returns < 0)
    win_rate = winning_days / (winning_days + losing_days) * 100 if (winning_days + losing_days) > 0 else 0

    # Profit factor
    gross_profit = np.sum(daily_returns[daily_returns > 0])
    gross_loss = abs(np.sum(daily_returns[daily_returns < 0]))
    profit_factor = gross_profit / (gross_loss + 1e-8)

    # VaR (95%) and CVaR
    var_95 = np.percentile(daily_returns, 5) * 100
    cvar_95 = np.mean(daily_returns[daily_returns <= np.percentile(daily_returns, 5)]) * 100

    return {
        "total_return_pct": round(float(total_return), 4),
        "annualized_return_pct": round(float(annualized_return), 4),
        "sharpe_ratio": round(float(sharpe), 4),
        "sortino_ratio": round(float(sortino), 4),
        "calmar_ratio": round(float(calmar), 4),
        "max_drawdown_pct": round(float(max_drawdown), 4),
        "max_drawdown_duration_days": int(max_drawdown_duration),
        "daily_volatility_pct": round(float(daily_vol), 4),
        "annual_volatility_pct": round(float(annual_vol), 4),
        "win_rate_pct": round(float(win_rate), 2),
        "profit_factor": round(float(profit_factor), 4),
        "var_95_pct": round(float(var_95), 4),
        "cvar_95_pct": round(float(cvar_95), 4),
        "total_trading_days": int(len(daily_returns)),
        "winning_days": int(winning_days),
        "losing_days": int(losing_days),
        "initial_value": round(float(values[0]), 2),
        "final_value": round(float(values[-1]), 2),
        "peak_value": round(float(np.max(values)), 2),
        "trough_value": round(float(np.min(values)), 2),
    }


def analyze_actions(actions: np.ndarray, tickers: list = None) -> dict:
    """Analyze trading actions from backtest."""
    actions = np.array(actions)
    if actions.ndim == 1:
        actions = actions.reshape(-1, 1)

    num_steps, num_stocks = actions.shape
    total_buys = int(np.sum(actions > 0))
    total_sells = int(np.sum(actions < 0))
    total_holds = int(np.sum(actions == 0))

    per_stock = []
    for i in range(num_stocks):
        stock_actions = actions[:, i]
        ticker_name = tickers[i] if tickers and i < len(tickers) else f"Stock_{i}"
        per_stock.append({
            "ticker": ticker_name,
            "buys": int(np.sum(stock_actions > 0)),
            "sells": int(np.sum(stock_actions < 0)),
            "holds": int(np.sum(stock_actions == 0)),
            "avg_buy_size": round(float(np.mean(stock_actions[stock_actions > 0])), 2) if np.any(stock_actions > 0) else 0,
            "avg_sell_size": round(float(np.mean(stock_actions[stock_actions < 0])), 2) if np.any(stock_actions < 0) else 0,
            "max_position_change": round(float(np.max(np.abs(stock_actions))), 2),
        })

    return {
        "total_steps": num_steps,
        "num_stocks": num_stocks,
        "total_buys": total_buys,
        "total_sells": total_sells,
        "total_holds": total_holds,
        "activity_rate_pct": round((total_buys + total_sells) / (num_steps * num_stocks) * 100, 2),
        "per_stock_analysis": per_stock,
    }


# ============================================================================
# Backtesting Functions
# ============================================================================

def backtest_agent(
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
    save_results: bool = True,
    results_dir: str = None,
    tickers: list = None,
) -> dict:
    """
    Backtest a trained agent on test data.
    Returns comprehensive performance metrics.
    """
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)
    if results_dir is None:
        results_dir = RESULTS_DIR
    if stock_dim is None:
        stock_dim = df["tic"].nunique() if "tic" in df.columns else 1

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

    # Load model and predict
    account_memory = DRLAgent.DRL_prediction_load_from_file(
        model_name=algorithm.lower(),
        environment=env,
        cwd=os.path.dirname(model_path) or ".",
        deterministic=deterministic,
    )

    # Extract account values
    if isinstance(account_memory, tuple):
        df_account, df_actions = account_memory
    else:
        df_account = account_memory
        df_actions = None

    if isinstance(df_account, pd.DataFrame) and "account_value" in df_account.columns:
        values = df_account["account_value"].values
    elif isinstance(df_account, list):
        values = np.array(df_account)
    else:
        values = np.array([initial_amount])

    # Calculate metrics
    metrics = calculate_metrics(values)
    metrics["algorithm"] = algorithm
    metrics["model_path"] = model_path

    # Action analysis
    if df_actions is not None:
        actions_array = df_actions.values if isinstance(df_actions, pd.DataFrame) else np.array(df_actions)
        metrics["action_analysis"] = analyze_actions(actions_array, tickers)

    # Save results
    if save_results:
        os.makedirs(results_dir, exist_ok=True)
        if isinstance(df_account, pd.DataFrame):
            df_account.to_csv(os.path.join(results_dir, f"{algorithm}_account_values.csv"), index=False)
        if df_actions is not None and isinstance(df_actions, pd.DataFrame):
            df_actions.to_csv(os.path.join(results_dir, f"{algorithm}_actions.csv"), index=False)
        metrics["results_saved_to"] = results_dir

    return metrics


def backtest_highlevel(
    algorithm: str = "ppo",
    tickers: list = None,
    test_start: str = None,
    test_end: str = None,
    data_source: str = "yahoofinance",
    time_interval: str = "1D",
    indicators: list = None,
    if_vix: bool = True,
    **kwargs,
) -> dict:
    """Backtest using FinRL's high-level test() API."""
    if tickers is None:
        tickers = ["AAPL", "MSFT", "GOOGL"]
    if indicators is None:
        indicators = list(INDICATORS)
    if test_start is None:
        test_start = TEST_START_DATE
    if test_end is None:
        test_end = TEST_END_DATE

    from finrl.meta.env_stock_trading.env_stocktrading import StockTradingEnv

    result = finrl_test(
        start_date=test_start,
        end_date=test_end,
        ticker_list=tickers,
        data_source=data_source,
        time_interval=time_interval,
        technical_indicator_list=indicators,
        drl_lib="stable_baselines3",
        env=StockTradingEnv,
        model_name=algorithm.lower(),
        if_vix=if_vix,
        **kwargs,
    )

    return {
        "status": "success",
        "algorithm": algorithm,
        "test_period": f"{test_start} to {test_end}",
        "tickers": tickers,
        "result": str(result)[:500] if result else "No result returned",
    }


def compare_algorithms(
    df: pd.DataFrame,
    algorithms: list = None,
    model_dir: str = None,
    stock_dim: int = None,
    hmax: int = 100,
    initial_amount: int = 1_000_000,
    tech_indicator_list: list = None,
    tickers: list = None,
) -> dict:
    """Compare multiple trained algorithms on the same test data."""
    if algorithms is None:
        algorithms = ["a2c", "ppo", "sac"]
    if model_dir is None:
        model_dir = TRAINED_MODEL_DIR
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)
    if stock_dim is None:
        stock_dim = df["tic"].nunique() if "tic" in df.columns else 1

    results = {}
    for algo in algorithms:
        # Try multiple path patterns
        possible_paths = [
            os.path.join(model_dir, f"{algo}_agent.zip"),
            os.path.join(model_dir, f"{algo}_agent"),
            os.path.join(model_dir, f"{algo}.zip"),
            os.path.join(model_dir, algo),
        ]

        model_path = None
        for p in possible_paths:
            if os.path.exists(p):
                model_path = p
                break

        if model_path is None:
            results[algo] = {"error": f"Model not found. Tried: {possible_paths}"}
            continue

        try:
            metrics = backtest_agent(
                model_path=model_path,
                df=df,
                algorithm=algo,
                stock_dim=stock_dim,
                hmax=hmax,
                initial_amount=initial_amount,
                tech_indicator_list=tech_indicator_list,
                save_results=False,
                tickers=tickers,
            )
            results[algo] = metrics
        except Exception as e:
            results[algo] = {"error": str(e)}

    # Ranking
    valid = {k: v for k, v in results.items() if "sharpe_ratio" in v}
    if valid:
        ranking = sorted(valid.items(), key=lambda x: x[1]["sharpe_ratio"], reverse=True)
        return {
            "comparison": results,
            "ranking_by_sharpe": [{"rank": i + 1, "algorithm": k, "sharpe": v["sharpe_ratio"],
                                   "return": v["total_return_pct"]}
                                  for i, (k, v) in enumerate(ranking)],
            "best_algorithm": ranking[0][0],
        }

    return {"comparison": results}


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="FinRL Backtesting & Evaluation")
    parser.add_argument("--action", required=True,
                        choices=["backtest", "backtest_highlevel", "compare",
                                 "metrics", "list_metrics"])
    parser.add_argument("--algorithm", default="ppo")
    parser.add_argument("--algorithms", nargs="*", default=["a2c", "ppo", "sac"])
    parser.add_argument("--model_path", help="Path to trained model")
    parser.add_argument("--model_dir", default=TRAINED_MODEL_DIR)
    parser.add_argument("--data_file", help="CSV test data file")
    parser.add_argument("--account_values_file", help="CSV with account values")
    parser.add_argument("--tickers", nargs="*")
    parser.add_argument("--stock_dim", type=int)
    parser.add_argument("--initial_amount", type=float, default=1_000_000)
    parser.add_argument("--hmax", type=int, default=100)
    parser.add_argument("--test_start", default=TEST_START_DATE)
    parser.add_argument("--test_end", default=TEST_END_DATE)
    parser.add_argument("--source", default="yahoofinance")
    parser.add_argument("--results_dir", default=RESULTS_DIR)
    parser.add_argument("--indicators", nargs="*")

    args = parser.parse_args()

    if args.action == "list_metrics":
        result = {
            "available_metrics": [
                "total_return_pct", "annualized_return_pct",
                "sharpe_ratio", "sortino_ratio", "calmar_ratio",
                "max_drawdown_pct", "max_drawdown_duration_days",
                "daily_volatility_pct", "annual_volatility_pct",
                "win_rate_pct", "profit_factor",
                "var_95_pct", "cvar_95_pct",
                "total_trading_days", "winning_days", "losing_days",
                "initial_value", "final_value", "peak_value", "trough_value",
            ]
        }

    elif args.action == "backtest":
        if not args.model_path or not args.data_file:
            result = {"error": "Provide --model_path and --data_file"}
        elif not os.path.exists(args.data_file):
            result = {"error": f"Data file not found: {args.data_file}"}
        else:
            df = pd.read_csv(args.data_file)
            result = backtest_agent(
                model_path=args.model_path,
                df=df,
                algorithm=args.algorithm,
                stock_dim=args.stock_dim,
                initial_amount=int(args.initial_amount),
                hmax=args.hmax,
                tech_indicator_list=args.indicators,
                results_dir=args.results_dir,
                tickers=args.tickers,
            )

    elif args.action == "backtest_highlevel":
        result = backtest_highlevel(
            algorithm=args.algorithm,
            tickers=args.tickers,
            test_start=args.test_start,
            test_end=args.test_end,
            data_source=args.source,
            indicators=args.indicators,
        )

    elif args.action == "compare":
        if not args.data_file or not os.path.exists(args.data_file):
            result = {"error": "Provide valid --data_file for comparison"}
        else:
            df = pd.read_csv(args.data_file)
            result = compare_algorithms(
                df=df,
                algorithms=args.algorithms,
                model_dir=args.model_dir,
                stock_dim=args.stock_dim,
                initial_amount=int(args.initial_amount),
                tech_indicator_list=args.indicators,
                tickers=args.tickers,
            )

    elif args.action == "metrics":
        if not args.account_values_file or not os.path.exists(args.account_values_file):
            result = {"error": "Provide valid --account_values_file"}
        else:
            df = pd.read_csv(args.account_values_file)
            col = "account_value" if "account_value" in df.columns else df.columns[-1]
            result = calculate_metrics(df[col].values)

    else:
        result = {"error": f"Unknown action: {args.action}"}

    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()
