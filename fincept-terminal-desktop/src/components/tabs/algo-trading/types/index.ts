export interface ConditionItem {
  indicator: string;
  params: Record<string, number>;
  field: string;
  operator: string;
  value: number;
  value2?: number;
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
  error?: string;
}

export interface IndicatorDefinition {
  name: string;
  label: string;
  category: 'trend' | 'momentum' | 'volatility' | 'volume' | 'price';
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

export type AlgoSubView = 'builder' | 'strategies' | 'scanner' | 'dashboard';
