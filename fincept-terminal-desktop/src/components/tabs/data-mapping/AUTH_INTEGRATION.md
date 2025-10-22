# Authentication Integration Strategy

## Overview

The Data Mapping system **does NOT handle authentication directly**. Instead, it leverages your existing **Data Sources** connections that already have authentication configured.

## Architecture Flow

```
┌──────────────────────────────────────────────────────────────────┐
│                     DATA SOURCE CONNECTION                       │
│  (Already configured in Data Sources tab)                       │
│                                                                  │
│  Connection ID: "conn_upstox_123"                               │
│  Type: "upstox"                                                 │
│  Config:                                                        │
│    - apiKey: "xyz123..."                                        │
│    - apiSecret: "secret..."                                     │
│    - redirectUrl: "..."                                         │
│    - accessToken: "..." (after OAuth flow)                      │
│                                                                  │
│  Status: CONNECTED ✓                                            │
└──────────────────────────────────────────────────────────────────┘
                            ↓
                            │ References connection
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│                     DATA MAPPING CONFIG                          │
│                                                                  │
│  Mapping ID: "map_upstox_ohlcv"                                 │
│  Name: "Upstox Historical Candles → OHLCV"                      │
│                                                                  │
│  Source:                                                        │
│    connectionId: "conn_upstox_123"  ← References connection     │
│    endpoint: "/historical-candle/{symbol}/day/{from}/{to}"      │
│    method: "GET"                                                │
│                                                                  │
│  Target:                                                        │
│    schema: "OHLCV"                                              │
│                                                                  │
│  Mappings: [...]                                                │
└──────────────────────────────────────────────────────────────────┘
                            ↓
                            │ Uses connection to fetch data
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│                     MAPPING EXECUTION ENGINE                     │
│                                                                  │
│  1. Load connection: getConnection("conn_upstox_123")           │
│  2. Get adapter: UpstoxAdapter(connection)                      │
│  3. Build request:                                              │
│     - URL: endpoint + parameters                                │
│     - Headers: Authorization from connection.config             │
│  4. Execute request via adapter                                 │
│  5. Receive raw response                                        │
│  6. Apply mapping transformations                               │
│  7. Return unified data                                         │
└──────────────────────────────────────────────────────────────────┘
```

## Implementation Details

### 1. Connection Selection in UI

When creating a mapping, users select from existing connections:

```typescript
// In DataMappingTab.tsx
function ConnectionSelector({ onSelect }: Props) {
  const connections = useDataSourceConnections(); // From existing data-sources

  return (
    <select onChange={(e) => onSelect(e.target.value)}>
      {connections.map(conn => (
        <option key={conn.id} value={conn.id}>
          {conn.name} ({conn.type}) - {conn.status}
        </option>
      ))}
    </select>
  );
}
```

### 2. Fetching Sample Data

During mapping creation, we need sample data. The system fetches it using the connection:

```typescript
// In MappingEngine.ts
async fetchSampleData(
  connection: DataSourceConnection,
  endpoint: string,
  params: Record<string, any>
): Promise<any> {
  // Get the adapter for this connection type
  const adapter = createAdapter(connection);

  if (!adapter) {
    throw new Error(`No adapter for ${connection.type}`);
  }

  // Build the request
  const request = {
    endpoint,
    method: 'GET',
    params,
  };

  // Execute via adapter (handles auth automatically)
  const response = await adapter.executeRequest(request);

  return response.data;
}
```

### 3. Runtime Execution

When a chart or analysis needs data, it requests mapped data:

```typescript
// Usage in other components
const mappingEngine = new MappingEngine();

const data = await mappingEngine.execute({
  mappingId: 'map_upstox_ohlcv',
  parameters: {
    symbol: 'SBIN',
    from: '2024-01-01',
    to: '2024-10-01'
  }
});

// Returns: [{ symbol, timestamp, open, high, low, close, volume }, ...]
```

## Broker-Specific Authentication

### Upstox Example

```typescript
// Connection configuration (already in data-sources)
const upstoxConnection: DataSourceConnection = {
  id: 'conn_upstox_123',
  type: 'upstox',
  config: {
    apiKey: 'xyz123',
    apiSecret: 'secret123',
    redirectUrl: 'http://localhost:1420/callback',
    accessToken: 'Bearer eyJ...',  // Obtained via OAuth
    refreshToken: '...',
  },
  status: 'connected'
};

// The UpstoxAdapter handles authentication
class UpstoxAdapter extends BaseAdapter {
  async executeRequest(request: any): Promise<any> {
    const headers = {
      'Authorization': this.connection.config.accessToken,
      'Accept': 'application/json'
    };

    const response = await fetch(
      `https://api.upstox.com/v2${request.endpoint}`,
      { headers, method: request.method }
    );

    return await response.json();
  }
}
```

### Fyers Example

```typescript
// Fyers uses API key + access token
const fyersConnection: DataSourceConnection = {
  id: 'conn_fyers_456',
  type: 'fyers',
  config: {
    appId: 'ABC123',
    apiSecret: 'secret456',
    accessToken: 'fyers_access_token_xyz',
  },
  status: 'connected'
};

class FyersAdapter extends BaseAdapter {
  async executeRequest(request: any): Promise<any> {
    const headers = {
      'Authorization': `${this.connection.config.appId}:${this.connection.config.accessToken}`,
    };

    const response = await fetch(
      `https://api.fyers.in${request.endpoint}`,
      { headers }
    );

    return await response.json();
  }
}
```

## Storage & Security

### Where Auth Data is Stored

```
DuckDB Tables:
├── data_source_connections
│   ├── id (primary key)
│   ├── type
│   ├── config (ENCRYPTED JSON with credentials)
│   └── status
│
└── data_mappings
    ├── id (primary key)
    ├── connection_id (foreign key → data_source_connections.id)
    ├── mapping_config (JSON)
    └── metadata
```

### Security Best Practices

1. **Credentials Encryption**: All API keys/secrets stored encrypted in DuckDB
2. **No Plaintext**: Never log credentials in console
3. **Token Refresh**: Adapters handle OAuth token refresh automatically
4. **Session Management**: Expired tokens trigger re-authentication flow
5. **Sandboxing**: Custom JS expressions run in sandboxed VM

## Endpoint Management

### How Users Specify Endpoints

```typescript
// Option 1: Manual entry
mapping.source.endpoint = "/historical-candle/{symbol}/day/{from}/{to}";

// Option 2: Select from pre-defined endpoints
const UPSTOX_ENDPOINTS = {
  historicalCandles: {
    path: '/historical-candle/{symbol}/{interval}/{from}/{to}',
    method: 'GET',
    parameters: [
      { name: 'symbol', type: 'string', required: true, location: 'path' },
      { name: 'interval', type: 'string', required: true, location: 'path' },
      { name: 'from', type: 'date', required: true, location: 'path' },
      { name: 'to', type: 'date', required: true, location: 'path' },
    ],
    authentication: { type: 'bearer', location: 'header' }
  },
  getProfile: {
    path: '/user/profile',
    method: 'GET',
    parameters: [],
    authentication: { type: 'bearer', location: 'header' }
  }
};
```

### Dynamic Parameter Substitution

```typescript
// User specifies:
endpoint: "/historical-candle/{symbol}/day/{from}/{to}"
parameters: { symbol: "SBIN", from: "2024-01-01", to: "2024-10-01" }

// Engine substitutes:
finalUrl: "/historical-candle/SBIN/day/2024-01-01/2024-10-01"
```

## Testing Flow

### In Mapping Creation UI

1. User selects connection: "Upstox Production"
2. System checks connection status (must be CONNECTED)
3. User enters or selects endpoint
4. User provides test parameters (symbol, dates, etc.)
5. Click "Fetch Sample Data"
6. System:
   - Gets adapter for connection
   - Builds authenticated request
   - Executes request
   - Returns raw JSON
7. User sees raw JSON in JSON Explorer
8. User creates field mappings visually
9. System shows live preview of transformed data
10. User tests with different parameters
11. Save mapping

## Error Handling

### Connection Errors

```typescript
try {
  const data = await mappingEngine.execute({ mappingId, parameters });
} catch (error) {
  if (error.code === 'AUTH_FAILED') {
    // Connection auth expired
    showNotification('Please reconnect your data source');
    redirectToDataSources();
  } else if (error.code === 'CONNECTION_NOT_FOUND') {
    showError('Data source connection deleted');
  } else if (error.code === 'RATE_LIMIT') {
    showWarning('API rate limit exceeded. Retry in 1 minute');
  }
}
```

## Benefits of This Approach

✅ **Separation of Concerns**: Auth handled by data-sources, mapping handles transformation
✅ **Reusability**: One connection can power multiple mappings
✅ **Security**: Credentials stored centrally with encryption
✅ **Maintainability**: Auth logic in adapters, not scattered in mapping code
✅ **Flexibility**: Easy to add new data sources without changing mapping system
✅ **User-Friendly**: Users don't re-enter credentials for each mapping

## Next Steps

When implementing, we'll:

1. Create `ConnectionService` to query existing connections
2. Extend adapters with `executeRequest(endpoint, params)` method
3. Build UI components to select connections and test endpoints
4. Add parameter substitution logic
5. Implement error handling for auth failures
