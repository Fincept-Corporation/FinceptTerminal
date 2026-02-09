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
 */

import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
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

// =============================================================================
// MAIN COMPONENT
// =============================================================================

export default function RenaissanceTechnologiesPanel() {
  // State
  const [activeTab, setActiveTab] = useState<'analysis' | 'agents' | 'config'>('analysis');
  const [ticker, setTicker] = useState('');
  const [signalType, setSignalType] = useState('momentum');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysisResult, setAnalysisResult] = useState<SignalAnalysisResult | null>(null);
  const [currentTime, setCurrentTime] = useState(new Date());
  const [showAdvanced, setShowAdvanced] = useState(false);

  // Config state
  const [riskLimits, setRiskLimits] = useState<RiskLimits>({
    max_position_size_pct: 5.0,
    max_leverage: 12.5,
    max_drawdown_pct: 15.0,
    max_daily_var_pct: 2.0,
    min_signal_confidence: 0.5075,
  });

  // Update time
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Run signal analysis
  const runSignalAnalysis = async () => {
    if (!ticker) return;

    setIsAnalyzing(true);
    setAnalysisResult(null);

    try {
      // Send only user inputs - backend will calculate signal metrics from real data
      const cliInput = {
        ticker,
        signal_type: signalType,
        trade_value: 1_000_000, // Default $1M trade
        period: '1y',
        include_deliberation: true, // Include IC deliberation
      };

      const result = await invoke<string>('execute_renaissance_cli', {
        command: 'analyze_signal',
        data: JSON.stringify(cliInput),
      });

      const parsed = JSON.parse(result);

      if (parsed.success) {
        setAnalysisResult(parsed.data || parsed);
      } else {
        setAnalysisResult({
          success: false,
          error: parsed.error || 'Analysis failed',
        });
      }
    } catch (error: any) {
      console.error('Analysis error:', error);
      setAnalysisResult({
        success: false,
        error: error.message || 'Analysis failed',
      });
    } finally {
      setIsAnalyzing(false);
    }
  };

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
                onClick={() => setActiveTab(tab)}
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
            setTicker={setTicker}
            signalType={signalType}
            setSignalType={setSignalType}
            isAnalyzing={isAnalyzing}
            analysisResult={analysisResult}
            onRunAnalysis={runSignalAnalysis}
            showAdvanced={showAdvanced}
            setShowAdvanced={setShowAdvanced}
          />
        )}

        {activeTab === 'agents' && (
          <AgentsTab agents={AGENTS} />
        )}

        {activeTab === 'config' && (
          <ConfigTab
            riskLimits={riskLimits}
            onUpdateLimits={setRiskLimits}
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
            onChange={(e) => setTicker(e.target.value.toUpperCase())}
            placeholder="AAPL"
            style={{
              ...COMMON_STYLES.inputField,
              marginTop: SPACING.SMALL,
              textTransform: 'uppercase',
              width: '200px',
            }}
            onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
            onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
          />
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
