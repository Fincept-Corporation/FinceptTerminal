# Data Mapping Tab - Standalone API Integration System

**Version**: 2.0.0 (Complete Rewrite - October 2025)
**Status**: ✅ Production Ready
**Dependencies Removed**: Data Sources Tab (now standalone)

## Overview

The Data Mapping Tab is a **standalone API integration system** that enables users to connect **any external API** directly to Fincept Terminal without requiring pre-built adapters. Users can configure APIs, map response data to schemas, and leverage existing terminal features - all with just basic API knowledge.

## 📁 Folder Structure

```
src/components/tabs/data-mapping/
├── DataMappingTab.tsx              # ✅ Main tab component (rewritten)
│
├── types.ts                        # ✅ TypeScript interfaces & types (updated)
├── README.md                       # ✅ This file (updated)
│
├── schemas/
│   ├── index.ts                    # ✅ Schema registry
│   └── unifiedSchemas.ts           # ✅ Unified financial data schemas
│
├── engine/
│   ├── MappingEngine.ts            # ✅ Core transformation engine (updated)
│   └── parsers/                    # ✅ Parser implementations
│       ├── index.ts
│       ├── BaseParser.ts
│       ├── JSONPathParser.ts
│       ├── JSONataParser.ts
│       ├── JMESPathParser.ts
│       ├── JavaScriptParser.ts
│       └── DirectParser.ts
│
├── components/                     # ✅ All UI components
│   ├── APIConfigurationPanel.tsx   # ✅ API setup UI (NEW)
│   ├── SchemaBuilder.tsx           # ✅ Schema selection/creation (NEW)
│   ├── VisualFieldMapper.tsx       # ✅ Field mapping interface (NEW)
│   ├── CacheSettings.tsx           # ✅ Cache & encryption settings (NEW)
│   ├── KeyValueEditor.tsx          # ✅ Headers/params editor (NEW)
│   ├── ErrorPanel.tsx              # ✅ User-friendly errors (NEW)
│   ├── JSONExplorer.tsx            # ✅ JSON tree viewer
│   ├── SchemaSelector.tsx          # ✅ Predefined schema picker
│   ├── LivePreview.tsx             # ✅ Test result display
│   ├── ExpressionBuilder.tsx       # ✅ Expression builder
│   └── TemplateLibrary.tsx         # ✅ Template library
│
├── services/                       # ✅ Core services (NEW)
│   ├── APIClient.ts                # ✅ HTTP client (NEW)
│   ├── MappingDatabase.ts          # ✅ SQLite operations (NEW)
│   └── EncryptionService.ts        # ✅ Credential encryption (NEW)
│
└── utils/                          # ✅ Utilities
    ├── urlBuilder.ts               # ✅ URL construction (NEW)
    └── transformFunctions.ts       # ✅ Data transformations
```

## 🎯 Key Features

### 🔌 Direct API Integration (No Pre-Built Adapters Required)
- Connect **any REST API** directly without pre-built adapters
- Support for all HTTP methods: GET, POST, PUT, DELETE, PATCH
- Comprehensive authentication:
  - API Key (header or query parameter)
  - Bearer Token
  - Basic Auth (username/password)
  - OAuth 2.0 (access token + refresh)
  - No Authentication
- Custom headers and query parameters
- Request body support (JSON) for POST/PUT/PATCH
- Dynamic URL parameters with `{placeholder}` syntax
- Timeout configuration (default: 30s)

### 🎯 Flexible Schema System
- **Predefined Schemas**: Built-in financial schemas (OHLCV, QUOTE, ORDER, POSITION, PORTFOLIO, etc.)
- **Custom Schemas**: Create your own field definitions for any data type
- Field types: string, number, boolean, datetime, array, object
- Field validation: min, max, pattern, required
- Default values support

### 🗺️ Visual Field Mapping
- Two-column interface: Source Data (JSON) ↔ Target Schema
- Multiple parser engines:
  - **JSONPath**: `$.data.price`
  - **JMESPath**: `data.price`
  - **Direct**: `data.price`
  - **JavaScript**: `data.price * 1.1`
- Click-to-map or manual expression entry
- Live preview of mapped values
- Smart status indicators (mapped/required/optional)

### 🔒 Security & Performance
- **Encryption**: AES-256-GCM encryption for API credentials (optional, user-controllable)
- **Caching**: Smart response caching with configurable TTL
- **SQLite Storage**: Local database for mappings and cache
- **Parameter Hashing**: Cache invalidation when parameters change

### 📊 Testing & Validation
- Test API requests before saving
- Live data preview
- Schema validation (type checking, required fields, patterns)
- Detailed error messages with contextual suggestions

## 🎯 Core Concepts

### 1. **Standalone System** (No Data Sources Dependency)
- Completely independent from Data Sources Tab
- Users configure APIs directly in this tab
- All credentials stored locally with optional encryption
- No need to create connections in another tab first

### 2. **Unified Schemas**
Standardized data formats that all sources map to:
- **OHLCV**: Candlestick/bar data
- **QUOTE**: Real-time quotes
- **ORDER**: Trading orders
- **POSITION**: Portfolio positions
- **PORTFOLIO**: Account summary
- **CUSTOM**: User-defined schemas for any data type

### 3. **Parser Engines**
Multiple ways to extract data from complex JSON:
- **JSONPath**: Simple queries (`$.data.candles[*]`)
- **JSONata**: Powerful transformations
- **JMESPath**: AWS-style queries
- **JavaScript**: Full custom logic
- **Direct**: Simple object access

### 4. **Mapping Configuration**
Each mapping stores:
- API configuration (URL, auth, headers, params)
- Target schema (predefined or custom)
- Field mappings (source → target)
- Transformations
- Validation rules
- Cache settings

## 🚀 6-Step Workflow

### Step 1: API Configuration
Configure your API connection:
- **Mapping Name** & Description
- **Base URL**: `https://api.example.com`
- **Endpoint Path**: `/v1/data/{symbol}` (supports `{placeholder}` syntax)
- **HTTP Method**: GET, POST, PUT, DELETE, PATCH
- **Authentication**:
  - Choose type (None, API Key, Bearer, Basic, OAuth2)
  - Configure credentials
- **Headers** & **Query Parameters** (optional)
- **Request Body** (for POST/PUT/PATCH)
- **Test API**: Click "Fetch Sample Data" to test connection

### Step 2: Schema Selection
Choose your target schema:
- **Option A - Predefined**: Select from built-in schemas (OHLCV, QUOTE, ORDER, etc.)
- **Option B - Custom**: Create your own fields
  - Add fields with name, type, description
  - Mark required fields
  - Set validation rules (min, max, pattern)

### Step 3: Fetch Sample Data
Verify sample data from Step 1:
- Review fetched JSON data
- Expand/collapse JSON tree
- If no data, go back to Step 1 and click "Fetch Sample Data"

### Step 4: Field Mapping
Map API response to schema fields:
- **Left Panel**: Source Data (JSON tree, click to select paths)
- **Right Panel**: Target Schema (click field to map)
- **Mapping Methods**:
  1. Click source path → select parser → click target field
  2. Or manually enter expression in target field
- **Preview**: Click play button to preview mapped value
- **Parser Engines**: JSONPath, JMESPath, Direct, JavaScript

### Step 5: Cache & Security Settings
Configure performance and security:
- **Enable Caching**: Toggle on/off
- **Cache TTL**: Set expiration time (seconds)
- **Encrypt Credentials**: Toggle on/off (AES-256-GCM)
- **Clear Cache**: Option to clear this mapping's cache or all cache

### Step 6: Test & Save
Test and save your mapping:
- **Click "Run Test"**: Test mapping with sample data
- **Review Output**: See transformed data
- **Validation Results**: Check if data meets schema requirements
- **Save Mapping**: Click "Save Mapping" (only enabled after successful test)

## 📝 Usage Example

### Complete Workflow: Alpha Vantage Stock Price API

```typescript
// Step 1: API Configuration
{
  name: "Alpha Vantage Stock Quote",
  baseUrl: "https://www.alphavantage.co",
  endpoint: "/query",
  method: "GET",
  authentication: {
    type: "apikey",
    config: {
      apiKey: {
        location: "query",
        keyName: "apikey",
        value: "YOUR_API_KEY"
      }
    }
  },
  queryParams: {
    "function": "GLOBAL_QUOTE",
    "symbol": "{symbol}"
  },
  cacheEnabled: true,
  cacheTTL: 300
}

// Step 2: Schema Selection
schemaType: "predefined"
selectedSchema: "QUOTE"

// Step 3: Sample Data Fetched
{
  "Global Quote": {
    "01. symbol": "IBM",
    "05. price": "139.74",
    "06. volume": "4935889",
    "07. latest trading day": "2024-10-20"
  }
}

// Step 4: Field Mappings
[
  {
    targetField: "symbol",
    source: { path: "$['Global Quote']['01. symbol']" },
    parser: "jsonpath"
  },
  {
    targetField: "price",
    source: { path: "$['Global Quote']['05. price']" },
    parser: "jsonpath"
  },
  {
    targetField: "volume",
    source: { path: "$['Global Quote']['06. volume']" },
    parser: "jsonpath"
  },
  {
    targetField: "timestamp",
    source: { path: "$['Global Quote']['07. latest trading day']" },
    parser: "jsonpath"
  }
]

// Step 5: Cache Settings
cacheEnabled: true
cacheTTL: 300

// Step 6: Test Result
{
  symbol: "IBM",
  price: 139.74,
  volume: 4935889,
  timestamp: "2024-10-20"
}
```

### Using Saved Mappings

```typescript
// In any component
import { mappingEngine } from './data-mapping/engine/MappingEngine';

// Execute mapping with parameters
const result = await mappingEngine.execute({
  mapping: savedMapping,
  parameters: { symbol: 'IBM' }
});

if (result.success) {
  console.log('Transformed data:', result.data);
  // Use standardized data in your application
}
```

## 📦 Installed Packages

```json
{
  "jsonpath": "^1.1.1",    // JSONPath queries
  "jsonata": "^2.0.3",     // JSONata transformations
  "jmespath": "^0.16.0"    // JMESPath queries
}
```

## 🎨 UI Design

### 3-Panel Layout
```
┌────────────┬─────────────────────┬────────────────┐
│            │                     │                │
│  Templates │   Mapping Canvas    │  Live Preview  │
│  Library   │                     │                │
│            │   Source → Target   │  Input:        │
│  • Upstox  │   ┌────┐   ┌────┐  │  {raw JSON}    │
│  • Fyers   │   │ltp ├──→│price│  │                │
│  • Dhan    │   ├────┤   ├────┤  │  ↓             │
│  • Yahoo   │   │vol ├──→│vol │  │                │
│  • Custom  │   └────┘   └────┘  │  Output:       │
│            │                     │  {transformed} │
│  [+ New]   │   [+ Transform]     │                │
│            │                     │  ✓ Validated   │
└────────────┴─────────────────────┴────────────────┘
```

## 🔧 Development Roadmap

### Phase 1: Foundation ✅
- [x] Create folder structure
- [x] Define TypeScript types
- [x] Create unified schemas
- [x] Design auth integration
- [x] Install parser libraries

### Phase 2: Parser Engine (Next)
- [ ] Implement BaseParser
- [ ] JSONPath parser
- [ ] JSONata parser
- [ ] JMESPath parser
- [ ] JavaScript parser with sandbox
- [ ] MappingEngine orchestration

### Phase 3: UI Components
- [ ] DataMappingTab main component
- [ ] ConnectionSelector
- [ ] JSONExplorer with expand/collapse
- [ ] SchemaSelector
- [ ] MappingCanvas visual mapper
- [ ] ExpressionBuilder with syntax highlighting
- [ ] LivePreview panel

### Phase 4: Templates & Storage
- [ ] Indian broker templates (Upstox, Fyers, Dhan)
- [ ] Global provider templates
- [ ] Import/export mappings

### Phase 5: Advanced Features
- [ ] AI-powered field suggestions
- [ ] Drag-drop field mapping
- [ ] Multi-mapping (combine data from multiple sources)
- [ ] Scheduled data refresh
- [ ] Caching layer

## 🧪 Testing Strategy

### Unit Tests
- Parser engines with sample JSON
- Transformation functions
- Validation rules

### Integration Tests
- Real API calls via connections
- End-to-end mapping execution
- Template loading and execution

### UI Tests
- Component rendering
- User interactions
- State management

## 📚 Example Mappings

### Upstox OHLCV
```typescript
{
  source: { connectionId: "conn_upstox", endpoint: "/historical-candle/{symbol}/day" },
  target: { schema: "OHLCV" },
  extraction: {
    engine: "jsonata",
    expression: "data.candles.{ timestamp: $[0], open: $[1], high: $[2], low: $[3], close: $[4], volume: $[5] }"
  }
}
```

### Fyers Quote
```typescript
{
  source: { connectionId: "conn_fyers", endpoint: "/quotes/" },
  target: { schema: "QUOTE" },
  extraction: {
    engine: "jsonata",
    expression: "d[0].v.{ symbol: symbol, price: lp, bid: spread.bid, ask: spread.ask }"
  }
}
```

## 🤝 Contributing

When adding new features:
1. Update types.ts if adding new interfaces
2. Add schema to schemas/index.ts if new data type
3. Document in README.md
4. Add examples
5. Write tests

## 📖 References

- [JSONPath Documentation](https://goessner.net/articles/JsonPath/)
- [JSONata Documentation](https://jsonata.org/)
- [JMESPath Documentation](https://jmespath.org/)
- [Upstox API Docs](https://upstox.com/developer/api-documentation)
- [Fyers API Docs](https://myapi.fyers.in/docsv3/)
