
__all__ = [
    'create_main_engine',
    'send_order',
    'cancel_order',
    'subscribe_market_data',
    'query_history',
    'get_all_contracts',
    'get_all_positions',
    'get_all_orders',
    'get_all_trades',
    'create_tick_data',
    'create_bar_data',
    'create_order_request',
    'create_cancel_request',
    'tick_to_dict',
    'bar_to_dict',
    'order_to_dict',
    'trade_to_dict',
    'position_to_dict',
    'account_to_dict',
    'convert_direction',
    'convert_order_type',
    'convert_offset',
    'convert_exchange',
    'get_trading_date',
    'calculate_pnl',
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "create_main_engine": ("engine", "create_main_engine"),
    "send_order": ("engine", "send_order"),
    "cancel_order": ("engine", "cancel_order"),
    "subscribe_market_data": ("engine", "subscribe_market_data"),
    "query_history": ("engine", "query_history"),
    "get_all_contracts": ("engine", "get_all_contracts"),
    "get_all_positions": ("engine", "get_all_positions"),
    "get_all_orders": ("engine", "get_all_orders"),
    "get_all_trades": ("engine", "get_all_trades"),
    "create_tick_data": ("data", "create_tick_data"),
    "create_bar_data": ("data", "create_bar_data"),
    "create_order_request": ("data", "create_order_request"),
    "create_cancel_request": ("data", "create_cancel_request"),
    "tick_to_dict": ("data", "tick_to_dict"),
    "bar_to_dict": ("data", "bar_to_dict"),
    "order_to_dict": ("data", "order_to_dict"),
    "trade_to_dict": ("data", "trade_to_dict"),
    "position_to_dict": ("data", "position_to_dict"),
    "account_to_dict": ("data", "account_to_dict"),
    "convert_direction": ("utility", "convert_direction"),
    "convert_order_type": ("utility", "convert_order_type"),
    "convert_offset": ("utility", "convert_offset"),
    "convert_exchange": ("utility", "convert_exchange"),
    "get_trading_date": ("utility", "get_trading_date"),
    "calculate_pnl": ("utility", "calculate_pnl"),
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
