"""
Turkish Market (BIST) & Crypto Prediction
BIST hisseleri ve kripto paralar için AI tahmin sistemi
"""

import json
import sys
import os
from datetime import datetime
from typing import Dict, List

# Add parent directories to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

# Import prediction modules
# Import simple prediction (no dependencies)
try:
    from simple_prediction_demo import simple_prediction
except ImportError:
    simple_prediction = None

# Import advanced predictor (requires numpy, etc.)
try:
    from stock_price_predictor import StockPricePredictor
    ADVANCED_AVAILABLE = True
except ImportError:
    ADVANCED_AVAILABLE = False
    StockPricePredictor = None

# Import data services
try:
    from DataSources.bist_data import BISTDataService
    from DataSources.crypto_data import CryptoDataService
except ImportError:
    # Fallback imports
    try:
        import sys
        scripts_path = os.path.join(os.path.dirname(__file__), '..', '..')
        sys.path.insert(0, scripts_path)
        from DataSources.bist_data import BISTDataService
        from DataSources.crypto_data import CryptoDataService
    except:
        BISTDataService = None
        CryptoDataService = None


class TurkishMarketPredictor:
    """BIST hisseleri için tahmin sistemi"""

    def __init__(self):
        self.bist_service = BISTDataService() if BISTDataService else None

    def predict_bist_stock(self, symbol: str, days_ahead: int = 30, use_advanced: bool = True) -> Dict:
        """
        BIST hissesi tahmini

        Args:
            symbol: BIST sembolü (örn: AKBNK.IS)
            days_ahead: Kaç gün ileri tahmin
            use_advanced: Gelişmiş modeller kullan (ARIMA, XGBoost, LSTM)

        Returns:
            Tahmin sonuçları
        """
        try:
            if not self.bist_service:
                return {'success': False, 'error': 'BIST data service not available'}

            # Get historical data
            hist_data = self.bist_service.get_historical_data(symbol, period='1y', interval='1d')

            if not hist_data['success']:
                return hist_data

            # Check if we have enough data
            if hist_data['data_points'] < 30:
                return {
                    'success': False,
                    'error': f'Insufficient data for {symbol}. Need at least 30 days, got {hist_data["data_points"]}'
                }

            # Use advanced models if available and requested
            if use_advanced and ADVANCED_AVAILABLE:
                try:
                    import pandas as pd

                    # Prepare data for stock predictor
                    df = pd.DataFrame({
                        'open': hist_data['open'],
                        'high': hist_data['high'],
                        'low': hist_data['low'],
                        'close': hist_data['close'],
                        'volume': hist_data['volume']
                    }, index=pd.to_datetime(hist_data['dates']))

                    # Initialize predictor
                    predictor = StockPricePredictor(symbol=symbol, lookback_days=252)

                    # Train models
                    fit_result = predictor.fit(df, test_size=0.2)

                    if fit_result['overall_status'] != 'success':
                        # Fallback to simple prediction
                        return self._simple_bist_prediction(hist_data, symbol, days_ahead)

                    # Generate predictions
                    pred_result = predictor.predict(steps_ahead=days_ahead, method='ensemble')

                    if pred_result['status'] != 'success':
                        return self._simple_bist_prediction(hist_data, symbol, days_ahead)

                    # Generate signals
                    signals = predictor.calculate_signals(pred_result)

                    return {
                        'success': True,
                        'market': 'BIST',
                        'symbol': symbol,
                        'method': 'Advanced Ensemble',
                        'predictions': pred_result,
                        'signals': signals['signals'],
                        'current_price': hist_data['close'][-1],
                        'currency': 'TRY'
                    }

                except Exception as e:
                    # Fallback to simple prediction
                    return self._simple_bist_prediction(hist_data, symbol, days_ahead)

            else:
                # Use simple prediction
                return self._simple_bist_prediction(hist_data, symbol, days_ahead)

        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'symbol': symbol
            }

    def _simple_bist_prediction(self, hist_data: Dict, symbol: str, days_ahead: int) -> Dict:
        """Simple prediction fallback"""
        if simple_prediction is None:
            return {'success': False, 'error': 'Prediction modules not available'}

        prices = hist_data['close']
        result = simple_prediction(prices, forecast_steps=days_ahead)

        if result['success']:
            result.update({
                'market': 'BIST',
                'symbol': symbol,
                'current_price': prices[-1],
                'currency': 'TRY'
            })

        return result

    def get_bist_recommendations(self, symbols: List[str] = None, top_n: int = 5) -> Dict:
        """
        BIST hisse önerileri

        Args:
            symbols: Analiz edilecek semboller (None ise BIST 100'den seçer)
            top_n: En iyi kaç öneri

        Returns:
            Öneri listesi
        """
        if symbols is None:
            # Use popular BIST stocks
            from DataSources.bist_data import BIST_100_SYMBOLS
            symbols = BIST_100_SYMBOLS[:20]  # İlk 20 hisseyi analiz et

        recommendations = []

        for symbol in symbols:
            try:
                prediction = self.predict_bist_stock(symbol, days_ahead=30, use_advanced=False)

                if prediction['success'] and 'signals' in prediction:
                    signal = prediction['signals'][0] if prediction['signals'] else None

                    if signal:
                        recommendations.append({
                            'symbol': symbol,
                            'signal': signal['type'],
                            'expected_return': signal.get('expected_return_1d', 0),
                            'strength': signal.get('strength', 0),
                            'confidence': signal.get('confidence', 'low'),
                            'current_price': prediction.get('current_price', 0)
                        })
            except:
                continue

        # Sort by expected return
        recommendations.sort(key=lambda x: x['expected_return'], reverse=True)

        return {
            'success': True,
            'market': 'BIST',
            'recommendations': recommendations[:top_n],
            'total_analyzed': len(symbols),
            'timestamp': datetime.now().isoformat()
        }


class CryptoPredictor:
    """Kripto para tahmin sistemi"""

    def __init__(self):
        self.crypto_service = CryptoDataService() if CryptoDataService else None

    def predict_crypto(self, symbol: str, days_ahead: int = 30, quote: str = 'USDT',
                      use_advanced: bool = True) -> Dict:
        """
        Kripto para tahmini

        Args:
            symbol: Kripto sembolü (BTC, ETH, vb.)
            days_ahead: Kaç gün ileri tahmin
            quote: Fiyat çifti (USDT, TRY, BUSD)
            use_advanced: Gelişmiş modeller kullan

        Returns:
            Tahmin sonuçları
        """
        try:
            if not self.crypto_service:
                return {'success': False, 'error': 'Crypto data service not available'}

            # Get historical data from Binance
            hist_data = self.crypto_service.get_historical_klines(
                symbol=symbol,
                interval='1d',
                limit=365,
                quote=quote
            )

            if not hist_data['success']:
                return hist_data

            # Check if we have enough data
            if hist_data['data_points'] < 30:
                return {
                    'success': False,
                    'error': f'Insufficient data for {symbol}. Need at least 30 days.'
                }

            # Use advanced models if available
            if use_advanced and ADVANCED_AVAILABLE:
                try:
                    import pandas as pd

                    # Prepare data
                    df = pd.DataFrame({
                        'open': hist_data['open'],
                        'high': hist_data['high'],
                        'low': hist_data['low'],
                        'close': hist_data['close'],
                        'volume': hist_data['volume']
                    }, index=pd.to_datetime(hist_data['dates']))

                    # Initialize predictor
                    predictor = StockPricePredictor(symbol=f"{symbol}{quote}", lookback_days=252)

                    # Train
                    fit_result = predictor.fit(df, test_size=0.2)

                    if fit_result['overall_status'] != 'success':
                        return self._simple_crypto_prediction(hist_data, symbol, quote, days_ahead)

                    # Predict
                    pred_result = predictor.predict(steps_ahead=days_ahead, method='ensemble')

                    if pred_result['status'] != 'success':
                        return self._simple_crypto_prediction(hist_data, symbol, quote, days_ahead)

                    # Signals
                    signals = predictor.calculate_signals(pred_result)

                    return {
                        'success': True,
                        'market': 'Crypto',
                        'symbol': symbol,
                        'quote': quote,
                        'method': 'Advanced Ensemble',
                        'predictions': pred_result,
                        'signals': signals['signals'],
                        'current_price': hist_data['close'][-1],
                        'volatility': self._calculate_crypto_volatility(hist_data['close'])
                    }

                except Exception as e:
                    return self._simple_crypto_prediction(hist_data, symbol, quote, days_ahead)

            else:
                return self._simple_crypto_prediction(hist_data, symbol, quote, days_ahead)

        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'symbol': symbol
            }

    def _simple_crypto_prediction(self, hist_data: Dict, symbol: str, quote: str, days_ahead: int) -> Dict:
        """Simple crypto prediction fallback"""
        if simple_prediction is None:
            return {'success': False, 'error': 'Prediction modules not available'}

        prices = hist_data['close']
        result = simple_prediction(prices, forecast_steps=days_ahead)

        if result['success']:
            result.update({
                'market': 'Crypto',
                'symbol': symbol,
                'quote': quote,
                'current_price': prices[-1],
                'volatility': self._calculate_crypto_volatility(prices)
            })

        return result

    def _calculate_crypto_volatility(self, prices: List[float]) -> float:
        """Calculate simple volatility"""
        if len(prices) < 2:
            return 0

        returns = []
        for i in range(1, len(prices)):
            ret = (prices[i] - prices[i-1]) / prices[i-1]
            returns.append(ret)

        # Standard deviation
        mean_return = sum(returns) / len(returns)
        variance = sum((r - mean_return) ** 2 for r in returns) / len(returns)
        volatility = (variance ** 0.5) * (365 ** 0.5) * 100  # Annualized

        return volatility

    def get_crypto_recommendations(self, symbols: List[str] = None, quote: str = 'USDT', top_n: int = 5) -> Dict:
        """
        Kripto para önerileri

        Args:
            symbols: Analiz edilecek kripto semboller
            quote: Fiyat çifti
            top_n: En iyi kaç öneri

        Returns:
            Öneri listesi
        """
        if symbols is None:
            from DataSources.crypto_data import TOP_CRYPTOCURRENCIES
            symbols = TOP_CRYPTOCURRENCIES[:15]

        recommendations = []

        for symbol in symbols:
            try:
                prediction = self.predict_crypto(symbol, days_ahead=30, quote=quote, use_advanced=False)

                if prediction['success'] and 'signals' in prediction:
                    signal = prediction['signals'][0] if prediction['signals'] else None

                    if signal:
                        recommendations.append({
                            'symbol': symbol,
                            'quote': quote,
                            'signal': signal['type'],
                            'expected_return': signal.get('expected_return_1d', 0),
                            'strength': signal.get('strength', 0),
                            'confidence': signal.get('confidence', 'low'),
                            'current_price': prediction.get('current_price', 0),
                            'volatility': prediction.get('volatility', 0)
                        })
            except:
                continue

        # Sort by expected return
        recommendations.sort(key=lambda x: x['expected_return'], reverse=True)

        return {
            'success': True,
            'market': 'Crypto',
            'quote': quote,
            'recommendations': recommendations[:top_n],
            'total_analyzed': len(symbols),
            'timestamp': datetime.now().isoformat()
        }


def main():
    """
    Command-line interface

    Input JSON:
    {
        "market": "bist" | "crypto",
        "action": "predict" | "recommendations",
        "symbol": "AKBNK.IS" | "BTC",
        "quote": "USDT" | "TRY",
        "days_ahead": 30,
        "use_advanced": true,
        "symbols": ["AKBNK.IS", "GARAN.IS"],
        "top_n": 5
    }
    """
    try:
        input_data = json.loads(sys.stdin.read())

        market = input_data.get('market', 'bist').lower()
        action = input_data.get('action', 'predict')

        if market == 'bist':
            predictor = TurkishMarketPredictor()

            if action == 'predict':
                symbol = input_data.get('symbol', 'AKBNK.IS')
                days_ahead = input_data.get('days_ahead', 30)
                use_advanced = input_data.get('use_advanced', True)

                result = predictor.predict_bist_stock(symbol, days_ahead, use_advanced)

            elif action == 'recommendations':
                symbols = input_data.get('symbols', None)
                top_n = input_data.get('top_n', 5)

                result = predictor.get_bist_recommendations(symbols, top_n)

            else:
                result = {'success': False, 'error': f'Unknown action: {action}'}

        elif market == 'crypto':
            predictor = CryptoPredictor()

            if action == 'predict':
                symbol = input_data.get('symbol', 'BTC')
                quote = input_data.get('quote', 'USDT')
                days_ahead = input_data.get('days_ahead', 30)
                use_advanced = input_data.get('use_advanced', True)

                result = predictor.predict_crypto(symbol, days_ahead, quote, use_advanced)

            elif action == 'recommendations':
                symbols = input_data.get('symbols', None)
                quote = input_data.get('quote', 'USDT')
                top_n = input_data.get('top_n', 5)

                result = predictor.get_crypto_recommendations(symbols, quote, top_n)

            else:
                result = {'success': False, 'error': f'Unknown action: {action}'}

        else:
            result = {'success': False, 'error': f'Unknown market: {market}'}

        print(json.dumps(result))

    except Exception as e:
        import traceback
        print(json.dumps({
            'success': False,
            'error': str(e),
            'traceback': traceback.format_exc()
        }))


if __name__ == '__main__':
    main()
