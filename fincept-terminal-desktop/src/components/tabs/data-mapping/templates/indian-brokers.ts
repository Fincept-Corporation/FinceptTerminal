// Pre-built templates for Indian brokers

import { MappingTemplate, ParserEngine, FieldMapping } from '../types';

// Helper to convert legacy field mappings to new format
function convertLegacyMapping(legacy: any): FieldMapping {
  // Map ParserEngine enum to string literals
  const parserMap: Record<number, FieldMapping['parser']> = {
    0: 'jsonpath',
    1: 'jmespath',
    2: 'direct',
    3: 'javascript',
    4: 'jsonata',
  };

  return {
    targetField: legacy.targetField,
    source: {
      type: 'path' as const,
      path: legacy.sourceExpression,
    },
    parser: parserMap[legacy.parserEngine] || 'direct',
    transform: legacy.transform ? [legacy.transform] : undefined,
    transformParams: legacy.transformParams,
    defaultValue: legacy.defaultValue,
    required: legacy.required,
    confidence: legacy.confidence,
  };
}

export const INDIAN_BROKER_TEMPLATES: MappingTemplate[] = [
  // ========== UPSTOX TEMPLATES ==========

  {
    id: 'upstox_historical_ohlcv',
    name: 'Upstox Historical Candles → OHLCV',
    description: 'Map Upstox historical candle data to standard OHLCV format',
    category: 'broker',
    tags: ['upstox', 'historical', 'candles', 'ohlcv', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Upstox Historical → OHLCV',
      description: 'Transform Upstox historical candle data',
      target: {
        schemaType: 'predefined',
        schema: 'OHLCV',
      },
      extraction: {
        engine: ParserEngine.JSONATA,
        rootPath: 'data.candles',
        isArray: true,
        isArrayOfArrays: true,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'symbol', required: true, parserEngine: ParserEngine.DIRECT, defaultValue: 'UNKNOWN' },
        { targetField: 'timestamp', sourceExpression: '$[0]', required: true, parserEngine: ParserEngine.JSONATA, transform: 'toISODate' },
        { targetField: 'open', sourceExpression: '$[1]', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'high', sourceExpression: '$[2]', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'low', sourceExpression: '$[3]', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'close', sourceExpression: '$[4]', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'volume', sourceExpression: '$[5]', required: true, parserEngine: ParserEngine.JSONATA },
      ].map(convertLegacyMapping),
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['upstox', 'ohlcv'],
        aiGenerated: false,
      },
    },

    instructions: `
1. Configure Upstox connection in Data Sources
2. Use endpoint: /historical-candle/{instrument_key}/{interval}/{to_date}/{from_date}
3. Parameters: instrument_key (e.g., NSE_EQ|INE062A01020), interval (1minute, day, etc.)
4. Sample response format: { "status": "success", "data": { "candles": [[timestamp, o, h, l, c, v, oi]] } }
    `,
  },

  {
    id: 'upstox_market_quote_full',
    name: 'Upstox Market Quote → QUOTE',
    description: 'Map Upstox market quote data to standard QUOTE format',
    category: 'broker',
    tags: ['upstox', 'quote', 'realtime', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Upstox Quote → QUOTE',
      description: 'Transform Upstox market quote data',
      target: {
        schemaType: 'predefined',
        schema: 'QUOTE',
      },
      extraction: {
        engine: ParserEngine.JSONATA,
        rootPath: 'data',
        isArray: false,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'symbol', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'timestamp', sourceExpression: 'last_trade_time', required: true, parserEngine: ParserEngine.JSONATA, transform: 'toISODate' },
        { targetField: 'price', sourceExpression: 'last_price', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'bid', sourceExpression: 'depth.buy[0].price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'ask', sourceExpression: 'depth.sell[0].price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'bidSize', sourceExpression: 'depth.buy[0].quantity', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'askSize', sourceExpression: 'depth.sell[0].quantity', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'volume', sourceExpression: 'volume', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'open', sourceExpression: 'ohlc.open', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'high', sourceExpression: 'ohlc.high', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'low', sourceExpression: 'ohlc.low', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'previousClose', sourceExpression: 'ohlc.close', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'change', sourceExpression: 'net_change', required: false, parserEngine: ParserEngine.JSONATA },
      ].map(convertLegacyMapping),
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['upstox', 'quote'],
        aiGenerated: false,
      },
    },
  },

  // ========== FYERS TEMPLATES ==========

  {
    id: 'fyers_quotes',
    name: 'Fyers Quotes → QUOTE',
    description: 'Map Fyers quote data to standard QUOTE format',
    category: 'broker',
    tags: ['fyers', 'quote', 'realtime', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Fyers Quote → QUOTE',
      description: 'Transform Fyers quote data',
      target: {
        schemaType: 'predefined',
        schema: 'QUOTE',
      },
      extraction: {
        engine: ParserEngine.JSONATA,
        rootPath: 'd',
        isArray: true,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'n', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'timestamp', sourceExpression: 'v.tt', required: true, parserEngine: ParserEngine.JSONATA, transform: 'toISODate' },
        { targetField: 'price', sourceExpression: 'v.lp', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'bid', sourceExpression: 'v.bid', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'ask', sourceExpression: 'v.ask', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'volume', sourceExpression: 'v.volume', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'open', sourceExpression: 'v.open_price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'high', sourceExpression: 'v.high_price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'low', sourceExpression: 'v.low_price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'previousClose', sourceExpression: 'v.prev_close_price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'change', sourceExpression: 'v.ch', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'changePercent', sourceExpression: 'v.chp', required: false, parserEngine: ParserEngine.JSONATA },
      ].map(convertLegacyMapping),
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['fyers', 'quote'],
        aiGenerated: false,
      },
    },

    instructions: `
1. Configure Fyers connection in Data Sources
2. Use endpoint: /data/quotes
3. Method: POST with symbols array in body
4. Sample response: { "s": "ok", "d": [{ "n": "NSE:SBIN-EQ", "v": {...} }] }
    `,
  },

  {
    id: 'fyers_positions',
    name: 'Fyers Positions → POSITION',
    description: 'Map Fyers position data to standard POSITION format',
    category: 'broker',
    tags: ['fyers', 'position', 'portfolio', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Fyers Position → POSITION',
      description: 'Transform Fyers position data',
      target: {
        schemaType: 'predefined',
        schema: 'POSITION',
      },
      extraction: {
        engine: ParserEngine.JSONATA,
        rootPath: 'netPositions',
        isArray: true,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'symbol', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'quantity', sourceExpression: 'netQty', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'averagePrice', sourceExpression: 'avgPrice', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'currentPrice', sourceExpression: 'ltp', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'pnl', sourceExpression: 'pl', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'pnlPercent', sourceExpression: 'plPercent', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'productType', sourceExpression: 'productType', required: false, parserEngine: ParserEngine.JSONATA },
      ].map(convertLegacyMapping),
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['fyers', 'position'],
        aiGenerated: false,
      },
    },
  },

  // ========== DHAN TEMPLATES ==========

  {
    id: 'dhan_positions',
    name: 'Dhan Positions → POSITION',
    description: 'Map Dhan position data to standard POSITION format',
    category: 'broker',
    tags: ['dhan', 'position', 'portfolio', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Dhan Position → POSITION',
      description: 'Transform Dhan position data',
      target: {
        schemaType: 'predefined',
        schema: 'POSITION',
      },
      extraction: {
        engine: ParserEngine.JSONPATH,
        rootPath: '$.data[*]',
        isArray: true,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'tradingSymbol', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'quantity', sourceExpression: 'netQty', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'averagePrice', sourceExpression: 'costPrice', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'currentPrice', sourceExpression: 'ltp', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'pnl', sourceExpression: 'realizedProfit', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'exchange', sourceExpression: 'exchangeSegment', required: false, parserEngine: ParserEngine.JSONPATH },
      ].map(convertLegacyMapping),
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['dhan', 'position'],
        aiGenerated: false,
      },
    },
  },

  {
    id: 'dhan_orders',
    name: 'Dhan Orders → ORDER',
    description: 'Map Dhan order data to standard ORDER format',
    category: 'broker',
    tags: ['dhan', 'order', 'trading', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Dhan Order → ORDER',
      description: 'Transform Dhan order data',
      target: {
        schemaType: 'predefined',
        schema: 'ORDER',
      },
      extraction: {
        engine: ParserEngine.JSONPATH,
        rootPath: '$.data[*]',
        isArray: true,
      },
      fieldMappings: [
        { targetField: 'orderId', sourceExpression: 'orderId', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'symbol', sourceExpression: 'tradingSymbol', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'side', sourceExpression: 'transactionType', required: true, parserEngine: ParserEngine.JSONPATH, transform: 'uppercase' },
        { targetField: 'type', sourceExpression: 'orderType', required: true, parserEngine: ParserEngine.JSONPATH, transform: 'uppercase' },
        { targetField: 'quantity', sourceExpression: 'quantity', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'filledQuantity', sourceExpression: 'tradedQuantity', required: false, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'price', sourceExpression: 'price', required: false, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'averagePrice', sourceExpression: 'tradedPrice', required: false, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'status', sourceExpression: 'orderStatus', required: true, parserEngine: ParserEngine.JSONPATH, transform: 'uppercase' },
        { targetField: 'timestamp', sourceExpression: 'createTime', required: true, parserEngine: ParserEngine.JSONPATH, transform: 'toISODate' },
        { targetField: 'exchange', sourceExpression: 'exchangeSegment', required: false, parserEngine: ParserEngine.JSONPATH },
      ].map(convertLegacyMapping),
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['dhan', 'order'],
        aiGenerated: false,
      },
    },
  },

  // ========== ZERODHA (KITE) TEMPLATES ==========

  {
    id: 'zerodha_ohlc',
    name: 'Zerodha OHLC → OHLCV',
    description: 'Map Zerodha/Kite OHLC data to standard OHLCV format',
    category: 'broker',
    tags: ['zerodha', 'kite', 'ohlc', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Zerodha OHLC → OHLCV',
      description: 'Transform Zerodha OHLC data',
      target: {
        schemaType: 'predefined',
        schema: 'OHLCV',
      },
      extraction: {
        engine: ParserEngine.CUSTOM_JS,
        rootPath: 'data',
        isArray: false,
      },
      fieldMappings: [
        {
          targetField: 'all',
          source: {
            type: 'expression',
            path: `
            return Object.entries(data).map(([symbol, ohlc]) => ({
              symbol: symbol,
              timestamp: new Date().toISOString(),
              open: ohlc.open,
              high: ohlc.high,
              low: ohlc.low,
              close: ohlc.close || ohlc.last_price,
              volume: ohlc.volume || 0
            }))
          `,
          },
          parser: 'javascript',
          required: true,
        },
      ],
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['zerodha', 'ohlc'],
        aiGenerated: false,
      },
    },

    instructions: `
1. Configure Zerodha/Kite connection
2. Use endpoint: /quote/ohlc
3. Query params: i=NSE:SBIN&i=NSE:RELIANCE
4. Response: { "data": { "NSE:SBIN": { "open": 590, ... } } }
    `,
  },

  // ========== SHIPNEXT TEMPLATES ==========

  {
    id: 'shipnext_ports',
    name: 'ShipNext Ports API',
    description: 'Fetch global port data from ShipNext API',
    category: 'custom',
    tags: ['shipnext', 'ports', 'maritime', 'shipping', 'logistics'],
    verified: true,
    official: false,

    mappingConfig: {
      name: 'ShipNext Ports',
      description: 'Fetch global port information from ShipNext',
      source: {
        type: 'api',
        apiConfig: {
          id: 'shipnext_ports_api',
          name: 'ShipNext Ports API',
          description: 'Global maritime port data from ShipNext',
          baseUrl: 'https://shipnext.com',
          endpoint: '/api/v1/ports/',
          method: 'GET',
          headers: {
            'Accept': '*/*',
            'Content-Type': 'application/json',
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
          },
          queryParams: {
            'page': '1',
            'query': '',
            'country': '',
          },
          authentication: {
            type: 'none',
            config: {},
          },
          timeout: 30000,
          cacheEnabled: true,
          cacheTTL: 3600,
        },
      },
      target: {
        schemaType: 'custom',
        schema: 'PORT',
        customFields: [
          { name: 'id', type: 'string', description: 'Unique port identifier', required: false },
          { name: 'name', type: 'string', description: 'Port name', required: false },
          { name: 'country', type: 'string', description: 'Country code', required: false },
          { name: 'code', type: 'string', description: 'Port code (e.g., LOCODE)', required: false },
          { name: 'latitude', type: 'number', description: 'Geographic latitude', required: false },
          { name: 'longitude', type: 'number', description: 'Geographic longitude', required: false },
          { name: 'type', type: 'string', description: 'Port type', required: false },
        ],
      },
      extraction: {
        engine: ParserEngine.JSONPATH,
        rootPath: '$.data[*]',
        isArray: true,
      },
      fieldMappings: [
        {
          targetField: 'id',
          source: { type: 'path', path: '$._id' },
          parser: 'jsonpath',
          required: false,
          defaultValue: '',
        },
        {
          targetField: 'name',
          source: { type: 'path', path: '$.name' },
          parser: 'jsonpath',
          required: false,
          defaultValue: '',
        },
        {
          targetField: 'country',
          source: { type: 'path', path: '$.country.name' },
          parser: 'jsonpath',
          required: false,
          defaultValue: '',
        },
        {
          targetField: 'code',
          source: { type: 'path', path: '$.unLoCode' },
          parser: 'jsonpath',
          required: false,
          defaultValue: '',
        },
        {
          targetField: 'latitude',
          source: { type: 'path', path: '$.coordinates[1]' },
          parser: 'jsonpath',
          required: false,
          defaultValue: 0,
        },
        {
          targetField: 'longitude',
          source: { type: 'path', path: '$.coordinates[0]' },
          parser: 'jsonpath',
          required: false,
          defaultValue: 0,
        },
        {
          targetField: 'type',
          source: { type: 'path', path: '$.sefName' },
          parser: 'jsonpath',
          required: false,
          defaultValue: '',
        },
      ],
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['shipnext', 'ports', 'maritime'],
        aiGenerated: false,
      },
    },

    instructions: `
1. Open Data Mapping tab
2. Load this template from the template library
3. Optional: Modify query parameters:
   - page: Change pagination (default: 1)
   - query: Add search term for port name
   - country: Filter by country code (e.g., 'IN', 'US', 'CN')
4. Click "Test & Fetch Sample" to verify API response
5. Field mappings are pre-configured for common port data
6. Data is cached for 1 hour to reduce API calls
7. Save the mapping to use it in your workflows

Note: This API does not require authentication. If you encounter CORS issues,
you may need to configure your Tauri app permissions or use a proxy.
    `,
  },
];

// Helper to get template by ID
export function getTemplateById(id: string): MappingTemplate | undefined {
  return INDIAN_BROKER_TEMPLATES.find((t) => t.id === id);
}

// Helper to get templates by broker
export function getTemplatesByBroker(broker: string): MappingTemplate[] {
  return INDIAN_BROKER_TEMPLATES.filter((t) => t.tags.includes(broker.toLowerCase()));
}

// Helper to get all broker names
export function getAllBrokers(): string[] {
  const brokers = new Set<string>();
  INDIAN_BROKER_TEMPLATES.forEach((t) => {
    t.tags.forEach((tag) => {
      if (['upstox', 'fyers', 'dhan', 'zerodha', 'angelone', 'shipnext'].includes(tag)) {
        brokers.add(tag);
      }
    });
  });
  return Array.from(brokers);
}
