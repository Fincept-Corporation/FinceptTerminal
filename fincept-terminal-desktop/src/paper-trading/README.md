# Universal Paper Trading Module

**Professional-grade paper trading system for ANY asset class and broker.**

This standalone module provides realistic trading simulation with support for:
- ✅ **Crypto** (Kraken, HyperLiquid, Binance, etc.)
- ✅ **Stocks** (Zerodha, Interactive Brokers, etc.)
- ✅ **Forex** (any FX broker)
- ✅ **Commodities** (any commodity broker)
- ✅ **Options, Futures** (any derivatives broker)

## Features

### Core Capabilities
- ✅ **Universal Adapter**: Works with ANY broker via unified interface
- ✅ **Real-Time Market Data**: Uses live prices from actual exchanges
- ✅ **All Order Types**: Market, Limit, Stop, Stop-Limit, Trailing Stop, Iceberg
- ✅ **Margin Trading**: Full spot & leveraged trading (up to 50x)
- ✅ **Position Management**: Long/short positions with real-time P&L
- ✅ **Realistic Execution**: Order matching based on real order books
- ✅ **Fees & Slippage**: Configurable maker/taker fees and slippage
- ✅ **Risk Management**: Margin requirements, liquidation simulation
- ✅ **Database Persistence**: SQLite for reliability across sessions
- ✅ **Event System**: Real-time events for UI updates

## Quick Start

```typescript
import { createPaperTradingAdapter } from '@/paper-trading';

const adapter = createPaperTradingAdapter({
  portfolioId: 'my-portfolio',
  portfolioName: 'Test Strategy',
  provider: 'kraken', // or any other broker
  initialBalance: 100000,
  fees: { maker: 0.0002, taker: 0.0005 },
  slippage: { market: 0.001, limit: 0 },
  realAdapter: realBrokerAdapter, // Actual broker adapter for market data
});

await adapter.connect();
await adapter.createOrder('BTC/USD', 'market', 'buy', 0.1);
```

See full documentation in the file for advanced usage, configuration, and examples.

---

**Version**: 2.0.0
**Location**: `src/paper-trading/`
**Universal**: Works with any broker (crypto, stocks, forex, commodities)
