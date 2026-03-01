/**
 * AI Agents Panel
 *
 * Main panel for Agno AI Trading Agents in the Trading tab.
 * Provides interface for creating, configuring, and interacting with AI agents.
 */

import React, { useState, useEffect } from 'react';
import { Sparkles } from 'lucide-react';
import agnoTradingService, {
  type AgentInfo,
} from '../../../../services/trading/agnoTradingService';
import { AgentConfigurationUI } from './AgentConfigurationUI';
import { CompetitionPanel } from './CompetitionPanel';
import { FINCEPT, QuickActionsView } from './AIAgentsPanelQuickActions';
import { ManageAgentsView } from './AIAgentsPanelManage';

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
          const models = configured.map(c => ({
            value: `${c.provider}:${c.model}`,
            label: `${c.provider.charAt(0).toUpperCase() + c.provider.slice(1)} - ${c.model}`
          }));
          setAvailableModels(models);
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
        setResult({ type: 'market_analysis', data: response.data });
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
        setResult({ type: 'trade_signal', data: response.data });
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
        setResult({ type: 'risk_analysis', data: response.data });
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
      const runResponse = await agnoTradingService.runCompetition(
        teamId,
        selectedSymbol,
        'analyze'
      );

      if (runResponse.success && runResponse.data) {
        setResult({ type: 'competition', data: runResponse.data });
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
      const signalData = result.data;
      const portfolio = {
        total_value: portfolioData.total_value,
        positions: portfolioData.positions || [],
        daily_pnl: 0
      };

      const response = await agnoTradingService.executeTrade(
        signalData,
        portfolio,
        'quick_action_agent',
        selectedModel
      );

      if (response.success && response.data) {
        setResult({ type: 'trade_execution', data: response.data });
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
          {(['quick-actions', 'create-agent', 'manage-agents', 'competition'] as const).map((view) => {
            const labels: Record<string, string> = {
              'quick-actions': 'QUICK',
              'create-agent': 'CREATE',
              'manage-agents': 'MANAGE',
              'competition': 'COMPETE'
            };
            return (
              <button
                key={view}
                onClick={() => setActiveView(view)}
                style={{
                  background: activeView === view ? FINCEPT.ORANGE : 'transparent',
                  color: activeView === view ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: `1px solid ${activeView === view ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  padding: '4px 8px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '600',
                  cursor: 'pointer',
                  transition: 'all 0.15s',
                  letterSpacing: '0.3px'
                }}
              >
                {labels[view]}
              </button>
            );
          })}
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
            <AgentConfigurationUI
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
