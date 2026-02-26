/**
 * Base Finance Node
 * Base class for all finance-related workflow nodes
 * Provides common functionality for market data, trading, and analytics nodes
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  INodeExecutionData,
  IDataObject,
  NodeConnectionType,
  NodePropertyType,
  INodeProperties,
} from '../../types';

// ================================
// FINANCE NODE CATEGORIES
// ================================

export enum FinanceNodeCategory {
  Trigger = 'trigger',
  MarketData = 'marketData',
  Trading = 'trading',
  Agent = 'agent',
  Analytics = 'analytics',
  ControlFlow = 'controlFlow',
  Safety = 'safety',
  Notification = 'notification',
  Transform = 'transform',
}

// ================================
// COMMON PARAMETER DEFINITIONS
// ================================

/**
 * Stock symbol parameter - reusable across nodes
 */
export const stockSymbolParameter: INodeProperties = {
  displayName: 'Symbol',
  name: 'symbol',
  type: NodePropertyType.String,
  default: '',
  required: true,
  description: 'Stock/crypto symbol (e.g., AAPL, BTC-USD)',
  placeholder: 'AAPL',
};

/**
 * Exchange parameter
 */
export const exchangeParameter: INodeProperties = {
  displayName: 'Exchange',
  name: 'exchange',
  type: NodePropertyType.Options,
  default: 'NSE',
  options: [
    { name: 'NSE', value: 'NSE', description: 'National Stock Exchange of India' },
    { name: 'BSE', value: 'BSE', description: 'Bombay Stock Exchange' },
    { name: 'NYSE', value: 'NYSE', description: 'New York Stock Exchange' },
    { name: 'NASDAQ', value: 'NASDAQ', description: 'NASDAQ' },
    { name: 'MCX', value: 'MCX', description: 'Multi Commodity Exchange' },
    { name: 'NFO', value: 'NFO', description: 'NSE F&O' },
    { name: 'Binance', value: 'binance', description: 'Binance Crypto Exchange' },
    { name: 'Kraken', value: 'kraken', description: 'Kraken Crypto Exchange' },
  ],
  description: 'Exchange to trade on',
};

/**
 * Broker parameter
 */
export const brokerParameter: INodeProperties = {
  displayName: 'Broker',
  name: 'broker',
  type: NodePropertyType.Options,
  default: 'paper',
  options: [
    { name: 'Paper Trading (Recommended)', value: 'paper', description: 'Simulated trading with no real money' },
    { name: 'Zerodha Kite', value: 'kite', description: 'Zerodha Kite broker' },
    { name: 'Fyers', value: 'fyers', description: 'Fyers broker' },
    { name: 'Alpaca', value: 'alpaca', description: 'Alpaca US broker' },
    { name: 'Binance', value: 'binance', description: 'Binance crypto exchange' },
    { name: 'Kraken', value: 'kraken', description: 'Kraken crypto exchange' },
  ],
  description: 'Broker to use for trading',
};

/**
 * Data provider parameter
 */
export const dataProviderParameter: INodeProperties = {
  displayName: 'Data Provider',
  name: 'provider',
  type: NodePropertyType.Options,
  default: 'yahoo',
  options: [
    { name: 'Yahoo Finance', value: 'yahoo', description: 'Free market data' },
    { name: 'Alpha Vantage', value: 'alphavantage', description: 'Free API with limits' },
    { name: 'Binance', value: 'binance', description: 'Crypto market data' },
    { name: 'CoinGecko', value: 'coingecko', description: 'Crypto prices and data' },
    { name: 'Finnhub', value: 'finnhub', description: 'Stock and crypto data' },
  ],
  description: 'Market data provider',
};

/**
 * Time interval parameter
 */
export const timeIntervalParameter: INodeProperties = {
  displayName: 'Interval',
  name: 'interval',
  type: NodePropertyType.Options,
  default: '1d',
  options: [
    { name: '1 Minute', value: '1m' },
    { name: '5 Minutes', value: '5m' },
    { name: '15 Minutes', value: '15m' },
    { name: '30 Minutes', value: '30m' },
    { name: '1 Hour', value: '1h' },
    { name: '4 Hours', value: '4h' },
    { name: '1 Day', value: '1d' },
    { name: '1 Week', value: '1w' },
    { name: '1 Month', value: '1M' },
  ],
  description: 'Time interval for data',
};

/**
 * Date range parameters
 */
export const dateRangeParameters: INodeProperties[] = [
  {
    displayName: 'Period',
    name: 'period',
    type: NodePropertyType.Options,
    default: '1mo',
    options: [
      { name: '1 Day', value: '1d' },
      { name: '5 Days', value: '5d' },
      { name: '1 Month', value: '1mo' },
      { name: '3 Months', value: '3mo' },
      { name: '6 Months', value: '6mo' },
      { name: '1 Year', value: '1y' },
      { name: '2 Years', value: '2y' },
      { name: '5 Years', value: '5y' },
      { name: 'Max', value: 'max' },
      { name: 'Custom', value: 'custom' },
    ],
    description: 'Time period for data',
  },
  {
    displayName: 'Start Date',
    name: 'startDate',
    type: NodePropertyType.DateTime,
    default: '',
    description: 'Start date for custom period',
    displayOptions: {
      show: { period: ['custom'] },
    },
  },
  {
    displayName: 'End Date',
    name: 'endDate',
    type: NodePropertyType.DateTime,
    default: '',
    description: 'End date for custom period',
    displayOptions: {
      show: { period: ['custom'] },
    },
  },
];

// ================================
// BASE FINANCE NODE CLASS
// ================================

/**
 * Abstract base class for finance nodes
 * Provides common utilities and patterns
 */
export abstract class BaseFinanceNode implements INodeType {
  abstract description: INodeTypeDescription;

  /**
   * Main execution method - must be implemented by subclasses
   */
  abstract execute(this: IExecuteFunctions): Promise<NodeOutput>;

  /**
   * Helper: Create execution data from JSON
   */
  protected createExecutionData(data: IDataObject | IDataObject[]): INodeExecutionData[] {
    if (Array.isArray(data)) {
      return data.map((item) => ({ json: item }));
    }
    return [{ json: data }];
  }

  /**
   * Helper: Create error output
   */
  protected createErrorOutput(error: Error, nodeId: string): INodeExecutionData[] {
    return [{
      json: {
        success: false,
        error: error.message,
        nodeId,
        timestamp: new Date().toISOString(),
      },
    }];
  }

  /**
   * Helper: Create success output with metadata
   */
  protected createSuccessOutput(
    data: IDataObject | IDataObject[],
    metadata?: IDataObject
  ): INodeExecutionData[] {
    const items = Array.isArray(data) ? data : [data];
    return items.map((item) => ({
      json: {
        success: true,
        data: item,
        timestamp: new Date().toISOString(),
        ...metadata,
      },
    }));
  }

  /**
   * Helper: Validate required parameters
   */
  protected validateParameters(
    params: IDataObject,
    required: string[]
  ): { valid: boolean; missing: string[] } {
    const missing = required.filter(
      (key) => params[key] === undefined || params[key] === null || params[key] === ''
    );
    return {
      valid: missing.length === 0,
      missing,
    };
  }

  /**
   * Helper: Parse symbol for different exchanges
   */
  protected parseSymbol(symbol: string, exchange?: string): {
    symbol: string;
    exchange: string;
    formatted: string;
  } {
    // Handle crypto symbols
    if (symbol.includes('-') || symbol.includes('/')) {
      const parts = symbol.split(/[-/]/);
      return {
        symbol: parts[0],
        exchange: exchange || 'crypto',
        formatted: `${parts[0]}/${parts[1] || 'USD'}`,
      };
    }

    // Handle stock symbols
    return {
      symbol: symbol.toUpperCase(),
      exchange: exchange || 'NSE',
      formatted: exchange ? `${exchange}:${symbol.toUpperCase()}` : symbol.toUpperCase(),
    };
  }

  /**
   * Helper: Format currency value
   */
  protected formatCurrency(value: number, currency: string = 'USD'): string {
    return new Intl.NumberFormat('en-US', {
      style: 'currency',
      currency,
    }).format(value);
  }

  /**
   * Helper: Calculate percentage change
   */
  protected calculatePercentChange(oldValue: number, newValue: number): number {
    if (oldValue === 0) return 0;
    return ((newValue - oldValue) / oldValue) * 100;
  }
}

// ================================
// NODE CREATION HELPERS
// ================================

/**
 * Create a basic node description with common defaults
 */
export function createNodeDescription(
  name: string,
  displayName: string,
  description: string,
  category: FinanceNodeCategory,
  properties: INodeProperties[],
  options?: Partial<INodeTypeDescription>
): INodeTypeDescription {
  const categoryColors: Record<FinanceNodeCategory, string> = {
    [FinanceNodeCategory.Trigger]: '#f59e0b',
    [FinanceNodeCategory.MarketData]: '#3b82f6',
    [FinanceNodeCategory.Trading]: '#10b981',
    [FinanceNodeCategory.Agent]: '#8b5cf6',
    [FinanceNodeCategory.Analytics]: '#ec4899',
    [FinanceNodeCategory.ControlFlow]: '#6b7280',
    [FinanceNodeCategory.Safety]: '#ef4444',
    [FinanceNodeCategory.Notification]: '#06b6d4',
    [FinanceNodeCategory.Transform]: '#84cc16',
  };

  return {
    displayName,
    name,
    icon: options?.icon || `file:${category}.svg`,
    group: [category],
    version: 1,
    description,
    defaults: {
      name: displayName,
      color: categoryColors[category],
    },
    inputs: options?.inputs || [NodeConnectionType.Main],
    outputs: options?.outputs || [NodeConnectionType.Main],
    properties,
    credentials: options?.credentials,
    hints: options?.hints,
    ...options,
  };
}

/**
 * Create a trigger node description (no inputs, only outputs)
 */
export function createTriggerNodeDescription(
  name: string,
  displayName: string,
  description: string,
  properties: INodeProperties[],
  options?: Partial<INodeTypeDescription>
): INodeTypeDescription {
  return createNodeDescription(
    name,
    displayName,
    description,
    FinanceNodeCategory.Trigger,
    properties,
    {
      ...options,
      inputs: [],
      outputs: [NodeConnectionType.Main],
    }
  );
}

/**
 * Create a trading node description with safety warning
 */
export function createTradingNodeDescription(
  name: string,
  displayName: string,
  description: string,
  properties: INodeProperties[],
  options?: Partial<INodeTypeDescription>
): INodeTypeDescription {
  return createNodeDescription(
    name,
    displayName,
    description,
    FinanceNodeCategory.Trading,
    properties,
    {
      ...options,
      hints: [
        {
          message: 'This node can execute real trades. Use paper trading mode for testing.',
          type: 'warning',
          location: 'outputPane',
          whenToDisplay: 'always',
        },
        ...(options?.hints || []),
      ],
    }
  );
}

// ================================
// EXPORTS
// ================================

export type {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  INodeExecutionData,
  IDataObject,
  INodeProperties,
};

export {
  NodeConnectionType,
  NodePropertyType,
};
