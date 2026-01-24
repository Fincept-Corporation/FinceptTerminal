# pypme Wrapper

Comprehensive Python wrapper for the pypme library, providing Public Market Equivalent (PME) calculations for private equity and venture capital performance measurement.

## Overview

This wrapper provides 100% coverage of the pypme library with 7 functions:

- **PME Calculations**: Standard Public Market Equivalent analysis
- **xPME**: Extended PME with time-weighted adjustments
- **Tessa Integration**: Automatic market data fetching from Yahoo Finance or CoinGecko
- **Verbose Outputs**: Detailed calculation breakdowns

## Installation

```bash
pip install pypme==0.6.2
```

## Module Structure

```
pypme_wrapper/
├── __init__.py              # Main exports
├── core.py                  # All PME functions (7 total)
└── README.md                # This file
```

## Quick Start

### Basic PME Calculation

```python
from pypme_wrapper import calculate_pme

cashflows = [-1000, 0, 1200]
prices = [100, 110, 120]
pme_prices = [100, 105, 115]

result = calculate_pme(cashflows, prices, pme_prices)
print(f"PME: {result['pme']:.4f}")
```

### Extended PME (xPME)

```python
from pypme_wrapper import calculate_xpme
from datetime import date

dates = [date(2020, 1, 1), date(2020, 6, 1), date(2020, 12, 31)]
cashflows = [-1000, 0, 1200]
prices = [100, 110, 120]
pme_prices = [100, 105, 115]

result = calculate_xpme(dates, cashflows, prices, pme_prices)
print(f"xPME: {result['xpme']:.4f}")
```

### Verbose Output with Details

```python
from pypme_wrapper import calculate_verbose_pme

result = calculate_verbose_pme(cashflows, prices, pme_prices)
print(f"PME: {result['pme']:.4f}")
print(f"NAV PME: {result['nav_pme']:.4f}")
print("Calculation details:", result['details'])
```

### Using Tessa for Market Data

```python
from pypme_wrapper import calculate_tessa_xpme

result = calculate_tessa_xpme(
    dates=dates,
    cashflows=cashflows,
    prices=prices,
    pme_ticker='SPY',
    pme_source='yahoo'
)
print(f"xPME (with SPY benchmark): {result['xpme']:.4f}")
```

## Function Reference

| Function | Description |
|----------|-------------|
| `calculate_pme` | Standard PME calculation |
| `calculate_verbose_pme` | PME with detailed output (NAV, calculation details) |
| `calculate_xpme` | Extended PME with time-weighted adjustments |
| `calculate_verbose_xpme` | xPME with detailed output |
| `calculate_tessa_xpme` | xPME with automatic market data from Tessa |
| `calculate_tessa_verbose_xpme` | Tessa xPME with detailed output |
| `pick_prices_from_dataframe` | Extract prices from DataFrame for given dates |

## Parameters

### Common Parameters

- **dates**: List of dates (datetime.date or 'YYYY-MM-DD' strings)
- **cashflows**: List of cashflows (negative = investment, positive = distribution)
- **prices**: List of portfolio prices/NAV at each date
- **pme_prices**: List of public market index prices at each date
- **pme_ticker**: Ticker symbol for benchmark (e.g., 'SPY', 'QQQ')
- **pme_source**: Data source - 'yahoo' or 'coingecko'

### Return Values

All functions return dictionaries with:
- **pme/xpme**: The calculated PME or xPME ratio
- **nav_pme**: (verbose only) Net Asset Value in PME terms
- **details**: (verbose only) DataFrame with step-by-step calculations

## Understanding PME

PME (Public Market Equivalent) measures private investment performance by comparing it to a public market index:
- PME > 1: Outperformed public market
- PME < 1: Underperformed public market
- PME = 1: Matched public market performance

xPME extends this by accounting for the timing of cashflows more accurately.

## Testing

```bash
python core.py
```

## Version

- **pypme**: 0.6.2
- **Wrapper Version**: 1.0.0
- **Coverage**: 100% (7/7 functions)
- **Last Updated**: 2026-01-23

## License

MIT License - Same as Fincept Terminal
