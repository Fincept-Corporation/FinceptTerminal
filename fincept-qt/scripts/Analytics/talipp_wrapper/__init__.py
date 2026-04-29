
__all__ = [
    'calculate_sma', 'calculate_ema', 'calculate_wma', 'calculate_dema', 'calculate_tema',
    'calculate_hma', 'calculate_kama', 'calculate_alma', 'calculate_t3', 'calculate_zlema',
    'calculate_rsi', 'calculate_macd', 'calculate_stoch', 'calculate_stoch_rsi',
    'calculate_cci', 'calculate_roc', 'calculate_tsi', 'calculate_williams',
    'calculate_atr', 'calculate_bb', 'calculate_keltner', 'calculate_donchian',
    'calculate_chandelier_stop', 'calculate_natr',
    'calculate_obv', 'calculate_vwap', 'calculate_vwma', 'calculate_mfi',
    'calculate_chaikin_osc', 'calculate_force_index',
    'calculate_adx', 'calculate_aroon', 'calculate_ichimoku', 'calculate_parabolic_sar',
    'calculate_supertrend',
    'calculate_ao', 'calculate_accu_dist', 'calculate_bop', 'calculate_chop',
    'calculate_coppock_curve', 'calculate_dpo', 'calculate_emv', 'calculate_fibonacci_retracement',
    'calculate_ibs', 'calculate_kst', 'calculate_kvo', 'calculate_mass_index',
    'calculate_mcginley_dynamic', 'calculate_mean_dev', 'calculate_pivots_hl',
    'calculate_rogers_satchell', 'calculate_sfx', 'calculate_smma', 'calculate_sobv',
    'calculate_stc', 'calculate_std_dev', 'calculate_trix', 'calculate_ttm',
    'calculate_uo', 'calculate_vtx', 'calculate_zigzag'
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "calculate_sma": ("trend", "calculate_sma"),
    "calculate_ema": ("trend", "calculate_ema"),
    "calculate_wma": ("trend", "calculate_wma"),
    "calculate_dema": ("trend", "calculate_dema"),
    "calculate_tema": ("trend", "calculate_tema"),
    "calculate_hma": ("trend", "calculate_hma"),
    "calculate_kama": ("trend", "calculate_kama"),
    "calculate_alma": ("trend", "calculate_alma"),
    "calculate_t3": ("trend", "calculate_t3"),
    "calculate_zlema": ("trend", "calculate_zlema"),
    "calculate_rsi": ("momentum", "calculate_rsi"),
    "calculate_macd": ("momentum", "calculate_macd"),
    "calculate_stoch": ("momentum", "calculate_stoch"),
    "calculate_stoch_rsi": ("momentum", "calculate_stoch_rsi"),
    "calculate_cci": ("momentum", "calculate_cci"),
    "calculate_roc": ("momentum", "calculate_roc"),
    "calculate_tsi": ("momentum", "calculate_tsi"),
    "calculate_williams": ("momentum", "calculate_williams"),
    "calculate_atr": ("volatility", "calculate_atr"),
    "calculate_bb": ("volatility", "calculate_bb"),
    "calculate_keltner": ("volatility", "calculate_keltner"),
    "calculate_donchian": ("volatility", "calculate_donchian"),
    "calculate_chandelier_stop": ("volatility", "calculate_chandelier_stop"),
    "calculate_natr": ("volatility", "calculate_natr"),
    "calculate_obv": ("volume", "calculate_obv"),
    "calculate_vwap": ("volume", "calculate_vwap"),
    "calculate_vwma": ("volume", "calculate_vwma"),
    "calculate_mfi": ("volume", "calculate_mfi"),
    "calculate_chaikin_osc": ("volume", "calculate_chaikin_osc"),
    "calculate_force_index": ("volume", "calculate_force_index"),
    "calculate_adx": ("trend_other", "calculate_adx"),
    "calculate_aroon": ("trend_other", "calculate_aroon"),
    "calculate_ichimoku": ("trend_other", "calculate_ichimoku"),
    "calculate_parabolic_sar": ("trend_other", "calculate_parabolic_sar"),
    "calculate_supertrend": ("trend_other", "calculate_supertrend"),
    "calculate_ao": ("specialized", "calculate_ao"),
    "calculate_accu_dist": ("specialized", "calculate_accu_dist"),
    "calculate_bop": ("specialized", "calculate_bop"),
    "calculate_chop": ("specialized", "calculate_chop"),
    "calculate_coppock_curve": ("specialized", "calculate_coppock_curve"),
    "calculate_dpo": ("specialized", "calculate_dpo"),
    "calculate_emv": ("specialized", "calculate_emv"),
    "calculate_fibonacci_retracement": ("specialized", "calculate_fibonacci_retracement"),
    "calculate_ibs": ("specialized", "calculate_ibs"),
    "calculate_kst": ("specialized", "calculate_kst"),
    "calculate_kvo": ("specialized", "calculate_kvo"),
    "calculate_mass_index": ("specialized", "calculate_mass_index"),
    "calculate_mcginley_dynamic": ("specialized", "calculate_mcginley_dynamic"),
    "calculate_mean_dev": ("specialized", "calculate_mean_dev"),
    "calculate_pivots_hl": ("specialized", "calculate_pivots_hl"),
    "calculate_rogers_satchell": ("specialized", "calculate_rogers_satchell"),
    "calculate_sfx": ("specialized", "calculate_sfx"),
    "calculate_smma": ("specialized", "calculate_smma"),
    "calculate_sobv": ("specialized", "calculate_sobv"),
    "calculate_stc": ("specialized", "calculate_stc"),
    "calculate_std_dev": ("specialized", "calculate_std_dev"),
    "calculate_trix": ("specialized", "calculate_trix"),
    "calculate_ttm": ("specialized", "calculate_ttm"),
    "calculate_uo": ("specialized", "calculate_uo"),
    "calculate_vtx": ("specialized", "calculate_vtx"),
    "calculate_zigzag": ("specialized", "calculate_zigzag"),
}


def __getattr__(name: str):  # PEP 562
    target = _LAZY_ATTRS.get(name)
    if target is None:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    submodule, original_name = target
    import importlib
    mod = importlib.import_module(f".{submodule}", __name__)
    value = getattr(mod, original_name)
    globals()[name] = value  # cache for subsequent access
    return value


def __dir__() -> list[str]:
    return sorted(set(globals()) | set(_LAZY_ATTRS))
