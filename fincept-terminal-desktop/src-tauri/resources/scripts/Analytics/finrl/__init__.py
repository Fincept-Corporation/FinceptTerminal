"""
FinRL Wrapper Module for Fincept Terminal
=========================================

Complete wrapper around the FinRL (Financial Reinforcement Learning) library.
Provides CLI-accessible Python scripts for all FinRL capabilities.

Modules:
--------
finrl_config       - Configuration, ticker universes, hyperparameters, date ranges
finrl_data         - Data downloading, preprocessing, feature engineering, splitting
finrl_environments - Trading environments (stock, crypto, portfolio, stop-loss, etc.)
finrl_agents       - DRL agent training (A2C, PPO, DDPG, SAC, TD3)
finrl_backtest     - Backtesting, performance metrics, algorithm comparison
finrl_portfolio    - Portfolio allocation & optimization with DRL
finrl_trading      - Paper trading (Alpaca), live trading, crypto trading
finrl_plot         - Visualization, equity curves, drawdowns, comparison charts
finrl_ensemble     - Ensemble strategies, validation-based selection, turbulence switching

Library Coverage:
-----------------
finrl.config              -> finrl_config.py
finrl.config_tickers      -> finrl_config.py
finrl.config_private      -> finrl_config.py
finrl.train               -> finrl_agents.py
finrl.test                -> finrl_backtest.py
finrl.trade               -> finrl_trading.py
finrl.plot                -> finrl_plot.py (custom, pyfolio-free)
finrl.main                -> finrl_config.py

finrl.meta.data_processor              -> finrl_data.py
finrl.meta.data_processors.*           -> finrl_data.py
finrl.meta.preprocessor.*              -> finrl_data.py

finrl.meta.env_stock_trading.*         -> finrl_environments.py
finrl.meta.env_cryptocurrency_trading.*-> finrl_environments.py
finrl.meta.env_portfolio_allocation.*  -> finrl_environments.py
finrl.meta.env_portfolio_optimization.*-> finrl_environments.py

finrl.agents.stablebaselines3.*        -> finrl_agents.py, finrl_ensemble.py
finrl.agents.portfolio_optimization.*  -> finrl_portfolio.py

finrl.applications.stock_trading.*     -> finrl_ensemble.py
"""

__version__ = "1.0.0"
__library__ = "finrl"
__library_version__ = "0.3.7"
