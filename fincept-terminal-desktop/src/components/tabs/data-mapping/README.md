# Data Mapping System

## 📁 Folder Structure

```
src/components/tabs/data-mapping/
├── DataMappingTab.tsx              # Main tab component (entry point)
│
├── types.ts                        # ✅ TypeScript interfaces & types
├── README.md                       # ✅ This file
├── AUTH_INTEGRATION.md             # ✅ Authentication strategy docs
│
├── schemas/
│   ├── index.ts                    # ✅ Unified financial data schemas (OHLCV, QUOTE, ORDER, etc.)
│   └── validators.ts               # TODO: Schema validation functions
│
├── engine/
│   ├── MappingEngine.ts            # TODO: Core transformation engine
│   ├── Validator.ts                # TODO: Data validation
│   │
│   └── parsers/
│       ├── index.ts                # TODO: Parser registry
│       ├── BaseParser.ts           # TODO: Abstract base class
│       ├── JSONPathParser.ts       # TODO: JSONPath implementation
│       ├── JSONataParser.ts        # TODO: JSONata implementation
│       ├── JMESPathParser.ts       # TODO: JMESPath implementation
│       ├── JavaScriptParser.ts     # TODO: Sandboxed JS execution
│       └── DirectParser.ts         # TODO: Simple object access
│
├── components/
│   ├── ConnectionSelector.tsx      # TODO: Select from existing data sources
│   ├── EndpointBuilder.tsx         # TODO: Build API endpoint with params
│   ├── JSONExplorer.tsx            # TODO: Interactive JSON tree viewer
│   ├── SchemaSelector.tsx          # TODO: Choose target schema
│   ├── MappingCanvas.tsx           # TODO: Visual field mapper
│   ├── FieldMapper.tsx             # TODO: Individual field mapping UI
│   ├── ExpressionBuilder.tsx       # TODO: Build parser expressions
│   ├── TransformBuilder.tsx        # TODO: Add transformation functions
│   ├── LivePreview.tsx             # TODO: Real-time preview
│   ├── ValidationPanel.tsx         # TODO: Show validation results
│   ├── TemplateLibrary.tsx         # TODO: Pre-built templates
│   └── MappingList.tsx             # TODO: List of saved mappings
│
├── templates/
│   ├── index.ts                    # TODO: Template registry
│   ├── indian-brokers.ts           # TODO: Upstox, Fyers, Dhan, etc.
│   ├── global-providers.ts         # TODO: Yahoo, Alpha Vantage, etc.
│   └── custom.ts                   # TODO: User custom templates
│
├── services/
│   ├── ConnectionService.ts        # TODO: Interface with data-sources
│   ├── MappingStorage.ts           # TODO: DuckDB persistence
│   └── AIMapper.ts                 # TODO: AI-powered suggestions
│
└── utils/
    ├── jsonFlattener.ts            # TODO: Flatten nested JSON
    ├── pathBuilder.ts              # TODO: Build JSONPath/JSONata
    ├── transformFunctions.ts       # TODO: Library of transforms
    └── sampleData.ts               # TODO: Sample data for testing
```

## 🎯 Core Concepts

### 1. **Unified Schemas**
Standardized data formats that all sources map to:
- **OHLCV**: Candlestick/bar data
- **QUOTE**: Real-time quotes
- **ORDER**: Trading orders
- **POSITION**: Portfolio positions
- **PORTFOLIO**: Account summary

### 2. **Parser Engines**
Multiple ways to extract data from complex JSON:
- **JSONPath**: Simple queries (`$.data.candles[*]`)
- **JSONata**: Powerful transformations
- **JMESPath**: AWS-style queries
- **JavaScript**: Full custom logic
- **Direct**: Simple object access

### 3. **Authentication Integration**
Mappings **reference** existing data source connections:
- No credential re-entry
- Uses existing adapters
- Handles OAuth/API keys automatically
- See `AUTH_INTEGRATION.md` for details

### 4. **Mapping Configuration**
Each mapping stores:
- Source connection ID
- API endpoint
- Field mappings (source → target)
- Transformations
- Validation rules

## 🚀 Usage Flow

### Step 1: User Creates Connection (in Data Sources tab)
```
Data Sources Tab
└─> Add Upstox
    └─> Enter API credentials
        └─> Test connection ✓
            └─> Save as "conn_upstox_prod"
```

### Step 2: User Creates Mapping (in Data Mapping tab)
```
Data Mapping Tab
└─> New Mapping
    └─> Select connection: "conn_upstox_prod"
        └─> Choose endpoint: "/historical-candle/{symbol}/day/{from}/{to}"
            └─> Fetch sample data
                └─> Map fields visually
                    └─> Test transformation
                        └─> Save as "map_upstox_ohlcv"
```

### Step 3: Other Components Use Mapped Data
```typescript
// In ChartComponent.tsx
const mappingEngine = new MappingEngine();

const candles = await mappingEngine.execute({
  mappingId: 'map_upstox_ohlcv',
  parameters: { symbol: 'SBIN', from: '2024-01-01', to: '2024-10-01' }
});

// Returns standardized OHLCV data
// [{ symbol, timestamp, open, high, low, close, volume }, ...]
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
- [ ] DuckDB integration
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
