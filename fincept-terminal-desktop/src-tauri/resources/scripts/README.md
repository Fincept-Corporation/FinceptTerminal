# Python Scripts Library

> Comprehensive collection of data sources, analytics, and AI agents for Fincept Terminal

## Overview

This directory contains Python scripts powering the backend analytics, data integrations, and AI capabilities of Fincept Terminal. All scripts are executed through Tauri's Rust backend via IPC commands.

## Directory Structure

```
scripts/
├── agents/              # AI agents for trading and geopolitical analysis
├── agno_trading/        # Agno trading system framework
├── ai_quant_lab/        # Quantitative research lab (Qlib, RDAgent)
├── Analytics/           # Financial analytics modules
├── *.py                 # Data source integrations (60+ providers)
└── README.md           # This file
```

## Quick Links

### Data Sources Documentation

| Category | Description | Link |
|----------|-------------|------|
| 🏛️ **Government Data** | 19 countries/portals - official statistics | [GOVERNMENT_DATA_SOURCES.md](./GOVERNMENT_DATA_SOURCES.md) |
| 🌍 **Economic Data** | 11 organizations - FRED, World Bank, IMF, OECD | [ECONOMIC_DATA_SOURCES.md](./ECONOMIC_DATA_SOURCES.md) |
| 📊 **Market Data** | 9 providers - stocks, options, crypto, forex | [MARKET_DATA_SOURCES.md](./MARKET_DATA_SOURCES.md) |
| 🇨🇳 **China Data** | 9 modules - AkShare ecosystem, Chinese markets | [CHINA_DATA_SOURCES.md](./CHINA_DATA_SOURCES.md) |
| 🌏 **Regional Data** | 5 sources - Japan, Sweden, Spain, Africa, Asia | [REGIONAL_DATA_SOURCES.md](./REGIONAL_DATA_SOURCES.md) |
| 🇺🇸 **US Financial** | 4 agencies - SEC, Treasury, Energy | [US_FINANCIAL_DATA_SOURCES.md](./US_FINANCIAL_DATA_SOURCES.md) |
| 🔧 **Specialty Data** | 7 tools - EconDB, technicals, reports, news | [SPECIALTY_DATA_SOURCES.md](./SPECIALTY_DATA_SOURCES.md) |
| 🛰️ **Satellite & Geo** | 4 providers - NASA, ESA, ocean data, tracking | [SATELLITE_GEO_DATA_SOURCES.md](./SATELLITE_GEO_DATA_SOURCES.md) |

### Module Documentation

| Category | Description | Link |
|----------|-------------|------|
| 📊 **Analytics** | 80+ modules - equity, portfolio, derivatives, economics | [Analytics/README.md](./Analytics/README.md) |
| 🤖 **AI Agents** | 30+ agents - hedge funds, investors, geopolitics | [agents/README.md](./agents/README.md) |
| 🔬 **AI Quant Lab** | Qlib + RDAgent - automated strategy research | [ai_quant_lab/README.md](./ai_quant_lab/README.md) |
| 🚀 **Agno Trading** | Multi-agent trading system with debates | [agno_trading/README.md](./agno_trading/README.md) |

## Key Features

### Data Integration (60+ Sources)
- **Market Data**: Yahoo Finance, Polygon.io, Alpha Vantage, TradingView, Databento
- **Economic Data**: FRED, World Bank, IMF, OECD, ECB, BEA, BLS
- **Crypto**: CoinGecko, Kraken, Binance
- **Government**: SEC Edgar, Congress.gov, Federal Reserve
- **International**: AkShare (China), Eurostat (EU), data.gov variants

### Analytics Modules
- **Equity Investment**: DCF, DDM, multiples valuation, fundamental analysis
- **Portfolio Management**: Optimization, risk management, ETF analytics
- **Derivatives**: Options pricing, Greeks, forward commitments
- **Economics**: Growth analysis, policy analysis, trade & geopolitics
- **Alternative Investments**: Real estate, hedge funds, private capital, crypto
- **Quantitative**: CFA quant models, rate calculations
- **Financial Analysis**: Statement analysis, quality metrics, tax analysis

### AI & Machine Learning
- **Agno Trading**: Multi-agent trading system with debate orchestration
- **Geopolitical Agents**: Grand Chessboard, Prisoners of Geography frameworks
- **Investor Personas**: Warren Buffett, Benjamin Graham strategies
- **Hedge Fund Agents**: Bridgewater, Citadel, Renaissance, Two Sigma
- **Quant Lab**: Qlib integration, RDAgent for hypothesis generation

### Backtesting Frameworks
- **LEAN Engine**: Institutional-grade algorithmic trading
- **Backtrading.py**: Flexible Python backtesting
- **VectorBT**: High-performance vectorized backtesting
- **FastTrade**: Lightweight backtesting library

## Usage Pattern

Scripts are invoked from the frontend via Tauri commands:

```typescript
import { invoke } from '@tauri-apps/api/core';

// Example: Fetch market data
const data = await invoke('execute_python_command', {
  script: 'yfinance_tools',
  method: 'get_historical_data',
  params: { ticker: 'AAPL', period: '1y' }
});
```

## Development Guidelines

### Adding New Data Sources
1. Create `{source}_data.py` in scripts root
2. Implement standardized response format
3. Add Rust command in `src-tauri/src/commands/`
4. Update [DATA_SOURCES.md](./DATA_SOURCES.md)

### Adding Analytics Modules
1. Place in appropriate `Analytics/` subdirectory
2. Follow CFA curriculum structure
3. Include docstrings and type hints
4. Update [ANALYTICS.md](./ANALYTICS.md)

### Adding AI Agents
1. Add to `agents/` with appropriate subdirectory
2. Use FinAgent core framework
3. Define persona and strategy
4. Update [AGENTS.md](./AGENTS.md)

## Technical Requirements

- **Python Version**: 3.11+
- **Execution**: Embedded Python runtime bundled with app
- **IPC**: Tauri command bridge (Rust ↔ Python)
- **Output Format**: JSON responses
- **Error Handling**: Structured error objects

## Project Context

Part of **Fincept Terminal** - a Fincept financial intelligence platform built with:
- **Frontend**: React 19 + TypeScript + Tauri
- **Backend**: Rust (Tauri 2.x)
- **Analytics**: Python (embedded runtime)
- **AI**: Ollama (local LLM), Langchain

## Documentation

- **Root CLAUDE.md**: `../../CLAUDE.md`
- **App CLAUDE.md**: `../../../CLAUDE.md`
- **Architecture**: `../../../../docs/ARCHITECTURE.md`
- **Python Contributor Guide**: `../../../../docs/PYTHON_CONTRIBUTOR_GUIDE.md`

## Performance Notes

- Scripts execute synchronously via Rust `std::process::Command`
- Large datasets should stream or paginate results
- Cache frequently accessed data when possible
- Use async/await patterns in frontend for better UX

## License

MIT License - Part of Fincept Terminal

---

**Last Updated**: 2026-01-23
**Python Scripts**: 250+
**Data Sources**: 60+
**Analytics Modules**: 15+
**AI Agents**: 30+
