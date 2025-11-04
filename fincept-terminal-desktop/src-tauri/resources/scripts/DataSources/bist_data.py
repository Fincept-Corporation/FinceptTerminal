"""
BIST (Borsa Istanbul) Data Service
Türkiye hisse senedi piyasası verileri için servis
"""

import json
import sys
from datetime import datetime, timedelta
from typing import Dict, List, Optional
import requests

# BIST 100 ve popüler hisseler
BIST_100_SYMBOLS = [
    # Bankacılık
    "AKBNK.IS",  # Akbank
    "GARAN.IS",  # Garanti BBVA
    "ISCTR.IS",  # İş Bankası (C)
    "YKBNK.IS",  # Yapı Kredi
    "HALKB.IS",  # Halkbank
    "VAKBN.IS",  # Vakıfbank

    # Holding
    "THYAO.IS",  # Türk Hava Yolları
    "KCHOL.IS",  # Koç Holding
    "SAHOL.IS",  # Sabancı Holding
    "EKGYO.IS",  # Emlak Konut GYO
    "EREGL.IS",  # Ereğli Demir Çelik

    # Teknoloji & Telekomünikasyon
    "ASELS.IS",  # Aselsan
    "TTKOM.IS",  # Türk Telekom
    "LOGO.IS",   # Logo Yazılım

    # Enerji
    "AKSEN.IS",  # Aksa Enerji
    "AKENR.IS",  # Ak Enerji
    "PETKM.IS",  # Petkim

    # Perakende & Tüketim
    "BIMAS.IS",  # BİM
    "MGROS.IS",  # Migros
    "SOKM.IS",   # Şok Marketler

    # Otomotiv
    "TOASO.IS",  # Tofaş
    "FROTO.IS",  # Ford Otosan

    # Dayanıklı Tüketim
    "VESBE.IS",  # Vestel Beyaz Eşya
    "ARCLK.IS",  # Arçelik

    # Sigorta
    "AKGRT.IS",  # Aksigorta
]

class BISTDataService:
    """Borsa Istanbul veri servisi"""

    def __init__(self):
        self.base_url = "https://query1.finance.yahoo.com/v8/finance"

    def get_stock_info(self, symbol: str) -> Dict:
        """
        BIST hissesi hakkında bilgi al

        Args:
            symbol: Hisse sembolü (örn: AKBNK.IS)

        Returns:
            Hisse bilgileri
        """
        try:
            url = f"{self.base_url}/chart/{symbol}"
            params = {
                "interval": "1d",
                "range": "1d"
            }

            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()

            data = response.json()
            result = data['chart']['result'][0]
            meta = result['meta']

            quote = result['indicators']['quote'][0]
            current_price = quote['close'][-1] if quote['close'] else meta.get('regularMarketPrice', 0)

            return {
                'success': True,
                'symbol': symbol,
                'name': meta.get('longName', symbol),
                'currency': meta.get('currency', 'TRY'),
                'exchange': meta.get('exchangeName', 'BIST'),
                'current_price': current_price,
                'previous_close': meta.get('previousClose', 0),
                'day_high': meta.get('regularMarketDayHigh', 0),
                'day_low': meta.get('regularMarketDayLow', 0),
                'volume': meta.get('regularMarketVolume', 0),
                'market_cap': meta.get('marketCap', 0),
            }

        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'symbol': symbol
            }

    def get_historical_data(self, symbol: str, period: str = "1y", interval: str = "1d") -> Dict:
        """
        Geçmiş fiyat verilerini al

        Args:
            symbol: Hisse sembolü
            period: Zaman periyodu (1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y, max)
            interval: Veri aralığı (1m, 5m, 15m, 1h, 1d, 1wk, 1mo)

        Returns:
            Geçmiş veri
        """
        try:
            url = f"{self.base_url}/chart/{symbol}"
            params = {
                "range": period,
                "interval": interval
            }

            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()

            data = response.json()
            result = data['chart']['result'][0]

            timestamps = result['timestamp']
            quote = result['indicators']['quote'][0]

            # Convert timestamps to dates
            dates = [datetime.fromtimestamp(ts).strftime('%Y-%m-%d') for ts in timestamps]

            return {
                'success': True,
                'symbol': symbol,
                'dates': dates,
                'open': quote.get('open', []),
                'high': quote.get('high', []),
                'low': quote.get('low', []),
                'close': quote.get('close', []),
                'volume': quote.get('volume', []),
                'data_points': len(dates)
            }

        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'symbol': symbol
            }

    def get_bist_100_summary(self) -> Dict:
        """BIST 100 endeksi özeti"""
        try:
            # BIST 100 index
            symbol = "XU100.IS"

            url = f"{self.base_url}/chart/{symbol}"
            params = {
                "interval": "1d",
                "range": "5d"
            }

            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()

            data = response.json()
            result = data['chart']['result'][0]
            meta = result['meta']
            quote = result['indicators']['quote'][0]

            current = quote['close'][-1]
            previous = quote['close'][-2] if len(quote['close']) > 1 else meta.get('previousClose', current)

            change = current - previous
            change_percent = (change / previous) * 100 if previous else 0

            return {
                'success': True,
                'index': 'BIST 100',
                'symbol': symbol,
                'current': current,
                'change': change,
                'change_percent': change_percent,
                'high': max(quote['high']) if quote['high'] else 0,
                'low': min(quote['low']) if quote['low'] else 0,
                'volume': sum(quote['volume']) if quote['volume'] else 0
            }

        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }

    def get_top_gainers(self, limit: int = 10) -> List[Dict]:
        """En çok yükselen hisseler"""
        gainers = []

        for symbol in BIST_100_SYMBOLS[:20]:  # İlk 20 hisseyi kontrol et
            info = self.get_stock_info(symbol)
            if info['success']:
                current = info['current_price']
                previous = info['previous_close']

                if current and previous:
                    change_percent = ((current - previous) / previous) * 100

                    gainers.append({
                        'symbol': symbol,
                        'name': info['name'],
                        'current_price': current,
                        'change_percent': change_percent
                    })

        # Sort by change percent
        gainers.sort(key=lambda x: x['change_percent'], reverse=True)

        return gainers[:limit]

    def get_top_losers(self, limit: int = 10) -> List[Dict]:
        """En çok düşen hisseler"""
        losers = []

        for symbol in BIST_100_SYMBOLS[:20]:
            info = self.get_stock_info(symbol)
            if info['success']:
                current = info['current_price']
                previous = info['previous_close']

                if current and previous:
                    change_percent = ((current - previous) / previous) * 100

                    losers.append({
                        'symbol': symbol,
                        'name': info['name'],
                        'current_price': current,
                        'change_percent': change_percent
                    })

        # Sort by change percent (ascending)
        losers.sort(key=lambda x: x['change_percent'])

        return losers[:limit]

    def get_market_summary(self) -> Dict:
        """Piyasa özeti"""
        summary = {
            'success': True,
            'timestamp': datetime.now().isoformat(),
            'bist_100': self.get_bist_100_summary(),
            'top_gainers': self.get_top_gainers(5),
            'top_losers': self.get_top_losers(5),
            'most_traded': []  # Volume bazlı
        }

        return summary


def main():
    """
    Command-line interface

    Input JSON:
    {
        "action": "stock_info" | "historical" | "bist_100" | "top_gainers" | "top_losers" | "market_summary",
        "symbol": "AKBNK.IS",
        "period": "1y",
        "interval": "1d",
        "limit": 10
    }
    """
    try:
        input_data = json.loads(sys.stdin.read())

        service = BISTDataService()
        action = input_data.get('action', 'market_summary')

        if action == 'stock_info':
            symbol = input_data.get('symbol', 'AKBNK.IS')
            result = service.get_stock_info(symbol)

        elif action == 'historical':
            symbol = input_data.get('symbol', 'AKBNK.IS')
            period = input_data.get('period', '1y')
            interval = input_data.get('interval', '1d')
            result = service.get_historical_data(symbol, period, interval)

        elif action == 'bist_100':
            result = service.get_bist_100_summary()

        elif action == 'top_gainers':
            limit = input_data.get('limit', 10)
            result = {
                'success': True,
                'gainers': service.get_top_gainers(limit)
            }

        elif action == 'top_losers':
            limit = input_data.get('limit', 10)
            result = {
                'success': True,
                'losers': service.get_top_losers(limit)
            }

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
