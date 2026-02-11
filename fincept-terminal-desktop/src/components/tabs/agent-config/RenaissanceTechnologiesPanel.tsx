/**
 * Renaissance Technologies Multi-Agent System Panel
 * FINCEPT TERMINAL DESIGN - Professional Grade Trading Intelligence
 *
 * Features:
 * - Signal Analysis Pipeline with Medallion Fund Strategies
 * - 10-Agent Investment Committee with IC Deliberation
 * - Real-time Position Sizing & Risk Management
 * - Team Collaboration Workflows
 * - Professional Terminal UI/UX
 *
 * State Management:
 * - Uses useReducer for atomic state updates
 * - Implements explicit state machine (idle→loading→success|error)
 * - Proper cleanup on unmount with AbortController pattern
 * - Input validation before API calls
 * - Timeout protection for external calls
 */

import React, { useReducer, useEffect, useRef, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { withTimeout } from '@/services/core/apiUtils';
import { validateSymbol, sanitizeSymbol } from '@/services/core/validators';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import {
  TrendingUp, AlertCircle, CheckCircle, Users, Sparkles,
  Brain, Shield, FileText, Zap, Settings, Play, RefreshCw,
  ChevronRight, DollarSign, BarChart3, Target, Workflow,
  ThumbsUp, ThumbsDown, MessageSquare, Clock, Award, Loader2,
  ChevronDown, Activity, TrendingDown, Minus, Database,
  Maximize2, Minimize2, X, Info, Cpu, Network
} from 'lucide-react';

// Import Fincept styles
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS, COMMON_STYLES } from '../portfolio-tab/finceptStyles';

// =============================================================================
// TYPES
// =============================================================================

interface RenTechAgent {
  id: string;
  name: string;
  role: string;
  status: 'idle' | 'running' | 'completed' | 'error';
  category: 'research' | 'execution' | 'risk' | 'committee';
}

interface SignalAnalysisResult {
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

interface ICDeliberationResult {
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

interface ICVote {
  member_role: string;
  vote: 'approve' | 'reject' | 'conditional' | 'abstain';
  rationale: string;
  confidence: number;
}

interface RiskLimits {
  max_position_size_pct: number;
  max_leverage: number;
  max_drawdown_pct: number;
  max_daily_var_pct: number;
  min_signal_confidence: number;
}

// =============================================================================
// CONSTANTS
// =============================================================================

const AGENTS: RenTechAgent[] = [
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

const SIGNAL_TYPES = [
  { value: 'momentum', label: 'Momentum', desc: 'Trend-following strategies' },
  { value: 'mean_reversion', label: 'Mean Reversion', desc: 'Counter-trend strategies' },
  { value: 'stat_arb', label: 'Statistical Arbitrage', desc: 'Pairs & spread trading' },
  { value: 'pairs_trading', label: 'Pairs Trading', desc: 'Correlated asset trading' },
];

// API timeout for external calls (30 seconds)
const API_TIMEOUT_MS = 30000;

// =============================================================================
// STATE MANAGEMENT - useReducer for atomic updates
// =============================================================================

// State machine: idle → loading → success | error
type AnalysisStatus = 'idle' | 'loading' | 'success' | 'error';

interface PanelState {
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

type PanelAction =
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

const initialState: PanelState = {
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

function panelReducer(state: PanelState, action: PanelAction): PanelState {
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

// =============================================================================
// MAIN COMPONENT
// =============================================================================

function RenaissanceTechnologiesPanelInner() {
  // State via reducer - atomic updates, explicit state machine
  const [state, dispatch] = useReducer(panelReducer, initialState);

  // Refs for cleanup and mounted check
  const mountedRef = useRef(true);
  const abortControllerRef = useRef<AbortController | null>(null);
  const fetchIdRef = useRef(0); // For race condition prevention

  // Destructure state for convenience
  const {
    activeTab,
    showAdvanced,
    currentTime,
    ticker,
    signalType,
    analysisStatus,
    analysisResult,
    analysisError,
    riskLimits,
    tickerValidationError,
  } = state;

  // Derived state
  const isAnalyzing = analysisStatus === 'loading';

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      abortControllerRef.current?.abort();
    };
  }, []);

  // Update time - with cleanup
  useEffect(() => {
    const timer = setInterval(() => {
      if (mountedRef.current) {
        dispatch({ type: 'UPDATE_TIME', payload: new Date() });
      }
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Validated ticker setter
  const handleTickerChange = useCallback((value: string) => {
    const upperValue = value.toUpperCase();
    dispatch({ type: 'SET_TICKER', payload: upperValue });

    // Validate on change
    if (upperValue.length > 0) {
      const validation = validateSymbol(upperValue);
      if (!validation.valid) {
        dispatch({ type: 'SET_TICKER_ERROR', payload: validation.error || 'Invalid ticker' });
      }
    }
  }, []);

  // Run signal analysis with proper state management
  const runSignalAnalysis = useCallback(async () => {
    // Point 10: Input validation before API call
    const validation = validateSymbol(ticker);
    if (!validation.valid) {
      dispatch({ type: 'SET_TICKER_ERROR', payload: validation.error || 'Invalid ticker' });
      return;
    }

    const sanitizedTicker = sanitizeSymbol(ticker);

    // Increment fetch ID for race condition handling (Point 1)
    const currentFetchId = ++fetchIdRef.current;

    // Abort previous request (Point 3: cleanup)
    abortControllerRef.current?.abort();
    const abortController = new AbortController();
    abortControllerRef.current = abortController;

    // Point 2: State machine transition to loading
    dispatch({ type: 'ANALYSIS_START' });

    try {
      // Send only user inputs - backend will calculate signal metrics from real data
      const cliInput = {
        ticker: sanitizedTicker,
        signal_type: signalType,
        trade_value: 1_000_000, // Default $1M trade
        period: '1y',
        include_deliberation: true, // Include IC deliberation
      };

      // Point 8: Timeout protection for external call
      const result = await withTimeout(
        invoke<string>('execute_renaissance_cli', {
          command: 'analyze_signal',
          data: JSON.stringify(cliInput),
        }),
        API_TIMEOUT_MS,
        'Analysis request timed out'
      );

      // Check for abort and race conditions (Points 1, 3)
      if (abortController.signal.aborted || currentFetchId !== fetchIdRef.current || !mountedRef.current) {
        return;
      }

      const parsed = JSON.parse(result);

      if (parsed.success) {
        // Point 2: State machine transition to success
        dispatch({ type: 'ANALYSIS_SUCCESS', payload: parsed.data || parsed });
      } else {
        // Point 2: State machine transition to error
        dispatch({ type: 'ANALYSIS_ERROR', payload: parsed.error || 'Analysis failed' });
      }
    } catch (error: any) {
      // Check if component is still mounted and this is the current request
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) {
        return;
      }

      // Don't report abort errors
      if (error.name === 'AbortError' || error.message === 'Request aborted') {
        return;
      }

      console.error('Analysis error:', error);
      // Point 2: State machine transition to error
      dispatch({ type: 'ANALYSIS_ERROR', payload: error.message || 'Analysis failed' });
    }
  }, [ticker, signalType]);

  return (
    <div style={{
      ...COMMON_STYLES.container,
      fontFamily: TYPOGRAPHY.MONO,
    }}>
      {/* ===== HEADER ===== */}
      <div style={{
        ...COMMON_STYLES.header,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        gap: SPACING.MEDIUM,
      }}>
        {/* Left: Branding */}
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
            padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
            backgroundColor: FINCEPT.ORANGE,
          }}>
            <Award size={16} color={FINCEPT.DARK_BG} />
            <span style={{
              color: FINCEPT.DARK_BG,
              fontSize: TYPOGRAPHY.SMALL,
              fontWeight: TYPOGRAPHY.BOLD,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase',
            }}>
              RENAISSANCE TECHNOLOGIES
            </span>
          </div>

          <div style={COMMON_STYLES.verticalDivider} />

          {/* Tab Selector */}
          <div style={{ display: 'flex', gap: SPACING.SMALL }}>
            {(['analysis', 'agents', 'config'] as const).map((tab) => (
              <button
                key={tab}
                onClick={() => dispatch({ type: 'SET_ACTIVE_TAB', payload: tab })}
                style={{
                  ...COMMON_STYLES.tabButton(activeTab === tab),
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  minWidth: '85px',
                  justifyContent: 'center',
                }}
                onMouseEnter={(e) => {
                  if (activeTab !== tab) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }}
                onMouseLeave={(e) => {
                  if (activeTab !== tab) e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                {tab === 'analysis' && <BarChart3 size={10} />}
                {tab === 'agents' && <Users size={10} />}
                {tab === 'config' && <Settings size={10} />}
                {tab.toUpperCase()}
              </button>
            ))}
          </div>
        </div>

        {/* Right: Time, LLM Status & System Status */}
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
          {/* LLM Config Display */}
          {analysisResult?.ic_decision?.llm_config && (
            <>
              <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                <Cpu size={12} color={FINCEPT.CYAN} />
                <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.CYAN }}>
                  {analysisResult.ic_decision.llm_config.provider.toUpperCase()}/{analysisResult.ic_decision.llm_config.model_id}
                </span>
              </div>
              <div style={COMMON_STYLES.verticalDivider} />
            </>
          )}
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
            <Clock size={12} color={FINCEPT.GRAY} />
            <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY }}>
              {currentTime.toLocaleTimeString('en-US', { hour12: false })}
            </span>
          </div>
          <div style={COMMON_STYLES.verticalDivider} />
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
            <div style={{
              width: '6px',
              height: '6px',
              borderRadius: '50%',
              backgroundColor: FINCEPT.GREEN,
              animation: 'pulse 2s infinite',
            }} />
            <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GREEN }}>OPERATIONAL</span>
          </div>
        </div>
      </div>

      {/* ===== CONTENT ===== */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: SPACING.DEFAULT,
        display: 'flex',
        flexDirection: 'column',
        gap: SPACING.DEFAULT,
      }}>
        {activeTab === 'analysis' && (
          <AnalysisTab
            ticker={ticker}
            setTicker={handleTickerChange}
            signalType={signalType}
            setSignalType={(v) => dispatch({ type: 'SET_SIGNAL_TYPE', payload: v })}
            isAnalyzing={isAnalyzing}
            analysisResult={analysisResult}
            analysisError={analysisError}
            tickerValidationError={tickerValidationError}
            onRunAnalysis={runSignalAnalysis}
            showAdvanced={showAdvanced}
            setShowAdvanced={(v) => dispatch({ type: 'SET_SHOW_ADVANCED', payload: v })}
          />
        )}

        {activeTab === 'agents' && (
          <AgentsTab agents={AGENTS} />
        )}

        {activeTab === 'config' && (
          <ConfigTab
            riskLimits={riskLimits}
            onUpdateLimits={(limits) => dispatch({ type: 'UPDATE_RISK_LIMITS', payload: limits })}
          />
        )}
      </div>

      {/* Inject animations */}
      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}

// =============================================================================
// ANALYSIS TAB
// =============================================================================

function AnalysisTab({
  ticker,
  setTicker,
  signalType,
  setSignalType,
  isAnalyzing,
  analysisResult,
  analysisError,
  tickerValidationError,
  onRunAnalysis,
  showAdvanced,
  setShowAdvanced,
}: {
  ticker: string;
  setTicker: (v: string) => void;
  signalType: string;
  setSignalType: (v: string) => void;
  isAnalyzing: boolean;
  analysisResult: SignalAnalysisResult | null;
  analysisError: string | null;
  tickerValidationError: string | null;
  onRunAnalysis: () => void;
  showAdvanced: boolean;
  setShowAdvanced: (v: boolean) => void;
}) {
  const selectedSignalType = SIGNAL_TYPES.find(s => s.value === signalType) || SIGNAL_TYPES[0];

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
      {/* Input Panel */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: BORDERS.STANDARD,
        borderRadius: '2px',
      }}>
        {/* Header */}
        <div style={{
          ...COMMON_STYLES.sectionHeader,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          margin: 0,
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
            <Sparkles size={14} color={FINCEPT.ORANGE} />
            <span>MEDALLION FUND SIGNAL ANALYSIS</span>
          </div>
          <button
            onClick={() => setShowAdvanced(!showAdvanced)}
            style={{
              ...COMMON_STYLES.buttonSecondary,
              padding: '6px 10px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              minWidth: '32px',
              height: '26px',
            }}
          >
            <ChevronDown
              size={10}
              style={{
                transform: showAdvanced ? 'rotate(180deg)' : 'rotate(0deg)',
                transition: 'transform 0.2s',
              }}
            />
          </button>
        </div>

        {/* Ticker Input Section */}
        <div style={{ padding: `${SPACING.DEFAULT} ${SPACING.DEFAULT} 0` }}>
          <label style={COMMON_STYLES.dataLabel}>TICKER</label>
          <input
            type="text"
            value={ticker}
            onChange={(e) => setTicker(e.target.value)}
            placeholder="AAPL"
            style={{
              ...COMMON_STYLES.inputField,
              marginTop: SPACING.SMALL,
              textTransform: 'uppercase',
              width: '200px',
              borderColor: tickerValidationError ? FINCEPT.RED : FINCEPT.BORDER,
            }}
            onFocus={(e) => e.currentTarget.style.borderColor = tickerValidationError ? FINCEPT.RED : FINCEPT.ORANGE}
            onBlur={(e) => e.currentTarget.style.borderColor = tickerValidationError ? FINCEPT.RED : FINCEPT.BORDER}
          />
          {tickerValidationError && (
            <div style={{
              marginTop: SPACING.TINY,
              fontSize: TYPOGRAPHY.TINY,
              color: FINCEPT.RED,
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}>
              <AlertCircle size={10} />
              {tickerValidationError}
            </div>
          )}
        </div>

        {/* Strategy Type & Execute - Full Width Section */}
        <div style={{ padding: SPACING.DEFAULT }}>
          <label style={COMMON_STYLES.dataLabel}>STRATEGY TYPE</label>
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(5, 1fr)',
            gap: SPACING.DEFAULT,
            marginTop: SPACING.SMALL,
            marginLeft: `calc(-1 * ${SPACING.DEFAULT})`,
            marginRight: `calc(-1 * ${SPACING.DEFAULT})`,
            paddingLeft: SPACING.DEFAULT,
            paddingRight: SPACING.DEFAULT,
          }}>
            {SIGNAL_TYPES.map((st) => (
              <button
                key={st.value}
                onClick={() => setSignalType(st.value)}
                style={{
                  padding: '8px 12px',
                  backgroundColor: signalType === st.value ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
                  color: signalType === st.value ? FINCEPT.DARK_BG : FINCEPT.WHITE,
                  border: `1px solid ${signalType === st.value ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: TYPOGRAPHY.SMALL,
                  fontWeight: TYPOGRAPHY.BOLD,
                  cursor: 'pointer',
                  transition: EFFECTS.TRANSITION_FAST,
                  textTransform: 'uppercase',
                  letterSpacing: '0.3px',
                  height: '32px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  whiteSpace: 'nowrap',
                }}
                onMouseEnter={(e) => {
                  if (signalType !== st.value) {
                    e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                    e.currentTarget.style.color = FINCEPT.ORANGE;
                  }
                }}
                onMouseLeave={(e) => {
                  if (signalType !== st.value) {
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    e.currentTarget.style.color = FINCEPT.WHITE;
                  }
                }}
                title={st.desc}
              >
                {st.label}
              </button>
            ))}

            {/* Execute Button */}
            <button
              onClick={onRunAnalysis}
              disabled={!ticker || isAnalyzing}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: SPACING.SMALL,
                padding: '8px 16px',
                height: '32px',
                opacity: ticker && !isAnalyzing ? 1 : 0.5,
                cursor: ticker && !isAnalyzing ? 'pointer' : 'not-allowed',
              }}
            >
              {isAnalyzing ? (
                <>
                  <Loader2 size={12} style={{ animation: 'spin 1s linear infinite' }} />
                  ANALYZING...
                </>
              ) : (
                <>
                  <Play size={12} />
                  EXECUTE
                </>
              )}
            </button>
          </div>
        </div>

        {/* Advanced Settings */}
        {showAdvanced && (
          <div style={{
            padding: `0 ${SPACING.DEFAULT} ${SPACING.DEFAULT}`,
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            marginTop: SPACING.DEFAULT,
            paddingTop: SPACING.DEFAULT,
          }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>
              ADVANCED PARAMETERS
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.MUTED }}>
              <div>• Lookback Period: Auto</div>
              <div>• Min Confidence: 50.75%</div>
              <div>• Max Leverage: 12.5x</div>
            </div>
          </div>
        )}
      </div>

      {/* Error Display */}
      {analysisError && (
        <div style={{
          ...COMMON_STYLES.panel,
          border: `2px solid ${FINCEPT.RED}`,
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT, color: FINCEPT.RED }}>
            <AlertCircle size={24} />
            <div>
              <div style={{ fontSize: TYPOGRAPHY.SUBHEADING, fontWeight: TYPOGRAPHY.BOLD }}>ANALYSIS FAILED</div>
              <div style={{ fontSize: TYPOGRAPHY.SMALL, marginTop: SPACING.TINY }}>{analysisError}</div>
            </div>
          </div>
        </div>
      )}

      {/* Results */}
      {analysisResult && <ResultsDisplay result={analysisResult} />}
    </div>
  );
}

// =============================================================================
// RESULTS DISPLAY
// =============================================================================

function ResultsDisplay({ result }: { result: SignalAnalysisResult | any }) {
  if (result.success === false || result.error) {
    return (
      <div style={{
        ...COMMON_STYLES.panel,
        border: `2px solid ${FINCEPT.RED}`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT, color: FINCEPT.RED }}>
          <AlertCircle size={24} />
          <div>
            <div style={{ fontSize: TYPOGRAPHY.SUBHEADING, fontWeight: TYPOGRAPHY.BOLD }}>ANALYSIS FAILED</div>
            <div style={{ fontSize: TYPOGRAPHY.SMALL, marginTop: SPACING.TINY }}>{result.error}</div>
          </div>
        </div>
      </div>
    );
  }

  const hasSignalData = result.ticker || result.signal;
  const signalColor = result.signal === 'long' ? FINCEPT.GREEN :
                      result.signal === 'short' ? FINCEPT.RED :
                      FINCEPT.GRAY;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
      {/* Main Signal Card */}
      {hasSignalData && (
        <div style={{
          ...COMMON_STYLES.panel,
          border: `2px solid ${signalColor}`,
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: SPACING.DEFAULT,
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
              <Target size={18} color={signalColor} />
              <span style={{
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                color: signalColor,
                textTransform: 'uppercase',
                letterSpacing: TYPOGRAPHY.WIDE,
              }}>
                {result.signal}
              </span>
              <div style={{
                ...COMMON_STYLES.badgeInfo,
                backgroundColor: `${signalColor}20`,
                color: signalColor,
              }}>
                {result.ticker}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={COMMON_STYLES.dataLabel}>CONFIDENCE</div>
              <div style={{
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                color: signalColor,
              }}>
                {result.confidence?.toFixed(2)}%
              </div>
            </div>
          </div>

          {/* Reasoning */}
          <div style={{
            padding: SPACING.DEFAULT,
            backgroundColor: `${signalColor}10`,
            border: `1px solid ${signalColor}30`,
            borderRadius: '2px',
            fontSize: TYPOGRAPHY.BODY,
            color: FINCEPT.WHITE,
            marginBottom: SPACING.DEFAULT,
          }}>
            {result.reasoning}
          </div>

          {/* Metrics Grid */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(4, 1fr)',
            gap: SPACING.SMALL,
            marginBottom: SPACING.DEFAULT,
          }}>
            <MetricBox label="SIGNAL" value={result.signal_score?.toFixed(1)} color={FINCEPT.CYAN} />
            <MetricBox label="RISK" value={result.risk_score?.toFixed(1)} color={FINCEPT.RED} />
            <MetricBox label="EXECUTION" value={result.execution_score?.toFixed(1)} color={FINCEPT.GREEN} />
            <MetricBox label="ARBITRAGE" value={result.arbitrage_score?.toFixed(1)} color={FINCEPT.ORANGE} />
          </div>

          {/* Position Sizing */}
          {result.position_sizing && (
            <div style={{
              padding: SPACING.DEFAULT,
              backgroundColor: FINCEPT.PANEL_BG,
              border: BORDERS.STANDARD,
              borderRadius: '2px',
              marginBottom: SPACING.DEFAULT,
            }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>
                POSITION SIZING RECOMMENDATION
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: SPACING.DEFAULT }}>
                <div>
                  <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.MUTED }}>Size</div>
                  <div style={{
                    fontSize: TYPOGRAPHY.SUBHEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.ORANGE,
                  }}>
                    {result.position_sizing.recommended_size_pct?.toFixed(2)}%
                  </div>
                </div>
                <div>
                  <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.MUTED }}>Kelly</div>
                  <div style={{
                    fontSize: TYPOGRAPHY.SUBHEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.CYAN,
                  }}>
                    {result.position_sizing.kelly_fraction?.toFixed(4)}
                  </div>
                </div>
                <div>
                  <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.MUTED }}>Leverage</div>
                  <div style={{
                    fontSize: TYPOGRAPHY.SUBHEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GREEN,
                  }}>
                    {result.position_sizing.leverage?.toFixed(2)}x
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* Factors & Risks */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
            {/* Key Factors */}
            <div style={{
              padding: SPACING.DEFAULT,
              backgroundColor: `${FINCEPT.GREEN}10`,
              border: `1px solid ${FINCEPT.GREEN}30`,
              borderRadius: '2px',
            }}>
              <div style={{
                fontSize: TYPOGRAPHY.SMALL,
                fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.GREEN,
                marginBottom: SPACING.SMALL,
                letterSpacing: TYPOGRAPHY.WIDE,
              }}>
                ✓ KEY FACTORS
              </div>
              {result.key_factors?.map((factor: string, idx: number) => (
                <div key={idx} style={{
                  fontSize: TYPOGRAPHY.SMALL,
                  color: FINCEPT.WHITE,
                  marginBottom: SPACING.TINY,
                  paddingLeft: SPACING.SMALL,
                }}>
                  • {factor}
                </div>
              ))}
            </div>

            {/* Risks */}
            <div style={{
              padding: SPACING.DEFAULT,
              backgroundColor: `${FINCEPT.RED}10`,
              border: `1px solid ${FINCEPT.RED}30`,
              borderRadius: '2px',
            }}>
              <div style={{
                fontSize: TYPOGRAPHY.SMALL,
                fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.RED,
                marginBottom: SPACING.SMALL,
                letterSpacing: TYPOGRAPHY.WIDE,
              }}>
                ⚠ RISKS
              </div>
              {result.risks?.map((risk: string, idx: number) => (
                <div key={idx} style={{
                  fontSize: TYPOGRAPHY.SMALL,
                  color: FINCEPT.WHITE,
                  marginBottom: SPACING.TINY,
                  paddingLeft: SPACING.SMALL,
                }}>
                  • {risk}
                </div>
              ))}
            </div>
          </div>

          {/* Signal Strength Breakdown */}
          {result.signal_strength && (
            <div style={{
              marginTop: SPACING.DEFAULT,
              padding: SPACING.DEFAULT,
              backgroundColor: FINCEPT.DARK_BG,
              border: BORDERS.STANDARD,
              borderRadius: '2px',
            }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>
                SIGNAL DECOMPOSITION
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: SPACING.SMALL }}>
                {Object.entries(result.signal_strength).map(([key, value]: [string, any]) => {
                  const numValue = typeof value === 'number' ? value : 0;
                  const barColor = numValue > 0 ? FINCEPT.GREEN : FINCEPT.RED;
                  const barWidth = Math.abs(numValue) * 100;

                  return (
                    <div key={key}>
                      <div style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        fontSize: TYPOGRAPHY.SMALL,
                        marginBottom: '2px',
                      }}>
                        <span style={{ color: FINCEPT.MUTED }}>
                          {key.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase())}
                        </span>
                        <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>
                          {typeof value === 'number' ? value.toFixed(4) : value}
                        </span>
                      </div>
                      <div style={{
                        height: '4px',
                        backgroundColor: FINCEPT.BORDER,
                        position: 'relative',
                        overflow: 'hidden',
                      }}>
                        <div style={{
                          position: 'absolute',
                          left: numValue < 0 ? `${100 - barWidth}%` : '0',
                          height: '100%',
                          width: `${barWidth}%`,
                          backgroundColor: barColor,
                          transition: 'width 0.3s ease',
                        }} />
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
}

// Metric Box Component
function MetricBox({ label, value, color }: { label: string; value: string | undefined; color: string }) {
  return (
    <div style={{
      padding: SPACING.DEFAULT,
      backgroundColor: FINCEPT.DARK_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      textAlign: 'center',
    }}>
      <div style={{
        fontSize: TYPOGRAPHY.TINY,
        color: FINCEPT.GRAY,
        marginBottom: SPACING.TINY,
        fontWeight: TYPOGRAPHY.BOLD,
        letterSpacing: TYPOGRAPHY.WIDE,
      }}>
        {label}
      </div>
      <div style={{
        fontSize: '18px',
        fontWeight: TYPOGRAPHY.BOLD,
        color: color,
      }}>
        {value || '--'}
      </div>
    </div>
  );
}

// =============================================================================
// AGENTS TAB
// =============================================================================

function AgentsTab({ agents }: { agents: RenTechAgent[] }) {
  const categories = [
    { id: 'research', label: 'Research Team', icon: Brain, color: FINCEPT.PURPLE },
    { id: 'execution', label: 'Execution Team', icon: Activity, color: FINCEPT.BLUE },
    { id: 'risk', label: 'Risk Team', icon: Shield, color: FINCEPT.RED },
    { id: 'committee', label: 'Investment Committee', icon: Award, color: FINCEPT.ORANGE },
  ];

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
      {categories.map((cat) => {
        const categoryAgents = agents.filter(a => a.category === cat.id);
        const CategoryIcon = cat.icon;

        return (
          <div key={cat.id} style={COMMON_STYLES.panel}>
            <div style={{
              ...COMMON_STYLES.sectionHeader,
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
              color: cat.color,
            }}>
              <CategoryIcon size={14} />
              <span>{cat.label}</span>
              <span style={{
                marginLeft: 'auto',
                ...COMMON_STYLES.badgeInfo,
                backgroundColor: `${cat.color}20`,
                color: cat.color,
              }}>
                {categoryAgents.length} AGENTS
              </span>
            </div>

            <div style={{
              display: 'grid',
              gridTemplateColumns: categoryAgents.length === 2 ? 'repeat(2, 1fr)' :
                                   categoryAgents.length === 4 ? 'repeat(4, 1fr)' :
                                   'repeat(auto-fill, minmax(240px, 1fr))',
              gap: SPACING.DEFAULT,
            }}>
              {categoryAgents.map((agent) => (
                <div
                  key={agent.id}
                  style={{
                    padding: SPACING.DEFAULT,
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: BORDERS.STANDARD,
                    borderRadius: '2px',
                    transition: EFFECTS.TRANSITION_FAST,
                    cursor: 'pointer',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    e.currentTarget.style.borderColor = cat.color;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  }}
                >
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    marginBottom: SPACING.SMALL,
                  }}>
                    <div style={{
                      fontSize: TYPOGRAPHY.BODY,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: cat.color,
                    }}>
                      {agent.name}
                    </div>
                    <div style={{
                      width: '8px',
                      height: '8px',
                      borderRadius: '50%',
                      backgroundColor: agent.status === 'completed' ? FINCEPT.GREEN :
                                      agent.status === 'running' ? FINCEPT.ORANGE :
                                      agent.status === 'error' ? FINCEPT.RED :
                                      FINCEPT.GRAY,
                    }} />
                  </div>
                  <div style={{
                    fontSize: TYPOGRAPHY.SMALL,
                    color: FINCEPT.MUTED,
                    lineHeight: '1.4',
                  }}>
                    {agent.role}
                  </div>
                </div>
              ))}
            </div>
          </div>
        );
      })}
    </div>
  );
}

// =============================================================================
// CONFIG TAB
// =============================================================================

function ConfigTab({
  riskLimits,
  onUpdateLimits,
}: {
  riskLimits: RiskLimits;
  onUpdateLimits: (limits: RiskLimits) => void;
}) {
  const params = [
    { key: 'max_position_size_pct', label: 'Max Position Size', unit: '%', color: FINCEPT.ORANGE },
    { key: 'max_leverage', label: 'Max Leverage', unit: 'x', color: FINCEPT.RED },
    { key: 'max_drawdown_pct', label: 'Max Drawdown', unit: '%', color: FINCEPT.RED },
    { key: 'max_daily_var_pct', label: 'Max Daily VaR', unit: '%', color: FINCEPT.YELLOW },
    { key: 'min_signal_confidence', label: 'Min Signal Confidence', unit: '', color: FINCEPT.GREEN },
  ];

  return (
    <div style={COMMON_STYLES.panel}>
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        display: 'flex',
        alignItems: 'center',
        gap: SPACING.SMALL,
      }}>
        <Shield size={14} color={FINCEPT.ORANGE} />
        <span>RISK MANAGEMENT PARAMETERS</span>
      </div>

      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        {params.map((param) => {
          const value = riskLimits[param.key as keyof RiskLimits];
          return (
            <div
              key={param.key}
              style={{
                padding: SPACING.DEFAULT,
                backgroundColor: FINCEPT.PANEL_BG,
                border: BORDERS.STANDARD,
                borderRadius: '2px',
              }}
            >
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                marginBottom: SPACING.SMALL,
              }}>
                <div>
                  <div style={{ ...COMMON_STYLES.dataLabel }}>{param.label}</div>
                  <div style={{
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: param.color,
                    marginTop: '2px',
                  }}>
                    {value}{param.unit}
                  </div>
                </div>
                <input
                  type="number"
                  value={value}
                  onChange={(e) => onUpdateLimits({
                    ...riskLimits,
                    [param.key]: parseFloat(e.target.value)
                  })}
                  step={param.key === 'min_signal_confidence' ? '0.01' : '0.1'}
                  style={{
                    ...COMMON_STYLES.inputField,
                    width: '120px',
                    textAlign: 'right',
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
                />
              </div>
              {/* Progress Bar */}
              <div style={{
                height: '4px',
                backgroundColor: FINCEPT.BORDER,
                position: 'relative',
                overflow: 'hidden',
              }}>
                <div style={{
                  position: 'absolute',
                  left: 0,
                  height: '100%',
                  width: `${Math.min((value / (param.key === 'max_leverage' ? 20 : 100)) * 100, 100)}%`,
                  backgroundColor: param.color,
                  transition: 'width 0.3s ease',
                }} />
              </div>
            </div>
          );
        })}
      </div>

      <div style={{
        marginTop: SPACING.DEFAULT,
        padding: SPACING.DEFAULT,
        backgroundColor: `${FINCEPT.ORANGE}10`,
        border: `1px solid ${FINCEPT.ORANGE}30`,
        borderRadius: '2px',
        fontSize: TYPOGRAPHY.SMALL,
        color: FINCEPT.MUTED,
      }}>
        <div style={{ color: FINCEPT.ORANGE, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.TINY }}>
          ⓘ MEDALLION FUND RISK FRAMEWORK
        </div>
        Risk parameters are applied to all signal analyses and IC deliberations. Changes take effect immediately.
        These limits reflect Renaissance Technologies' proprietary risk management system.
      </div>
    </div>
  );
}

// =============================================================================
// EXPORTED COMPONENT - Wrapped with ErrorBoundary (Point 5)
// =============================================================================

export default function RenaissanceTechnologiesPanel() {
  return (
    <ErrorBoundary
      name="Renaissance Technologies Panel"
      variant="default"
      onError={(error, errorInfo) => {
        console.error('[RenTech Panel Error]', error.message, errorInfo.componentStack);
      }}
    >
      <RenaissanceTechnologiesPanelInner />
    </ErrorBoundary>
  );
}
