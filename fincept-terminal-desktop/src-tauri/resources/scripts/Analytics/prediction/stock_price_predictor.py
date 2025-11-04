"""
Stock Price Prediction Module for FinceptTerminal
Multi-model ensemble approach combining technical, fundamental, and sentiment analysis
"""

import numpy as np
import pandas as pd
from typing import Dict, List, Tuple, Optional, Any
import json
import sys
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

# Import time series models
from time_series_models import TimeSeriesPredictor

# ML Libraries
try:
    from sklearn.ensemble import RandomForestRegressor, GradientBoostingRegressor
    from sklearn.linear_model import Ridge
    from sklearn.preprocessing import StandardScaler
    from sklearn.model_selection import train_test_split
    import xgboost as xgb
except ImportError:
    RandomForestRegressor = None
    xgb = None


class StockPricePredictor:
    """
    Multi-model stock price forecasting system
    Combines ARIMA, XGBoost, LSTM, and ensemble methods
    Integrates technical indicators and market features
    """

    def __init__(self, symbol: str, lookback_days: int = 252):
        """
        Initialize predictor

        Args:
            symbol: Stock ticker symbol
            lookback_days: Number of historical days to use (default 252 = 1 year)
        """
        self.symbol = symbol
        self.lookback_days = lookback_days
        self.models = {}
        self.scalers = {}
        self.is_fitted = False
        self.feature_names = []
        self.historical_data = None

    def create_technical_features(self, df: pd.DataFrame) -> pd.DataFrame:
        """
        Create technical indicator features

        Args:
            df: DataFrame with OHLCV data

        Returns:
            DataFrame with added technical features
        """
        features = df.copy()

        # Moving Averages
        for period in [5, 10, 20, 50, 100, 200]:
            features[f'sma_{period}'] = features['close'].rolling(window=period).mean()
            features[f'ema_{period}'] = features['close'].ewm(span=period, adjust=False).mean()

        # Price momentum
        for period in [1, 5, 10, 20]:
            features[f'momentum_{period}'] = features['close'].pct_change(periods=period)

        # RSI
        delta = features['close'].diff()
        gain = (delta.where(delta > 0, 0)).rolling(window=14).mean()
        loss = (-delta.where(delta < 0, 0)).rolling(window=14).mean()
        rs = gain / loss
        features['rsi_14'] = 100 - (100 / (1 + rs))

        # MACD
        exp1 = features['close'].ewm(span=12, adjust=False).mean()
        exp2 = features['close'].ewm(span=26, adjust=False).mean()
        features['macd'] = exp1 - exp2
        features['macd_signal'] = features['macd'].ewm(span=9, adjust=False).mean()
        features['macd_histogram'] = features['macd'] - features['macd_signal']

        # Bollinger Bands
        features['bb_middle'] = features['close'].rolling(window=20).mean()
        bb_std = features['close'].rolling(window=20).std()
        features['bb_upper'] = features['bb_middle'] + (bb_std * 2)
        features['bb_lower'] = features['bb_middle'] - (bb_std * 2)
        features['bb_width'] = (features['bb_upper'] - features['bb_lower']) / features['bb_middle']
        features['bb_position'] = (features['close'] - features['bb_lower']) / (features['bb_upper'] - features['bb_lower'])

        # ATR (Average True Range)
        high_low = features['high'] - features['low']
        high_close = np.abs(features['high'] - features['close'].shift())
        low_close = np.abs(features['low'] - features['close'].shift())
        ranges = pd.concat([high_low, high_close, low_close], axis=1)
        true_range = np.max(ranges, axis=1)
        features['atr_14'] = true_range.rolling(14).mean()

        # Volume features
        features['volume_ma_20'] = features['volume'].rolling(window=20).mean()
        features['volume_ratio'] = features['volume'] / features['volume_ma_20']

        # Stochastic Oscillator
        low_14 = features['low'].rolling(window=14).min()
        high_14 = features['high'].rolling(window=14).max()
        features['stoch_k'] = 100 * ((features['close'] - low_14) / (high_14 - low_14))
        features['stoch_d'] = features['stoch_k'].rolling(window=3).mean()

        # Williams %R
        features['williams_r'] = -100 * ((high_14 - features['close']) / (high_14 - low_14))

        # OBV (On-Balance Volume)
        obv = [0]
        for i in range(1, len(features)):
            if features['close'].iloc[i] > features['close'].iloc[i-1]:
                obv.append(obv[-1] + features['volume'].iloc[i])
            elif features['close'].iloc[i] < features['close'].iloc[i-1]:
                obv.append(obv[-1] - features['volume'].iloc[i])
            else:
                obv.append(obv[-1])
        features['obv'] = obv

        # Price position relative to highs/lows
        features['price_vs_52w_high'] = features['close'] / features['close'].rolling(252).max()
        features['price_vs_52w_low'] = features['close'] / features['close'].rolling(252).min()

        # Volatility
        features['volatility_20'] = features['close'].pct_change().rolling(20).std()
        features['volatility_50'] = features['close'].pct_change().rolling(50).std()

        return features

    def create_lag_features(self, df: pd.DataFrame, target_col: str = 'close', lags: int = 20) -> pd.DataFrame:
        """
        Create lagged features for supervised learning

        Args:
            df: Input DataFrame
            target_col: Column to create lags for
            lags: Number of lag periods

        Returns:
            DataFrame with lag features
        """
        features = df.copy()

        for lag in range(1, lags + 1):
            features[f'{target_col}_lag_{lag}'] = features[target_col].shift(lag)

        return features

    def prepare_data(self, ohlcv_data: pd.DataFrame) -> Tuple[pd.DataFrame, pd.Series]:
        """
        Prepare data for model training

        Args:
            ohlcv_data: DataFrame with OHLCV columns

        Returns:
            Tuple of (features DataFrame, target Series)
        """
        # Create technical features
        data = self.create_technical_features(ohlcv_data)

        # Create lag features
        data = self.create_lag_features(data, target_col='close', lags=5)

        # Target: next day's close price
        data['target'] = data['close'].shift(-1)

        # Drop NaN rows
        data = data.dropna()

        # Select features (exclude OHLCV and target)
        exclude_cols = ['open', 'high', 'low', 'close', 'volume', 'target']
        feature_cols = [col for col in data.columns if col not in exclude_cols]

        X = data[feature_cols]
        y = data['target']

        self.feature_names = feature_cols
        self.historical_data = data

        return X, y

    def fit(self, ohlcv_data: pd.DataFrame, test_size: float = 0.2) -> Dict:
        """
        Train all prediction models

        Args:
            ohlcv_data: Historical OHLCV data
            test_size: Proportion of data for testing

        Returns:
            Dict with training results
        """
        results = {
            'symbol': self.symbol,
            'training_start': str(ohlcv_data.index[0]),
            'training_end': str(ohlcv_data.index[-1]),
            'data_points': len(ohlcv_data),
            'models': {}
        }

        try:
            # Prepare data
            X, y = self.prepare_data(ohlcv_data)

            # Split data (time-series split)
            split_idx = int(len(X) * (1 - test_size))
            X_train, X_test = X[:split_idx], X[split_idx:]
            y_train, y_test = y[:split_idx], y[split_idx:]

            # Normalize features
            self.scalers['features'] = StandardScaler()
            X_train_scaled = self.scalers['features'].fit_transform(X_train)
            X_test_scaled = self.scalers['features'].transform(X_test)

            # 1. ARIMA Model
            try:
                arima_predictor = TimeSeriesPredictor(model_type='arima')
                arima_result = arima_predictor.fit_arima(
                    ohlcv_data['close'][:split_idx],
                    auto_order=True
                )
                self.models['arima'] = arima_predictor
                results['models']['arima'] = {
                    'status': 'success' if 'success' in arima_result else 'failed',
                    'details': arima_result
                }
            except Exception as e:
                results['models']['arima'] = {'status': 'failed', 'error': str(e)}

            # 2. XGBoost Model
            if xgb is not None:
                try:
                    self.models['xgboost'] = xgb.XGBRegressor(
                        n_estimators=100,
                        max_depth=5,
                        learning_rate=0.1,
                        random_state=42
                    )
                    self.models['xgboost'].fit(X_train_scaled, y_train)

                    # Calculate metrics
                    train_pred = self.models['xgboost'].predict(X_train_scaled)
                    test_pred = self.models['xgboost'].predict(X_test_scaled)

                    results['models']['xgboost'] = {
                        'status': 'success',
                        'train_rmse': float(np.sqrt(np.mean((y_train - train_pred) ** 2))),
                        'test_rmse': float(np.sqrt(np.mean((y_test - test_pred) ** 2))),
                        'feature_importance': dict(zip(
                            self.feature_names,
                            self.models['xgboost'].feature_importances_.tolist()
                        ))
                    }
                except Exception as e:
                    results['models']['xgboost'] = {'status': 'failed', 'error': str(e)}

            # 3. Random Forest Model
            if RandomForestRegressor is not None:
                try:
                    self.models['random_forest'] = RandomForestRegressor(
                        n_estimators=100,
                        max_depth=10,
                        random_state=42,
                        n_jobs=-1
                    )
                    self.models['random_forest'].fit(X_train_scaled, y_train)

                    train_pred = self.models['random_forest'].predict(X_train_scaled)
                    test_pred = self.models['random_forest'].predict(X_test_scaled)

                    results['models']['random_forest'] = {
                        'status': 'success',
                        'train_rmse': float(np.sqrt(np.mean((y_train - train_pred) ** 2))),
                        'test_rmse': float(np.sqrt(np.mean((y_test - test_pred) ** 2)))
                    }
                except Exception as e:
                    results['models']['random_forest'] = {'status': 'failed', 'error': str(e)}

            # 4. LSTM Model
            try:
                lstm_predictor = TimeSeriesPredictor(model_type='lstm')
                lstm_result = lstm_predictor.fit_lstm(
                    ohlcv_data['close'][:split_idx],
                    lookback=60,
                    units=64,
                    epochs=50
                )
                self.models['lstm'] = lstm_predictor
                results['models']['lstm'] = {
                    'status': 'success' if 'success' in lstm_result else 'failed',
                    'details': lstm_result
                }
            except Exception as e:
                results['models']['lstm'] = {'status': 'failed', 'error': str(e)}

            self.is_fitted = True
            results['overall_status'] = 'success'

        except Exception as e:
            results['overall_status'] = 'failed'
            results['error'] = str(e)

        return results

    def predict(self, steps_ahead: int = 30, method: str = 'ensemble') -> Dict:
        """
        Generate price predictions

        Args:
            steps_ahead: Number of days to forecast
            method: Prediction method ('arima', 'xgboost', 'lstm', 'ensemble')

        Returns:
            Dict with predictions and confidence intervals
        """
        if not self.is_fitted:
            return {'error': 'Model not fitted. Call fit() first.'}

        predictions = {
            'symbol': self.symbol,
            'prediction_date': str(datetime.now()),
            'steps_ahead': steps_ahead,
            'method': method,
            'forecasts': {}
        }

        try:
            if method == 'ensemble' or method == 'all':
                # Get predictions from all available models
                model_predictions = []
                model_weights = []

                # ARIMA prediction
                if 'arima' in self.models:
                    arima_pred = self.models['arima'].predict(steps_ahead)
                    if 'error' not in arima_pred:
                        predictions['forecasts']['arima'] = arima_pred
                        model_predictions.append(arima_pred['forecast'])
                        model_weights.append(0.25)

                # XGBoost prediction
                if 'xgboost' in self.models:
                    xgb_forecast = self._predict_ml_model('xgboost', steps_ahead)
                    predictions['forecasts']['xgboost'] = xgb_forecast
                    model_predictions.append(xgb_forecast['forecast'])
                    model_weights.append(0.35)

                # Random Forest prediction
                if 'random_forest' in self.models:
                    rf_forecast = self._predict_ml_model('random_forest', steps_ahead)
                    predictions['forecasts']['random_forest'] = rf_forecast
                    model_predictions.append(rf_forecast['forecast'])
                    model_weights.append(0.15)

                # LSTM prediction
                if 'lstm' in self.models:
                    lstm_pred = self.models['lstm'].predict(steps_ahead)
                    if 'error' not in lstm_pred:
                        predictions['forecasts']['lstm'] = lstm_pred
                        model_predictions.append(lstm_pred['forecast'])
                        model_weights.append(0.25)

                # Ensemble (weighted average)
                if model_predictions:
                    # Normalize weights
                    model_weights = np.array(model_weights) / np.sum(model_weights)

                    ensemble_forecast = np.average(model_predictions, axis=0, weights=model_weights)

                    # Calculate confidence intervals from model variance
                    forecast_std = np.std(model_predictions, axis=0)

                    predictions['ensemble'] = {
                        'forecast': ensemble_forecast.tolist(),
                        'lower_ci': (ensemble_forecast - 2*forecast_std).tolist(),
                        'upper_ci': (ensemble_forecast + 2*forecast_std).tolist(),
                        'model_weights': {
                            name: float(weight)
                            for name, weight in zip(['arima', 'xgboost', 'random_forest', 'lstm'], model_weights)
                        },
                        'forecast_dates': self._get_forecast_dates(steps_ahead)
                    }

            else:
                # Single model prediction
                if method == 'arima' and 'arima' in self.models:
                    predictions['forecasts']['arima'] = self.models['arima'].predict(steps_ahead)
                elif method == 'lstm' and 'lstm' in self.models:
                    predictions['forecasts']['lstm'] = self.models['lstm'].predict(steps_ahead)
                elif method in ['xgboost', 'random_forest'] and method in self.models:
                    predictions['forecasts'][method] = self._predict_ml_model(method, steps_ahead)
                else:
                    return {'error': f'Model {method} not available or not fitted'}

            predictions['status'] = 'success'

        except Exception as e:
            predictions['status'] = 'failed'
            predictions['error'] = str(e)

        return predictions

    def _predict_ml_model(self, model_name: str, steps_ahead: int) -> Dict:
        """
        Multi-step prediction for ML models (XGBoost, Random Forest)

        Args:
            model_name: Name of the model
            steps_ahead: Number of steps to predict

        Returns:
            Dict with forecast
        """
        model = self.models[model_name]
        last_features = self.historical_data[self.feature_names].iloc[-1:].values

        predictions = []
        current_features = last_features.copy()

        for _ in range(steps_ahead):
            # Scale features
            scaled_features = self.scalers['features'].transform(current_features)

            # Predict
            pred = model.predict(scaled_features)[0]
            predictions.append(pred)

            # Update features for next prediction (simplified)
            # In production, you'd update all lag features properly
            current_features = np.roll(current_features, 1)
            current_features[0, 0] = pred

        # Simple confidence intervals (±5%)
        predictions = np.array(predictions)
        uncertainty = predictions * 0.05

        return {
            'forecast': predictions.tolist(),
            'lower_ci': (predictions - uncertainty).tolist(),
            'upper_ci': (predictions + uncertainty).tolist(),
            'model': model_name,
            'forecast_dates': self._get_forecast_dates(steps_ahead)
        }

    def _get_forecast_dates(self, steps_ahead: int) -> List[str]:
        """Generate forecast dates"""
        if self.historical_data is None:
            return []

        last_date = self.historical_data.index[-1]
        forecast_dates = pd.date_range(
            start=last_date + timedelta(days=1),
            periods=steps_ahead,
            freq='D'
        )

        return [str(d.date()) for d in forecast_dates]

    def calculate_signals(self, predictions: Dict) -> Dict:
        """
        Generate trading signals from predictions

        Args:
            predictions: Prediction results

        Returns:
            Trading signals
        """
        signals = {
            'symbol': self.symbol,
            'timestamp': str(datetime.now()),
            'signals': []
        }

        try:
            if 'ensemble' in predictions:
                forecast = np.array(predictions['ensemble']['forecast'])
                current_price = self.historical_data['close'].iloc[-1]

                # Calculate expected return
                expected_return = (forecast[0] - current_price) / current_price * 100

                # Generate signal
                if expected_return > 2:
                    signal = 'BUY'
                    strength = min(expected_return / 10, 1.0)
                elif expected_return < -2:
                    signal = 'SELL'
                    strength = min(abs(expected_return) / 10, 1.0)
                else:
                    signal = 'HOLD'
                    strength = 0.5

                signals['signals'].append({
                    'type': signal,
                    'strength': float(strength),
                    'expected_return_1d': float(expected_return),
                    'current_price': float(current_price),
                    'predicted_price_1d': float(forecast[0]),
                    'confidence': 'high' if strength > 0.7 else 'medium' if strength > 0.4 else 'low'
                })

        except Exception as e:
            signals['error'] = str(e)

        return signals


def main():
    """
    Command-line interface
    Input JSON format:
    {
        "symbol": "AAPL",
        "ohlcv_data": {
            "dates": [...],
            "open": [...],
            "high": [...],
            "low": [...],
            "close": [...],
            "volume": [...]
        },
        "action": "fit" | "predict" | "both",
        "steps_ahead": 30,
        "method": "ensemble"
    }
    """
    try:
        input_data = json.loads(sys.stdin.read())

        symbol = input_data.get('symbol', 'UNKNOWN')
        ohlcv_raw = input_data.get('ohlcv_data', {})
        action = input_data.get('action', 'both')
        steps_ahead = input_data.get('steps_ahead', 30)
        method = input_data.get('method', 'ensemble')

        # Create DataFrame
        df = pd.DataFrame({
            'open': ohlcv_raw.get('open', []),
            'high': ohlcv_raw.get('high', []),
            'low': ohlcv_raw.get('low', []),
            'close': ohlcv_raw.get('close', []),
            'volume': ohlcv_raw.get('volume', [])
        }, index=pd.to_datetime(ohlcv_raw.get('dates', [])))

        predictor = StockPricePredictor(symbol=symbol)

        result = {}

        if action in ['fit', 'both']:
            fit_result = predictor.fit(df)
            result['fit'] = fit_result

        if action in ['predict', 'both']:
            pred_result = predictor.predict(steps_ahead=steps_ahead, method=method)
            result['predictions'] = pred_result

            # Generate signals
            signals = predictor.calculate_signals(pred_result)
            result['signals'] = signals

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({'error': str(e)}))


if __name__ == '__main__':
    main()
