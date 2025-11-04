"""
Simple Prediction Demo (No Dependencies Required)
Uses simple moving average and linear regression for basic predictions
"""

import json
import sys
from datetime import datetime, timedelta


def simple_moving_average(data, window=20):
    """Calculate simple moving average"""
    if len(data) < window:
        return data

    result = []
    for i in range(len(data)):
        if i < window - 1:
            result.append(sum(data[:i+1]) / (i+1))
        else:
            result.append(sum(data[i-window+1:i+1]) / window)
    return result


def simple_linear_regression(data, forecast_steps=30):
    """Simple linear regression forecast"""
    n = len(data)

    # Calculate slope and intercept
    sum_x = sum(range(n))
    sum_y = sum(data)
    sum_xy = sum(i * y for i, y in enumerate(data))
    sum_x2 = sum(i * i for i in range(n))

    slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x)
    intercept = (sum_y - slope * sum_x) / n

    # Generate forecast
    forecast = []
    for i in range(n, n + forecast_steps):
        forecast.append(slope * i + intercept)

    return forecast


def exponential_smoothing(data, alpha=0.3, forecast_steps=30):
    """Simple exponential smoothing"""
    smoothed = [data[0]]

    for i in range(1, len(data)):
        smoothed.append(alpha * data[i] + (1 - alpha) * smoothed[-1])

    # Forecast
    forecast = []
    last_value = smoothed[-1]
    for _ in range(forecast_steps):
        forecast.append(last_value)
        last_value = alpha * last_value + (1 - alpha) * last_value

    return forecast


def calculate_volatility(data, window=20):
    """Simple volatility calculation"""
    if len(data) < 2:
        return 0

    returns = []
    for i in range(1, len(data)):
        ret = (data[i] - data[i-1]) / data[i-1]
        returns.append(ret)

    # Calculate standard deviation
    mean_return = sum(returns) / len(returns)
    variance = sum((r - mean_return) ** 2 for r in returns) / len(returns)
    volatility = (variance ** 0.5) * (252 ** 0.5) * 100  # Annualized

    return volatility


def generate_signals(current_price, forecast_price):
    """Generate trading signals"""
    expected_return = (forecast_price - current_price) / current_price * 100

    if expected_return > 2:
        signal = 'BUY'
        strength = min(expected_return / 10, 1.0)
    elif expected_return < -2:
        signal = 'SELL'
        strength = min(abs(expected_return) / 10, 1.0)
    else:
        signal = 'HOLD'
        strength = 0.5

    return {
        'type': signal,
        'strength': strength,
        'confidence': 'high' if strength > 0.7 else 'medium' if strength > 0.4 else 'low',
        'expected_return_1d': expected_return,
        'current_price': current_price,
        'predicted_price_1d': forecast_price
    }


def simple_prediction(prices, forecast_steps=30):
    """
    Simple prediction using multiple methods
    No external dependencies required
    """
    if len(prices) < 10:
        return {
            'success': False,
            'error': 'Insufficient data (need at least 10 data points)'
        }

    current_price = prices[-1]

    # Method 1: Linear Regression
    lr_forecast = simple_linear_regression(prices, forecast_steps)

    # Method 2: Exponential Smoothing
    es_forecast = exponential_smoothing(prices, alpha=0.3, forecast_steps=forecast_steps)

    # Method 3: Moving Average Trend
    ma_20 = simple_moving_average(prices, window=20)
    trend = ma_20[-1] - ma_20[-20] if len(ma_20) >= 20 else 0
    ma_forecast = [current_price + trend * (i+1) for i in range(forecast_steps)]

    # Ensemble (simple average)
    ensemble_forecast = []
    for i in range(forecast_steps):
        avg = (lr_forecast[i] + es_forecast[i] + ma_forecast[i]) / 3
        ensemble_forecast.append(avg)

    # Calculate confidence intervals (±5%)
    lower_ci = [f * 0.95 for f in ensemble_forecast]
    upper_ci = [f * 1.05 for f in ensemble_forecast]

    # Generate dates
    start_date = datetime.now()
    forecast_dates = [(start_date + timedelta(days=i+1)).strftime('%Y-%m-%d')
                     for i in range(forecast_steps)]

    # Generate signals
    signals = [generate_signals(current_price, ensemble_forecast[0])]

    # Calculate volatility
    volatility = calculate_volatility(prices)

    return {
        'success': True,
        'symbol': 'DEMO',
        'method': 'Simple Ensemble (LR + ES + MA)',
        'predictions': {
            'ensemble': {
                'forecast': ensemble_forecast,
                'lower_ci': lower_ci,
                'upper_ci': upper_ci,
                'forecast_dates': forecast_dates,
                'model_weights': {
                    'linear_regression': 0.33,
                    'exponential_smoothing': 0.33,
                    'moving_average': 0.34
                }
            },
            'forecasts': {
                'linear_regression': {'forecast': lr_forecast},
                'exponential_smoothing': {'forecast': es_forecast},
                'moving_average': {'forecast': ma_forecast}
            }
        },
        'signals': signals,
        'metrics': {
            'current_price': current_price,
            'volatility': volatility,
            'forecast_horizon': forecast_steps,
            'data_points': len(prices)
        },
        'note': 'This is a simple demo. Install ML libraries for advanced predictions.'
    }


def main():
    """Command-line interface"""
    try:
        # Read input
        input_data = json.loads(sys.stdin.read())

        # Extract data
        if 'ohlcv_data' in input_data:
            prices = input_data['ohlcv_data'].get('close', [])
        elif 'data' in input_data:
            prices = input_data['data']
        else:
            prices = input_data.get('prices', [])

        steps_ahead = input_data.get('steps_ahead', 30)

        # Run prediction
        result = simple_prediction(prices, forecast_steps=steps_ahead)

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({
            'success': False,
            'error': str(e)
        }))


if __name__ == '__main__':
    main()
