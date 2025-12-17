/**
 * Competition Panel
 *
 * Alpha Arena style AI trading competition
 * - Create multi-model competitions
 * - Start/stop competitions
 * - Real-time decision streaming
 * - Live leaderboard updates
 */

import React, { useState, useEffect } from 'react';
import { Play, Square, Trophy, Brain, TrendingUp, AlertCircle, Plus, X } from 'lucide-react';
import { agnoTradingService, type CompetitionResult, type ModelPerformance } from '../../../../services/agnoTradingService';

const BLOOMBERG = {
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

interface CompetitionPanelProps {
  selectedSymbol: string;
}

export function CompetitionPanel({ selectedSymbol }: CompetitionPanelProps) {
  const [teamId, setTeamId] = useState<string | null>(null);
  const [isRunning, setIsRunning] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<CompetitionResult | null>(null);
  const [showSetup, setShowSetup] = useState(false);

  // Competition setup state
  const [competitionName, setCompetitionName] = useState('AI Trading Competition');
  const [selectedModels, setSelectedModels] = useState<string[]>([]);
  const [taskType, setTaskType] = useState<'analyze' | 'signal' | 'evaluate'>('analyze');
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [availableModels, setAvailableModels] = useState<Array<{ value: string; label: string; color: string }>>([]);

  // Color mapping for providers
  const getProviderColor = (provider: string) => {
    const colorMap: Record<string, string> = {
      'openai': BLOOMBERG.GREEN,
      'anthropic': BLOOMBERG.ORANGE,
      'google': BLOOMBERG.BLUE,
      'groq': BLOOMBERG.PURPLE,
      'deepseek': BLOOMBERG.CYAN,
      'xai': BLOOMBERG.YELLOW,
      'together': BLOOMBERG.PURPLE,
      'mistral': BLOOMBERG.BLUE,
      'cohere': BLOOMBERG.CYAN
    };
    return colorMap[provider.toLowerCase()] || BLOOMBERG.GRAY;
  };

  // Load configured models from database
  useEffect(() => {
    const loadConfiguredModels = async () => {
      try {
        console.log('[CompetitionPanel] Loading configured models...');
        const { sqliteService } = await import('../../../../services/sqliteService');
        const configs = await sqliteService.getLLMConfigs();
        console.log('[CompetitionPanel] Raw configs:', configs);

        // Filter to only models with API keys configured
        const configuredModels = configs
          .filter(c => c.api_key && c.api_key.trim().length > 0)
          .map(c => ({
            value: `${c.provider}:${c.model}`,
            label: `${c.provider.charAt(0).toUpperCase() + c.provider.slice(1)} - ${c.model}`,
            color: getProviderColor(c.provider)
          }));

        console.log('[CompetitionPanel] Configured models:', configuredModels);
        setAvailableModels(configuredModels);

        // Auto-select first 3 models if available
        if (configuredModels.length >= 3) {
          const selected = configuredModels.slice(0, 3).map(m => m.value);
          console.log('[CompetitionPanel] Auto-selecting first 3 models:', selected);
          setSelectedModels(selected);
        } else if (configuredModels.length >= 2) {
          const selected = configuredModels.slice(0, 2).map(m => m.value);
          console.log('[CompetitionPanel] Auto-selecting first 2 models:', selected);
          setSelectedModels(selected);
        } else {
          console.warn('[CompetitionPanel] Not enough models configured for competition (need at least 2)');
        }
      } catch (err) {
        console.error('[CompetitionPanel] Failed to load configured models:', err);
        setError('Failed to load configured models. Please configure API keys in Settings → LLM Config');
      }
    };

    loadConfiguredModels();
  }, []);

  // Create competition
  const handleCreateCompetition = async () => {
    if (selectedModels.length < 2) {
      const errorMsg = 'Select at least 2 models to compete';
      console.error('[CompetitionPanel]', errorMsg);
      setError(errorMsg);
      return;
    }

    try {
      setIsLoading(true);
      setError(null);

      console.log('[CompetitionPanel] Creating competition:', {
        name: competitionName,
        models: selectedModels,
        type: 'trading'
      });

      const response = await agnoTradingService.createCompetition(
        competitionName,
        selectedModels,
        'trading'
      );

      console.log('[CompetitionPanel] Create competition response:', response);

      if (response.success && response.data) {
        console.log('[CompetitionPanel] Competition created successfully. Team ID:', response.data.team_id);
        setTeamId(response.data.team_id);
        setShowSetup(false);
      } else {
        const errorMsg = response.error || 'Failed to create competition';
        console.error('[CompetitionPanel] Competition creation failed:', errorMsg);
        setError(errorMsg);
      }
    } catch (err) {
      console.error('[CompetitionPanel] Exception during competition creation:', err);
      const errorMsg = err instanceof Error ? err.message : String(err);
      setError(errorMsg);
    } finally {
      setIsLoading(false);
    }
  };

  // Run competition
  const handleRunCompetition = async () => {
    if (!teamId) {
      console.error('[CompetitionPanel] No team ID available');
      return;
    }

    try {
      setIsLoading(true);
      setIsRunning(true);
      setError(null);

      console.log('[CompetitionPanel] Running competition:', {
        teamId,
        symbol: selectedSymbol,
        taskType
      });

      const response = await agnoTradingService.runCompetition(
        teamId,
        selectedSymbol,
        taskType
      );

      console.log('[CompetitionPanel] Run competition response:', response);

      if (response.success && response.data) {
        console.log('[CompetitionPanel] Competition completed successfully');
        setResult(response.data);
      } else {
        const errorMsg = response.error || 'Competition failed';
        console.error('[CompetitionPanel] Competition run failed:', errorMsg);
        console.error('[CompetitionPanel] Full response:', JSON.stringify(response, null, 2));
        setError(errorMsg);
      }
    } catch (err) {
      console.error('[CompetitionPanel] Exception during competition run:', err);
      console.error('[CompetitionPanel] Error details:', {
        message: err instanceof Error ? err.message : String(err),
        stack: err instanceof Error ? err.stack : undefined,
        raw: err
      });
      const errorMsg = err instanceof Error ? err.message : String(err);
      setError(errorMsg);
    } finally {
      setIsLoading(false);
      setIsRunning(false);
    }
  };

  // Stop competition
  const handleStopCompetition = () => {
    setIsRunning(false);
  };

  // Reset competition
  const handleResetCompetition = () => {
    setTeamId(null);
    setResult(null);
    setError(null);
    setShowSetup(true);
  };

  // Auto-refresh leaderboard
  useEffect(() => {
    if (!autoRefresh || !teamId) return;

    const interval = setInterval(async () => {
      try {
        const response = await agnoTradingService.getLeaderboard();
        if (response.success && response.data && result) {
          setResult({
            ...result,
            leaderboard: response.data.leaderboard
          });
        }
      } catch (err) {
        console.error('Failed to refresh leaderboard:', err);
      }
    }, 5000);

    return () => clearInterval(interval);
  }, [autoRefresh, teamId, result]);

  // Toggle model selection
  const toggleModel = (model: string) => {
    setSelectedModels(prev => {
      const newSelection = prev.includes(model)
        ? prev.filter(m => m !== model)
        : [...prev, model];

      console.log('[CompetitionPanel] Model selection updated:', {
        toggled: model,
        wasPreviouslySelected: prev.includes(model),
        previousSelection: prev,
        newSelection: newSelection
      });

      return newSelection;
    });
  };

  return (
    <div style={{
      height: '100%',
      background: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderRadius: '2px',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div style={{
        background: BLOOMBERG.HEADER_BG,
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
        padding: '8px 10px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Trophy size={14} color={BLOOMBERG.YELLOW} />
          <span style={{
            color: BLOOMBERG.WHITE,
            fontSize: '11px',
            fontWeight: '600',
            letterSpacing: '0.5px'
          }}>
            COMPETITION
          </span>
          {isRunning && (
            <div style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              background: BLOOMBERG.GREEN,
              animation: 'pulse 2s infinite'
            }} />
          )}
        </div>

        {/* Controls */}
        <div style={{ display: 'flex', gap: '4px' }}>
          {!teamId && (
            <button
              onClick={() => setShowSetup(!showSetup)}
              style={{
                background: BLOOMBERG.ORANGE,
                border: 'none',
                color: BLOOMBERG.DARK_BG,
                padding: '4px 8px',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: '700',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Plus size={10} />
              SETUP
            </button>
          )}

          {teamId && !isRunning && (
            <button
              onClick={handleRunCompetition}
              disabled={isLoading}
              style={{
                background: BLOOMBERG.GREEN,
                border: 'none',
                color: BLOOMBERG.DARK_BG,
                padding: '4px 8px',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: '700',
                cursor: isLoading ? 'not-allowed' : 'pointer',
                opacity: isLoading ? 0.5 : 1,
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Play size={10} />
              RUN
            </button>
          )}

          {isRunning && (
            <button
              onClick={handleStopCompetition}
              style={{
                background: BLOOMBERG.RED,
                border: 'none',
                color: BLOOMBERG.WHITE,
                padding: '4px 8px',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: '700',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Square size={10} />
              STOP
            </button>
          )}

          {teamId && (
            <button
              onClick={handleResetCompetition}
              style={{
                background: 'transparent',
                border: `1px solid ${BLOOMBERG.BORDER}`,
                color: BLOOMBERG.GRAY,
                padding: '4px 8px',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: '700',
                cursor: 'pointer'
              }}
            >
              RESET
            </button>
          )}
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '10px' }}>
        {/* Setup View */}
        {showSetup && !teamId && (
          <SetupView
            competitionName={competitionName}
            setCompetitionName={setCompetitionName}
            selectedModels={selectedModels}
            toggleModel={toggleModel}
            availableModels={availableModels}
            taskType={taskType}
            setTaskType={setTaskType}
            onCreateCompetition={handleCreateCompetition}
            isLoading={isLoading}
          />
        )}

        {/* Results View */}
        {teamId && !showSetup && result && (
          <ResultsView result={result} selectedSymbol={selectedSymbol} />
        )}

        {/* Empty State */}
        {!showSetup && !teamId && !result && (
          <div style={{
            textAlign: 'center',
            color: BLOOMBERG.GRAY,
            fontSize: '10px',
            marginTop: '40px'
          }}>
            <Trophy size={40} color={BLOOMBERG.GRAY} style={{ marginBottom: '10px' }} />
            <div>No active competition</div>
            <div style={{ marginTop: '4px', fontSize: '9px' }}>Click SETUP to create one</div>
          </div>
        )}

        {/* Error Display */}
        {error && (
          <div style={{
            background: `${BLOOMBERG.RED}15`,
            border: `1px solid ${BLOOMBERG.RED}`,
            borderRadius: '2px',
            padding: '8px',
            marginTop: '10px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}>
            <AlertCircle size={14} color={BLOOMBERG.RED} />
            <span style={{ color: BLOOMBERG.RED, fontSize: '10px' }}>{error}</span>
          </div>
        )}
      </div>
    </div>
  );
}

// Setup View Component
function SetupView({
  competitionName,
  setCompetitionName,
  selectedModels,
  toggleModel,
  availableModels,
  taskType,
  setTaskType,
  onCreateCompetition,
  isLoading
}: any) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Competition Name */}
      <div>
        <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', display: 'block', marginBottom: '4px' }}>
          COMPETITION NAME
        </label>
        <input
          type="text"
          value={competitionName}
          onChange={(e) => setCompetitionName(e.target.value)}
          style={{
            width: '100%',
            background: BLOOMBERG.DARK_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            color: BLOOMBERG.WHITE,
            padding: '6px 8px',
            borderRadius: '2px',
            fontSize: '10px',
            fontFamily: '"IBM Plex Mono", monospace'
          }}
        />
      </div>

      {/* Model Selection */}
      <div>
        <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', display: 'block', marginBottom: '6px' }}>
          SELECT MODELS (min 2) - {availableModels.length} Available
        </label>

        {availableModels.length === 0 ? (
          <div style={{
            background: `${BLOOMBERG.YELLOW}15`,
            border: `1px solid ${BLOOMBERG.YELLOW}`,
            borderRadius: '2px',
            padding: '8px',
            fontSize: '9px',
            color: BLOOMBERG.YELLOW,
            textAlign: 'center'
          }}>
            No models configured. Go to Settings → LLM Config to add API keys.
          </div>
        ) : availableModels.length < 2 ? (
          <div style={{
            background: `${BLOOMBERG.ORANGE}15`,
            border: `1px solid ${BLOOMBERG.ORANGE}`,
            borderRadius: '2px',
            padding: '8px',
            fontSize: '9px',
            color: BLOOMBERG.ORANGE,
            textAlign: 'center'
          }}>
            Need at least 2 models for competition. Configure more in Settings → LLM Config.
          </div>
        ) : (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
            {availableModels.map((model: any) => {
              const isSelected = selectedModels.includes(model.value);
              return (
                <button
                  key={model.value}
                  onClick={() => {
                    console.log('[CompetitionPanel] Toggle model:', model.value, 'Currently selected:', isSelected);
                    toggleModel(model.value);
                  }}
                  style={{
                    background: isSelected ? `${model.color}30` : BLOOMBERG.DARK_BG,
                    border: `2px solid ${isSelected ? model.color : BLOOMBERG.BORDER}`,
                    color: isSelected ? model.color : BLOOMBERG.GRAY,
                    padding: '8px 10px',
                    borderRadius: '3px',
                    fontSize: '9px',
                    fontWeight: isSelected ? '700' : '500',
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                    textAlign: 'left',
                    position: 'relative',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.background = BLOOMBERG.HOVER;
                      e.currentTarget.style.borderColor = model.color + '50';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.background = BLOOMBERG.DARK_BG;
                      e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                    }
                  }}
                >
                  {/* Selection Indicator */}
                  <div style={{
                    width: '10px',
                    height: '10px',
                    borderRadius: '50%',
                    border: `2px solid ${isSelected ? model.color : BLOOMBERG.BORDER}`,
                    background: isSelected ? model.color : 'transparent',
                    flexShrink: 0,
                    transition: 'all 0.2s'
                  }} />
                  <span>{model.label}</span>
                </button>
              );
            })}
          </div>
        )}
      </div>

      {/* Task Type */}
      <div>
        <label style={{ color: BLOOMBERG.GRAY, fontSize: '9px', display: 'block', marginBottom: '4px' }}>
          TASK TYPE
        </label>
        <div style={{ display: 'flex', gap: '4px' }}>
          {['analyze', 'signal', 'evaluate'].map((task) => (
            <button
              key={task}
              onClick={() => setTaskType(task)}
              style={{
                flex: 1,
                background: taskType === task ? BLOOMBERG.ORANGE : 'transparent',
                border: `1px solid ${taskType === task ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                color: taskType === task ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                padding: '6px',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: '700',
                cursor: 'pointer',
                transition: 'all 0.2s',
                textTransform: 'uppercase'
              }}
            >
              {task}
            </button>
          ))}
        </div>
      </div>

      {/* Create Button */}
      <button
        onClick={onCreateCompetition}
        disabled={isLoading || selectedModels.length < 2}
        style={{
          background: BLOOMBERG.ORANGE,
          border: 'none',
          color: BLOOMBERG.DARK_BG,
          padding: '10px',
          borderRadius: '2px',
          fontSize: '10px',
          fontWeight: '700',
          cursor: (isLoading || selectedModels.length < 2) ? 'not-allowed' : 'pointer',
          opacity: (isLoading || selectedModels.length < 2) ? 0.5 : 1,
          marginTop: '8px'
        }}
      >
        {isLoading ? 'CREATING...' : 'CREATE COMPETITION'}
      </button>
    </div>
  );
}

// Results View Component
function ResultsView({ result, selectedSymbol }: { result: CompetitionResult; selectedSymbol: string }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Consensus */}
      {result.consensus && (
        <div style={{
          background: BLOOMBERG.DARK_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '2px',
          padding: '10px'
        }}>
          <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '6px' }}>
            CONSENSUS
          </div>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: '8px'
          }}>
            <span style={{
              color: BLOOMBERG.WHITE,
              fontSize: '14px',
              fontWeight: '700',
              textTransform: 'uppercase'
            }}>
              {result.consensus.action}
            </span>
            <span style={{
              color: BLOOMBERG.ORANGE,
              fontSize: '12px',
              fontWeight: '700'
            }}>
              {(result.consensus.confidence * 100).toFixed(0)}% CONFIDENCE
            </span>
          </div>
          <div style={{
            display: 'flex',
            gap: '12px',
            fontSize: '9px',
            color: BLOOMBERG.GRAY
          }}>
            <div>
              AGREEMENT: <span style={{ color: BLOOMBERG.WHITE, fontWeight: '600' }}>
                {result.consensus.agreement}/{result.consensus.total_models}
              </span>
            </div>
          </div>
        </div>
      )}

      {/* Model Results */}
      <div style={{
        background: BLOOMBERG.DARK_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        borderRadius: '2px',
        padding: '10px'
      }}>
        <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '8px' }}>
          MODEL RESULTS
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
          {result.results.map((modelResult, idx) => (
            <div
              key={idx}
              style={{
                background: BLOOMBERG.PANEL_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                borderRadius: '2px',
                padding: '8px'
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                marginBottom: '4px'
              }}>
                <span style={{ color: BLOOMBERG.ORANGE, fontSize: '10px', fontWeight: '700' }}>
                  {modelResult.model.split(':')[1] || modelResult.model}
                </span>
                <span style={{ color: BLOOMBERG.GRAY, fontSize: '8px' }}>
                  {new Date(modelResult.timestamp).toLocaleTimeString()}
                </span>
              </div>
              <div style={{ color: BLOOMBERG.WHITE, fontSize: '9px', lineHeight: '1.4' }}>
                {modelResult.response.substring(0, 150)}...
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Leaderboard */}
      {result.leaderboard && result.leaderboard.length > 0 && (
        <div style={{
          background: BLOOMBERG.DARK_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '2px',
          padding: '10px'
        }}>
          <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '8px' }}>
            LEADERBOARD
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            {result.leaderboard.slice(0, 5).map((model, idx) => (
              <div
                key={model.model}
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  padding: '6px',
                  background: BLOOMBERG.PANEL_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  borderRadius: '2px'
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <span style={{
                    color: idx === 0 ? BLOOMBERG.YELLOW : BLOOMBERG.GRAY,
                    fontSize: '10px',
                    fontWeight: '700'
                  }}>
                    #{idx + 1}
                  </span>
                  <span style={{ color: BLOOMBERG.WHITE, fontSize: '10px' }}>
                    {model.model.split(':')[1]}
                  </span>
                </div>
                <span style={{
                  color: model.total_pnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                  fontSize: '10px',
                  fontWeight: '700'
                }}>
                  {model.total_pnl >= 0 ? '+' : ''}{model.total_pnl.toFixed(2)}
                </span>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
