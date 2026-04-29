"""
pypme Wrapper for Fincept Terminal

Complete wrapper for pypme library covering:
- PME (Public Market Equivalent) calculations
- xPME (Extended PME) with time-weighted adjustments
- Tessa integration for automatic market data fetching
- Verbose outputs with detailed calculations
"""


__all__ = [
    'calculate_pme',
    'calculate_verbose_pme',
    'calculate_xpme',
    'calculate_verbose_xpme',
    'calculate_tessa_xpme',
    'calculate_tessa_verbose_xpme',
    'pick_prices_from_dataframe'
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "calculate_pme": ("core", "calculate_pme"),
    "calculate_verbose_pme": ("core", "calculate_verbose_pme"),
    "calculate_xpme": ("core", "calculate_xpme"),
    "calculate_verbose_xpme": ("core", "calculate_verbose_xpme"),
    "calculate_tessa_xpme": ("core", "calculate_tessa_xpme"),
    "calculate_tessa_verbose_xpme": ("core", "calculate_tessa_verbose_xpme"),
    "pick_prices_from_dataframe": ("core", "pick_prices_from_dataframe"),
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
