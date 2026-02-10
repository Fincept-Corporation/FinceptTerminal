/**
 * Metric formatting utilities for backtesting results
 */

import { FINCEPT } from '../constants';

export function formatMetricValue(key: string, value: any): string {
  if (value === null || value === undefined) return 'N/A';

  const percentageKeys = ['totalReturn', 'annualizedReturn', 'maxDrawdown', 'winRate', 'lossRate', 'volatility'];
  if (percentageKeys.includes(key)) return `${(value * 100).toFixed(2)}%`;

  const ratioKeys = ['sharpeRatio', 'sortinoRatio', 'calmarRatio', 'profitFactor', 'informationRatio', 'treynorRatio'];
  if (ratioKeys.includes(key)) return value.toFixed(2);

  const countKeys = ['totalTrades', 'winningTrades', 'losingTrades', 'maxDrawdownDuration'];
  if (countKeys.includes(key)) return Math.round(value).toString();

  const currencyKeys = ['averageWin', 'averageLoss', 'largestWin', 'largestLoss', 'averageTradeReturn', 'expectancy'];
  if (currencyKeys.includes(key)) return `$${value.toFixed(2)}`;

  if (key === 'alpha' || key === 'beta') return value.toFixed(2);

  return typeof value === 'number' ? value.toFixed(2) : String(value);
}

export function getMetricColor(key: string, value: any): string {
  if (value === null || value === undefined) return FINCEPT.MUTED;

  const directionalKeys = ['totalReturn', 'annualizedReturn', 'sharpeRatio', 'sortinoRatio', 'calmarRatio', 'profitFactor', 'alpha'];
  if (directionalKeys.includes(key)) {
    return value > 0 ? FINCEPT.GREEN : value < 0 ? FINCEPT.RED : FINCEPT.GRAY;
  }

  if (key === 'maxDrawdown') return value > 0 ? FINCEPT.RED : FINCEPT.GRAY;
  if (key === 'winRate') return value > 0.5 ? FINCEPT.GREEN : value > 0 ? FINCEPT.YELLOW : FINCEPT.RED;

  return FINCEPT.CYAN;
}

export function getMetricDescription(key: string): string {
  const descriptions: Record<string, string> = {
    totalReturn: 'Total percentage gain/loss',
    annualizedReturn: 'Return normalized to yearly basis',
    sharpeRatio: 'Risk-adjusted return (>1 is good)',
    sortinoRatio: 'Downside risk-adjusted return',
    maxDrawdown: 'Largest peak-to-trough decline',
    winRate: 'Percentage of winning trades',
    lossRate: 'Percentage of losing trades',
    profitFactor: 'Gross profit / gross loss',
    volatility: 'Standard deviation of returns',
    calmarRatio: 'Return / max drawdown',
    totalTrades: 'Number of completed trades',
    winningTrades: 'Number of profitable trades',
    losingTrades: 'Number of losing trades',
    averageWin: 'Average profit per winning trade',
    averageLoss: 'Average loss per losing trade',
    largestWin: 'Biggest single winning trade',
    largestLoss: 'Biggest single losing trade',
    averageTradeReturn: 'Mean return across all trades',
    expectancy: 'Expected profit per trade',
    alpha: 'Excess return vs benchmark',
    beta: 'Volatility vs benchmark',
    maxDrawdownDuration: 'Longest drawdown period in bars',
    informationRatio: 'Risk-adjusted excess return',
    treynorRatio: 'Return per unit of systematic risk',
  };
  return descriptions[key] || '';
}
