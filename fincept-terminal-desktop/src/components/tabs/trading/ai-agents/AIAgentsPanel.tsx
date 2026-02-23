/**
 * AI Agents Panel
 *
 * Main panel for Agno AI Trading Agents in the Trading tab.
 * Provides interface for creating, configuring, and interacting with AI agents.
 */

import React, { useState, useEffect } from 'react';
import {
  Bot, Brain, TrendingUp, Shield, PieChart, MessageSquare,
  Settings, Play, Loader2, AlertCircle, CheckCircle, Sparkles,
  ChevronDown, ChevronUp, Zap, Target, Pause, Square, Activity,
  TrendingDown
} from 'lucide-react';
import agnoTradingService, {
  type AgentConfig,
  type AgentInfo,
  type MarketAnalysis,
  type TradeSignal,
  type RiskAnalysis,
  type TradeExecutionResult
} from '../../../../services/trading/agnoTradingService';
import { AgentConfigurationUI } from './AgentConfigurationUI';
import { CompetitionPanel } from './CompetitionPanel';
import { DebateArenaPanel } from './DebateArenaPanel';
import { DecisionFeedPanel } from './DecisionFeedPanel';
import { TradeHistoryPanel } from './TradeHistoryPanel';

// Fincept color palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface AIAgentsPanelProps {
  selectedSymbol: string;
  portfolioData?: {
    positions: any[];
    total_value: number;
  };
}

export function AIAgentsPanel({ selectedSymbol, portfolioData }: AIAgentsPanelProps) {
  const [activeView, setActiveView] = useState<'quick-actions' | 'create-agent' | 'manage-agents' | 'competition'>('quick-actions');
  const [isLoading, setIsLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  // Quick Actions State
  const [selectedModel, setSelectedModel] = useState('');
  const [analysisType, setAnalysisType] = useState<'quick' | 'comprehensive' | 'deep'>('comprehensive');
  const [strategy, setStrategy] = useState<'momentum' | 'breakout' | 'reversal' | 'trend_following'>('momentum');
  const [hasConfiguredKeys, setHasConfiguredKeys] = useState(false);
  const [availableModels, setAvailableModels] = useState<Array<{ value: string; label: string }>>([]);

  // Agents State
  const [agents, setAgents] = useState<AgentInfo[]>([]);
  const [selectedAgent, setSelectedAgent] = useState<string | null>(null);

  // ============================================================================
  // Check for configured API keys on mount
  // ============================================================================
  useEffect(() => {
    const checkAPIKeys = async () => {
      try {
        const { sqliteService } = await import('../../../../services/core/sqliteService');
        const configs = await sqliteService.getLLMConfigs();

        const configured = configs.filter(c => c.api_key && c.api_key.trim().length > 0);
        setHasConfiguredKeys(configured.length > 0);

        if (configured.length > 0) {
          // Build available models list from configured providers
          const models = configured.map(c => ({
            value: `${c.provider}:${c.model}`,
            label: `${c.provider.charAt(0).toUpperCase() + c.provider.slice(1)} - ${c.model}`
          }));
          setAvailableModels(models);

          // Auto-select first configured model
          setSelectedModel(models[0].value);
        }
      } catch (err) {
        console.error('Failed to check API keys:', err);
      }
    };

    checkAPIKeys();
  }, []);

  // ============================================================================
  // Quick Actions
  // ============================================================================

  const handleAnalyzeMarket = async () => {
    setIsLoading(true);
    setError(null);
    setResult(null);

    try {
      const response = await agnoTradingService.analyzeMarket(
        selectedSymbol,
        selectedModel,
        analysisType
      );

      if (response.success && response.data) {
        setResult({
          type: 'market_analysis',
          data: response.data
        });
      } else {
        setError(response.error || 'Analysis failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsLoading(false);
    }
  };

  const handleGenerateSignal = async () => {
    setIsLoading(true);
    setError(null);
    setResult(null);

    try {
      const response = await agnoTradingService.generateTradeSignal(
        selectedSymbol,
        strategy,
        selectedModel
      );

      if (response.success && response.data) {
        setResult({
          type: 'trade_signal',
          data: response.data
        });
      } else {
        setError(response.error || 'Signal generation failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsLoading(false);
    }
  };

  const handleAssessRisk = async () => {
    if (!portfolioData) {
      setError('Portfolio data not available');
      return;
    }

    setIsLoading(true);
    setError(null);
    setResult(null);

    try {
      const response = await agnoTradingService.manageRisk(
        portfolioData,
        selectedModel,
        'moderate'
      );

      if (response.success && response.data) {
        setResult({
          type: 'risk_analysis',
          data: response.data
        });
      } else {
        setError(response.error || 'Risk analysis failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsLoading(false);
    }
  };

  const handleStartCompetition = async () => {
    if (availableModels.length < 2) {
      setError('Need at least 2 configured models to start a competition');
      return;
    }

    setIsLoading(true);
    setError(null);
    setResult(null);

    try {
      // Create competition with all available models
      const modelStrings = availableModels.map(m => m.value);
      const createResponse = await agnoTradingService.createCompetition(
        `${selectedSymbol}_${Date.now()}`,
        modelStrings,
        'trading'
      );

      if (!createResponse.success || !createResponse.data) {
        throw new Error(createResponse.error || 'Failed to create competition');
      }

      const teamId = createResponse.data.team_id;

      // Run competition
      const runResponse = await agnoTradingService.runCompetition(
        teamId,
        selectedSymbol,
        'analyze'
      );

      if (runResponse.success && runResponse.data) {
        setResult({
          type: 'competition',
          data: runResponse.data
        });
      } else {
        setError(runResponse.error || 'Competition failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsLoading(false);
    }
  };

  const handleExecuteTrade = async () => {
    if (!result || result.type !== 'trade_signal') {
      setError('Generate a trade signal first');
      return;
    }

    if (!portfolioData) {
      setError('Portfolio data not available');
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Extract signal data
      const signalData = result.data;

      // Create portfolio snapshot
      const portfolio = {
        total_value: portfolioData.total_value,
        positions: portfolioData.positions || [],
        daily_pnl: 0
      };

      // Execute trade via trade executor
      const response = await agnoTradingService.executeTrade(
        signalData,
        portfolio,
        'quick_action_agent',
        selectedModel
      );

      if (response.success && response.data) {
        setResult({
          type: 'trade_execution',
          data: response.data
        });
      } else {
        setError(response.error || 'Trade execution failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsLoading(false);
    }
  };

  // ============================================================================
  // Render
  // ============================================================================

  return (
    <div
      style={{
        background: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '4px',
        height: '100%',
        display: 'flex',
        flexDirection: 'column'
      }}
    >
      {/* Header */}
      <div
        style={{
          background: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          padding: '6px 8px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between'
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Sparkles size={14} color={FINCEPT.ORANGE} />
          <h3 style={{
            color: FINCEPT.WHITE,
            fontSize: '11px',
            fontWeight: '600',
            margin: 0,
            letterSpacing: '0.5px'
          }}>
            AI AGENTS
          </h3>
        </div>

        <div style={{ display: 'flex', gap: '3px' }}>
          <button
            onClick={() => setActiveView('quick-actions')}
            style={{
              background: activeView === 'quick-actions' ? FINCEPT.ORANGE : 'transparent',
              color: activeView === 'quick-actions' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: `1px solid ${activeView === 'quick-actions' ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              padding: '4px 8px',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: '600',
              cursor: 'pointer',
              transition: 'all 0.15s',
              letterSpacing: '0.3px'
            }}
          >
            QUICK
          </button>

          <button
            onClick={() => setActiveView('create-agent')}
            style={{
              background: activeView === 'create-agent' ? FINCEPT.ORANGE : 'transparent',
              color: activeView === 'create-agent' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: `1px solid ${activeView === 'create-agent' ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              padding: '4px 8px',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: '600',
              cursor: 'pointer',
              transition: 'all 0.15s',
              letterSpacing: '0.3px'
            }}
          >
            CREATE
          </button>

          <button
            onClick={() => setActiveView('manage-agents')}
            style={{
              background: activeView === 'manage-agents' ? FINCEPT.ORANGE : 'transparent',
              color: activeView === 'manage-agents' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: `1px solid ${activeView === 'manage-agents' ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              padding: '4px 8px',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: '600',
              cursor: 'pointer',
              transition: 'all 0.15s',
              letterSpacing: '0.3px'
            }}
          >
            MANAGE
          </button>

          <button
            onClick={() => setActiveView('competition')}
            style={{
              background: activeView === 'competition' ? FINCEPT.ORANGE : 'transparent',
              color: activeView === 'competition' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: `1px solid ${activeView === 'competition' ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              padding: '4px 8px',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: '600',
              cursor: 'pointer',
              transition: 'all 0.15s',
              letterSpacing: '0.3px'
            }}
          >
            COMPETE
          </button>
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        {activeView === 'quick-actions' && (
          <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
            <QuickActionsView
              selectedSymbol={selectedSymbol}
              selectedModel={selectedModel}
              setSelectedModel={setSelectedModel}
              analysisType={analysisType}
              setAnalysisType={setAnalysisType}
              strategy={strategy}
              setStrategy={setStrategy}
              isLoading={isLoading}
              result={result}
              error={error}
              hasConfiguredKeys={hasConfiguredKeys}
              availableModels={availableModels}
              onAnalyzeMarket={handleAnalyzeMarket}
              onGenerateSignal={handleGenerateSignal}
              onAssessRisk={handleAssessRisk}
              onStartCompetition={handleStartCompetition}
              onExecuteTrade={handleExecuteTrade}
            />
          </div>
        )}

        {activeView === 'create-agent' && (
          <div style={{ flex: 1, overflow: 'auto' }}>
            <CreateAgentView
              selectedSymbol={selectedSymbol}
              onAgentCreated={(agent) => {
                setAgents([...agents, agent]);
                setActiveView('manage-agents');
              }}
            />
          </div>
        )}

        {activeView === 'manage-agents' && (
          <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
            <ManageAgentsView agents={agents} />
          </div>
        )}

        {activeView === 'competition' && (
          <div style={{ flex: 1, overflow: 'hidden' }}>
            <CompetitionPanel selectedSymbol={selectedSymbol} />
          </div>
        )}
      </div>
    </div>
  );
}

// ============================================================================
// Quick Actions View
// ============================================================================

interface QuickActionsViewProps {
  selectedSymbol: string;
  selectedModel: string;
  setSelectedModel: (model: string) => void;
  analysisType: 'quick' | 'comprehensive' | 'deep';
  setAnalysisType: (type: 'quick' | 'comprehensive' | 'deep') => void;
  strategy: 'momentum' | 'breakout' | 'reversal' | 'trend_following';
  setStrategy: (strategy: 'momentum' | 'breakout' | 'reversal' | 'trend_following') => void;
  isLoading: boolean;
  result: any;
  error: string | null;
  hasConfiguredKeys: boolean;
  availableModels: Array<{ value: string; label: string }>;
  onAnalyzeMarket: () => void;
  onGenerateSignal: () => void;
  onAssessRisk: () => void;
  onStartCompetition: () => void;
  onExecuteTrade: () => void;
}

function QuickActionsView({
  selectedSymbol,
  selectedModel,
  setSelectedModel,
  analysisType,
  setAnalysisType,
  strategy,
  setStrategy,
  isLoading,
  result,
  error,
  hasConfiguredKeys,
  availableModels,
  onAnalyzeMarket,
  onGenerateSignal,
  onAssessRisk,
  onStartCompetition,
  onExecuteTrade
}: QuickActionsViewProps) {
  const models = availableModels;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      {/* Model Selection */}
      <div>
        <label style={{
          color: FINCEPT.GRAY,
          fontSize: '9px',
          fontWeight: '600',
          display: 'block',
          marginBottom: '4px',
          letterSpacing: '0.5px'
        }}>
          MODEL
        </label>
        <select
          value={selectedModel}
          onChange={(e) => setSelectedModel(e.target.value)}
          disabled={!hasConfiguredKeys}
          style={{
            width: '100%',
            background: FINCEPT.DARK_BG,
            border: `1px solid ${hasConfiguredKeys ? FINCEPT.BORDER : FINCEPT.RED}`,
            color: hasConfiguredKeys ? FINCEPT.WHITE : FINCEPT.GRAY,
            padding: '5px 8px',
            borderRadius: '2px',
            fontSize: '10px',
            cursor: hasConfiguredKeys ? 'pointer' : 'not-allowed',
            fontFamily: '"IBM Plex Mono", monospace',
            opacity: hasConfiguredKeys ? 1 : 0.6
          }}
        >
          {models.map(m => (
            <option key={m.value} value={m.value}>{m.label}</option>
          ))}
        </select>
        {!hasConfiguredKeys && (
          <div style={{ fontSize: '8px', color: FINCEPT.RED, marginTop: '3px' }}>
            Configure API keys in Settings → LLM Config
          </div>
        )}
      </div>

      {/* Quick Action Buttons */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
        {/* Analyze Market */}
        <div>
          <button
            onClick={onAnalyzeMarket}
            disabled={isLoading || !hasConfiguredKeys}
            style={{
              width: '100%',
              background: hasConfiguredKeys ? FINCEPT.BLUE : FINCEPT.GRAY,
              border: `1px solid ${hasConfiguredKeys ? FINCEPT.BLUE : FINCEPT.GRAY}`,
              color: FINCEPT.WHITE,
              padding: '6px',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: '600',
              cursor: (isLoading || !hasConfiguredKeys) ? 'not-allowed' : 'pointer',
              opacity: (isLoading || !hasConfiguredKeys) ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
              letterSpacing: '0.3px'
            }}
          >
            {isLoading ? <Loader2 size={12} className="animate-spin" /> : <Brain size={12} />}
            ANALYZE
          </button>
          <select
            value={analysisType}
            onChange={(e) => setAnalysisType(e.target.value as any)}
            style={{
              width: '100%',
              background: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              padding: '4px 6px',
              borderRadius: '2px',
              fontSize: '9px',
              marginTop: '3px',
              cursor: 'pointer'
            }}
          >
            <option value="quick">Quick</option>
            <option value="comprehensive">Comprehensive</option>
            <option value="deep">Deep</option>
          </select>
        </div>

        {/* Generate Signal */}
        <div>
          <button
            onClick={onGenerateSignal}
            disabled={isLoading || !hasConfiguredKeys}
            style={{
              width: '100%',
              background: hasConfiguredKeys ? FINCEPT.GREEN : FINCEPT.GRAY,
              border: `1px solid ${hasConfiguredKeys ? FINCEPT.GREEN : FINCEPT.GRAY}`,
              color: FINCEPT.DARK_BG,
              padding: '6px',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: '600',
              cursor: (isLoading || !hasConfiguredKeys) ? 'not-allowed' : 'pointer',
              opacity: (isLoading || !hasConfiguredKeys) ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
              letterSpacing: '0.3px'
            }}
          >
            {isLoading ? <Loader2 size={12} className="animate-spin" /> : <Target size={12} />}
            SIGNAL
          </button>
          <select
            value={strategy}
            onChange={(e) => setStrategy(e.target.value as any)}
            style={{
              width: '100%',
              background: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              padding: '4px 6px',
              borderRadius: '2px',
              fontSize: '9px',
              marginTop: '3px',
              cursor: 'pointer'
            }}
          >
            <option value="momentum">Momentum</option>
            <option value="breakout">Breakout</option>
            <option value="reversal">Reversal</option>
            <option value="trend_following">Trend</option>
          </select>
        </div>

        {/* Assess Risk */}
        <button
          onClick={onAssessRisk}
          disabled={isLoading || !hasConfiguredKeys}
          style={{
            width: '100%',
            background: hasConfiguredKeys ? FINCEPT.ORANGE : FINCEPT.GRAY,
            border: `1px solid ${hasConfiguredKeys ? FINCEPT.ORANGE : FINCEPT.GRAY}`,
            color: FINCEPT.DARK_BG,
            padding: '6px',
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: '600',
            cursor: (isLoading || !hasConfiguredKeys) ? 'not-allowed' : 'pointer',
            opacity: (isLoading || !hasConfiguredKeys) ? 0.5 : 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            letterSpacing: '0.3px'
          }}
        >
          {isLoading ? <Loader2 size={12} className="animate-spin" /> : <Shield size={12} />}
          RISK
        </button>

        {/* Start Competition */}
        <button
          onClick={onStartCompetition}
          disabled={isLoading || availableModels.length < 2}
          style={{
            width: '100%',
            background: (availableModels.length >= 2) ? FINCEPT.PURPLE : FINCEPT.GRAY,
            border: `1px solid ${(availableModels.length >= 2) ? FINCEPT.PURPLE : FINCEPT.GRAY}`,
            color: FINCEPT.WHITE,
            padding: '6px',
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: '600',
            cursor: (isLoading || availableModels.length < 2) ? 'not-allowed' : 'pointer',
            opacity: (isLoading || availableModels.length < 2) ? 0.5 : 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            letterSpacing: '0.3px'
          }}
        >
          {isLoading ? <Loader2 size={12} className="animate-spin" /> : <Sparkles size={12} />}
          COMPETE
        </button>
      </div>

      {!hasConfiguredKeys && (
        <div style={{ fontSize: '8px', color: FINCEPT.RED, marginTop: '4px', textAlign: 'center' }}>
          Configure API keys in Settings → LLM Config
        </div>
      )}

      {hasConfiguredKeys && availableModels.length < 2 && (
        <div style={{ fontSize: '8px', color: FINCEPT.YELLOW, marginTop: '4px', textAlign: 'center' }}>
          Add 2+ models for multi-model competition
        </div>
      )}

      {/* Error Display */}
      {error && (
        <div style={{
          background: `${FINCEPT.RED}10`,
          border: `1px solid ${FINCEPT.RED}`,
          borderRadius: '2px',
          padding: '6px 8px',
          display: 'flex',
          alignItems: 'flex-start',
          gap: '6px'
        }}>
          <AlertCircle size={12} color={FINCEPT.RED} style={{ flexShrink: 0, marginTop: '1px' }} />
          <div>
            <div style={{ color: FINCEPT.RED, fontSize: '9px', fontWeight: '600', letterSpacing: '0.3px' }}>ERROR</div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px', lineHeight: '1.4' }}>{error}</div>
          </div>
        </div>
      )}

      {/* Result Display */}
      {result && (
        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '8px',
          maxHeight: '300px',
          overflow: 'auto'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            marginBottom: '6px',
            paddingBottom: '6px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            <CheckCircle size={12} color={FINCEPT.GREEN} />
            <span style={{ color: FINCEPT.GREEN, fontSize: '9px', fontWeight: '600', letterSpacing: '0.5px' }}>
              {result.type === 'market_analysis' && 'MARKET ANALYSIS'}
              {result.type === 'trade_signal' && 'TRADE SIGNAL'}
              {result.type === 'risk_analysis' && 'RISK ASSESSMENT'}
              {result.type === 'competition' && 'MULTI-MODEL COMPETITION'}
              {result.type === 'trade_execution' && 'TRADE EXECUTED'}
            </span>

            {/* Execute Button for Trade Signals */}
            {result.type === 'trade_signal' && (
              <button
                onClick={onExecuteTrade}
                disabled={isLoading}
                style={{
                  marginLeft: 'auto',
                  background: FINCEPT.ORANGE,
                  border: `1px solid ${FINCEPT.ORANGE}`,
                  color: FINCEPT.DARK_BG,
                  padding: '3px 8px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '700',
                  cursor: isLoading ? 'not-allowed' : 'pointer',
                  opacity: isLoading ? 0.5 : 1,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  letterSpacing: '0.3px',
                  transition: 'all 0.2s'
                }}
              >
                {isLoading ? <Loader2 size={10} className="animate-spin" /> : <Zap size={10} />}
                EXECUTE
              </button>
            )}
          </div>

          {result.type === 'competition' ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {/* Consensus */}
              <div>
                <div style={{ color: FINCEPT.ORANGE, fontSize: '8px', fontWeight: '700', marginBottom: '4px' }}>
                  CONSENSUS ({result.data.consensus.total_models} MODELS)
                </div>
                <div style={{
                  background: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  padding: '6px',
                  fontSize: '9px',
                  color: FINCEPT.WHITE
                }}>
                  Action: <span style={{ color: FINCEPT.CYAN, fontWeight: '700' }}>
                    {result.data.consensus.action.toUpperCase()}
                  </span>
                  {' • '}
                  Confidence: <span style={{ color: FINCEPT.YELLOW, fontWeight: '700' }}>
                    {(result.data.consensus.confidence * 100).toFixed(0)}%
                  </span>
                  {' • '}
                  Agreement: <span style={{ color: FINCEPT.GREEN, fontWeight: '700' }}>
                    {result.data.consensus.agreement}/{result.data.consensus.total_models}
                  </span>
                </div>
              </div>

              {/* Model Results */}
              <div>
                <div style={{ color: FINCEPT.ORANGE, fontSize: '8px', fontWeight: '700', marginBottom: '4px' }}>
                  MODEL DECISIONS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                  {result.data.results.map((r: any, idx: number) => (
                    <div
                      key={idx}
                      style={{
                        background: FINCEPT.PANEL_BG,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        padding: '4px 6px',
                        fontSize: '9px'
                      }}
                    >
                      <span style={{ color: FINCEPT.CYAN, fontWeight: '700' }}>
                        {r.model.split(':')[1] || r.model}:
                      </span>
                      {' '}
                      <span style={{ color: FINCEPT.WHITE }}>
                        {r.decision?.action?.toUpperCase() || 'ANALYZING...'}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          ) : result.type === 'trade_execution' ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {/* Trade Success Message */}
              {result.data.success && (
                <div style={{
                  background: `${FINCEPT.GREEN}15`,
                  border: `1px solid ${FINCEPT.GREEN}`,
                  borderRadius: '2px',
                  padding: '6px',
                  fontSize: '9px',
                  color: FINCEPT.GREEN
                }}>
                  [OK] Trade executed successfully (Paper Trading)
                  {result.data.trade_id && <span style={{ marginLeft: '6px' }}>ID: {result.data.trade_id}</span>}
                </div>
              )}

              {/* Warnings */}
              {result.data.warnings && result.data.warnings.length > 0 && (
                <div style={{
                  background: `${FINCEPT.YELLOW}15`,
                  border: `1px solid ${FINCEPT.YELLOW}`,
                  borderRadius: '2px',
                  padding: '6px'
                }}>
                  {result.data.warnings.map((warn: string, idx: number) => (
                    <div key={idx} style={{ fontSize: '8px', color: FINCEPT.YELLOW, marginBottom: '2px' }}>
                      [WARN] {warn}
                    </div>
                  ))}
                </div>
              )}

              {/* Trade Details */}
              {result.data.trade_data && (
                <div style={{
                  background: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  padding: '6px',
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr',
                  gap: '4px',
                  fontSize: '9px'
                }}>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Symbol:</span>
                    <span style={{ color: FINCEPT.WHITE, fontWeight: '700', marginLeft: '4px' }}>
                      {result.data.trade_data.symbol}
                    </span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Side:</span>
                    <span style={{
                      color: result.data.trade_data.side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED,
                      fontWeight: '700',
                      marginLeft: '4px',
                      textTransform: 'uppercase'
                    }}>
                      {result.data.trade_data.side}
                    </span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Entry:</span>
                    <span style={{ color: FINCEPT.CYAN, fontWeight: '700', marginLeft: '4px' }}>
                      ${result.data.trade_data.entry_price?.toFixed(2)}
                    </span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Quantity:</span>
                    <span style={{ color: FINCEPT.WHITE, fontWeight: '700', marginLeft: '4px' }}>
                      {result.data.trade_data.quantity}
                    </span>
                  </div>
                  {result.data.trade_data.stop_loss && (
                    <div>
                      <span style={{ color: FINCEPT.GRAY }}>Stop Loss:</span>
                      <span style={{ color: FINCEPT.RED, fontWeight: '700', marginLeft: '4px' }}>
                        ${result.data.trade_data.stop_loss.toFixed(2)}
                      </span>
                    </div>
                  )}
                  {result.data.trade_data.take_profit && (
                    <div>
                      <span style={{ color: FINCEPT.GRAY }}>Take Profit:</span>
                      <span style={{ color: FINCEPT.GREEN, fontWeight: '700', marginLeft: '4px' }}>
                        ${result.data.trade_data.take_profit.toFixed(2)}
                      </span>
                    </div>
                  )}
                </div>
              )}

              {/* Execution Time */}
              {result.data.execution_time_ms && (
                <div style={{ fontSize: '8px', color: FINCEPT.GRAY, textAlign: 'right' }}>
                  Executed in {result.data.execution_time_ms}ms
                </div>
              )}
            </div>
          ) : (
            <div style={{
              color: FINCEPT.WHITE,
              fontSize: '10px',
              lineHeight: '1.5',
              whiteSpace: 'pre-wrap',
              fontFamily: '"IBM Plex Mono", monospace'
            }}>
              {result.data.analysis || result.data.signal || result.data.risk_analysis}
            </div>
          )}
        </div>
      )}
    </div>
  );
}

// ============================================================================
// Create Agent View
// ============================================================================

function CreateAgentView({ selectedSymbol, onAgentCreated }: {
  selectedSymbol: string;
  onAgentCreated: (agent: AgentInfo) => void;
}) {
  return <AgentConfigurationUI onAgentCreated={onAgentCreated} />;
}

// ============================================================================
// Manage Agents View
// ============================================================================

interface AgentStatus {
  id: string;
  name: string;
  type: string;
  status: 'running' | 'paused' | 'stopped';
  autoTrading: boolean;
  metrics: {
    totalTrades: number;
    winningTrades: number;
    losingTrades: number;
    totalPnL: number;
    dailyPnL: number;
    currentDrawdown: number;
    confidence: number;
  };
  lastActivity?: string;
}

function ManageAgentsView({ agents }: { agents: AgentInfo[] }) {
  const [agentStatuses, setAgentStatuses] = useState<AgentStatus[]>([]);
  const [expandedAgent, setExpandedAgent] = useState<string | null>(null);

  // Mock data for demonstration
  useEffect(() => {
    if (agents.length > 0) {
      const mockStatuses: AgentStatus[] = agents.map((agent, idx) => ({
        id: agent.agent_id,
        name: agent.config.name,
        type: agent.config.role,
        status: idx === 0 ? 'running' : 'stopped',
        autoTrading: idx === 0,
        metrics: {
          totalTrades: Math.floor(Math.random() * 50),
          winningTrades: Math.floor(Math.random() * 30),
          losingTrades: Math.floor(Math.random() * 20),
          totalPnL: (Math.random() - 0.5) * 5000,
          dailyPnL: (Math.random() - 0.5) * 500,
          currentDrawdown: Math.random() * 0.15,
          confidence: 0.6 + Math.random() * 0.3
        },
        lastActivity: new Date().toISOString()
      }));
      setAgentStatuses(mockStatuses);
    }
  }, [agents]);

  const handleStart = (agentId: string) => {
    setAgentStatuses(prev => prev.map(a =>
      a.id === agentId ? { ...a, status: 'running' as const } : a
    ));
  };

  const handlePause = (agentId: string) => {
    setAgentStatuses(prev => prev.map(a =>
      a.id === agentId ? { ...a, status: 'paused' as const } : a
    ));
  };

  const handleStop = (agentId: string) => {
    setAgentStatuses(prev => prev.map(a =>
      a.id === agentId ? { ...a, status: 'stopped' as const, autoTrading: false } : a
    ));
  };

  const handleEmergencyStop = () => {
    setAgentStatuses(prev => prev.map(a => ({ ...a, status: 'stopped' as const, autoTrading: false })));
  };

  if (agents.length === 0) {
    return (
      <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '30px 12px' }}>
        <Bot size={32} color={FINCEPT.GRAY} style={{ margin: '0 auto 10px' }} />
        <div style={{ fontSize: '11px', fontWeight: '600', marginBottom: '4px', letterSpacing: '0.5px' }}>
          NO AGENTS
        </div>
        <div style={{ fontSize: '9px', lineHeight: '1.4' }}>
          Create your first AI agent
        </div>
      </div>
    );
  }

  const runningAgents = agentStatuses.filter(a => a.status === 'running').length;
  const totalPnL = agentStatuses.reduce((sum, a) => sum + a.metrics.totalPnL, 0);
  const totalTrades = agentStatuses.reduce((sum, a) => sum + a.metrics.totalTrades, 0);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      {/* Overview Stats */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px' }}>
        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '6px 8px'
        }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '8px', marginBottom: '3px', letterSpacing: '0.5px' }}>
            ACTIVE
          </div>
          <div style={{ color: FINCEPT.WHITE, fontSize: '14px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
            {runningAgents}/{agents.length}
          </div>
        </div>

        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '6px 8px'
        }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '8px', marginBottom: '3px', letterSpacing: '0.5px' }}>
            P&L
          </div>
          <div style={{
            color: totalPnL >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: '14px',
            fontWeight: '700',
            fontFamily: '"IBM Plex Mono", monospace'
          }}>
            ${totalPnL.toFixed(0)}
          </div>
        </div>

        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '6px 8px'
        }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '8px', marginBottom: '3px', letterSpacing: '0.5px' }}>
            TRADES
          </div>
          <div style={{ color: FINCEPT.WHITE, fontSize: '14px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
            {totalTrades}
          </div>
        </div>

        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '6px 8px'
        }}>
          <button
            onClick={handleEmergencyStop}
            disabled={runningAgents === 0}
            style={{
              width: '100%',
              height: '100%',
              background: runningAgents > 0 ? FINCEPT.RED : 'transparent',
              border: `1px solid ${runningAgents > 0 ? FINCEPT.RED : FINCEPT.BORDER}`,
              color: runningAgents > 0 ? FINCEPT.WHITE : FINCEPT.GRAY,
              borderRadius: '2px',
              fontSize: '8px',
              fontWeight: '600',
              cursor: runningAgents > 0 ? 'pointer' : 'not-allowed',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '3px',
              letterSpacing: '0.3px'
            }}
          >
            <Square size={10} />
            STOP
          </button>
        </div>
      </div>

      {/* Agent List */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
        {agentStatuses.map(agent => (
          <AgentCard
            key={agent.id}
            agent={agent}
            isExpanded={expandedAgent === agent.id}
            onToggleExpand={() => setExpandedAgent(expandedAgent === agent.id ? null : agent.id)}
            onStart={() => handleStart(agent.id)}
            onPause={() => handlePause(agent.id)}
            onStop={() => handleStop(agent.id)}
          />
        ))}
      </div>
    </div>
  );
}

// ============================================================================
// Agent Card Component
// ============================================================================

interface AgentCardProps {
  agent: AgentStatus;
  isExpanded: boolean;
  onToggleExpand: () => void;
  onStart: () => void;
  onPause: () => void;
  onStop: () => void;
}

function AgentCard({ agent, isExpanded, onToggleExpand, onStart, onPause, onStop }: AgentCardProps) {
  const winRate = agent.metrics.totalTrades > 0
    ? (agent.metrics.winningTrades / agent.metrics.totalTrades) * 100
    : 0;

  const statusColor = {
    running: FINCEPT.GREEN,
    paused: FINCEPT.YELLOW,
    stopped: FINCEPT.GRAY
  }[agent.status];

  return (
    <div style={{
      background: FINCEPT.DARK_BG,
      border: `1px solid ${agent.status === 'running' ? FINCEPT.GREEN : FINCEPT.BORDER}`,
      borderRadius: '2px',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div
        onClick={onToggleExpand}
        style={{
          padding: '6px 8px',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          background: agent.status === 'running' ? `${FINCEPT.GREEN}08` : 'transparent'
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flex: 1 }}>
          <div style={{
            width: '6px',
            height: '6px',
            borderRadius: '50%',
            background: statusColor,
            boxShadow: agent.status === 'running' ? `0 0 6px ${statusColor}` : 'none'
          }} />

          <div style={{ flex: 1 }}>
            <div style={{
              color: FINCEPT.WHITE,
              fontSize: '10px',
              fontWeight: '600',
              marginBottom: '1px'
            }}>
              {agent.name}
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>
              {agent.type.replace(/_/g, ' ').toUpperCase()}
              {agent.autoTrading && <span style={{ color: FINCEPT.ORANGE, marginLeft: '6px' }}>● AUTO</span>}
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '7px', letterSpacing: '0.5px' }}>P&L</div>
              <div style={{
                color: agent.metrics.totalPnL >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                fontSize: '10px',
                fontWeight: '700',
                fontFamily: '"IBM Plex Mono", monospace'
              }}>
                ${agent.metrics.totalPnL.toFixed(0)}
              </div>
            </div>

            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '7px', letterSpacing: '0.5px' }}>WIN</div>
              <div style={{ color: FINCEPT.WHITE, fontSize: '10px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
                {winRate.toFixed(0)}%
              </div>
            </div>

            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '7px', letterSpacing: '0.5px' }}>TRD</div>
              <div style={{ color: FINCEPT.WHITE, fontSize: '10px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
                {agent.metrics.totalTrades}
              </div>
            </div>
          </div>
        </div>

        {isExpanded ? <ChevronUp size={12} color={FINCEPT.GRAY} /> : <ChevronDown size={12} color={FINCEPT.GRAY} />}
      </div>

      {/* Expanded Details */}
      {isExpanded && (
        <div style={{
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          padding: '8px',
          background: FINCEPT.PANEL_BG
        }}>
          {/* Metrics Grid */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(3, 1fr)',
            gap: '6px',
            marginBottom: '8px'
          }}>
            <MetricBox label="DAILY" value={`$${agent.metrics.dailyPnL.toFixed(0)}`}
              color={agent.metrics.dailyPnL >= 0 ? FINCEPT.GREEN : FINCEPT.RED} />
            <MetricBox label="DD" value={`${(agent.metrics.currentDrawdown * 100).toFixed(1)}%`}
              color={FINCEPT.ORANGE} />
            <MetricBox label="CONF" value={`${(agent.metrics.confidence * 100).toFixed(0)}%`}
              color={FINCEPT.CYAN} />
            <MetricBox label="WIN" value={agent.metrics.winningTrades.toString()}
              color={FINCEPT.GREEN} />
            <MetricBox label="LOSS" value={agent.metrics.losingTrades.toString()}
              color={FINCEPT.RED} />
            <MetricBox label="STATE" value={agent.status.toUpperCase()}
              color={statusColor} />
          </div>

          {/* Controls */}
          <div style={{ display: 'flex', gap: '4px' }}>
            {agent.status !== 'running' && (
              <button
                onClick={onStart}
                style={{
                  flex: 1,
                  background: FINCEPT.GREEN,
                  border: 'none',
                  color: FINCEPT.DARK_BG,
                  padding: '6px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '600',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                  letterSpacing: '0.3px'
                }}
              >
                <Play size={10} />
                START
              </button>
            )}

            {agent.status === 'running' && (
              <button
                onClick={onPause}
                style={{
                  flex: 1,
                  background: FINCEPT.YELLOW,
                  border: 'none',
                  color: FINCEPT.DARK_BG,
                  padding: '6px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '600',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                  letterSpacing: '0.3px'
                }}
              >
                <Pause size={10} />
                PAUSE
              </button>
            )}

            {agent.status !== 'stopped' && (
              <button
                onClick={onStop}
                style={{
                  flex: 1,
                  background: FINCEPT.RED,
                  border: 'none',
                  color: FINCEPT.WHITE,
                  padding: '6px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '600',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                  letterSpacing: '0.3px'
                }}
              >
                <Square size={10} />
                STOP
              </button>
            )}
          </div>
        </div>
      )}
    </div>
  );
}

// ============================================================================
// Metric Box Component
// ============================================================================

function MetricBox({ label, value, color }: { label: string; value: string; color: string }) {
  return (
    <div style={{
      background: FINCEPT.DARK_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      padding: '5px 6px'
    }}>
      <div style={{ color: FINCEPT.GRAY, fontSize: '7px', marginBottom: '2px', letterSpacing: '0.5px' }}>
        {label}
      </div>
      <div style={{ color, fontSize: '11px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
        {value}
      </div>
    </div>
  );
}
