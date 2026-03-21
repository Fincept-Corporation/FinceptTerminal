from typing import List, Dict, Union
from talipp.indicators import SMA, EMA, WMA, DEMA, TEMA, HMA, KAMA, ALMA, T3, ZLEMA

def calculate_sma(prices: List[float], period: int = 20) -> Dict:
    sma = SMA(period)
    for price in prices:
        sma.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(sma)],
        'period': period,
        'last': float(sma[-1]) if len(sma) > 0 and sma[-1] is not None else None
    }

def calculate_ema(prices: List[float], period: int = 20) -> Dict:
    ema = EMA(period)
    for price in prices:
        ema.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(ema)],
        'period': period,
        'last': float(ema[-1]) if len(ema) > 0 and ema[-1] is not None else None
    }

def calculate_wma(prices: List[float], period: int = 20) -> Dict:
    wma = WMA(period)
    for price in prices:
        wma.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(wma)],
        'period': period,
        'last': float(wma[-1]) if len(wma) > 0 and wma[-1] is not None else None
    }

def calculate_dema(prices: List[float], period: int = 20) -> Dict:
    dema = DEMA(period)
    for price in prices:
        dema.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(dema)],
        'period': period,
        'last': float(dema[-1]) if len(dema) > 0 and dema[-1] is not None else None
    }

def calculate_tema(prices: List[float], period: int = 20) -> Dict:
    tema = TEMA(period)
    for price in prices:
        tema.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(tema)],
        'period': period,
        'last': float(tema[-1]) if len(tema) > 0 and tema[-1] is not None else None
    }

def calculate_hma(prices: List[float], period: int = 20) -> Dict:
    hma = HMA(period)
    for price in prices:
        hma.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(hma)],
        'period': period,
        'last': float(hma[-1]) if len(hma) > 0 and hma[-1] is not None else None
    }

def calculate_kama(prices: List[float], period: int = 10, fast_period: int = 2, slow_period: int = 30) -> Dict:
    kama = KAMA(period, fast_period, slow_period)
    for price in prices:
        kama.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(kama)],
        'period': period,
        'last': float(kama[-1]) if len(kama) > 0 and kama[-1] is not None else None
    }

def calculate_alma(prices: List[float], period: int = 9, sigma: float = 6.0, offset: float = 0.85) -> Dict:
    alma = ALMA(period, sigma, offset)
    for price in prices:
        alma.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(alma)],
        'period': period,
        'last': float(alma[-1]) if len(alma) > 0 and alma[-1] is not None else None
    }

def calculate_t3(prices: List[float], period: int = 5, vfactor: float = 0.7) -> Dict:
    t3 = T3(period, vfactor)
    for price in prices:
        t3.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(t3)],
        'period': period,
        'last': float(t3[-1]) if len(t3) > 0 and t3[-1] is not None else None
    }

def calculate_zlema(prices: List[float], period: int = 20) -> Dict:
    zlema = ZLEMA(period)
    for price in prices:
        zlema.add(price)

    return {
        'values': [float(v) if v is not None else None for v in list(zlema)],
        'period': period,
        'last': float(zlema[-1]) if len(zlema) > 0 and zlema[-1] is not None else None
    }

def main():
    print("Testing TALIpp Trend Indicators")

    prices = [100, 102, 101, 105, 107, 106, 108, 110, 109, 111, 113, 112, 115, 114, 116]

    print("\n1. Testing SMA...")
    result = calculate_sma(prices, period=5)
    print("SMA values:", len(result['values']))
    print("Last SMA:", result['last'])
    assert result['last'] is not None
    print("Test 1: PASSED")

    print("\n2. Testing EMA...")
    result = calculate_ema(prices, period=5)
    print("Last EMA:", result['last'])
    assert result['last'] is not None
    print("Test 2: PASSED")

    print("\n3. Testing DEMA...")
    result = calculate_dema(prices, period=5)
    print("Last DEMA:", result['last'])
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
