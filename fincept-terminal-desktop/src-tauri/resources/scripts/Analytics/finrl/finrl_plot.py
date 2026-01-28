"""
FinRL Visualization & Performance Charts
Custom plotting since pyfolio is incompatible with Python 3.13.

Covers:
- Equity curve plotting
- Drawdown chart
- Daily/monthly returns analysis
- Algorithm comparison charts
- Portfolio weight visualization
- Action heatmaps
- Risk metrics radar chart
- Export to JSON for frontend rendering

Usage (CLI):
    python finrl_plot.py --action equity_curve --data_file results/account_values.csv --output results/equity_curve.json
    python finrl_plot.py --action drawdown --data_file results/account_values.csv --output results/drawdown.json
    python finrl_plot.py --action returns_analysis --data_file results/account_values.csv --output results/returns.json
    python finrl_plot.py --action comparison --data_dir results --algorithms a2c ppo sac --output results/comparison.json
    python finrl_plot.py --action full_report --data_file results/account_values.csv --output results/full_report.json
"""

import json
import sys
import os
import argparse
import numpy as np
import pandas as pd
from datetime import datetime


# ============================================================================
# Data Extraction
# ============================================================================

def load_account_values(filepath: str) -> np.ndarray:
    """Load account values from CSV."""
    df = pd.read_csv(filepath)
    if "account_value" in df.columns:
        return df["account_value"].values.astype(float)
    return df.iloc[:, -1].values.astype(float)


def load_account_df(filepath: str) -> pd.DataFrame:
    """Load full account dataframe."""
    return pd.read_csv(filepath)


# ============================================================================
# Equity Curve
# ============================================================================

def generate_equity_curve(values: np.ndarray, dates: list = None, benchmark: np.ndarray = None) -> dict:
    """Generate equity curve data for frontend charting."""
    n = len(values)
    if dates is None:
        dates = list(range(n))

    # Normalize to percentage returns from initial
    normalized = (values / values[0] - 1) * 100

    result = {
        "chart_type": "equity_curve",
        "title": "Portfolio Equity Curve",
        "x_label": "Date",
        "y_label": "Return (%)",
        "series": [
            {
                "name": "Portfolio",
                "data": [{"x": str(dates[i]), "y": round(float(normalized[i]), 2)} for i in range(n)],
                "color": "#FF8800",
            }
        ],
        "summary": {
            "initial_value": round(float(values[0]), 2),
            "final_value": round(float(values[-1]), 2),
            "total_return_pct": round(float(normalized[-1]), 2),
            "peak_return_pct": round(float(np.max(normalized)), 2),
        },
    }

    if benchmark is not None:
        bench_norm = (benchmark / benchmark[0] - 1) * 100
        result["series"].append({
            "name": "Benchmark",
            "data": [{"x": str(dates[i]), "y": round(float(bench_norm[i]), 2)} for i in range(min(n, len(benchmark)))],
            "color": "#787878",
        })
        result["summary"]["benchmark_return_pct"] = round(float(bench_norm[-1]), 2)
        result["summary"]["alpha_pct"] = round(float(normalized[-1] - bench_norm[-1]), 2)

    return result


# ============================================================================
# Drawdown Chart
# ============================================================================

def generate_drawdown_chart(values: np.ndarray, dates: list = None) -> dict:
    """Generate drawdown chart data."""
    n = len(values)
    if dates is None:
        dates = list(range(n))

    running_max = np.maximum.accumulate(values)
    drawdowns = (values - running_max) / running_max * 100

    max_dd_idx = np.argmin(drawdowns)
    max_dd_start = np.argmax(values[:max_dd_idx + 1]) if max_dd_idx > 0 else 0

    return {
        "chart_type": "drawdown",
        "title": "Underwater (Drawdown) Chart",
        "x_label": "Date",
        "y_label": "Drawdown (%)",
        "series": [
            {
                "name": "Drawdown",
                "data": [{"x": str(dates[i]), "y": round(float(drawdowns[i]), 2)} for i in range(n)],
                "color": "#FF3B3B",
                "fill": True,
            }
        ],
        "summary": {
            "max_drawdown_pct": round(float(np.min(drawdowns)), 2),
            "max_drawdown_date": str(dates[max_dd_idx]),
            "max_drawdown_peak_date": str(dates[max_dd_start]),
            "current_drawdown_pct": round(float(drawdowns[-1]), 2),
            "avg_drawdown_pct": round(float(np.mean(drawdowns)), 2),
        },
    }


# ============================================================================
# Returns Analysis
# ============================================================================

def generate_returns_analysis(values: np.ndarray, dates: list = None) -> dict:
    """Generate returns distribution and time-series analysis."""
    daily_returns = np.diff(values) / values[:-1] * 100
    n = len(daily_returns)

    # Distribution
    hist, bin_edges = np.histogram(daily_returns, bins=50)
    distribution = [
        {"x": round(float((bin_edges[i] + bin_edges[i + 1]) / 2), 3),
         "y": int(hist[i])}
        for i in range(len(hist))
    ]

    # Rolling metrics (21-day window)
    window = min(21, n)
    rolling_mean = pd.Series(daily_returns).rolling(window).mean().values
    rolling_std = pd.Series(daily_returns).rolling(window).std().values
    rolling_sharpe = rolling_mean / (rolling_std + 1e-8) * np.sqrt(252)

    dates_r = dates[1:] if dates and len(dates) > 1 else list(range(n))

    return {
        "chart_type": "returns_analysis",
        "title": "Returns Analysis",
        "distribution": {
            "title": "Daily Returns Distribution",
            "data": distribution,
            "stats": {
                "mean_pct": round(float(np.mean(daily_returns)), 4),
                "median_pct": round(float(np.median(daily_returns)), 4),
                "std_pct": round(float(np.std(daily_returns)), 4),
                "skewness": round(float(pd.Series(daily_returns).skew()), 4),
                "kurtosis": round(float(pd.Series(daily_returns).kurtosis()), 4),
                "best_day_pct": round(float(np.max(daily_returns)), 2),
                "worst_day_pct": round(float(np.min(daily_returns)), 2),
                "positive_days": int(np.sum(daily_returns > 0)),
                "negative_days": int(np.sum(daily_returns < 0)),
            },
        },
        "rolling_sharpe": {
            "title": f"Rolling {window}-Day Sharpe Ratio",
            "data": [
                {"x": str(dates_r[i]), "y": round(float(rolling_sharpe[i]), 2)}
                for i in range(n) if not np.isnan(rolling_sharpe[i])
            ],
            "color": "#00E5FF",
        },
        "monthly_returns": _calc_monthly_returns(values, dates),
    }


def _calc_monthly_returns(values: np.ndarray, dates: list = None) -> dict:
    """Calculate monthly return heatmap data."""
    if dates is None or len(dates) < 2:
        total = (values[-1] / values[0] - 1) * 100 if len(values) > 1 else 0
        return {"data": [], "total_return_pct": round(float(total), 2)}

    try:
        df = pd.DataFrame({"date": pd.to_datetime(dates[:len(values)]), "value": values})
        df.set_index("date", inplace=True)
        monthly = df.resample("M").last()
        monthly_ret = monthly.pct_change().dropna() * 100

        data = []
        for idx, row in monthly_ret.iterrows():
            data.append({
                "year": idx.year,
                "month": idx.month,
                "return_pct": round(float(row["value"]), 2),
            })
        return {"title": "Monthly Returns Heatmap", "data": data}
    except Exception:
        return {"data": []}


# ============================================================================
# Algorithm Comparison
# ============================================================================

def generate_comparison_chart(results: dict) -> dict:
    """Generate comparison data for multiple algorithms."""
    series = []
    metrics_table = []

    for algo_name, data in results.items():
        values = np.array(data["values"], dtype=float)
        normalized = (values / values[0] - 1) * 100
        dates = data.get("dates", list(range(len(values))))

        series.append({
            "name": algo_name.upper(),
            "data": [{"x": str(dates[i]), "y": round(float(normalized[i]), 2)}
                     for i in range(len(values))],
        })

        daily_ret = np.diff(values) / values[:-1]
        sharpe = np.mean(daily_ret) / (np.std(daily_ret) + 1e-8) * np.sqrt(252)
        max_dd = np.min(values / np.maximum.accumulate(values) - 1) * 100

        metrics_table.append({
            "algorithm": algo_name.upper(),
            "total_return_pct": round(float(normalized[-1]), 2),
            "sharpe_ratio": round(float(sharpe), 4),
            "max_drawdown_pct": round(float(max_dd), 2),
            "final_value": round(float(values[-1]), 2),
        })

    # Sort by Sharpe
    metrics_table.sort(key=lambda x: x["sharpe_ratio"], reverse=True)

    return {
        "chart_type": "comparison",
        "title": "Algorithm Comparison",
        "series": series,
        "metrics_table": metrics_table,
        "best_by_return": max(metrics_table, key=lambda x: x["total_return_pct"])["algorithm"],
        "best_by_sharpe": metrics_table[0]["algorithm"],
        "best_by_drawdown": min(metrics_table, key=lambda x: abs(x["max_drawdown_pct"]))["algorithm"],
    }


# ============================================================================
# Full Report
# ============================================================================

def generate_full_report(
    values: np.ndarray,
    dates: list = None,
    algorithm: str = "DRL Agent",
    benchmark_values: np.ndarray = None,
    actions: np.ndarray = None,
    tickers: list = None,
) -> dict:
    """Generate a complete performance report with all charts."""
    report = {
        "generated_at": datetime.now().isoformat(),
        "algorithm": algorithm,
        "equity_curve": generate_equity_curve(values, dates, benchmark_values),
        "drawdown": generate_drawdown_chart(values, dates),
        "returns_analysis": generate_returns_analysis(values, dates),
    }

    # Performance metrics
    daily_ret = np.diff(values) / values[:-1]
    report["performance_summary"] = {
        "initial_value": round(float(values[0]), 2),
        "final_value": round(float(values[-1]), 2),
        "total_return_pct": round(float((values[-1] / values[0] - 1) * 100), 2),
        "sharpe_ratio": round(float(np.mean(daily_ret) / (np.std(daily_ret) + 1e-8) * np.sqrt(252)), 4),
        "max_drawdown_pct": round(float(np.min(values / np.maximum.accumulate(values) - 1) * 100), 2),
        "volatility_annual_pct": round(float(np.std(daily_ret) * np.sqrt(252) * 100), 2),
        "win_rate_pct": round(float(np.sum(daily_ret > 0) / len(daily_ret) * 100), 2),
        "total_trading_days": int(len(daily_ret)),
    }

    # Action analysis
    if actions is not None:
        actions = np.array(actions)
        report["action_summary"] = {
            "total_actions": int(actions.size),
            "buy_actions": int(np.sum(actions > 0)),
            "sell_actions": int(np.sum(actions < 0)),
            "hold_actions": int(np.sum(actions == 0)),
        }

    return report


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="FinRL Visualization & Reporting")
    parser.add_argument("--action", required=True,
                        choices=["equity_curve", "drawdown", "returns_analysis",
                                 "comparison", "full_report"])
    parser.add_argument("--data_file", help="CSV with account values")
    parser.add_argument("--data_dir", help="Directory with multiple algorithm results")
    parser.add_argument("--algorithms", nargs="*", default=["a2c", "ppo", "sac"])
    parser.add_argument("--benchmark_file", help="CSV with benchmark values")
    parser.add_argument("--algorithm_name", default="DRL Agent")
    parser.add_argument("--output", help="Output JSON file path")

    args = parser.parse_args()

    if args.action in ("equity_curve", "drawdown", "returns_analysis", "full_report"):
        if not args.data_file or not os.path.exists(args.data_file):
            result = {"error": "Provide valid --data_file"}
            print(json.dumps(result, indent=2))
            return

        values = load_account_values(args.data_file)
        df = load_account_df(args.data_file)
        dates = df["date"].tolist() if "date" in df.columns else None

        benchmark = None
        if args.benchmark_file and os.path.exists(args.benchmark_file):
            benchmark = load_account_values(args.benchmark_file)

        if args.action == "equity_curve":
            result = generate_equity_curve(values, dates, benchmark)
        elif args.action == "drawdown":
            result = generate_drawdown_chart(values, dates)
        elif args.action == "returns_analysis":
            result = generate_returns_analysis(values, dates)
        elif args.action == "full_report":
            result = generate_full_report(values, dates, args.algorithm_name, benchmark)

    elif args.action == "comparison":
        if not args.data_dir or not os.path.exists(args.data_dir):
            result = {"error": "Provide valid --data_dir"}
            print(json.dumps(result, indent=2))
            return

        results = {}
        for algo in args.algorithms:
            filepath = os.path.join(args.data_dir, f"{algo}_account_values.csv")
            if os.path.exists(filepath):
                df = pd.read_csv(filepath)
                col = "account_value" if "account_value" in df.columns else df.columns[-1]
                dates_col = df["date"].tolist() if "date" in df.columns else None
                results[algo] = {"values": df[col].tolist(), "dates": dates_col}

        if not results:
            result = {"error": f"No result files found in {args.data_dir}"}
        else:
            result = generate_comparison_chart(results)

    else:
        result = {"error": f"Unknown action: {args.action}"}

    output = json.dumps(result, indent=2, default=str)

    if args.output:
        os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
        with open(args.output, "w") as f:
            f.write(output)
        print(json.dumps({"status": "saved", "output": args.output}))
    else:
        print(output)


if __name__ == "__main__":
    main()
