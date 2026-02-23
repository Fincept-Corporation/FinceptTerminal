/**
 * VisionQuant Pattern Intelligence Service
 * Frontend service layer for AI K-line pattern recognition.
 * Calls Rust commands via Tauri invoke.
 */
import { invoke } from '@tauri-apps/api/core';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export interface PatternMatch {
  symbol: string;
  date: string;
  score: number;
  dtw_sim: number | null;
  correlation: number | null;
  feature_sim: number;
  visual_sim: number;
  trend_match: number | null;
  path: string | null;
  tb_label?: number;
  tb_hit_day?: number;
  tb_hit_type?: string;
  final_return_pct?: number;
}

export interface WinRateStats {
  total: number;
  bullish: number;
  neutral: number;
  bearish: number;
  win_rate: number;
  win_loss_ratio: number;
  avg_return_pct: number;
  avg_holding_days: number;
}

export interface SearchResult {
  matches: PatternMatch[];
  query_info: {
    symbol: string;
    date: string;
    bars: number;
  };
  win_rate_stats: WinRateStats;
}

export interface VisionScore {
  score: number;
  max: number;
  win_rate: number;
  label: string;
}

export interface FundamentalScore {
  score: number;
  max: number;
  pe: number | null;
  roe: number | null;
  pe_score: number;
  roe_score: number;
  label: string;
  error?: string;
}

export interface TechnicalScore {
  score: number;
  max: number;
  ma60_signal: number | null;
  rsi: number | null;
  rsi_score: number;
  macd_hist: number | null;
  macd_score: number;
  label: string;
  error?: string;
}

export interface Scorecard {
  total_score: number;
  max_score: number;
  action: 'BUY' | 'WAIT' | 'SELL';
  vision: VisionScore;
  fundamental: FundamentalScore;
  technical: TechnicalScore;
  symbol: string;
  date: string;
}

export interface Trade {
  entry_date: string;
  exit_date: string;
  entry_price: number;
  exit_price: number;
  shares: number;
  profit: number;
  return_pct: number;
  hold_days: number;
  exit_reason: string;
}

export interface EquityPoint {
  date: string;
  equity: number;
}

export interface BacktestResult {
  symbol: string;
  start_date: string;
  end_date: string;
  initial_capital: number;
  final_equity: number;
  return_pct: number;
  buy_hold_return_pct: number;
  sharpe_ratio: number;
  max_drawdown_pct: number;
  total_trades: number;
  win_rate: number;
  wins: number;
  losses: number;
  avg_profit: number;
  avg_loss: number;
  avg_hold_days: number;
  trades: Trade[];
  equity_curve: EquityPoint[];
}

export interface IndexStatus {
  index_ready: boolean;
  model_exists: boolean;
  index_exists: boolean;
  meta_exists: boolean;
  n_records: number;
  model_loaded?: boolean;
  data_dir: string;
}

// ---------------------------------------------------------------------------
// Config interfaces — advanced settings passed through to Python
// ---------------------------------------------------------------------------

export interface SearchConfig {
  isolation_days?: number;
  dtw_window?: number;
  w_dtw?: number;
  w_corr?: number;
  w_shape?: number;
  w_visual?: number;
  trend_bonus?: number;
  faiss_multiplier?: number;
  hq_dtw_threshold?: number;
  tb_upper?: number;
  tb_lower?: number;
  tb_max_hold?: number;
}

export interface ScoringConfig {
  vision_breakpoints?: number[];
  pe_thresholds?: number[];
  roe_thresholds?: number[];
  ma_period?: number;
  rsi_ranges?: number[];
  macd_fast?: number;
  macd_slow?: number;
  macd_signal?: number;
  ma_thresholds?: number[];
  buy_threshold?: number;
  wait_threshold?: number;
}

export interface BacktestConfig {
  stop_loss?: number;
  take_profit?: number;
  max_hold?: number;
  entry_rsi?: number;
  exit_rsi?: number;
  ma_period?: number;
  macd_fast?: number;
  macd_slow?: number;
  macd_signal?: number;
}

export interface SetupConfig {
  batch_size?: number;
  chart_style?: 'international' | 'chinese';
  learning_rate?: number;
}

// ---------------------------------------------------------------------------
// Response parsing helper
// ---------------------------------------------------------------------------

interface PythonResponse<T> {
  status: string;
  data?: T;
  error?: string;
}

function parseResponse<T>(raw: string): T {
  // Python scripts may output progress lines before final JSON
  // Find the last valid JSON line
  const lines = raw.trim().split('\n');
  let lastJson = '';
  for (let i = lines.length - 1; i >= 0; i--) {
    const line = lines[i].trim();
    if (line.startsWith('{')) {
      lastJson = line;
      break;
    }
  }

  if (!lastJson) {
    throw new Error('No JSON response received from backend');
  }

  const parsed: PythonResponse<T> = JSON.parse(lastJson);

  if (parsed.status === 'error') {
    throw new Error(parsed.error || 'Unknown backend error');
  }

  if (parsed.data === undefined) {
    throw new Error('Response missing data field');
  }

  return parsed.data;
}

// ---------------------------------------------------------------------------
// Helper: strip undefined values from config before serializing
// ---------------------------------------------------------------------------

function serializeConfig(config?: Record<string, unknown>): string | null {
  if (!config) return null;
  const cleaned: Record<string, unknown> = {};
  for (const [k, v] of Object.entries(config)) {
    if (v !== undefined) cleaned[k] = v;
  }
  return Object.keys(cleaned).length > 0 ? JSON.stringify(cleaned) : null;
}

// ---------------------------------------------------------------------------
// Service API
// ---------------------------------------------------------------------------

export const visionQuantService = {
  /**
   * Search for similar K-line patterns.
   */
  async searchPatterns(
    symbol: string,
    date?: string,
    topK: number = 10,
    lookback: number = 60,
    config?: SearchConfig,
  ): Promise<SearchResult> {
    const raw = await invoke<string>('vision_quant_search_patterns', {
      symbol,
      date: date || null,
      topK,
      lookback,
      config: serializeConfig(config as Record<string, unknown>),
    });
    return parseResponse<SearchResult>(raw);
  },

  /**
   * Get multi-factor scorecard (Vision + Fundamental + Technical).
   */
  async getScore(
    symbol: string,
    winRate: number,
    date?: string,
    config?: ScoringConfig,
  ): Promise<Scorecard> {
    const raw = await invoke<string>('vision_quant_score', {
      symbol,
      winRate,
      date: date || null,
      config: serializeConfig(config as Record<string, unknown>),
    });
    return parseResponse<Scorecard>(raw);
  },

  /**
   * Run adaptive strategy backtest.
   */
  async runBacktest(
    symbol: string,
    startDate: string,
    endDate: string,
    initialCapital: number = 100000,
    config?: BacktestConfig,
  ): Promise<BacktestResult> {
    const raw = await invoke<string>('vision_quant_backtest', {
      symbol,
      startDate,
      endDate,
      initialCapital,
      config: serializeConfig(config as Record<string, unknown>),
    });
    return parseResponse<BacktestResult>(raw);
  },

  /**
   * Build FAISS index (first-run setup).
   */
  async setupIndex(
    symbols?: string[],
    startDate?: string,
    stride?: number,
    epochs?: number,
    config?: SetupConfig,
  ): Promise<Record<string, unknown>> {
    const raw = await invoke<string>('vision_quant_setup_index', {
      symbols: symbols || null,
      startDate: startDate || null,
      stride: stride || null,
      epochs: epochs || null,
      config: serializeConfig(config as Record<string, unknown>),
    });
    return parseResponse<Record<string, unknown>>(raw);
  },

  /**
   * Check VisionQuant index/model status.
   */
  async getStatus(): Promise<IndexStatus> {
    const raw = await invoke<string>('vision_quant_status', {});
    return parseResponse<IndexStatus>(raw);
  },
};
