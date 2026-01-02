"""
FFN Service - Python backend for FFN analytics via PyO3
========================================================

Provides JSON-RPC interface for Rust/Tauri to call FFN analytics functions.
"""

import json
import sys
import os
from typing import Dict, List, Any, Optional
from datetime import datetime

# Add this script's directory to Python path for local imports
_script_dir = os.path.dirname(os.path.abspath(__file__))
if _script_dir not in sys.path:
    sys.path.insert(0, _script_dir)

import pandas as pd
import numpy as np

try:
    import ffn
    FFN_AVAILABLE = True
except ImportError:
    FFN_AVAILABLE = False

# Import local module after path fix
try:
    from ffn_analytics import FFNAnalyticsEngine, FFNConfig
except ImportError as e:
    # Fallback: define minimal classes if import fails
    FFN_AVAILABLE = False


def parse_prices_json(prices_json: str) -> pd.DataFrame:
    """Parse JSON price data into DataFrame"""
    data = json.loads(prices_json)

    if isinstance(data, dict):
        # Could be {date: price} or {symbol: {date: price}}
        first_value = next(iter(data.values()))
        if isinstance(first_value, dict):
            # Multi-asset: {symbol: {date: price}}
            df = pd.DataFrame(data)
            df.index = pd.to_datetime(df.index)
        else:
            # Single asset: {date: price}
            df = pd.DataFrame({'price': data})
            df.index = pd.to_datetime(df.index)
            df = df['price']  # Return as Series
    elif isinstance(data, list):
        # Array format: [{date, price, symbol?}, ...]
        df = pd.DataFrame(data)
        if 'date' in df.columns:
            df['date'] = pd.to_datetime(df['date'])
            df.set_index('date', inplace=True)
        if 'symbol' in df.columns:
            # Pivot to multi-asset
            df = df.pivot(columns='symbol', values='price')
        elif 'price' in df.columns:
            df = df['price']
    else:
        raise ValueError(f"Unsupported price data format: {type(data)}")

    df = df.sort_index()
    return df


def parse_config(config_json: Optional[str]) -> FFNConfig:
    """Parse JSON config into FFNConfig"""
    if not config_json:
        return FFNConfig()

    config_data = json.loads(config_json)
    return FFNConfig(
        risk_free_rate=config_data.get('risk_free_rate', 0.0),
        annualization_factor=config_data.get('annualization_factor', 252),
        rebase_value=config_data.get('rebase_value', 100),
        log_returns=config_data.get('log_returns', False),
        drawdown_threshold=config_data.get('drawdown_threshold', 0.10),
    )


def serialize_result(data: Any) -> str:
    """Serialize result to JSON, handling numpy/pandas types"""
    def convert(obj):
        # Handle None first
        if obj is None:
            return None

        # Handle numpy types
        if isinstance(obj, (np.integer, np.floating)):
            if np.isnan(obj):
                return None
            return float(obj)
        elif isinstance(obj, np.ndarray):
            return obj.tolist()
        elif isinstance(obj, np.bool_):
            return bool(obj)

        # Handle pandas types
        elif isinstance(obj, pd.Series):
            return {str(k): convert(v) for k, v in obj.items()}
        elif isinstance(obj, pd.DataFrame):
            return {str(col): {str(idx): convert(val) for idx, val in obj[col].items()}
                    for col in obj.columns}
        elif isinstance(obj, (datetime, pd.Timestamp)):
            return obj.isoformat()

        # Handle NaN/NaT - must check scalar first to avoid array truth value error
        elif isinstance(obj, float) and np.isnan(obj):
            return None

        # Handle dicts and lists
        elif isinstance(obj, dict):
            return {k: convert(v) for k, v in obj.items()}
        elif isinstance(obj, list):
            return [convert(v) for v in obj]

        # Return as-is for other types
        return obj

    return json.dumps(convert(data), indent=2)


# ============================================================================
# COMMAND HANDLERS
# ============================================================================

def check_status() -> Dict[str, Any]:
    """Check FFN library availability and version"""
    return {
        "success": True,
        "available": FFN_AVAILABLE,
        "version": ffn.__version__ if FFN_AVAILABLE else None,
        "message": "FFN analytics ready" if FFN_AVAILABLE else "FFN library not installed"
    }


def calculate_performance(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate comprehensive performance statistics"""
    if not FFN_AVAILABLE:
        return {"success": False, "error": "FFN library not available"}

    try:
        prices = parse_prices_json(params['prices'])
        config = parse_config(params.get('config'))

        engine = FFNAnalyticsEngine(config)
        engine.load_data(prices)

        stats = engine.calculate_performance_stats()

        return {
            "success": True,
            "metrics": stats,
            "data_points": len(prices),
            "date_range": {
                "start": str(prices.index[0]),
                "end": str(prices.index[-1])
            }
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def calculate_drawdowns(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate drawdown analysis with details"""
    if not FFN_AVAILABLE:
        return {"success": False, "error": "FFN library not available"}

    try:
        prices = parse_prices_json(params['prices'])
        threshold = params.get('threshold', 0.10)

        config = FFNConfig(drawdown_threshold=threshold)
        engine = FFNAnalyticsEngine(config)
        engine.load_data(prices)

        # Get drawdown series
        if isinstance(prices, pd.DataFrame):
            prices = prices.iloc[:, 0]  # Use first column for drawdown

        dd_series = ffn.to_drawdown_series(prices)
        dd_details = engine.calculate_drawdown_analysis(prices)

        # Format drawdown details - check size instead of truthiness
        drawdowns = []
        if dd_details is not None and hasattr(dd_details, 'size') and dd_details.size > 0:
            for _, row in dd_details.iterrows():
                drawdowns.append({
                    "start": str(row.get('Start', row.get('start', ''))),
                    "end": str(row.get('End', row.get('end', ''))),
                    "length": int(row.get('Length', row.get('length', 0))) if not pd.isna(row.get('Length', row.get('length'))) else 0,
                    "drawdown": float(row.get('drawdown', 0)),
                })

        return {
            "success": True,
            "max_drawdown": float(ffn.calc_max_drawdown(prices)),
            "current_drawdown": float(dd_series.iloc[-1]) if len(dd_series) > 0 else 0,
            "drawdowns": drawdowns,
            "drawdown_series": {str(k): float(v) for k, v in dd_series.tail(100).items()}
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def calculate_rolling_metrics(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate rolling performance metrics"""
    if not FFN_AVAILABLE:
        return {"success": False, "error": "FFN library not available"}

    try:
        prices = parse_prices_json(params['prices'])
        window = params.get('window', 252)
        metrics = params.get('metrics', ['sharpe', 'volatility', 'returns'])

        engine = FFNAnalyticsEngine()
        engine.load_data(prices)

        rolling = engine.calculate_rolling_metrics(window=window, metrics=metrics)

        # Convert to serializable format (last 252 points for each metric)
        result = {}
        for key, series in rolling.items():
            if isinstance(series, pd.DataFrame):
                result[key] = {col: {str(idx): float(val) for idx, val in series[col].dropna().tail(252).items()}
                              for col in series.columns}
            else:
                result[key] = {str(k): float(v) for k, v in series.dropna().tail(252).items()}

        return {
            "success": True,
            "window": window,
            "metrics": result
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def monthly_returns(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate monthly returns table"""
    if not FFN_AVAILABLE:
        return {"success": False, "error": "FFN library not available"}

    try:
        prices = parse_prices_json(params['prices'])

        if isinstance(prices, pd.DataFrame):
            prices = prices.iloc[:, 0]

        engine = FFNAnalyticsEngine()
        engine.load_data(prices)

        monthly = engine.calculate_monthly_returns(prices)

        # Convert to nested dict format
        result = {}
        for year in monthly.index:
            result[str(year)] = {}
            for month in monthly.columns:
                val = monthly.loc[year, month]
                result[str(year)][str(month)] = float(val) if not pd.isna(val) else None

        return {
            "success": True,
            "monthly_returns": result,
            "years": list(monthly.index.astype(str))
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def rebase_prices(params: Dict[str, Any]) -> Dict[str, Any]:
    """Rebase prices to a starting value"""
    if not FFN_AVAILABLE:
        return {"success": False, "error": "FFN library not available"}

    try:
        prices = parse_prices_json(params['prices'])
        base_value = params.get('base_value', 100.0)

        rebased = ffn.rebase(prices, base_value)

        if isinstance(rebased, pd.DataFrame):
            result = {col: {str(idx): float(val) for idx, val in rebased[col].items()}
                     for col in rebased.columns}
        else:
            result = {str(k): float(v) for k, v in rebased.items()}

        return {
            "success": True,
            "base_value": base_value,
            "rebased_prices": result
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def compare_assets(params: Dict[str, Any]) -> Dict[str, Any]:
    """Compare multiple assets performance"""
    if not FFN_AVAILABLE:
        return {"success": False, "error": "FFN library not available"}

    try:
        prices = parse_prices_json(params['prices'])
        benchmark = params.get('benchmark')
        rf = params.get('risk_free_rate', 0.0)

        if not isinstance(prices, pd.DataFrame):
            return {"success": False, "error": "Multiple assets required for comparison"}

        config = FFNConfig(risk_free_rate=rf)
        engine = FFNAnalyticsEngine(config)
        engine.load_data(prices)

        # Calculate stats for each asset
        all_stats = engine.calculate_performance_stats()

        # Calculate correlation matrix
        returns = ffn.to_returns(prices)
        correlation = returns.corr()

        # Rebase for comparison
        rebased = ffn.rebase(prices, 100)

        return {
            "success": True,
            "asset_stats": all_stats,
            "correlation_matrix": {col: {str(idx): float(val) for idx, val in correlation[col].items()}
                                  for col in correlation.columns},
            "rebased_performance": {col: {str(idx): float(val) for idx, val in rebased[col].items()}
                                   for col in rebased.columns},
            "benchmark": benchmark
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def risk_metrics(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate risk metrics"""
    if not FFN_AVAILABLE:
        return {"success": False, "error": "FFN library not available"}

    try:
        prices = parse_prices_json(params['prices'])
        rf = params.get('risk_free_rate', 0.0)

        if isinstance(prices, pd.DataFrame):
            prices = prices.iloc[:, 0]

        engine = FFNAnalyticsEngine(FFNConfig(risk_free_rate=rf))
        engine.load_data(prices)

        returns = engine.returns

        # Helper to safely convert to float
        def safe_float(val):
            if val is None:
                return None
            try:
                if hasattr(val, 'size') and val.size == 0:
                    return None
                if pd.isna(val):
                    return None
                return float(val)
            except (TypeError, ValueError):
                return None

        # Calculate various risk metrics with error handling
        ulcer_index = None
        try:
            ulcer_index = safe_float(engine.to_ulcer_index(prices))
        except Exception:
            pass

        ulcer_perf = None
        try:
            ulcer_perf = safe_float(engine.to_ulcer_performance_index(prices, rf))
        except Exception:
            pass

        # Calculate skewness and kurtosis safely
        skewness = None
        kurtosis = None
        try:
            skew_val = returns.skew()
            skewness = safe_float(skew_val)
        except Exception:
            pass
        try:
            kurt_val = returns.kurtosis()
            kurtosis = safe_float(kurt_val)
        except Exception:
            pass

        # Calculate VaR and CVaR safely
        var_95 = None
        cvar_95 = None
        try:
            if returns is not None and hasattr(returns, 'size') and returns.size > 0:
                var_threshold = returns.quantile(0.05)
                var_95 = safe_float(var_threshold)
                tail_returns = returns[returns <= var_threshold]
                if hasattr(tail_returns, 'size') and tail_returns.size > 0:
                    cvar_95 = safe_float(tail_returns.mean())
        except Exception:
            pass

        # Calculate max drawdown safely
        max_dd = None
        try:
            max_dd = safe_float(ffn.calc_max_drawdown(prices))
        except Exception:
            pass

        # Calculate volatility safely
        daily_vol = None
        annual_vol = None
        try:
            if returns is not None and hasattr(returns, 'std'):
                std_val = returns.std()
                daily_vol = safe_float(std_val)
                if daily_vol is not None:
                    annual_vol = daily_vol * np.sqrt(252)
        except Exception:
            pass

        return {
            "success": True,
            "ulcer_index": ulcer_index,
            "ulcer_performance_index": ulcer_perf,
            "skewness": skewness,
            "kurtosis": kurtosis,
            "var_95": var_95,
            "cvar_95": cvar_95,
            "max_drawdown": max_dd,
            "daily_vol": daily_vol,
            "annual_vol": annual_vol
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def full_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Full portfolio analysis combining all metrics"""
    if not FFN_AVAILABLE:
        return {"success": False, "error": "FFN library not available"}

    try:
        prices = parse_prices_json(params['prices'])
        config = parse_config(params.get('config'))

        engine = FFNAnalyticsEngine(config)
        engine.load_data(prices)

        # Get all metrics
        perf_stats = engine.calculate_performance_stats()

        # Handle single vs multi-asset
        if isinstance(prices, pd.DataFrame):
            single_prices = prices.iloc[:, 0]
        else:
            single_prices = prices

        # Drawdown analysis with error handling
        dd_details = None
        try:
            dd_details = engine.calculate_drawdown_analysis(single_prices)
        except Exception:
            pass

        # Rolling metrics - only if we have enough data points
        rolling = {}
        data_points = len(prices)
        if data_points >= 20:  # Minimum threshold for rolling metrics
            try:
                # Use smaller window if not enough data
                window = min(252, data_points - 1)
                rolling = engine.calculate_rolling_metrics(window=window)
            except Exception:
                pass

        # Monthly returns with error handling
        monthly = None
        try:
            monthly = engine.calculate_monthly_returns(single_prices)
        except Exception:
            pass

        # Drawdowns list - check size instead of truthiness
        drawdowns = []
        if dd_details is not None and hasattr(dd_details, 'size') and dd_details.size > 0:
            try:
                for _, row in dd_details.head(5).iterrows():
                    drawdowns.append({
                        "start": str(row.get('Start', row.get('start', ''))),
                        "end": str(row.get('End', row.get('end', ''))),
                        "drawdown": float(row.get('drawdown', 0)),
                    })
            except Exception:
                pass

        # Build monthly returns dict safely
        monthly_returns_dict = {}
        if monthly is not None and hasattr(monthly, 'iterrows'):
            try:
                for year, row in monthly.iterrows():
                    monthly_returns_dict[str(year)] = {}
                    for m, v in row.items():
                        try:
                            monthly_returns_dict[str(year)][str(m)] = float(v) if not pd.isna(v) else None
                        except (TypeError, ValueError):
                            monthly_returns_dict[str(year)][str(m)] = None
            except Exception:
                pass

        # Build rolling sharpe dict safely
        rolling_sharpe_dict = {}
        if 'rolling_sharpe' in rolling:
            try:
                rs = rolling['rolling_sharpe']
                if hasattr(rs, 'dropna'):
                    rs = rs.dropna()
                    if hasattr(rs, 'size') and rs.size > 0:
                        for k, v in rs.tail(50).items():
                            try:
                                rolling_sharpe_dict[str(k)] = float(v)
                            except (TypeError, ValueError):
                                pass
            except Exception:
                pass

        # Calculate max drawdown safely
        max_dd = None
        try:
            max_dd = float(ffn.calc_max_drawdown(single_prices))
        except Exception:
            max_dd = 0.0

        return {
            "success": True,
            "performance": perf_stats,
            "drawdowns": {
                "max_drawdown": max_dd,
                "top_drawdowns": drawdowns
            },
            "monthly_returns": monthly_returns_dict,
            "rolling_sharpe": rolling_sharpe_dict,
            "data_summary": {
                "data_points": data_points,
                "start_date": str(prices.index[0]),
                "end_date": str(prices.index[-1]),
                "assets": list(prices.columns) if isinstance(prices, pd.DataFrame) else ["single_asset"]
            }
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

COMMANDS = {
    "check_status": lambda _: check_status(),
    "calculate_performance": calculate_performance,
    "calculate_drawdowns": calculate_drawdowns,
    "calculate_rolling_metrics": calculate_rolling_metrics,
    "monthly_returns": monthly_returns,
    "rebase_prices": rebase_prices,
    "compare_assets": compare_assets,
    "risk_metrics": risk_metrics,
    "full_analysis": full_analysis,
}


def main(args: list) -> str:
    """Main entry point called by Rust via PyO3"""
    try:
        if len(args) < 1:
            return serialize_result({"success": False, "error": "No command specified"})

        command = args[0]

        if command not in COMMANDS:
            return serialize_result({"success": False, "error": f"Unknown command: {command}"})

        # Parse params if provided
        params = {}
        if len(args) > 1:
            params = json.loads(args[1])

        result = COMMANDS[command](params)
        return serialize_result(result)

    except Exception as e:
        return serialize_result({"success": False, "error": str(e)})


if __name__ == "__main__":
    output = main(sys.argv[1:])
    print(output)
