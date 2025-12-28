# Specialty Data Sources

> Specialized financial utilities, analytics tools, and alternative data

## Overview

Specialized data sources and tools for specific financial tasks including economic databases, technical analysis, news aggregation, report generation, and financial modeling utilities.

## Specialty Data Providers

| Provider | File Name | API Key | Specialty |
|----------|-----------|---------|-----------|
| 📊 **EconDB** | `econdb_data.py` | 🔑 Required | Economic database aggregator - global macro data |
| 📈 **Multpl** | `multpl_data.py` | ❌ No | Historical market valuation multiples |
| 📅 **Economic Calendar** | `economic_calendar.py` | ❌ No | Economic events and release calendar |
| 📰 **Company News** | `fetch_company_news.py` | ❌ No | Company-specific news aggregation |

## Financial Analysis Tools

| Tool | File Name | API Key | Purpose |
|------|-----------|---------|---------|
| 🔧 **Technical Indicators** | `compute_technicals.py` | ❌ No | Compute technical indicators from OHLCV data |
| 📄 **Report Generator** | `financial_report_generator.py` | ❌ No | Generate financial analysis reports |
| 💼 **FinancePy** | `financepy_wrapper.py` | ❌ No | Financial calculations library wrapper |

## Data Categories

| Category | Tools | Use Cases |
|----------|-------|-----------|
| **Economic Data** | EconDB, Economic Calendar | Macro research, event tracking |
| **Market Valuation** | Multpl | P/E ratios, yields, historical context |
| **News & Events** | Company News, Economic Calendar | Sentiment, event-driven trading |
| **Technical Analysis** | Compute Technicals | Chart analysis, indicators |
| **Financial Modeling** | FinancePy, Report Generator | Valuations, bond pricing, derivatives |

## Usage Examples

```python
# EconDB - Global economic data
from econdb_data import get_indicator
gdp_data = get_indicator('RGDP', countries=['USA', 'CHN'], start_date='2020-01-01')

# Multpl - Historical market multiples
from multpl_data import get_pe_ratio
sp500_pe = get_pe_ratio(index='sp500', metric='pe_ratio')

# Economic calendar
from economic_calendar import get_events
upcoming = get_events(country='US', start_date='2024-01-01', end_date='2024-01-31')

# Company news
from fetch_company_news import get_news
aapl_news = get_news(ticker='AAPL', days=7)

# Technical indicators
from compute_technicals import calculate_indicators
indicators = calculate_indicators(ohlcv_data, indicators=['RSI', 'MACD', 'BB'])

# Financial report generation
from financial_report_generator import generate_report
report = generate_report(ticker='AAPL', report_type='comprehensive')

# FinancePy - Financial calculations
from financepy_wrapper import price_bond
bond_price = price_bond(coupon=5.0, maturity=10, ytm=4.5)
```

## Key Features by Source

### EconDB
- **Economic data aggregator**
- 200+ countries
- 200,000+ economic indicators
- Standardized data format
- Historical data (decades)
- API access with key

### Multpl
- **Historical market metrics**
- S&P 500 P/E ratio (Shiller, trailing)
- Dividend yields
- Market cap to GDP
- 10-year treasury yields
- Historical valuation context
- Free data source

### Economic Calendar
- **Global economic events**
- Central bank meetings
- Economic data releases (GDP, CPI, employment)
- Earnings calendars
- Political events
- Real-time updates

### Company News
- **News aggregation**
- Company-specific news
- Multiple news sources
- Sentiment analysis ready
- Historical news archives
- RSS/API feeds

### Compute Technicals
- **Technical indicator library**
- 50+ indicators (RSI, MACD, Bollinger Bands, etc.)
- Custom indicator support
- Vectorized calculations
- Works with any OHLCV data
- Pandas DataFrame output

### Financial Report Generator
- **Automated report creation**
- Company analysis reports
- Valuation models
- Financial statement analysis
- Charts and visualizations
- PDF/HTML export

### FinancePy
- **Financial calculations library**
- Bond pricing and yields
- Option pricing (Black-Scholes, binomial)
- Interest rate models
- Credit risk calculations
- Portfolio analytics

## EconDB Indicators

| Indicator | Description |
|-----------|-------------|
| **RGDP** | Real GDP |
| **CPI** | Consumer Price Index |
| **URATE** | Unemployment Rate |
| **POLICY** | Central Bank Policy Rate |
| **TB** | Trade Balance |
| **GDEBT** | Government Debt |
| **IP** | Industrial Production |

## Technical Indicators Available

| Indicator | Type | Description |
|-----------|------|-------------|
| **RSI** | Momentum | Relative Strength Index |
| **MACD** | Momentum | Moving Average Convergence Divergence |
| **BB** | Volatility | Bollinger Bands |
| **SMA/EMA** | Trend | Simple/Exponential Moving Averages |
| **ATR** | Volatility | Average True Range |
| **Stochastic** | Momentum | Stochastic Oscillator |
| **ADX** | Trend | Average Directional Index |

## FinancePy Capabilities

| Module | Functions |
|--------|-----------|
| **Bonds** | Price, yield, duration, convexity |
| **Options** | Black-Scholes, Greeks, implied volatility |
| **Swaps** | Interest rate swaps, valuation |
| **Credit** | CDS pricing, credit spreads |
| **Calendars** | Business day calculations |

## API Key Setup

```bash
# EconDB (required)
export ECONDB_API_KEY="your_key_here"  # Get from: https://www.econdb.com/
```

## Data Quality & Coverage

| Source | Update Frequency | Historical Depth | Rate Limits |
|--------|------------------|------------------|-------------|
| EconDB | Daily/Monthly | Decades | Based on plan |
| Multpl | Daily | 100+ years | Unlimited |
| Economic Calendar | Real-time | Current + future events | Unlimited |
| Company News | Real-time | Varies | Varies by source |
| Compute Technicals | On-demand | N/A (calculates) | None |
| Report Generator | On-demand | N/A (generates) | None |
| FinancePy | On-demand | N/A (calculates) | None |

## Technical Details

- **Protocol**: REST API (data sources), Python libraries (tools)
- **Format**: JSON (APIs), Pandas DataFrames (tools)
- **Authentication**: API keys where required
- **Dependencies**: NumPy, Pandas, TA-Lib (for technicals)
- **Performance**: Optimized for large datasets

## Use Case Examples

### Macro Research
```python
# Combine EconDB + Economic Calendar
indicators = get_indicator('CPI', countries=['USA'])
events = get_events(country='US', event_type='inflation')
```

### Technical Analysis
```python
# Full technical analysis suite
from compute_technicals import full_analysis
analysis = full_analysis(ticker='AAPL', period='6m')
# Returns: RSI, MACD, Bollinger Bands, SMA, EMA, Volume analysis
```

### Valuation Research
```python
# Historical context for valuations
current_pe = get_current_pe('AAPL')
historical_pe = get_pe_ratio(index='sp500')
# Compare current vs historical
```

---

**Total Sources**: 7 (4 data + 3 tools) | **Free Access**: 6 sources | **Unique Features**: Technicals, Reports, Financial math | **Last Updated**: 2025-12-28
