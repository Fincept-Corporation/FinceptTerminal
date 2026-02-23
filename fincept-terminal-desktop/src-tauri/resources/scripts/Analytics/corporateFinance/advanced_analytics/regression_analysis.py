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

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import sys
    import json

    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "regression":
            if len(sys.argv) < 3:
                raise ValueError("Regression parameters required")

            # Accept either a single JSON dict with all params, or positional args
            try:
                params = json.loads(sys.argv[2])
            except (json.JSONDecodeError, TypeError):
                params = {}

            # Check if params is a wrapper dict with comp_data, subject_metrics, type keys
            if isinstance(params, dict) and ('comp_data' in params or 'dependent' in params):
                if 'dependent' in params:
                    # Simple regression input format
                    dependent = params['dependent']
                    independent = params['independent']
                    analyzer = MARegression()
                    y = np.array(dependent)
                    X = np.array(independent).T if len(np.array(independent).shape) > 1 else np.array(independent).reshape(-1, 1)
                    result_data = analyzer.linear_regression(X, y)
                    analysis = {
                        'r_squared': result_data.r_squared,
                        'adjusted_r_squared': result_data.adjusted_r_squared,
                        'coefficients': result_data.coefficients.tolist(),
                        'intercept': result_data.intercept,
                        'p_values': result_data.p_values.tolist(),
                        'std_errors': result_data.std_errors.tolist()
                    }
                else:
                    comp_data = params.get('comp_data', [])
                    subject_metrics = params.get('subject_metrics', {})
                    regression_type = params.get('type', params.get('regression_type', 'premium'))
                    analyzer = MARegression()
                    if regression_type == "premium":
                        analysis = analyzer.premium_regression(comp_data)
                    elif regression_type == "multiple":
                        analysis = analyzer.multiple_regression(comp_data, subject_metrics)
                    else:
                        analysis = analyzer.premium_regression(comp_data)
            else:
                # Positional args: comp_data subject_metrics regression_type
                comp_data = params if isinstance(params, list) else json.loads(sys.argv[2])
                subject_metrics = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
                regression_type = sys.argv[4] if len(sys.argv) > 4 else "multiple"

                analyzer = MARegression()
                # Both 'ols' and 'multiple' use multiple_regression_valuation with frontend data format
                # (comp_data has 'ev', 'revenue', 'ebitda', 'growth' keys)
                # Only route to premium_regression if comp_data has 'premium' key
                has_premium_key = comp_data and 'premium' in comp_data[0]
                if has_premium_key and regression_type == "premium":
                    analysis = analyzer.premium_regression(comp_data)
                else:
                    # Filter out non-numeric keys (like 'name') from comp_data for regression
                    numeric_keys = [k for k in comp_data[0].keys() if k != 'name'] if comp_data else []
                    filtered_comp = [{k: v for k, v in row.items() if k in numeric_keys} for row in comp_data]
                    # Also filter subject_metrics to only include keys that are features (not 'ev', not 'name')
                    feature_keys = [k for k in numeric_keys if k != 'ev']
                    filtered_subject = {k: v for k, v in subject_metrics.items() if k in feature_keys}
                    analysis_raw = analyzer.multiple_regression_valuation(filtered_comp, filtered_subject)

                    # Normalize output to what the frontend expects:
                    # result.implied_ev, result.r_squared, result.adj_r_squared,
                    # result.coefficients (key → plain number), result.implied_multiples
                    rs = analysis_raw.get('regression_statistics', {})
                    raw_coeffs = analysis_raw.get('coefficients', {})
                    predicted_ev = analysis_raw.get('predicted_value', 0)

                    # coefficients: flatten to {feature: coefficient_value}
                    flat_coeffs = {
                        feat: coeff_info['coefficient']
                        for feat, coeff_info in raw_coeffs.items()
                    }

                    # implied_multiples: compute EV / metric for subject company
                    implied_multiples = {}
                    if filtered_subject.get('revenue', 0) > 0:
                        implied_multiples['ev_revenue'] = predicted_ev / filtered_subject['revenue']
                    if filtered_subject.get('ebitda', 0) > 0:
                        implied_multiples['ev_ebitda'] = predicted_ev / filtered_subject['ebitda']

                    # prediction_interval
                    pi = analysis_raw.get('prediction_interval', {})

                    analysis = {
                        'implied_ev': predicted_ev,
                        'prediction_interval_low': pi.get('lower', 0),
                        'prediction_interval_high': pi.get('upper', 0),
                        'r_squared': rs.get('r_squared', 0),
                        'adj_r_squared': rs.get('adjusted_r_squared', 0),
                        'intercept': rs.get('intercept', 0),
                        'coefficients': flat_coeffs,
                        'implied_multiples': implied_multiples,
                        'num_comparables': analysis_raw.get('num_comparables', len(comp_data)),
                        'regression_type': regression_type,
                        'raw_coefficients': raw_coeffs,  # keep full detail for raw view
                    }

            result = {"success": True, "data": analysis}

            def safe_json(o):
                if isinstance(o, np.bool_):
                    return bool(o)
                if isinstance(o, (np.floating, np.integer)):
                    v = float(o)
                    if v != v or v == float('inf') or v == float('-inf'):  # nan or inf
                        return None
                    return v
                return str(o)

            print(json.dumps(result, default=safe_json))

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e)}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
