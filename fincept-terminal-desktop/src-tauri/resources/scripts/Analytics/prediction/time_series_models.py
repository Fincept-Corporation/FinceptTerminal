"""
Time Series Prediction Models for FinceptTerminal
Supports ARIMA, Prophet, LSTM, and Ensemble methods
"""

import numpy as np
import pandas as pd
from typing import Dict, List, Tuple, Optional, Any
import json
import sys
from datetime import datetime, timedelta

# Statistical & Time Series
try:
    from statsmodels.tsa.arima.model import ARIMA
    from statsmodels.tsa.statespace.sarimax import SARIMAX
    from statsmodels.tsa.stattools import adfuller
except ImportError:
    ARIMA = None
    SARIMAX = None

try:
    from prophet import Prophet
except ImportError:
    Prophet = None

# Deep Learning
try:
    import tensorflow as tf
    from tensorflow import keras
    from tensorflow.keras.models import Sequential
    from tensorflow.keras.layers import LSTM, Dense, Dropout
    from tensorflow.keras.callbacks import EarlyStopping
    from sklearn.preprocessing import MinMaxScaler
except ImportError:
    tf = None
    keras = None


class TimeSeriesPredictor:
    """
    Unified interface for multiple time-series prediction models
    Supports ARIMA, SARIMA, Prophet, LSTM, and Ensemble methods
    """

    def __init__(self, model_type: str = "arima"):
        """
        Initialize the predictor

        Args:
            model_type: Type of model ('arima', 'sarima', 'prophet', 'lstm', 'ensemble')
        """
        self.model_type = model_type.lower()
        self.model = None
        self.scaler = None
        self.data = None
        self.is_fitted = False
        self.training_data = None

    def check_stationarity(self, timeseries: pd.Series, significance_level: float = 0.05) -> Dict:
        """
        Check if time series is stationary using Augmented Dickey-Fuller test

        Args:
            timeseries: Time series data
            significance_level: Significance level for ADF test

        Returns:
            Dict with test results
        """
        if ARIMA is None:
            return {"error": "statsmodels not installed"}

        result = adfuller(timeseries.dropna(), autolag='AIC')

        return {
            'adf_statistic': result[0],
            'p_value': result[1],
            'critical_values': result[4],
            'is_stationary': result[1] < significance_level,
            'recommendation': 'No differencing needed' if result[1] < significance_level else 'Differencing recommended'
        }

    def fit_arima(self, data: pd.Series, order: Tuple[int, int, int] = (1, 1, 1),
                  auto_order: bool = True) -> Dict:
        """
        Fit ARIMA model

        Args:
            data: Time series data
            order: (p, d, q) order for ARIMA
            auto_order: Auto-select order based on AIC

        Returns:
            Dict with fitting results
        """
        if ARIMA is None:
            return {"error": "statsmodels not installed. Install with: pip install statsmodels"}

        self.training_data = data

        try:
            # Auto-select order if requested
            if auto_order:
                best_aic = np.inf
                best_order = order

                # Grid search for best order
                for p in range(0, 3):
                    for d in range(0, 2):
                        for q in range(0, 3):
                            try:
                                model = ARIMA(data, order=(p, d, q))
                                fitted = model.fit()
                                if fitted.aic < best_aic:
                                    best_aic = fitted.aic
                                    best_order = (p, d, q)
                            except:
                                continue

                order = best_order

            # Fit final model
            self.model = ARIMA(data, order=order)
            fitted_model = self.model.fit()
            self.is_fitted = True

            return {
                'success': True,
                'model_type': 'ARIMA',
                'order': order,
                'aic': fitted_model.aic,
                'bic': fitted_model.bic,
                'summary': str(fitted_model.summary())
            }

        except Exception as e:
            return {'error': str(e)}

    def fit_sarima(self, data: pd.Series, order: Tuple[int, int, int] = (1, 1, 1),
                   seasonal_order: Tuple[int, int, int, int] = (1, 1, 1, 12)) -> Dict:
        """
        Fit SARIMA model for seasonal data

        Args:
            data: Time series data
            order: (p, d, q) order
            seasonal_order: (P, D, Q, s) seasonal order

        Returns:
            Dict with fitting results
        """
        if SARIMAX is None:
            return {"error": "statsmodels not installed"}

        self.training_data = data

        try:
            self.model = SARIMAX(data, order=order, seasonal_order=seasonal_order)
            fitted_model = self.model.fit(disp=False)
            self.is_fitted = True

            return {
                'success': True,
                'model_type': 'SARIMA',
                'order': order,
                'seasonal_order': seasonal_order,
                'aic': fitted_model.aic,
                'bic': fitted_model.bic
            }

        except Exception as e:
            return {'error': str(e)}

    def fit_prophet(self, df: pd.DataFrame, periods: int = 30,
                    yearly_seasonality: bool = True,
                    weekly_seasonality: bool = True,
                    daily_seasonality: bool = False) -> Dict:
        """
        Fit Facebook Prophet model

        Args:
            df: DataFrame with 'ds' (date) and 'y' (value) columns
            periods: Number of periods to forecast
            yearly_seasonality: Include yearly seasonality
            weekly_seasonality: Include weekly seasonality
            daily_seasonality: Include daily seasonality

        Returns:
            Dict with fitting results
        """
        if Prophet is None:
            return {"error": "prophet not installed. Install with: pip install prophet"}

        try:
            # Prepare data
            if 'ds' not in df.columns or 'y' not in df.columns:
                df = pd.DataFrame({
                    'ds': df.index,
                    'y': df.values if isinstance(df, pd.Series) else df.iloc[:, 0].values
                })

            self.training_data = df

            # Initialize and fit Prophet
            self.model = Prophet(
                yearly_seasonality=yearly_seasonality,
                weekly_seasonality=weekly_seasonality,
                daily_seasonality=daily_seasonality,
                changepoint_prior_scale=0.05
            )

            self.model.fit(df)
            self.is_fitted = True

            return {
                'success': True,
                'model_type': 'Prophet',
                'n_changepoints': len(self.model.changepoints),
                'yearly_seasonality': yearly_seasonality,
                'weekly_seasonality': weekly_seasonality
            }

        except Exception as e:
            return {'error': str(e)}

    def fit_lstm(self, data: pd.Series, lookback: int = 60,
                 units: int = 128, epochs: int = 100,
                 batch_size: int = 32, validation_split: float = 0.2) -> Dict:
        """
        Fit LSTM neural network model

        Args:
            data: Time series data
            lookback: Number of previous time steps to use
            units: Number of LSTM units
            epochs: Number of training epochs
            batch_size: Batch size for training
            validation_split: Validation split ratio

        Returns:
            Dict with fitting results
        """
        if tf is None or keras is None:
            return {"error": "tensorflow not installed. Install with: pip install tensorflow"}

        try:
            self.training_data = data

            # Normalize data
            self.scaler = MinMaxScaler(feature_range=(0, 1))
            scaled_data = self.scaler.fit_transform(data.values.reshape(-1, 1))

            # Create sequences
            X, y = [], []
            for i in range(lookback, len(scaled_data)):
                X.append(scaled_data[i-lookback:i, 0])
                y.append(scaled_data[i, 0])

            X, y = np.array(X), np.array(y)
            X = np.reshape(X, (X.shape[0], X.shape[1], 1))

            # Build LSTM model
            self.model = Sequential([
                LSTM(units=units, return_sequences=True, input_shape=(lookback, 1)),
                Dropout(0.2),
                LSTM(units=units, return_sequences=False),
                Dropout(0.2),
                Dense(units=25),
                Dense(units=1)
            ])

            self.model.compile(optimizer='adam', loss='mean_squared_error')

            # Early stopping
            early_stop = EarlyStopping(monitor='val_loss', patience=10, restore_best_weights=True)

            # Train model
            history = self.model.fit(
                X, y,
                epochs=epochs,
                batch_size=batch_size,
                validation_split=validation_split,
                callbacks=[early_stop],
                verbose=0
            )

            self.is_fitted = True
            self.lookback = lookback

            return {
                'success': True,
                'model_type': 'LSTM',
                'lookback': lookback,
                'units': units,
                'final_loss': float(history.history['loss'][-1]),
                'final_val_loss': float(history.history['val_loss'][-1]),
                'epochs_trained': len(history.history['loss'])
            }

        except Exception as e:
            return {'error': str(e)}

    def predict(self, steps_ahead: int, confidence_level: float = 0.95) -> Dict:
        """
        Generate predictions with confidence intervals

        Args:
            steps_ahead: Number of steps to forecast
            confidence_level: Confidence level for intervals (0-1)

        Returns:
            Dict with predictions and confidence intervals
        """
        if not self.is_fitted:
            return {"error": "Model not fitted. Call fit method first."}

        try:
            if self.model_type == 'arima' or self.model_type == 'sarima':
                return self._predict_arima(steps_ahead, confidence_level)
            elif self.model_type == 'prophet':
                return self._predict_prophet(steps_ahead)
            elif self.model_type == 'lstm':
                return self._predict_lstm(steps_ahead)
            else:
                return {"error": f"Unknown model type: {self.model_type}"}

        except Exception as e:
            return {'error': str(e)}

    def _predict_arima(self, steps_ahead: int, confidence_level: float) -> Dict:
        """ARIMA prediction implementation"""
        forecast_result = self.model.fit().get_forecast(steps=steps_ahead)
        forecast = forecast_result.predicted_mean
        conf_int = forecast_result.conf_int(alpha=1-confidence_level)

        return {
            'success': True,
            'model_type': self.model_type.upper(),
            'forecast': forecast.tolist(),
            'lower_ci': conf_int.iloc[:, 0].tolist(),
            'upper_ci': conf_int.iloc[:, 1].tolist(),
            'confidence_level': confidence_level,
            'steps_ahead': steps_ahead,
            'forecast_dates': [str(d) for d in forecast.index] if hasattr(forecast.index, 'tolist') else None
        }

    def _predict_prophet(self, steps_ahead: int) -> Dict:
        """Prophet prediction implementation"""
        # Create future dataframe
        future = self.model.make_future_dataframe(periods=steps_ahead)
        forecast = self.model.predict(future)

        # Extract predictions
        predictions = forecast.tail(steps_ahead)

        return {
            'success': True,
            'model_type': 'Prophet',
            'forecast': predictions['yhat'].tolist(),
            'lower_ci': predictions['yhat_lower'].tolist(),
            'upper_ci': predictions['yhat_upper'].tolist(),
            'trend': predictions['trend'].tolist(),
            'steps_ahead': steps_ahead,
            'forecast_dates': predictions['ds'].astype(str).tolist()
        }

    def _predict_lstm(self, steps_ahead: int) -> Dict:
        """LSTM prediction implementation"""
        # Get last lookback values
        last_values = self.training_data.values[-self.lookback:]
        scaled_values = self.scaler.transform(last_values.reshape(-1, 1))

        predictions = []
        current_sequence = scaled_values.reshape(1, self.lookback, 1)

        # Multi-step prediction
        for _ in range(steps_ahead):
            pred = self.model.predict(current_sequence, verbose=0)
            predictions.append(pred[0, 0])

            # Update sequence
            current_sequence = np.append(current_sequence[:, 1:, :],
                                        pred.reshape(1, 1, 1), axis=1)

        # Inverse transform
        predictions = self.scaler.inverse_transform(np.array(predictions).reshape(-1, 1))

        # Simple confidence intervals (±2 std)
        std = np.std(predictions)

        return {
            'success': True,
            'model_type': 'LSTM',
            'forecast': predictions.flatten().tolist(),
            'lower_ci': (predictions - 2*std).flatten().tolist(),
            'upper_ci': (predictions + 2*std).flatten().tolist(),
            'steps_ahead': steps_ahead,
            'note': 'Confidence intervals are approximate (±2 std)'
        }

    def calculate_accuracy_metrics(self, actual: pd.Series, predicted: np.ndarray) -> Dict:
        """
        Calculate prediction accuracy metrics

        Args:
            actual: Actual values
            predicted: Predicted values

        Returns:
            Dict with accuracy metrics
        """
        actual = np.array(actual)
        predicted = np.array(predicted)

        # Ensure same length
        min_len = min(len(actual), len(predicted))
        actual = actual[:min_len]
        predicted = predicted[:min_len]

        # Calculate metrics
        mse = np.mean((actual - predicted) ** 2)
        rmse = np.sqrt(mse)
        mae = np.mean(np.abs(actual - predicted))
        mape = np.mean(np.abs((actual - predicted) / actual)) * 100

        # Directional accuracy
        actual_direction = np.diff(actual) > 0
        predicted_direction = np.diff(predicted) > 0
        directional_accuracy = np.mean(actual_direction == predicted_direction) * 100

        return {
            'mse': float(mse),
            'rmse': float(rmse),
            'mae': float(mae),
            'mape': float(mape),
            'directional_accuracy': float(directional_accuracy)
        }


def main():
    """
    Main function for command-line usage
    Expected input format (JSON):
    {
        "data": [list of values],
        "dates": [list of dates (optional)],
        "model_type": "arima|prophet|lstm",
        "steps_ahead": 30,
        "params": {model-specific parameters}
    }
    """
    try:
        # Read input from stdin
        input_data = json.loads(sys.stdin.read())

        data = input_data.get('data', [])
        dates = input_data.get('dates', None)
        model_type = input_data.get('model_type', 'arima')
        steps_ahead = input_data.get('steps_ahead', 30)
        params = input_data.get('params', {})

        # Create pandas Series
        if dates:
            series = pd.Series(data, index=pd.to_datetime(dates))
        else:
            series = pd.Series(data)

        # Initialize predictor
        predictor = TimeSeriesPredictor(model_type=model_type)

        # Fit model
        if model_type == 'arima':
            fit_result = predictor.fit_arima(series, **params)
        elif model_type == 'sarima':
            fit_result = predictor.fit_sarima(series, **params)
        elif model_type == 'prophet':
            df = pd.DataFrame({'ds': series.index, 'y': series.values})
            fit_result = predictor.fit_prophet(df, **params)
        elif model_type == 'lstm':
            fit_result = predictor.fit_lstm(series, **params)
        else:
            fit_result = {'error': f'Unknown model type: {model_type}'}

        if 'error' in fit_result:
            print(json.dumps(fit_result))
            return

        # Make predictions
        predictions = predictor.predict(steps_ahead)

        # Combine results
        result = {
            'fit_results': fit_result,
            'predictions': predictions,
            'model_info': {
                'model_type': model_type,
                'data_points': len(data),
                'steps_ahead': steps_ahead
            }
        }

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({'error': str(e)}))


if __name__ == '__main__':
    main()
