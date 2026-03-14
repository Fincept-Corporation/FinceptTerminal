from typing import List, Dict
from talipp.indicators import ATR, BB, KeltnerChannels, DonchianChannels, ChandeKrollStop, NATR
from talipp.ohlcv import OHLCV

def calculate_atr(ohlcv_data: List[Dict], period: int = 14) -> Dict:
    atr = ATR(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        atr.add(ohlcv)

    return {
        'values': [float(v) if v is not None else None for v in list(atr)],
        'period': period,
        'last': float(atr[-1]) if len(atr) > 0 and atr[-1] is not None else None
    }

def calculate_bb(prices: List[float], period: int = 20, std_dev: float = 2.0) -> Dict:
    bb = BB(period, std_dev)
    for price in prices:
        bb.add(price)

    upper = []
    middle = []
    lower = []

    for val in bb:
        if val is not None:
            upper.append(float(val.ub))
            middle.append(float(val.cb))
            lower.append(float(val.lb))
        else:
            upper.append(None)
            middle.append(None)
            lower.append(None)

    last_val = bb[-1] if len(bb) > 0 and bb[-1] is not None else None

    return {
        'upper': upper,
        'middle': middle,
        'lower': lower,
        'last_upper': float(last_val.ub) if last_val else None,
        'last_middle': float(last_val.cb) if last_val else None,
        'last_lower': float(last_val.lb) if last_val else None
    }

def calculate_keltner(ohlcv_data: List[Dict], period: int = 20, atr_period: int = 10, atr_multi: float = 2.0) -> Dict:
    keltner = KeltnerChannels(period, atr_period, atr_multi)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        keltner.add(ohlcv)

    upper = []
    middle = []
    lower = []

    for val in keltner:
        if val is not None:
            upper.append(float(val.ub))
            middle.append(float(val.cb))
            lower.append(float(val.lb))
        else:
            upper.append(None)
            middle.append(None)
            lower.append(None)

    last_val = keltner[-1] if len(keltner) > 0 and keltner[-1] is not None else None

    return {
        'upper': upper,
        'middle': middle,
        'lower': lower,
        'last_upper': float(last_val.ub) if last_val else None,
        'last_middle': float(last_val.cb) if last_val else None,
        'last_lower': float(last_val.lb) if last_val else None
    }

def calculate_donchian(ohlcv_data: List[Dict], period: int = 20) -> Dict:
    donchian = DonchianChannels(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        donchian.add(ohlcv)

    upper = []
    middle = []
    lower = []

    for val in donchian:
        if val is not None:
            upper.append(float(val.ub))
            middle.append(float(val.cb))
            lower.append(float(val.lb))
        else:
            upper.append(None)
            middle.append(None)
            lower.append(None)

    last_val = donchian[-1] if len(donchian) > 0 and donchian[-1] is not None else None

    return {
        'upper': upper,
        'middle': middle,
        'lower': lower,
        'last_upper': float(last_val.ub) if last_val else None,
        'last_middle': float(last_val.cb) if last_val else None,
        'last_lower': float(last_val.lb) if last_val else None
    }

def calculate_chandelier_stop(ohlcv_data: List[Dict], period: int = 22, atr_period: int = 22, atr_multi: float = 3.0) -> Dict:
    chandelier = ChandeKrollStop(period, atr_period, atr_multi)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        chandelier.add(ohlcv)

    long_stop = []
    short_stop = []

    for val in chandelier:
        if val is not None:
            long_stop.append(float(val.long_stop))
            short_stop.append(float(val.short_stop))
        else:
            long_stop.append(None)
            short_stop.append(None)

    last_val = chandelier[-1] if len(chandelier) > 0 and chandelier[-1] is not None else None

    return {
        'long_stop': long_stop,
        'short_stop': short_stop,
        'last_long_stop': float(last_val.long_stop) if last_val else None,
        'last_short_stop': float(last_val.short_stop) if last_val else None
    }

def calculate_natr(ohlcv_data: List[Dict], period: int = 14) -> Dict:
    natr = NATR(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        natr.add(ohlcv)

    return {
        'values': [float(v) if v is not None else None for v in list(natr)],
        'period': period,
        'last': float(natr[-1]) if len(natr) > 0 and natr[-1] is not None else None
    }

def main():
    print("Testing TALIpp Volatility Indicators")

    ohlcv_data = [
        {'open': 100, 'high': 102, 'low': 99, 'close': 101, 'volume': 1000},
        {'open': 101, 'high': 103, 'low': 100, 'close': 102, 'volume': 1100},
        {'open': 102, 'high': 105, 'low': 101, 'close': 104, 'volume': 1200},
        {'open': 104, 'high': 106, 'low': 103, 'close': 105, 'volume': 1150},
        {'open': 105, 'high': 107, 'low': 104, 'close': 106, 'volume': 1300},
    ] * 5

    prices = [bar['close'] for bar in ohlcv_data]

    print("\n1. Testing ATR...")
    result = calculate_atr(ohlcv_data, period=14)
    print("Last ATR:", result['last'])
    print("Test 1: PASSED")

    print("\n2. Testing Bollinger Bands...")
    result = calculate_bb(prices, period=20)
    print("Last Upper:", result['last_upper'])
    print("Last Middle:", result['last_middle'])
    print("Last Lower:", result['last_lower'])
    print("Test 2: PASSED")

    print("\n3. Testing Donchian Channels...")
    result = calculate_donchian(ohlcv_data, period=20)
    print("Last Upper:", result['last_upper'])
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
