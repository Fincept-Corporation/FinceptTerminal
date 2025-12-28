# AI Quant Lab

> Quantitative research lab powered by Qlib and RDAgent for automated strategy development

## Overview

AI Quant Lab combines Microsoft's Qlib quantitative investment platform with RDAgent (Research & Development Agent) for automated hypothesis generation, backtesting, and strategy optimization.

**Key Technologies**: Qlib (quant framework), RDAgent (AI research agent), automated strategy evolution

## System Components

| Component | Modules | Purpose |
|-----------|---------|---------|
| **Qlib Integration** | 8 modules | Backtesting, portfolio optimization, ML models |
| **RDAgent System** | 4 modules | Hypothesis generation, knowledge base, research automation |

## Qlib Modules

| Module | File | Purpose |
|--------|------|---------|
| **Advanced Backtest** | `qlib_advanced_backtest.py` | Multi-factor backtesting engine |
| **Data Processors** | `qlib_data_processors.py` | Data cleaning, normalization, feature extraction |
| **Evaluation** | `qlib_evaluation.py` | Strategy performance evaluation, metrics |
| **Feature Engineering** | `qlib_feature_engineering.py` | Alpha factor creation, feature selection |
| **Portfolio Optimization** | `qlib_portfolio_opt.py` | Portfolio construction, risk management |
| **Reporting** | `qlib_reporting.py` | Performance reports, visualizations |
| **Strategy** | `qlib_strategy.py` | Trading strategy implementation |
| **Service** | `qlib_service.py` | Main service interface for Tauri integration |

## RDAgent System (`/rdagent/`)

| Module | File | Purpose |
|--------|------|---------|
| **Hypothesis Generation** | `core/hypothesis_gen.py` | AI-generated trading hypotheses |
| **Knowledge Base** | `core/knowledge_base.py` | Research knowledge storage, retrieval |
| **Proposal System** | `core/proposal_system.py` | Strategy proposal and ranking |
| **Service** | `rd_agent_service.py` | RDAgent main service |

## Qlib Features

### Data Processing
- **Data normalization**: Z-score, min-max, robust scaling
- **Missing data handling**: Forward fill, interpolation
- **Outlier detection**: Statistical methods, isolation forest
- **Feature extraction**: Price-volume features, technical indicators

### Feature Engineering
- **Alpha factors**: 158+ built-in alpha factors
- **Technical indicators**: MACD, RSI, Bollinger Bands, ATR
- **Market microstructure**: VWAP, spread, liquidity
- **Custom factors**: User-defined alpha expressions

### Backtesting
- **Realistic simulation**: Slippage, transaction costs, market impact
- **Multiple frequencies**: Daily, hourly, minute-level
- **Position limits**: Long/short constraints, leverage limits
- **Risk management**: Stop loss, take profit, drawdown limits

### Portfolio Optimization
- **Mean-variance optimization**: Markowitz efficient frontier
- **Risk parity**: Equal risk contribution
- **Minimum variance**: Minimum risk portfolio
- **Maximum Sharpe**: Optimal risk-adjusted returns
- **Custom objectives**: User-defined optimization goals

### Model Library
- **Linear models**: Ridge, Lasso, ElasticNet
- **Tree models**: XGBoost, LightGBM, CatBoost
- **Neural networks**: MLP, LSTM, Transformer
- **Ensemble methods**: Stacking, blending

### Evaluation Metrics
- **Returns**: Annualized, cumulative, daily
- **Risk**: Volatility, max drawdown, VaR, CVaR
- **Risk-adjusted**: Sharpe, Sortino, Calmar ratios
- **Trading**: Win rate, profit factor, turnover

## RDAgent Features

### Hypothesis Generation
- **AI-powered**: LLM generates trading hypotheses
- **Market regime analysis**: Identifies market conditions
- **Factor discovery**: Discovers new alpha factors
- **Strategy templates**: Pre-built strategy patterns

### Knowledge Base
- **Research repository**: Stores successful strategies
- **Factor library**: Proven alpha factors
- **Failure analysis**: Learns from failed strategies
- **Best practices**: Accumulated trading wisdom

### Proposal System
- **Strategy ranking**: Ranks strategies by potential
- **Backtesting automation**: Automated strategy testing
- **Hyperparameter tuning**: Automated optimization
- **Ensemble creation**: Combines multiple strategies

## Workflow

```
┌─────────────────┐
│  Market Data    │ ← Stock prices, fundamentals
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Data Processing │ ← Clean, normalize, extract features
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ RDAgent         │ ← Generate hypotheses
│ Hypothesis Gen  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Feature Eng     │ ← Create alpha factors
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Model Training  │ ← Train ML models
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Strategy        │ ← Trading signals
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Backtest        │ ← Historical simulation
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Portfolio Opt   │ ← Optimal weights
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Evaluation      │ ← Performance metrics
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Knowledge Base  │ ← Store results
└─────────────────┘
```

## Usage Examples

### Qlib Backtesting
```python
from ai_quant_lab.qlib_service import run_backtest

# Run backtest
results = run_backtest(
    strategy='top_k',
    universe='csi300',
    start_date='2020-01-01',
    end_date='2023-12-31',
    top_k=30
)

print(f"Sharpe Ratio: {results['sharpe']}")
print(f"Annual Return: {results['annual_return']}")
```

### Feature Engineering
```python
from ai_quant_lab.qlib_feature_engineering import create_alpha_factors

# Create custom alpha factors
factors = create_alpha_factors(
    data=stock_data,
    factors=['momentum', 'value', 'quality']
)
```

### RDAgent Hypothesis Generation
```python
from ai_quant_lab.rd_agent_service import generate_hypothesis

# AI generates trading hypothesis
hypothesis = generate_hypothesis(
    market_regime='bull_market',
    asset_class='equities',
    timeframe='daily'
)

print(f"Hypothesis: {hypothesis['description']}")
print(f"Factors: {hypothesis['factors']}")
```

### Portfolio Optimization
```python
from ai_quant_lab.qlib_portfolio_opt import optimize_portfolio

# Optimize portfolio
weights = optimize_portfolio(
    returns=expected_returns,
    covariance=cov_matrix,
    method='max_sharpe',
    constraints={'long_only': True}
)
```

## Configuration

### Qlib Configuration
- **Data source**: Yahoo Finance, CSV, database
- **Universe**: Stock pools (CSI300, S&P500, custom)
- **Frequency**: 1min, 5min, 1day
- **Benchmark**: Index for comparison

### RDAgent Configuration
- **LLM model**: GPT-4, Claude, Llama (via Ollama)
- **Knowledge base**: SQLite, vector DB
- **Hypothesis count**: Number of hypotheses per run
- **Evaluation criteria**: Sharpe, returns, drawdown

## Technical Details

- **Framework**: Qlib (Microsoft Research)
- **ML Libraries**: XGBoost, LightGBM, PyTorch
- **Data**: Pandas, NumPy
- **Optimization**: SciPy, cvxpy
- **LLM**: Ollama (local), OpenAI API
- **Database**: SQLite (knowledge base)
- **Language**: Python 3.11+

## Performance Optimization

- **Data caching**: Pickle, HDF5 for fast loading
- **Parallel processing**: Multi-core backtesting
- **GPU acceleration**: PyTorch for neural networks
- **Vectorization**: NumPy/Pandas operations

## Integration with Fincept Terminal

Services exposed via Tauri commands:
- `qlib_service.py`: Main Qlib interface
- `rd_agent_service.py`: RDAgent interface

Called from Rust backend for frontend integration.

---

**Total Modules**: 12 (8 Qlib + 4 RDAgent) | **Frameworks**: Qlib, RDAgent | **ML Models**: 10+ algorithms | **Last Updated**: 2025-12-28
