"""
skfolio Worker Handler - Advanced Portfolio Optimization
Usage: python worker_handler.py <operation> <json_data>

Operations:
  optimize           - Portfolio optimization (mean_risk, hrp, herc, nco, etc.)
  efficient_frontier - Generate efficient frontier
  risk_metrics       - Calculate comprehensive risk metrics
  stress_test        - Stress test portfolio
  backtest           - Backtest with rebalancing
  compare_strategies - Compare multiple optimization strategies
  risk_attribution   - Risk attribution analysis
  hyperparameter_tune- Hyperparameter tuning with cross-validation
  measures           - Calculate specific or all measures
  validate_model     - Walk-forward / combinatorial purged validation
  scenario_analysis  - Scenario analysis
  generate_report    - Full portfolio report
"""
import sys
import os
import json
import traceback
import numpy as np
import pandas as pd

# Add parent directory to path so we can import from python_skfolio_lib
script_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(script_dir)
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)


def _parse_prices(data):
    """Parse price data from JSON into DataFrame"""
    prices_raw = data.get("prices_data") or data.get("prices") or data.get("data")
    if isinstance(prices_raw, str):
        prices_raw = json.loads(prices_raw)

    if isinstance(prices_raw, dict):
        df = pd.DataFrame(prices_raw)
        if "date" in df.columns:
            df.set_index("date", inplace=True)
        elif "Date" in df.columns:
            df.set_index("Date", inplace=True)
        df.index = pd.to_datetime(df.index)
        for col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
        df.dropna(inplace=True)
        return df
    elif isinstance(prices_raw, list):
        df = pd.DataFrame(prices_raw)
        if "date" in df.columns:
            df.set_index("date", inplace=True)
        df.index = pd.to_datetime(df.index)
        for col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
        df.dropna(inplace=True)
        return df
    else:
        raise ValueError("prices_data must be a JSON object or array")


def _parse_weights(data):
    """Parse weights from JSON"""
    weights_raw = data.get("weights")
    if isinstance(weights_raw, str):
        weights_raw = json.loads(weights_raw)
    if isinstance(weights_raw, dict):
        return weights_raw
    elif isinstance(weights_raw, list):
        return weights_raw
    return None


def _serialize(obj):
    """JSON serializer for numpy/pandas types"""
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, (np.floating,)):
        return float(obj)
    if isinstance(obj, np.ndarray):
        return obj.tolist()
    if isinstance(obj, pd.Series):
        return obj.to_dict()
    if isinstance(obj, pd.DataFrame):
        return obj.to_dict("records")
    if isinstance(obj, (pd.Timestamp,)):
        return obj.isoformat()
    return str(obj)


# ==================== OPERATIONS ====================

def op_optimize(data):
    """Optimize portfolio using skfolio"""
    from skfolio.preprocessing import prices_to_returns
    from skfolio.optimization import (
        MeanRisk, HierarchicalRiskParity, HierarchicalEqualRiskContribution,
        NestedClustersOptimization, RiskBudgeting, MaximumDiversification,
        EqualWeighted, InverseVolatility, Random, DistributionallyRobustCVaR,
        ObjectiveFunction
    )
    from skfolio import RiskMeasure

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    method = data.get("optimization_method", "mean_risk")
    objective = data.get("objective_function", "maximize_ratio")
    risk_measure = data.get("risk_measure", "variance")
    config = data.get("config", {})
    if isinstance(config, str):
        config = json.loads(config) if config and config != "null" else {}

    # Map risk measures
    risk_map = {
        "variance": RiskMeasure.VARIANCE,
        "semi_variance": RiskMeasure.SEMI_VARIANCE,
        "cvar": RiskMeasure.CVAR,
        "evar": RiskMeasure.EVAR,
        "max_drawdown": RiskMeasure.MAX_DRAWDOWN,
        "cdar": RiskMeasure.CDAR,
        "ulcer_index": RiskMeasure.ULCER_INDEX,
        "mean_absolute_deviation": RiskMeasure.MEAN_ABSOLUTE_DEVIATION,
        "worst_realization": RiskMeasure.WORST_REALIZATION,
        "standard_deviation": RiskMeasure.STANDARD_DEVIATION,
        "semi_deviation": RiskMeasure.SEMI_DEVIATION,
        "average_drawdown": RiskMeasure.AVERAGE_DRAWDOWN,
        "edar": RiskMeasure.EDAR,
        "gini_mean_difference": RiskMeasure.GINI_MEAN_DIFFERENCE,
        "first_lower_partial_moment": RiskMeasure.FIRST_LOWER_PARTIAL_MOMENT,
    }
    risk_enum = risk_map.get(risk_measure, RiskMeasure.VARIANCE)

    # Map objectives
    obj_map = {
        "minimize_risk": ObjectiveFunction.MINIMIZE_RISK,
        "maximize_return": ObjectiveFunction.MAXIMIZE_RETURN,
        "maximize_ratio": ObjectiveFunction.MAXIMIZE_RATIO,
        "maximize_utility": ObjectiveFunction.MAXIMIZE_UTILITY,
    }
    obj_enum = obj_map.get(objective, ObjectiveFunction.MAXIMIZE_RATIO)

    # Build model based on method
    if method == "mean_risk":
        model_kwargs = {
            "risk_measure": risk_enum,
            "objective_function": obj_enum,
        }
        if config.get("l1_coef"):
            model_kwargs["l1_coef"] = float(config["l1_coef"])
        if config.get("l2_coef"):
            model_kwargs["l2_coef"] = float(config["l2_coef"])
        if config.get("risk_aversion"):
            model_kwargs["risk_aversion"] = float(config["risk_aversion"])
        if config.get("max_weights"):
            model_kwargs["max_weights"] = float(config["max_weights"])
        if config.get("min_weights"):
            model_kwargs["min_weights"] = float(config["min_weights"])
        model = MeanRisk(**model_kwargs)

    elif method == "hrp":
        from skfolio.distance import PearsonDistance
        from skfolio.cluster import HierarchicalClustering
        model = HierarchicalRiskParity(
            risk_measure=risk_enum,
        )

    elif method == "herc":
        model = HierarchicalEqualRiskContribution(
            risk_measure=risk_enum,
        )

    elif method == "nco":
        model = NestedClustersOptimization(
            risk_measure=risk_enum,
        )

    elif method == "risk_budgeting":
        model = RiskBudgeting(
            risk_measure=risk_enum,
        )

    elif method == "max_diversification":
        model = MaximumDiversification()

    elif method == "equal_weight":
        model = EqualWeighted()

    elif method == "inverse_volatility":
        model = InverseVolatility()

    elif method == "random":
        model = Random()

    elif method == "distributionally_robust_cvar":
        model = DistributionallyRobustCVaR()

    else:
        raise ValueError(f"Unknown optimization method: {method}")

    # Fit and predict
    model.fit(returns)
    portfolio = model.predict(returns)

    # Extract results
    weights = dict(zip(returns.columns, model.weights_.tolist()))
    asset_names = returns.columns.tolist()

    # Portfolio metrics
    metrics = {}
    for attr in ["mean", "annualized_mean", "variance", "annualized_variance",
                 "semi_variance", "standard_deviation", "annualized_standard_deviation",
                 "max_drawdown", "cvar", "sharpe_ratio", "sortino_ratio",
                 "calmar_ratio", "value_at_risk", "cdar", "ulcer_index",
                 "average_drawdown", "worst_realization"]:
        try:
            val = getattr(portfolio, attr, None)
            if val is not None:
                metrics[attr] = float(val)
        except Exception:
            pass

    return {
        "status": "success",
        "weights": weights,
        "asset_names": asset_names,
        "metrics": metrics,
        "optimization_method": method,
        "objective_function": objective,
        "risk_measure": risk_measure,
        "n_assets": len(asset_names),
        "n_observations": len(returns),
    }


def op_efficient_frontier(data):
    """Generate efficient frontier"""
    from skfolio.preprocessing import prices_to_returns
    from skfolio.optimization import MeanRisk, ObjectiveFunction
    from skfolio import RiskMeasure

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)
    n_portfolios = int(data.get("n_portfolios", 100))
    risk_measure = data.get("risk_measure", "variance")

    risk_map = {
        "variance": RiskMeasure.VARIANCE,
        "cvar": RiskMeasure.CVAR,
        "semi_variance": RiskMeasure.SEMI_VARIANCE,
    }
    risk_enum = risk_map.get(risk_measure, RiskMeasure.VARIANCE)

    frontier_returns = []
    frontier_volatility = []
    frontier_sharpe = []
    frontier_weights = []

    risk_aversions = np.logspace(-3, 3, min(n_portfolios, 500))

    for ra in risk_aversions:
        try:
            model = MeanRisk(
                risk_measure=RiskMeasure.VARIANCE,
                objective_function=ObjectiveFunction.MAXIMIZE_UTILITY,
                risk_aversion=ra,
            )
            model.fit(returns)
            w = model.weights_
            port_ret = float(np.dot(w, returns.mean()) * 252)
            port_vol = float(np.sqrt(np.dot(w, np.dot(returns.cov() * 252, w))))
            sharpe = (port_ret - 0.02) / port_vol if port_vol > 0 else 0
            frontier_returns.append(port_ret)
            frontier_volatility.append(port_vol)
            frontier_sharpe.append(sharpe)
            frontier_weights.append(w.tolist())
        except Exception:
            continue

    # Sort by volatility
    if frontier_returns:
        idx = np.argsort(frontier_volatility)
        frontier_returns = [frontier_returns[i] for i in idx]
        frontier_volatility = [frontier_volatility[i] for i in idx]
        frontier_sharpe = [frontier_sharpe[i] for i in idx]
        frontier_weights = [frontier_weights[i] for i in idx]

    return {
        "returns": frontier_returns,
        "volatility": frontier_volatility,
        "sharpe_ratios": frontier_sharpe,
        "portfolios": frontier_weights,
        "n_portfolios": len(frontier_returns),
        "asset_names": returns.columns.tolist(),
    }


def op_risk_metrics(data):
    """Calculate comprehensive risk metrics for a portfolio"""
    from skfolio.preprocessing import prices_to_returns
    from skfolio.measures import (
        mean, variance, semi_variance, standard_deviation, semi_deviation,
        cvar, value_at_risk, max_drawdown, average_drawdown, cdar, edar,
        ulcer_index, worst_realization, evar, entropic_risk_measure,
        gini_mean_difference, mean_absolute_deviation,
        drawdown_at_risk, get_cumulative_returns, get_drawdowns,
    )

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    weights_raw = _parse_weights(data)
    if weights_raw and isinstance(weights_raw, dict):
        w = np.array([weights_raw.get(col, 0) for col in returns.columns])
    elif weights_raw and isinstance(weights_raw, list):
        w = np.array(weights_raw)
    else:
        # Equal weight
        w = np.ones(len(returns.columns)) / len(returns.columns)

    port_returns = returns.values @ w

    metrics = {}
    calc = [
        ("mean", mean), ("variance", variance), ("semi_variance", semi_variance),
        ("standard_deviation", standard_deviation), ("semi_deviation", semi_deviation),
        ("cvar_95", lambda r: cvar(r, q=0.05)),
        ("value_at_risk_95", lambda r: value_at_risk(r, q=0.05)),
        ("max_drawdown", max_drawdown), ("average_drawdown", average_drawdown),
        ("cdar_95", lambda r: cdar(r, q=0.05)),
        ("edar_95", lambda r: edar(r, q=0.05)),
        ("ulcer_index", ulcer_index), ("worst_realization", worst_realization),
        ("evar", evar), ("entropic_risk_measure", entropic_risk_measure),
        ("gini_mean_difference", gini_mean_difference),
        ("mean_absolute_deviation", mean_absolute_deviation),
        ("drawdown_at_risk_95", lambda r: drawdown_at_risk(r, q=0.05)),
    ]
    for name, func in calc:
        try:
            metrics[name] = float(func(port_returns))
        except Exception:
            metrics[name] = None

    # Derived metrics
    ann_ret = float(np.mean(port_returns) * 252)
    ann_vol = float(np.std(port_returns) * np.sqrt(252))
    metrics["annualized_return"] = ann_ret
    metrics["annualized_volatility"] = ann_vol
    metrics["sharpe_ratio"] = (ann_ret - 0.02) / ann_vol if ann_vol > 0 else 0
    down = port_returns[port_returns < 0]
    down_vol = float(np.std(down) * np.sqrt(252)) if len(down) > 0 else 0
    metrics["sortino_ratio"] = (ann_ret - 0.02) / down_vol if down_vol > 0 else 0
    mdd = metrics.get("max_drawdown", 0)
    metrics["calmar_ratio"] = ann_ret / abs(mdd) if mdd and mdd < 0 else 0

    return {
        "status": "success",
        "metrics": metrics,
        "n_observations": len(port_returns),
    }


def op_stress_test(data):
    """Stress test portfolio with scenarios"""
    from skfolio.preprocessing import prices_to_returns

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    weights_raw = _parse_weights(data)
    if weights_raw and isinstance(weights_raw, dict):
        w = np.array([weights_raw.get(col, 0) for col in returns.columns])
    elif weights_raw and isinstance(weights_raw, list):
        w = np.array(weights_raw)
    else:
        w = np.ones(len(returns.columns)) / len(returns.columns)

    n_simulations = int(data.get("n_simulations", 10000))
    scenarios_raw = data.get("scenarios")
    if isinstance(scenarios_raw, str):
        scenarios_raw = json.loads(scenarios_raw) if scenarios_raw and scenarios_raw != "null" else None

    port_returns = returns.values @ w

    # Monte Carlo simulation
    mu = np.mean(port_returns)
    sigma = np.std(port_returns)
    simulated = np.random.normal(mu, sigma, (n_simulations, 252))
    cumulative = np.cumprod(1 + simulated, axis=1)
    final_values = cumulative[:, -1]

    # Calculate simulation statistics
    results = {
        "status": "success",
        "monte_carlo": {
            "mean_final_value": float(np.mean(final_values)),
            "median_final_value": float(np.median(final_values)),
            "std_final_value": float(np.std(final_values)),
            "var_5": float(np.percentile(final_values, 5)),
            "var_1": float(np.percentile(final_values, 1)),
            "best_case": float(np.percentile(final_values, 95)),
            "worst_case": float(np.percentile(final_values, 5)),
            "probability_loss": float(np.mean(final_values < 1.0)),
            "n_simulations": n_simulations,
        }
    }

    # Historical stress scenarios
    historical_scenarios = {
        "2008_financial_crisis": {"volatility_shock": 3.0, "return_shock": -0.40},
        "2020_covid_crash": {"volatility_shock": 4.0, "return_shock": -0.35},
        "2022_rate_hike": {"volatility_shock": 1.5, "return_shock": -0.20},
        "dot_com_bubble": {"volatility_shock": 2.0, "return_shock": -0.50},
        "black_monday_1987": {"volatility_shock": 5.0, "return_shock": -0.22},
    }

    scenario_results = {}
    for name, scenario in historical_scenarios.items():
        shocked_mu = mu + scenario["return_shock"] / 252
        shocked_sigma = sigma * scenario["volatility_shock"]
        shocked_sim = np.random.normal(shocked_mu, shocked_sigma, (1000, 252))
        shocked_cum = np.cumprod(1 + shocked_sim, axis=1)
        shocked_final = shocked_cum[:, -1]
        scenario_results[name] = {
            "expected_loss": float(1 - np.mean(shocked_final)),
            "worst_case_loss": float(1 - np.percentile(shocked_final, 5)),
            "probability_of_ruin": float(np.mean(shocked_final < 0.5)),
        }

    results["scenarios"] = scenario_results
    return results


def op_backtest(data):
    """Backtest portfolio with rebalancing"""
    from skfolio.preprocessing import prices_to_returns
    from skfolio.optimization import MeanRisk, ObjectiveFunction
    from skfolio import RiskMeasure

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    rebalance_freq = int(data.get("rebalance_freq", 21))
    window_size = int(data.get("window_size", 252))
    method = data.get("optimization_method", "mean_risk")
    risk_measure = data.get("risk_measure", "variance")

    risk_map = {
        "variance": RiskMeasure.VARIANCE,
        "cvar": RiskMeasure.CVAR,
        "semi_variance": RiskMeasure.SEMI_VARIANCE,
        "max_drawdown": RiskMeasure.MAX_DRAWDOWN,
    }
    risk_enum = risk_map.get(risk_measure, RiskMeasure.VARIANCE)

    # Walk-forward backtest
    portfolio_returns = []
    rebalance_dates = []
    weight_history = []

    current_weights = np.ones(len(returns.columns)) / len(returns.columns)

    for i in range(window_size, len(returns)):
        if (i - window_size) % rebalance_freq == 0:
            train_data = returns.iloc[i - window_size:i]
            try:
                model = MeanRisk(
                    risk_measure=risk_enum,
                    objective_function=ObjectiveFunction.MAXIMIZE_RATIO,
                )
                model.fit(train_data)
                current_weights = model.weights_
                rebalance_dates.append(str(returns.index[i]))
                weight_history.append(current_weights.tolist())
            except Exception:
                pass

        port_ret = float(np.dot(current_weights, returns.iloc[i].values))
        portfolio_returns.append(port_ret)

    portfolio_returns = np.array(portfolio_returns)
    cumulative = np.cumprod(1 + portfolio_returns)

    # Equal weight benchmark
    ew_returns = returns.iloc[window_size:].mean(axis=1).values
    ew_cumulative = np.cumprod(1 + ew_returns)

    ann_ret = float(np.mean(portfolio_returns) * 252)
    ann_vol = float(np.std(portfolio_returns) * np.sqrt(252))

    running_max = np.maximum.accumulate(cumulative)
    drawdown = (cumulative - running_max) / running_max
    max_dd = float(np.min(drawdown))

    return {
        "status": "success",
        "portfolio_cumulative": cumulative.tolist(),
        "benchmark_cumulative": ew_cumulative.tolist(),
        "dates": [str(d) for d in returns.index[window_size:]],
        "rebalance_dates": rebalance_dates,
        "n_rebalances": len(rebalance_dates),
        "metrics": {
            "annualized_return": ann_ret,
            "annualized_volatility": ann_vol,
            "sharpe_ratio": (ann_ret - 0.02) / ann_vol if ann_vol > 0 else 0,
            "max_drawdown": max_dd,
            "calmar_ratio": ann_ret / abs(max_dd) if max_dd < 0 else 0,
            "total_return": float(cumulative[-1] - 1) if len(cumulative) > 0 else 0,
        },
        "weight_history": weight_history,
    }


def op_compare_strategies(data):
    """Compare multiple optimization strategies"""
    from skfolio.preprocessing import prices_to_returns
    from skfolio.optimization import (
        MeanRisk, HierarchicalRiskParity, HierarchicalEqualRiskContribution,
        MaximumDiversification, EqualWeighted, InverseVolatility,
        RiskBudgeting, ObjectiveFunction
    )
    from skfolio import RiskMeasure

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    strategies_raw = data.get("strategies")
    if isinstance(strategies_raw, str):
        strategies_raw = json.loads(strategies_raw) if strategies_raw and strategies_raw != "null" else None

    if not strategies_raw:
        strategies_raw = ["mean_risk", "hrp", "herc", "max_diversification",
                          "equal_weight", "inverse_volatility", "risk_budgeting"]

    results = {}
    for strategy in strategies_raw:
        try:
            if strategy == "mean_risk":
                model = MeanRisk(objective_function=ObjectiveFunction.MAXIMIZE_RATIO)
            elif strategy == "hrp":
                model = HierarchicalRiskParity()
            elif strategy == "herc":
                model = HierarchicalEqualRiskContribution()
            elif strategy == "max_diversification":
                model = MaximumDiversification()
            elif strategy == "equal_weight":
                model = EqualWeighted()
            elif strategy == "inverse_volatility":
                model = InverseVolatility()
            elif strategy == "risk_budgeting":
                model = RiskBudgeting()
            else:
                continue

            model.fit(returns)
            portfolio = model.predict(returns)

            metrics = {}
            for attr in ["mean", "annualized_mean", "variance", "standard_deviation",
                         "annualized_standard_deviation", "max_drawdown", "cvar",
                         "sharpe_ratio", "sortino_ratio", "calmar_ratio"]:
                try:
                    val = getattr(portfolio, attr, None)
                    if val is not None:
                        metrics[attr] = float(val)
                except Exception:
                    pass

            results[strategy] = {
                "weights": dict(zip(returns.columns, model.weights_.tolist())),
                "metrics": metrics,
            }
        except Exception as e:
            results[strategy] = {"error": str(e)}

    return {"status": "success", "strategies": results}


def op_risk_attribution(data):
    """Risk attribution for portfolio"""
    from skfolio.preprocessing import prices_to_returns

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    weights_raw = _parse_weights(data)
    if weights_raw and isinstance(weights_raw, dict):
        w = np.array([weights_raw.get(col, 0) for col in returns.columns])
    elif weights_raw and isinstance(weights_raw, list):
        w = np.array(weights_raw)
    else:
        w = np.ones(len(returns.columns)) / len(returns.columns)

    cov = returns.cov().values * 252
    port_var = float(w @ cov @ w)
    port_vol = np.sqrt(port_var)

    # Marginal risk contribution
    marginal_risk = cov @ w / port_vol
    # Component risk contribution
    component_risk = w * marginal_risk
    # Percentage contribution
    pct_contribution = component_risk / port_vol

    return {
        "status": "success",
        "portfolio_volatility": float(port_vol),
        "portfolio_variance": float(port_var),
        "asset_names": returns.columns.tolist(),
        "weights": w.tolist(),
        "marginal_risk_contribution": marginal_risk.tolist(),
        "component_risk_contribution": component_risk.tolist(),
        "percentage_risk_contribution": pct_contribution.tolist(),
        "correlation_matrix": returns.corr().values.tolist(),
        "covariance_matrix": cov.tolist(),
    }


def op_hyperparameter_tune(data):
    """Hyperparameter tuning with cross-validation"""
    from skfolio.preprocessing import prices_to_returns
    from skfolio.optimization import MeanRisk, ObjectiveFunction
    from skfolio import RiskMeasure
    from skfolio.model_selection import WalkForward, CombinatorialPurgedCV
    from sklearn.model_selection import GridSearchCV

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    cv_method = data.get("cv_method", "walk_forward")

    if cv_method == "walk_forward":
        cv = WalkForward(train_size=252, test_size=63)
    elif cv_method == "combinatorial_purged":
        cv = CombinatorialPurgedCV(n_folds=5, n_test_folds=2)
    else:
        from sklearn.model_selection import KFold
        cv = KFold(n_splits=5, shuffle=False)

    param_grid = data.get("param_grid")
    if isinstance(param_grid, str):
        param_grid = json.loads(param_grid) if param_grid and param_grid != "null" else None

    if not param_grid:
        param_grid = {
            "risk_measure": [RiskMeasure.VARIANCE, RiskMeasure.CVAR, RiskMeasure.SEMI_VARIANCE],
            "l2_coef": [0.0, 0.001, 0.01, 0.1],
        }

    model = MeanRisk(objective_function=ObjectiveFunction.MAXIMIZE_RATIO)
    grid_search = GridSearchCV(
        estimator=model, param_grid=param_grid, cv=cv, n_jobs=-1
    )
    grid_search.fit(returns)

    best_params = {}
    for k, v in grid_search.best_params_.items():
        if hasattr(v, "name"):
            best_params[k] = v.name
        else:
            best_params[k] = v

    return {
        "status": "success",
        "best_params": best_params,
        "best_score": float(grid_search.best_score_),
        "cv_method": cv_method,
    }


def op_measures(data):
    """Calculate specific or all portfolio measures"""
    from skfolio.preprocessing import prices_to_returns
    from python_skfolio_lib.skfolio_measures import MeasuresExtended

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    weights_raw = _parse_weights(data)
    if weights_raw and isinstance(weights_raw, dict):
        w = np.array([weights_raw.get(col, 0) for col in returns.columns])
    elif weights_raw and isinstance(weights_raw, list):
        w = np.array(weights_raw)
    else:
        w = np.ones(len(returns.columns)) / len(returns.columns)

    port_returns = returns.values @ w
    measure_name = data.get("measure_name")

    calculator = MeasuresExtended()

    if measure_name:
        value = calculator.calculate_measure(port_returns, measure_name)
        return {"status": "success", "measure": measure_name, "value": float(value)}
    else:
        all_measures = calculator.calculate_all_measures(port_returns)
        # Convert nan to None for JSON
        clean = {}
        for k, v in all_measures.items():
            if isinstance(v, float) and np.isnan(v):
                clean[k] = None
            else:
                clean[k] = float(v) if isinstance(v, (float, np.floating)) else v
        return {"status": "success", "measures": clean}


def op_validate_model(data):
    """Walk-forward or combinatorial purged model validation"""
    from skfolio.preprocessing import prices_to_returns
    from skfolio.optimization import MeanRisk, ObjectiveFunction
    from skfolio import RiskMeasure
    from skfolio.model_selection import WalkForward, CombinatorialPurgedCV, cross_val_predict

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    validation_method = data.get("validation_method", "walk_forward")
    risk_measure = data.get("risk_measure", "variance")

    risk_map = {
        "variance": RiskMeasure.VARIANCE,
        "cvar": RiskMeasure.CVAR,
        "semi_variance": RiskMeasure.SEMI_VARIANCE,
    }
    risk_enum = risk_map.get(risk_measure, RiskMeasure.VARIANCE)

    model = MeanRisk(
        risk_measure=risk_enum,
        objective_function=ObjectiveFunction.MAXIMIZE_RATIO,
    )

    if validation_method == "walk_forward":
        cv = WalkForward(train_size=252, test_size=63)
    elif validation_method == "combinatorial_purged":
        cv = CombinatorialPurgedCV(n_folds=5, n_test_folds=2)
    else:
        from sklearn.model_selection import KFold
        cv = KFold(n_splits=5, shuffle=False)

    pred = cross_val_predict(model, returns, cv=cv)

    # Extract fold metrics
    fold_metrics = []
    if hasattr(pred, '__iter__'):
        for p in pred:
            fold_data = {}
            for attr in ["mean", "annualized_mean", "sharpe_ratio", "max_drawdown",
                         "standard_deviation", "sortino_ratio"]:
                try:
                    val = getattr(p, attr, None)
                    if val is not None:
                        fold_data[attr] = float(val)
                except Exception:
                    pass
            fold_metrics.append(fold_data)

    return {
        "status": "success",
        "validation_method": validation_method,
        "n_folds": len(fold_metrics),
        "fold_metrics": fold_metrics,
    }


def op_scenario_analysis(data):
    """Scenario analysis for portfolio"""
    from skfolio.preprocessing import prices_to_returns

    prices = _parse_prices(data)
    returns = prices_to_returns(prices)

    weights_raw = _parse_weights(data)
    if weights_raw and isinstance(weights_raw, dict):
        w = np.array([weights_raw.get(col, 0) for col in returns.columns])
    elif weights_raw and isinstance(weights_raw, list):
        w = np.array(weights_raw)
    else:
        w = np.ones(len(returns.columns)) / len(returns.columns)

    scenarios_raw = data.get("scenarios")
    if isinstance(scenarios_raw, str):
        scenarios_raw = json.loads(scenarios_raw) if scenarios_raw and scenarios_raw != "null" else None

    if not scenarios_raw:
        scenarios_raw = {
            "bull_market": {"return_shift": 0.10, "vol_multiplier": 0.8},
            "bear_market": {"return_shift": -0.20, "vol_multiplier": 1.5},
            "high_volatility": {"return_shift": 0.0, "vol_multiplier": 2.0},
            "low_volatility": {"return_shift": 0.05, "vol_multiplier": 0.5},
            "stagflation": {"return_shift": -0.10, "vol_multiplier": 1.3},
        }

    port_returns = returns.values @ w
    base_mean = float(np.mean(port_returns) * 252)
    base_vol = float(np.std(port_returns) * np.sqrt(252))

    scenario_results = {}
    for name, params in scenarios_raw.items():
        shifted_ret = base_mean + params.get("return_shift", 0)
        shifted_vol = base_vol * params.get("vol_multiplier", 1.0)
        sharpe = (shifted_ret - 0.02) / shifted_vol if shifted_vol > 0 else 0
        scenario_results[name] = {
            "expected_return": shifted_ret,
            "expected_volatility": shifted_vol,
            "sharpe_ratio": sharpe,
            "return_shift": params.get("return_shift", 0),
            "vol_multiplier": params.get("vol_multiplier", 1.0),
        }

    return {
        "status": "success",
        "base_metrics": {"annualized_return": base_mean, "annualized_volatility": base_vol},
        "scenarios": scenario_results,
    }


def op_generate_report(data):
    """Generate comprehensive portfolio report"""
    optimize_result = op_optimize(data)
    if optimize_result.get("status") != "success":
        return optimize_result

    risk_data = dict(data)
    risk_data["weights"] = optimize_result["weights"]
    risk_result = op_risk_metrics(risk_data)

    attribution_result = op_risk_attribution(risk_data)

    return {
        "status": "success",
        "optimization": optimize_result,
        "risk_metrics": risk_result.get("metrics", {}),
        "risk_attribution": {
            "asset_names": attribution_result.get("asset_names", []),
            "percentage_risk_contribution": attribution_result.get("percentage_risk_contribution", []),
            "component_risk_contribution": attribution_result.get("component_risk_contribution", []),
        },
    }


# ==================== DISPATCH ====================

def dispatch(operation, data):
    ops = {
        "optimize": op_optimize,
        "efficient_frontier": op_efficient_frontier,
        "risk_metrics": op_risk_metrics,
        "stress_test": op_stress_test,
        "backtest": op_backtest,
        "compare_strategies": op_compare_strategies,
        "risk_attribution": op_risk_attribution,
        "hyperparameter_tune": op_hyperparameter_tune,
        "measures": op_measures,
        "validate_model": op_validate_model,
        "scenario_analysis": op_scenario_analysis,
        "generate_report": op_generate_report,
    }
    fn = ops.get(operation)
    if not fn:
        return {"error": f"Unknown operation: {operation}", "available": list(ops.keys())}
    return fn(data)


def main(args):
    if len(args) < 2:
        print(json.dumps({"error": "Usage: worker_handler.py <operation> <json_data>"}))
        return

    operation = args[0]
    try:
        data = json.loads(args[1])
    except json.JSONDecodeError as e:
        print(json.dumps({"error": f"Invalid JSON: {e}"}))
        return

    try:
        result = dispatch(operation, data)
        print(json.dumps(result, default=_serialize))
    except Exception as e:
        print(json.dumps({"error": str(e), "traceback": traceback.format_exc()}))


if __name__ == "__main__":
    main(sys.argv[1:])
