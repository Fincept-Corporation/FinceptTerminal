# US Financial Data Sources

> Specialized US government financial and regulatory data

## Overview

Access specialized financial and regulatory data from US government agencies. These sources provide SEC filings, treasury data, energy statistics, and corporate disclosures not available in general market data feeds.

## US Government Financial Agencies

| Agency | File Name | API Key | Coverage |
|--------|-----------|---------|----------|
| 💵 **US Treasury** | `fiscaldata_data.py` | ❌ No | Fiscal Data Treasury - debt, revenue, spending |
| 📋 **SEC** | `sec_data.py` | ❌ No | Securities filings, company data |
| 📄 **SEC Edgar** | `edgar_tools.py` | ❌ No | Edgar filings parser and analyzer |
| ⚡ **EIA** | `eia_data.py` | 🔑 Required | Energy Information Administration |

## Data Categories

| Category | Sources | Examples |
|----------|---------|----------|
| **Corporate Filings** | SEC, Edgar | 10-K, 10-Q, 8-K, proxy statements |
| **Fiscal Policy** | Fiscal Data | Government debt, deficits, spending |
| **Energy** | EIA | Oil, gas, electricity, renewables |
| **Treasury** | Fiscal Data | Bond yields, auctions, cash flow |

## API Key Setup

```bash
# Energy Information Administration (required)
export EIA_API_KEY="your_key_here"  # Get from: https://www.eia.gov/opendata/register.php
```

## Usage Examples

```python
# US Treasury fiscal data
from fiscaldata_data import get_debt_data
debt = get_debt_data(start_date='2020-01-01', end_date='2023-12-31')

# SEC company filings
from sec_data import get_company_filings
filings = get_company_filings(cik='0000320193', filing_type='10-K')  # Apple

# Edgar tools - parse filings
from edgar_tools import parse_10k
financial_data = parse_10k(cik='0000320193', year=2023)

# EIA energy data
from eia_data import get_petroleum_data
oil_prices = get_petroleum_data(series='PET.RWTC.D')  # WTI crude
```

## Key Features by Source

### Fiscal Data (US Treasury)
- **Treasury.gov official data**
- Federal debt outstanding
- Government revenue and spending
- Treasury auctions
- Interest rates and yields
- SOMA holdings (Fed balance sheet)
- Daily Treasury Statement
- Free, unlimited access

### SEC (Securities and Exchange Commission)
- **EDGAR database access**
- Company filings (10-K, 10-Q, 8-K, S-1)
- Insider transactions
- Institutional holdings (13-F)
- Mutual fund holdings
- Company CIK lookup
- Real-time filing updates

### Edgar Tools
- **Advanced SEC filing parser**
- Extract financial statements
- Parse XBRL data
- Company fundamentals
- Historical filings
- Bulk download capabilities
- Python library integration

### EIA (Energy Information Administration)
- **Official US energy statistics**
- Petroleum (crude oil, gasoline, diesel)
- Natural gas
- Electricity generation and consumption
- Coal production
- Renewable energy
- International energy data
- 4+ million time series

## Data Quality & Coverage

| Source | Update Frequency | Historical Depth | Rate Limits |
|--------|------------------|------------------|-------------|
| Fiscal Data | Daily | 1998+ | Unlimited |
| SEC | Real-time | 1994+ | 10 requests/second |
| Edgar Tools | Real-time | 1994+ | 10 requests/second |
| EIA | Daily/Weekly/Monthly | Varies (10+ years) | 5000/hour |

## Specialized Use Cases

### Fiscal Data
- **Treasury yield curve analysis**
- Debt ceiling monitoring
- Government cash flow
- Federal budget analysis
- Deficit tracking

### SEC & Edgar
- **Fundamental analysis**
- Due diligence research
- Insider trading analysis
- Institutional investor tracking
- Corporate governance research
- M&A activity monitoring

### EIA
- **Energy market analysis**
- Oil price forecasting
- Natural gas supply/demand
- Electricity market trends
- Renewable energy adoption
- Energy policy analysis

## Filing Types (SEC)

| Filing Type | Description | Frequency |
|-------------|-------------|-----------|
| **10-K** | Annual report | Yearly |
| **10-Q** | Quarterly report | Quarterly |
| **8-K** | Current events | As needed |
| **S-1** | IPO registration | One-time |
| **13-F** | Institutional holdings | Quarterly |
| **4** | Insider transactions | As needed |
| **DEF 14A** | Proxy statement | Yearly |

## EIA Data Series Examples

| Series | Description |
|--------|-------------|
| **PET.RWTC.D** | WTI crude oil spot price |
| **NG.RNGWHHD.D** | Henry Hub natural gas price |
| **ELEC.GEN.ALL-US-99.M** | Total US electricity generation |
| **COAL.PROD.TOT-US-TOT.M** | Total US coal production |
| **TOTAL.SODEPR.M** | Solar energy production |

## Technical Details

- **Protocol**: REST API (all), Web scraping (SEC)
- **Format**: JSON (primary), XML/XBRL (SEC filings), CSV
- **Authentication**: API key (EIA only)
- **Rate Limits**: Varies by source
- **Data Delivery**: Real-time (SEC), Daily/Monthly (others)

## Advanced Features

### Edgar Tools
```python
# Get all financials for a company
from edgar_tools import Company
apple = Company('AAPL')
financials = apple.financials

# Search filings
filings = apple.get_filings(form='10-K')
latest_10k = filings[0]
```

### EIA Bulk Downloads
```python
# Bulk data access
from eia_data import get_bulk_data
petroleum_data = get_bulk_data(category='petroleum', series='all')
```

---

**Total Sources**: 4 agencies | **Free Access**: 3 sources | **Unique Coverage**: SEC filings, Treasury data, Energy stats | **Last Updated**: 2025-12-28
