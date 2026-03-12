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
    displayName: 'Ticker Statistics',
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

    
    const results: Array<{ json: Record<string, any> }> = [];

    for (const symbol of symbols) {
      try {
        const stats = await MarketDataBridge.getTickerStats(symbol);

        const result: Record<string, any> = {
          symbol,
          statsType,
          provider,
          fetchedAt: new Date().toISOString(),
        };

        // Add stats based on type
        switch (statsType) {
          case '24h':
            result.stats24h = {
              lastPrice: stats.lastPrice,
              high: stats.highPrice24h,
              low: stats.lowPrice24h,
              volume: stats.volume24h,
              change: stats.change24h,
              changePercent: stats.changePercent24h,
            };
            break;

          case 'performance': {
            const periods = this.getNodeParameter('performancePeriods', 0) as string[];
            result.performance = {};
            for (const period of periods) {
              (result.performance as Record<string, any>)[period] = await GetTickerStatsNode.calculatePerformance(stats, period, symbol);
            }
            break;
          }

          case 'ath': {
            // Use max available history (5y) for ATH/ATL
            try {
              const history = await MarketDataBridge.getHistoricalData({ symbol, period: '5y' });
              if (history.length > 0) {
                let athVal = -Infinity, atlVal = Infinity, athDate = '', atlDate = '';
                for (const bar of history) {
                  if (bar.high > athVal) { athVal = bar.high; athDate = bar.timestamp; }
                  if (bar.low < atlVal) { atlVal = bar.low; atlDate = bar.timestamp; }
                }
                result.allTimeHigh = athVal;
                result.allTimeLow = atlVal;
                result.athDate = athDate;
                result.atlDate = atlDate;
                result.fromAth = stats.lastPrice ? parseFloat(((stats.lastPrice - athVal) / athVal * 100).toFixed(2)) + '%' : null;
                result.fromAtl = stats.lastPrice ? parseFloat(((stats.lastPrice - atlVal) / atlVal * 100).toFixed(2)) + '%' : null;
              }
            } catch {
              result.allTimeHigh = null;
              result.allTimeLow = null;
              result.note = 'Historical data unavailable';
            }
            break;
          }

          case '52week': {
            try {
              const history = await MarketDataBridge.getHistoricalData({ symbol, period: '1y' });
              if (history.length > 0) {
                const high52 = Math.max(...history.map(b => b.high));
                const low52 = Math.min(...history.map(b => b.low));
                result.week52 = {
                  high: high52,
                  low: low52,
                  range: parseFloat((high52 - low52).toFixed(2)),
                  percentFromHigh: stats.lastPrice ? parseFloat(((stats.lastPrice - high52) / high52 * 100).toFixed(2)) + '%' : null,
                  percentFromLow: stats.lastPrice ? parseFloat(((stats.lastPrice - low52) / low52 * 100).toFixed(2)) + '%' : null,
                };
              }
            } catch {
              result.week52 = { high: null, low: null, note: 'Historical data unavailable' };
            }
            break;
          }

          case 'volatility': {
            // Compute real volatility from historical daily returns
            let dailyVol = GetTickerStatsNode.estimateVolatility(stats, 'daily');
            try {
              const history = await MarketDataBridge.getHistoricalData({ symbol, period: '3mo' });
              if (history.length >= 5) {
                const returns: number[] = [];
                for (let i = 1; i < history.length; i++) {
                  if (history[i - 1].close > 0) {
                    returns.push((history[i].close - history[i - 1].close) / history[i - 1].close);
                  }
                }
                if (returns.length > 1) {
                  const mean = returns.reduce((a, b) => a + b, 0) / returns.length;
                  const variance = returns.reduce((sum, r) => sum + Math.pow(r - mean, 2), 0) / (returns.length - 1);
                  dailyVol = parseFloat((Math.sqrt(variance) * 100).toFixed(2));
                }
              }
            } catch { /* use estimate */ }

            result.volatility = {
              daily: dailyVol,
              weekly: parseFloat((dailyVol * Math.sqrt(5)).toFixed(2)),
              monthly: parseFloat((dailyVol * Math.sqrt(21)).toFixed(2)),
              annual: parseFloat((dailyVol * Math.sqrt(252)).toFixed(2)),
            };
            break;
          }

          case 'volume': {
            let avgVol: number | null = null;
            try {
              const history = await MarketDataBridge.getHistoricalData({ symbol, period: '1mo' });
              if (history.length > 0) {
                avgVol = Math.round(history.reduce((sum, b) => sum + b.volume, 0) / history.length);
              }
            } catch { /* leave null */ }

            const currentVol = stats.volume24h || 0;
            result.volumeProfile = {
              current: currentVol,
              average: avgVol,
              ratio: avgVol ? parseFloat((currentVol / avgVol).toFixed(2)) : null,
              dollarVolume: stats.lastPrice && currentVol ? parseFloat((stats.lastPrice * currentVol).toFixed(0)) : null,
              avgDollarVolume: stats.lastPrice && avgVol ? parseFloat((stats.lastPrice * avgVol).toFixed(0)) : null,
            };
            break;
          }

          case 'all':
          default: {
            result.stats24h = {
              lastPrice: stats.lastPrice,
              high: stats.highPrice24h,
              low: stats.lowPrice24h,
              volume: stats.volume24h,
              change: stats.change24h,
              changePercent: stats.changePercent24h,
            };

            // Fetch 52-week range and averages from real historical data
            try {
              const history = await MarketDataBridge.getHistoricalData({ symbol, period: '1y' });
              if (history.length > 0) {
                result.week52 = {
                  high: Math.max(...history.map(b => b.high)),
                  low: Math.min(...history.map(b => b.low)),
                };
                const avgVol = Math.round(history.reduce((s, b) => s + b.volume, 0) / history.length);
                const closes = history.map(b => b.close);
                const sma50 = closes.length >= 50
                  ? parseFloat((closes.slice(-50).reduce((a, b) => a + b, 0) / 50).toFixed(2))
                  : null;
                const sma200 = closes.length >= 200
                  ? parseFloat((closes.slice(-200).reduce((a, b) => a + b, 0) / 200).toFixed(2))
                  : null;
                result.averages = { volume: avgVol, price50: sma50, price200: sma200 };
              }
            } catch {
              result.week52 = { high: null, low: null };
              result.averages = { volume: null, price50: null, price200: null };
            }
            break;
          }
        }

        // Add technical levels if requested
        if (includeTechnical) {
          result.technicalLevels = GetTickerStatsNode.calculateTechnicalLevels(stats);
        }

        // Calculate real beta from historical data
        if (includeBeta) {
          const benchmark = this.getNodeParameter('benchmark', 0) as string;
          result.benchmark = benchmark;
          try {
            const [symHistory, benchHistory] = await Promise.all([
              MarketDataBridge.getHistoricalData({ symbol, period: '1y' }),
              MarketDataBridge.getHistoricalData({ symbol: benchmark, period: '1y' }),
            ]);

            if (symHistory.length >= 20 && benchHistory.length >= 20) {
              // Align by date (use shorter length)
              const len = Math.min(symHistory.length, benchHistory.length);
              const symReturns: number[] = [];
              const benchReturns: number[] = [];
              for (let i = 1; i < len; i++) {
                if (symHistory[i - 1].close > 0 && benchHistory[i - 1].close > 0) {
                  symReturns.push((symHistory[i].close - symHistory[i - 1].close) / symHistory[i - 1].close);
                  benchReturns.push((benchHistory[i].close - benchHistory[i - 1].close) / benchHistory[i - 1].close);
                }
              }
              if (symReturns.length > 5) {
                const meanB = benchReturns.reduce((a, b) => a + b, 0) / benchReturns.length;
                const meanS = symReturns.reduce((a, b) => a + b, 0) / symReturns.length;
                let cov = 0, varB = 0;
                for (let i = 0; i < symReturns.length; i++) {
                  cov += (symReturns[i] - meanS) * (benchReturns[i] - meanB);
                  varB += Math.pow(benchReturns[i] - meanB, 2);
                }
                result.beta = varB > 0 ? parseFloat((cov / varB).toFixed(3)) : 1.0;
              } else {
                result.beta = null;
              }
            } else {
              result.beta = null;
              result.betaNote = 'Insufficient historical data';
            }
          } catch {
            result.beta = null;
            result.betaNote = 'Failed to calculate beta';
          }
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

  /**
   * Calculate real performance by fetching historical data from MarketDataBridge.
   * Falls back to the 24h stats if historical fetch fails.
   */
  private static async calculatePerformance(stats: any, period: string, symbol: string): Promise<Record<string, any>> {
    // Map UI period codes to MarketDataBridge period strings
    const periodMap: Record<string, string> = {
      '1d': '1d', '1w': '5d', '1m': '1mo', '3m': '3mo',
      '6m': '6mo', 'ytd': '6mo', '1y': '1y', '3y': '2y', '5y': '5y',
    };

    const bridgePeriod = periodMap[period] || '1mo';

    try {
      const history = await MarketDataBridge.getHistoricalData({
        symbol,
        period: bridgePeriod,
      });

      if (history.length >= 2) {
        const startPrice = history[0].close;
        const endPrice = history[history.length - 1].close;
        const returnPct = ((endPrice - startPrice) / startPrice) * 100;

        return {
          startPrice: parseFloat(startPrice.toFixed(2)),
          endPrice: parseFloat(endPrice.toFixed(2)),
          return: parseFloat((endPrice - startPrice).toFixed(2)),
          returnPercent: parseFloat(returnPct.toFixed(2)) + '%',
          dataPoints: history.length,
          period,
        };
      }
    } catch {
      // Fall through to fallback
    }

    // Fallback: use 24h change from stats
    return {
      return: stats.change24h || 0,
      returnPercent: (stats.changePercent24h || 0) + '%',
      period,
      note: 'Fallback to 24h data',
    };
  }

  private static estimateVolatility(stats: any, period: string): number {
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

  private static calculateTechnicalLevels(stats: any): Record<string, any> {
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
