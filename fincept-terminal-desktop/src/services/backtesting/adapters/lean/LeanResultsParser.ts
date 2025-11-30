/**
 * Lean Results Parser
 *
 * Parses Lean backtest results from JSON output into platform-independent format.
 * Converts Lean-specific structures to our BacktestResult interface.
 */

import {
  BacktestResult,
  PerformanceMetrics,
  Trade,
  EquityPoint,
  BacktestStatistics,
  ChartData,
} from '../../interfaces/types';
import {
  LeanRawBacktestResult,
  LeanRawOptimizationResult,
  LeanChartSeries,
  LeanOrder,
} from './types';

/**
 * LeanResultsParser
 *
 * Converts Lean CLI output to platform-independent BacktestResult.
 */
export class LeanResultsParser {
  /**
   * Parse complete backtest result from Lean JSON output
   */
  parseBacktestResult(
    backtestId: string,
    leanResult: LeanRawBacktestResult,
    logs: string[]
  ): BacktestResult {
    try {
      return {
        id: backtestId,
        status: 'completed',
        performance: this.parsePerformanceMetrics(leanResult),
        trades: this.parseTrades(leanResult),
        equity: this.parseEquityCurve(leanResult),
        statistics: this.parseStatistics(leanResult),
        charts: this.parseCharts(leanResult),
        logs,
        startTime: this.extractStatistic(leanResult, 'Start'),
        endTime: this.extractStatistic(leanResult, 'End'),
        duration: this.calculateDuration(leanResult),
      };
    } catch (error) {
      return this.createErrorResult(backtestId, String(error), logs);
    }
  }

  /**
   * Parse performance metrics
   */
  private parsePerformanceMetrics(result: LeanRawBacktestResult): PerformanceMetrics {
    const stats = result.TotalPerformance?.PortfolioStatistics || {};
    const tradeStats = result.TotalPerformance?.TradeStatistics || {};

    return {
      // Returns
      totalReturn: this.parsePercent(stats.TotalReturn) || 0,
      annualizedReturn: this.parsePercent(stats.AnnualReturn) || 0,

      // Risk Metrics
      sharpeRatio: this.parseFloat(stats.SharpeRatio) || 0,
      sortinoRatio: this.parseFloat(stats.SortinoRatio) || 0,
      maxDrawdown: this.parsePercent(stats.MaxDrawdown) || 0,
      volatility: this.parsePercent(stats.AnnualStandardDeviation) || 0,

      // Trade Statistics
      winRate: this.parsePercent(tradeStats.WinRate) || 0,
      lossRate: this.parsePercent(tradeStats.LossRate) || 0,
      totalTrades: Number(tradeStats.TotalNumberOfTrades) || 0,
      winningTrades: this.calculateWinningTrades(tradeStats),
      losingTrades: this.calculateLosingTrades(tradeStats),

      // Win/Loss Metrics
      averageWin: this.parseFloat(tradeStats.AverageWin) || 0,
      averageLoss: Math.abs(this.parseFloat(tradeStats.AverageLoss) || 0),
      largestWin: this.parseFloat(tradeStats.LargestWin) || 0,
      largestLoss: Math.abs(this.parseFloat(tradeStats.LargestLoss) || 0),

      // Other Metrics
      profitFactor: this.parseFloat(tradeStats.ProfitLossRatio) || 0,
      averageTradeReturn: this.parseFloat(tradeStats.AverageProfit) || 0,
      expectancy: this.calculateExpectancy(tradeStats),

      // Advanced (optional)
      calmarRatio: this.calculateCalmarRatio(stats),
      alpha: this.parseFloat(stats.Alpha),
      beta: this.parseFloat(stats.Beta),
      informationRatio: this.parseFloat(stats.InformationRatio),
      treynorRatio: this.parseFloat(stats.TreynorRatio),
    };
  }

  /**
   * Parse trades from Lean orders
   */
  private parseTrades(result: LeanRawBacktestResult): Trade[] {
    if (!result.Orders) return [];

    const orders = Object.values(result.Orders);
    const trades: Trade[] = [];

    // Group orders into trades (entry + exit pairs)
    // Simplified - Lean has complex order tracking
    // Full implementation would match entries and exits properly

    for (const order of orders) {
      if (order.Status !== 'Filled') continue;

      trades.push({
        id: `trade_${order.Id}`,
        symbol: order.Symbol,
        entryDate: order.Time,
        side: order.Quantity > 0 ? 'long' : 'short',
        quantity: Math.abs(order.Quantity),
        entryPrice: order.Price,
        commission: 0, // Would need to extract from Lean
        slippage: 0,
        // Exit fields would be filled when matching exit order
        exitDate: undefined,
        exitPrice: undefined,
        pnl: undefined,
        pnlPercent: undefined,
        holdingPeriod: undefined,
        exitReason: undefined,
      });
    }

    return trades;
  }

  /**
   * Parse equity curve
   */
  private parseEquityCurve(result: LeanRawBacktestResult): EquityPoint[] {
    if (!result.Charts || !result.Charts['Strategy Equity']) {
      return [];
    }

    const equityChart = result.Charts['Strategy Equity'];
    const equitySeries = equityChart.Series['Equity'];

    if (!equitySeries || !equitySeries.Values) {
      return [];
    }

    const points: EquityPoint[] = [];
    let previousEquity = 0;

    for (const point of equitySeries.Values) {
      const equity = point.y;
      const returns = previousEquity > 0 ? (equity - previousEquity) / previousEquity : 0;

      points.push({
        date: new Date(point.x * 1000).toISOString(), // Unix timestamp to ISO
        equity,
        returns,
        drawdown: this.calculateDrawdown(equity, points),
        benchmark: this.extractBenchmark(result, point.x),
      });

      previousEquity = equity;
    }

    return points;
  }

  /**
   * Parse general statistics
   */
  private parseStatistics(result: LeanRawBacktestResult): BacktestStatistics {
    const stats = result.Statistics || [];
    const runtimeStats = result.RuntimeStatistics || [];

    return {
      startDate: this.extractStatistic(result, 'Start') || '',
      endDate: this.extractStatistic(result, 'End') || '',
      initialCapital: this.parseFloat(this.extractStatistic(result, 'Equity')) || 100000,
      finalCapital:
        this.parseFloat(this.extractStatistic(result, 'Net Profit')) || 100000,
      totalFees: this.parseFloat(this.extractStatistic(result, 'Fees')) || 0,
      totalSlippage: 0, // Would extract from Lean if available
      totalTrades: this.parseFloat(this.extractStatistic(result, 'Total Trades')) || 0,
      winningDays: 0, // Calculate from equity curve
      losingDays: 0,
      averageDailyReturn: 0,
      bestDay: 0,
      worstDay: 0,
      consecutiveWins: 0,
      consecutiveLosses: 0,
      maxDrawdownDate: undefined,
      recoveryTime: undefined,
    };
  }

  /**
   * Parse charts for visualization
   */
  private parseCharts(result: LeanRawBacktestResult): ChartData[] | undefined {
    if (!result.Charts) return undefined;

    const charts: ChartData[] = [];

    for (const [chartName, chartData] of Object.entries(result.Charts)) {
      for (const [seriesName, seriesData] of Object.entries(chartData.Series)) {
        charts.push({
          type: this.mapChartType(chartData.ChartType) as any,
          title: `${chartName} - ${seriesName}`,
          data: seriesData.Values.map((v) => ({
            x: new Date(v.x * 1000).toISOString(),
            y: v.y,
          })),
        });
      }
    }

    return charts;
  }

  /**
   * Parse optimization result
   */
  parseOptimizationResult(
    optimizationId: string,
    results: LeanRawOptimizationResult[],
    logs: string[]
  ): any {
    if (!results || results.length === 0) {
      throw new Error('No optimization results found');
    }

    // Find best result (highest Sharpe Ratio or other metric)
    const bestResult = this.findBestOptimizationResult(results);

    return {
      id: optimizationId,
      status: 'completed',
      best_parameters: bestResult.ParameterSet,
      best_result: bestResult, // Would parse to full BacktestResult
      all_results: results.map((r) => ({
        parameters: r.ParameterSet,
        statistics: this.parseStatisticsArray(r.Statistics),
      })),
      iterations: results.length,
      duration: 0, // Calculate from timestamps
      error: undefined,
    };
  }

  /**
   * Create error result
   */
  private createErrorResult(
    backtestId: string,
    error: string,
    logs: string[]
  ): BacktestResult {
    return {
      id: backtestId,
      status: 'failed',
      error,
      logs,
      performance: this.getEmptyMetrics(),
      trades: [],
      equity: [],
      statistics: this.getEmptyStatistics(),
    };
  }

  // ============================================================================
  // Helper Methods
  // ============================================================================

  private parseFloat(value: any): number | undefined {
    if (value === null || value === undefined) return undefined;
    const num = typeof value === 'string' ? parseFloat(value) : Number(value);
    return isNaN(num) ? undefined : num;
  }

  private parsePercent(value: any): number {
    if (!value) return 0;
    const str = String(value).replace('%', '');
    return parseFloat(str) / 100;
  }

  private extractStatistic(result: LeanRawBacktestResult, key: string): string | undefined {
    const stat = result.Statistics?.find((s) => s.Key === key);
    return stat?.Value;
  }

  private parseStatisticsArray(stats: Array<{ Key: string; Value: string }>): any {
    const parsed: any = {};
    for (const stat of stats) {
      parsed[stat.Key] = stat.Value;
    }
    return parsed;
  }

  private calculateWinningTrades(tradeStats: any): number {
    const total = Number(tradeStats.TotalNumberOfTrades) || 0;
    const winRate = this.parsePercent(tradeStats.WinRate) || 0;
    return Math.round(total * winRate);
  }

  private calculateLosingTrades(tradeStats: any): number {
    const total = Number(tradeStats.TotalNumberOfTrades) || 0;
    const lossRate = this.parsePercent(tradeStats.LossRate) || 0;
    return Math.round(total * lossRate);
  }

  private calculateExpectancy(tradeStats: any): number {
    const avgWin = this.parseFloat(tradeStats.AverageWin) || 0;
    const avgLoss = Math.abs(this.parseFloat(tradeStats.AverageLoss) || 0);
    const winRate = this.parsePercent(tradeStats.WinRate) || 0;
    const lossRate = this.parsePercent(tradeStats.LossRate) || 0;

    return winRate * avgWin - lossRate * avgLoss;
  }

  private calculateCalmarRatio(stats: any): number {
    const annualReturn = this.parsePercent(stats.AnnualReturn) || 0;
    const maxDrawdown = Math.abs(this.parsePercent(stats.MaxDrawdown) || 1);
    return maxDrawdown !== 0 ? annualReturn / maxDrawdown : 0;
  }

  private calculateDrawdown(currentEquity: number, points: EquityPoint[]): number {
    if (points.length === 0) return 0;

    const maxEquity = Math.max(...points.map((p) => p.equity), currentEquity);
    return maxEquity > 0 ? (currentEquity - maxEquity) / maxEquity : 0;
  }

  private extractBenchmark(result: LeanRawBacktestResult, timestamp: number): number | undefined {
    // Extract benchmark data if available in charts
    const benchmarkChart = result.Charts?.['Benchmark'];
    if (!benchmarkChart) return undefined;

    // Would find closest benchmark point to timestamp
    return undefined;
  }

  private calculateDuration(result: LeanRawBacktestResult): number | undefined {
    const start = this.extractStatistic(result, 'Start');
    const end = this.extractStatistic(result, 'End');

    if (!start || !end) return undefined;

    const startTime = new Date(start).getTime();
    const endTime = new Date(end).getTime();

    return endTime - startTime;
  }

  private findBestOptimizationResult(
    results: LeanRawOptimizationResult[]
  ): LeanRawOptimizationResult {
    // Find result with highest Sharpe Ratio
    let best = results[0];
    let bestSharpe = -Infinity;

    for (const result of results) {
      const sharpe =
        this.parseFloat(result.Statistics.find((s) => s.Key === 'Sharpe Ratio')?.Value) || 0;
      if (sharpe > bestSharpe) {
        bestSharpe = sharpe;
        best = result;
      }
    }

    return best;
  }

  private mapChartType(leanType: string): 'line' | 'candlestick' | 'bar' | 'area' {
    const map: Record<string, any> = {
      Line: 'line',
      Stock: 'candlestick',
      Scatter: 'line',
      Pie: 'bar',
    };
    return map[leanType] || 'line';
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
