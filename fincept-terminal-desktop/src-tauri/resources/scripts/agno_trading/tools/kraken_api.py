"""
Kraken REST API Integration
Fetches market data from Kraken exchange (NO WebSocket - REST only)
"""

import requests
from typing import Dict, Any, List, Optional
from datetime import datetime
import time


class KrakenAPI:
    """Kraken REST API client for market data"""

    BASE_URL = "https://api.kraken.com/0/public"

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0'
        })
        self.last_request_time = 0
        self.min_request_interval = 1.0  # 1 second between requests

    def _rate_limit(self):
        """Ensure we don't exceed rate limits"""
        elapsed = time.time() - self.last_request_time
        if elapsed < self.min_request_interval:
            time.sleep(self.min_request_interval - elapsed)
        self.last_request_time = time.time()

    def get_ticker(self, pair: str = "XBTUSD") -> Dict[str, Any]:
        """
        Get current ticker data

        Args:
            pair: Trading pair (e.g., XBTUSD for BTC/USD)

        Returns:
            Dict with current price, bid, ask, volume, etc.
        """
        self._rate_limit()

        try:
            response = self.session.get(
                f"{self.BASE_URL}/Ticker",
                params={"pair": pair},
                timeout=10
            )
            response.raise_for_status()
            data = response.json()

            if data.get('error') and len(data['error']) > 0:
                return {"error": f"Kraken API error: {data['error']}"}

            result = data.get('result', {})
            if not result or pair not in result:
                # Try alternate pair name
                alt_pair = list(result.keys())[0] if result else None
                if alt_pair:
                    ticker = result[alt_pair]
                else:
                    return {"error": "No ticker data returned"}
            else:
                ticker = result[pair]

            # Parse Kraken ticker format
            # [ask, bid, last_price, volume, vwap, trades, low, high, open]
            return {
                "symbol": pair,
                "price": float(ticker['c'][0]),  # Last trade price
                "bid": float(ticker['b'][0]),    # Best bid
                "ask": float(ticker['a'][0]),    # Best ask
                "high_24h": float(ticker['h'][1]),  # 24h high
                "low_24h": float(ticker['l'][1]),   # 24h low
                "volume_24h": float(ticker['v'][1]),  # 24h volume
                "vwap_24h": float(ticker['p'][1]),   # 24h VWAP
                "open": float(ticker['o']),      # Open price
                "timestamp": datetime.now().isoformat(),
                "source": "kraken_rest"
            }

        except Exception as e:
            return {"error": f"Failed to fetch ticker: {str(e)}"}

    def get_ohlc(self, pair: str = "XBTUSD", interval: int = 60) -> Dict[str, Any]:
        """
        Get OHLC (candlestick) data

        Args:
            pair: Trading pair
            interval: Time interval in minutes (1, 5, 15, 30, 60, 240, 1440)

        Returns:
            Dict with OHLC data
        """
        self._rate_limit()

        try:
            response = self.session.get(
                f"{self.BASE_URL}/OHLC",
                params={"pair": pair, "interval": interval},
                timeout=10
            )
            response.raise_for_status()
            data = response.json()

            if data.get('error') and len(data['error']) > 0:
                return {"error": f"Kraken API error: {data['error']}"}

            result = data.get('result', {})
            pair_data = None
            for key in result:
                if key != 'last':
                    pair_data = result[key]
                    break

            if not pair_data:
                return {"error": "No OHLC data returned"}

            # Get last 50 candles
            candles = []
            for candle in pair_data[-50:]:
                candles.append({
                    "time": int(candle[0]),
                    "open": float(candle[1]),
                    "high": float(candle[2]),
                    "low": float(candle[3]),
                    "close": float(candle[4]),
                    "volume": float(candle[6])
                })

            return {
                "symbol": pair,
                "interval": interval,
                "candles": candles,
                "count": len(candles),
                "timestamp": datetime.now().isoformat(),
                "source": "kraken_rest"
            }

        except Exception as e:
            return {"error": f"Failed to fetch OHLC: {str(e)}"}


# Singleton instance
_kraken_api = None

def get_kraken_api() -> KrakenAPI:
    """Get singleton Kraken API instance"""
    global _kraken_api
    if _kraken_api is None:
        _kraken_api = KrakenAPI()
    return _kraken_api
