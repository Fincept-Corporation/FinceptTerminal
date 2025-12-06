# Brokers Integration

Professional-grade broker and exchange integrations for Fincept Terminal. Bloomberg-quality infrastructure for trading, paper trading, and agentic systems.

## Structure

```
src/brokers/
├── index.ts              # Main exports and broker registry
├── exchanges/            # Crypto exchanges (CCXT-based)
│   ├── types.ts         # Unified type definitions
│   ├── BaseExchangeAdapter.ts  # Abstract base class
│   ├── KrakenAdapter.ts        # Kraken implementation
│   ├── index.ts         # Exchange registry & factory
│   └── README.md        # Detailed documentation
├── zerodha/             # Zerodha Kite (Indian broker)
│   ├── index.ts         # Zerodha exports and metadata
│   └── kiteAdapter.tsx  # Complete Kite Connect API adapter
└── README.md            # This file
```

## Supported Brokers

### 1. **Kraken** (Crypto Exchange - CEX) - CCXT
- **Region**: Global
- **Assets**: 200+ Cryptocurrencies
- **Features**: Spot, Margin, Futures, Staking, Earn
- **Technology**: CCXT unified API
- **WebSocket**: Real-time market data
- **Advanced Orders**: Stop-loss, Take-profit, Trailing stops, Iceberg
- **Files**: `src/brokers/exchanges/KrakenAdapter.ts`

### 2. **HyperLiquid** (Crypto DEX) - CCXT
- **Region**: Global (Arbitrum L2)
- **Assets**: Perpetual futures on major crypto assets
- **Features**: Spot, Perpetuals (up to 50x), Vaults, Subaccounts
- **Technology**: CCXT unified API
- **WebSocket**: Real-time market data
- **Special**: 4% fee discount (Builder Code), No market orders (simulated with limits)
- **Files**: `src/brokers/exchanges/HyperLiquidAdapter.ts`

### 3. **Zerodha Kite** (Stock Broker)
- **Region**: India
- **Assets**: Equity, F&O, Commodity, Currency, Mutual Funds
- **Features**: Trading, Portfolio, GTT, Alerts, Margin Calculator
- **API Version**: Kite Connect v3
- **Files**: `src/brokers/zerodha/`

## Adding New Brokers

To add a new broker integration:

1. Create a new directory: `src/brokers/{broker-name}/`
2. Implement the broker adapter with standard interface
3. Create `index.ts` with exports and metadata
4. Update `src/brokers/index.ts` to include the new broker
5. Add broker to `SUPPORTED_BROKERS` registry

## Usage

```typescript
import { SUPPORTED_BROKERS } from '@/brokers';
import { createKiteAdapter } from '@/brokers/zerodha';
import { KrakenService } from '@/brokers/kraken';

// Check available brokers
console.log(SUPPORTED_BROKERS);

// Initialize Zerodha
const kite = createKiteAdapter({
  apiKey: 'your_api_key',
  apiSecret: 'your_secret',
});

// Initialize Kraken
const kraken = new KrakenService(config);
```

## Migration Notes

This directory consolidates broker integrations previously scattered across:
- `src/stockBrokers/` (old)
- `src/services/stockBrokers/` (old)

All broker code is now unified under `src/brokers/` for better organization.
