# Broker Integration Guide

Complete checklist for integrating Indian stock brokers from OpenAlgo into Fincept Terminal.

## Pre-Integration Checklist

### 1. Study OpenAlgo Source (Local Reference Project)

**Local Path:** `C:\projects\featureimplement\openalgo\broker\{broker_name}\`

```
C:\projects\featureimplement\openalgo\broker\{broker_name}\
├── api\
│   ├── auth_api.py      # OAuth/TOTP flow, token exchange
│   ├── order_api.py     # Place/modify/cancel orders
│   ├── data.py          # Quotes, historical, depth
│   └── funds.py         # Margins, balances
├── database\
│   └── master_contract_db.py  # Symbol database, CSV parsing
├── mapping\
│   ├── order_data.py    # Order field mapping
│   └── transform_data.py # Response transformation
└── streaming\           # WebSocket implementation
```

**Extract from OpenAlgo:**
- [ ] API endpoints and headers format
- [ ] Auth flow (OAuth redirect URI, token exchange)
- [ ] Order/position/holding field mappings
- [ ] Master contract CSV URLs and parsing logic
- [ ] WebSocket connection and message format

## Implementation Checklist

### Phase 1: Rust Backend (`src-tauri/src/commands/brokers/`)

Create `{broker}.rs` with these commands:

| Command | Purpose | Priority |
|---------|---------|----------|
| `{broker}_exchange_token` | Exchange auth code for access token | P0 |
| `{broker}_validate_token` | Validate existing token | P0 |
| `{broker}_place_order` | Place new order | P0 |
| `{broker}_modify_order` | Modify existing order | P0 |
| `{broker}_cancel_order` | Cancel order | P0 |
| `{broker}_get_orders` | Fetch all orders | P0 |
| `{broker}_get_positions` | Fetch positions | P0 |
| `{broker}_get_holdings` | Fetch holdings | P0 |
| `{broker}_get_funds` | Fetch margins/balance | P0 |
| `{broker}_get_quotes` | Get LTP/quotes | P0 |
| `{broker}_get_history` | Historical OHLCV | P0 |
| `{broker}_get_depth` | Market depth L5 | P1 |
| `{broker}_get_trade_book` | Executed trades | P1 |
| `{broker}_download_master_contract` | Download symbol CSV | P0 |
| `{broker}_search_symbol` | Search in master contract | P0 |
| `{broker}_get_token_for_symbol` | Symbol → Token lookup | P0 |
| `{broker}_get_symbol_by_token` | Token → Symbol lookup | P1 |
| `{broker}_get_master_contract_metadata` | Last updated timestamp | P0 |

**Register in `lib.rs`:**
```rust
commands::brokers::{broker}_exchange_token,
commands::brokers::{broker}_place_order,
// ... all commands
```

### Phase 2: TypeScript Adapter (`src/brokers/stocks/india/{broker}/`)

**File Structure:**
```
{broker}/
├── {Broker}Adapter.ts   # Main adapter class
├── constants.ts         # API URLs, exchange maps, metadata
├── mapper.ts            # Response transformers
└── index.ts             # Export barrel
```

**Required Methods in Adapter:**

| Method | Invokes Rust Command | Notes |
|--------|---------------------|-------|
| `setCredentials()` | - | Store API key/secret in memory |
| `getAuthUrl()` | - | Return OAuth URL |
| `authenticate()` | `{broker}_exchange_token` | Handle OAuth callback |
| `initFromStorage()` | `get_indian_broker_credentials` | Restore session |
| `placeOrderInternal()` | `{broker}_place_order` | |
| `modifyOrderInternal()` | `{broker}_modify_order` | |
| `cancelOrderInternal()` | `{broker}_cancel_order` | |
| `getOrdersInternal()` | `{broker}_get_orders` | |
| `getTradeBookInternal()` | `{broker}_get_trade_book` | |
| `getPositionsInternal()` | `{broker}_get_positions` | |
| `getHoldingsInternal()` | `{broker}_get_holdings` | |
| `getFundsInternal()` | `{broker}_get_funds` | |
| `getQuoteInternal()` | `{broker}_get_quotes` | |
| `getOHLCVInternal()` | `{broker}_get_history` | |
| `getMarketDepthInternal()` | `{broker}_get_depth` | |
| `searchSymbols()` | `{broker}_search_symbol` | **Don't forget!** |
| `getInstrument()` | `{broker}_get_token_for_symbol` | **Don't forget!** |
| `downloadMasterContract()` | `{broker}_download_master_contract` | **Don't forget!** |
| `getMasterContractLastUpdated()` | `{broker}_get_master_contract_metadata` | **Don't forget!** |

### Phase 3: Registration

**`src/brokers/stocks/registry.ts`:**
```typescript
{broker}: {
  id: '{broker}',
  name: '{broker}',
  displayName: '{Broker Name}',
  region: 'india',
  country: 'IN',
  currency: 'INR',
  exchanges: ['NSE', 'BSE', 'NFO', 'MCX'],
  authType: 'oauth', // or 'totp'
  features: { webSocket: true, amo: true, gtt: false, ... },
  tradingFeatures: { marketOrders: true, limitOrders: true, ... },
  productTypes: ['CNC', 'INTRADAY', 'MARGIN'],
}
```

**`src/brokers/stocks/init.ts`:**
```typescript
import { {Broker}Adapter } from './india/{broker}/{Broker}Adapter';
registerBrokerAdapter('{broker}', {Broker}Adapter);
```

### Phase 4: UI Integration

**Files to update:**
- `BrokerConfigPanel.tsx` - Add broker to dropdown
- `BrokerAuthPanel.tsx` - Handle auth flow UI
- `BrokerSwitcher.tsx` - Add broker option

**No code changes needed if:**
- Adapter follows `IStockBrokerAdapter` interface
- Broker registered in `registry.ts` and `init.ts`

## Common Mistakes to Avoid

### ❌ Mistake 1: Missing Master Contract Methods
**Problem:** Fyers was missing `searchSymbols()`, `getInstrument()`, `downloadMasterContract()`, `getMasterContractLastUpdated()` while Rust backend had the commands.

**Fix:** Always implement ALL 4 master contract methods in TypeScript adapter.

### ❌ Mistake 2: Inconsistent Rust Command Naming
**Problem:** Mixed naming like `download_fyers_master_contract` vs `fyers_download_master_contract`.

**Fix:** Use consistent prefix: `{broker}_{action}` (e.g., `fyers_place_order`).

### ❌ Mistake 3: Not Registering Commands in lib.rs
**Problem:** Rust command exists but not callable from TypeScript.

**Fix:** Add every command to `tauri::generate_handler![]` in `lib.rs`.

### ❌ Mistake 4: Missing Error Handling in Adapter
**Problem:** API errors not properly caught and returned.

**Fix:** Always wrap invoke calls in try-catch, return fallback data on error.

### ❌ Mistake 5: Forgetting to Import `Instrument` Type
**Problem:** TypeScript error when adding `searchSymbols()`.

**Fix:** Add `Instrument` to imports from `../../types`.

### ❌ Mistake 6: Master Contract Not Persisted (Zerodha)
**Problem:** `downloadMasterContract` returns data but doesn't store to SQLite.

**Fix:** Persist to database like Fyers implementation.

### ❌ Mistake 7: Wrong ProductType Values in Metadata
**Problem:** Using broker-specific product types ('CNC', 'MIS', 'NRML') in `productTypes` array instead of unified types.

**Fix:** Always use unified ProductType values: `['CASH', 'INTRADAY', 'MARGIN']`. Map broker-specific codes in `mapper.ts`.

### ❌ Mistake 8: Wrong ProductType Return in Mapper
**Problem:** `fromBrokerProduct()` function returns broker-specific strings ('CNC', 'MIS') instead of unified ProductType.

**Fix:** Mapper must convert broker codes to unified types:
```typescript
// Broker 'D' (Delivery) → 'CASH' for equity, 'MARGIN' for F&O
// Broker 'I' (Intraday) → 'INTRADAY'
export function fromBrokerProduct(product: string, exchange: string): ProductType {
  if (product === 'D') {
    return ['NFO', 'BFO', 'MCX', 'CDS'].includes(exchange) ? 'MARGIN' : 'CASH';
  }
  return 'INTRADAY';
}
```

### ❌ Mistake 9: Wrong OrderStatus Values
**Problem:** Using 'COMPLETED' instead of 'FILLED' in status mapping.

**Fix:** Check `OrderStatus` type definition and use exact values:
```typescript
// Valid: 'PENDING' | 'OPEN' | 'PARTIALLY_FILLED' | 'FILLED' | 'CANCELLED' | 'REJECTED' | 'EXPIRED' | 'TRIGGER_PENDING'
// Wrong: 'COMPLETED' (doesn't exist)
```

### ❌ Mistake 10: Wrong Order Property Name
**Problem:** Using `message` instead of `statusMessage` in Order object.

**Fix:** Check the `Order` interface - it uses `statusMessage?: string`, not `message`.

## Verification Checklist

### Rust Backend
- [ ] All commands compile (`cargo check`)
- [ ] Commands registered in `lib.rs`
- [ ] Proper error handling with `ApiResponse<T>`
- [ ] Master contract stored in SQLite

### TypeScript Adapter
- [ ] Extends `BaseStockBrokerAdapter`
- [ ] All required internal methods implemented
- [ ] All 4 master contract methods implemented
- [ ] `Instrument` and `ProductType` types imported
- [ ] Mapper returns unified `ProductType` values (not broker-specific)
- [ ] Order status uses valid `OrderStatus` values (FILLED not COMPLETED)
- [ ] Order object uses `statusMessage` (not `message`)
- [ ] No TypeScript errors (`bun run build`)

### Registration
- [ ] Metadata in `registry.ts`
- [ ] Adapter registered in `init.ts`
- [ ] Singleton exported from adapter file

### UI
- [ ] Broker appears in BrokerSwitcher
- [ ] OAuth flow works (redirect → callback → token)
- [ ] Credentials persist across app restart
- [ ] Trading panel shows broker data

### End-to-End Testing
- [ ] Login flow completes
- [ ] Funds/margins display correctly
- [ ] Orders can be placed
- [ ] Positions/holdings load
- [ ] Historical charts work
- [ ] Symbol search works
- [ ] Master contract downloads

## Post-Implementation Cross-Check

**IMPORTANT:** After implementing a broker, perform this cross-check against the OpenAlgo source to verify correctness.

### Cross-Check Checklist

#### 1. API Endpoints Verification
- [ ] **Base URL** matches OpenAlgo `baseurl.py` or constants
- [ ] **Auth URL** (if separate) matches OpenAlgo `auth_api.py`
- [ ] **Order endpoints** (`/orders`, `/orders/{id}`) match OpenAlgo `order_api.py`
- [ ] **Portfolio endpoints** (`/positions`, `/holdings`, `/funds`) match OpenAlgo
- [ ] **Market data endpoints** (`/quotes`, `/history`, `/depth`) match OpenAlgo `data.py`

#### 2. Headers Verification
- [ ] **Auth header** name correct (e.g., `access-token`, `Authorization`, `X-Auth-Token`)
- [ ] **API key header** name correct (e.g., `api-key`, `client-id`, `appkey`)
- [ ] **Content-Type** matches OpenAlgo (usually `application/json`)
- [ ] Any **additional required headers** included

#### 3. Exchange Mappings Verification
Compare `constants.ts` EXCHANGE_MAP with OpenAlgo `transform_data.py`:
- [ ] NSE → (broker format)
- [ ] BSE → (broker format)
- [ ] NFO → (broker format)
- [ ] BFO → (broker format)
- [ ] MCX → (broker format)
- [ ] CDS → (broker format)
- [ ] BCD → (broker format) if applicable
- [ ] Index exchanges (NSE_INDEX, BSE_INDEX) if applicable

#### 4. Product Type Mappings Verification
Compare `constants.ts` PRODUCT_MAP with OpenAlgo `transform_data.py`:
- [ ] CASH/CNC → (broker format)
- [ ] INTRADAY/MIS → (broker format)
- [ ] MARGIN/NRML → (broker format)

#### 5. Order Type Mappings Verification
Compare `constants.ts` ORDER_TYPE_MAP with OpenAlgo `transform_data.py`:
- [ ] MARKET → (broker format)
- [ ] LIMIT → (broker format)
- [ ] STOP → (broker format, e.g., SL, STOP_LOSS)
- [ ] STOP_LIMIT → (broker format, e.g., SL-M, STOP_LOSS_MARKET)

#### 6. Response Field Mappings Verification
Compare `mapper.ts` with OpenAlgo `order_data.py` / `transform_data.py`:

**Order Response:**
- [ ] `orderId` field name correct
- [ ] `tradingSymbol` / `symbol` field name correct
- [ ] `exchangeSegment` / `exchange` field name correct
- [ ] `transactionType` / `side` field name correct
- [ ] `quantity`, `filledQty`, `remainingQuantity` field names correct
- [ ] `price`, `tradedPrice`, `averagePrice` field names correct
- [ ] `orderStatus` field name and values correct (PENDING, TRADED, REJECTED, etc.)
- [ ] `omsErrorDescription` / error message field name correct

**Position Response:**
- [ ] `netQty` field name correct
- [ ] `dayBuyQty`, `daySellQty` field names correct
- [ ] `costPrice`, `lastPrice` field names correct
- [ ] `realizedProfit`, `unrealizedProfit` field names correct

**Funds Response:**
- [ ] `availableBalance` field name correct (watch for typos like `availabelBalance`)
- [ ] `utilizedAmount` / `usedMargin` field name correct
- [ ] `collateralAmount` field name correct

**Quote Response:**
- [ ] `last_price` / `lastPrice` field name (check both cases)
- [ ] `ohlc.open/high/low/close` structure correct
- [ ] `depth.buy/sell` array structure correct
- [ ] `volume`, `oi` field names correct

#### 7. Auth Flow Verification
Compare with OpenAlgo `auth_api.py`:
- [ ] OAuth flow steps match (consent → login → token exchange)
- [ ] Token exchange endpoint and parameters correct
- [ ] Token validation method correct
- [ ] Response fields (`accessToken`, `userId`, etc.) correctly parsed

#### 8. Master Contract Verification
Compare with OpenAlgo `master_contract_db.py`:
- [ ] CSV/JSON download URL correct
- [ ] Column/field parsing matches OpenAlgo logic
- [ ] Symbol, securityId, exchange mapping correct
- [ ] Instrument type detection logic matches

### Cross-Check Command

After implementation, run:
```bash
# TypeScript compilation check
bunx tsc --noEmit

# Rust compilation check
cargo check --manifest-path src-tauri/Cargo.toml
```

### Common Cross-Check Findings

| Issue | Where to Check | Fix |
|-------|----------------|-----|
| Wrong header name | OpenAlgo `order_api.py` headers dict | Update Rust `dhan_request()` |
| Wrong endpoint path | OpenAlgo `get_url()` calls | Update Rust endpoint strings |
| Wrong field name | OpenAlgo response parsing | Update TS `mapper.ts` |
| Missing exchange | OpenAlgo `map_exchange_type()` | Add to `constants.ts` |
| Wrong order type | OpenAlgo `map_order_type()` | Update `ORDER_TYPE_MAP` |
| Typo in API field | OpenAlgo source (e.g., `availabelBalance`) | Handle both spellings |

## Quick Reference: File Locations

| Component | Location |
|-----------|----------|
| Rust commands | `src-tauri/src/commands/brokers/{broker}.rs` |
| Rust mod.rs | `src-tauri/src/commands/brokers/mod.rs` |
| Rust lib.rs | `src-tauri/src/lib.rs` |
| TS Adapter | `src/brokers/stocks/india/{broker}/{Broker}Adapter.ts` |
| TS constants | `src/brokers/stocks/india/{broker}/constants.ts` |
| TS mapper | `src/brokers/stocks/india/{broker}/mapper.ts` |
| Registry | `src/brokers/stocks/registry.ts` |
| Init | `src/brokers/stocks/init.ts` |
| UI Setup | `src/components/tabs/equity-trading/components/BrokerSetupPanel.tsx` |
| UI Auth | `src/components/tabs/equity-trading/components/BrokerAuthPanel.tsx` |

## OpenAlgo → Fincept Mapping

| OpenAlgo File | Fincept Equivalent |
|---------------|-------------------|
| `api/auth_api.py` | Rust: `{broker}_exchange_token`, TS: `authenticate()` |
| `api/order_api.py` | Rust: `{broker}_place/modify/cancel_order` |
| `api/data.py` | Rust: `{broker}_get_quotes/history/depth` |
| `api/funds.py` | Rust: `{broker}_get_funds` |
| `database/master_contract_db.py` | Rust: `{broker}_download_master_contract`, symbol search commands |
| `mapping/order_data.py` | TS: `mapper.ts` |
| `mapping/transform_data.py` | TS: `mapper.ts` |

---
**Last Updated:** January 2025 | **Supported Brokers:** Angel One, Zerodha, Fyers, Upstox, Dhan
