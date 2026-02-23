export interface CandleData {
  symbol: string;
  provider: string;
  timeframe: string;
  open_time: number;
  o: number;
  h: number;
  l: number;
  c: number;
  volume: number;
  is_closed: number;
}

export interface ConditionItem {
  indicator: string;
  params: Record<string, number>;
  field: string;
  operator: string;
  value: number;
  value2?: number;
  // Indicator-vs-indicator comparison (Phase 3)
  compareMode?: 'value' | 'indicator';  // default 'value'
  compareIndicator?: string;
  compareParams?: Record<string, number>;
  compareField?: string;
  // Chart offsets (Phase 4)
  offset?: number;         // 0=current (default), 1=1 candle ago, etc.
  compareOffset?: number;  // offset for comparison side
  // Advanced operators
  lookback?: number;       // for crossed_within, rising, falling operators
}

export interface ConditionGroup {
  conditions: ConditionItem[];
  logic: 'AND' | 'OR';
}

export interface AlgoStrategy {
  id: string;
  name: string;
  description: string;
  entry_conditions: string; // JSON string of (ConditionItem | string)[]
  exit_conditions: string;  // JSON string of (ConditionItem | string)[]
  entry_logic: 'AND' | 'OR';
  exit_logic: 'AND' | 'OR';
  stop_loss: number | null;
  take_profit: number | null;
  trailing_stop: number | null;
  trailing_stop_type: string;
  timeframe: string;
  symbols: string; // JSON string of string[]
  is_active: number;
  created_at: string;
  updated_at: string;
}

export interface AlgoDeployment {
  id: string;
  strategy_id: string;
  strategy_name: string | null;
  symbol: string;
  provider: string;
  mode: 'paper' | 'live';
  status: 'pending' | 'starting' | 'running' | 'stopped' | 'error';
  timeframe: string;
  quantity: number;
  params: string;
  pid: number | null;
  error_message: string | null;
  created_at: string;
  updated_at: string;
  stopped_at: string | null;
  metrics: AlgoMetrics;
}

export interface AlgoMetrics {
  total_pnl: number;
  unrealized_pnl: number;
  total_trades: number;
  win_rate: number;
  max_drawdown: number;
  current_position_qty: number;
  current_position_side: string;
  current_position_entry: number;
  metrics_updated: string | null;
}

export interface AlgoTrade {
  id: string;
  deployment_id: string;
  symbol: string;
  side: 'BUY' | 'SELL';
  quantity: number;
  price: number;
  pnl: number;
  timestamp: string;
  signal_reason: string;
}

export interface ScanResult {
  symbol: string;
  price: number;
  details: ConditionDetail[];
}

export interface ConditionDetail {
  met: boolean;
  indicator: string;
  field: string;
  computed_value: number | null;
  operator: string;
  target: number;
  target_indicator?: string;
  target_computed_value?: number;
  error?: string;
}

export interface IndicatorDefinition {
  name: string;
  label: string;
  category: 'stock_attributes' | 'moving_averages' | 'momentum' | 'trend' | 'volatility' | 'volume';
  fields: string[];
  defaultField: string;
  params: IndicatorParam[];
}

export interface IndicatorParam {
  key: string;
  label: string;
  default: number;
  min?: number;
  max?: number;
}

export type AlgoSubView = 'builder' | 'strategies' | 'library' | 'scanner' | 'dashboard';

// ============================================================================
// PYTHON STRATEGY LIBRARY TYPES
// ============================================================================

/** Python strategy from the library registry */
export interface PythonStrategy {
  id: string;                    // FCT-XXXXXXXX
  name: string;                  // Algorithm class name
  category: string;              // Category from registry
  path: string;                  // Relative file path
  description?: string;          // From docstring
  code?: string;                 // Source code (loaded on demand)
  compatibility: ('backtest' | 'paper' | 'live')[];
}

/** Custom Python strategy (user-modified copy) */
export interface CustomPythonStrategy extends PythonStrategy {
  base_strategy_id: string;      // Original library strategy ID
  parameters: string;            // JSON parameter overrides
  is_active: number;
  created_at: string;
  updated_at: string;
  strategy_type: 'python';
}

/** Strategy parameter definition */
export interface StrategyParameter {
  name: string;
  type: 'int' | 'float' | 'string' | 'bool';
  default_value: string;
  description?: string;
  min?: number;
  max?: number;
}

/** Python backtest request */
export interface PythonBacktestRequest {
  strategy_id: string;
  custom_code?: string;          // Optional override
  symbols: string[];
  start_date: string;
  end_date: string;
  initial_cash: number;
  parameters?: Record<string, string>;
  data_provider: 'yfinance' | 'fyers';
}

/** Python backtest result */
export interface PythonBacktestResult {
  success: boolean;
  trades: BacktestTrade[];
  equity_curve: EquityPoint[];
  metrics: BacktestMetrics;
  debug?: string[];
  error?: string;
}

export interface BacktestTrade {
  id: string;
  symbol: string;
  side: 'BUY' | 'SELL';
  quantity: number;
  entry_price: number;
  exit_price?: number;
  pnl: number;
  entry_time: string;
  exit_time?: string;
  bars_held: number;
}

export interface EquityPoint {
  time: string;
  value: number;
}

export interface BacktestMetrics {
  total_trades: number;
  winning_trades: number;
  losing_trades: number;
  win_rate: number;
  total_return: number;
  total_return_pct: number;
  max_drawdown: number;
  max_drawdown_pct: number;
  sharpe_ratio: number;
  profit_factor: number;
  avg_pnl: number;
  avg_bars_held: number;
}

/** Python deployment (extends AlgoDeployment) */
export interface PythonDeployment extends AlgoDeployment {
  strategy_type: 'python';
  python_strategy_id: string;    // FCT-XXXXXXXX or custom-XXXXXXXX
  parameter_overrides?: string;  // JSON object
}
