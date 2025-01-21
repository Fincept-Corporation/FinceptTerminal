import logging
import pandas as pd

class TechnicalAnalyst:
    def __init__(self):
        self.logger = logging.getLogger(self.__class__.__name__)

    def analyze_technical_indicators(self, market_data: pd.DataFrame) -> dict:
        short_ma = market_data["Close"].rolling(window=20).mean().iloc[-1]
        long_ma = market_data["Close"].rolling(window=50).mean().iloc[-1]
        signal = "BUY" if short_ma > long_ma else "SELL"
        rsi_val = self._calculate_rsi(market_data["Close"], 14)
        return {
            "ma_signal": signal,
            "short_ma": round(short_ma, 2),
            "long_ma": round(long_ma, 2),
            "rsi": round(rsi_val, 2),
        }

    def _calculate_rsi(self, series: pd.Series, period: int) -> float:
        delta = series.diff()
        gains = delta.where(delta > 0, 0).rolling(period).mean()
        losses = (-delta.where(delta < 0, 0)).rolling(period).mean()
        rs = gains / (losses + 1e-9)
        return 100 - 100 / (1 + rs.iloc[-1])
