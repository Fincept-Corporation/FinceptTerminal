import { InternalTool } from '../types';

export const backtestingTools: InternalTool[] = [
  {
    name: 'run_backtest',
    description: 'Run a backtest with a given strategy on historical data',
    inputSchema: {
      type: 'object',
      properties: {
        strategy: { type: 'string', description: 'Strategy name or code identifier' },
        symbol: { type: 'string', description: 'Trading symbol (e.g., AAPL, BTC/USD)' },
        start_date: { type: 'string', description: 'Start date (YYYY-MM-DD)' },
        end_date: { type: 'string', description: 'End date (YYYY-MM-DD)' },
        initial_capital: { type: 'number', description: 'Starting capital amount' },
        provider: { type: 'string', description: 'Backtesting engine', enum: ['backtestingpy', 'vectorbt', 'fasttrade'] },
      },
      required: ['strategy', 'symbol', 'start_date', 'end_date'],
    },
    handler: async (args, contexts) => {
      if (!contexts.runBacktest) {
        return { success: false, error: 'Backtesting context not available' };
      }
      const result = await contexts.runBacktest(args);
      return { success: true, data: result, message: `Backtest completed for ${args.symbol}` };
    },
  },
  {
    name: 'optimize_strategy',
    description: 'Run parameter optimization on a strategy',
    inputSchema: {
      type: 'object',
      properties: {
        strategy: { type: 'string', description: 'Strategy to optimize' },
        symbol: { type: 'string', description: 'Symbol for optimization' },
        start_date: { type: 'string', description: 'Start date (YYYY-MM-DD)' },
        end_date: { type: 'string', description: 'End date (YYYY-MM-DD)' },
        parameters: { type: 'string', description: 'JSON string of parameter ranges to optimize' },
      },
      required: ['strategy', 'symbol', 'start_date', 'end_date'],
    },
    handler: async (args, contexts) => {
      if (!contexts.optimizeStrategy) {
        return { success: false, error: 'Backtesting context not available' };
      }
      const params = { ...args };
      if (args.parameters && typeof args.parameters === 'string') {
        try { params.parameters = JSON.parse(args.parameters); } catch { /* keep as string */ }
      }
      const result = await contexts.optimizeStrategy(params);
      return { success: true, data: result, message: 'Optimization completed' };
    },
  },
  {
    name: 'get_historical_data',
    description: 'Fetch historical OHLCV price data for a symbol',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Symbol (e.g., AAPL)' },
        start_date: { type: 'string', description: 'Start date (YYYY-MM-DD)' },
        end_date: { type: 'string', description: 'End date (YYYY-MM-DD)' },
        interval: { type: 'string', description: 'Data interval', enum: ['1m', '5m', '15m', '1h', '4h', '1d', '1w'] },
      },
      required: ['symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getHistoricalData) {
        return { success: false, error: 'Market data context not available' };
      }
      const data = await contexts.getHistoricalData(args);
      return { success: true, data };
    },
  },
];
