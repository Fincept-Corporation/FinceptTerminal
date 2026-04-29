"""Renaissance Technologies utility functions."""

from .data_fetcher import (
    StockData,
    fetch_stock_data_yfinance,
    fetch_market_benchmark,
    fetch_peer_prices,
    validate_data,
    prepare_analysis_inputs,
)

from .model_factory import (
    ModelFactory,
    ModelConfig,
    get_model_factory,
    create_model_from_config,
    get_default_model_settings,
)

from .embedder_factory import (
    EmbedderFactory,
    EmbedderConfig,
    get_embedder_factory,
    create_embedder_from_config,
)


__all__ = [
    "StockData",
    "fetch_stock_data_yfinance",
    "fetch_market_benchmark",
    "fetch_peer_prices",
    "validate_data",
    "prepare_analysis_inputs",
    "ModelFactory",
    "ModelConfig",
    "get_model_factory",
    "create_model_from_config",
    "get_default_model_settings",
    "EmbedderFactory",
    "EmbedderConfig",
    "get_embedder_factory",
    "create_embedder_from_config",
    "get_app_data_dir",
    "get_rentech_data_dir",
    "get_knowledge_base_path",
    "get_memory_db_path",
    "get_logs_dir",
    "get_cache_dir",
    "get_frontend_db_path",
    "get_knowledge_path",
    "get_memory_path",
    "get_log_file_path",
    "get_cache_file_path",
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "get_app_data_dir": ("paths", "get_app_data_dir"),
    "get_rentech_data_dir": ("paths", "get_rentech_data_dir"),
    "get_knowledge_base_path": ("paths", "get_knowledge_base_path"),
    "get_memory_db_path": ("paths", "get_memory_db_path"),
    "get_logs_dir": ("paths", "get_logs_dir"),
    "get_cache_dir": ("paths", "get_cache_dir"),
    "get_frontend_db_path": ("paths", "get_frontend_db_path"),
    "get_knowledge_path": ("paths", "get_knowledge_path"),
    "get_memory_path": ("paths", "get_memory_path"),
    "get_log_file_path": ("paths", "get_log_file_path"),
    "get_cache_file_path": ("paths", "get_cache_file_path"),
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
