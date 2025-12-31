/**
 * Statistics Calculator
 *
 * Calculates accurate trading statistics including:
 * - Win rate (% of profitable trades)
 * - Profit factor (gross profit / gross loss)
 * - Sharpe ratio (risk-adjusted returns)
 * - Maximum drawdown
 * - Average holding period
 * - Expectancy
 */

import type { PaperTradingStats } from './types';
import { paperTradingDatabase } from './PaperTradingDatabase';

export class StatisticsCalculator {
  /**
   * Calculate comprehensive trading statistics
   */
  async calculateStatistics(portfolioId: string, initialBalance: number): Promise<PaperTradingStats> {
    const portfolio = await paperTradingDatabase.getPortfolio(portfolioId);
    const positions = await paperTradingDatabase.getPortfolioPositions(portfolioId);
    const trades = await paperTradingDatabase.getPortfolioTrades(portfolioId);

    if (!portfolio) {
      throw new Error(`Portfolio ${portfolioId} not found`);
    }

    // Calculate basic P&L
    const realizedPnL = positions.reduce((sum, p) => sum + p.realizedPnl, 0);
    const unrealizedPnL = positions
      .filter(p => p.status === 'open')
      .reduce((sum, p) => sum + (p.unrealizedPnl || 0), 0);
    const totalPnL = realizedPnL + unrealizedPnL;

    // Calculate total fees
    const totalFees = trades.reduce((sum, t) => sum + t.fee, 0);

    // Analyze closed positions for win/loss statistics
    const closedPositions = positions.filter(p => p.status === 'closed' && p.realizedPnl !== 0);

    const winningTrades = closedPositions.filter(p => p.realizedPnl > 0);
    const losingTrades = closedPositions.filter(p => p.realizedPnl < 0);

    const totalTrades = closedPositions.length;
    const winRate = totalTrades > 0 ? (winningTrades.length / totalTrades) * 100 : 0;

    // Calculate average win/loss
    const totalWins = winningTrades.reduce((sum, p) => sum + p.realizedPnl, 0);
    const totalLosses = Math.abs(losingTrades.reduce((sum, p) => sum + p.realizedPnl, 0));

    const averageWin = winningTrades.length > 0 ? totalWins / winningTrades.length : 0;
    const averageLoss = losingTrades.length > 0 ? totalLosses / losingTrades.length : 0;

    // Calculate largest win/loss
    const largestWin = winningTrades.length > 0
      ? Math.max(...winningTrades.map(p => p.realizedPnl))
      : 0;
    const largestLoss = losingTrades.length > 0
      ? Math.min(...losingTrades.map(p => p.realizedPnl))
      : 0;

    // Calculate profit factor
    const profitFactor = totalLosses > 0 ? totalWins / totalLosses : totalWins > 0 ? Infinity : 0;

    // Calculate Sharpe ratio (if enough data)
    const sharpeRatio = this.calculateSharpeRatio(closedPositions, portfolio.initialBalance);

    // Calculate maximum drawdown
    const maxDrawdown = await this.calculateMaxDrawdown(portfolioId, initialBalance);

    // Calculate average holding period
    const avgHoldingPeriod = this.calculateAverageHoldingPeriod(closedPositions);

    return {
      portfolioId,
      totalTrades,
      winningTrades: winningTrades.length,
      losingTrades: losingTrades.length,
      winRate,
      totalPnL,
      realizedPnL,
      unrealizedPnL,
      totalFees,
      averageWin,
      averageLoss,
      largestWin,
      largestLoss,
      profitFactor,
      sharpeRatio,
      maxDrawdown,
      avgHoldingPeriod,
    };
  }

  /**
   * Calculate Sharpe ratio (risk-adjusted returns)
   * Sharpe = (Mean Return - Risk Free Rate) / Standard Deviation of Returns
   *
   * We'll use 0% risk-free rate for simplicity
   */
  private calculateSharpeRatio(closedPositions: any[], initialBalance: number): number | null {
    if (closedPositions.length < 2) {
      return null; // Not enough data
    }

    // Calculate returns for each trade
    const returns = closedPositions.map(p => p.realizedPnl / initialBalance);

    // Calculate mean return
    const meanReturn = returns.reduce((sum, r) => sum + r, 0) / returns.length;

    // Calculate standard deviation
    const squaredDiffs = returns.map(r => Math.pow(r - meanReturn, 2));
    const variance = squaredDiffs.reduce((sum, sd) => sum + sd, 0) / squaredDiffs.length;
    const stdDev = Math.sqrt(variance);

    // Handle edge cases for zero volatility
    if (stdDev === 0 || !isFinite(stdDev)) {
      // Zero volatility means all returns are identical
      // Sharpe ratio is undefined in this case - return null instead of Infinity/0
      // (Infinity breaks JSON serialization, 0 is misleading)
      return null;
    }

    // Check for invalid mean return
    if (!isFinite(meanReturn)) {
      return null;
    }

    // Annualize Sharpe ratio (assuming 252 trading days)
    // For simplicity, we'll return the raw Sharpe ratio
    const sharpeRatio = meanReturn / stdDev;

    // Additional safety check for the final result
    if (!isFinite(sharpeRatio)) {
      return null;
    }

    return sharpeRatio;
  }

  /**
   * Calculate maximum drawdown
   * Drawdown = (Peak - Trough) / Peak
   */
  private async calculateMaxDrawdown(portfolioId: string, initialBalance: number): Promise<number | null> {
    const positions = await paperTradingDatabase.getPortfolioPositions(portfolioId);

    if (positions.length === 0) {
      return null;
    }

    // Sort positions by close time
    const sortedPositions = positions
      .filter(p => p.closedAt)
      .sort((a, b) => new Date(a.closedAt!).getTime() - new Date(b.closedAt!).getTime());

    if (sortedPositions.length === 0) {
      return null;
    }

    // Calculate cumulative balance over time
    let cumulativeBalance = initialBalance;
    let peak = initialBalance;
    let maxDrawdown = 0;

    for (const position of sortedPositions) {
      cumulativeBalance += position.realizedPnl;

      // Update peak if we have a new high
      if (cumulativeBalance > peak) {
        peak = cumulativeBalance;
      }

      // Calculate drawdown from peak
      const drawdown = peak > 0 ? (peak - cumulativeBalance) / peak : 0;

      // Update max drawdown
      if (drawdown > maxDrawdown) {
        maxDrawdown = drawdown;
      }
    }

    return maxDrawdown * 100; // Return as percentage
  }

  /**
   * Calculate average holding period in minutes
   */
  private calculateAverageHoldingPeriod(closedPositions: any[]): number | null {
    if (closedPositions.length === 0) {
      return null;
    }

    const holdingPeriods = closedPositions
      .filter(p => p.openedAt && p.closedAt)
      .map(p => {
        const openTime = new Date(p.openedAt).getTime();
        const closeTime = new Date(p.closedAt!).getTime();
        return (closeTime - openTime) / 1000 / 60; // Convert to minutes
      });

    if (holdingPeriods.length === 0) {
      return null;
    }

    const totalMinutes = holdingPeriods.reduce((sum, minutes) => sum + minutes, 0);
    return totalMinutes / holdingPeriods.length;
  }

  /**
   * Calculate expectancy (average P&L per trade)
   */
  calculateExpectancy(
    winRate: number,
    averageWin: number,
    averageLoss: number
  ): number {
    const lossRate = 100 - winRate;
    return (winRate / 100) * averageWin - (lossRate / 100) * averageLoss;
  }

  /**
   * Calculate Kelly Criterion (optimal position sizing)
   */
  calculateKellyCriterion(
    winRate: number,
    averageWin: number,
    averageLoss: number
  ): number {
    if (averageLoss === 0) return 0;

    const winProbability = winRate / 100;
    const lossProbability = 1 - winProbability;
    const winLossRatio = averageWin / averageLoss;

    // Kelly % = (Win% * WinLossRatio - Loss%) / WinLossRatio
    const kelly = (winProbability * winLossRatio - lossProbability) / winLossRatio;

    // Cap at 25% for safety (full Kelly can be aggressive)
    return Math.max(0, Math.min(kelly, 0.25));
  }

  /**
   * Calculate performance metrics summary
   */
  async getPerformanceSummary(portfolioId: string, initialBalance: number): Promise<{
    stats: PaperTradingStats;
    expectancy: number;
    kellyCriterion: number;
    returnPercent: number;
    riskRewardRatio: number;
  }> {
    const stats = await this.calculateStatistics(portfolioId, initialBalance);

    const expectancy = this.calculateExpectancy(stats.winRate, stats.averageWin, stats.averageLoss);
    const kellyCriterion = this.calculateKellyCriterion(stats.winRate, stats.averageWin, stats.averageLoss);
    const returnPercent = (stats.totalPnL / initialBalance) * 100;
    const riskRewardRatio = stats.averageLoss > 0 ? stats.averageWin / stats.averageLoss : 0;

    return {
      stats,
      expectancy,
      kellyCriterion,
      returnPercent,
      riskRewardRatio,
    };
  }
}
