
"""Economic Reporting Module
=============================

Economic analysis reporting and visualization

===== DATA SOURCES REQUIRED =====
INPUT:
  - Macroeconomic time series data from official sources
  - Central bank policy statements and interest rate data
  - International trade and balance of payments statistics
  - Market indicators and sentiment measures
  - Demographic and structural economic data

OUTPUT:
  - Economic trend analysis and forecasts
  - Policy impact assessment and scenario modeling
  - Market cycle identification and timing analysis
  - Cross-country economic comparisons and rankings
  - Investment recommendations based on economic outlook

PARAMETERS:
  - forecast_horizon: Economic forecast horizon (default: 12 months)
  - confidence_level: Confidence level for predictions (default: 0.90)
  - base_currency: Base currency for analysis (default: 'USD')
  - seasonal_adjustment: Seasonal adjustment method (default: true)
  - lookback_period: Historical analysis period (default: 10 years)
"""



import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from decimal import Decimal
from typing import Dict, List, Optional, Any, Union
from datetime import datetime
import json
import warnings
from .core import EconomicsBase, ValidationError

warnings.filterwarnings('ignore')
plt.style.use('seaborn-v0_8')


class VisualizationEngine(EconomicsBase):
    """Advanced visualization for economic analysis"""

    def __init__(self, precision: int = 8):
        super().__init__(precision)
        self.default_figsize = (12, 8)
        self.color_palette = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']

    def plot_time_series(self, data: pd.DataFrame, title: str = "Time Series Analysis",
                         figsize: tuple = None) -> Dict[str, Any]:
        """Create professional time series plots"""

        if not isinstance(data.index, pd.DatetimeIndex):
            raise ValidationError("Data must have datetime index")

        figsize = figsize or self.default_figsize
        fig, axes = plt.subplots(2, 2, figsize=(figsize[0], figsize[1] * 1.2))
        fig.suptitle(title, fontsize=16, fontweight='bold')

        # Main time series plot
        ax1 = axes[0, 0]
        for i, col in enumerate(data.columns[:5]):  # Max 5 series
            ax1.plot(data.index, data[col], label=col,
                     color=self.color_palette[i % len(self.color_palette)], linewidth=2)
        ax1.set_title('Time Series Data')
        ax1.legend()
        ax1.grid(True, alpha=0.3)

        # Returns plot
        ax2 = axes[0, 1]
        returns = data.pct_change().dropna()
        if not returns.empty:
            ax2.plot(returns.index, returns.iloc[:, 0],
                     color=self.color_palette[0], linewidth=1, alpha=0.7)
            ax2.set_title('Returns')
            ax2.grid(True, alpha=0.3)

        # Distribution plot
        ax3 = axes[1, 0]
        if not returns.empty:
            ax3.hist(returns.iloc[:, 0].dropna(), bins=30, alpha=0.7,
                     color=self.color_palette[0], edgecolor='black')
            ax3.set_title('Returns Distribution')
            ax3.set_xlabel('Returns')
            ax3.set_ylabel('Frequency')

        # Autocorrelation plot
        ax4 = axes[1, 1]
        try:
            from statsmodels.tsa.stattools import acf
            if len(data.iloc[:, 0].dropna()) > 20:
                lags = min(20, len(data) // 4)
                autocorr = acf(data.iloc[:, 0].dropna(), nlags=lags)
                ax4.bar(range(len(autocorr)), autocorr, alpha=0.7, color=self.color_palette[0])
                ax4.axhline(y=0, color='black', linestyle='-', alpha=0.3)
                ax4.set_title('Autocorrelation')
                ax4.set_xlabel('Lags')
        except ImportError:
            ax4.text(0.5, 0.5, 'Autocorrelation\nrequires statsmodels',
                     ha='center', va='center', transform=ax4.transAxes)

        plt.tight_layout()
        return {'figure': fig, 'plot_type': 'time_series'}

    def plot_correlation_matrix(self, corr_matrix: pd.DataFrame,
                                title: str = "Correlation Matrix") -> Dict[str, Any]:
        """Create correlation heatmap"""

        fig, ax = plt.subplots(figsize=self.default_figsize)

        # Create heatmap
        sns.heatmap(corr_matrix, annot=True, cmap='RdYlBu_r', center=0,
                    square=True, fmt='.2f', cbar_kws={'label': 'Correlation'})

        ax.set_title(title, fontsize=16, fontweight='bold', pad=20)
        plt.tight_layout()

        return {'figure': fig, 'plot_type': 'correlation_heatmap'}

    def plot_economic_indicators(self, data: Dict[str, pd.Series],
                                 title: str = "Economic Indicators") -> Dict[str, Any]:
        """Plot multiple economic indicators with subplots"""

        n_indicators = len(data)
        if n_indicators == 0:
            raise ValidationError("No data provided")

        # Determine subplot layout
        if n_indicators <= 2:
            rows, cols = 1, n_indicators
        elif n_indicators <= 4:
            rows, cols = 2, 2
        else:
            rows, cols = 3, 3

        fig, axes = plt.subplots(rows, cols, figsize=(cols * 6, rows * 4))
        if n_indicators == 1:
            axes = [axes]
        elif rows == 1 or cols == 1:
            axes = axes.flatten()
        else:
            axes = axes.flatten()

        fig.suptitle(title, fontsize=16, fontweight='bold')

        for i, (indicator, series) in enumerate(data.items()):
            if i >= len(axes):
                break

            ax = axes[i]
            ax.plot(series.index, series.values,
                    color=self.color_palette[i % len(self.color_palette)], linewidth=2)
            ax.set_title(indicator)
            ax.grid(True, alpha=0.3)

            # Add trend line
            if len(series) > 2:
                z = np.polyfit(range(len(series)), series.values, 1)
                trend = np.poly1d(z)
                ax.plot(series.index, trend(range(len(series))),
                        '--', alpha=0.7, color='red', linewidth=1)

        # Hide unused subplots
        for i in range(n_indicators, len(axes)):
            axes[i].set_visible(False)

        plt.tight_layout()
        return {'figure': fig, 'plot_type': 'economic_indicators'}

    def plot_forecast(self, historical: pd.Series, forecast: List[float],
                      confidence_intervals: Optional[Dict[str, List[float]]] = None,
                      title: str = "Forecast Analysis") -> Dict[str, Any]:
        """Plot forecast with confidence intervals"""

        fig, ax = plt.subplots(figsize=self.default_figsize)

        # Plot historical data
        ax.plot(historical.index, historical.values,
                label='Historical', color=self.color_palette[0], linewidth=2)

        # Create forecast index
        last_date = historical.index[-1]
        if isinstance(last_date, pd.Timestamp):
            freq = pd.infer_freq(historical.index) or 'D'
            forecast_index = pd.date_range(start=last_date + pd.Timedelta(freq),
                                           periods=len(forecast), freq=freq)
        else:
            forecast_index = range(len(historical), len(historical) + len(forecast))

        # Plot forecast
        ax.plot(forecast_index, forecast,
                label='Forecast', color=self.color_palette[1], linewidth=2, linestyle='--')

        # Plot confidence intervals
        if confidence_intervals:
            lower = confidence_intervals.get('lower', [])
            upper = confidence_intervals.get('upper', [])
            if len(lower) == len(forecast) and len(upper) == len(forecast):
                ax.fill_between(forecast_index, lower, upper,
                                alpha=0.3, color=self.color_palette[1], label='95% CI')

        ax.set_title(title, fontsize=16, fontweight='bold')
        ax.legend()
        ax.grid(True, alpha=0.3)
        plt.tight_layout()

        return {'figure': fig, 'plot_type': 'forecast'}


class ReportGenerator(EconomicsBase):
    """Generate comprehensive analysis reports"""

    def __init__(self, precision: int = 8):
        super().__init__(precision)
        self.viz_engine = VisualizationEngine(precision)

    def generate_analysis_report(self, analysis_results: Dict[str, Any],
                                 report_title: str = "Economic Analysis Report") -> Dict[str, Any]:
        """Generate comprehensive analysis report"""

        report = {
            'title': report_title,
            'generated_at': datetime.now().isoformat(),
            'summary': self._generate_executive_summary(analysis_results),
            'sections': {},
            'visualizations': [],
            'recommendations': self._generate_recommendations(analysis_results)
        }

        # Process different analysis types
        for analysis_type, results in analysis_results.items():
            if 'error' in str(results):
                continue

            section = self._create_analysis_section(analysis_type, results)
            if section:
                report['sections'][analysis_type] = section

        return report

    def _generate_executive_summary(self, results: Dict[str, Any]) -> Dict[str, Any]:
        """Generate executive summary from analysis results"""

        summary = {
            'key_findings': [],
            'risk_assessment': 'Not Available',
            'outlook': 'Neutral',
            'confidence_level': 'Medium'
        }

        # Extract key findings from various analyses
        for analysis_type, analysis_results in results.items():
            if isinstance(analysis_results, dict) and 'error' not in analysis_results:

                if 'correlation' in analysis_type.lower():
                    high_corr = analysis_results.get('highest_correlations', [])
                    if high_corr:
                        summary['key_findings'].append(
                            f"Highest correlation: {high_corr[0]['variable_1']} - {high_corr[0]['variable_2']} ({float(high_corr[0]['correlation']):.3f})"
                        )

                elif 'forecast' in analysis_type.lower():
                    forecasts = analysis_results.get('forecasts', {})
                    if forecasts:
                        best_method = analysis_results.get('evaluation', {}).get('best_method', 'Unknown')
                        summary['key_findings'].append(f"Best forecasting method: {best_method}")

                elif 'monte_carlo' in analysis_type.lower():
                    risk_metrics = analysis_results.get('risk_metrics', {})
                    if risk_metrics:
                        var_95 = risk_metrics.get('var_95')
                        if var_95:
                            summary['key_findings'].append(f"95% VaR: {float(var_95):.2f}")

        return summary

    def _create_analysis_section(self, analysis_type: str, results: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Create report section for specific analysis type"""

        section = {
            'title': analysis_type.replace('_', ' ').title(),
            'content': {},
            'key_metrics': {},
            'interpretation': ''
        }

        if 'statistical' in analysis_type:
            stats = results.get('basic_statistics', {})
            section['key_metrics'] = {
                'Mean': stats.get('mean'),
                'Std Dev': stats.get('standard_deviation'),
                'Skewness': stats.get('skewness'),
                'Kurtosis': stats.get('kurtosis')
            }
            section['interpretation'] = self._interpret_statistics(stats)

        elif 'correlation' in analysis_type:
            section['key_metrics'] = {
                'Mean Correlation': results.get('summary_statistics', {}).get('mean_correlation'),
                'Max Correlation': results.get('summary_statistics', {}).get('max_correlation'),
                'Significant Pairs': len(results.get('significant_correlations', []))
            }

        elif 'forecast' in analysis_type:
            evaluation = results.get('evaluation', {})
            if evaluation:
                best_method = evaluation.get('best_method', 'Unknown')
                section['key_metrics'] = {
                    'Best Method': best_method,
                    'Forecast Periods': results.get('forecast_periods', 0)
                }

        section['content'] = results
        return section

    def _interpret_statistics(self, stats: Dict[str, Any]) -> str:
        """Generate interpretation of statistical results"""

        interpretations = []

        skewness = stats.get('skewness')
        if skewness:
            skew_val = float(skewness)
            if abs(skew_val) < 0.5:
                interpretations.append("Distribution is approximately symmetric")
            elif skew_val > 0.5:
                interpretations.append("Distribution is positively skewed (right tail)")
            else:
                interpretations.append("Distribution is negatively skewed (left tail)")

        kurtosis = stats.get('kurtosis')
        if kurtosis:
            kurt_val = float(kurtosis)
            if kurt_val > 3:
                interpretations.append("Distribution has heavy tails (leptokurtic)")
            elif kurt_val < 3:
                interpretations.append("Distribution has light tails (platykurtic)")

        return ". ".join(interpretations) + "." if interpretations else "No specific interpretation available."

    def _generate_recommendations(self, results: Dict[str, Any]) -> List[str]:
        """Generate actionable recommendations"""

        recommendations = []

        for analysis_type, analysis_results in results.items():
            if isinstance(analysis_results, dict) and 'error' not in analysis_results:

                if 'risk' in analysis_type.lower():
                    recommendations.append("Monitor risk metrics regularly and adjust exposure accordingly")

                elif 'correlation' in analysis_type.lower():
                    recommendations.append("Consider correlation relationships for portfolio diversification")

                elif 'forecast' in analysis_type.lower():
                    recommendations.append("Use multiple forecasting methods and update predictions regularly")

        if not recommendations:
            recommendations = ["Continue monitoring economic indicators and market conditions"]

        return recommendations


class ExportManager(EconomicsBase):
    """Export analysis results to various formats"""

    def __init__(self, precision: int = 8):
        super().__init__(precision)

    def export_to_json(self, data: Dict[str, Any], file_path: str = None) -> str:
        """Export results to JSON format"""

        # Convert Decimal objects to float for JSON serialization
        json_data = self._prepare_for_json(data)

        json_str = json.dumps(json_data, indent=2, default=self._json_serializer)

        if file_path:
            with open(file_path, 'w') as f:
                f.write(json_str)

        return json_str

    def export_to_excel(self, data: Dict[str, Any], file_path: str) -> bool:
        """Export results to Excel format"""

        try:
            with pd.ExcelWriter(file_path, engine='openpyxl') as writer:

                # Export summary data
                summary_data = []
                for key, value in data.items():
                    if isinstance(value, (str, int, float, Decimal)):
                        summary_data.append(
                            {'Metric': key, 'Value': float(value) if isinstance(value, Decimal) else value})

                if summary_data:
                    pd.DataFrame(summary_data).to_excel(writer, sheet_name='Summary', index=False)

                # Export detailed data
                for section_name, section_data in data.items():
                    if isinstance(section_data, dict):
                        try:
                            df = pd.DataFrame(section_data)
                            if not df.empty:
                                # Clean sheet name
                                sheet_name = section_name.replace('_', ' ').title()[:31]
                                df.to_excel(writer, sheet_name=sheet_name, index=True)
                        except:
                            continue

            return True

        except Exception as e:
            raise ValidationError(f"Error exporting to Excel: {e}")

    def _prepare_for_json(self, obj: Any) -> Any:
        """Prepare object for JSON serialization"""

        if isinstance(obj, dict):
            return {key: self._prepare_for_json(value) for key, value in obj.items()}
        elif isinstance(obj, list):
            return [self._prepare_for_json(item) for item in obj]
        elif isinstance(obj, Decimal):
            return float(obj)
        elif isinstance(obj, (pd.Timestamp, datetime)):
            return obj.isoformat()
        elif isinstance(obj, pd.Series):
            return obj.to_dict()
        elif isinstance(obj, pd.DataFrame):
            return obj.to_dict('records')
        else:
            return obj

    def _json_serializer(self, obj: Any) -> Any:
        """Custom JSON serializer for special objects"""

        if isinstance(obj, (pd.Timestamp, datetime)):
            return obj.isoformat()
        elif isinstance(obj, Decimal):
            return float(obj)
        elif hasattr(obj, 'tolist'):  # numpy arrays
            return obj.tolist()
        else:
            return str(obj)

    def generate_pdf_summary(self, analysis_report: Dict[str, Any],
                             file_path: str = None) -> str:
        """Generate PDF summary report"""

        # Simple text-based summary for PDF generation
        summary_text = f"""
ECONOMIC ANALYSIS REPORT
{analysis_report.get('title', 'Analysis Report')}
Generated: {analysis_report.get('generated_at', 'Unknown')}

EXECUTIVE SUMMARY
{'-' * 50}
"""

        summary = analysis_report.get('summary', {})

        # Key findings
        findings = summary.get('key_findings', [])
        if findings:
            summary_text += "\nKey Findings:\n"
            for finding in findings:
                summary_text += f"• {finding}\n"

        # Risk assessment
        risk = summary.get('risk_assessment', 'Not Available')
        summary_text += f"\nRisk Assessment: {risk}\n"

        # Outlook
        outlook = summary.get('outlook', 'Neutral')
        summary_text += f"Outlook: {outlook}\n"

        # Recommendations
        recommendations = analysis_report.get('recommendations', [])
        if recommendations:
            summary_text += f"\nRECOMMENDATIONS\n{'-' * 50}\n"
            for i, rec in enumerate(recommendations, 1):
                summary_text += f"{i}. {rec}\n"

        if file_path:
            with open(file_path, 'w') as f:
                f.write(summary_text)

        return summary_text

    def calculate(self, export_type: str, **kwargs) -> Any:
        """Main export dispatcher"""

        exports = {
            'json': lambda: self.export_to_json(kwargs['data'], kwargs.get('file_path')),
            'excel': lambda: self.export_to_excel(kwargs['data'], kwargs['file_path']),
            'pdf_summary': lambda: self.generate_pdf_summary(kwargs['data'], kwargs.get('file_path'))
        }

        if export_type not in exports:
            raise ValidationError(f"Unknown export type: {export_type}")

        return exports[export_type]()