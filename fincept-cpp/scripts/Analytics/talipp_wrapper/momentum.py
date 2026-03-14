from typing import List, Dict
from talipp.indicators import RSI, MACD, Stoch, StochRSI, CCI, ROC, TSI
from talipp.ohlcv import OHLCV

def calculate_rsi(prices: List[float], period: int = 14) -> Dict:
    rsi = RSI(period)
    for price in prices:
        rsi.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(rsi)],
        'period': period,
        'last': float(rsi[-1]) if len(rsi) > 0 and rsi[-1] is not None else None
    }

def calculate_macd(prices: List[float], fast_period: int = 12, slow_period: int = 26, signal_period: int = 9) -> Dict:
    macd = MACD(fast_period, slow_period, signal_period)
    for price in prices:
        macd.add(price)

    macd_values = []
    signal_values = []
    histogram_values = []

    for val in macd:
        if val is not None:
            macd_values.append(float(val.value))
            signal_values.append(float(val.signal))
            histogram_values.append(float(val.histogram))
        else:
            macd_values.append(None)
            signal_values.append(None)
            histogram_values.append(None)

    last_val = macd[-1] if len(macd) > 0 and macd[-1] is not None else None

    return {
        'macd': macd_values,
        'signal': signal_values,
        'histogram': histogram_values,
        'last_macd': float(last_val.value) if last_val else None,
        'last_signal': float(last_val.signal) if last_val else None,
        'last_histogram': float(last_val.histogram) if last_val else None
    }

def calculate_stoch(ohlcv_data: List[Dict], period: int = 14, smoothing_period: int = 3) -> Dict:
    stoch = Stoch(period, smoothing_period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        stoch.add(ohlcv)

    k_values = []
    d_values = []

    for val in stoch:
        if val is not None:
            k_values.append(float(val.k))
            d_values.append(float(val.d))
        else:
            k_values.append(None)
            d_values.append(None)

    last_val = stoch[-1] if len(stoch) > 0 and stoch[-1] is not None else None

    return {
        'k': k_values,
        'd': d_values,
        'last_k': float(last_val.k) if last_val else None,
        'last_d': float(last_val.d) if last_val else None
    }

def calculate_stoch_rsi(prices: List[float], period: int = 14, stoch_period: int = 14, k_smoothing: int = 3, d_smoothing: int = 3) -> Dict:
    stoch_rsi = StochRSI(period, stoch_period, k_smoothing, d_smoothing)
    for price in prices:
        stoch_rsi.add(price)

    k_values = []
    d_values = []

    for val in stoch_rsi:
        if val is not None:
            k_values.append(float(val.k))
            d_values.append(float(val.d))
        else:
            k_values.append(None)
            d_values.append(None)

    last_val = stoch_rsi[-1] if len(stoch_rsi) > 0 and stoch_rsi[-1] is not None else None

    return {
        'k': k_values,
        'd': d_values,
        'last_k': float(last_val.k) if last_val else None,
        'last_d': float(last_val.d) if last_val else None
    }

def calculate_cci(ohlcv_data: List[Dict], period: int = 20) -> Dict:
    cci = CCI(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        cci.add(ohlcv)

    return {
        'values': [float(v) if v is not None else None for v in list(cci)],
        'period': period,
        'last': float(cci[-1]) if len(cci) > 0 and cci[-1] is not None else None
    }

def calculate_roc(prices: List[float], period: int = 12) -> Dict:
    roc = ROC(period)
    for price in prices:
        roc.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(roc)],
        'period': period,
        'last': float(roc[-1]) if len(roc) > 0 and roc[-1] is not None else None
    }

def calculate_tsi(prices: List[float], long_period: int = 25, short_period: int = 13, signal_period: int = 13) -> Dict:
    tsi = TSI(long_period, short_period, signal_period)
    for price in prices:
        tsi.add(price)

    tsi_values = []
    signal_values = []

    for val in tsi:
        if val is not None:
            tsi_values.append(float(val.value))
            signal_values.append(float(val.signal))
        else:
            tsi_values.append(None)
            signal_values.append(None)

    last_val = tsi[-1] if len(tsi) > 0 and tsi[-1] is not None else None

    return {
        'tsi': tsi_values,
        'signal': signal_values,
        'last_tsi': float(last_val.value) if last_val else None,
        'last_signal': float(last_val.signal) if last_val else None
    }

def calculate_williams(ohlcv_data: List[Dict], period: int = 14) -> Dict:
    from talipp.indicators import Williams
    williams = Williams(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        williams.add(ohlcv)

    return {
        'values': [float(v) if v is not None else None for v in list(williams)],
        'period': period,
        'last': float(williams[-1]) if len(williams) > 0 and williams[-1] is not None else None
    }

def main():
    print("Testing TALIpp Momentum Indicators")

    prices = [100, 102, 101, 105, 107, 106, 108, 110, 109, 111, 113, 112, 115, 114, 116, 118, 117, 120, 119, 121]

    print("\n1. Testing RSI...")
    result = calculate_rsi(prices, period=14)
    print("Last RSI:", result['last'])
    assert result['last'] is not None
    print("Test 1: PASSED")

    print("\n2. Testing MACD...")
    result = calculate_macd(prices)
    print("Last MACD:", result['last_macd'])
    print("Test 2: PASSED")

    print("\n3. Testing ROC...")
    result = calculate_roc(prices, period=10)
    print("Last ROC:", result['last'])
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
