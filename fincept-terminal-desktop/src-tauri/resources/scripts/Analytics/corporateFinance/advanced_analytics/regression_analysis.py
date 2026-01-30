"""Regression Analysis for M&A Valuation"""
from typing import Dict, Any, List, Optional, Tuple
import numpy as np
from dataclasses import dataclass

@dataclass
class RegressionResult:
    coefficients: np.ndarray
    intercept: float
    r_squared: float
    adjusted_r_squared: float
    std_errors: np.ndarray
    t_statistics: np.ndarray
    p_values: np.ndarray

class MARegression:
    """Regression analysis for M&A valuation and comps"""

    def linear_regression(self, X: np.ndarray, y: np.ndarray) -> RegressionResult:
        """Perform OLS linear regression"""

        n = len(y)
        k = X.shape[1]

        X_with_intercept = np.column_stack([np.ones(n), X])

        beta = np.linalg.lstsq(X_with_intercept, y, rcond=None)[0]

        intercept = beta[0]
        coefficients = beta[1:]

        y_pred = X_with_intercept @ beta
        residuals = y - y_pred

        ss_total = np.sum((y - np.mean(y)) ** 2)
        ss_residual = np.sum(residuals ** 2)

        r_squared = 1 - (ss_residual / ss_total) if ss_total > 0 else 0
        adjusted_r_squared = 1 - ((1 - r_squared) * (n - 1) / (n - k - 1)) if n > k + 1 else 0

        mse = ss_residual / (n - k - 1) if n > k + 1 else 0
        var_beta = mse * np.linalg.inv(X_with_intercept.T @ X_with_intercept)
        std_errors = np.sqrt(np.diag(var_beta))

        t_statistics = beta / std_errors
        from scipy import stats
        p_values = 2 * (1 - stats.t.cdf(np.abs(t_statistics), n - k - 1))

        return RegressionResult(
            coefficients=coefficients,
            intercept=intercept,
            r_squared=r_squared,
            adjusted_r_squared=adjusted_r_squared,
            std_errors=std_errors[1:],
            t_statistics=t_statistics[1:],
            p_values=p_values[1:]
        )

    def multiple_regression_valuation(self, comp_data: List[Dict[str, float]],
                                     subject_metrics: Dict[str, float]) -> Dict[str, Any]:
        """
        Perform multiple regression for comparable company valuation

        comp_data: [{'ev': X, 'revenue': Y, 'ebitda_margin': Z, 'growth': W}, ...]
        subject_metrics: {'revenue': A, 'ebitda_margin': B, 'growth': C}
        """

        if len(comp_data) < 3:
            raise ValueError("Need at least 3 comparable companies")

        y = np.array([comp['ev'] for comp in comp_data])

        feature_names = [k for k in comp_data[0].keys() if k != 'ev']
        X = np.array([[comp[feature] for feature in feature_names] for comp in comp_data])

        result = self.linear_regression(X, y)

        subject_features = np.array([subject_metrics[feature] for feature in feature_names])
        predicted_value = result.intercept + np.dot(result.coefficients, subject_features)

        residual_std = np.std(y - (result.intercept + X @ result.coefficients))
        prediction_interval_95 = 1.96 * residual_std

        return {
            'predicted_value': float(predicted_value),
            'prediction_interval': {
                'lower': float(predicted_value - prediction_interval_95),
                'upper': float(predicted_value + prediction_interval_95)
            },
            'regression_statistics': {
                'r_squared': result.r_squared,
                'adjusted_r_squared': result.adjusted_r_squared,
                'intercept': result.intercept
            },
            'coefficients': {
                feature: {
                    'coefficient': float(result.coefficients[i]),
                    'std_error': float(result.std_errors[i]),
                    't_statistic': float(result.t_statistics[i]),
                    'p_value': float(result.p_values[i]),
                    'significant': result.p_values[i] < 0.05
                }
                for i, feature in enumerate(feature_names)
            },
            'subject_metrics': subject_metrics,
            'num_comparables': len(comp_data)
        }

    def premium_regression(self, deal_data: List[Dict[str, float]]) -> Dict[str, Any]:
        """
        Regression analysis of acquisition premiums

        deal_data: [{'premium': X, 'size': Y, 'leverage': Z, 'growth': W, 'multiple': M}, ...]
        """

        y = np.array([deal['premium'] for deal in deal_data])

        feature_names = [k for k in deal_data[0].keys() if k != 'premium']
        X = np.array([[deal[feature] for feature in feature_names] for deal in deal_data])

        result = self.linear_regression(X, y)

        return {
            'regression_statistics': {
                'r_squared': result.r_squared,
                'adjusted_r_squared': result.adjusted_r_squared,
                'intercept': result.intercept
            },
            'premium_drivers': {
                feature: {
                    'coefficient': float(result.coefficients[i]),
                    'interpretation': f"1 unit increase in {feature} → {result.coefficients[i]:+.2f}% premium change",
                    't_statistic': float(result.t_statistics[i]),
                    'p_value': float(result.p_values[i]),
                    'significant': result.p_values[i] < 0.05
                }
                for i, feature in enumerate(feature_names)
            },
            'num_observations': len(deal_data)
        }

    def multiple_expansion_analysis(self, historical_data: List[Dict[str, float]]) -> Dict[str, Any]:
        """
        Analyze relationship between company metrics and valuation multiples

        historical_data: [{'multiple': X, 'growth': Y, 'margin': Z, 'roe': W}, ...]
        """

        y = np.array([data['multiple'] for data in historical_data])

        feature_names = [k for k in historical_data[0].keys() if k != 'multiple']
        X = np.array([[data[feature] for feature in feature_names] for data in historical_data])

        result = self.linear_regression(X, y)

        return {
            'base_multiple': result.intercept,
            'multiple_drivers': {
                feature: {
                    'impact_per_unit': float(result.coefficients[i]),
                    'std_error': float(result.std_errors[i]),
                    'significance': 'significant' if result.p_values[i] < 0.05 else 'not significant',
                    'p_value': float(result.p_values[i])
                }
                for i, feature in enumerate(feature_names)
            },
            'model_fit': {
                'r_squared': result.r_squared,
                'adjusted_r_squared': result.adjusted_r_squared
            }
        }

    def predict_synergies(self, historical_deals: List[Dict[str, float]],
                         current_deal: Dict[str, float]) -> Dict[str, Any]:
        """
        Predict synergies based on historical deal characteristics

        historical_deals: [{'synergy_pct': X, 'overlap': Y, 'size_ratio': Z}, ...]
        current_deal: {'overlap': A, 'size_ratio': B, ...}
        """

        y = np.array([deal['synergy_pct'] for deal in historical_deals])

        feature_names = [k for k in historical_deals[0].keys() if k != 'synergy_pct']
        X = np.array([[deal[feature] for feature in feature_names] for deal in historical_deals])

        result = self.linear_regression(X, y)

        current_features = np.array([current_deal[feature] for feature in feature_names])
        predicted_synergy_pct = result.intercept + np.dot(result.coefficients, current_features)

        residual_std = np.std(y - (result.intercept + X @ result.coefficients))
        confidence_interval = 1.96 * residual_std

        return {
            'predicted_synergy_pct': float(predicted_synergy_pct),
            'confidence_interval_95': {
                'lower': float(predicted_synergy_pct - confidence_interval),
                'upper': float(predicted_synergy_pct + confidence_interval)
            },
            'model_quality': {
                'r_squared': result.r_squared,
                'prediction_error_std': residual_std
            },
            'synergy_drivers': {
                feature: {
                    'coefficient': float(result.coefficients[i]),
                    'current_value': current_deal[feature]
                }
                for i, feature in enumerate(feature_names)
            }
        }

    def transaction_multiple_regression(self, transactions: List[Dict[str, float]],
                                       metric_name: str = 'ebitda') -> Dict[str, Any]:
        """
        Regression to determine fair transaction multiple

        transactions: [{'ev_to_metric': X, 'growth': Y, 'margin': Z, 'size': W}, ...]
        """

        y = np.array([txn['ev_to_metric'] for txn in transactions])

        feature_names = [k for k in transactions[0].keys() if k != 'ev_to_metric']
        X = np.array([[txn[feature] for feature in feature_names] for txn in transactions])

        result = self.linear_regression(X, y)

        avg_growth = np.mean([txn['growth'] for txn in transactions])
        avg_margin = np.mean([txn['margin'] for txn in transactions])
        avg_size = np.mean([txn['size'] for txn in transactions])

        median_company = {
            'growth': avg_growth,
            'margin': avg_margin,
            'size': avg_size
        }

        median_features = np.array([median_company.get(f, np.mean(X[:, i])) for i, f in enumerate(feature_names)])
        median_multiple = result.intercept + np.dot(result.coefficients, median_features)

        return {
            'base_multiple': result.intercept,
            'median_market_multiple': float(median_multiple),
            'multiple_adjustments': {
                feature: {
                    'per_unit_change': float(result.coefficients[i]),
                    'example': f"1% higher {feature} → {result.coefficients[i]:+.2f}x multiple change"
                }
                for i, feature in enumerate(feature_names)
            },
            'model_statistics': {
                'r_squared': result.r_squared,
                'num_transactions': len(transactions)
            }
        }

if __name__ == '__main__':
    analyzer = MARegression()

    comp_data = [
        {'ev': 5_000_000_000, 'revenue': 1_000_000_000, 'ebitda_margin': 25, 'growth': 15},
        {'ev': 8_000_000_000, 'revenue': 1_500_000_000, 'ebitda_margin': 30, 'growth': 20},
        {'ev': 3_500_000_000, 'revenue': 800_000_000, 'ebitda_margin': 20, 'growth': 12},
        {'ev': 6_500_000_000, 'revenue': 1_200_000_000, 'ebitda_margin': 28, 'growth': 18},
        {'ev': 4_000_000_000, 'revenue': 900_000_000, 'ebitda_margin': 22, 'growth': 14}
    ]

    subject = {'revenue': 1_100_000_000, 'ebitda_margin': 26, 'growth': 16}

    valuation = analyzer.multiple_regression_valuation(comp_data, subject)

    print("=== REGRESSION-BASED VALUATION ===\n")
    print(f"Predicted Value: ${valuation['predicted_value']:,.0f}")
    print(f"95% Prediction Interval:")
    print(f"  Lower: ${valuation['prediction_interval']['lower']:,.0f}")
    print(f"  Upper: ${valuation['prediction_interval']['upper']:,.0f}")
    print(f"\nR-Squared: {valuation['regression_statistics']['r_squared']:.3f}")
    print(f"Adjusted R-Squared: {valuation['regression_statistics']['adjusted_r_squared']:.3f}")

    print("\n\nCoefficients:")
    for feature, stats in valuation['coefficients'].items():
        sig = "***" if stats['significant'] else ""
        print(f"  {feature}: {stats['coefficient']:,.0f} (t={stats['t_statistic']:.2f}, p={stats['p_value']:.4f}) {sig}")

    deal_data = [
        {'premium': 35, 'size': 500, 'leverage': 0.3, 'growth': 10, 'multiple': 8},
        {'premium': 42, 'size': 1000, 'leverage': 0.25, 'growth': 15, 'multiple': 10},
        {'premium': 28, 'size': 300, 'leverage': 0.4, 'growth': 8, 'multiple': 7},
        {'premium': 38, 'size': 750, 'leverage': 0.3, 'growth': 12, 'multiple': 9},
        {'premium': 45, 'size': 1200, 'leverage': 0.2, 'growth': 18, 'multiple': 11}
    ]

    premium_analysis = analyzer.premium_regression(deal_data)

    print("\n\n=== PREMIUM REGRESSION ANALYSIS ===")
    print(f"R-Squared: {premium_analysis['regression_statistics']['r_squared']:.3f}")
    print(f"Base Premium: {premium_analysis['regression_statistics']['intercept']:.1f}%\n")

    print("Premium Drivers:")
    for driver, stats in premium_analysis['premium_drivers'].items():
        sig = "***" if stats['significant'] else ""
        print(f"  {driver}: {stats['coefficient']:+.2f}% {sig}")
        print(f"    {stats['interpretation']}")
