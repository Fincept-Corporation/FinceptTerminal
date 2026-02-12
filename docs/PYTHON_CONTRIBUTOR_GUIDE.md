# Python Contributor Guide

This guide covers Python development for Fincept Terminal - 119 scripts including 34 Analytics modules and 80+ data fetchers.

> **Prerequisites**: Read the [Contributing Guide](./CONTRIBUTING.md) first for setup and workflow.

---

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Analytics Modules](#analytics-modules)
- [Data Fetcher Scripts](#data-fetcher-scripts)
- [Script Standards](#script-standards)
- [Integration with Rust](#integration-with-rust)
- [Testing](#testing)
- [Resources](#resources)

---

## Overview

Python powers Fincept Terminal's analytics and data capabilities:
- **34 Analytics modules** - Financial calculations, portfolio optimization, ML models
- **80+ Data fetchers** - APIs for market data, economics, government sources
- **AI Agents** - Geopolitical analysis, trading strategies
- **Technical Analysis** - Indicators and chart patterns

**Related Guides:**
- [Rust Guide](./RUST_CONTRIBUTOR_GUIDE.md) - How Rust executes Python
- [TypeScript Guide](./TYPESCRIPT_CONTRIBUTOR_GUIDE.md) - Frontend that displays results

---

## Project Structure

```
src-tauri/resources/scripts/           # 119 Python scripts
│
├── Analytics/                         # 34 analytics modules
│   ├── equityInvestment/              # Stock valuation, DCF
│   ├── portfolioManagement/           # Portfolio optimization
│   ├── derivatives/                   # Options pricing, Greeks
│   ├── fixedIncome/                   # Bond analytics
│   ├── corporateFinance/              # M&A, valuation
│   ├── economics/                     # Economic models
│   ├── quant/                         # Quantitative analysis
│   ├── alternateInvestment/           # Alternative assets
│   ├── backtesting/                   # Strategy backtesting
│   ├── finanicalanalysis/             # Financial statements
│   ├── technical_analysis/            # Technical indicators
│   │
│   ├── pyportfolioopt_wrapper/        # Portfolio optimization
│   ├── quantstats_analytics.py        # Portfolio metrics
│   ├── skfolio_wrapper.py             # Scikit-portfolio
│   ├── riskfoliolib_wrapper.py        # Risk-folio lib
│   ├── talipp_wrapper/                # Technical indicators
│   ├── gs_quant_wrapper/              # Goldman Sachs Quant
│   ├── gluonts_wrapper/               # Time series forecasting
│   ├── statsmodels_wrapper/           # Statistical models
│   ├── finrl/                         # Reinforcement learning
│   └── vnpy_wrapper/                  # VN.PY trading
│
├── agents/                            # AI agents
│   ├── geopolitics/                   # Geopolitical analysis
│   └── trading/                       # Trading agents
│
├── strategies/                        # Trading strategies
├── technicals/                        # Technical analysis
│
├── yfinance_data.py                   # Yahoo Finance
├── fred_data.py                       # Federal Reserve
├── imf_data.py                        # IMF data
├── worldbank_data.py                  # World Bank
├── oecd_data.py                       # OECD data
├── ecb_data.py                        # European Central Bank
├── bis_data.py                        # Bank for Intl Settlements
├── nasdaq_data.py                     # NASDAQ data
├── sec_data.py                        # SEC filings
├── edgar_tools.py                     # EDGAR database
├── fmp_data.py                        # Financial Modeling Prep
├── databento_provider.py              # Databento market data
│
├── akshare_*.py                       # 20+ Chinese market scripts
├── *_gov_api.py                       # Government data APIs
└── ...                                # 40+ more data fetchers
```

---

## Analytics Modules

### Core Financial Modules

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| `equityInvestment/` | Stock analysis | DCF, comparables, valuation |
| `portfolioManagement/` | Portfolio optimization | Mean-variance, Black-Litterman |
| `derivatives/` | Options & futures | Black-Scholes, Greeks, pricing |
| `fixedIncome/` | Bond analytics | Duration, convexity, yields |
| `corporateFinance/` | Corporate valuation | M&A, LBO, WACC |
| `economics/` | Economic models | GDP, inflation, policy |
| `quant/` | Quantitative analysis | Factor models, risk |

### Library Wrappers

| Wrapper | Library | Purpose |
|---------|---------|---------|
| `pyportfolioopt_wrapper/` | PyPortfolioOpt | Portfolio optimization |
| `quantstats_analytics.py` | QuantStats | Performance metrics |
| `skfolio_wrapper.py` | Skfolio | Scikit-learn portfolios |
| `riskfoliolib_wrapper.py` | Riskfolio-Lib | Risk management |
| `talipp_wrapper/` | Talipp | Technical indicators |
| `statsmodels_wrapper/` | Statsmodels | Statistical models |
| `gluonts_wrapper/` | GluonTS | Time series forecasting |
| `gs_quant_wrapper/` | GS Quant | Goldman Sachs library |
| `finrl/` | FinRL | Reinforcement learning |

---

## Data Fetcher Scripts

### Market Data

| Script | Source | Data Types |
|--------|--------|------------|
| `yfinance_data.py` | Yahoo Finance | Stocks, ETFs, options |
| `databento_provider.py` | Databento | Real-time market data |
| `nasdaq_data.py` | NASDAQ | US equities |
| `fmp_data.py` | Financial Modeling Prep | Fundamentals |
| `coingecko.py` | CoinGecko | Cryptocurrency |

### Economic Data

| Script | Source | Data Types |
|--------|--------|------------|
| `fred_data.py` | Federal Reserve | US economic indicators |
| `imf_data.py` | IMF | Global economic data |
| `worldbank_data.py` | World Bank | Development indicators |
| `oecd_data.py` | OECD | OECD statistics |
| `ecb_data.py` | ECB | European data |
| `bis_data.py` | BIS | Banking statistics |
| `bls_data.py` | Bureau of Labor | Employment, CPI |
| `bea_data.py` | Bureau of Economic Analysis | GDP, trade |

### Chinese Market (AkShare)

20+ scripts for Chinese market data:
- `akshare_stocks_*.py` - A-shares data
- `akshare_futures.py` - Futures data
- `akshare_funds_*.py` - Fund data
- `akshare_macro.py` - Macro indicators
- `akshare_index.py` - Index data

### Government & Regulatory

| Script | Source |
|--------|--------|
| `sec_data.py` | SEC filings |
| `edgar_tools.py` | EDGAR database |
| `congress_gov_data.py` | US Congress |
| `cftc_data.py` | CFTC positions |
| `government_us_data.py` | US government |

---

## Script Standards

### Input/Output Format

All scripts use JSON for communication:

```python
import sys
import json

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "Usage: script.py <command>"}))
        sys.exit(1)

    command = sys.argv[1]

    try:
        result = process_command(command)
        print(json.dumps({"success": True, "data": result}))
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)

if __name__ == "__main__":
    main()
```

### Type Hints

Always use type hints:

```python
from typing import List, Dict, Any, Optional

def calculate_returns(prices: List[float], period: int = 1) -> Dict[str, Any]:
    """Calculate returns from price series."""
    ...
```

### Error Handling

```python
def safe_fetch(symbol: str) -> Dict[str, Any]:
    try:
        if not symbol:
            return {"success": False, "error": "Symbol required"}

        data = fetch_data(symbol)
        return {"success": True, "data": data}

    except ValueError as e:
        return {"success": False, "error": f"Invalid input: {e}"}
    except Exception as e:
        return {"success": False, "error": f"Unexpected error: {e}"}
```

---

## Integration with Rust

Python scripts are called from Rust:

```
Frontend (React)
    ↓ invoke('calculate_portfolio')
Rust (Tauri Command)
    ↓ Command::new("python").arg("script.py")
Python Script
    ↓ JSON to stdout
Rust
    ↓ parse JSON, return to frontend
Frontend
    ↓ display results
```

### Script Execution

```bash
# Scripts are called as:
python Analytics/portfolioManagement/optimize.py '{"symbols":["AAPL","MSFT"]}'

# Expected output:
{"success": true, "data": {"weights": [0.6, 0.4], "sharpe": 1.23}}
```

---

## Testing

### Manual Testing

```bash
cd src-tauri/resources/scripts

# Test a data fetcher
python yfinance_data.py quote AAPL

# Test an analytics module
python Analytics/quantstats_analytics.py metrics '{"returns":[0.01,0.02,-0.01]}'
```

### Test Pattern

```python
def test_calculate_returns():
    prices = [100, 102, 101, 105]
    result = calculate_returns(prices)

    assert result["success"] == True
    assert "returns" in result["data"]
    assert len(result["data"]["returns"]) == 3

if __name__ == "__main__":
    test_calculate_returns()
    print("Tests passed!")
```

---

## Resources

### Key Libraries

| Library | Purpose |
|---------|---------|
| `pandas` | Data manipulation |
| `numpy` | Numerical computing |
| `scipy` | Scientific computing |
| `yfinance` | Yahoo Finance API |
| `akshare` | Chinese market data |
| `pyportfolioopt` | Portfolio optimization |
| `quantstats` | Performance analytics |
| `ta-lib` / `talipp` | Technical indicators |
| `statsmodels` | Statistical models |
| `scikit-learn` | Machine learning |
| `langchain` | LLM integration |

### Documentation

- [Pandas Docs](https://pandas.pydata.org/docs/)
- [NumPy Docs](https://numpy.org/doc/)
- [PyPortfolioOpt](https://pyportfolioopt.readthedocs.io/)
- [QuantStats](https://github.com/ranaroussi/quantstats)
- [AkShare](https://akshare.readthedocs.io/)

### Related Guides

- [Contributing Guide](./CONTRIBUTING.md) - General workflow
- [Rust Guide](./RUST_CONTRIBUTOR_GUIDE.md) - How Rust calls Python
- [TypeScript Guide](./TYPESCRIPT_CONTRIBUTOR_GUIDE.md) - Frontend integration

---

**Questions?** Open an issue on [GitHub](https://github.com/Fincept-Corporation/FinceptTerminal).
