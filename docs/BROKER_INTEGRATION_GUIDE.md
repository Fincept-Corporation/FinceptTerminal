# Indian Stock Broker Integration Guide

Guide for integrating new Indian stock brokers (Zerodha, Fyers, Angel, Dhan, Upstox, etc.) into Fincept Terminal.

## Architecture Overview

```
src/brokers/stocks/
├── types.ts                    # IStockBrokerAdapter interface
├── BaseStockBrokerAdapter.ts   # Abstract base class
├── registry.ts                 # Broker metadata & registration
├── init.ts                     # Adapter initialization at startup
└── india/
    ├── fyers/
    │   ├── FyersAdapter.ts     # ~1287 lines
    │   ├── constants.ts
    │   └── mapper.ts
    └── zerodha/
        ├── ZerodhaAdapter.ts   # ~1295 lines
        ├── constants.ts
        └── mapper.ts
```

## Step 1: Add Broker Metadata

Update `src/brokers/stocks/registry.ts`:

```typescript
mybroker: {
  id: 'mybroker',
  name: 'mybroker',
  displayName: 'My Broker',
  website: 'https://mybroker.com',
  region: 'india',
  country: 'IN',
  currency: 'INR',
  exchanges: ['NSE', 'BSE', 'NFO', 'MCX'],
  marketHours: { open: '09:15', close: '15:30', timezone: 'Asia/Kolkata' },
  features: {
    webSocket: true, amo: true, gtt: false, bracketOrder: true,
    coverOrder: true, marginCalculator: false, optionsChain: true, paperTrading: false,
  },
  tradingFeatures: {
    marketOrders: true, limitOrders: true, stopOrders: true,
    stopLimitOrders: true, trailingStopOrders: false,
  },
  productTypes: ['CASH', 'INTRADAY', 'MARGIN'],
  authType: 'oauth',  // 'oauth' | 'totp' | 'password'
  rateLimit: { ordersPerSecond: 10, quotesPerSecond: 5 },
  defaultSymbols: ['RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK'],
},
```

## Step 2: Create Adapter

Create `src/brokers/stocks/india/mybroker/MyBrokerAdapter.ts`:

```typescript
import { BaseStockBrokerAdapter } from '../../BaseStockBrokerAdapter';
import type { StockBrokerMetadata, BrokerCredentials, AuthResponse, OrderParams, OrderResponse,
  ModifyOrderParams, Order, Trade, Position, Holding, Funds, Quote, OHLCV, MarketDepth,
  TimeFrame, StockExchange, Instrument } from '../../types';
import { STOCK_BROKER_REGISTRY } from '../../registry';

export class MyBrokerAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'mybroker';
  readonly brokerName = 'My Broker';
  readonly region = 'india' as const;
  readonly metadata: StockBrokerMetadata = STOCK_BROKER_REGISTRY.mybroker;

  private apiKey = '';
  private apiSecret = '';
  private readonly BASE_URL = 'https://api.mybroker.com/v1';

  // === AUTHENTICATION ===
  setCredentials(apiKey: string, apiSecret: string): void {
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
  }

  getAuthUrl(): string {
    return `https://api.mybroker.com/authorize?client_id=${this.apiKey}&redirect_uri=${encodeURIComponent('http://127.0.0.1/')}&response_type=code`;
  }

  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    const res = await fetch(`${this.BASE_URL}/token`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        grant_type: 'authorization_code',
        client_id: this.apiKey,
        client_secret: this.apiSecret,
        code: credentials.accessToken,
      }),
    });
    const data = await res.json();
    if (data.access_token) {
      this.accessToken = data.access_token;
      this._isConnected = true;
      await this.storeCredentials({ apiKey: this.apiKey, apiSecret: this.apiSecret, accessToken: data.access_token });
      return { success: true, accessToken: data.access_token };
    }
    return { success: false, message: data.message || 'Auth failed' };
  }

  // === REQUIRED INTERNAL IMPLEMENTATIONS ===
  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    const res = await this.apiCall('POST', '/orders', { symbol: params.symbol, exchange: params.exchange,
      side: params.side, qty: params.quantity, type: params.orderType, product: params.productType, price: params.price });
    return { success: res.status === 'ok', orderId: res.data?.order_id, message: res.message };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const res = await this.apiCall('PUT', `/orders/${params.orderId}`, params);
    return { success: res.status === 'ok', orderId: params.orderId };
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    const res = await this.apiCall('DELETE', `/orders/${orderId}`);
    return { success: res.status === 'ok', orderId };
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    const res = await this.apiCall('GET', '/orders');
    return res.data?.orders?.map((o: any) => this.mapOrder(o)) || [];
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    const res = await this.apiCall('GET', '/trades');
    return res.data?.trades?.map((t: any) => this.mapTrade(t)) || [];
  }

  protected async getPositionsInternal(): Promise<Position[]> {
    const res = await this.apiCall('GET', '/positions');
    return res.data?.positions?.map((p: any) => this.mapPosition(p)) || [];
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const res = await this.apiCall('GET', '/holdings');
    return res.data?.holdings?.map((h: any) => this.mapHolding(h)) || [];
  }

  protected async getFundsInternal(): Promise<Funds> {
    const res = await this.apiCall('GET', '/funds');
    return { availableCash: res.data.available, usedMargin: res.data.used, availableMargin: res.data.available,
      totalBalance: res.data.total, currency: 'INR' };
  }

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    const res = await this.apiCall('GET', `/quote?symbol=${symbol}&exchange=${exchange}`);
    return res.data;
  }

  protected async getQuotesInternal(instruments: Array<{ symbol: string; exchange: StockExchange }>): Promise<Quote[]> {
    const symbols = instruments.map(i => `${i.exchange}:${i.symbol}`).join(',');
    const res = await this.apiCall('GET', `/quotes?symbols=${symbols}`);
    return res.data || [];
  }

  protected async getOHLCVInternal(symbol: string, exchange: StockExchange, tf: TimeFrame, from: Date, to: Date): Promise<OHLCV[]> {
    const res = await this.apiCall('GET', `/historical?symbol=${symbol}&exchange=${exchange}&tf=${tf}&from=${from.getTime()}&to=${to.getTime()}`);
    return res.data?.candles || [];
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    const res = await this.apiCall('GET', `/depth?symbol=${symbol}&exchange=${exchange}`);
    return { bids: res.data.bids, asks: res.data.asks };
  }

  protected async searchSymbolsInternal(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    const res = await this.apiCall('GET', `/search?q=${query}${exchange ? `&exchange=${exchange}` : ''}`);
    return res.data || [];
  }

  // === HELPERS ===
  private async apiCall(method: string, endpoint: string, body?: any): Promise<any> {
    const res = await fetch(`${this.BASE_URL}${endpoint}`, {
      method, headers: { 'Authorization': `Bearer ${this.accessToken}`, 'Content-Type': 'application/json' },
      body: body ? JSON.stringify(body) : undefined,
    });
    return res.json();
  }

  private mapOrder(o: any): Order {
    return { orderId: o.order_id, symbol: o.symbol, exchange: o.exchange, side: o.side, quantity: o.qty,
      filledQuantity: o.filled_qty, pendingQuantity: o.pending_qty, price: o.price, averagePrice: o.avg_price,
      orderType: o.type, productType: o.product, validity: 'DAY', status: o.status, placedAt: new Date(o.placed_at) };
  }
  private mapTrade(t: any): Trade {
    return { tradeId: t.trade_id, orderId: t.order_id, symbol: t.symbol, exchange: t.exchange, side: t.side,
      quantity: t.qty, price: t.price, productType: t.product, tradedAt: new Date(t.traded_at) };
  }
  private mapPosition(p: any): Position {
    return { symbol: p.symbol, exchange: p.exchange, productType: p.product, quantity: p.qty, buyQuantity: p.buy_qty,
      sellQuantity: p.sell_qty, buyValue: p.buy_value, sellValue: p.sell_value, averagePrice: p.avg_price,
      lastPrice: p.ltp, pnl: p.pnl, pnlPercent: p.pnl_percent, dayPnl: p.day_pnl };
  }
  private mapHolding(h: any): Holding {
    return { symbol: h.symbol, exchange: h.exchange, quantity: h.qty, averagePrice: h.avg_price, lastPrice: h.ltp,
      investedValue: h.invested, currentValue: h.current, pnl: h.pnl, pnlPercent: h.pnl_percent };
  }
}
```

## Step 3: Register Adapter

Update `src/brokers/stocks/init.ts`:

```typescript
import { MyBrokerAdapter } from './india/mybroker/MyBrokerAdapter';
registerBrokerAdapter('mybroker', MyBrokerAdapter);
```

## Paper Trading

Paper trading is separate from adapters - handled by `StockBrokerContext.tsx` which routes to:
- **Live mode**: `adapter.getPositions()`, `adapter.getFunds()`, etc.
- **Paper mode**: `unifiedTradingService.ts` → Rust backend (SQLite)

## Key Files

| File | Purpose |
|------|---------|
| `src/brokers/stocks/types.ts` | `IStockBrokerAdapter` interface |
| `src/brokers/stocks/BaseStockBrokerAdapter.ts` | Abstract base class |
| `src/brokers/stocks/registry.ts` | Broker metadata |
| `src/contexts/StockBrokerContext.tsx` | State management, paper/live routing |
| `src/services/unifiedTradingService.ts` | Paper trading backend bridge |

## Testing Checklist

- [ ] Metadata added to `registry.ts`
- [ ] Adapter registered in `init.ts`
- [ ] OAuth: `getAuthUrl()` → `authenticate()` flow works
- [ ] Credentials persist via `storeCredentials()`/`loadCredentials()`
- [ ] `getPositions()`, `getHoldings()`, `getOrders()`, `getFunds()` return correct data
- [ ] `placeOrder()` executes successfully
- [ ] Paper mode shows paper data, Live mode shows broker data

---
**Last Updated**: January 2025
