import { invoke } from '@tauri-apps/api/core';
import type {
  AlgoStrategy,
  AlgoDeployment,
  AlgoTrade,
  ScanResult,
  CandleData,
  PythonStrategy,
  CustomPythonStrategy,
  PythonBacktestRequest,
  PythonBacktestResult,
  StrategyParameter
} from '../types';

interface ApiResult<T = unknown> {
  success: boolean;
  data?: T;
  error?: string;
  count?: number;
  debug?: string[];
  prefetch_warning?: string;
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

export async function deleteAlgoDeployment(deployId: string): Promise<ApiResult> {
  const raw = await invoke<string>('delete_algo_deployment', { deployId });
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
export interface ScanResponse {
  matches: ScanResult[];
  scanned: number;
  errors?: Array<{ symbol: string; error: string }>;
  debug?: string[];
  prefetch_warning?: string;
}

export async function runAlgoScan(
  conditions: string,
  symbols: string,
  timeframe?: string,
  provider?: string,
  lookbackDays?: number
): Promise<ApiResult<ScanResponse>> {
  const raw = await invoke<string>('run_algo_scan', { conditions, symbols, timeframe, provider, lookbackDays });

  // Parse once and normalize the flat scanner JSON into ApiResult<ScanResponse>
  let parsed: Record<string, unknown>;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return { success: false, error: 'Failed to parse scan response' };
  }

  const result: ApiResult<ScanResponse> = {
    success: (parsed.success as boolean) ?? false,
    error: parsed.error as string | undefined,
    debug: parsed.debug as string[] | undefined,
    prefetch_warning: parsed.prefetch_warning as string | undefined,
  };

  // The scanner output is flat JSON with matches/scanned at root level.
  // Normalize into the data property.
  if (result.success) {
    result.data = {
      matches: (parsed.matches || parsed.data && (parsed.data as Record<string, unknown>).matches || []) as ScanResult[],
      scanned: (parsed.scanned || parsed.data && (parsed.data as Record<string, unknown>).scanned || 0) as number,
      errors: (parsed.errors || parsed.data && (parsed.data as Record<string, unknown>).errors) as Array<{ symbol: string; error: string }> | undefined,
      debug: result.debug,
      prefetch_warning: result.prefetch_warning,
    };
  }

  return result;
}

// Candle cache
export async function getCandleCache(
  symbol: string,
  timeframe?: string,
  limit?: number
): Promise<ApiResult<CandleData[]>> {
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
export interface BacktestTrade {
  entry_bar: number;
  exit_bar: number;
  entry_price: number;
  exit_price: number;
  pnl: number;
  pnl_pct: number;
  reason: string;
  bars_held: number;
}

export interface BacktestMetrics {
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
}

export interface BacktestResultData {
  trades: BacktestTrade[];
  equity_curve: number[];
  metrics: BacktestMetrics;
  debug?: string[];
  symbol?: string;
  timeframe?: string;
  period?: string;
  total_bars?: number;
}

export async function runAlgoBacktest(params: {
  symbol: string;
  entryConditions: string;
  exitConditions: string;
  timeframe?: string;
  period?: string;
  stopLoss?: number;
  takeProfit?: number;
  initialCapital?: number;
  provider?: 'yfinance' | 'fyers';
}): Promise<ApiResult<BacktestResultData>> {
  const raw = await invoke<string>('run_algo_backtest', params);

  try {
    const parsed = JSON.parse(raw);

    // The backtest response has a flat structure with success, trades, metrics at root level
    // We need to normalize it to match ApiResult<BacktestResultData> format
    if (parsed.success === true && parsed.trades !== undefined) {
      // Successful backtest - restructure to have data property
      return {
        success: true,
        data: {
          trades: parsed.trades || [],
          equity_curve: parsed.equity_curve || [],
          metrics: parsed.metrics || {},
          debug: parsed.debug,
          symbol: parsed.symbol,
          timeframe: parsed.timeframe,
          period: parsed.period,
          total_bars: parsed.total_bars,
        },
        debug: parsed.debug,
      };
    } else if (parsed.success === false) {
      // Failed backtest
      return {
        success: false,
        error: parsed.error || 'Backtest failed',
        debug: parsed.debug,
      };
    } else {
      // Fallback - try standard parseResult
      return parseResult<BacktestResultData>(raw);
    }
  } catch {
    return { success: false, error: 'Failed to parse backtest response' };
  }
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

// ============================================================================
// PYTHON STRATEGY LIBRARY
// ============================================================================

/** List all Python strategies from the library registry */
export async function listPythonStrategies(): Promise<ApiResult<PythonStrategy[]>> {
  const raw = await invoke<string>('list_python_strategies');
  return parseResult(raw);
}

/** Get all available strategy categories */
export async function getStrategyCategories(): Promise<ApiResult<string[]>> {
  const raw = await invoke<string>('get_strategy_categories');
  return parseResult(raw);
}

/** Get a single Python strategy by ID */
export async function getPythonStrategy(
  strategyId: string
): Promise<ApiResult<PythonStrategy>> {
  const raw = await invoke<string>('get_python_strategy', { id: strategyId });
  return parseResult(raw);
}

/** Get the Python source code for a strategy */
export async function getPythonStrategyCode(
  strategyId: string
): Promise<ApiResult<{ code: string }>> {
  const raw = await invoke<string>('get_python_strategy_code', { id: strategyId });
  return parseResult(raw);
}

// ============================================================================
// CUSTOM PYTHON STRATEGIES (User-modified copies)
// ============================================================================

/** Save a custom (user-modified) Python strategy */
export async function saveCustomPythonStrategy(params: {
  baseStrategyId: string;
  name: string;
  description?: string;
  code: string;
  parameters?: string;
  category?: string;
}): Promise<ApiResult<{ id: string }>> {
  const raw = await invoke<string>('save_custom_python_strategy', {
    baseStrategyId: params.baseStrategyId,
    name: params.name,
    description: params.description || '',
    code: params.code,
    parameters: params.parameters || '{}',
    category: params.category || 'Custom',
  });
  return parseResult(raw);
}

/** List all custom Python strategies */
export async function listCustomPythonStrategies(): Promise<ApiResult<CustomPythonStrategy[]>> {
  const raw = await invoke<string>('list_custom_python_strategies');
  return parseResult(raw);
}

/** Get a single custom Python strategy */
export async function getCustomPythonStrategy(
  id: string
): Promise<ApiResult<CustomPythonStrategy>> {
  const raw = await invoke<string>('get_custom_python_strategy', { id });
  return parseResult(raw);
}

/** Update a custom Python strategy */
export async function updateCustomPythonStrategy(params: {
  id: string;
  name?: string;
  description?: string;
  code?: string;
  parameters?: string;
  category?: string;
  isActive?: boolean;
}): Promise<ApiResult> {
  const raw = await invoke<string>('update_custom_python_strategy', {
    id: params.id,
    name: params.name || null,
    description: params.description || null,
    code: params.code || null,
    parameters: params.parameters || null,
    category: params.category || null,
    isActive: params.isActive !== undefined ? (params.isActive ? 1 : 0) : null,
  });
  return parseResult(raw);
}

/** Delete a custom Python strategy */
export async function deleteCustomPythonStrategy(id: string): Promise<ApiResult> {
  const raw = await invoke<string>('delete_custom_python_strategy', { id });
  return parseResult(raw);
}

/** Validate Python syntax */
export async function validatePythonSyntax(
  code: string
): Promise<ApiResult<{ valid: boolean; error?: string }>> {
  const raw = await invoke<string>('validate_python_syntax', { code });
  return parseResult(raw);
}

// ============================================================================
// PYTHON STRATEGY BACKTEST (Phase 3 - placeholder)
// ============================================================================

/** Run backtest on a Python strategy */
export async function runPythonBacktest(
  request: PythonBacktestRequest
): Promise<ApiResult<PythonBacktestResult>> {
  console.log('[algoTradingService] runPythonBacktest invoke params:', {
    strategyId: request.strategy_id,
    symbol: request.symbol,
    startDate: request.start_date,
    endDate: request.end_date,
    initialCapital: request.initial_capital,
    params: request.params || null,
  });
  const raw = await invoke<string>('run_python_backtest', {
    strategyId: request.strategy_id,
    symbol: request.symbol,
    startDate: request.start_date,
    endDate: request.end_date,
    initialCapital: request.initial_capital,
    params: request.params || null,
  });
  console.log('[algoTradingService] runPythonBacktest raw response:', raw);

  // Rust now passes through the Python engine JSON directly:
  // { success: true, trades: [...], equity_curve: [...], metrics: {...}, debug: [...] }
  // or { success: false, error: "...", debug: [...] }
  try {
    const parsed = JSON.parse(raw) as Record<string, unknown>;
    console.log('[algoTradingService] Parsed backtest response:', parsed);

    if (!parsed.success) {
      const debugInfo = parsed.debug as string[] | undefined;
      if (debugInfo) {
        console.warn('[algoTradingService] Backtest debug log:', debugInfo);
      }
      return { success: false, error: (parsed.error as string) || 'Backtest failed', debug: debugInfo };
    }

    // Map flat response to ApiResult<PythonBacktestResult>
    const data: PythonBacktestResult = {
      success: true,
      trades: (parsed.trades || []) as PythonBacktestResult['trades'],
      equity_curve: (parsed.equity_curve || []) as PythonBacktestResult['equity_curve'],
      metrics: (parsed.metrics || {}) as PythonBacktestResult['metrics'],
      debug: parsed.debug as string[] | undefined,
    };
    return { success: true, data };
  } catch (e) {
    console.error('[algoTradingService] Failed to parse backtest response:', e, raw);
    return { success: false, error: 'Failed to parse backtest response' };
  }
}

/** Extract parameters from Python strategy code */
export async function extractStrategyParameters(
  code: string
): Promise<ApiResult<StrategyParameter[]>> {
  const raw = await invoke<string>('extract_strategy_parameters', { code });
  console.log('[algoTradingService] extractStrategyParameters raw:', raw);

  // Rust returns { success, parameters: [{ name, default, type }] }
  // We need to map to ApiResult<StrategyParameter[]> with default_value field
  try {
    const parsed = JSON.parse(raw) as { success: boolean; parameters?: Array<{ name: string; default: string; type: string }>; error?: string };

    if (parsed.success && parsed.parameters) {
      const mapped: StrategyParameter[] = parsed.parameters.map(p => ({
        name: p.name,
        type: (p.type || 'string') as StrategyParameter['type'],
        default_value: String(p.default ?? ''),
      }));
      console.log('[algoTradingService] Extracted parameters:', mapped);
      return { success: true, data: mapped };
    }

    return { success: false, error: parsed.error || 'No parameters found' };
  } catch {
    return { success: false, error: 'Failed to parse parameter extraction response' };
  }
}

// ============================================================================
// PYTHON STRATEGY DEPLOYMENT (Phase 6 - placeholder)
// ============================================================================

/** Deploy a Python strategy to paper or live trading */
export async function deployPythonStrategy(params: {
  strategyId: string;
  symbol: string;
  mode: 'paper' | 'live';
  broker?: string;
  quantity: number;
  timeframe: string;
  parameters?: Record<string, string>;
  stopLoss?: number;
  takeProfit?: number;
  maxDailyLoss?: number;
}): Promise<ApiResult<{ deploy_id: string; pid: number }>> {
  const raw = await invoke<string>('deploy_python_strategy', {
    strategyId: params.strategyId,
    symbol: params.symbol,
    mode: params.mode,
    broker: params.broker || null,
    quantity: params.quantity,
    timeframe: params.timeframe,
    parameters: JSON.stringify(params.parameters || {}),
    stopLoss: params.stopLoss || null,
    takeProfit: params.takeProfit || null,
    maxDailyLoss: params.maxDailyLoss || null,
  });
  return parseResult(raw);
}
