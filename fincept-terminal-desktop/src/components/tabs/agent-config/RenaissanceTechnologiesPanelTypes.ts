/**
 * RenaissanceTechnologiesPanel - Types, Constants, and Reducer
 */

// =============================================================================
// TYPES
// =============================================================================

export interface RenTechAgent {
  id: string;
  name: string;
  role: string;
  status: 'idle' | 'running' | 'completed' | 'error';
  category: 'research' | 'execution' | 'risk' | 'committee';
}

export interface SignalAnalysisResult {
  success: boolean;
  ticker?: string;
  signal?: string;
  confidence?: number;
  statistical_edge?: number;
  signal_score?: number;
  risk_score?: number;
  execution_score?: number;
  arbitrage_score?: number;
  position_sizing?: {
    recommended_size_pct: number;
    kelly_fraction: number;
    leverage: number;
  };
  signal_strength?: {
    mean_reversion: number;
    momentum: number;
    statistical_arbitrage: number;
    risk_adjusted_return: number;
  };
  key_factors?: string[];
  risks?: string[];
  reasoning?: string;
  ic_decision?: ICDeliberationResult;
  error?: string;
}

export interface ICDeliberationResult {
  decision: string;
  decision_rationale: string;
  confidence: number;
  pros: string[];
  cons: string[];
  member_opinions: ICVote[];
  vote_summary: Record<string, number>;
  approved_size_pct?: number;
  conditions?: string[];
  llm_config?: {
    provider: string;
    model_id: string;
    temperature: number;
  };
}

export interface ICVote {
  member_role: string;
  vote: 'approve' | 'reject' | 'conditional' | 'abstain';
  rationale: string;
  confidence: number;
}

export interface RiskLimits {
  max_position_size_pct: number;
  max_leverage: number;
  max_drawdown_pct: number;
  max_daily_var_pct: number;
  min_signal_confidence: number;
}

// =============================================================================
// CONSTANTS
// =============================================================================

export const AGENTS: RenTechAgent[] = [
  { id: 'signal_scientist', name: 'Signal Scientist', role: 'Statistical signal discovery & pattern recognition', status: 'idle', category: 'research' },
  { id: 'data_scientist', name: 'Data Scientist', role: 'Data quality assurance & feature engineering', status: 'idle', category: 'research' },
  { id: 'quant_researcher', name: 'Quant Researcher', role: 'Strategy development & optimization', status: 'idle', category: 'research' },
  { id: 'research_lead', name: 'Research Lead', role: 'Research validation & peer review', status: 'idle', category: 'research' },
  { id: 'execution_trader', name: 'Execution Trader', role: 'Optimal execution & slippage minimization', status: 'idle', category: 'execution' },
  { id: 'market_maker', name: 'Market Maker', role: 'Liquidity provision & spread capture', status: 'idle', category: 'execution' },
  { id: 'risk_quant', name: 'Risk Quant', role: 'Risk modeling & VaR calculation', status: 'idle', category: 'risk' },
  { id: 'compliance_officer', name: 'Compliance Officer', role: 'Regulatory compliance & oversight', status: 'idle', category: 'risk' },
  { id: 'portfolio_manager', name: 'Portfolio Manager', role: 'Position sizing & portfolio construction', status: 'idle', category: 'committee' },
  { id: 'investment_committee_chair', name: 'IC Chair', role: 'Final approval & decision authority', status: 'idle', category: 'committee' },
];

export const SIGNAL_TYPES = [
  { value: 'momentum', label: 'Momentum', desc: 'Trend-following strategies' },
  { value: 'mean_reversion', label: 'Mean Reversion', desc: 'Counter-trend strategies' },
  { value: 'stat_arb', label: 'Statistical Arbitrage', desc: 'Pairs & spread trading' },
  { value: 'pairs_trading', label: 'Pairs Trading', desc: 'Correlated asset trading' },
];

// API timeout for external calls (30 seconds)
export const API_TIMEOUT_MS = 30000;

// =============================================================================
// STATE MANAGEMENT - useReducer for atomic updates
// =============================================================================

// State machine: idle → loading → success | error
export type AnalysisStatus = 'idle' | 'loading' | 'success' | 'error';

export interface PanelState {
  // UI state
  activeTab: 'analysis' | 'agents' | 'config';
  showAdvanced: boolean;
  currentTime: Date;

  // Form state
  ticker: string;
  signalType: string;

  // Analysis state (state machine pattern)
  analysisStatus: AnalysisStatus;
  analysisResult: SignalAnalysisResult | null;
  analysisError: string | null;

  // Config state
  riskLimits: RiskLimits;

  // Validation
  tickerValidationError: string | null;
}

export type PanelAction =
  | { type: 'SET_ACTIVE_TAB'; payload: 'analysis' | 'agents' | 'config' }
  | { type: 'SET_SHOW_ADVANCED'; payload: boolean }
  | { type: 'UPDATE_TIME'; payload: Date }
  | { type: 'SET_TICKER'; payload: string }
  | { type: 'SET_TICKER_ERROR'; payload: string | null }
  | { type: 'SET_SIGNAL_TYPE'; payload: string }
  | { type: 'ANALYSIS_START' }
  | { type: 'ANALYSIS_SUCCESS'; payload: SignalAnalysisResult }
  | { type: 'ANALYSIS_ERROR'; payload: string }
  | { type: 'ANALYSIS_RESET' }
  | { type: 'UPDATE_RISK_LIMITS'; payload: RiskLimits };

export const initialState: PanelState = {
  activeTab: 'analysis',
  showAdvanced: false,
  currentTime: new Date(),
  ticker: '',
  signalType: 'momentum',
  analysisStatus: 'idle',
  analysisResult: null,
  analysisError: null,
  riskLimits: {
    max_position_size_pct: 5.0,
    max_leverage: 12.5,
    max_drawdown_pct: 15.0,
    max_daily_var_pct: 2.0,
    min_signal_confidence: 0.5075,
  },
  tickerValidationError: null,
};

export function panelReducer(state: PanelState, action: PanelAction): PanelState {
  switch (action.type) {
    case 'SET_ACTIVE_TAB':
      return { ...state, activeTab: action.payload };
    case 'SET_SHOW_ADVANCED':
      return { ...state, showAdvanced: action.payload };
    case 'UPDATE_TIME':
      return { ...state, currentTime: action.payload };
    case 'SET_TICKER':
      return { ...state, ticker: action.payload, tickerValidationError: null };
    case 'SET_TICKER_ERROR':
      return { ...state, tickerValidationError: action.payload };
    case 'SET_SIGNAL_TYPE':
      return { ...state, signalType: action.payload };
    case 'ANALYSIS_START':
      // Atomic update: loading state + clear previous results
      return {
        ...state,
        analysisStatus: 'loading',
        analysisResult: null,
        analysisError: null,
      };
    case 'ANALYSIS_SUCCESS':
      return {
        ...state,
        analysisStatus: 'success',
        analysisResult: action.payload,
        analysisError: null,
      };
    case 'ANALYSIS_ERROR':
      return {
        ...state,
        analysisStatus: 'error',
        analysisResult: null,
        analysisError: action.payload,
      };
    case 'ANALYSIS_RESET':
      return {
        ...state,
        analysisStatus: 'idle',
        analysisResult: null,
        analysisError: null,
      };
    case 'UPDATE_RISK_LIMITS':
      return { ...state, riskLimits: action.payload };
    default:
      return state;
  }
}
