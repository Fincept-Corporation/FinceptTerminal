# Broker Integration Guide

Guide for integrating new brokers into Fincept Terminal with paper trading support.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    MARKET DATA LAYER                            │
│  Rust WebSocket (useRustTicker, useRustOrderBook, useRustTrades)│
│  Feeds into UnifiedMarketDataService                            │
└─────────────────────────────────┬───────────────────────────────┘
                                  │
              ┌───────────────────┴───────────────────┐
              ▼                                       ▼
┌─────────────────────────────┐       ┌─────────────────────────────┐
│     PAPER TRADING           │       │     LIVE TRADING            │
│     PaperTradingAdapter     │       │     BrokerAdapter (CCXT)    │
│     - Uses MarketDataService│       │     - Direct exchange API   │
│     - Simulated orders      │       │     - Real order execution  │
│     - Rust backend storage  │       │                             │
└─────────────────────────────┘       └─────────────────────────────┘
```

## Key Principle: Separation of Concerns

- **Market Data**: Always from real exchange (WebSocket + REST)
- **Paper Trading**: Only simulates orders/positions, NOT market data
- **`realAdapter`**: Used for market data in BOTH live and paper modes
- **`activeAdapter`**: Paper adapter in paper mode, real adapter in live mode

## Step 1: Create Broker Adapter

Create `src/stockBrokers/adapters/{broker}Adapter.ts`:

```typescript
import { IExchangeAdapter, BrokerConfig } from '../types';

export class MyBrokerAdapter implements IExchangeAdapter {
  private exchange: any; // CCXT instance or custom client
  private connected = false;
  private config: BrokerConfig;

  constructor(config: BrokerConfig) {
    this.config = config;
  }

  async connect(): Promise<void> {
    // Initialize exchange connection
    this.exchange = new ccxt.mybroker({
      apiKey: this.config.apiKey,
      secret: this.config.apiSecret,
    });
    await this.exchange.loadMarkets();
    this.connected = true;
  }

  async disconnect(): Promise<void> {
    this.connected = false;
  }

  isConnected(): boolean {
    return this.connected;
  }

  // Required methods
  async fetchTicker(symbol: string): Promise<Ticker> { /* ... */ }
  async fetchOrderBook(symbol: string, limit?: number): Promise<OrderBook> { /* ... */ }
  async fetchOHLCV(symbol: string, timeframe: string, limit?: number): Promise<OHLCV[]> { /* ... */ }
  async fetchBalance(): Promise<Balance> { /* ... */ }
  async fetchPositions(symbols?: string[]): Promise<Position[]> { /* ... */ }
  async fetchOpenOrders(symbol?: string): Promise<Order[]> { /* ... */ }
  async createOrder(symbol: string, type: string, side: string, amount: number, price?: number, params?: any): Promise<Order> { /* ... */ }
  async cancelOrder(orderId: string, symbol?: string): Promise<void> { /* ... */ }
}
```

## Step 2: Register in Broker Registry

Update `src/stockBrokers/registry.ts`:

```typescript
import { MyBrokerAdapter } from './adapters/myBrokerAdapter';

export const BROKER_REGISTRY: BrokerMetadata[] = [
  // ... existing brokers
  {
    id: 'mybroker',
    name: 'My Broker',
    type: 'crypto', // or 'stock', 'forex'
    features: ['spot', 'margin', 'futures'],
    requiredCredentials: ['apiKey', 'apiSecret'],
    testnet: true,
    defaultSymbols: ['BTC/USD', 'ETH/USD'],
  },
];

export function createAdapter(brokerId: string, config: BrokerConfig): IExchangeAdapter {
  switch (brokerId) {
    case 'mybroker':
      return new MyBrokerAdapter(config);
    // ... other brokers
  }
}
```

## Step 3: Add WebSocket Support (Rust Backend)

Add provider config in `src-tauri/src/websocket/providers/`:

```rust
// mybroker.rs
pub fn get_mybroker_config() -> ProviderConfig {
    ProviderConfig {
        name: "mybroker".to_string(),
        url: "wss://ws.mybroker.com".to_string(),
        // ... config
    }
}
```

## Step 4: Paper Trading Integration

Paper trading works automatically if your adapter implements `IExchangeAdapter`. The system:

1. Creates `PaperTradingAdapter` wrapping market data service
2. Uses `realAdapter` for all market data (ticker, OHLCV, orderbook)
3. Simulates orders/positions in Rust backend

### How Paper Trading Gets Prices

```typescript
// In TradingTab.tsx - feeds WebSocket data to market data service
useEffect(() => {
  if (tickerData && activeBroker) {
    const marketDataService = getMarketDataService();
    marketDataService.updatePrice(activeBroker, selectedSymbol, {
      price: tickerData.price,
      bid: tickerData.bid,
      ask: tickerData.ask,
      // ...
    });
  }
}, [tickerData]);
```

### Paper Order Matching (Touch-Based)

Orders fill when price touches limit:
- **Buy Limit**: Fills when `ask <= limit_price`
- **Sell Limit**: Fills when `bid >= limit_price`
- **Buy Stop**: Fills when `price >= stop_price`
- **Sell Stop**: Fills when `price <= stop_price`

## Step 5: Market Data Hooks

The hooks in `src/components/tabs/trading/hooks/useMarketData.ts` use `realAdapter`:

```typescript
// useOHLCV - fetches chart data
const { realAdapter } = useBrokerContext();
const rawData = await realAdapter.fetchOHLCV(symbol, timeframe, limit);

// useTicker - gets live prices from MarketDataService
const marketDataService = getMarketDataService();
const cachedPrice = marketDataService.getCurrentPrice(symbol, activeBroker);

// useOrderBook - fetches depth from realAdapter
const rawBook = await realAdapter.fetchOrderBook(symbol, limit);
```

## Key Files Reference

| File | Purpose |
|------|---------|
| `src/stockBrokers/adapters/` | Broker adapter implementations |
| `src/stockBrokers/registry.ts` | Broker registration and metadata |
| `src/contexts/BrokerContext.tsx` | Adapter lifecycle, mode switching |
| `src/paper-trading/index.ts` | PaperTradingAdapter implementation |
| `src/paper-trading/OrderMatcher.ts` | Order fill logic |
| `src/services/trading/UnifiedMarketDataService.ts` | Price cache and subscriptions |
| `src/hooks/useRustWebSocket.ts` | WebSocket data hooks |
| `src/components/tabs/trading/hooks/useMarketData.ts` | Market data hooks (OHLCV, ticker, orderbook) |

## IExchangeAdapter Interface

```typescript
interface IExchangeAdapter {
  connect(): Promise<void>;
  disconnect(): Promise<void>;
  isConnected(): boolean;

  // Market Data
  fetchTicker(symbol: string): Promise<Ticker>;
  fetchOrderBook(symbol: string, limit?: number): Promise<OrderBook>;
  fetchOHLCV(symbol: string, timeframe: string, limit?: number): Promise<OHLCV[]>;
  fetchTickers(symbols?: string[]): Promise<Record<string, Ticker>>;
  fetchMarkets(): Promise<Market[]>;

  // Account
  fetchBalance(): Promise<Balance>;
  fetchPositions(symbols?: string[]): Promise<Position[]>;
  fetchOpenOrders(symbol?: string): Promise<Order[]>;
  fetchClosedOrders(symbol?: string, since?: number, limit?: number): Promise<Order[]>;

  // Trading
  createOrder(symbol: string, type: string, side: string, amount: number, price?: number, params?: any): Promise<Order>;
  cancelOrder(orderId: string, symbol?: string): Promise<void>;
  cancelAllOrders(symbol?: string): Promise<void>;
}
```

## Testing Checklist

- [ ] Adapter connects successfully with API credentials
- [ ] `fetchTicker` returns valid price data
- [ ] `fetchOHLCV` returns chart data (100+ candles)
- [ ] `fetchOrderBook` returns 20+ levels for bids/asks
- [ ] `fetchBalance` returns account balances
- [ ] `createOrder` places orders correctly
- [ ] Paper trading shows live P&L with your broker's prices
- [ ] WebSocket streams real-time ticker data
- [ ] Symbol format matches (e.g., `BTC/USD` vs `BTCUSD`)

## Common Issues

1. **Empty orderbook in paper mode**: Ensure WebSocket is connected and streaming
2. **Chart not loading**: Check `realAdapter.fetchOHLCV()` returns data
3. **Prices showing 0**: Verify `marketDataService.updatePrice()` is being called
4. **Symbol mismatch**: Normalize symbols (remove `/`, `-`, uppercase)
