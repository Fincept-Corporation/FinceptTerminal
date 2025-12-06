## Exchange Adapters (CCXT)

Professional-grade exchange integrations using CCXT for Fincept Terminal. Bloomberg-quality infrastructure for trading, paper trading, and agentic systems.

## Architecture

```
src/brokers/exchanges/
├── types.ts                  # Unified type definitions
├── BaseExchangeAdapter.ts    # Abstract base class
├── KrakenAdapter.ts          # Kraken implementation
├── index.ts                  # Registry & factory
└── README.md                 # This file
```

## Features

### Core Capabilities
- ✅ **Unified Interface**: Consistent API across all exchanges
- ✅ **Type-Safe**: Full TypeScript support
- ✅ **Error Handling**: Professional error types & recovery
- ✅ **Event System**: Real-time event notifications
- ✅ **Rate Limiting**: Built-in rate limit management
- ✅ **Authentication**: Secure credential handling

### Trading Features
- ✅ Market, Limit, Stop, Stop-Limit orders
- ✅ Trailing stops, Iceberg orders
- ✅ Margin trading with leverage
- ✅ Position management
- ✅ Balance & portfolio tracking

### Market Data
- ✅ Real-time tickers
- ✅ Order book depth
- ✅ Trade history
- ✅ OHLCV candles (1m to 1w)
- ✅ Historical data

### Advanced Features (Kraken)
- ✅ Staking & Earn
- ✅ Futures trading
- ✅ Margin trading
- ✅ Fund transfers
- ✅ Deposit/Withdrawal

## Quick Start

### Basic Usage

```typescript
import { createExchangeAdapter } from '@/brokers/exchanges';

// Create exchange instance
const kraken = createExchangeAdapter('kraken', {
  enableRateLimit: true,
  sandbox: false, // Use testnet
});

// Connect to exchange
await kraken.connect();

// Fetch market data
const ticker = await kraken.fetchTicker('BTC/USD');
const orderBook = await kraken.fetchOrderBook('BTC/USD', 25);
const trades = await kraken.fetchTrades('BTC/USD', 100);
```

### Authentication & Trading

```typescript
import { createExchangeAdapter } from '@/brokers/exchanges';

const kraken = createExchangeAdapter('kraken');

// Authenticate
await kraken.authenticate({
  apiKey: 'YOUR_API_KEY',
  secret: 'YOUR_SECRET',
});

// Check balance
const balance = await kraken.fetchBalance();
console.log('BTC Balance:', balance.BTC?.free);

// Place order
const order = await kraken.createOrder(
  'BTC/USD',     // symbol
  'limit',       // type
  'buy',         // side
  0.001,         // amount
  50000          // price
);

console.log('Order placed:', order.id);
```

### Advanced Orders (Kraken)

```typescript
import { KrakenAdapter } from '@/brokers/exchanges';

const kraken = new KrakenAdapter();
await kraken.authenticate({ ... });

// Stop-loss order
await kraken.createStopLossOrder(
  'BTC/USD',
  'sell',
  0.001,
  48000 // stop price
);

// Take-profit order
await kraken.createTakeProfitOrder(
  'BTC/USD',
  'sell',
  0.001,
  52000 // take profit price
);

// Trailing stop
await kraken.createTrailingStopOrder(
  'BTC/USD',
  'sell',
  0.001,
  2 // 2% trailing
);

// Iceberg order (hidden liquidity)
await kraken.createIcebergOrder(
  'BTC/USD',
  'buy',
  1.0,       // total amount
  50000,     // price
  0.1        // visible amount
);
```

### Margin Trading

```typescript
// Set leverage
await kraken.setLeverage('BTC/USD', 5);

// Place margin order with leverage
await kraken.createMarginOrder(
  'BTC/USD',
  'limit',
  'buy',
  0.01,
  50000,
  5 // 5x leverage
);

// Check margin balance
const marginBalance = await kraken.fetchMarginBalance();
```

### Staking (Kraken-specific)

```typescript
// View staking options
const stakingAssets = await kraken.fetchStakingAssets();

// Stake ETH
await kraken.stakeAsset('ETH', 1.0, 'ethereum-staked');

// Unstake
await kraken.unstakeAsset('ETH', 0.5);
```

### Historical Data

```typescript
// Fetch OHLCV candles
const candles = await kraken.fetchHistoricalOHLCV(
  'BTC/USD',
  '1h',      // timeframe
  undefined, // since (optional)
  720        // limit (30 days of hourly data)
);

candles.forEach(([time, open, high, low, close, volume]) => {
  console.log({ time, open, high, low, close, volume });
});
```

### Event Handling

```typescript
// Subscribe to events
kraken.on('connected', (data) => {
  console.log('Connected to', data.exchange);
});

kraken.on('error', (data) => {
  console.error('Exchange error:', data.data);
});

kraken.on('authenticated', (data) => {
  console.log('Authenticated successfully');
});

// Connect
await kraken.connect();
```

### Error Handling

```typescript
import {
  ExchangeError,
  AuthenticationError,
  RateLimitError,
  InsufficientFunds,
} from '@/brokers/exchanges';

try {
  await kraken.createOrder('BTC/USD', 'limit', 'buy', 10, 50000);
} catch (error) {
  if (error instanceof AuthenticationError) {
    console.error('Auth failed:', error.message);
  } else if (error instanceof RateLimitError) {
    console.error('Rate limit exceeded, retry after delay');
  } else if (error instanceof InsufficientFunds) {
    console.error('Not enough funds');
  } else if (error instanceof ExchangeError) {
    console.error('Exchange error:', error.code, error.message);
  }
}
```

## Exchange Capabilities

```typescript
// Check what exchange supports
const capabilities = kraken.getCapabilities();

console.log('Spot trading:', capabilities.spot);
console.log('Margin trading:', capabilities.margin);
console.log('Futures trading:', capabilities.futures);
console.log('Supported order types:', capabilities.supportedOrderTypes);
console.log('WebSocket support:', capabilities.websocketSupport);
```

## Paper Trading Integration

```typescript
import { createExchangeAdapter } from '@/brokers/exchanges';
import { PaperTradingEngine } from '@/services/paperTrading';

// Create exchange adapter for market data
const kraken = createExchangeAdapter('kraken');
await kraken.connect();

// Create paper trading engine
const paperTrading = new PaperTradingEngine({
  exchange: kraken,
  initialBalance: 100000,
  portfolio: 'my-paper-portfolio',
});

// Use paper trading engine instead of real exchange
const order = await paperTrading.createOrder(
  'BTC/USD',
  'limit',
  'buy',
  0.01,
  50000
);
```

## Agentic Trading Integration

```typescript
import { createExchangeAdapter } from '@/brokers/exchanges';
import { TradingAgent } from '@/services/agenticTrading';

const kraken = createExchangeAdapter('kraken');
await kraken.authenticate({ ... });

// Create AI trading agent
const agent = new TradingAgent({
  exchange: kraken,
  strategy: 'momentum',
  riskLevel: 'medium',
  maxPositionSize: 0.1,
});

// Start autonomous trading
await agent.start();
```

## Adding New Exchanges

To add support for another exchange:

### 1. Create Adapter Class

```typescript
// src/brokers/exchanges/BinanceAdapter.ts
import { BaseExchangeAdapter } from './BaseExchangeAdapter';
import { ExchangeConfig } from './types';

export class BinanceAdapter extends BaseExchangeAdapter {
  constructor(config?: Partial<ExchangeConfig>) {
    super({
      exchange: 'binance',
      ...config,
    });
  }

  // Add Binance-specific methods here
}
```

### 2. Register in Index

```typescript
// src/brokers/exchanges/index.ts
import { BinanceAdapter } from './BinanceAdapter';

export const SUPPORTED_EXCHANGES = {
  kraken: { ... },
  binance: {
    id: 'binance',
    name: 'Binance',
    adapter: BinanceAdapter,
    type: 'centralized',
    assetClass: 'crypto',
    region: 'global',
  },
};
```

### 3. Use It

```typescript
const binance = createExchangeAdapter('binance');
await binance.connect();
```

## Supported Exchanges

Currently implemented:
- ✅ **Kraken** - Full implementation with advanced features

Coming soon (CCXT supports 100+ exchanges):
- 🔄 Binance
- 🔄 Coinbase
- 🔄 Bybit
- 🔄 OKX
- 🔄 Bitfinex
- 🔄 And many more...

## Performance Tips

1. **Enable Rate Limiting**: Always use `enableRateLimit: true`
2. **Reuse Connections**: Create one adapter instance and reuse it
3. **Batch Requests**: Use `fetchTickers()` for multiple symbols
4. **Use WebSocket**: For real-time data (coming soon with ccxt.pro)
5. **Cache Markets**: Call `loadMarkets()` once and cache

## Testing

```typescript
// Use sandbox mode for testing
const krakenTest = createExchangeAdapter('kraken', {
  sandbox: true,
  credentials: {
    apiKey: 'test_key',
    secret: 'test_secret',
  },
});

await krakenTest.connect();
// All operations now run on testnet
```

## Security Best Practices

1. **Never commit API keys** - Use environment variables
2. **Use read-only keys** for market data
3. **Restrict IP addresses** in exchange API settings
4. **Enable 2FA** on exchange accounts
5. **Monitor API usage** and set alerts
6. **Use separate keys** for paper vs live trading

## Resources

- **CCXT Documentation**: https://docs.ccxt.com/
- **Kraken API Docs**: https://docs.kraken.com/
- **CCXT GitHub**: https://github.com/ccxt/ccxt
- **Fincept Terminal Docs**: [Internal documentation]

---

**Version**: 1.0.0
**Last Updated**: 2025-01-29
**Maintained by**: Fincept Corporation
