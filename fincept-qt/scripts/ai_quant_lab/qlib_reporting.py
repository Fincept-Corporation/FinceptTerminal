"""
AI Quant Lab - Reporting & Visualization Module
Analysis and visualization tools for backtest results

Features:
- Position Analysis Reports
- IC Analysis Graphs
- Cumulative Return Visualization
- Risk Analysis Charts
- Model Performance Reports
- Factor Analysis Visualization
"""

import json
import sys
from typing import Dict, List, Any, Optional, Union, Tuple
from datetime import datetime
import warnings
warnings.filterwarnings('ignore')

try:
    import pandas as pd
    import numpy as np
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False
    pd = None
    np = None

# Plotting libraries
PLOTLY_AVAILABLE = False
MATPLOTLIB_AVAILABLE = False

try:
    import plotly.graph_objects as go
    import plotly.express as px
    from plotly.subplots import make_subplots
    PLOTLY_AVAILABLE = True
except ImportError:
    pass

try:
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    pass


class ReportingService:
    """
    Comprehensive reporting and visualization service.
    """

    def __init__(self):
        self.reports = {}

    def generate_position_analysis_report(self,
                                         positions: pd.DataFrame,
                                         returns: pd.DataFrame,
                                         benchmark_returns: Optional[pd.Series] = None) -> Dict[str, Any]:
        """
        Generate position analysis report.

        Args:
            positions: Position weights over time (datetime x instruments)
            returns: Asset returns (datetime x instruments)
            benchmark_returns: Benchmark returns (optional)

        Returns:
            Complete position analysis report
        """
        if not PANDAS_AVAILABLE:
            return {"success": False, "error": "Pandas not available"}

        try:
            # Calculate portfolio returns
            portfolio_returns = (positions.shift(1) * returns).sum(axis=1)

            # Basic metrics
            total_return = (1 + portfolio_returns).prod() - 1
            annual_return = (1 + portfolio_returns).mean() * 252
            volatility = portfolio_returns.std() * np.sqrt(252)
            sharpe = annual_return / volatility if volatility > 0 else 0

            # Drawdown analysis
            cumulative = (1 + portfolio_returns).cumprod()
            running_max = cumulative.expanding().max()
            drawdown = (cumulative - running_max) / running_max
            max_drawdown = drawdown.min()

            report = {
                "success": True,
                "performance_metrics": {
                    "total_return": float(total_return),
                    "annual_return": float(annual_return),
                    "volatility": float(volatility),
                    "sharpe_ratio": float(sharpe),
                    "max_drawdown": float(max_drawdown)
                },
                "portfolio_stats": {
                    "num_positions": int(positions.shape[1]),
                    "avg_num_holdings": float((positions > 0).sum(axis=1).mean()),
                    "avg_position_size": float(positions[positions > 0].mean().mean()),
                    "concentration": float(positions.max(axis=1).mean())
                }
            }

            # Benchmark comparison if provided
            if benchmark_returns is not None:
                excess_returns = portfolio_returns - benchmark_returns
                tracking_error = excess_returns.std() * np.sqrt(252)
                information_ratio = excess_returns.mean() * 252 / tracking_error if tracking_error > 0 else 0

                report["benchmark_comparison"] = {
                    "excess_return": float(excess_returns.mean() * 252),
                    "tracking_error": float(tracking_error),
                    "information_ratio": float(information_ratio)
                }

            # Generate visualization data
            report["visualization_data"] = {
                "cumulative_returns": cumulative.to_dict(),
                "drawdown_series": drawdown.to_dict(),
                "position_concentration": (positions > 0).sum(axis=1).to_dict()
            }

            return report

        except Exception as e:
            return {"success": False, "error": f"Report generation failed: {str(e)}"}

    def generate_ic_analysis_report(self,
                                    predictions: pd.DataFrame,
                                    returns: pd.DataFrame,
                                    method: str = "both") -> Dict[str, Any]:
        """
        Generate IC analysis report with graphs.

        Args:
            predictions: Model predictions
            returns: Actual returns
            method: 'pearson', 'spearman', or 'both'

        Returns:
            IC analysis report with visualization data
        """
        if not PANDAS_AVAILABLE:
            return {"success": False, "error": "Pandas not available"}

        try:
            from scipy.stats import pearsonr, spearmanr

            common_dates = predictions.index.intersection(returns.index)
            common_instruments = predictions.columns.intersection(returns.columns)

            # Calculate IC for each date
            ic_series = []
            rank_ic_series = []

            for date in common_dates:
                pred_vals = predictions.loc[date, common_instruments].dropna()
                ret_vals = returns.loc[date, common_instruments].dropna()

                common_inst = pred_vals.index.intersection(ret_vals.index)
                if len(common_inst) < 10:
                    continue

                pred = pred_vals[common_inst].values
                ret = ret_vals[common_inst].values

                # Pearson IC
                if method in ['pearson', 'both']:
                    ic, _ = pearsonr(pred, ret)
                    ic_series.append({"date": str(date), "ic": float(ic)})

                # Spearman IC (Rank IC)
                if method in ['spearman', 'both']:
                    rank_ic, _ = spearmanr(pred, ret)
                    rank_ic_series.append({"date": str(date), "rank_ic": float(rank_ic)})

            # Calculate statistics
            if ic_series:
                ic_values = [x["ic"] for x in ic_series]
                ic_mean = np.mean(ic_values)
                ic_std = np.std(ic_values)
                ic_ir = ic_mean / ic_std if ic_std > 0 else 0
            else:
                ic_mean = ic_std = ic_ir = 0

            if rank_ic_series:
                rank_ic_values = [x["rank_ic"] for x in rank_ic_series]
                rank_ic_mean = np.mean(rank_ic_values)
                rank_ic_std = np.std(rank_ic_values)
                rank_ic_ir = rank_ic_mean / rank_ic_std if rank_ic_std > 0 else 0
            else:
                rank_ic_mean = rank_ic_std = rank_ic_ir = 0

            report = {
                "success": True,
                "ic_metrics": {
                    "ic_mean": float(ic_mean),
                    "ic_std": float(ic_std),
                    "icir": float(ic_ir),
                    "ic_positive_rate": float(sum(1 for ic in ic_values if ic > 0) / len(ic_values)) if ic_values else 0
                },
                "rank_ic_metrics": {
                    "rank_ic_mean": float(rank_ic_mean),
                    "rank_ic_std": float(rank_ic_std),
                    "rank_icir": float(rank_ic_ir)
                },
                "visualization_data": {
                    "ic_series": ic_series,
                    "rank_ic_series": rank_ic_series
                }
            }

            return report

        except Exception as e:
            return {"success": False, "error": f"IC analysis failed: {str(e)}"}

    def generate_cumulative_return_graph(self,
                                        returns: pd.Series,
                                        benchmark_returns: Optional[pd.Series] = None,
                                        title: str = "Cumulative Returns") -> Dict[str, Any]:
        """
        Generate cumulative return visualization data.

        Args:
            returns: Portfolio returns
            benchmark_returns: Benchmark returns (optional)
            title: Graph title

        Returns:
            Visualization data
        """
        if not PANDAS_AVAILABLE:
            return {"success": False, "error": "Pandas not available"}

        try:
            cumulative_returns = (1 + returns).cumprod()

            graph_data = {
                "success": True,
                "title": title,
                "dates": [str(d) for d in cumulative_returns.index],
                "portfolio_returns": cumulative_returns.tolist(),
            }

            if benchmark_returns is not None:
                cumulative_benchmark = (1 + benchmark_returns).cumprod()
                graph_data["benchmark_returns"] = cumulative_benchmark.tolist()

            # Plotly graph if available
            if PLOTLY_AVAILABLE:
                fig = go.Figure()

                fig.add_trace(go.Scatter(
                    x=cumulative_returns.index,
                    y=cumulative_returns.values,
                    mode='lines',
                    name='Portfolio',
                    line=dict(color='#2E86DE', width=2)
                ))

                if benchmark_returns is not None:
                    fig.add_trace(go.Scatter(
                        x=cumulative_benchmark.index,
                        y=cumulative_benchmark.values,
                        mode='lines',
                        name='Benchmark',
                        line=dict(color='#EE5A6F', width=2, dash='dash')
                    ))

                fig.update_layout(
                    title=title,
                    xaxis_title="Date",
                    yaxis_title="Cumulative Return",
                    hovermode='x unified',
                    template='plotly_white'
                )

                graph_data["plotly_json"] = fig.to_json()

            return graph_data

        except Exception as e:
            return {"success": False, "error": f"Graph generation failed: {str(e)}"}

    def generate_risk_analysis_graph(self,
                                    returns: pd.Series,
                                    title: str = "Risk Analysis") -> Dict[str, Any]:
        """
        Generate risk analysis visualization.

        Args:
            returns: Portfolio returns
            title: Graph title

        Returns:
            Risk analysis visualization data
        """
        if not PANDAS_AVAILABLE:
            return {"success": False, "error": "Pandas not available"}

        try:
            # Calculate metrics
            cumulative = (1 + returns).cumprod()
            running_max = cumulative.expanding().max()
            drawdown = (cumulative - running_max) / running_max

            # Rolling volatility
            rolling_vol = returns.rolling(window=20).std() * np.sqrt(252)

            graph_data = {
                "success": True,
                "title": title,
                "dates": [str(d) for d in returns.index],
                "drawdown": drawdown.tolist(),
                "rolling_volatility": rolling_vol.tolist(),
                "max_drawdown": float(drawdown.min()),
                "avg_volatility": float(rolling_vol.mean())
            }

            # Plotly graph if available
            if PLOTLY_AVAILABLE:
                fig = make_subplots(
                    rows=2, cols=1,
                    subplot_titles=('Drawdown', 'Rolling Volatility'),
                    vertical_spacing=0.12
                )

                # Drawdown
                fig.add_trace(go.Scatter(
                    x=drawdown.index,
                    y=drawdown.values,
                    mode='lines',
                    name='Drawdown',
                    line=dict(color='#E74C3C', width=2),
                    fill='tozeroy'
                ), row=1, col=1)

                # Rolling volatility
                fig.add_trace(go.Scatter(
                    x=rolling_vol.index,
                    y=rolling_vol.values,
                    mode='lines',
                    name='Rolling Vol (20d)',
                    line=dict(color='#9B59B6', width=2)
                ), row=2, col=1)

                fig.update_layout(
                    title=title,
                    hovermode='x unified',
                    template='plotly_white',
                    height=600
                )

                graph_data["plotly_json"] = fig.to_json()

            return graph_data

        except Exception as e:
            return {"success": False, "error": f"Risk graph generation failed: {str(e)}"}

    def generate_model_performance_report(self,
                                         predictions: pd.DataFrame,
                                         returns: pd.DataFrame,
                                         model_name: str = "Model") -> Dict[str, Any]:
        """
        Generate model performance report.

        Args:
            predictions: Model predictions
            returns: Actual returns
            model_name: Model name

        Returns:
            Model performance report
        """
        try:
            # IC analysis
            ic_report = self.generate_ic_analysis_report(predictions, returns)

            # Quantile analysis
            from qlib_evaluation import EvaluationService
            eval_service = EvaluationService()
            quantile_analysis = eval_service.analyze_factor_returns(predictions, returns, quantiles=5)

            report = {
                "success": True,
                "model_name": model_name,
                "ic_analysis": ic_report,
                "quantile_analysis": quantile_analysis,
                "summary": {
                    "ic": ic_report.get("ic_metrics", {}).get("ic_mean", 0),
                    "icir": ic_report.get("ic_metrics", {}).get("icir", 0),
                    "long_short_sharpe": quantile_analysis.get("long_short_sharpe", 0),
                    "rating": self._get_model_rating(
                        ic_report.get("ic_metrics", {}).get("icir", 0),
                        quantile_analysis.get("long_short_sharpe", 0)
                    )
                }
            }

            return report

        except Exception as e:
            return {"success": False, "error": f"Model performance report failed: {str(e)}"}

    def _get_model_rating(self, icir: float, sharpe: float) -> str:
        """Calculate model rating based on ICIR and Sharpe"""
        score = icir * 0.5 + sharpe * 0.5

        if score >= 2.5:
            return "Excellent"
        elif score >= 1.5:
            return "Good"
        elif score >= 0.8:
            return "Fair"
        elif score >= 0.3:
            return "Poor"
        else:
            return "Very Poor"

    def export_report(self,
                     report_data: Dict[str, Any],
                     format: str = "json",
                     filepath: Optional[str] = None) -> Dict[str, Any]:
        """
        Export report to file.

        Args:
            report_data: Report data to export
            format: Export format ('json', 'html')
            filepath: File path (optional)

        Returns:
            Export result
        """
        try:
            if format == "json":
                if filepath:
                    with open(filepath, 'w') as f:
                        json.dump(report_data, f, indent=2)
                return {
                    "success": True,
                    "format": "json",
                    "filepath": filepath,
                    "data": report_data
                }

            elif format == "html":
                html_content = self._generate_html_report(report_data)
                if filepath:
                    with open(filepath, 'w') as f:
                        f.write(html_content)
                return {
                    "success": True,
                    "format": "html",
                    "filepath": filepath,
                    "html": html_content
                }

            else:
                return {"success": False, "error": f"Unknown format: {format}"}

        except Exception as e:
            return {"success": False, "error": f"Export failed: {str(e)}"}

    def _generate_html_report(self, report_data: Dict[str, Any]) -> str:
        """Generate HTML report"""
        html = f"""
        <!DOCTYPE html>
        <html>
        <head>
            <title>Qlib Analysis Report</title>
            <style>
                body {{ font-family: Arial, sans-serif; margin: 20px; }}
                h1 {{ color: #2E86DE; }}
                h2 {{ color: #555; }}
                table {{ border-collapse: collapse; width: 100%; margin: 20px 0; }}
                th, td {{ border: 1px solid #ddd; padding: 12px; text-align: left; }}
                th {{ background-color: #2E86DE; color: white; }}
                .metric {{ background-color: #f9f9f9; padding: 15px; margin: 10px 0; border-left: 4px solid #2E86DE; }}
            </style>
        </head>
        <body>
            <h1>Qlib Analysis Report</h1>
            <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>

            <h2>Summary</h2>
            <pre>{json.dumps(report_data, indent=2)}</pre>
        </body>
        </html>
        """
        return html


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]
    service = ReportingService()

    try:
        if command == "check_status":
            result = {
                "success": True,
                "pandas_available": PANDAS_AVAILABLE,
                "plotly_available": PLOTLY_AVAILABLE,
                "matplotlib_available": MATPLOTLIB_AVAILABLE
            }
            print(json.dumps(result))
        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
