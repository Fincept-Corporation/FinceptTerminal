import { invoke } from '@tauri-apps/api/core';

// ============================================================================
// Types
// ============================================================================

export interface QuantStatsMetrics {
  cagr: number | null;
  cumulative_return: number | null;
  sharpe: number | null;
  sortino: number | null;
  calmar: number | null;
  max_drawdown: number | null;
  avg_drawdown: number | null;
  volatility: number | null;
  win_rate: number | null;
  avg_win: number | null;
  avg_loss: number | null;
  best_day: number | null;
  worst_day: number | null;
  best_month: number | null;
  worst_month: number | null;
  profit_factor: number | null;
  payoff_ratio: number | null;
  skew: number | null;
  kurtosis: number | null;
  kelly_criterion: number | null;
  risk_of_ruin: number | null;
  tail_ratio: number | null;
  common_sense_ratio: number | null;
  outlier_win_ratio: number | null;
  outlier_loss_ratio: number | null;
  smart_sharpe: number | null;
  smart_sortino: number | null;
  adjusted_sortino: number | null;
  omega: number | null;
  ulcer_index: number | null;
  upi: number | null;
  serenity_index: number | null;
  risk_return_ratio: number | null;
  recovery_factor: number | null;
  cpc_index: number | null;
  exposure: number | null;
  consecutive_wins: number | null;
  consecutive_losses: number | null;
  expected_return: number | null;
  probabilistic_sharpe_ratio: number | null;
  rar: number | null;
  r_squared: number | null;
  var_95: number | null;
  cvar_95: number | null;
  var_99: number | null;
  max_drawdown_duration: number | null;
  alpha: number | null;
  beta: number | null;
  information_ratio: number | null;
  treynor_ratio: number | null;
  benchmark_cagr: number | null;
  benchmark_sharpe: number | null;
  benchmark_volatility: number | null;
  benchmark_max_drawdown: number | null;
  correlation: number | null;
  excess_return: number | null;
}

export interface MonthlyReturn {
  year: number;
  month: number;
  return: number | null;
}

export interface YearlyReturn {
  year: number;
  return: number | null;
}

export interface DistributionStats {
  mean: number | null;
  std: number | null;
  skew: number | null;
  kurtosis: number | null;
  min: number | null;
  max: number | null;
  median: number | null;
  positive_days: number;
  negative_days: number;
  total_days: number;
}

export interface ReturnsAnalysis {
  monthly_returns: MonthlyReturn[];
  yearly_returns: YearlyReturn[];
  distribution: DistributionStats;
  heatmap: Record<string, Record<string, number | null>>;
}

export interface DrawdownPoint {
  date: string;
  drawdown: number | null;
}

export interface DrawdownPeriod {
  [key: string]: string | number | null;
}

export interface DrawdownAnalysis {
  drawdown_series: DrawdownPoint[];
  drawdown_periods: DrawdownPeriod[];
}

export interface TimeSeriesPoint {
  date: string;
  value: number | null;
}

export interface RollingAnalysis {
  rolling_sharpe_21d?: TimeSeriesPoint[];
  rolling_sharpe_63d?: TimeSeriesPoint[];
  rolling_sharpe_126d?: TimeSeriesPoint[];
  rolling_sharpe_252d?: TimeSeriesPoint[];
  rolling_volatility_21d?: TimeSeriesPoint[];
  rolling_volatility_63d?: TimeSeriesPoint[];
  rolling_volatility_126d?: TimeSeriesPoint[];
  rolling_volatility_252d?: TimeSeriesPoint[];
  rolling_sortino_21d?: TimeSeriesPoint[];
  rolling_sortino_63d?: TimeSeriesPoint[];
  rolling_sortino_126d?: TimeSeriesPoint[];
  rolling_sortino_252d?: TimeSeriesPoint[];
  cumulative_returns: TimeSeriesPoint[];
  benchmark_cumulative_returns?: TimeSeriesPoint[];
}

export interface MonteCarloDistribution {
  mean: number | null;
  std: number | null;
  p5: number | null;
  p10: number | null;
  p25: number | null;
  p50: number | null;
  p75: number | null;
  p90: number | null;
  p95: number | null;
  min: number | null;
  max: number | null;
  count: number | null;
}

export interface MonteCarloSimulationPaths {
  paths: (number | null)[][];
  time_steps: number;
  num_paths: number;
  p5_band: (number | null)[];
  p50_band: (number | null)[];
  p95_band: (number | null)[];
  original_path: (number | null)[] | null;
}

export interface MonteCarloAnalysis {
  sharpe_distribution: MonteCarloDistribution | null;
  max_drawdown_distribution: MonteCarloDistribution | null;
  cagr_distribution: MonteCarloDistribution | null;
  wealth_distribution: MonteCarloDistribution | null;
  simulation_paths: MonteCarloSimulationPaths | null;
}

export interface FullReport {
  stats: QuantStatsMetrics;
  returns: ReturnsAnalysis;
  drawdown: DrawdownAnalysis;
  rolling: RollingAnalysis;
  montecarlo: MonteCarloAnalysis;
}

export interface QuantStatsResponse<T> {
  success: boolean;
  action: string;
  data: T;
  error?: string;
}

// ============================================================================
// Service
// ============================================================================

async function runQuantStats<T>(
  tickersWeights: Record<string, number>,
  action: string,
  benchmark: string = 'SPY',
  period: string = '1y',
  riskFreeRate: number = 0.02,
  numSims: number = 1000
): Promise<T> {
  let result: string;
  try {
    result = await invoke<string>('run_quantstats_analysis', {
      tickersJson: JSON.stringify(tickersWeights),
      action,
      benchmark,
      period,
      riskFreeRate,
      numSims,
    });
  } catch (e) {
    // Rust command returned Err - extract useful message
    const msg = typeof e === 'string' ? e : (e instanceof Error ? e.message : String(e));
    // Check for common issues
    if (msg.includes('No module named')) {
      throw new Error('QuantStats not installed. Run setup to install Python packages.');
    }
    if (msg.includes('Script not found') || msg.includes('not found')) {
      throw new Error(`Python setup issue: ${msg}`);
    }
    throw new Error(msg);
  }

  let parsed: QuantStatsResponse<T>;
  try {
    parsed = JSON.parse(result);
  } catch {
    throw new Error(`Invalid response from QuantStats: ${result.slice(0, 200)}`);
  }

  if (parsed.error) {
    throw new Error(parsed.error);
  }

  return parsed.data;
}

export const quantstatsService = {
  getStats: (tickers: Record<string, number>, benchmark?: string, period?: string, rf?: number) =>
    runQuantStats<QuantStatsMetrics>(tickers, 'stats', benchmark, period, rf),

  getReturnsAnalysis: (tickers: Record<string, number>, benchmark?: string, period?: string) =>
    runQuantStats<ReturnsAnalysis>(tickers, 'returns', benchmark, period),

  getDrawdowns: (tickers: Record<string, number>, benchmark?: string, period?: string) =>
    runQuantStats<DrawdownAnalysis>(tickers, 'drawdown', benchmark, period),

  getRolling: (tickers: Record<string, number>, benchmark?: string, period?: string, rf?: number) =>
    runQuantStats<RollingAnalysis>(tickers, 'rolling', benchmark, period, rf),

  getFullReport: (tickers: Record<string, number>, benchmark?: string, period?: string, rf?: number, numSims?: number) =>
    runQuantStats<FullReport>(tickers, 'full_report', benchmark, period, rf, numSims),

  getMonteCarlo: (tickers: Record<string, number>, benchmark?: string, period?: string, numSims?: number) =>
    runQuantStats<MonteCarloAnalysis>(tickers, 'montecarlo', benchmark, period, undefined, numSims),

  getHtmlReport: (tickers: Record<string, number>, benchmark?: string, period?: string) =>
    runQuantStats<{ html_base64: string }>(tickers, 'html_report', benchmark, period),
};
