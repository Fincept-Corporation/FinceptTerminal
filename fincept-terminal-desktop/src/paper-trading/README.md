# Universal Paper Trading Module

**Production-grade paper trading system for ANY asset class and broker.**

This standalone module provides realistic trading simulation with professional-grade accuracy, suitable for algorithmic trading development, strategy backtesting, and trader education.

## 🚀 Supported Markets
- ✅ **Crypto** (Kraken, HyperLiquid, Binance, Coinbase, etc.)
- ✅ **Stocks** (Zerodha Kite, Fyers, Interactive Brokers, etc.)
- ✅ **Forex** (any FX broker)
- ✅ **Commodities** (any commodity broker)
- ✅ **Options & Futures** (any derivatives broker)

---

## ✨ Features

### Core Trading Capabilities
- ✅ **Universal Adapter**: Works with ANY broker via unified CCXT-compatible interface
- ✅ **Real-Time Market Data**: Uses live prices from actual exchanges via WebSocket or REST
- ✅ **Complete Order Types**:
  - Market, Limit, Stop, Stop-Limit
  - **Trailing Stop** (with dynamic adjustment) 🆕
  - Iceberg, Post-Only, Reduce-Only
  - IOC, FOK, GTC time-in-force
- ✅ **Margin Trading**: Full spot & leveraged trading (up to 50x leverage)
- ✅ **Position Management**: Long/short positions with fee-inclusive VWAP averaging 🆕
- ✅ **Realistic Execution**:
  - **Size-Dependent Slippage** (larger orders = more slippage) 🆕
  - **Volatility-Adjusted Slippage** (dynamic based on market conditions) 🆕
  - Configurable maker/taker fees
- ✅ **Risk Management**:
  - Margin requirements with **fee-inclusive liquidation prices** 🆕
  - Automatic liquidation simulation
  - Cross and isolated margin modes
- ✅ **Thread-Safe Concurrency**: Transaction locks prevent race conditions 🆕
- ✅ **Database Persistence**: SQLite for reliability across sessions
- ✅ **Event System**: Real-time events for UI updates

### Advanced Analytics 🆕
- ✅ **Win Rate**: Accurate calculation based on closed positions
- ✅ **Sharpe Ratio**: Risk-adjusted returns
- ✅ **Maximum Drawdown**: Peak-to-trough analysis
- ✅ **Profit Factor**: Gross profits / gross losses
- ✅ **Expectancy**: Average P&L per trade
- ✅ **Kelly Criterion**: Optimal position sizing recommendation
- ✅ **Average Holding Period**: Time-weighted metrics

---

## 🎯 Quick Start

### Basic Usage

```typescript
import { createPaperTradingAdapter } from '@/paper-trading';
import { createExchangeAdapter } from '@/brokers/crypto';

// Create real exchange adapter for market data
const realAdapter = createExchangeAdapter('kraken');
await realAdapter.connect();

// Create paper trading adapter
const paperAdapter = createPaperTradingAdapter(
  {
    portfolioId: 'my-portfolio',
    portfolioName: 'BTC Momentum Strategy',
    provider: 'kraken',
    assetClass: 'crypto', // Enables crypto-optimized settings
    initialBalance: 100000,
    currency: 'USD',
    fees: { maker: 0.0002, taker: 0.0005 },
    slippage: {
      market: 0.001,
      limit: 0,
      modelType: 'volatility-adjusted', // 🆕 Advanced slippage
      sizeImpactFactor: 0.0001,
      volatilityMultiplier: 2.0
    },
    defaultLeverage: 1,
    marginMode: 'cross',
    enableRealtimeUpdates: true,
  },
  realAdapter
);

await paperAdapter.connect();

// Place orders
const order = await paperAdapter.createOrder('BTC/USD', 'market', 'buy', 0.1);

// Get performance statistics 🆕
const stats = await paperAdapter.getStatistics();
console.log('Win Rate:', stats.winRate, '%');
console.log('Sharpe Ratio:', stats.sharpeRatio);
console.log('Max Drawdown:', stats.maxDrawdown, '%');
console.log('Profit Factor:', stats.profitFactor);
```

### Advanced: Trailing Stops 🆕

```typescript
// Trailing stop with 2% trail
const trailingStop = await paperAdapter.createTrailingStopOrder(
  'BTC/USD',
  'sell',
  0.1,
  2.0 // 2% trailing percentage
);

// Stop price will automatically adjust upward as price rises
// Triggers when price drops 2% from peak
```

### Advanced: Size-Dependent Slippage 🆕

```typescript
const config = {
  // ... other config
  slippage: {
    market: 0.001, // 0.1% base slippage
    limit: 0,
    modelType: 'size-dependent',
    sizeImpactFactor: 0.0001, // +0.01% per $10k order value
  },
};

// Small order: ~0.1% slippage
await adapter.createOrder('BTC/USD', 'market', 'buy', 0.01);

// Large order: ~0.3% slippage
await adapter.createOrder('BTC/USD', 'market', 'buy', 5.0);
```

---

## 📊 Configuration Options

```typescript
interface PaperTradingConfig {
  // Portfolio settings
  portfolioId: string;
  portfolioName: string;
  provider: string; // Any broker: 'kraken', 'binance', 'zerodha', etc.
  assetClass?: 'crypto' | 'stocks' | 'forex' | 'commodities'; // 🆕 Optimizes cache timing
  initialBalance: number;
  currency?: string; // Default: 'USD'

  // Execution settings
  fees: {
    maker: number; // e.g., 0.0002 (0.02%)
    taker: number; // e.g., 0.0005 (0.05%)
  };

  slippage: {
    market: number; // Base slippage (e.g., 0.001 = 0.1%)
    limit: number; // Usually 0
    modelType?: 'fixed' | 'size-dependent' | 'volatility-adjusted'; // 🆕
    sizeImpactFactor?: number; // 🆕 Additional slippage per unit size
    volatilityMultiplier?: number; // 🆕 Multiplier during high volatility
  };

  // Risk settings
  defaultLeverage?: number; // Default: 1
  marginMode?: 'cross' | 'isolated'; // Default: 'cross'
  maxPositionSize?: number;
  maxLeverage?: number;

  // Simulation settings
  simulatedLatency?: number; // ms delay (default: 0)
  enableRealtimeUpdates?: boolean; // Use WebSocket prices (default: true)
  priceUpdateInterval?: number; // Polling interval if WS unavailable (default: 200ms)
}
```

---

## 🔧 Production-Grade Improvements

### 1. Transaction Locking (Concurrency Safety) 🆕
Prevents race conditions when multiple orders execute simultaneously:
- Portfolio-level locks for balance operations
- Symbol-level locks for position updates
- Order-level locks for modifications
- Deadlock prevention with 5-second timeout

### 2. Atomic Order Editing 🆕
Edit orders safely without losing the original:
- Creates new order FIRST
- Only cancels original if new order succeeds
- Automatic rollback on failure

### 3. Optimized Price Caching 🆕
Asset class-specific cache freshness:
- **Crypto**: 200ms (fast-moving)
- **Forex**: 150ms (very fast)
- **Stocks**: 500ms (slower)
- **Commodities**: 1000ms (slowest)
- LRU eviction with 1000 symbol limit

### 4. Fee-Inclusive Liquidation Prices 🆕
Accurate liquidation calculations:
- Accounts for entry fees (already paid)
- Accounts for exit fees (will be charged)
- Accurate within 0.01% at all leverage levels

### 5. Fee-Inclusive VWAP 🆕
Position averaging includes all costs:
```typescript
newEntryPrice = (existingCost + newFillCost + fees) / totalQuantity
```

### 6. Advanced Statistics 🆕
Professional-grade performance metrics:
- Sharpe ratio, profit factor, expectancy
- Maximum drawdown with peak-to-trough analysis
- Kelly criterion for position sizing
- Average holding period

---

## 📈 API Reference

### Order Placement

```typescript
// Market order
await adapter.createOrder(symbol, 'market', side, amount);

// Limit order
await adapter.createOrder(symbol, 'limit', side, amount, price);

// Stop loss
await adapter.createStopLossOrder(symbol, side, amount, stopPrice);

// Trailing stop 🆕
await adapter.createTrailingStopOrder(symbol, side, amount, trailingPercent);

// Post-only limit
await adapter.createPostOnlyOrder(symbol, side, amount, price);

// Reduce-only (close position)
await adapter.createReduceOnlyOrder(symbol, 'market', side, amount);
```

### Position Management

```typescript
// Fetch positions
const positions = await adapter.fetchPositions();

// Fetch balance
const balance = await adapter.fetchBalance();

// Set leverage
await adapter.setLeverage(symbol, 10); // 10x leverage

// Set margin mode
await adapter.setMarginMode(symbol, 'isolated');
```

### Statistics 🆕

```typescript
const stats = await adapter.getStatistics();

// Returns:
{
  // P&L metrics
  totalPnL: number,
  realizedPnL: number,
  unrealizedPnL: number,
  returnPercent: string,

  // Trade metrics
  totalTrades: number,
  winningTrades: number,
  losingTrades: number,
  winRate: string, // %

  // Performance metrics
  averageWin: string,
  averageLoss: string,
  largestWin: string,
  largestLoss: string,
  profitFactor: string,
  riskRewardRatio: string,

  // Advanced metrics
  sharpeRatio: string | null,
  maxDrawdown: string | null, // %
  expectancy: string,
  kellyCriterion: string, // % of capital

  // Timing
  avgHoldingPeriod: string | null, // minutes

  // Fees
  totalFees: string
}
```

---

## 🧪 Testing

```typescript
// Test concurrent orders (thread safety)
await Promise.all([
  adapter.createOrder('BTC/USD', 'market', 'buy', 0.1),
  adapter.createOrder('BTC/USD', 'market', 'buy', 0.1),
  adapter.createOrder('BTC/USD', 'market', 'buy', 0.1),
]);

// Test trailing stop
const order = await adapter.createTrailingStopOrder('BTC/USD', 'sell', 0.1, 2.0);
// Verify stop price adjusts as market moves

// Test statistics
const stats = await adapter.getStatistics();
assert(stats.winRate >= 0 && stats.winRate <= 100);
assert(stats.profitFactor >= 0);
```

---

## 📚 Documentation

- `PRODUCTION_FIXES_SUMMARY.md` - Comprehensive list of all production fixes
- `TransactionLockManager.ts` - Concurrency control system
- `SlippageCalculator.ts` - Advanced slippage modeling
- `StatisticsCalculator.ts` - Performance analytics engine

---

## 🎯 Version History

### v3.1.4 (2026-01-10) - Current Release 🚀
**Latest stable release**
- Updated pricing system integration
- Removed free plan references
- Bug fixes and improvements

### v3.1.0 (2025-12-31) - Production-Hardened 🛡️
**All critical bugs fixed - Truly production-ready**
- ✅ **CRITICAL FIX**: TransactionLockManager lock release (was completely broken)
- ✅ **CRITICAL FIX**: Division by zero in SlippageCalculator
- ✅ **CRITICAL FIX**: Missing volatility updates in monitoring loop
- ✅ **HIGH FIX**: StatisticsCalculator Sharpe ratio edge cases
- ✅ **HIGH FIX**: PaperTradingBalance liquidation division by zero
- ✅ **MEDIUM FIX**: Resource cleanup on disconnect (memory leaks)
- 📄 Added comprehensive audit documentation

### v3.0.0 (2025-12-31) - Production Release 🚀
**⚠️ WARNING: Had critical bugs, use v3.1.4 or v3.1.0 instead**
- ✅ Transaction locking for thread safety (but broken release)
- ✅ Atomic order editing
- ✅ Optimized price caching (asset class-specific)
- ✅ Fee-inclusive liquidation prices
- ✅ Fee-inclusive VWAP averaging
- ✅ Trailing stop implementation
- ✅ Advanced slippage modeling
- ✅ Professional-grade statistics
- ✅ LRU cache with memory limits

### v2.0.0 (Previous)
- Basic paper trading functionality
- Simple order types
- Basic statistics

---

**Version**: 3.1.4 (Current Release)
**Location**: `src/paper-trading/`
**License**: MIT
**Status**: ✅ **PRODUCTION-READY** for real-time trading simulation

**Audit Report**: See `PRODUCTION_AUDIT_FIXES.md` for complete list of fixes

---

## 🤝 Contributing

See production fixes summary for areas of improvement.

## 📄 License

MIT - Free to use for any purpose
