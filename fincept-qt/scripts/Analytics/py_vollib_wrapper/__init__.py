
__all__ = [
    'calculate_black_price', 'calculate_black_greeks', 'calculate_black_iv',
    'calculate_bs_price', 'calculate_bs_greeks', 'calculate_bs_iv',
    'calculate_bsm_price', 'calculate_bsm_greeks', 'calculate_bsm_iv'
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "calculate_black_price": ("black", "calculate_black_price"),
    "calculate_black_greeks": ("black", "calculate_black_greeks"),
    "calculate_black_iv": ("black", "calculate_black_iv"),
    "calculate_bs_price": ("black_scholes", "calculate_bs_price"),
    "calculate_bs_greeks": ("black_scholes", "calculate_bs_greeks"),
    "calculate_bs_iv": ("black_scholes", "calculate_bs_iv"),
    "calculate_bsm_price": ("black_scholes_merton", "calculate_bsm_price"),
    "calculate_bsm_greeks": ("black_scholes_merton", "calculate_bsm_greeks"),
    "calculate_bsm_iv": ("black_scholes_merton", "calculate_bsm_iv"),
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
