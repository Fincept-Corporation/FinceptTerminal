"""
TALIpp Worker Handler - Dispatches indicator calculations by category.
Usage: python worker_handler.py <operation> <json_data>
"""
import sys
import json

def main(args):
    if len(args) < 2:
        print(json.dumps({"error": "Usage: worker_handler.py <operation> <json_data>"}))
        return

    operation = args[0]
    data = json.loads(args[1])

    result = dispatch(operation, data)
    print(json.dumps(result))

def dispatch(operation, data):
    try:
        # ==================== TREND INDICATORS ====================
        if operation == "sma":
            from talipp_wrapper.trend import calculate_sma
            return calculate_sma(data["prices"], data.get("period", 20))
        elif operation == "ema":
            from talipp_wrapper.trend import calculate_ema
            return calculate_ema(data["prices"], data.get("period", 20))
        elif operation == "wma":
            from talipp_wrapper.trend import calculate_wma
            return calculate_wma(data["prices"], data.get("period", 20))
        elif operation == "dema":
            from talipp_wrapper.trend import calculate_dema
            return calculate_dema(data["prices"], data.get("period", 20))
        elif operation == "tema":
            from talipp_wrapper.trend import calculate_tema
            return calculate_tema(data["prices"], data.get("period", 20))
        elif operation == "hma":
            from talipp_wrapper.trend import calculate_hma
            return calculate_hma(data["prices"], data.get("period", 20))
        elif operation == "kama":
            from talipp_wrapper.trend import calculate_kama
            return calculate_kama(data["prices"], data.get("period", 10), data.get("fast_period", 2), data.get("slow_period", 30))
        elif operation == "alma":
            from talipp_wrapper.trend import calculate_alma
            return calculate_alma(data["prices"], data.get("period", 9), data.get("sigma", 6.0), data.get("offset", 0.85))
        elif operation == "t3":
            from talipp_wrapper.trend import calculate_t3
            return calculate_t3(data["prices"], data.get("period", 5), data.get("vfactor", 0.7))
        elif operation == "zlema":
            from talipp_wrapper.trend import calculate_zlema
            return calculate_zlema(data["prices"], data.get("period", 20))

        # ==================== TREND OTHER (ADX, Aroon, Ichimoku, etc.) ====================
        elif operation == "adx":
            from talipp_wrapper.trend_other import calculate_adx
            return calculate_adx(data["ohlcv"], data.get("di_period", 14), data.get("adx_period", 14))
        elif operation == "aroon":
            from talipp_wrapper.trend_other import calculate_aroon
            return calculate_aroon(data["ohlcv"], data.get("period", 25))
        elif operation == "ichimoku":
            from talipp_wrapper.trend_other import calculate_ichimoku
            return calculate_ichimoku(data["ohlcv"])
        elif operation == "parabolic_sar":
            from talipp_wrapper.trend_other import calculate_parabolic_sar
            return calculate_parabolic_sar(data["ohlcv"], data.get("af_start", 0.02), data.get("af_max", 0.2), data.get("af_increment", 0.02))
        elif operation == "supertrend":
            from talipp_wrapper.trend_other import calculate_supertrend
            return calculate_supertrend(data["ohlcv"], data.get("period", 10), data.get("multiplier", 3.0))

        # ==================== MOMENTUM INDICATORS ====================
        elif operation == "rsi":
            from talipp_wrapper.momentum import calculate_rsi
            return calculate_rsi(data["prices"], data.get("period", 14))
        elif operation == "macd":
            from talipp_wrapper.momentum import calculate_macd
            return calculate_macd(data["prices"], data.get("fast_period", 12), data.get("slow_period", 26), data.get("signal_period", 9))
        elif operation == "stoch":
            from talipp_wrapper.momentum import calculate_stoch
            return calculate_stoch(data["ohlcv"], data.get("period", 14), data.get("smoothing_period", 3))
        elif operation == "stoch_rsi":
            from talipp_wrapper.momentum import calculate_stoch_rsi
            return calculate_stoch_rsi(data["prices"], data.get("period", 14), data.get("stoch_period", 14), data.get("k_smoothing", 3), data.get("d_smoothing", 3))
        elif operation == "cci":
            from talipp_wrapper.momentum import calculate_cci
            return calculate_cci(data["ohlcv"], data.get("period", 20))
        elif operation == "roc":
            from talipp_wrapper.momentum import calculate_roc
            return calculate_roc(data["prices"], data.get("period", 12))
        elif operation == "tsi":
            from talipp_wrapper.momentum import calculate_tsi
            return calculate_tsi(data["prices"], data.get("long_period", 25), data.get("short_period", 13), data.get("signal_period", 13))
        elif operation == "williams":
            from talipp_wrapper.momentum import calculate_williams
            return calculate_williams(data["ohlcv"], data.get("period", 14))

        # ==================== VOLATILITY INDICATORS ====================
        elif operation == "atr":
            from talipp_wrapper.volatility import calculate_atr
            return calculate_atr(data["ohlcv"], data.get("period", 14))
        elif operation == "bb":
            from talipp_wrapper.volatility import calculate_bb
            return calculate_bb(data["prices"], data.get("period", 20), data.get("std_dev", 2.0))
        elif operation == "keltner":
            from talipp_wrapper.volatility import calculate_keltner
            return calculate_keltner(data["ohlcv"], data.get("period", 20), data.get("atr_period", 10), data.get("atr_multi", 2.0))
        elif operation == "donchian":
            from talipp_wrapper.volatility import calculate_donchian
            return calculate_donchian(data["ohlcv"], data.get("period", 20))
        elif operation == "chandelier_stop":
            from talipp_wrapper.volatility import calculate_chandelier_stop
            return calculate_chandelier_stop(data["ohlcv"], data.get("period", 22), data.get("atr_period", 22), data.get("atr_multi", 3.0))
        elif operation == "natr":
            from talipp_wrapper.volatility import calculate_natr
            return calculate_natr(data["ohlcv"], data.get("period", 14))

        # ==================== VOLUME INDICATORS ====================
        elif operation == "obv":
            from talipp_wrapper.volume import calculate_obv
            return calculate_obv(data["ohlcv"])
        elif operation == "vwap":
            from talipp_wrapper.volume import calculate_vwap
            return calculate_vwap(data["ohlcv"])
        elif operation == "vwma":
            from talipp_wrapper.volume import calculate_vwma
            return calculate_vwma(data["ohlcv"], data.get("period", 20))
        elif operation == "mfi":
            from talipp_wrapper.volume import calculate_mfi
            return calculate_mfi(data["ohlcv"], data.get("period", 14))
        elif operation == "chaikin_osc":
            from talipp_wrapper.volume import calculate_chaikin_osc
            return calculate_chaikin_osc(data["ohlcv"], data.get("fast_period", 3), data.get("slow_period", 10))
        elif operation == "force_index":
            from talipp_wrapper.volume import calculate_force_index
            return calculate_force_index(data["ohlcv"], data.get("period", 13))

        # ==================== SPECIALIZED INDICATORS ====================
        elif operation == "ao":
            from talipp_wrapper.specialized import calculate_ao
            return calculate_ao(data["ohlcv"], data.get("fast_period", 5), data.get("slow_period", 34))
        elif operation == "accu_dist":
            from talipp_wrapper.specialized import calculate_accu_dist
            return calculate_accu_dist(data["ohlcv"])
        elif operation == "bop":
            from talipp_wrapper.specialized import calculate_bop
            return calculate_bop(data["ohlcv"])
        elif operation == "chop":
            from talipp_wrapper.specialized import calculate_chop
            return calculate_chop(data["ohlcv"], data.get("period", 14))
        elif operation == "coppock_curve":
            from talipp_wrapper.specialized import calculate_coppock_curve
            return calculate_coppock_curve(data["prices"], data.get("roc1_period", 14), data.get("roc2_period", 11), data.get("wma_period", 10))
        elif operation == "dpo":
            from talipp_wrapper.specialized import calculate_dpo
            return calculate_dpo(data["prices"], data.get("period", 20))
        elif operation == "emv":
            from talipp_wrapper.specialized import calculate_emv
            return calculate_emv(data["ohlcv"], data.get("period", 14))
        elif operation == "fibonacci":
            from talipp_wrapper.specialized import calculate_fibonacci_retracement
            return calculate_fibonacci_retracement(data["ohlcv"])
        elif operation == "ibs":
            from talipp_wrapper.specialized import calculate_ibs
            return calculate_ibs(data["ohlcv"])
        elif operation == "kst":
            from talipp_wrapper.specialized import calculate_kst
            return calculate_kst(data["prices"])
        elif operation == "kvo":
            from talipp_wrapper.specialized import calculate_kvo
            return calculate_kvo(data["ohlcv"], data.get("fast_period", 34), data.get("slow_period", 55), data.get("signal_period", 13))
        elif operation == "mass_index":
            from talipp_wrapper.specialized import calculate_mass_index
            return calculate_mass_index(data["ohlcv"], data.get("period", 25), data.get("ema_period", 9))
        elif operation == "mcginley":
            from talipp_wrapper.specialized import calculate_mcginley_dynamic
            return calculate_mcginley_dynamic(data["prices"], data.get("period", 14), data.get("k", 0.6))
        elif operation == "mean_dev":
            from talipp_wrapper.specialized import calculate_mean_dev
            return calculate_mean_dev(data["prices"], data.get("period", 20))
        elif operation == "pivots_hl":
            from talipp_wrapper.specialized import calculate_pivots_hl
            return calculate_pivots_hl(data["ohlcv"], data.get("left_bars", 5), data.get("right_bars", 5))
        elif operation == "rogers_satchell":
            from talipp_wrapper.specialized import calculate_rogers_satchell
            return calculate_rogers_satchell(data["ohlcv"], data.get("period", 14))
        elif operation == "sfx":
            from talipp_wrapper.specialized import calculate_sfx
            return calculate_sfx(data["ohlcv"], data.get("period", 5), data.get("atr_period", 10), data.get("atr_mult", 4.0))
        elif operation == "smma":
            from talipp_wrapper.specialized import calculate_smma
            return calculate_smma(data["prices"], data.get("period", 20))
        elif operation == "sobv":
            from talipp_wrapper.specialized import calculate_sobv
            return calculate_sobv(data["ohlcv"], data.get("period", 10))
        elif operation == "stc":
            from talipp_wrapper.specialized import calculate_stc
            return calculate_stc(data["prices"], data.get("fast_period", 23), data.get("slow_period", 50), data.get("cycle", 10))
        elif operation == "std_dev":
            from talipp_wrapper.specialized import calculate_std_dev
            return calculate_std_dev(data["prices"], data.get("period", 20))
        elif operation == "trix":
            from talipp_wrapper.specialized import calculate_trix
            return calculate_trix(data["prices"], data.get("period", 15), data.get("signal_period", 9))
        elif operation == "ttm":
            from talipp_wrapper.specialized import calculate_ttm
            return calculate_ttm(data["ohlcv"], data.get("bb_period", 20), data.get("kc_period", 20))
        elif operation == "uo":
            from talipp_wrapper.specialized import calculate_uo
            return calculate_uo(data["ohlcv"], data.get("short_period", 7), data.get("medium_period", 14), data.get("long_period", 28))
        elif operation == "vtx":
            from talipp_wrapper.specialized import calculate_vtx
            return calculate_vtx(data["ohlcv"], data.get("period", 14))
        elif operation == "zigzag":
            from talipp_wrapper.specialized import calculate_zigzag
            return calculate_zigzag(data["ohlcv"], data.get("change", 5.0))

        else:
            return {"error": f"Unknown operation: {operation}"}

    except Exception as e:
        return {"error": str(e)}

if __name__ == "__main__":
    main(sys.argv[1:])
