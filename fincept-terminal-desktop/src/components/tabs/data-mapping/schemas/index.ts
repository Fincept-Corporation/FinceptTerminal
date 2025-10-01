// Unified Financial Data Schemas
// These are the standardized formats that all data sources will be mapped to

import { UnifiedSchema } from '../types';

export const UNIFIED_SCHEMAS: Record<string, UnifiedSchema> = {
  // ========== MARKET DATA SCHEMAS ==========

  OHLCV: {
    name: 'OHLCV (Candlestick Data)',
    category: 'market-data',
    description: 'Open, High, Low, Close, Volume - Standard candlestick/bar data',
    fields: {
      symbol: {
        type: 'string',
        required: true,
        description: 'Trading symbol or ticker',
      },
      timestamp: {
        type: 'datetime',
        required: true,
        description: 'Bar timestamp (start time)',
      },
      open: {
        type: 'number',
        required: true,
        description: 'Opening price',
        validation: { min: 0 },
      },
      high: {
        type: 'number',
        required: true,
        description: 'Highest price in the period',
        validation: { min: 0 },
      },
      low: {
        type: 'number',
        required: true,
        description: 'Lowest price in the period',
        validation: { min: 0 },
      },
      close: {
        type: 'number',
        required: true,
        description: 'Closing price',
        validation: { min: 0 },
      },
      volume: {
        type: 'number',
        required: true,
        description: 'Trading volume',
        validation: { min: 0 },
      },
      vwap: {
        type: 'number',
        required: false,
        description: 'Volume weighted average price',
        validation: { min: 0 },
      },
      trades: {
        type: 'number',
        required: false,
        description: 'Number of trades',
        validation: { min: 0 },
      },
    },
    examples: [
      {
        symbol: 'AAPL',
        timestamp: '2024-10-01T09:30:00.000Z',
        open: 174.50,
        high: 176.20,
        low: 174.10,
        close: 175.43,
        volume: 5200000,
        vwap: 175.15,
      },
    ],
  },

  QUOTE: {
    name: 'Real-time Quote',
    category: 'market-data',
    description: 'Real-time or delayed quote with bid/ask spreads',
    fields: {
      symbol: {
        type: 'string',
        required: true,
        description: 'Trading symbol',
      },
      timestamp: {
        type: 'datetime',
        required: true,
        description: 'Quote timestamp',
      },
      price: {
        type: 'number',
        required: true,
        description: 'Last traded price',
        validation: { min: 0 },
      },
      bid: {
        type: 'number',
        required: false,
        description: 'Best bid price',
        validation: { min: 0 },
      },
      ask: {
        type: 'number',
        required: false,
        description: 'Best ask price',
        validation: { min: 0 },
      },
      bidSize: {
        type: 'number',
        required: false,
        description: 'Bid quantity',
        validation: { min: 0 },
      },
      askSize: {
        type: 'number',
        required: false,
        description: 'Ask quantity',
        validation: { min: 0 },
      },
      volume: {
        type: 'number',
        required: false,
        description: 'Total volume',
        validation: { min: 0 },
      },
      change: {
        type: 'number',
        required: false,
        description: 'Change from previous close',
      },
      changePercent: {
        type: 'number',
        required: false,
        description: 'Percentage change',
      },
      open: {
        type: 'number',
        required: false,
        description: 'Day open price',
        validation: { min: 0 },
      },
      high: {
        type: 'number',
        required: false,
        description: 'Day high price',
        validation: { min: 0 },
      },
      low: {
        type: 'number',
        required: false,
        description: 'Day low price',
        validation: { min: 0 },
      },
      previousClose: {
        type: 'number',
        required: false,
        description: 'Previous day close',
        validation: { min: 0 },
      },
    },
    examples: [
      {
        symbol: 'SBIN',
        timestamp: '2024-10-01T15:30:00.000Z',
        price: 590.50,
        bid: 590.45,
        ask: 590.55,
        bidSize: 5000,
        askSize: 3800,
        volume: 5420000,
        change: 12.45,
        changePercent: 2.15,
        open: 580.25,
        high: 592.10,
        low: 579.80,
        previousClose: 578.05,
      },
    ],
  },

  TICK: {
    name: 'Tick Data',
    category: 'market-data',
    description: 'Individual trade ticks',
    fields: {
      symbol: {
        type: 'string',
        required: true,
        description: 'Trading symbol',
      },
      timestamp: {
        type: 'datetime',
        required: true,
        description: 'Trade timestamp',
      },
      price: {
        type: 'number',
        required: true,
        description: 'Trade price',
        validation: { min: 0 },
      },
      quantity: {
        type: 'number',
        required: true,
        description: 'Trade quantity',
        validation: { min: 0 },
      },
      side: {
        type: 'enum',
        required: false,
        description: 'Buy or Sell',
        values: ['BUY', 'SELL', 'UNKNOWN'],
      },
      tradeId: {
        type: 'string',
        required: false,
        description: 'Unique trade identifier',
      },
    },
  },

  // ========== TRADING SCHEMAS ==========

  ORDER: {
    name: 'Order',
    category: 'trading',
    description: 'Trading order (placed, pending, filled, cancelled)',
    fields: {
      orderId: {
        type: 'string',
        required: true,
        description: 'Unique order identifier',
      },
      symbol: {
        type: 'string',
        required: true,
        description: 'Trading symbol',
      },
      side: {
        type: 'enum',
        required: true,
        description: 'Buy or Sell',
        values: ['BUY', 'SELL'],
      },
      type: {
        type: 'enum',
        required: true,
        description: 'Order type',
        values: ['MARKET', 'LIMIT', 'STOP', 'STOP_LIMIT'],
      },
      quantity: {
        type: 'number',
        required: true,
        description: 'Order quantity',
        validation: { min: 0 },
      },
      filledQuantity: {
        type: 'number',
        required: false,
        description: 'Filled quantity',
        validation: { min: 0 },
      },
      price: {
        type: 'number',
        required: false,
        description: 'Limit price (for limit orders)',
        validation: { min: 0 },
      },
      stopPrice: {
        type: 'number',
        required: false,
        description: 'Stop price (for stop orders)',
        validation: { min: 0 },
      },
      averagePrice: {
        type: 'number',
        required: false,
        description: 'Average fill price',
        validation: { min: 0 },
      },
      status: {
        type: 'enum',
        required: true,
        description: 'Order status',
        values: ['PENDING', 'OPEN', 'PARTIALLY_FILLED', 'FILLED', 'CANCELLED', 'REJECTED'],
      },
      timestamp: {
        type: 'datetime',
        required: true,
        description: 'Order placement timestamp',
      },
      updatedAt: {
        type: 'datetime',
        required: false,
        description: 'Last update timestamp',
      },
      timeInForce: {
        type: 'enum',
        required: false,
        description: 'Time in force',
        values: ['DAY', 'GTC', 'IOC', 'FOK'],
      },
      exchange: {
        type: 'string',
        required: false,
        description: 'Exchange name',
      },
    },
  },

  POSITION: {
    name: 'Position',
    category: 'portfolio',
    description: 'Open position in portfolio',
    fields: {
      symbol: {
        type: 'string',
        required: true,
        description: 'Trading symbol',
      },
      quantity: {
        type: 'number',
        required: true,
        description: 'Position quantity (positive for long, negative for short)',
      },
      averagePrice: {
        type: 'number',
        required: true,
        description: 'Average entry price',
        validation: { min: 0 },
      },
      currentPrice: {
        type: 'number',
        required: true,
        description: 'Current market price',
        validation: { min: 0 },
      },
      marketValue: {
        type: 'number',
        required: true,
        description: 'Current market value',
      },
      costBasis: {
        type: 'number',
        required: true,
        description: 'Total cost basis',
      },
      pnl: {
        type: 'number',
        required: true,
        description: 'Unrealized profit/loss',
      },
      pnlPercent: {
        type: 'number',
        required: true,
        description: 'Unrealized P&L percentage',
      },
      realizedPnl: {
        type: 'number',
        required: false,
        description: 'Realized profit/loss',
      },
      exchange: {
        type: 'string',
        required: false,
        description: 'Exchange name',
      },
      productType: {
        type: 'enum',
        required: false,
        description: 'Product type',
        values: ['DELIVERY', 'INTRADAY', 'MARGIN'],
      },
    },
  },

  PORTFOLIO: {
    name: 'Portfolio Summary',
    category: 'portfolio',
    description: 'Overall portfolio summary',
    fields: {
      totalValue: {
        type: 'number',
        required: true,
        description: 'Total portfolio value',
      },
      cash: {
        type: 'number',
        required: true,
        description: 'Available cash balance',
      },
      invested: {
        type: 'number',
        required: true,
        description: 'Total invested amount',
      },
      marketValue: {
        type: 'number',
        required: true,
        description: 'Total market value of holdings',
      },
      pnl: {
        type: 'number',
        required: true,
        description: 'Total unrealized P&L',
      },
      pnlPercent: {
        type: 'number',
        required: true,
        description: 'Total P&L percentage',
      },
      realizedPnl: {
        type: 'number',
        required: false,
        description: 'Total realized P&L',
      },
      dayPnl: {
        type: 'number',
        required: false,
        description: 'Today\'s P&L',
      },
      buyingPower: {
        type: 'number',
        required: false,
        description: 'Available buying power',
      },
      marginUsed: {
        type: 'number',
        required: false,
        description: 'Margin used',
      },
      timestamp: {
        type: 'datetime',
        required: true,
        description: 'Portfolio snapshot timestamp',
      },
    },
  },

  // ========== REFERENCE DATA SCHEMAS ==========

  INSTRUMENT: {
    name: 'Instrument',
    category: 'reference',
    description: 'Security/instrument master data',
    fields: {
      symbol: {
        type: 'string',
        required: true,
        description: 'Trading symbol',
      },
      name: {
        type: 'string',
        required: true,
        description: 'Instrument name',
      },
      exchange: {
        type: 'string',
        required: true,
        description: 'Exchange',
      },
      instrumentType: {
        type: 'enum',
        required: true,
        description: 'Type of instrument',
        values: ['EQUITY', 'FUTURES', 'OPTIONS', 'CURRENCY', 'COMMODITY', 'INDEX'],
      },
      isin: {
        type: 'string',
        required: false,
        description: 'ISIN code',
      },
      lotSize: {
        type: 'number',
        required: false,
        description: 'Lot size',
        validation: { min: 1 },
      },
      tickSize: {
        type: 'number',
        required: false,
        description: 'Minimum price movement',
        validation: { min: 0 },
      },
      expiry: {
        type: 'datetime',
        required: false,
        description: 'Expiry date (for derivatives)',
      },
      strike: {
        type: 'number',
        required: false,
        description: 'Strike price (for options)',
        validation: { min: 0 },
      },
      optionType: {
        type: 'enum',
        required: false,
        description: 'Call or Put',
        values: ['CALL', 'PUT'],
      },
    },
  },
};

// Helper function to get schema by name
export function getSchema(schemaName: string): UnifiedSchema | null {
  return UNIFIED_SCHEMAS[schemaName] || null;
}

// Get all schema names
export function getSchemaNames(): string[] {
  return Object.keys(UNIFIED_SCHEMAS);
}

// Get schemas by category
export function getSchemasByCategory(category: UnifiedSchema['category']): UnifiedSchema[] {
  return Object.values(UNIFIED_SCHEMAS).filter(schema => schema.category === category);
}
