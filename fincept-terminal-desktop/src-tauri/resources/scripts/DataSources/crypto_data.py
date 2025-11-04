"""
Cryptocurrency Data Service
Kripto para verileri için kapsamlı servis (Binance, CoinGecko, CoinMarketCap)
"""

import json
import sys
from datetime import datetime, timedelta
from typing import Dict, List, Optional
import requests

# Popüler kripto paralar
TOP_CRYPTOCURRENCIES = [
    # Major Coins
    "BTC",   # Bitcoin
    "ETH",   # Ethereum
    "USDT",  # Tether
    "BNB",   # Binance Coin
    "XRP",   # Ripple
    "ADA",   # Cardano
    "SOL",   # Solana
    "DOGE",  # Dogecoin
    "TRX",   # TRON
    "MATIC", # Polygon

    # DeFi
    "UNI",   # Uniswap
    "LINK",  # Chainlink
    "AVAX",  # Avalanche
    "AAVE",  # Aave

    # Türk kripto projeleri
    "CHZ",   # Chiliz (Türk kökenli)
]

class CryptoDataService:
    """Kripto para veri servisi"""

    def __init__(self):
        self.binance_base = "https://api.binance.com/api/v3"
        self.coingecko_base = "https://api.coingecko.com/api/v3"

    def get_binance_price(self, symbol: str, quote: str = "USDT") -> Dict:
        """
        Binance'den anlık fiyat al

        Args:
            symbol: Kripto sembolü (BTC, ETH, vb.)
            quote: Fiyat çifti (USDT, BUSD, TRY)

        Returns:
            Fiyat bilgisi
        """
        try:
            pair = f"{symbol}{quote}"
            url = f"{self.binance_base}/ticker/24hr"
            params = {"symbol": pair}

            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()

            data = response.json()

            return {
                'success': True,
                'symbol': symbol,
                'quote': quote,
                'pair': pair,
                'price': float(data['lastPrice']),
                'change_24h': float(data['priceChange']),
                'change_percent_24h': float(data['priceChangePercent']),
                'high_24h': float(data['highPrice']),
                'low_24h': float(data['lowPrice']),
                'volume_24h': float(data['volume']),
                'quote_volume_24h': float(data['quoteVolume']),
                'timestamp': datetime.now().isoformat()
            }

        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'symbol': symbol
            }

    def get_binance_try_price(self, symbol: str) -> Dict:
        """Türk Lirası cinsinden fiyat"""
        return self.get_binance_price(symbol, quote="TRY")

    def get_historical_klines(self, symbol: str, interval: str = "1d",
                             limit: int = 365, quote: str = "USDT") -> Dict:
        """
        Binance geçmiş fiyat verileri (Klines/Candlestick)

        Args:
            symbol: Kripto sembolü
            interval: Zaman aralığı (1m, 5m, 15m, 1h, 4h, 1d, 1w, 1M)
            limit: Veri sayısı (max 1000)
            quote: Fiyat çifti

        Returns:
            OHLCV verisi
        """
        try:
            pair = f"{symbol}{quote}"
            url = f"{self.binance_base}/klines"
            params = {
                "symbol": pair,
                "interval": interval,
                "limit": min(limit, 1000)
            }

            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()

            klines = response.json()

            dates = []
            open_prices = []
            high_prices = []
            low_prices = []
            close_prices = []
            volumes = []

            for kline in klines:
                timestamp = kline[0]
                dates.append(datetime.fromtimestamp(timestamp / 1000).strftime('%Y-%m-%d %H:%M:%S'))
                open_prices.append(float(kline[1]))
                high_prices.append(float(kline[2]))
                low_prices.append(float(kline[3]))
                close_prices.append(float(kline[4]))
                volumes.append(float(kline[5]))

            return {
                'success': True,
                'symbol': symbol,
                'quote': quote,
                'pair': pair,
                'interval': interval,
                'dates': dates,
                'open': open_prices,
                'high': high_prices,
                'low': low_prices,
                'close': close_prices,
                'volume': volumes,
                'data_points': len(dates)
            }

        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'symbol': symbol
            }

    def get_coingecko_info(self, coin_id: str) -> Dict:
        """
        CoinGecko'dan detaylı coin bilgisi

        Args:
            coin_id: CoinGecko coin ID (bitcoin, ethereum, vb.)

        Returns:
            Detaylı bilgi
        """
        try:
            url = f"{self.coingecko_base}/coins/{coin_id}"
            params = {
                'localization': 'false',
                'tickers': 'false',
                'community_data': 'true',
                'developer_data': 'true'
            }

            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()

            data = response.json()

            market_data = data.get('market_data', {})

            return {
                'success': True,
                'id': coin_id,
                'symbol': data.get('symbol', '').upper(),
                'name': data.get('name', ''),
                'current_price_usd': market_data.get('current_price', {}).get('usd', 0),
                'current_price_try': market_data.get('current_price', {}).get('try', 0),
                'market_cap_usd': market_data.get('market_cap', {}).get('usd', 0),
                'market_cap_rank': data.get('market_cap_rank', 0),
                'total_volume_usd': market_data.get('total_volume', {}).get('usd', 0),
                'high_24h': market_data.get('high_24h', {}).get('usd', 0),
                'low_24h': market_data.get('low_24h', {}).get('usd', 0),
                'price_change_24h': market_data.get('price_change_24h', 0),
                'price_change_percentage_24h': market_data.get('price_change_percentage_24h', 0),
                'price_change_percentage_7d': market_data.get('price_change_percentage_7d', 0),
                'price_change_percentage_30d': market_data.get('price_change_percentage_30d', 0),
                'circulating_supply': market_data.get('circulating_supply', 0),
                'total_supply': market_data.get('total_supply', 0),
                'max_supply': market_data.get('max_supply', 0),
                'ath': market_data.get('ath', {}).get('usd', 0),
                'ath_date': market_data.get('ath_date', {}).get('usd', ''),
                'atl': market_data.get('atl', {}).get('usd', 0),
                'atl_date': market_data.get('atl_date', {}).get('usd', '')
            }

        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'coin_id': coin_id
            }

    def get_top_cryptocurrencies(self, limit: int = 100, currency: str = 'usd') -> List[Dict]:
        """
        CoinGecko'dan top kripto paralar

        Args:
            limit: Kaç coin (max 250)
            currency: Para birimi (usd, try, eur)

        Returns:
            Top coin listesi
        """
        try:
            url = f"{self.coingecko_base}/coins/markets"
            params = {
                'vs_currency': currency,
                'order': 'market_cap_desc',
                'per_page': min(limit, 250),
                'page': 1,
                'sparkline': 'false'
            }

            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()

            coins = response.json()

            result = []
            for coin in coins:
                result.append({
                    'rank': coin.get('market_cap_rank', 0),
                    'id': coin.get('id', ''),
                    'symbol': coin.get('symbol', '').upper(),
                    'name': coin.get('name', ''),
                    'current_price': coin.get('current_price', 0),
                    'market_cap': coin.get('market_cap', 0),
                    'volume_24h': coin.get('total_volume', 0),
                    'change_24h': coin.get('price_change_percentage_24h', 0),
                    'high_24h': coin.get('high_24h', 0),
                    'low_24h': coin.get('low_24h', 0)
                })

            return result

        except Exception as e:
            return []

    def get_trending_coins(self) -> Dict:
        """CoinGecko'dan trending coinler"""
        try:
            url = f"{self.coingecko_base}/search/trending"

            response = requests.get(url, timeout=10)
            response.raise_for_status()

            data = response.json()
            trending = data.get('coins', [])

            result = []
            for item in trending:
                coin = item.get('item', {})
                result.append({
                    'id': coin.get('id', ''),
                    'symbol': coin.get('symbol', '').upper(),
                    'name': coin.get('name', ''),
                    'market_cap_rank': coin.get('market_cap_rank', 0),
                    'thumb': coin.get('thumb', ''),
                    'score': coin.get('score', 0)
                })

            return {
                'success': True,
                'trending': result,
                'timestamp': datetime.now().isoformat()
            }

        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }

    def get_global_market_data(self) -> Dict:
        """Global kripto market verileri"""
        try:
            url = f"{self.coingecko_base}/global"

            response = requests.get(url, timeout=10)
            response.raise_for_status()

            data = response.json()
            global_data = data.get('data', {})

            return {
                'success': True,
                'active_cryptocurrencies': global_data.get('active_cryptocurrencies', 0),
                'markets': global_data.get('markets', 0),
                'total_market_cap_usd': global_data.get('total_market_cap', {}).get('usd', 0),
                'total_market_cap_try': global_data.get('total_market_cap', {}).get('try', 0),
                'total_volume_24h_usd': global_data.get('total_volume', {}).get('usd', 0),
                'market_cap_change_24h': global_data.get('market_cap_change_percentage_24h_usd', 0),
                'bitcoin_dominance': global_data.get('market_cap_percentage', {}).get('btc', 0),
                'ethereum_dominance': global_data.get('market_cap_percentage', {}).get('eth', 0),
                'timestamp': datetime.now().isoformat()
            }

        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }

    def get_fear_greed_index(self) -> Dict:
        """Crypto Fear & Greed Index"""
        try:
            url = "https://api.alternative.me/fng/"
            params = {'limit': 1}

            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()

            data = response.json()
            fng_data = data['data'][0]

            value = int(fng_data['value'])
            classification = fng_data['value_classification']

            return {
                'success': True,
                'value': value,
                'classification': classification,
                'timestamp': fng_data['timestamp']
            }

        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }

    def get_market_summary(self) -> Dict:
        """Kripto market özeti"""
        summary = {
            'success': True,
            'timestamp': datetime.now().isoformat(),
            'global_data': self.get_global_market_data(),
            'fear_greed_index': self.get_fear_greed_index(),
            'top_10': self.get_top_cryptocurrencies(10, 'usd'),
            'top_10_try': self.get_top_cryptocurrencies(10, 'try'),
            'trending': self.get_trending_coins()
        }

        return summary


def main():
    """
    Command-line interface

    Input JSON:
    {
        "action": "binance_price" | "binance_try" | "historical" | "coingecko_info" |
                  "top_coins" | "trending" | "global_data" | "fear_greed" | "market_summary",
        "symbol": "BTC",
        "coin_id": "bitcoin",
        "quote": "USDT",
        "interval": "1d",
        "limit": 365,
        "currency": "usd"
    }
    """
    try:
        input_data = json.loads(sys.stdin.read())

        service = CryptoDataService()
        action = input_data.get('action', 'market_summary')

        if action == 'binance_price':
            symbol = input_data.get('symbol', 'BTC')
            quote = input_data.get('quote', 'USDT')
            result = service.get_binance_price(symbol, quote)

        elif action == 'binance_try':
            symbol = input_data.get('symbol', 'BTC')
            result = service.get_binance_try_price(symbol)

        elif action == 'historical':
            symbol = input_data.get('symbol', 'BTC')
            interval = input_data.get('interval', '1d')
            limit = input_data.get('limit', 365)
            quote = input_data.get('quote', 'USDT')
            result = service.get_historical_klines(symbol, interval, limit, quote)

        elif action == 'coingecko_info':
            coin_id = input_data.get('coin_id', 'bitcoin')
            result = service.get_coingecko_info(coin_id)

        elif action == 'top_coins':
            limit = input_data.get('limit', 100)
            currency = input_data.get('currency', 'usd')
            result = {
                'success': True,
                'coins': service.get_top_cryptocurrencies(limit, currency)
            }

        elif action == 'trending':
            result = service.get_trending_coins()

        elif action == 'global_data':
            result = service.get_global_market_data()

        elif action == 'fear_greed':
            result = service.get_fear_greed_index()

        elif action == 'market_summary':
            result = service.get_market_summary()

        else:
            result = {
                'success': False,
                'error': f'Unknown action: {action}'
            }

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({
            'success': False,
            'error': str(e)
        }))


if __name__ == '__main__':
    main()
