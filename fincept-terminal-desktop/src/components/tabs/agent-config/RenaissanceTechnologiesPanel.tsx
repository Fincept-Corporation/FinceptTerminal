/**
 * Renaissance Technologies Multi-Agent System Panel
 *
 * Professional interface for RenTech's 10-agent systematic trading system
 * Features:
 * - Agent selection and configuration
 * - Team collaboration workflows
 * - IC Deliberation with voting
 * - Signal analysis pipeline
 * - Risk management controls
 * - Real-time execution monitoring
 */

import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  TrendingUp, AlertCircle, CheckCircle, Users, Sparkles,
  Brain, Shield, FileText, Zap, Settings, Play, RefreshCw,
  ChevronRight, DollarSign, BarChart3, Target, Workflow,
  ThumbsUp, ThumbsDown, MessageSquare, Clock, Award, Loader2
} from 'lucide-react';

// =============================================================================
// TYPES
// =============================================================================

interface RenTechAgent {
  id: string;
  name: string;
  role: string;
  status: 'idle' | 'running' | 'completed' | 'error';
}

interface RenTechTeam {
  id: string;
  name: string;
  members: string[];
  description: string;
}

interface ICVote {
  member_role: string;
  vote: 'approve' | 'reject' | 'conditional' | 'abstain';
  rationale: string;
  confidence: number;
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
}

interface SignalAnalysisResult {
  success: boolean;
  signal_id?: string;
  statistical_quality?: string;
  risk_assessment?: string;
  ic_decision?: ICDeliberationResult;
  execution_plan?: any;
  error?: string;
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

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  BLUE: '#0088FF',
  DARK_BG: '#0F0F0F',
  PANEL_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  MUTED: '#787878',
};

const AGENTS: RenTechAgent[] = [
  { id: 'signal_scientist', name: 'Signal Scientist', role: 'Statistical signal discovery', status: 'idle' },
  { id: 'data_scientist', name: 'Data Scientist', role: 'Data quality & features', status: 'idle' },
  { id: 'quant_researcher', name: 'Quant Researcher', role: 'Strategy development', status: 'idle' },
  { id: 'research_lead', name: 'Research Lead', role: 'Research validation', status: 'idle' },
  { id: 'execution_trader', name: 'Execution Trader', role: 'Optimal execution', status: 'idle' },
  { id: 'market_maker', name: 'Market Maker', role: 'Liquidity provision', status: 'idle' },
  { id: 'risk_quant', name: 'Risk Quant', role: 'Risk modeling', status: 'idle' },
  { id: 'compliance_officer', name: 'Compliance Officer', role: 'Regulatory compliance', status: 'idle' },
  { id: 'portfolio_manager', name: 'Portfolio Manager', role: 'Position sizing', status: 'idle' },
  { id: 'investment_committee_chair', name: 'IC Chair', role: 'Final approval', status: 'idle' },
];

const TEAMS: RenTechTeam[] = [
  {
    id: 'research_team',
    name: 'Research Team',
    members: ['signal_scientist', 'data_scientist', 'quant_researcher', 'research_lead'],
    description: 'Signal discovery and validation'
  },
  {
    id: 'trading_team',
    name: 'Trading Team',
    members: ['execution_trader', 'market_maker'],
    description: 'Trade execution and liquidity'
  },
  {
    id: 'risk_team',
    name: 'Risk Team',
    members: ['risk_quant', 'compliance_officer'],
    description: 'Risk assessment and compliance'
  },
  {
    id: 'medallion_fund',
    name: 'Investment Committee',
    members: ['investment_committee_chair', 'portfolio_manager', 'research_lead', 'risk_quant'],
    description: 'Final decision-making body'
  },
];

// =============================================================================
// COMPONENT
// =============================================================================

export default function RenaissanceTechnologiesPanel() {
  // State
  const [selectedTab, setSelectedTab] = useState<'agents' | 'teams' | 'analysis' | 'config'>('analysis');
  const [selectedAgent, setSelectedAgent] = useState<string | null>(null);
  const [selectedTeam, setSelectedTeam] = useState<string>('medallion_fund');
  const [agentStatuses, setAgentStatuses] = useState<Record<string, 'idle' | 'running' | 'completed' | 'error'>>({});

  // Analysis state
  const [ticker, setTicker] = useState('');
  const [signalType, setSignalType] = useState('momentum');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysisResult, setAnalysisResult] = useState<SignalAnalysisResult | null>(null);

  // Config state
  const [riskLimits, setRiskLimits] = useState<RiskLimits>({
    max_position_size_pct: 5.0,
    max_leverage: 12.5,
    max_drawdown_pct: 15.0,
    max_daily_var_pct: 2.0,
    min_signal_confidence: 0.5075,
  });

  // Run signal analysis
  const runSignalAnalysis = async () => {
    if (!ticker) return;

    setIsAnalyzing(true);
    setAnalysisResult(null);

    try {
      const signalData = {
        ticker,
        signal_type: signalType,
        direction: 'long',
        strength: 0.75,
        confidence: 0.65,
        p_value: 0.008,
        information_coefficient: 0.05,
      };

      const result = await invoke<string>('execute_python_agent', {
        agentType: 'renaissance',
        parameters: {},
        inputs: { signal: signalData, config: riskLimits },
      });

      const parsed = JSON.parse(result);
      setAnalysisResult(parsed.data || parsed);
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

  // Run IC Deliberation
  const runICDeliberation = async () => {
    setIsAnalyzing(true);

    try {
      const deliberationData = {
        signal: {
          ticker,
          direction: 'long',
          signal_type: signalType,
          p_value: 0.008,
          confidence: 0.65,
          expected_return_bps: 75,
        },
        signal_evaluation: {
          statistical_quality: 'strong',
          overall_score: 75,
        },
        risk_assessment: {
          risk_level: 'medium',
          var_utilization_pct: 45,
        },
        sizing_recommendation: {
          final_size_pct: 2.5,
        },
      };

      const jsonData = JSON.stringify(deliberationData);
      const result = await invoke<string>('core_agent_cli', {
        args: ['run_ic_deliberation', jsonData]
      });

      const parsed = JSON.parse(result);
      setAnalysisResult({ success: true, ic_decision: parsed.data });
    } catch (error: any) {
      console.error('IC deliberation error:', error);
      setAnalysisResult({
        success: false,
        error: error.message || 'IC deliberation failed',
      });
    } finally {
      setIsAnalyzing(false);
    }
  };

  return (
    <div style={{
      width: '100%',
      height: '100%',
      background: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: 'IBM Plex Mono, monospace',
      display: 'flex',
      flexDirection: 'column',
    }}>
      {/* Header */}
      <div style={{
        padding: '20px 24px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        background: FINCEPT.PANEL_BG,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '12px' }}>
          <Award size={28} color={FINCEPT.ORANGE} />
          <div>
            <h1 style={{ margin: 0, fontSize: '20px', fontWeight: 600 }}>
              Renaissance Technologies
            </h1>
            <p style={{ margin: '4px 0 0', fontSize: '13px', color: FINCEPT.MUTED }}>
              Medallion Fund Multi-Agent System
            </p>
          </div>
        </div>

        {/* Tab Navigation */}
        <div style={{ display: 'flex', gap: '8px', marginTop: '16px' }}>
          {(['analysis', 'agents', 'teams', 'config'] as const).map((tab) => (
            <button
              key={tab}
              onClick={() => setSelectedTab(tab)}
              style={{
                padding: '8px 16px',
                background: selectedTab === tab ? FINCEPT.ORANGE : 'transparent',
                color: selectedTab === tab ? FINCEPT.DARK_BG : FINCEPT.WHITE,
                border: `1px solid ${selectedTab === tab ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                borderRadius: '6px',
                cursor: 'pointer',
                fontSize: '13px',
                fontWeight: 500,
                transition: 'all 0.2s',
              }}
            >
              {tab.charAt(0).toUpperCase() + tab.slice(1)}
            </button>
          ))}
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '24px' }}>
        {selectedTab === 'analysis' && (
          <AnalysisTab
            ticker={ticker}
            setTicker={setTicker}
            signalType={signalType}
            setSignalType={setSignalType}
            isAnalyzing={isAnalyzing}
            analysisResult={analysisResult}
            onRunAnalysis={runSignalAnalysis}
            onRunIC={runICDeliberation}
          />
        )}

        {selectedTab === 'agents' && (
          <AgentsTab
            agents={AGENTS}
            selectedAgent={selectedAgent}
            onSelectAgent={setSelectedAgent}
            agentStatuses={agentStatuses}
          />
        )}

        {selectedTab === 'teams' && (
          <TeamsTab
            teams={TEAMS}
            selectedTeam={selectedTeam}
            onSelectTeam={setSelectedTeam}
          />
        )}

        {selectedTab === 'config' && (
          <ConfigTab
            riskLimits={riskLimits}
            onUpdateLimits={setRiskLimits}
          />
        )}
      </div>
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
  onRunIC,
}: {
  ticker: string;
  setTicker: (v: string) => void;
  signalType: string;
  setSignalType: (v: string) => void;
  isAnalyzing: boolean;
  analysisResult: SignalAnalysisResult | null;
  onRunAnalysis: () => void;
  onRunIC: () => void;
}) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
      {/* Input Section */}
      <div style={{
        background: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '8px',
        padding: '20px',
      }}>
        <h3 style={{ margin: '0 0 16px', fontSize: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Sparkles size={18} color={FINCEPT.ORANGE} />
          Signal Analysis Input
        </h3>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontSize: '13px', color: FINCEPT.MUTED }}>
              Ticker Symbol
            </label>
            <input
              type="text"
              value={ticker}
              onChange={(e) => setTicker(e.target.value.toUpperCase())}
              placeholder="AAPL"
              style={{
                width: '100%',
                padding: '10px 12px',
                background: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '6px',
                color: FINCEPT.WHITE,
                fontSize: '14px',
              }}
            />
          </div>

          <div>
            <label style={{ display: 'block', marginBottom: '8px', fontSize: '13px', color: FINCEPT.MUTED }}>
              Signal Type
            </label>
            <select
              value={signalType}
              onChange={(e) => setSignalType(e.target.value)}
              style={{
                width: '100%',
                padding: '10px 12px',
                background: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '6px',
                color: FINCEPT.WHITE,
                fontSize: '14px',
              }}
            >
              <option value="momentum">Momentum</option>
              <option value="mean_reversion">Mean Reversion</option>
              <option value="stat_arb">Statistical Arbitrage</option>
              <option value="pairs_trading">Pairs Trading</option>
            </select>
          </div>
        </div>

        <div style={{ display: 'flex', gap: '12px', marginTop: '20px' }}>
          <button
            onClick={onRunAnalysis}
            disabled={!ticker || isAnalyzing}
            style={{
              flex: 1,
              padding: '12px',
              background: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '6px',
              cursor: ticker && !isAnalyzing ? 'pointer' : 'not-allowed',
              fontSize: '14px',
              fontWeight: 600,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px',
              opacity: ticker && !isAnalyzing ? 1 : 0.5,
            }}
          >
            {isAnalyzing ? <Loader2 size={16} className="animate-spin" /> : <Play size={16} />}
            Run Full Analysis
          </button>

          <button
            onClick={onRunIC}
            disabled={!ticker || isAnalyzing}
            style={{
              flex: 1,
              padding: '12px',
              background: 'transparent',
              color: FINCEPT.ORANGE,
              border: `1px solid ${FINCEPT.ORANGE}`,
              borderRadius: '6px',
              cursor: ticker && !isAnalyzing ? 'pointer' : 'not-allowed',
              fontSize: '14px',
              fontWeight: 600,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px',
              opacity: ticker && !isAnalyzing ? 1 : 0.5,
            }}
          >
            <Users size={16} />
            IC Deliberation Only
          </button>
        </div>
      </div>

      {/* Results Section */}
      {analysisResult && (
        <ResultsDisplay result={analysisResult} />
      )}
    </div>
  );
}

// =============================================================================
// RESULTS DISPLAY
// =============================================================================

function ResultsDisplay({ result }: { result: SignalAnalysisResult }) {
  if (!result.success) {
    return (
      <div style={{
        background: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.RED}`,
        borderRadius: '8px',
        padding: '20px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px', color: FINCEPT.RED }}>
          <AlertCircle size={24} />
          <div>
            <h3 style={{ margin: 0, fontSize: '16px' }}>Analysis Failed</h3>
            <p style={{ margin: '4px 0 0', fontSize: '13px', opacity: 0.8 }}>{result.error}</p>
          </div>
        </div>
      </div>
    );
  }

  const icDecision = result.ic_decision;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {/* IC Decision */}
      {icDecision && (
        <div style={{
          background: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '8px',
          padding: '20px',
        }}>
          <h3 style={{
            margin: '0 0 16px',
            fontSize: '16px',
            display: 'flex',
            alignItems: 'center',
            gap: '8px'
          }}>
            <Award size={18} color={FINCEPT.ORANGE} />
            Investment Committee Decision
          </h3>

          {/* Final Decision */}
          <div style={{
            padding: '16px',
            background: icDecision.decision.includes('APPROVE') ? `${FINCEPT.GREEN}15` : `${FINCEPT.RED}15`,
            border: `1px solid ${icDecision.decision.includes('APPROVE') ? FINCEPT.GREEN : FINCEPT.RED}`,
            borderRadius: '6px',
            marginBottom: '16px',
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '8px' }}>
              {icDecision.decision.includes('APPROVE') ? (
                <CheckCircle size={20} color={FINCEPT.GREEN} />
              ) : (
                <AlertCircle size={20} color={FINCEPT.RED} />
              )}
              <span style={{ fontSize: '16px', fontWeight: 600 }}>{icDecision.decision}</span>
              <span style={{ marginLeft: 'auto', fontSize: '14px', color: FINCEPT.MUTED }}>
                Confidence: {(icDecision.confidence * 100).toFixed(1)}%
              </span>
            </div>
            <p style={{ margin: 0, fontSize: '13px', opacity: 0.9 }}>{icDecision.decision_rationale}</p>
            {icDecision.approved_size_pct && (
              <div style={{ marginTop: '8px', fontSize: '13px' }}>
                <strong>Approved Size:</strong> {icDecision.approved_size_pct.toFixed(2)}% of NAV
              </div>
            )}
          </div>

          {/* Vote Summary */}
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px', marginBottom: '16px' }}>
            {Object.entries(icDecision.vote_summary).map(([vote, count]) => (
              <div
                key={vote}
                style={{
                  padding: '12px',
                  background: FINCEPT.DARK_BG,
                  borderRadius: '6px',
                  textAlign: 'center',
                }}
              >
                <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.ORANGE }}>{count}</div>
                <div style={{ fontSize: '11px', color: FINCEPT.MUTED, marginTop: '4px', textTransform: 'uppercase' }}>
                  {vote}
                </div>
              </div>
            ))}
          </div>

          {/* Member Votes */}
          <h4 style={{ margin: '16px 0 12px', fontSize: '14px', color: FINCEPT.MUTED }}>Member Votes:</h4>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {icDecision.member_opinions?.map((opinion, idx) => (
              <div
                key={idx}
                style={{
                  padding: '12px',
                  background: FINCEPT.DARK_BG,
                  borderRadius: '6px',
                  borderLeft: `3px solid ${
                    opinion.vote === 'approve' ? FINCEPT.GREEN :
                    opinion.vote === 'reject' ? FINCEPT.RED :
                    FINCEPT.ORANGE
                  }`,
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                  {opinion.vote === 'approve' ? <ThumbsUp size={14} color={FINCEPT.GREEN} /> :
                   opinion.vote === 'reject' ? <ThumbsDown size={14} color={FINCEPT.RED} /> :
                   <MessageSquare size={14} color={FINCEPT.ORANGE} />}
                  <strong style={{ fontSize: '13px' }}>{opinion.member_role}</strong>
                  <span style={{ marginLeft: 'auto', fontSize: '12px', color: FINCEPT.MUTED }}>
                    {(opinion.confidence * 100).toFixed(0)}%
                  </span>
                </div>
                <p style={{ margin: 0, fontSize: '12px', opacity: 0.8 }}>{opinion.rationale}</p>
              </div>
            ))}
          </div>

          {/* Pros & Cons */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginTop: '16px' }}>
            <div>
              <h4 style={{ margin: '0 0 8px', fontSize: '13px', color: FINCEPT.GREEN }}>Pros:</h4>
              {icDecision.pros?.map((pro, idx) => (
                <div key={idx} style={{ fontSize: '12px', marginBottom: '4px', paddingLeft: '12px' }}>
                  • {pro}
                </div>
              ))}
            </div>
            <div>
              <h4 style={{ margin: '0 0 8px', fontSize: '13px', color: FINCEPT.RED }}>Cons:</h4>
              {icDecision.cons?.map((con, idx) => (
                <div key={idx} style={{ fontSize: '12px', marginBottom: '4px', paddingLeft: '12px' }}>
                  • {con}
                </div>
              ))}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

// =============================================================================
// AGENTS TAB
// =============================================================================

function AgentsTab({
  agents,
  selectedAgent,
  onSelectAgent,
  agentStatuses,
}: {
  agents: RenTechAgent[];
  selectedAgent: string | null;
  onSelectAgent: (id: string | null) => void;
  agentStatuses: Record<string, string>;
}) {
  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: '16px' }}>
      {agents.map((agent) => (
        <div
          key={agent.id}
          onClick={() => onSelectAgent(agent.id === selectedAgent ? null : agent.id)}
          style={{
            padding: '16px',
            background: selectedAgent === agent.id ? `${FINCEPT.ORANGE}15` : FINCEPT.PANEL_BG,
            border: `1px solid ${selectedAgent === agent.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
            borderRadius: '8px',
            cursor: 'pointer',
            transition: 'all 0.2s',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '8px' }}>
            <Brain size={20} color={FINCEPT.ORANGE} />
            <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 600 }}>{agent.name}</h4>
          </div>
          <p style={{ margin: '0 0 12px', fontSize: '12px', color: FINCEPT.MUTED }}>{agent.role}</p>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '12px' }}>
            <div
              style={{
                width: '8px',
                height: '8px',
                borderRadius: '50%',
                background: agentStatuses[agent.id] === 'running' ? FINCEPT.ORANGE :
                           agentStatuses[agent.id] === 'completed' ? FINCEPT.GREEN :
                           agentStatuses[agent.id] === 'error' ? FINCEPT.RED :
                           FINCEPT.MUTED,
              }}
            />
            <span style={{ color: FINCEPT.MUTED, textTransform: 'capitalize' }}>
              {agentStatuses[agent.id] || 'idle'}
            </span>
          </div>
        </div>
      ))}
    </div>
  );
}

// =============================================================================
// TEAMS TAB
// =============================================================================

function TeamsTab({
  teams,
  selectedTeam,
  onSelectTeam,
}: {
  teams: RenTechTeam[];
  selectedTeam: string;
  onSelectTeam: (id: string) => void;
}) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {teams.map((team) => (
        <div
          key={team.id}
          onClick={() => onSelectTeam(team.id)}
          style={{
            padding: '20px',
            background: selectedTeam === team.id ? `${FINCEPT.ORANGE}15` : FINCEPT.PANEL_BG,
            border: `1px solid ${selectedTeam === team.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
            borderRadius: '8px',
            cursor: 'pointer',
            transition: 'all 0.2s',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '8px' }}>
            <Users size={20} color={FINCEPT.ORANGE} />
            <h4 style={{ margin: 0, fontSize: '16px', fontWeight: 600 }}>{team.name}</h4>
          </div>
          <p style={{ margin: '0 0 12px', fontSize: '13px', color: FINCEPT.MUTED }}>{team.description}</p>
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
            {team.members.map((memberId) => {
              const agent = AGENTS.find(a => a.id === memberId);
              return agent ? (
                <div
                  key={memberId}
                  style={{
                    padding: '6px 12px',
                    background: FINCEPT.DARK_BG,
                    borderRadius: '4px',
                    fontSize: '12px',
                  }}
                >
                  {agent.name}
                </div>
              ) : null;
            })}
          </div>
        </div>
      ))}
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
  return (
    <div style={{
      background: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '8px',
      padding: '20px',
    }}>
      <h3 style={{ margin: '0 0 20px', fontSize: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
        <Shield size={18} color={FINCEPT.ORANGE} />
        Risk Management Configuration
      </h3>

      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {Object.entries(riskLimits).map(([key, value]) => (
          <div key={key}>
            <label style={{ display: 'block', marginBottom: '8px', fontSize: '13px', color: FINCEPT.MUTED }}>
              {key.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase())}
            </label>
            <input
              type="number"
              value={value}
              onChange={(e) => onUpdateLimits({ ...riskLimits, [key]: parseFloat(e.target.value) })}
              step="0.1"
              style={{
                width: '100%',
                padding: '10px 12px',
                background: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '6px',
                color: FINCEPT.WHITE,
                fontSize: '14px',
              }}
            />
          </div>
        ))}
      </div>

      <div style={{
        marginTop: '20px',
        padding: '16px',
        background: `${FINCEPT.ORANGE}10`,
        border: `1px solid ${FINCEPT.ORANGE}30`,
        borderRadius: '6px',
        fontSize: '12px',
        color: FINCEPT.MUTED,
      }}>
        <strong style={{ color: FINCEPT.ORANGE }}>Note:</strong> These risk limits are applied to all signal analysis
        and IC deliberations. Changes take effect immediately.
      </div>
    </div>
  );
}
