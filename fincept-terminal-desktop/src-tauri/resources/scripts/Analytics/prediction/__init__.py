"""
AI Prediction Module for FinceptTerminal
Comprehensive time series forecasting and stock price prediction
"""

from .time_series_models import TimeSeriesPredictor
from .stock_price_predictor import StockPricePredictor
from .volatility_forecaster import VolatilityForecaster
from .backtesting_engine import BacktestingEngine

__all__ = [
    'TimeSeriesPredictor',
    'StockPricePredictor',
    'VolatilityForecaster',
    'BacktestingEngine'
]

__version__ = '1.0.0'
