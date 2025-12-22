/**
 * Alpha Arena Tab - Simplified Real-time UI
 * Direct integration with backend Python service
 */

import React, { useState, useEffect } from 'react';
import { Trophy, Play, Square, TrendingUp, TrendingDown, RefreshCw } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { TabFooter } from '@/components/common/TabFooter';

// Bloomberg color palette
const COLORS = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
};

interface LeaderboardEntry {
  model_name: string;
  portfolio_value: number;
  total_pnl: number;
  return_pct: number;
  trades_count: number;
  cash: number;
  positions_count: number;
  rank: number;
}

interface ModelDecision {
  competition_id: string;
  model_name: string;
  cycle_number: number;
  action: string;
  symbol: string;
  quantity: number;
  confidence: number;
  reasoning: string;
  trade_executed: string;
  timestamp: string;
}

const AlphaArenaTab: React.FC = () => {
  const [competitionId, setCompetitionId] = useState<string>('');
  const [isRunning, setIsRunning] = useState(false);
  const [leaderboard, setLeaderboard] = useState<LeaderboardEntry[]>([]);
  const [decisions, setDecisions] = useState<ModelDecision[]>([]);
  const [cycleCount, setCycleCount] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [showCreateForm, setShowCreateForm] = useState(false);

  // Form state
  const [competitionName, setCompetitionName] = useState('Alpha Arena Competition');
  const [symbol, setSymbol] = useState('BTC/USD');
  const [mode, setMode] = useState<'baseline' | 'monk' | 'situational' | 'max_leverage'>('baseline');
  const [selectedModels, setSelectedModels] = useState<string[]>(['gpt-4o-mini', 'claude-3-5-sonnet']);
  const [apiKeys, setApiKeys] = useState<Record<string, string>>({
    'gpt-4o-mini': '',
    'claude-3-5-sonnet': ''
  });

  // Refresh leaderboard and decisions
  const refreshData = async () => {
    if (!competitionId) return;

    try {
      // Get leaderboard
      const leaderboardResult = await invoke<{ success: boolean; leaderboard: LeaderboardEntry[] }>(
        'get_alpha_leaderboard',
        { competitionId }
      );

      if (leaderboardResult.success) {
        setLeaderboard(leaderboardResult.leaderboard);
      }

      // Get decisions
      const decisionsResult = await invoke<{ success: boolean; decisions: ModelDecision[] }>(
        'get_alpha_model_decisions',
        { competitionId, modelName: null }
      );

      if (decisionsResult.success) {
        setDecisions(decisionsResult.decisions);
      }
    } catch (err) {
      console.error('[AlphaArena] Refresh failed:', err);
    }
  };

  // Auto-refresh when competition exists (regardless of running state)
  useEffect(() => {
    if (!competitionId) return;

    // Initial load
    refreshData();

    // Refresh every 3 seconds for live updates
    const interval = setInterval(refreshData, 3000);
    return () => clearInterval(interval);
  }, [competitionId]);

  // Available models
  const availableModels = [
    { id: 'gpt-4o-mini', name: 'GPT-4O Mini', provider: 'openai' },
    { id: 'gpt-4o', name: 'GPT-4O', provider: 'openai' },
    { id: 'claude-3-5-sonnet', name: 'Claude 3.5 Sonnet', provider: 'anthropic' },
    { id: 'gemini-2.0-flash', name: 'Gemini 2.0 Flash', provider: 'google' },
    { id: 'deepseek-chat', name: 'DeepSeek Chat', provider: 'deepseek' }
  ];

  const competitionModes = [
    { value: 'baseline', label: 'Baseline', description: 'Standard trading with full market data' },
    { value: 'monk', label: 'Monk Mode', description: 'Conservative capital preservation' },
    { value: 'situational', label: 'Situational Awareness', description: 'Competition-aware agents' },
    { value: 'max_leverage', label: 'Max Leverage', description: 'Maximum capital efficiency' }
  ];

  // Create competition
  const handleCreate = async () => {
    setIsLoading(true);
    setError(null);

    try {
      // Validate
      if (selectedModels.length === 0) {
        setError('Please select at least one model');
        setIsLoading(false);
        return;
      }

      const modelsWithKeys = selectedModels.filter(modelId => apiKeys[modelId]?.trim());
      if (modelsWithKeys.length === 0) {
        setError('Please provide API keys for selected models');
        setIsLoading(false);
        return;
      }

      const models = modelsWithKeys.map(modelId => {
        const model = availableModels.find(m => m.id === modelId);
        return {
          name: model?.name || modelId,
          provider: model?.provider || 'openai',
          model_id: modelId,
          api_key: apiKeys[modelId]
        };
      });

      const config = {
        competition_id: `comp_${Date.now()}_${Math.random().toString(36).substring(7)}`,
        competition_name: competitionName,
        models,
        symbols: [symbol],
        initial_capital: 10000.0,
        exchange_id: 'kraken'
      };

      console.log('[AlphaArena] Creating competition with config:', config);

      const result = await invoke<{ success: boolean; competition_id?: string; error?: string }>(
        'create_alpha_competition',
        { configJson: JSON.stringify(config) }
      );

      console.log('[AlphaArena] Create result:', result);

      if (result.success && result.competition_id) {
        setCompetitionId(result.competition_id);
        setShowCreateForm(false);
        await refreshData();
      } else {
        setError(result.error || 'Failed to create competition');
      }
    } catch (err) {
      console.error('[AlphaArena] Create error:', err);
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const toggleModelSelection = (modelId: string) => {
    setSelectedModels(prev =>
      prev.includes(modelId)
        ? prev.filter(id => id !== modelId)
        : [...prev, modelId]
    );
    if (!apiKeys[modelId]) {
      setApiKeys(prev => ({ ...prev, [modelId]: '' }));
    }
  };

  // Run single cycle
  const handleRunCycle = async () => {
    if (!competitionId) return;

    setIsLoading(true);
    try {
      const result = await invoke<{ success: boolean; cycle_number?: number; error?: string }>(
        'run_alpha_cycle',
        { competitionId }
      );

      if (result.success) {
        setCycleCount(result.cycle_number || 0);
        await refreshData();
      } else {
        setError(result.error || 'Cycle failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsLoading(false);
    }
  };

  // Toggle continuous running
  const handleToggleRunning = () => {
    setIsRunning(!isRunning);
  };

  // Auto-run cycles when running
  useEffect(() => {
    if (!isRunning || !competitionId) {
      console.log('[AlphaArena] Auto-run disabled. Running:', isRunning, 'CompID:', competitionId);
      return;
    }

    console.log('[AlphaArena] Starting auto-run cycles');

    let isCancelled = false;

    const runCycle = async () => {
      if (isCancelled) return;

      console.log('[AlphaArena] Executing cycle...');
      setIsLoading(true);

      try {
        const result = await invoke<{ success: boolean; cycle_number?: number; error?: string }>(
          'run_alpha_cycle',
          { competitionId }
        );

        if (!isCancelled) {
          if (result.success) {
            console.log('[AlphaArena] Cycle completed:', result.cycle_number);
            setCycleCount(result.cycle_number || 0);
            await refreshData();
          } else {
            console.error('[AlphaArena] Cycle failed:', result.error);
            setError(result.error || 'Cycle failed');
          }
        }
      } catch (err) {
        if (!isCancelled) {
          console.error('[AlphaArena] Cycle error:', err);
          setError(err instanceof Error ? err.message : 'Unknown error');
        }
      } finally {
        if (!isCancelled) {
          setIsLoading(false);
        }
      }
    };

    // Run first cycle immediately
    runCycle();

    // Then run every 60 seconds
    const interval = setInterval(runCycle, 60000);

    return () => {
      console.log('[AlphaArena] Stopping auto-run cycles');
      isCancelled = true;
      clearInterval(interval);
    };
  }, [isRunning, competitionId]);

  return (
    <div style={{
      height: '100%',
      backgroundColor: COLORS.DARK_BG,
      color: COLORS.WHITE,
      fontFamily: '"IBM Plex Mono", monospace',
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Header */}
      <div style={{
        padding: '16px',
        backgroundColor: COLORS.HEADER_BG,
        borderBottom: `2px solid ${COLORS.ORANGE}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Trophy size={24} color={COLORS.ORANGE} />
          <div>
            <div style={{ fontSize: '18px', fontWeight: 700, color: COLORS.ORANGE }}>
              ALPHA ARENA
            </div>
            <div style={{ fontSize: '10px', color: COLORS.GRAY }}>
              Live AI Trading Competition
            </div>
          </div>
        </div>

        <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
          {!competitionId ? (
            <button
              onClick={() => setShowCreateForm(true)}
              style={{
                padding: '10px 20px',
                backgroundColor: COLORS.ORANGE,
                border: 'none',
                color: COLORS.DARK_BG,
                fontSize: '12px',
                fontWeight: 700,
                cursor: 'pointer'
              }}
            >
              NEW COMPETITION
            </button>
          ) : (
            <>
              <button
                onClick={handleRunCycle}
                disabled={isLoading || isRunning}
                style={{
                  padding: '10px 20px',
                  backgroundColor: COLORS.ORANGE,
                  border: 'none',
                  color: COLORS.DARK_BG,
                  fontSize: '12px',
                  fontWeight: 700,
                  cursor: (isLoading || isRunning) ? 'not-allowed' : 'pointer',
                  opacity: (isLoading || isRunning) ? 0.6 : 1,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px'
                }}
              >
                <RefreshCw size={14} />
                RUN CYCLE
              </button>

              <button
                onClick={() => {
                  console.log('[AlphaArena] Toggle button clicked. Current state:', isRunning);
                  setIsRunning(!isRunning);
                }}
                disabled={isLoading}
                style={{
                  padding: '10px 20px',
                  backgroundColor: isRunning ? COLORS.RED : COLORS.GREEN,
                  border: 'none',
                  color: COLORS.WHITE,
                  fontSize: '12px',
                  fontWeight: 700,
                  cursor: isLoading ? 'not-allowed' : 'pointer',
                  opacity: isLoading ? 0.6 : 1,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px'
                }}
              >
                {isRunning ? (
                  <>
                    <Square size={14} />
                    STOP AUTO {isLoading && '(RUNNING...)'}
                  </>
                ) : (
                  <>
                    <Play size={14} />
                    START AUTO
                  </>
                )}
              </button>
            </>
          )}
        </div>
      </div>

      {/* Status Bar */}
      {competitionId && (
        <div style={{
          padding: '12px 16px',
          backgroundColor: COLORS.PANEL_BG,
          borderBottom: `1px solid ${COLORS.BORDER}`,
          display: 'flex',
          gap: '24px',
          fontSize: '11px'
        }}>
          <div>
            <span style={{ color: COLORS.GRAY }}>COMPETITION ID: </span>
            <span style={{ color: COLORS.ORANGE, fontWeight: 600 }}>{competitionId}</span>
          </div>
          <div>
            <span style={{ color: COLORS.GRAY }}>CYCLE: </span>
            <span style={{ color: COLORS.WHITE, fontWeight: 600 }}>{cycleCount}</span>
          </div>
          <div>
            <span style={{ color: COLORS.GRAY }}>STATUS: </span>
            <span style={{ color: isRunning ? COLORS.GREEN : COLORS.GRAY, fontWeight: 600 }}>
              {isRunning ? 'RUNNING' : 'PAUSED'}
            </span>
          </div>
        </div>
      )}

      {/* Create Form Modal */}
      {showCreateForm && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `2px solid ${COLORS.ORANGE}`,
            padding: '24px',
            maxWidth: '600px',
            width: '90%',
            maxHeight: '80vh',
            overflow: 'auto'
          }}>
            <h2 style={{ fontSize: '16px', color: COLORS.ORANGE, marginBottom: '20px', fontWeight: 700 }}>
              CREATE NEW COMPETITION
            </h2>

            {/* Competition Name */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ display: 'block', fontSize: '10px', color: COLORS.GRAY, marginBottom: '6px', fontWeight: 600 }}>
                COMPETITION NAME
              </label>
              <input
                type="text"
                value={competitionName}
                onChange={(e) => setCompetitionName(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: COLORS.HEADER_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  color: COLORS.WHITE,
                  fontSize: '11px',
                  fontFamily: 'inherit'
                }}
              />
            </div>

            {/* Symbol */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ display: 'block', fontSize: '10px', color: COLORS.GRAY, marginBottom: '6px', fontWeight: 600 }}>
                TRADING SYMBOL
              </label>
              <input
                type="text"
                value={symbol}
                onChange={(e) => setSymbol(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: COLORS.HEADER_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  color: COLORS.WHITE,
                  fontSize: '11px',
                  fontFamily: 'inherit'
                }}
              />
            </div>

            {/* Mode */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ display: 'block', fontSize: '10px', color: COLORS.GRAY, marginBottom: '6px', fontWeight: 600 }}>
                MODE
              </label>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                {competitionModes.map(m => (
                  <button
                    key={m.value}
                    onClick={() => setMode(m.value as any)}
                    style={{
                      padding: '10px',
                      backgroundColor: mode === m.value ? COLORS.ORANGE : COLORS.HEADER_BG,
                      border: `1px solid ${mode === m.value ? COLORS.ORANGE : COLORS.BORDER}`,
                      color: mode === m.value ? COLORS.DARK_BG : COLORS.WHITE,
                      fontSize: '10px',
                      fontWeight: 600,
                      cursor: 'pointer',
                      textAlign: 'left'
                    }}
                  >
                    <div style={{ fontWeight: 700 }}>{m.label}</div>
                    <div style={{ fontSize: '8px', opacity: 0.8 }}>{m.description}</div>
                  </button>
                ))}
              </div>
            </div>

            {/* Models */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ display: 'block', fontSize: '10px', color: COLORS.GRAY, marginBottom: '6px', fontWeight: 600 }}>
                SELECT MODELS (AT LEAST 2)
              </label>
              {availableModels.map(model => (
                <div key={model.id} style={{ marginBottom: '12px' }}>
                  <label style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    padding: '8px',
                    backgroundColor: COLORS.HEADER_BG,
                    border: `1px solid ${selectedModels.includes(model.id) ? COLORS.ORANGE : COLORS.BORDER}`,
                    cursor: 'pointer'
                  }}>
                    <input
                      type="checkbox"
                      checked={selectedModels.includes(model.id)}
                      onChange={() => toggleModelSelection(model.id)}
                      style={{ accentColor: COLORS.ORANGE }}
                    />
                    <div style={{ flex: 1 }}>
                      <div style={{ fontSize: '11px', fontWeight: 600 }}>{model.name}</div>
                      <div style={{ fontSize: '9px', color: COLORS.GRAY }}>{model.provider}</div>
                    </div>
                  </label>
                  {selectedModels.includes(model.id) && (
                    <input
                      type="password"
                      placeholder={`${model.provider} API Key`}
                      value={apiKeys[model.id] || ''}
                      onChange={(e) => setApiKeys(prev => ({ ...prev, [model.id]: e.target.value }))}
                      style={{
                        width: '100%',
                        padding: '6px',
                        marginTop: '6px',
                        backgroundColor: COLORS.DARK_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        color: COLORS.WHITE,
                        fontSize: '10px',
                        fontFamily: 'inherit'
                      }}
                    />
                  )}
                </div>
              ))}
            </div>

            {/* Error */}
            {error && (
              <div style={{
                marginBottom: '16px',
                padding: '10px',
                backgroundColor: `${COLORS.RED}20`,
                border: `1px solid ${COLORS.RED}`,
                color: COLORS.RED,
                fontSize: '10px'
              }}>
                {error}
              </div>
            )}

            {/* Actions */}
            <div style={{ display: 'flex', gap: '12px' }}>
              <button
                onClick={handleCreate}
                disabled={isLoading}
                style={{
                  flex: 1,
                  padding: '10px',
                  backgroundColor: isLoading ? COLORS.GRAY : COLORS.ORANGE,
                  border: 'none',
                  color: isLoading ? COLORS.DARK_BG : COLORS.DARK_BG,
                  fontSize: '11px',
                  fontWeight: 700,
                  cursor: isLoading ? 'not-allowed' : 'pointer'
                }}
              >
                {isLoading ? 'CREATING...' : 'CREATE'}
              </button>
              <button
                onClick={() => { setShowCreateForm(false); setError(null); }}
                disabled={isLoading}
                style={{
                  padding: '10px 20px',
                  backgroundColor: COLORS.HEADER_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  color: COLORS.WHITE,
                  fontSize: '11px',
                  fontWeight: 700,
                  cursor: isLoading ? 'not-allowed' : 'pointer'
                }}
              >
                CANCEL
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Leaderboard */}
        <div style={{
          flex: 1,
          backgroundColor: COLORS.PANEL_BG,
          borderRight: `1px solid ${COLORS.BORDER}`,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '12px 16px',
            backgroundColor: COLORS.HEADER_BG,
            borderBottom: `1px solid ${COLORS.BORDER}`,
            fontSize: '12px',
            fontWeight: 700,
            color: COLORS.ORANGE
          }}>
            LEADERBOARD
          </div>

          <div style={{ flex: 1, overflow: 'auto' }}>
            {leaderboard.length === 0 ? (
              <div style={{
                padding: '40px',
                textAlign: 'center',
                color: COLORS.GRAY,
                fontSize: '11px'
              }}>
                No data yet. Create competition and run cycles.
              </div>
            ) : (
              <table style={{ width: '100%', fontSize: '11px', borderCollapse: 'collapse' }}>
                <thead>
                  <tr style={{ backgroundColor: COLORS.HEADER_BG, fontSize: '9px', color: COLORS.GRAY }}>
                    <th style={{ padding: '10px', textAlign: 'left' }}>RANK</th>
                    <th style={{ padding: '10px', textAlign: 'left' }}>MODEL</th>
                    <th style={{ padding: '10px', textAlign: 'right' }}>VALUE</th>
                    <th style={{ padding: '10px', textAlign: 'right' }}>P&L</th>
                    <th style={{ padding: '10px', textAlign: 'right' }}>RETURN</th>
                    <th style={{ padding: '10px', textAlign: 'right' }}>TRADES</th>
                  </tr>
                </thead>
                <tbody>
                  {leaderboard.map((entry) => (
                    <tr key={entry.rank} style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                      <td style={{ padding: '10px', fontWeight: 700, color: COLORS.ORANGE }}>
                        #{entry.rank}
                      </td>
                      <td style={{ padding: '10px', fontWeight: 600 }}>{entry.model_name}</td>
                      <td style={{ padding: '10px', textAlign: 'right', fontWeight: 600 }}>
                        ${entry.portfolio_value.toFixed(2)}
                      </td>
                      <td style={{
                        padding: '10px',
                        textAlign: 'right',
                        fontWeight: 700,
                        color: entry.total_pnl >= 0 ? COLORS.GREEN : COLORS.RED
                      }}>
                        {entry.total_pnl >= 0 ? '+' : ''}{entry.total_pnl.toFixed(2)}
                      </td>
                      <td style={{
                        padding: '10px',
                        textAlign: 'right',
                        fontWeight: 700,
                        color: entry.return_pct >= 0 ? COLORS.GREEN : COLORS.RED
                      }}>
                        {entry.return_pct >= 0 ? '+' : ''}{entry.return_pct.toFixed(2)}%
                      </td>
                      <td style={{ padding: '10px', textAlign: 'right' }}>
                        {entry.trades_count}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>
        </div>

        {/* Decisions Panel */}
        <div style={{
          width: '500px',
          backgroundColor: COLORS.PANEL_BG,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '12px 16px',
            backgroundColor: COLORS.HEADER_BG,
            borderBottom: `1px solid ${COLORS.BORDER}`,
            fontSize: '12px',
            fontWeight: 700,
            color: COLORS.ORANGE
          }}>
            RECENT DECISIONS ({decisions.length})
          </div>

          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            {decisions.length === 0 ? (
              <div style={{
                padding: '40px 20px',
                textAlign: 'center',
                color: COLORS.GRAY,
                fontSize: '11px'
              }}>
                No trading decisions yet
              </div>
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                {decisions.slice().reverse().map((decision, idx) => (
                  <div
                    key={idx}
                    style={{
                      backgroundColor: COLORS.HEADER_BG,
                      border: `1px solid ${COLORS.BORDER}`,
                      padding: '12px'
                    }}
                  >
                    <div style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      marginBottom: '8px',
                      alignItems: 'center'
                    }}>
                      <span style={{ fontSize: '11px', fontWeight: 700 }}>
                        {decision.model_name}
                      </span>
                      <span style={{
                        fontSize: '10px',
                        padding: '3px 8px',
                        backgroundColor: decision.action === 'BUY' ? `${COLORS.GREEN}30` : `${COLORS.RED}30`,
                        color: decision.action === 'BUY' ? COLORS.GREEN : COLORS.RED,
                        fontWeight: 700
                      }}>
                        {decision.action}
                      </span>
                    </div>

                    <div style={{ fontSize: '10px', color: COLORS.GRAY, marginBottom: '6px' }}>
                      <span style={{ color: COLORS.ORANGE }}>{decision.symbol}</span>
                      {' × '}
                      <span>{decision.quantity}</span>
                      {' | Cycle '}
                      <span>{decision.cycle_number}</span>
                    </div>

                    <div style={{ fontSize: '10px', marginBottom: '8px' }}>
                      <span style={{ color: COLORS.GRAY }}>Confidence: </span>
                      <span style={{ color: COLORS.WHITE, fontWeight: 600 }}>
                        {(decision.confidence * 100).toFixed(0)}%
                      </span>
                    </div>

                    <div style={{
                      fontSize: '9px',
                      color: COLORS.GRAY,
                      lineHeight: '1.5',
                      maxHeight: '60px',
                      overflow: 'hidden'
                    }}>
                      {decision.reasoning}
                    </div>

                    <div style={{
                      fontSize: '8px',
                      color: COLORS.GRAY,
                      marginTop: '8px',
                      paddingTop: '8px',
                      borderTop: `1px solid ${COLORS.BORDER}`
                    }}>
                      {new Date(parseInt(decision.timestamp)).toLocaleString()}
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      </div>

      <TabFooter
        tabName="ALPHA ARENA"
        leftInfo={[
          { label: `Competition: ${competitionId || 'None'}`, color: COLORS.GRAY },
          { label: `Models: ${leaderboard.length}`, color: COLORS.GRAY },
          { label: `Status: ${isRunning ? 'RUNNING' : 'STOPPED'}`, color: isRunning ? COLORS.GREEN : COLORS.RED },
        ]}
        statusInfo={`Cycle: ${currentCycle} | Decisions: ${recentDecisions.length} | ${status}`}
        backgroundColor={COLORS.PANEL_BG}
        borderColor={COLORS.BORDER}
      />
    </div>
  );
};

export default AlphaArenaTab;
