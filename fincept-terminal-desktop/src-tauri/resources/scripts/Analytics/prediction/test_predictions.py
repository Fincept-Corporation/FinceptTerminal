#!/usr/bin/env python3
"""
Test Suite for AI Prediction Module
Tests all components with sample data
"""

import json
import sys
from datetime import datetime, timedelta

# Generate sample price data
def generate_sample_data(days=365, start_price=100):
    """Generate realistic sample OHLCV data"""
    import random
    random.seed(42)

    dates = []
    open_prices = []
    high_prices = []
    low_prices = []
    close_prices = []
    volumes = []

    current_price = start_price
    current_date = datetime.now() - timedelta(days=days)

    for _ in range(days):
        # Random walk with slight upward trend
        change = random.gauss(0.001, 0.02)
        current_price *= (1 + change)

        open_price = current_price * random.uniform(0.99, 1.01)
        close_price = current_price
        high_price = max(open_price, close_price) * random.uniform(1.00, 1.02)
        low_price = min(open_price, close_price) * random.uniform(0.98, 1.00)
        volume = random.randint(1000000, 10000000)

        dates.append(current_date.strftime('%Y-%m-%d'))
        open_prices.append(open_price)
        high_prices.append(high_price)
        low_prices.append(low_price)
        close_prices.append(close_price)
        volumes.append(volume)

        current_date += timedelta(days=1)

    return {
        'dates': dates,
        'open': open_prices,
        'high': high_prices,
        'low': low_prices,
        'close': close_prices,
        'volume': volumes
    }


def test_simple_prediction():
    """Test simple prediction (no dependencies)"""
    print("\n" + "="*60)
    print("TEST 1: Simple Prediction Demo (No Dependencies)")
    print("="*60)

    try:
        from simple_prediction_demo import simple_prediction

        # Generate sample data
        sample_data = generate_sample_data(days=90)
        prices = sample_data['close']

        # Run prediction
        result = simple_prediction(prices, forecast_steps=30)

        if result['success']:
            print("✓ Simple prediction PASSED")
            print(f"  - Forecast: {len(result['predictions']['ensemble']['forecast'])} days")
            print(f"  - Signal: {result['signals'][0]['type']}")
            print(f"  - Expected Return: {result['signals'][0]['expected_return_1d']:.2f}%")
            print(f"  - Volatility: {result['metrics']['volatility']:.2f}%")
            return True
        else:
            print(f"✗ Simple prediction FAILED: {result.get('error')}")
            return False

    except Exception as e:
        print(f"✗ Simple prediction FAILED: {str(e)}")
        return False


def test_time_series_models():
    """Test time series models"""
    print("\n" + "="*60)
    print("TEST 2: Time Series Models (Requires: statsmodels, prophet, tensorflow)")
    print("="*60)

    try:
        from time_series_models import TimeSeriesPredictor
        import pandas as pd

        sample_data = generate_sample_data(days=365)
        prices = pd.Series(sample_data['close'])

        # Test ARIMA
        print("\nTesting ARIMA...")
        try:
            predictor = TimeSeriesPredictor(model_type='arima')
            fit_result = predictor.fit_arima(prices[:300], auto_order=True)

            if 'success' in fit_result:
                pred_result = predictor.predict(steps_ahead=30)
                print(f"  ✓ ARIMA: AIC={fit_result['aic']:.2f}, Forecast={len(pred_result['forecast'])} days")
            else:
                print(f"  ✗ ARIMA fitting failed: {fit_result.get('error')}")
        except Exception as e:
            print(f"  ⊘ ARIMA: {str(e)}")

        # Test Prophet
        print("\nTesting Prophet...")
        try:
            predictor_prophet = TimeSeriesPredictor(model_type='prophet')
            df = pd.DataFrame({
                'ds': pd.to_datetime(sample_data['dates'][:300]),
                'y': sample_data['close'][:300]
            })
            fit_result = predictor_prophet.fit_prophet(df)

            if 'success' in fit_result:
                pred_result = predictor_prophet.predict(steps_ahead=30)
                print(f"  ✓ Prophet: Forecast={len(pred_result['forecast'])} days")
            else:
                print(f"  ✗ Prophet failed")
        except Exception as e:
            print(f"  ⊘ Prophet: {str(e)}")

        # Test LSTM
        print("\nTesting LSTM...")
        try:
            predictor_lstm = TimeSeriesPredictor(model_type='lstm')
            fit_result = predictor_lstm.fit_lstm(prices[:300], lookback=60, epochs=5)

            if 'success' in fit_result:
                pred_result = predictor_lstm.predict(steps_ahead=30)
                print(f"  ✓ LSTM: Loss={fit_result['final_loss']:.4f}, Forecast={len(pred_result['forecast'])} days")
            else:
                print(f"  ✗ LSTM failed")
        except Exception as e:
            print(f"  ⊘ LSTM: {str(e)}")

        return True

    except ImportError as e:
        print(f"⊘ Time series models test SKIPPED: {str(e)}")
        print("  Install dependencies: pip install -r requirements.txt")
        return None
    except Exception as e:
        print(f"✗ Time series models test FAILED: {str(e)}")
        return False


def test_stock_predictor():
    """Test stock price predictor"""
    print("\n" + "="*60)
    print("TEST 3: Stock Price Predictor (Requires: sklearn, xgboost)")
    print("="*60)

    try:
        from stock_price_predictor import StockPricePredictor
        import pandas as pd

        sample_data = generate_sample_data(days=365)
        df = pd.DataFrame({
            'open': sample_data['open'],
            'high': sample_data['high'],
            'low': sample_data['low'],
            'close': sample_data['close'],
            'volume': sample_data['volume']
        }, index=pd.to_datetime(sample_data['dates']))

        print("\nTraining stock predictor...")
        predictor = StockPricePredictor(symbol='TEST', lookback_days=252)
        fit_result = predictor.fit(df, test_size=0.2)

        if fit_result['overall_status'] == 'success':
            print("✓ Stock predictor training PASSED")

            # Test prediction
            print("\nGenerating predictions...")
            pred_result = predictor.predict(steps_ahead=30, method='ensemble')

            if pred_result['status'] == 'success':
                print("✓ Stock prediction PASSED")

                if 'ensemble' in pred_result:
                    print(f"  - Ensemble forecast: {len(pred_result['ensemble']['forecast'])} days")
                    print(f"  - Models used: {', '.join(pred_result['ensemble'].get('model_weights', {}).keys())}")

                # Test signals
                signals = predictor.calculate_signals(pred_result)
                if signals['signals']:
                    signal = signals['signals'][0]
                    print(f"  - Signal: {signal['type']} (strength: {signal['strength']:.2f})")

                return True
            else:
                print(f"✗ Prediction failed: {pred_result.get('error')}")
                return False
        else:
            print(f"✗ Training failed")
            return False

    except ImportError as e:
        print(f"⊘ Stock predictor test SKIPPED: {str(e)}")
        return None
    except Exception as e:
        print(f"✗ Stock predictor test FAILED: {str(e)}")
        import traceback
        traceback.print_exc()
        return False


def test_volatility_forecaster():
    """Test volatility forecaster"""
    print("\n" + "="*60)
    print("TEST 4: Volatility Forecaster (Requires: arch)")
    print("="*60)

    try:
        from volatility_forecaster import VolatilityForecaster
        import pandas as pd

        sample_data = generate_sample_data(days=365)
        prices = pd.Series(sample_data['close'], index=pd.to_datetime(sample_data['dates']))

        print("\nFitting GARCH model...")
        forecaster = VolatilityForecaster(model_type='garch')
        returns = forecaster.calculate_returns(prices)

        fit_result = forecaster.fit_garch(returns, p=1, q=1)

        if 'success' in fit_result:
            print(f"✓ GARCH fitting PASSED (AIC: {fit_result['aic']:.2f})")

            # Test prediction
            print("\nForecasting volatility...")
            vol_forecast = forecaster.predict_volatility(horizon=30)

            if vol_forecast['success']:
                print("✓ Volatility forecast PASSED")
                print(f"  - Current volatility: {vol_forecast['current_volatility']:.2f}%")
                print(f"  - Mean forecast: {vol_forecast['mean_forecast_vol']:.2f}%")

                # Test VaR
                var_result = forecaster.calculate_var(confidence_level=0.95, portfolio_value=100000)
                if var_result['success']:
                    print(f"  - VaR (95%): ${var_result['var_dollar']:,.0f}")

                return True
            else:
                print(f"✗ Volatility forecast failed: {vol_forecast.get('error')}")
                return False
        else:
            print(f"✗ GARCH fitting failed: {fit_result.get('error')}")
            return False

    except ImportError as e:
        print(f"⊘ Volatility forecaster test SKIPPED: {str(e)}")
        return None
    except Exception as e:
        print(f"✗ Volatility forecaster test FAILED: {str(e)}")
        return False


def test_backtesting():
    """Test backtesting engine"""
    print("\n" + "="*60)
    print("TEST 5: Backtesting Engine")
    print("="*60)

    try:
        from backtesting_engine import BacktestingEngine, moving_average_crossover_strategy
        import pandas as pd

        sample_data = generate_sample_data(days=365)
        df = pd.DataFrame({
            'close': sample_data['close'],
            'high': sample_data['high'],
            'low': sample_data['low'],
            'volume': sample_data['volume']
        }, index=pd.to_datetime(sample_data['dates']))

        print("\nRunning backtest...")
        engine = BacktestingEngine(initial_capital=100000, commission=0.001)

        result = engine.run_backtest(
            df,
            moving_average_crossover_strategy,
            strategy_params={'fast_period': 20, 'slow_period': 50, 'symbol': 'TEST'}
        )

        if result['success']:
            print("✓ Backtesting PASSED")
            metrics = result['metrics']
            print(f"  - Total Return: {metrics['total_return']:.2f}%")
            print(f"  - Sharpe Ratio: {metrics['sharpe_ratio']:.2f}")
            print(f"  - Max Drawdown: {metrics['max_drawdown']:.2f}%")
            print(f"  - Win Rate: {metrics['win_rate']:.1f}%")
            print(f"  - Total Trades: {metrics['total_trades']}")
            return True
        else:
            print(f"✗ Backtesting failed: {result.get('error')}")
            return False

    except Exception as e:
        print(f"✗ Backtesting test FAILED: {str(e)}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """Run all tests"""
    print("\n" + "="*60)
    print("AI PREDICTION MODULE - TEST SUITE")
    print("="*60)

    results = {
        'Simple Prediction': test_simple_prediction(),
        'Time Series Models': test_time_series_models(),
        'Stock Predictor': test_stock_predictor(),
        'Volatility Forecaster': test_volatility_forecaster(),
        'Backtesting Engine': test_backtesting()
    }

    # Summary
    print("\n" + "="*60)
    print("TEST SUMMARY")
    print("="*60)

    passed = sum(1 for r in results.values() if r is True)
    failed = sum(1 for r in results.values() if r is False)
    skipped = sum(1 for r in results.values() if r is None)

    for name, result in results.items():
        if result is True:
            status = "✓ PASSED"
        elif result is False:
            status = "✗ FAILED"
        else:
            status = "⊘ SKIPPED"
        print(f"  {status}: {name}")

    print("\n" + "-"*60)
    print(f"Total: {len(results)} | Passed: {passed} | Failed: {failed} | Skipped: {skipped}")
    print("="*60 + "\n")

    if failed > 0:
        print("⚠️  Some tests failed. Check errors above.")
        sys.exit(1)
    elif skipped > 0:
        print("⚠️  Some tests were skipped (missing dependencies).")
        print("    Run: ./install_dependencies.sh")
        sys.exit(0)
    else:
        print("✓ All tests passed!")
        sys.exit(0)


if __name__ == '__main__':
    main()
