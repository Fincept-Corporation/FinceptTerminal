# Python Contributor Guide

This guide covers Python development for Fincept Terminal — 100+ scripts including 34 Analytics modules and 80+ data fetchers.

> **Prerequisites**: Read the [Contributing Guide](./CONTRIBUTING.md) first for setup and workflow.

---

## Overview

Python powers Fincept Terminal's analytics and data capabilities:
- **34 Analytics modules** — Financial calculations, portfolio optimization, ML models
- **80+ Data fetchers** — APIs for market data, economics, government sources
- **AI Agents** — Geopolitical analysis, trading strategies
- **Technical Analysis** — Indicators and chart patterns

Python scripts are executed by the C++ application via `python_runner.cpp` and communicate through JSON on stdout.

**Related Guides:**
- [C++ Guide](../fincept-cpp/CONTRIBUTING.md) — How C++ executes Python and renders results

---

## Project Structure

```
fincept-cpp/scripts/                   # 100+ Python scripts
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
│   ├── GeopoliticsAgents/             # Geopolitical analysis
│   ├── finagent_core/                 # Core agent framework
│   └── ...
│
├── agno_trading/                      # Trading agents
├── ai_quant_lab/                      # AI/ML analytics
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

## Integration with C++

Python scripts are called from C++ via `python_runner.cpp`:

```
C++ Screen (e.g., research_screen.cpp)
    ↓ calls data service
C++ Data Service (research_data.cpp)
    ↓ calls PythonRunner::run()
python_runner.cpp
    ↓ spawns: python script.py <args>
Python Script
    ↓ outputs JSON to stdout
C++ parses JSON
    ↓ returns data to screen
Screen renders result
```

### Script Execution

```bash
# Scripts are called as:
python Analytics/portfolioManagement/optimize.py '{"symbols":["AAPL","MSFT"]}'

# Expected output:
{"success": true, "data": {"weights": [0.6, 0.4], "sharpe": 1.23}}
```

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

## Testing

### Manual Testing

```bash
cd fincept-cpp/scripts

# Test a data fetcher
python yfinance_data.py quote AAPL

# Test an analytics module
python Analytics/quantstats_analytics.py metrics '{"returns":[0.01,0.02,-0.01]}'
```

---

## Key Libraries

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

---

**Questions?** Open an issue on [GitHub](https://github.com/Fincept-Corporation/FinceptTerminal).
