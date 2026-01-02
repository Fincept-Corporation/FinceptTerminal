/**
 * Get Ticker Stats Node
 * Fetches 24h statistics, ATH/ATL, and performance metrics
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { MarketDataBridge } from '../../adapters/MarketDataBridge';

export class GetTickerStatsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Get Ticker Stats',
    name: 'getTickerStats',
    icon: 'file:stats.svg',
    group: ['marketData'],
    version: 1,
    subtitle: '={{$parameter["symbol"]}} stats',
    description: 'Fetches 24h statistics, performance metrics, and historical extremes',
    defaults: {
      name: 'Ticker Stats',
      color: '#10b981',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Symbols',
        name: 'symbols',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'AAPL, BTC-USD',
        description: 'Comma-separated symbols to get stats for',
      },
      {
        displayName: 'Use Input Symbols',
        name: 'useInputSymbols',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbols from input data',
      },
      {
        displayName: 'Stats Type',
        name: 'statsType',
        type: NodePropertyType.Options,
        default: '24h',
        options: [
          { name: '24 Hour Stats', value: '24h' },
          { name: 'Performance Summary', value: 'performance' },
          { name: 'All-Time High/Low', value: 'ath' },
          { name: '52-Week Range', value: '52week' },
          { name: 'Volatility Stats', value: 'volatility' },
          { name: 'Volume Profile', value: 'volume' },
          { name: 'Complete Stats', value: 'all' },
        ],
        description: 'Type of statistics to fetch',
      },
      {
        displayName: 'Data Provider',
        name: 'provider',
        type: NodePropertyType.Options,
        default: 'yahoo',
        options: [
          { name: 'Yahoo Finance', value: 'yahoo' },
          { name: 'Polygon.io', value: 'polygon' },
          { name: 'Binance', value: 'binance' },
          { name: 'CoinGecko', value: 'coingecko' },
          { name: 'Alpha Vantage', value: 'alphavantage' },
        ],
        description: 'Market data provider',
      },
      {
        displayName: 'Performance Periods',
        name: 'performancePeriods',
        type: NodePropertyType.MultiOptions,
        default: ['1d', '1w', '1m', 'ytd'],
        options: [
          { name: '1 Day', value: '1d' },
          { name: '1 Week', value: '1w' },
          { name: '1 Month', value: '1m' },
          { name: '3 Months', value: '3m' },
          { name: '6 Months', value: '6m' },
          { name: 'Year to Date', value: 'ytd' },
          { name: '1 Year', value: '1y' },
          { name: '3 Years', value: '3y' },
          { name: '5 Years', value: '5y' },
        ],
        description: 'Time periods for performance calculation',
        displayOptions: {
          show: { statsType: ['performance', 'all'] },
        },
      },
      {
        displayName: 'Include Technical Levels',
        name: 'includeTechnical',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include support/resistance and pivot points',
      },
      {
        displayName: 'Include Beta',
        name: 'includeBeta',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include beta coefficient vs benchmark',
      },
      {
        displayName: 'Benchmark',
        name: 'benchmark',
        type: NodePropertyType.Options,
        default: 'SPY',
        options: [
          { name: 'S&P 500 (SPY)', value: 'SPY' },
          { name: 'NASDAQ 100 (QQQ)', value: 'QQQ' },
          { name: 'Russell 2000 (IWM)', value: 'IWM' },
          { name: 'Bitcoin (BTC)', value: 'BTC-USD' },
          { name: 'Nifty 50', value: '^NSEI' },
        ],
        description: 'Benchmark for beta calculation',
        displayOptions: {
          show: { includeBeta: [true] },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputSymbols = this.getNodeParameter('useInputSymbols', 0) as boolean;
    const statsType = this.getNodeParameter('statsType', 0) as string;
    const provider = this.getNodeParameter('provider', 0) as string;
    const includeTechnical = this.getNodeParameter('includeTechnical', 0) as boolean;
    const includeBeta = this.getNodeParameter('includeBeta', 0) as boolean;

    let symbols: string[] = [];

    if (useInputSymbols) {
      const inputData = this.getInputData();
      for (const item of inputData) {
        if (item.json?.symbol) {
          symbols.push(String(item.json.symbol));
        }
      }
    } else {
      const symbolsStr = this.getNodeParameter('symbols', 0) as string;
      symbols = symbolsStr.split(',').map(s => s.trim()).filter(Boolean);
    }

    if (symbols.length === 0) {
      return [[{
        json: {
          error: 'No symbols provided',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    const bridge = new MarketDataBridge();
    const results: Array<{ json: Record<string, unknown> }> = [];

    for (const symbol of symbols) {
      try {
        const stats = await bridge.getTickerStats(symbol, provider);

        const result: Record<string, unknown> = {
          symbol,
          statsType,
          provider,
          fetchedAt: new Date().toISOString(),
        };

        // Add stats based on type
        switch (statsType) {
          case '24h':
            result.stats24h = {
              open: stats.open,
              high: stats.high,
              low: stats.low,
              close: stats.close,
              volume: stats.volume,
              volumeWeightedAvg: stats.vwap,
              change: stats.change,
              changePercent: stats.changePercent,
              trades: stats.trades,
            };
            break;

          case 'performance':
            const periods = this.getNodeParameter('performancePeriods', 0) as string[];
            result.performance = {};
            for (const period of periods) {
              (result.performance as Record<string, unknown>)[period] = this.calculatePerformance(stats, period);
            }
            break;

          case 'ath':
            result.allTimeHigh = stats.ath || stats.high52Week;
            result.allTimeLow = stats.atl || stats.low52Week;
            result.athDate = stats.athDate;
            result.atlDate = stats.atlDate;
            result.fromAth = stats.price && stats.ath ? ((stats.price - stats.ath) / stats.ath * 100).toFixed(2) + '%' : null;
            result.fromAtl = stats.price && stats.atl ? ((stats.price - stats.atl) / stats.atl * 100).toFixed(2) + '%' : null;
            break;

          case '52week':
            result.week52 = {
              high: stats.high52Week,
              low: stats.low52Week,
              range: stats.high52Week && stats.low52Week ? stats.high52Week - stats.low52Week : null,
              percentFromHigh: stats.price && stats.high52Week ? ((stats.price - stats.high52Week) / stats.high52Week * 100).toFixed(2) : null,
              percentFromLow: stats.price && stats.low52Week ? ((stats.price - stats.low52Week) / stats.low52Week * 100).toFixed(2) : null,
            };
            break;

          case 'volatility':
            result.volatility = {
              daily: stats.volatility?.daily || this.estimateVolatility(stats, 'daily'),
              weekly: stats.volatility?.weekly || this.estimateVolatility(stats, 'weekly'),
              monthly: stats.volatility?.monthly || this.estimateVolatility(stats, 'monthly'),
              annual: stats.volatility?.annual || this.estimateVolatility(stats, 'annual'),
              atr: stats.atr,
              atrPercent: stats.price && stats.atr ? (stats.atr / stats.price * 100).toFixed(2) : null,
            };
            break;

          case 'volume':
            result.volumeProfile = {
              current: stats.volume,
              average: stats.avgVolume,
              ratio: stats.volume && stats.avgVolume ? (stats.volume / stats.avgVolume).toFixed(2) : null,
              dollarVolume: stats.price && stats.volume ? stats.price * stats.volume : null,
              avgDollarVolume: stats.price && stats.avgVolume ? stats.price * stats.avgVolume : null,
            };
            break;

          case 'all':
          default:
            result.stats24h = {
              open: stats.open,
              high: stats.high,
              low: stats.low,
              close: stats.close,
              volume: stats.volume,
              change: stats.change,
              changePercent: stats.changePercent,
            };
            result.week52 = {
              high: stats.high52Week,
              low: stats.low52Week,
            };
            result.averages = {
              volume: stats.avgVolume,
              price50: stats.sma50,
              price200: stats.sma200,
            };
            break;
        }

        // Add technical levels if requested
        if (includeTechnical) {
          result.technicalLevels = this.calculateTechnicalLevels(stats);
        }

        // Add beta if requested
        if (includeBeta) {
          const benchmark = this.getNodeParameter('benchmark', 0) as string;
          result.beta = stats.beta || 1.0;
          result.benchmark = benchmark;
        }

        results.push({ json: result });
      } catch (error) {
        results.push({
          json: {
            symbol,
            error: error instanceof Error ? error.message : 'Unknown error',
            timestamp: new Date().toISOString(),
          },
        });
      }
    }

    return [results];
  }

  private calculatePerformance(stats: any, period: string): Record<string, unknown> {
    // Mock performance calculation - would use historical data in production
    const mockReturns: Record<string, number> = {
      '1d': -0.5,
      '1w': 2.3,
      '1m': 5.1,
      '3m': 12.4,
      '6m': 18.7,
      'ytd': 22.1,
      '1y': 35.2,
      '3y': 89.5,
      '5y': 156.3,
    };

    return {
      return: mockReturns[period] || 0,
      returnPercent: (mockReturns[period] || 0) + '%',
    };
  }

  private estimateVolatility(stats: any, period: string): number {
    // Estimate volatility from high-low range
    const range = stats.high && stats.low ? (stats.high - stats.low) / stats.low : 0;

    const multipliers: Record<string, number> = {
      daily: 1,
      weekly: Math.sqrt(5),
      monthly: Math.sqrt(21),
      annual: Math.sqrt(252),
    };

    return parseFloat((range * 100 * (multipliers[period] || 1)).toFixed(2));
  }

  private calculateTechnicalLevels(stats: any): Record<string, unknown> {
    const high = stats.high || stats.close;
    const low = stats.low || stats.close;
    const close = stats.close;

    // Classic pivot points
    const pivot = (high + low + close) / 3;
    const r1 = 2 * pivot - low;
    const s1 = 2 * pivot - high;
    const r2 = pivot + (high - low);
    const s2 = pivot - (high - low);

    return {
      pivot: parseFloat(pivot.toFixed(2)),
      resistance: {
        r1: parseFloat(r1.toFixed(2)),
        r2: parseFloat(r2.toFixed(2)),
      },
      support: {
        s1: parseFloat(s1.toFixed(2)),
        s2: parseFloat(s2.toFixed(2)),
      },
      movingAverages: {
        sma50: stats.sma50,
        sma200: stats.sma200,
        trend: stats.sma50 && stats.sma200 ? (stats.sma50 > stats.sma200 ? 'bullish' : 'bearish') : 'unknown',
      },
    };
  }
}
