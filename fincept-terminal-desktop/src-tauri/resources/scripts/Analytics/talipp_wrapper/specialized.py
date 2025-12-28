from typing import List, Dict
from talipp.indicators import (
    AO, AccuDist, BOP, CHOP, CoppockCurve, DPO, EMV, FibonacciRetracement,
    IBS, KST, KVO, MassIndex, McGinleyDynamic, MeanDev, PivotsHL,
    RogersSatchell, SFX, SMMA, SOBV, STC, StdDev, TRIX, TTM, UO, VTX, ZigZag
)
from talipp.ohlcv import OHLCV

def calculate_ao(ohlcv_data: List[Dict], fast_period: int = 5, slow_period: int = 34) -> Dict:
    ao = AO(fast_period, slow_period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        ao.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(ao)],
        'last': float(ao[-1]) if len(ao) > 0 and ao[-1] is not None else None
    }

def calculate_accu_dist(ohlcv_data: List[Dict]) -> Dict:
    ad = AccuDist()
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        ad.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(ad)],
        'last': float(ad[-1]) if len(ad) > 0 and ad[-1] is not None else None
    }

def calculate_bop(ohlcv_data: List[Dict]) -> Dict:
    bop = BOP()
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        bop.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(bop)],
        'last': float(bop[-1]) if len(bop) > 0 and bop[-1] is not None else None
    }

def calculate_chop(ohlcv_data: List[Dict], period: int = 14) -> Dict:
    chop = CHOP(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        chop.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(chop)],
        'period': period,
        'last': float(chop[-1]) if len(chop) > 0 and chop[-1] is not None else None
    }

def calculate_coppock_curve(prices: List[float], roc1_period: int = 14, roc2_period: int = 11, wma_period: int = 10) -> Dict:
    cc = CoppockCurve(roc1_period, roc2_period, wma_period)
    for price in prices:
        cc.add(price)
    return {
        'values': [float(v) if v is not None else None for v in list(cc)],
        'last': float(cc[-1]) if len(cc) > 0 and cc[-1] is not None else None
    }

def calculate_dpo(prices: List[float], period: int = 20) -> Dict:
    dpo = DPO(period)
    for price in prices:
        dpo.add(price)
    return {
        'values': [float(v) if v is not None else None for v in list(dpo)],
        'period': period,
        'last': float(dpo[-1]) if len(dpo) > 0 and dpo[-1] is not None else None
    }

def calculate_emv(ohlcv_data: List[Dict], period: int = 14) -> Dict:
    emv = EMV(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        emv.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(emv)],
        'period': period,
        'last': float(emv[-1]) if len(emv) > 0 and emv[-1] is not None else None
    }

def calculate_fibonacci_retracement(ohlcv_data: List[Dict]) -> Dict:
    fib = FibonacciRetracement()
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        fib.add(ohlcv)

    values = []
    for val in fib:
        if val is not None:
            values.append({
                'level_0': float(val.level_0),
                'level_236': float(val.level_236),
                'level_382': float(val.level_382),
                'level_500': float(val.level_500),
                'level_618': float(val.level_618),
                'level_1000': float(val.level_1000)
            })
        else:
            values.append(None)

    last_val = fib[-1] if len(fib) > 0 and fib[-1] is not None else None
    return {
        'values': values,
        'last': {
            'level_0': float(last_val.level_0),
            'level_236': float(last_val.level_236),
            'level_382': float(last_val.level_382),
            'level_500': float(last_val.level_500),
            'level_618': float(last_val.level_618),
            'level_1000': float(last_val.level_1000)
        } if last_val else None
    }

def calculate_ibs(ohlcv_data: List[Dict]) -> Dict:
    ibs = IBS()
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        ibs.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(ibs)],
        'last': float(ibs[-1]) if len(ibs) > 0 and ibs[-1] is not None else None
    }

def calculate_kst(prices: List[float]) -> Dict:
    kst = KST()
    for price in prices:
        kst.add(price)

    values = []
    signals = []
    for val in kst:
        if val is not None:
            values.append(float(val.value))
            signals.append(float(val.signal))
        else:
            values.append(None)
            signals.append(None)

    last_val = kst[-1] if len(kst) > 0 and kst[-1] is not None else None
    return {
        'kst': values,
        'signal': signals,
        'last_kst': float(last_val.value) if last_val else None,
        'last_signal': float(last_val.signal) if last_val else None
    }

def calculate_kvo(ohlcv_data: List[Dict], fast_period: int = 34, slow_period: int = 55, signal_period: int = 13) -> Dict:
    kvo = KVO(fast_period, slow_period, signal_period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        kvo.add(ohlcv)

    values = []
    signals = []
    for val in kvo:
        if val is not None:
            values.append(float(val.value))
            signals.append(float(val.signal))
        else:
            values.append(None)
            signals.append(None)

    last_val = kvo[-1] if len(kvo) > 0 and kvo[-1] is not None else None
    return {
        'kvo': values,
        'signal': signals,
        'last_kvo': float(last_val.value) if last_val else None,
        'last_signal': float(last_val.signal) if last_val else None
    }

def calculate_mass_index(ohlcv_data: List[Dict], period: int = 25, ema_period: int = 9) -> Dict:
    mi = MassIndex(period, ema_period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        mi.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(mi)],
        'period': period,
        'last': float(mi[-1]) if len(mi) > 0 and mi[-1] is not None else None
    }

def calculate_mcginley_dynamic(prices: List[float], period: int = 14, k: float = 0.6) -> Dict:
    md = McGinleyDynamic(period, k)
    for price in prices:
        md.add(price)
    return {
        'values': [float(v) if v is not None else None for v in list(md)],
        'period': period,
        'last': float(md[-1]) if len(md) > 0 and md[-1] is not None else None
    }

def calculate_mean_dev(prices: List[float], period: int = 20) -> Dict:
    md = MeanDev(period)
    for price in prices:
        md.add(price)
    return {
        'values': [float(v) if v is not None else None for v in list(md)],
        'period': period,
        'last': float(md[-1]) if len(md) > 0 and md[-1] is not None else None
    }

def calculate_pivots_hl(ohlcv_data: List[Dict], left_bars: int = 5, right_bars: int = 5) -> Dict:
    pivots = PivotsHL(left_bars, right_bars)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        pivots.add(ohlcv)

    values = []
    for val in pivots:
        if val is not None:
            values.append({
                'high': float(val.high) if val.high is not None else None,
                'low': float(val.low) if val.low is not None else None
            })
        else:
            values.append(None)

    last_val = pivots[-1] if len(pivots) > 0 and pivots[-1] is not None else None
    return {
        'values': values,
        'last': {
            'high': float(last_val.high) if last_val and last_val.high is not None else None,
            'low': float(last_val.low) if last_val and last_val.low is not None else None
        } if last_val else None
    }

def calculate_rogers_satchell(ohlcv_data: List[Dict], period: int = 14) -> Dict:
    rs = RogersSatchell(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        rs.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(rs)],
        'period': period,
        'last': float(rs[-1]) if len(rs) > 0 and rs[-1] is not None else None
    }

def calculate_sfx(ohlcv_data: List[Dict], period: int = 5, atr_period: int = 10, atr_mult: float = 4.0) -> Dict:
    sfx = SFX(period, atr_period, atr_mult)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        sfx.add(ohlcv)

    values = []
    for val in sfx:
        if val is not None:
            values.append({'value': float(val.value), 'trend': int(val.trend)})
        else:
            values.append(None)

    last_val = sfx[-1] if len(sfx) > 0 and sfx[-1] is not None else None
    return {
        'values': values,
        'last': {'value': float(last_val.value), 'trend': int(last_val.trend)} if last_val else None
    }

def calculate_smma(prices: List[float], period: int = 20) -> Dict:
    smma = SMMA(period)
    for price in prices:
        smma.add(price)
    return {
        'values': [float(v) if v is not None else None for v in list(smma)],
        'period': period,
        'last': float(smma[-1]) if len(smma) > 0 and smma[-1] is not None else None
    }

def calculate_sobv(ohlcv_data: List[Dict], period: int = 10) -> Dict:
    sobv = SOBV(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        sobv.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(sobv)],
        'period': period,
        'last': float(sobv[-1]) if len(sobv) > 0 and sobv[-1] is not None else None
    }

def calculate_stc(prices: List[float], fast_period: int = 23, slow_period: int = 50, cycle: int = 10) -> Dict:
    stc = STC(fast_period, slow_period, cycle)
    for price in prices:
        stc.add(price)
    return {
        'values': [float(v) if v is not None else None for v in list(stc)],
        'last': float(stc[-1]) if len(stc) > 0 and stc[-1] is not None else None
    }

def calculate_std_dev(prices: List[float], period: int = 20) -> Dict:
    std = StdDev(period)
    for price in prices:
        std.add(price)
    return {
        'values': [float(v) if v is not None else None for v in list(std)],
        'period': period,
        'last': float(std[-1]) if len(std) > 0 and std[-1] is not None else None
    }

def calculate_trix(prices: List[float], period: int = 15, signal_period: int = 9) -> Dict:
    trix = TRIX(period, signal_period)
    for price in prices:
        trix.add(price)

    values = []
    signals = []
    for val in trix:
        if val is not None:
            values.append(float(val.value))
            signals.append(float(val.signal))
        else:
            values.append(None)
            signals.append(None)

    last_val = trix[-1] if len(trix) > 0 and trix[-1] is not None else None
    return {
        'trix': values,
        'signal': signals,
        'last_trix': float(last_val.value) if last_val else None,
        'last_signal': float(last_val.signal) if last_val else None
    }

def calculate_ttm(ohlcv_data: List[Dict], bb_period: int = 20, kc_period: int = 20) -> Dict:
    ttm = TTM(bb_period, kc_period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        ttm.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(ttm)],
        'last': float(ttm[-1]) if len(ttm) > 0 and ttm[-1] is not None else None
    }

def calculate_uo(ohlcv_data: List[Dict], short_period: int = 7, medium_period: int = 14, long_period: int = 28) -> Dict:
    uo = UO(short_period, medium_period, long_period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        uo.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(uo)],
        'last': float(uo[-1]) if len(uo) > 0 and uo[-1] is not None else None
    }

def calculate_vtx(ohlcv_data: List[Dict], period: int = 14) -> Dict:
    vtx = VTX(period)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        vtx.add(ohlcv)

    plus = []
    minus = []
    for val in vtx:
        if val is not None:
            plus.append(float(val.plus))
            minus.append(float(val.minus))
        else:
            plus.append(None)
            minus.append(None)

    last_val = vtx[-1] if len(vtx) > 0 and vtx[-1] is not None else None
    return {
        'plus': plus,
        'minus': minus,
        'last_plus': float(last_val.plus) if last_val else None,
        'last_minus': float(last_val.minus) if last_val else None
    }

def calculate_zigzag(ohlcv_data: List[Dict], change: float = 5.0) -> Dict:
    zz = ZigZag(change)
    for bar in ohlcv_data:
        ohlcv = OHLCV(bar['open'], bar['high'], bar['low'], bar['close'], bar.get('volume', 0))
        zz.add(ohlcv)
    return {
        'values': [float(v) if v is not None else None for v in list(zz)],
        'change': change,
        'last': float(zz[-1]) if len(zz) > 0 and zz[-1] is not None else None
    }

def main():
    print("Testing TALIpp Specialized Indicators")

    prices = [100, 102, 101, 105, 107, 106, 108, 110, 109, 111, 113, 112, 115, 114, 116, 118, 117, 120, 119, 121] * 3
    ohlcv_data = [
        {'open': p-1, 'high': p+2, 'low': p-2, 'close': p, 'volume': 1000+i*10}
        for i, p in enumerate(prices)
    ]

    print("\n1. Testing SMMA...")
    result = calculate_smma(prices, period=10)
    print("Last SMMA:", result['last'])
    assert result['last'] is not None
    print("Test 1: PASSED")

    print("\n2. Testing StdDev...")
    result = calculate_std_dev(prices, period=10)
    print("Last StdDev:", result['last'])
    assert result['last'] is not None
    print("Test 2: PASSED")

    print("\n3. Testing BOP...")
    result = calculate_bop(ohlcv_data)
    print("Last BOP:", result['last'])
    assert result['last'] is not None
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
