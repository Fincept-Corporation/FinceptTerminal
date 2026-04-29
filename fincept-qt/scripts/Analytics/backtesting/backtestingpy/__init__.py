"""
Backtesting.py Provider
Ultra-lightweight and fast backtesting framework

The provider class is intentionally not re-exported here. The CLI entry point
runs as `python -m Analytics.backtesting.backtestingpy.backtestingpy_provider`,
so an eager `from .backtestingpy_provider import ...` would load the module
into sys.modules during package init and trigger a RuntimeWarning when Python
then re-executes it as __main__. Import directly from the submodule when
needed: `from Analytics.backtesting.backtestingpy.backtestingpy_provider
import BacktestingPyProvider`.
"""
