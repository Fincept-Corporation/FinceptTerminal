# Economic Data Sources

> International economic organizations and central banks providing macroeconomic indicators

## Overview

Access comprehensive economic data from world-leading organizations including central banks, development institutions, and international agencies. All sources provide free access to macroeconomic indicators, trade statistics, and policy data.

## Global Economic Organizations

| Organization | File Name | API Key | Coverage |
|--------------|-----------|---------|----------|
| 🏦 **FRED** | `fred_data.py` | 🔑 Required | US Federal Reserve Economic Data - 800K+ series |
| 🌍 **World Bank** | `worldbank_data.py` | ❌ No | Global development indicators, 200+ countries |
| 💰 **IMF** | `imf_data.py` | ❌ No | International finance, GDP, inflation, balance of payments |
| 📊 **OECD** | `oecd_data.py` | ❌ No | Developed economies, 38 member countries |
| 🇺🇸 **BEA** | `bea_data.py` | 🔑 Required | US Bureau of Economic Analysis - GDP, trade, industries |
| 📈 **BLS** | `bls_data.py` | 🔑 Optional | US Bureau of Labor Statistics - employment, CPI, wages |
| 🇪🇺 **ECB** | `ecb_data.py` | ❌ No | European Central Bank - Eurozone monetary data |
| 🌐 **WTO** | `wto_data.py` | ❌ No | World Trade Organization - international trade stats |
| 📦 **WITS** | `wits_trade_data.py` | ❌ No | World Integrated Trade Solution - tariffs, trade flows |
| 🎓 **UNESCO** | `unesco_data.py` | ❌ No | Education, science, culture statistics |
| 🆘 **HDX** | `hdx_data.py` | ❌ No | Humanitarian Data Exchange - crisis data |

## Data Categories

| Category | Sources | Examples |
|----------|---------|----------|
| **Macroeconomic** | FRED, World Bank, IMF, OECD | GDP, inflation, unemployment, interest rates |
| **Trade & Commerce** | WTO, WITS, BEA | Import/export, tariffs, trade balance |
| **Labor & Employment** | BLS, OECD | Employment rates, wages, CPI, productivity |
| **Monetary Policy** | FRED, ECB, IMF | Money supply, exchange rates, central bank rates |
| **Development** | World Bank, UNESCO, HDX | Poverty, education, health, infrastructure |

## API Key Setup

```bash
# Required
export FRED_API_KEY="your_key_here"          # Get from: https://fred.stlouisfed.org/docs/api/api_key.html
export BEA_API_KEY="your_key_here"           # Get from: https://apps.bea.gov/API/signup/

# Optional (improves limits)
export BLS_API_KEY="your_key_here"           # Get from: https://www.bls.gov/developers/
```

## Usage Examples

```python
# FRED - US economic data
from fred_data import get_series
gdp = get_series('GDP', observation_start='2020-01-01')

# World Bank - Global indicators
from worldbank_data import get_indicator
poverty = get_indicator('SI.POV.DDAY', country='all', date='2010:2020')

# IMF - International finance
from imf_data import get_data
balance = get_data(database='BOP', indicator='BCA', country='USA')

# OECD - Developed economies
from oecd_data import get_dataset
unemployment = get_dataset('UNEMP', country='USA+GBR+DEU')
```

## Key Features by Source

### FRED (Federal Reserve Economic Data)
- 800,000+ time series
- US-focused economic indicators
- Real-time updates
- Historical data back to 1776

### World Bank
- 200+ countries and regions
- 1,400+ indicators
- Development and poverty metrics
- Project-level data

### IMF (International Monetary Fund)
- Balance of payments
- International financial statistics
- Exchange rates
- Government finance

### OECD
- 38 member countries (developed economies)
- Standardized economic indicators
- Comparative country data
- Policy analysis

### BEA (Bureau of Economic Analysis)
- US GDP by industry
- International trade statistics
- Regional economic data
- Input-output tables

### BLS (Bureau of Labor Statistics)
- Employment and unemployment
- Consumer Price Index (CPI)
- Producer Price Index (PPI)
- Wage and productivity data

### ECB (European Central Bank)
- Eurozone monetary statistics
- Interest rates
- Exchange rates
- Banking sector data

### WTO (World Trade Organization)
- International trade flows
- Tariff data
- Trade agreements
- Dispute resolution

### WITS (World Integrated Trade Solution)
- Detailed trade statistics
- Tariff and non-tariff measures
- 266 countries/territories
- Product-level data (HS codes)

## Technical Details

- **Protocol**: REST API (all sources)
- **Format**: JSON (primary), CSV/XML (some)
- **Rate Limits**: Varies by source (generally generous)
- **Historical Data**: Most sources provide 10+ years
- **Update Frequency**: Daily to monthly depending on indicator

---

**Total Sources**: 11 organizations | **Free Access**: 8 sources | **Last Updated**: 2025-12-28
