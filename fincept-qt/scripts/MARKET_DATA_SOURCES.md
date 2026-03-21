# Market Data Sources

> Real-time and historical financial market data providers

## Overview

Access comprehensive market data from leading financial data providers covering equities, options, crypto, commodities, and forex. Sources provide real-time quotes, historical prices, fundamentals, and market sentiment.

## Market Data Providers

| Provider | File Name | API Key | Markets Covered |
|----------|-----------|---------|-----------------|
| 📊 **Yahoo Finance** | `yfinance_data.py` | ❌ No | Global stocks, ETFs, indices, forex, crypto, commodities |
| 📈 **Alpha Vantage** | `alphavantage_data.py` | 🔑 Required | Stocks, forex, crypto, technical indicators |
| 🏛️ **NASDAQ** | `nasdaq_data.py` | ❌ No | NASDAQ-listed stocks, market statistics |
| 💼 **FMP** | `fmp_data.py` | 🔑 Required | Financial Modeling Prep - fundamentals, financials |
| 📉 **Trading Economics** | `trading_economics_data.py` | 🔑 Required | Global economic indicators, market forecasts |
| ₿ **CoinGecko** | `coingecko.py` | ❌ No | Cryptocurrency prices, market cap, volume |
| 📊 **CBOE** | `cboe_data.py` | ❌ No | Options data, VIX, volatility indices |
| 📋 **CFTC** | `cftc_data.py` | ❌ No | Commitments of Traders (COT) reports |

## Data Categories

| Category | Sources | Data Types |
|----------|---------|------------|
| **Equities** | Yahoo Finance, NASDAQ, FMP | Quotes, OHLCV, fundamentals, financials |
| **Options** | CBOE | Chains, Greeks, implied volatility, open interest |
| **Crypto** | CoinGecko, Yahoo Finance, Alpha Vantage | Prices, market cap, volume, exchanges |
| **Forex** | Yahoo Finance, Alpha Vantage | Exchange rates, historical FX data |
| **Commodities** | Yahoo Finance, Trading Economics | Metals, energy, agriculture |
| **Derivatives** | CFTC, CBOE | COT reports, futures positioning, volatility |

## API Key Setup

```bash
# Required for premium features
export ALPHA_VANTAGE_API_KEY="your_key_here"     # Get from: https://www.alphavantage.co/support/#api-key
export FMP_API_KEY="your_key_here"               # Get from: https://financialmodelingprep.com/
export TRADING_ECONOMICS_KEY="your_key_here"     # Get from: https://tradingeconomics.com/api/
```

## Usage Examples

```python
# Yahoo Finance - Most versatile, no API key
from yfinance_data import get_historical_data
aapl = get_historical_data('AAPL', period='1y', interval='1d')

# Alpha Vantage - Technical indicators
from alphavantage_data import get_technical_indicator
rsi = get_technical_indicator('AAPL', 'RSI', interval='daily')

# CoinGecko - Crypto data
from coingecko import get_coin_data
btc = get_coin_data('bitcoin', vs_currency='usd', days=30)

# CBOE - Options volatility
from cboe_data import get_vix_data
vix = get_vix_data()

# CFTC - Futures positioning
from cftc_data import get_cot_report
cot = get_cot_report(commodity='gold')
```

## Key Features by Source

### Yahoo Finance
- **Free, unlimited access**
- Global coverage: stocks, ETFs, indices, forex, crypto
- Historical data (decades)
- Real-time quotes (15-min delay)
- Dividends, splits, fundamentals

### Alpha Vantage
- Technical indicators (50+ built-in)
- Intraday data (1min, 5min, 15min, 30min, 60min)
- Forex and crypto support
- Fundamental data
- 500 calls/day (free tier)

### NASDAQ
- NASDAQ-listed securities
- Market statistics
- IPO calendar
- Analyst ratings
- No API key needed

### FMP (Financial Modeling Prep)
- Company fundamentals
- Financial statements
- Valuation metrics
- Analyst estimates
- Historical financials (10+ years)

### Trading Economics
- 300,000+ economic indicators
- Market forecasts
- Historical data (70+ years)
- 196 countries
- Credit ratings

### CoinGecko
- 10,000+ cryptocurrencies
- Exchange data
- Market cap rankings
- DeFi metrics
- NFT data
- Free tier available

### CBOE
- VIX (volatility index)
- Options volume
- Put/call ratios
- Volatility indices
- Historical volatility

### CFTC
- Commitments of Traders (COT) reports
- Futures positioning
- Commercial vs speculative
- Weekly updates
- Historical data since 1986

## Data Quality & Coverage

| Provider | Update Frequency | Historical Depth | Rate Limits |
|----------|------------------|------------------|-------------|
| Yahoo Finance | Real-time (15min delay) | Decades | None (unofficial API) |
| Alpha Vantage | Real-time | 20+ years | 500/day (free), unlimited (paid) |
| NASDAQ | Real-time | Varies | Generous |
| FMP | Daily | 10+ years | 250/day (free), more (paid) |
| Trading Economics | Varies | 70+ years | Based on plan |
| CoinGecko | Real-time | Varies | 50 calls/min (free) |
| CBOE | Real-time | Years | Generous |
| CFTC | Weekly | 1986+ | None |

## Technical Details

- **Protocol**: REST API (all)
- **Format**: JSON (primary), CSV (some)
- **Authentication**: API keys via headers or query params
- **Rate Limiting**: Varies by provider
- **Data Delivery**: Real-time, delayed, end-of-day

---

**Total Sources**: 8 providers | **Free Access**: 4 sources | **Real-time Data**: 5 sources | **Last Updated**: 2025-01-29
