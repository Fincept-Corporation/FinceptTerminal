"""
Utility functions for economic agents
"""

import json
import csv
from typing import Dict, List, Any, Union, Optional
from datetime import datetime, date
import statistics
import math

def validate_economic_data(data) -> List[str]:
    """
    Validate economic data and return list of issues

    Args:
        data: EconomicData object to validate

    Returns:
        List[str]: List of validation issues
    """
    issues = []

    # GDP growth validation
    if data.gdp < -20 or data.gdp > 20:
        issues.append(f"Unrealistic GDP growth rate: {data.gdp}%")

    # Inflation validation
    if data.inflation < -10 or data.inflation > 50:
        issues.append(f"Unrealistic inflation rate: {data.inflation}%")

    # Unemployment validation
    if data.unemployment < 0 or data.unemployment > 50:
        issues.append(f"Unrealistic unemployment rate: {data.unemployment}%")

    # Interest rate validation
    if data.interest_rate < 0 or data.interest_rate > 50:
        issues.append(f"Unrealistic interest rate: {data.interest_rate}%")

    # Tax rate validation
    if data.tax_rate < 0 or data.tax_rate > 100:
        issues.append(f"Invalid tax rate: {data.tax_rate}%")

    # Government spending validation
    if data.government_spending < 0 or data.government_spending > 100:
        issues.append(f"Invalid government spending: {data.government_spending}% of GDP")

    # Private investment validation
    if data.private_investment < 0 or data.private_investment > 100:
        issues.append(f"Invalid private investment: {data.private_investment}% of GDP")

    # Consumer confidence validation
    if data.consumer_confidence < 0 or data.consumer_confidence > 100:
        issues.append(f"Invalid consumer confidence: {data.consumer_confidence}")

    return issues

def calculate_growth_rate(current: float, previous: float) -> float:
    """
    Calculate growth rate between two values

    Args:
        current (float): Current value
        previous (float): Previous value

    Returns:
        float: Growth rate as percentage
    """
    if previous == 0:
        return 0.0
    return ((current - previous) / previous) * 100

def normalize_score(value: float, min_val: float, max_val: float) -> float:
    """
    Normalize a value to 0-100 scale

    Args:
        value (float): Value to normalize
        min_val (float): Minimum possible value
        max_val (float): Maximum possible value

    Returns:
        float: Normalized value between 0 and 100
    """
    if max_val == min_val:
        return 50.0
    normalized = ((value - min_val) / (max_val - min_val)) * 100
    return max(0, min(100, normalized))

def standardize_score(value: float, target: float, tolerance: float) -> float:
    """
    Standardize score based on distance from target value

    Args:
        value (float): Actual value
        target (float): Target value
        tolerance (float): Acceptable tolerance

    Returns:
        float: Score from 0-100 where target gets 100
    """
    distance = abs(value - target)
    if distance <= tolerance:
        return 100.0
    penalty = (distance - tolerance) * 10
    return max(0, 100 - penalty)

def calculate_composite_score(scores: List[float], weights: Optional[List[float]] = None) -> float:
    """
    Calculate composite score from multiple components

    Args:
        scores (List[float]): List of individual scores
        weights (Optional[List[float]]): Optional weights for each score

    Returns:
        float: Composite score
    """
    if not scores:
        return 0.0

    if weights is None:
        return statistics.mean(scores)

    if len(scores) != len(weights):
        raise ValueError("Number of scores and weights must match")

    total_weight = sum(weights)
    if total_weight == 0:
        return statistics.mean(scores)

    weighted_sum = sum(score * weight for score, weight in zip(scores, weights))
    return weighted_sum / total_weight

def format_currency(amount: float, currency: str = "USD") -> str:
    """
    Format currency amount

    Args:
        amount (float): Amount to format
        currency (str): Currency code

    Returns:
        str: Formatted currency string
    """
    if amount >= 1e12:
        return f"{currency}{amount/1e12:.2f}T"
    elif amount >= 1e9:
        return f"{currency}{amount/1e9:.2f}B"
    elif amount >= 1e6:
        return f"{currency}{amount/1e6:.2f}M"
    elif amount >= 1e3:
        return f"{currency}{amount/1e3:.2f}K"
    else:
        return f"{currency}{amount:.2f}"

def calculate_trend(values: List[float]) -> Dict[str, Any]:
    """
    Calculate trend statistics for a series of values

    Args:
        values (List[float]): List of values

    Returns:
        Dict[str, Any]: Trend statistics
    """
    if len(values) < 2:
        return {
            "trend": "insufficient_data",
            "direction": "neutral",
            "change_rate": 0.0,
            "volatility": 0.0
        }

    # Calculate trend direction
    first_half = statistics.mean(values[:len(values)//2])
    second_half = statistics.mean(values[len(values)//2:])

    if second_half > first_half * 1.05:
        direction = "increasing"
        trend = "upward"
    elif second_half < first_half * 0.95:
        direction = "decreasing"
        trend = "downward"
    else:
        direction = "stable"
        trend = "neutral"

    # Calculate change rate
    change_rate = calculate_growth_rate(values[-1], values[0])

    # Calculate volatility (coefficient of variation)
    if statistics.mean(values) != 0:
        volatility = (statistics.stdev(values) / abs(statistics.mean(values))) * 100
    else:
        volatility = 0.0

    return {
        "trend": trend,
        "direction": direction,
        "change_rate": change_rate,
        "volatility": volatility,
        "mean": statistics.mean(values),
        "median": statistics.median(values)
    }

def interpolate_data_point(start_value: float, end_value: float, steps: int, target_step: int) -> float:
    """
    Interpolate data point between two values

    Args:
        start_value (float): Starting value
        end_value (float): Ending value
        steps (int): Total number of steps
        target_step (int): Target step number

    Returns:
        float: Interpolated value
    """
    if steps <= 1:
        return start_value

    if target_step >= steps:
        return end_value

    ratio = target_step / (steps - 1)
    return start_value + (end_value - start_value) * ratio

def calculate_risk_metrics(values: List[float]) -> Dict[str, float]:
    """
    Calculate risk metrics for a series of values

    Args:
        values (List[float]): List of values

    Returns:
        Dict[str, float]: Risk metrics
    """
    if len(values) < 2:
        return {
            "volatility": 0.0,
            "max_drawdown": 0.0,
            "sharpe_ratio": 0.0,
            "var_95": 0.0
        }

    mean_val = statistics.mean(values)

    # Volatility
    volatility = statistics.stdev(values) if len(values) > 1 else 0.0

    # Maximum drawdown
    max_val = max(values)
    min_val = min(values)
    max_drawdown = (max_val - min_val) / max_val * 100 if max_val != 0 else 0.0

    # Simplified Sharpe ratio (assuming risk-free rate = 0)
    sharpe_ratio = mean_val / volatility if volatility != 0 else 0.0

    # Value at Risk (95% confidence)
    sorted_values = sorted(values)
    var_index = int(0.05 * len(sorted_values))
    var_95 = sorted_values[var_index] if var_index < len(sorted_values) else sorted_values[0]

    return {
        "volatility": volatility,
        "max_drawdown": max_drawdown,
        "sharpe_ratio": sharpe_ratio,
        "var_95": var_95,
        "mean": mean_val,
        "median": statistics.median(values)
    }

def create_economic_summary(data_points: List) -> Dict[str, Any]:
    """
    Create summary statistics for multiple economic data points

    Args:
        data_points (List): List of EconomicData objects

    Returns:
        Dict[str, Any]: Summary statistics
    """
    if not data_points:
        return {"error": "No data points provided"}

    # Extract values for each indicator
    gdp_values = [dp.gdp for dp in data_points]
    inflation_values = [dp.inflation for dp in data_points]
    unemployment_values = [dp.unemployment for dp in data_points]
    interest_values = [dp.interest_rate for dp in data_points]

    summary = {
        "data_points_count": len(data_points),
        "date_range": {
            "start": min(dp.timestamp for dp in data_points).strftime("%Y-%m-%d"),
            "end": max(dp.timestamp for dp in data_points).strftime("%Y-%m-%d")
        },
        "indicators": {
            "gdp": calculate_trend(gdp_values),
            "inflation": calculate_trend(inflation_values),
            "unemployment": calculate_trend(unemployment_values),
            "interest_rate": calculate_trend(interest_values)
        }
    }

    return summary

def export_to_json(data: Any, filename: str, indent: int = 2) -> bool:
    """
    Export data to JSON file

    Args:
        data (Any): Data to export
        filename (str): Output filename
        indent (int): JSON indentation

    Returns:
        bool: Success status
    """
    try:
        with open(filename, 'w') as f:
            json.dump(data, f, indent=indent, default=str)
        return True
    except Exception as e:
        print(f"Error exporting to JSON: {e}")
        return False

def export_to_csv(data: List[Dict], filename: str) -> bool:
    """
    Export data to CSV file

    Args:
        data (List[Dict]): List of dictionaries to export
        filename (str): Output filename

    Returns:
        bool: Success status
    """
    if not data:
        return False

    try:
        with open(filename, 'w', newline='') as csvfile:
            fieldnames = data[0].keys()
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(data)
        return True
    except Exception as e:
        print(f"Error exporting to CSV: {e}")
        return False

def clamp_value(value: float, min_val: float, max_val: float) -> float:
    """
    Clamp a value between min and max bounds

    Args:
        value (float): Value to clamp
        min_val (float): Minimum bound
        max_val (float): Maximum bound

    Returns:
        float: Clamped value
    """
    return max(min_val, min(max_val, value))