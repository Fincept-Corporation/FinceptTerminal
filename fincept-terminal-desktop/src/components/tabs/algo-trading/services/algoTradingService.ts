import { invoke } from '@tauri-apps/api/core';
import type { AlgoStrategy, AlgoDeployment, AlgoTrade, ScanResult } from '../types';

interface ApiResult<T = unknown> {
  success: boolean;
  data?: T;
  error?: string;
  count?: number;
}

function parseResult<T>(raw: string): ApiResult<T> {
  try {
    return JSON.parse(raw);
  } catch {
    return { success: false, error: 'Failed to parse response' };
  }
}

// Strategy CRUD
export async function saveAlgoStrategy(strategy: {
  id: string;
  name: string;
  description?: string;
  entry_conditions: string;
  exit_conditions: string;
  entry_logic?: string;
  exit_logic?: string;
  stop_loss?: number | null;
  take_profit?: number | null;
  trailing_stop?: number | null;
  trailing_stop_type?: string;
  timeframe?: string;
  symbols?: string;
}): Promise<ApiResult> {
  const raw = await invoke<string>('save_algo_strategy', {
    id: strategy.id,
    name: strategy.name,
    description: strategy.description,
    entryConditions: strategy.entry_conditions,
    exitConditions: strategy.exit_conditions,
    entryLogic: strategy.entry_logic,
    exitLogic: strategy.exit_logic,
    stopLoss: strategy.stop_loss,
    takeProfit: strategy.take_profit,
    trailingStop: strategy.trailing_stop,
    trailingStopType: strategy.trailing_stop_type,
    timeframe: strategy.timeframe,
    symbols: strategy.symbols,
  });
  return parseResult(raw);
}

export async function listAlgoStrategies(): Promise<ApiResult<AlgoStrategy[]>> {
  const raw = await invoke<string>('list_algo_strategies');
  return parseResult(raw);
}

export async function getAlgoStrategy(id: string): Promise<ApiResult<AlgoStrategy>> {
  const raw = await invoke<string>('get_algo_strategy', { id });
  return parseResult(raw);
}

export async function deleteAlgoStrategy(id: string): Promise<ApiResult> {
  const raw = await invoke<string>('delete_algo_strategy', { id });
  return parseResult(raw);
}

// Deployment lifecycle
export async function deployAlgoStrategy(params: {
  strategyId: string;
  symbol: string;
  provider?: string;
  mode?: string;
  timeframe?: string;
  quantity?: number;
  params?: string;
}): Promise<ApiResult<{ deploy_id: string; pid: number }>> {
  const raw = await invoke<string>('deploy_algo_strategy', params);
  return parseResult(raw);
}

export async function stopAlgoDeployment(deployId: string): Promise<ApiResult> {
  const raw = await invoke<string>('stop_algo_deployment', { deployId });
  return parseResult(raw);
}

export async function stopAllAlgoDeployments(): Promise<ApiResult<{ stopped: number }>> {
  const raw = await invoke<string>('stop_all_algo_deployments');
  return parseResult(raw);
}

export async function listAlgoDeployments(): Promise<ApiResult<AlgoDeployment[]>> {
  const raw = await invoke<string>('list_algo_deployments');
  return parseResult(raw);
}

// Trades
export async function getAlgoTrades(
  deploymentId: string,
  limit?: number
): Promise<ApiResult<AlgoTrade[]>> {
  const raw = await invoke<string>('get_algo_trades', { deploymentId, limit });
  return parseResult(raw);
}

// Scanner
export async function runAlgoScan(
  conditions: string,
  symbols: string,
  timeframe?: string,
  provider?: string
): Promise<ApiResult<{ matches: ScanResult[]; scanned: number }>> {
  const raw = await invoke<string>('run_algo_scan', { conditions, symbols, timeframe, provider });
  return parseResult(raw);
}

// Candle cache
export async function getCandleCache(
  symbol: string,
  timeframe?: string,
  limit?: number
): Promise<ApiResult<unknown[]>> {
  const raw = await invoke<string>('get_candle_cache', { symbol, timeframe, limit });
  return parseResult(raw);
}

// Condition evaluation (one-shot preview)
export async function evaluateConditionsOnce(
  conditions: string,
  symbol: string,
  timeframe?: string
): Promise<ApiResult<{ result: boolean; details: unknown[] }>> {
  const raw = await invoke<string>('evaluate_conditions_once', { conditions, symbol, timeframe });
  return parseResult(raw);
}

// Backtest
export async function runAlgoBacktest(params: {
  symbol: string;
  entryConditions: string;
  exitConditions: string;
  timeframe?: string;
  period?: string;
  stopLoss?: number;
  takeProfit?: number;
  initialCapital?: number;
}): Promise<ApiResult<{
  trades: Array<{
    entry_bar: number;
    exit_bar: number;
    entry_price: number;
    exit_price: number;
    pnl: number;
    pnl_pct: number;
    reason: string;
    bars_held: number;
  }>;
  equity_curve: number[];
  metrics: {
    total_trades: number;
    winning_trades: number;
    losing_trades: number;
    win_rate: number;
    total_return: number;
    total_return_pct: number;
    max_drawdown: number;
    avg_pnl: number;
    avg_bars_held: number;
    profit_factor: number;
    sharpe: number;
  };
}>> {
  const raw = await invoke<string>('run_algo_backtest', params);
  return parseResult(raw);
}

// Candle aggregation control
export async function startCandleAggregation(
  symbol: string,
  timeframe?: string
): Promise<ApiResult> {
  const raw = await invoke<string>('start_candle_aggregation', { symbol, timeframe });
  return parseResult(raw);
}

export async function stopCandleAggregation(
  symbol: string,
  timeframe?: string
): Promise<ApiResult> {
  const raw = await invoke<string>('stop_candle_aggregation', { symbol, timeframe });
  return parseResult(raw);
}

// Order signal bridge
export async function startOrderSignalBridge(): Promise<ApiResult> {
  const raw = await invoke<string>('start_order_signal_bridge');
  return parseResult(raw);
}

export async function stopOrderSignalBridge(): Promise<ApiResult> {
  const raw = await invoke<string>('stop_order_signal_bridge');
  return parseResult(raw);
}
