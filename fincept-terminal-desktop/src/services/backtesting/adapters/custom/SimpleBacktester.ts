/**
 * Simple Custom Backtester
 *
 * Minimal backtesting engine for educational purposes.
 * Demonstrates how easy it is to add new providers to the platform.
 */

import { BaseBacktestingAdapter } from '../BaseAdapter';
import {
  ProviderCapabilities,
  ProviderConfig,
  InitResult,
  TestResult,
  BacktestRequest,
  BacktestResult,
  BacktestStatus,
  DataRequest,
  HistoricalData,
  IndicatorType,
  IndicatorParams,
  IndicatorResult,
  DataCatalog,
  IndicatorCatalog,
  ValidationResult,
  HealthStatus,
  PerformanceMetrics,
  BacktestStatistics,
} from '../../interfaces/types';
import { StrategyDefinition } from '../../interfaces/IStrategyDefinition';

const SIMPLE_CAPABILITIES: ProviderCapabilities = {
  backtesting: true,
  optimization: false,
  liveTrading: false,
  research: false,

  multiAsset: ['stocks'],

  indicators: false,
  customStrategies: true,
  maxConcurrentBacktests: 1,

  supportedTimeframes: ['daily'],
};

/**
 * Simple Backtester Adapter
 *
 * Ultra-simple backtesting engine that runs entirely in TypeScript.
 * No external dependencies required - perfect for learning and testing.
 */
export class SimpleBacktester extends BaseBacktestingAdapter {
  readonly name = 'Simple Backtester';
  readonly version = '1.0.0';
  readonly capabilities = SIMPLE_CAPABILITIES;
  readonly description = 'Simple in-memory backtester for education and testing';

  async initialize(config: ProviderConfig): Promise<InitResult> {
    this.config = config;
    this.initialized = true;
    this.connected = true;

    return this.createSuccessResult('Simple Backtester initialized');
  }

  async testConnection(): Promise<TestResult> {
    return {
      success: true,
      message: 'Simple Backtester is always available (in-memory)',
      latency: 0,
    };
  }

  async disconnect(): Promise<void> {
    this.connected = false;
    this.initialized = false;
  }

  async runBacktest(request: BacktestRequest): Promise<BacktestResult> {
    this.ensureConnected();

    const backtestId = this.generateId();

    try {
      // Simple buy-and-hold backtest
      const startDate = new Date(request.startDate);
      const endDate = new Date(request.endDate);
      const days = Math.ceil((endDate.getTime() - startDate.getTime()) / (1000 * 60 * 60 * 24));

      // Simulate price movement (random walk)
      const prices: number[] = [100]; // Start at $100
      const returns: number[] = [0];

      for (let i = 1; i < days; i++) {
        // Random daily return between -2% and +2%
        const dailyReturn = (Math.random() - 0.5) * 0.04;
        const newPrice = prices[i - 1] * (1 + dailyReturn);
        prices.push(newPrice);
        returns.push(dailyReturn);
      }

      // Calculate metrics
      const finalPrice = prices[prices.length - 1];
      const totalReturn = (finalPrice - 100) / 100;

      const avgReturn = returns.reduce((a, b) => a + b, 0) / returns.length;
      const stdDev = Math.sqrt(
        returns.reduce((sum, r) => sum + Math.pow(r - avgReturn, 2), 0) / returns.length
      );
      const sharpeRatio = avgReturn / stdDev * Math.sqrt(252); // Annualized

      const maxDrawdown = this.calculateMaxDrawdown(prices);

      const performance: PerformanceMetrics = {
        totalReturn: totalReturn,
        annualizedReturn: Math.pow(1 + totalReturn, 365 / days) - 1,
        sharpeRatio: sharpeRatio,
        sortinoRatio: sharpeRatio * 0.9, // Approximation
        maxDrawdown: maxDrawdown,
        winRate: returns.filter((r) => r > 0).length / returns.length,
        lossRate: returns.filter((r) => r < 0).length / returns.length,
        profitFactor: 1.5,
        volatility: stdDev * Math.sqrt(252),
        calmarRatio: totalReturn / Math.abs(maxDrawdown),
        totalTrades: 1,
        winningTrades: totalReturn > 0 ? 1 : 0,
        losingTrades: totalReturn < 0 ? 1 : 0,
        averageWin: totalReturn > 0 ? totalReturn : 0,
        averageLoss: totalReturn < 0 ? totalReturn : 0,
        largestWin: totalReturn > 0 ? totalReturn : 0,
        largestLoss: totalReturn < 0 ? totalReturn : 0,
        averageTradeReturn: totalReturn,
        expectancy: totalReturn,
      };

      const statistics: BacktestStatistics = {
        startDate: request.startDate,
        endDate: request.endDate,
        initialCapital: request.initialCapital,
        finalCapital: request.initialCapital * (1 + totalReturn),
        totalFees: 0,
        totalSlippage: 0,
        totalTrades: 1,
        winningDays: returns.filter((r) => r > 0).length,
        losingDays: returns.filter((r) => r < 0).length,
        averageDailyReturn: avgReturn,
        bestDay: Math.max(...returns),
        worstDay: Math.min(...returns),
        consecutiveWins: 0,
        consecutiveLosses: 0,
      };

      // Equity curve
      const equity = prices.map((price, i) => ({
        date: new Date(startDate.getTime() + i * 24 * 60 * 60 * 1000).toISOString(),
        equity: request.initialCapital * (price / 100),
        returns: returns[i],
        drawdown: this.calculateDrawdown(prices.slice(0, i + 1)),
      }));

      return {
        id: backtestId,
        status: 'completed',
        performance,
        trades: [
          {
            id: 'trade_1',
            symbol: request.assets[0]?.symbol || 'SPY',
            entryDate: request.startDate,
            side: 'long',
            quantity: request.initialCapital / 100,
            entryPrice: 100,
            commission: 0,
            slippage: 0,
            exitDate: request.endDate,
            exitPrice: finalPrice,
            pnl: request.initialCapital * totalReturn,
            pnlPercent: totalReturn,
            holdingPeriod: days,
            exitReason: 'signal',
          },
        ],
        equity,
        statistics,
        logs: [
          `${new Date().toISOString()}: Started simple backtest`,
          `${new Date().toISOString()}: Simulated ${days} days of trading`,
          `${new Date().toISOString()}: Final return: ${(totalReturn * 100).toFixed(2)}%`,
          `${new Date().toISOString()}: Backtest completed`,
        ],
        startTime: new Date().toISOString(),
        endTime: new Date().toISOString(),
        duration: 100, // 100ms
      };
    } catch (error) {
      return {
        id: backtestId,
        status: 'failed',
        error: String(error),
        performance: this.getEmptyMetrics(),
        trades: [],
        equity: [],
        statistics: this.getEmptyStatistics(),
        logs: [`Error: ${error}`],
      };
    }
  }

  async getBacktestStatus(id: string): Promise<BacktestStatus> {
    return {
      id,
      status: 'completed',
      progress: 100,
    };
  }

  async cancelBacktest(id: string): Promise<void> {
    // Nothing to cancel - runs instantly
  }

  async getHistoricalData(request: DataRequest): Promise<HistoricalData[]> {
    return [];
  }

  async getAvailableData(): Promise<DataCatalog> {
    return {
      providers: ['simulated'],
      assetClasses: ['stocks'],
      timeframes: ['daily'],
      startDate: '2000-01-01',
      endDate: new Date().toISOString().split('T')[0],
      totalSymbols: 0,
      description: 'Simple backtester generates random price data for testing',
    };
  }

  async calculateIndicator(type: IndicatorType, params: IndicatorParams): Promise<IndicatorResult> {
    throw new Error('Indicators not supported by Simple Backtester');
  }

  async getAvailableIndicators(): Promise<IndicatorCatalog> {
    return {
      indicators: [],
    };
  }

  async validateStrategy(strategy: StrategyDefinition): Promise<{ valid: boolean; errors: string[]; warnings: string[] }> {
    return {
      valid: true,
      errors: [],
      warnings: ['Simple Backtester ignores strategy code and runs buy-and-hold simulation'],
    };
  }

  async getHealth(): Promise<HealthStatus> {
    return { status: 'healthy', message: 'Always available' };
  }

  // Helper methods
  private calculateMaxDrawdown(prices: number[]): number {
    let maxDD = 0;
    let peak = prices[0];

    for (const price of prices) {
      if (price > peak) {
        peak = price;
      }
      const dd = (price - peak) / peak;
      if (dd < maxDD) {
        maxDD = dd;
      }
    }

    return maxDD;
  }

  private calculateDrawdown(prices: number[]): number {
    if (prices.length === 0) return 0;
    const peak = Math.max(...prices);
    const current = prices[prices.length - 1];
    return (current - peak) / peak;
  }

  private getEmptyMetrics(): PerformanceMetrics {
    return {
      totalReturn: 0,
      annualizedReturn: 0,
      sharpeRatio: 0,
      sortinoRatio: 0,
      maxDrawdown: 0,
      winRate: 0,
      lossRate: 0,
      profitFactor: 0,
      volatility: 0,
      calmarRatio: 0,
      totalTrades: 0,
      winningTrades: 0,
      losingTrades: 0,
      averageWin: 0,
      averageLoss: 0,
      largestWin: 0,
      largestLoss: 0,
      averageTradeReturn: 0,
      expectancy: 0,
    };
  }

  private getEmptyStatistics(): BacktestStatistics {
    return {
      startDate: '',
      endDate: '',
      initialCapital: 0,
      finalCapital: 0,
      totalFees: 0,
      totalSlippage: 0,
      totalTrades: 0,
      winningDays: 0,
      losingDays: 0,
      averageDailyReturn: 0,
      bestDay: 0,
      worstDay: 0,
      consecutiveWins: 0,
      consecutiveLosses: 0,
    };
  }
}
