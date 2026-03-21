# China Data Sources

> Comprehensive Chinese market and economic data via AkShare ecosystem

## Overview

Access extensive Chinese financial markets, economic indicators, and alternative data through the AkShare library and official Chinese government statistics. AkShare is a comprehensive Python library specifically designed for Chinese market data collection.

**AkShare**: Open-source financial data interface for China - covers stocks, bonds, funds, futures, options, macroeconomic data, and more from 100+ Chinese data sources.

## AkShare Data Modules

| Module | File Name | API Key | Coverage |
|--------|-----------|---------|----------|
| 📊 **Core Markets** | `akshare_data.py` | ❌ No | Stocks (A-shares, HK), indices, ETFs, real-time quotes |
| 📈 **Analysis Tools** | `akshare_analysis.py` | ❌ No | Technical analysis, market sentiment, money flow |
| 💰 **Bonds** | `akshare_bonds.py` | ❌ No | Government bonds, corporate bonds, convertibles |
| 📉 **Derivatives** | `akshare_derivatives.py` | ❌ No | Futures, options, warrants |
| 🇨🇳 **China Economics** | `akshare_economics_china.py` | ❌ No | GDP, CPI, PMI, trade, industrial production |
| 🌍 **Global Economics** | `akshare_economics_global.py` | ❌ No | International economic data via Chinese sources |
| 🏦 **Funds** | `akshare_funds_expanded.py` | ❌ No | Mutual funds, ETFs, fund rankings, managers |
| 🔄 **Alternative Data** | `akshare_alternative.py` | ❌ No | News sentiment, social media, alternative indicators |

## Official Chinese Statistics

| Source | File Name | API Key | Coverage |
|--------|-----------|---------|----------|
| 📊 **National Bureau of Statistics** | `cnstats_data.py` | ❌ No | Official Chinese government economic statistics |

## Data Categories

| Category | Sources | Examples |
|----------|---------|----------|
| **A-Share Stocks** | akshare_data, akshare_analysis | SSE, SZSE quotes, fundamentals, technicals |
| **Hong Kong** | akshare_data | HSI constituents, HK-listed stocks |
| **Fixed Income** | akshare_bonds | Government bonds, corporate bonds, yields |
| **Derivatives** | akshare_derivatives | Commodity futures, index futures, options |
| **Funds** | akshare_funds | Mutual funds, ETF data, fund performance |
| **Macroeconomic** | akshare_economics_china, cnstats | GDP, inflation, trade, monetary policy |
| **Alternative** | akshare_alternative | Sentiment, news, social media trends |

## Usage Examples

```python
# Stock data - A-shares
from akshare_data import get_stock_data
sh600000 = get_stock_data('600000', market='sh')  # Shanghai stock

# Technical analysis
from akshare_analysis import get_technical_indicators
indicators = get_technical_indicators('000001', indicator='MACD')

# Bond yields
from akshare_bonds import get_bond_yields
yields = get_bond_yields(bond_type='china_treasury')

# Futures data
from akshare_derivatives import get_futures_data
futures = get_futures_data(exchange='SHFE', symbol='AU')  # Gold futures

# Chinese macroeconomic data
from akshare_economics_china import get_gdp_data
gdp = get_gdp_data()

# Fund data
from akshare_funds_expanded import get_fund_rankings
top_funds = get_fund_rankings(category='equity', period='1y')

# Official statistics
from cnstats_data import get_economic_indicator
cpi = get_economic_indicator('CPI', start_date='2020-01-01')
```

## Key Features by Module

### akshare_data.py (Core Markets)
- A-share stocks (Shanghai, Shenzhen)
- Hong Kong stocks
- Real-time quotes
- Historical OHLCV
- Index data (SSE, SZSE, CSI)

### akshare_analysis.py (Analysis)
- Technical indicators
- Market sentiment
- Money flow analysis
- Dragon-Tiger List (major trades)
- Margin trading data

### akshare_bonds.py (Fixed Income)
- Government bonds
- Corporate bonds
- Convertible bonds
- Bond yields and ratings
- Issuance data

### akshare_derivatives.py (Derivatives)
- Commodity futures (SHFE, DCE, CZCE)
- Financial futures (CFFEX)
- Options (SSE, SZSE)
- Warrants

### akshare_economics_china.py (China Economics)
- GDP (national, provincial)
- CPI, PPI inflation
- PMI (manufacturing, services)
- Trade balance
- Industrial production
- Fixed asset investment

### akshare_economics_global.py (Global Economics)
- International economic data
- Global market indices
- Commodity prices
- FX rates

### akshare_funds_expanded.py (Funds)
- Mutual fund data
- ETF information
- Fund rankings
- Manager performance
- Net asset value (NAV)

### akshare_alternative.py (Alternative Data)
- News sentiment
- Social media trends
- Search trends
- Alternative economic indicators

### cnstats_data.py (Official Statistics)
- National Bureau of Statistics data
- Official government releases
- Authoritative source for policy analysis

## Chinese Market Structure

| Exchange | Code | Markets |
|----------|------|---------|
| **Shanghai Stock Exchange** | SSE/SH | A-shares, B-shares, bonds, funds |
| **Shenzhen Stock Exchange** | SZSE/SZ | A-shares, ChiNext, SME board |
| **Hong Kong Exchange** | HKEX/HK | Main board, GEM |
| **Shanghai Futures Exchange** | SHFE | Metals, energy |
| **Dalian Commodity Exchange** | DCE | Agriculture, chemicals |
| **Zhengzhou Commodity Exchange** | CZCE | Agriculture, energy |
| **China Financial Futures Exchange** | CFFEX | Index futures, bonds |

## Data Access

**No API Keys Required**: AkShare scrapes public data sources - all modules are free and open-source.

**Update Frequency**:
- Intraday: Real-time to 3-minute delay
- Daily: End-of-day updates
- Economic: Monthly/quarterly releases

**Language**: Data returned in Chinese (some modules support English)

## Technical Details

- **Library**: AkShare Python package
- **Protocol**: Web scraping + API aggregation
- **Format**: Pandas DataFrames (Python), JSON export available
- **Sources**: 100+ Chinese financial websites and APIs
- **Rate Limits**: None (respectful scraping)
- **Historical Data**: Varies by source (typically 5+ years)

## Installation

```bash
pip install akshare
```

---

**Total Modules**: 9 (8 AkShare + 1 official stats) | **Free Access**: All sources | **Markets**: A-shares, HK, Futures, Bonds | **Last Updated**: 2025-12-28
