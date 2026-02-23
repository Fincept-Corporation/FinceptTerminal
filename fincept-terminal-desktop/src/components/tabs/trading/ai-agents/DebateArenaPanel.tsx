/**
 * Debate Arena Panel
 * Bull vs Bear vs Analyst - Multi-agent trading debate system
 */

import React, { useState, useEffect } from 'react';
import { MessageSquare, TrendingUp, TrendingDown, Brain, Loader2, AlertCircle, CheckCircle, Zap, Play } from 'lucide-react';
import agnoTradingService, { type DebateResult } from '../../../../services/trading/agnoTradingService';

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

interface DebateArenaPanelProps {
  selectedSymbol: string;
  currentPrice?: number;
  marketData?: any;
}

export function DebateArenaPanel({ selectedSymbol, currentPrice, marketData }: DebateArenaPanelProps) {
  const [bullModel, setBullModel] = useState('');
  const [bearModel, setBearModel] = useState('');
  const [analystModel, setAnalystModel] = useState('');
  const [isDebating, setIsDebating] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [debateResult, setDebateResult] = useState<DebateResult | null>(null);
  const [recentDebates, setRecentDebates] = useState<any[]>([]);
  const [availableModels, setAvailableModels] = useState<Array<{ value: string; label: string; color: string }>>([]);

  // Load available models on mount
  useEffect(() => {
    loadAvailableModels();
    loadRecentDebates();
  }, []);

  const loadAvailableModels = async () => {
    try {
      const { sqliteService } = await import('../../../../services/core/sqliteService');
      const configs = await sqliteService.getLLMConfigs();

      const configured = configs.filter(c => c.api_key && c.api_key.trim().length > 0);

      const models = configured.map(c => ({
        value: `${c.provider}:${c.model}`,
        label: `${c.provider.charAt(0).toUpperCase() + c.provider.slice(1)} - ${c.model}`,
        color: getModelColor(c.provider)
      }));

      setAvailableModels(models);

      // Auto-select first 3 if available
      if (models.length >= 3) {
        setBullModel(models[0].value);
        setBearModel(models[1].value);
        setAnalystModel(models[2].value);
      }
    } catch (err) {
      console.error('Failed to load models:', err);
    }
  };

  const loadRecentDebates = async () => {
    try {
      const response = await agnoTradingService.getRecentDebates(5);
      if (response.success && response.data) {
        setRecentDebates(response.data.debates || []);
      }
    } catch (err) {
      console.error('Failed to load debates:', err);
    }
  };

  const getModelColor = (provider: string) => {
    const colors: Record<string, string> = {
      openai: FINCEPT.GREEN,
      anthropic: FINCEPT.ORANGE,
      google: FINCEPT.BLUE,
      gemini: FINCEPT.BLUE,
      groq: FINCEPT.PURPLE,
      deepseek: FINCEPT.CYAN,
      xai: FINCEPT.YELLOW
    };
    return colors[provider.toLowerCase()] || FINCEPT.GRAY;
  };

  const handleStartDebate = async () => {
    if (!selectedSymbol) {
      setError('No symbol selected');
      return;
    }

    if (bullModel === bearModel || bearModel === analystModel || bullModel === analystModel) {
      setError('Please select different models for each role');
      return;
    }

    setIsDebating(true);
    setError(null);
    setDebateResult(null);

    try {
      const market = {
        price: currentPrice || 0,
        symbol: selectedSymbol,
        ...marketData
      };

      const response = await agnoTradingService.runDebate(
        selectedSymbol,
        market,
        bullModel,
        bearModel,
        analystModel
      );

      if (response.success && response.data) {
        setDebateResult(response.data);
        await loadRecentDebates(); // Refresh history
      } else {
        setError(response.error || 'Debate failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setIsDebating(false);
    }
  };

  const canStartDebate = availableModels.length >= 3 && !isDebating;

  return (
    <div style={{
      height: '100%',
      background: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '4px',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div style={{
        background: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        padding: '8px 10px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <MessageSquare size={14} color={FINCEPT.ORANGE} />
          <span style={{
            color: FINCEPT.WHITE,
            fontSize: '11px',
            fontWeight: '600',
            letterSpacing: '0.5px'
          }}>
            DEBATE ARENA
          </span>
          {isDebating && (
            <Loader2 size={12} color={FINCEPT.ORANGE} className="animate-spin" />
          )}
        </div>
        <span style={{ color: FINCEPT.GRAY, fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }}>
          {selectedSymbol}
        </span>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '10px', display: 'flex', flexDirection: 'column', gap: '10px' }}>

        {/* Model Selection */}
        {!debateResult && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            {/* Bull Model */}
            <div>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                marginBottom: '4px'
              }}>
                <TrendingUp size={10} color={FINCEPT.GREEN} />
                <label style={{
                  color: FINCEPT.GREEN,
                  fontSize: '9px',
                  fontWeight: '600',
                  letterSpacing: '0.5px'
                }}>
                  BULL AGENT
                </label>
              </div>
              <select
                value={bullModel}
                onChange={(e) => setBullModel(e.target.value)}
                disabled={isDebating || availableModels.length === 0}
                style={{
                  width: '100%',
                  background: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.GREEN}`,
                  color: FINCEPT.WHITE,
                  padding: '5px 8px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  cursor: 'pointer'
                }}
              >
                {availableModels.map(m => (
                  <option key={m.value} value={m.value}>{m.label}</option>
                ))}
              </select>
            </div>

            {/* Bear Model */}
            <div>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                marginBottom: '4px'
              }}>
                <TrendingDown size={10} color={FINCEPT.RED} />
                <label style={{
                  color: FINCEPT.RED,
                  fontSize: '9px',
                  fontWeight: '600',
                  letterSpacing: '0.5px'
                }}>
                  BEAR AGENT
                </label>
              </div>
              <select
                value={bearModel}
                onChange={(e) => setBearModel(e.target.value)}
                disabled={isDebating || availableModels.length === 0}
                style={{
                  width: '100%',
                  background: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.RED}`,
                  color: FINCEPT.WHITE,
                  padding: '5px 8px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  cursor: 'pointer'
                }}
              >
                {availableModels.map(m => (
                  <option key={m.value} value={m.value}>{m.label}</option>
                ))}
              </select>
            </div>

            {/* Analyst Model */}
            <div>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                marginBottom: '4px'
              }}>
                <Brain size={10} color={FINCEPT.ORANGE} />
                <label style={{
                  color: FINCEPT.ORANGE,
                  fontSize: '9px',
                  fontWeight: '600',
                  letterSpacing: '0.5px'
                }}>
                  ANALYST AGENT
                </label>
              </div>
              <select
                value={analystModel}
                onChange={(e) => setAnalystModel(e.target.value)}
                disabled={isDebating || availableModels.length === 0}
                style={{
                  width: '100%',
                  background: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.ORANGE}`,
                  color: FINCEPT.WHITE,
                  padding: '5px 8px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  cursor: 'pointer'
                }}
              >
                {availableModels.map(m => (
                  <option key={m.value} value={m.value}>{m.label}</option>
                ))}
              </select>
            </div>

            {/* Start Button */}
            <button
              onClick={handleStartDebate}
              disabled={!canStartDebate}
              style={{
                width: '100%',
                background: canStartDebate ? FINCEPT.ORANGE : FINCEPT.GRAY,
                border: 'none',
                color: canStartDebate ? FINCEPT.DARK_BG : FINCEPT.MUTED,
                padding: '10px',
                borderRadius: '2px',
                fontSize: '10px',
                fontWeight: '700',
                cursor: canStartDebate ? 'pointer' : 'not-allowed',
                opacity: canStartDebate ? 1 : 0.5,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
                letterSpacing: '0.5px',
                marginTop: '4px'
              }}
            >
              {isDebating ? (
                <>
                  <Loader2 size={14} className="animate-spin" />
                  DEBATING...
                </>
              ) : (
                <>
                  <Play size={14} />
                  START DEBATE
                </>
              )}
            </button>

            {availableModels.length < 3 && (
              <div style={{
                background: `${FINCEPT.YELLOW}15`,
                border: `1px solid ${FINCEPT.YELLOW}`,
                borderRadius: '2px',
                padding: '6px 8px',
                fontSize: '8px',
                color: FINCEPT.YELLOW
              }}>
                Configure at least 3 models in Settings → LLM Config
              </div>
            )}
          </div>
        )}

        {/* Error Display */}
        {error && (
          <div style={{
            background: `${FINCEPT.RED}15`,
            border: `1px solid ${FINCEPT.RED}`,
            borderRadius: '2px',
            padding: '8px',
            display: 'flex',
            alignItems: 'flex-start',
            gap: '6px'
          }}>
            <AlertCircle size={12} color={FINCEPT.RED} style={{ flexShrink: 0, marginTop: '1px' }} />
            <div>
              <div style={{ color: FINCEPT.RED, fontSize: '9px', fontWeight: '600' }}>ERROR</div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '8px', marginTop: '2px' }}>{error}</div>
            </div>
          </div>
        )}

        {/* Debate Result */}
        {debateResult && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
            {/* Final Decision Card */}
            <div style={{
              background: FINCEPT.DARK_BG,
              border: `2px solid ${FINCEPT.ORANGE}`,
              borderRadius: '4px',
              padding: '10px'
            }}>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                marginBottom: '8px'
              }}>
                <CheckCircle size={14} color={FINCEPT.ORANGE} />
                <span style={{ color: FINCEPT.ORANGE, fontSize: '10px', fontWeight: '700', letterSpacing: '0.5px' }}>
                  FINAL DECISION
                </span>
              </div>

              <div style={{
                background: FINCEPT.PANEL_BG,
                borderRadius: '2px',
                padding: '8px',
                marginBottom: '8px'
              }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '8px' }}>
                  <div>
                    <div style={{ color: FINCEPT.GRAY, fontSize: '7px', marginBottom: '3px' }}>ACTION</div>
                    <div style={{
                      color: debateResult.final_action === 'BUY' ? FINCEPT.GREEN :
                             debateResult.final_action === 'SELL' ? FINCEPT.RED : FINCEPT.YELLOW,
                      fontSize: '14px',
                      fontWeight: '700',
                      fontFamily: '"IBM Plex Mono", monospace'
                    }}>
                      {debateResult.final_action}
                    </div>
                  </div>
                  <div>
                    <div style={{ color: FINCEPT.GRAY, fontSize: '7px', marginBottom: '3px' }}>CONFIDENCE</div>
                    <div style={{
                      color: FINCEPT.CYAN,
                      fontSize: '14px',
                      fontWeight: '700',
                      fontFamily: '"IBM Plex Mono", monospace'
                    }}>
                      {debateResult.confidence}%
                    </div>
                  </div>
                </div>

                {debateResult.entry_price && (
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '6px', fontSize: '8px' }}>
                    <div>
                      <div style={{ color: FINCEPT.GRAY, marginBottom: '2px' }}>ENTRY</div>
                      <div style={{ color: FINCEPT.WHITE, fontWeight: '600', fontFamily: '"IBM Plex Mono", monospace' }}>
                        ${debateResult.entry_price.toFixed(2)}
                      </div>
                    </div>
                    {debateResult.stop_loss && (
                      <div>
                        <div style={{ color: FINCEPT.GRAY, marginBottom: '2px' }}>STOP</div>
                        <div style={{ color: FINCEPT.RED, fontWeight: '600', fontFamily: '"IBM Plex Mono", monospace' }}>
                          ${debateResult.stop_loss.toFixed(2)}
                        </div>
                      </div>
                    )}
                    {debateResult.take_profit && (
                      <div>
                        <div style={{ color: FINCEPT.GRAY, marginBottom: '2px' }}>TARGET</div>
                        <div style={{ color: FINCEPT.GREEN, fontWeight: '600', fontFamily: '"IBM Plex Mono", monospace' }}>
                          ${debateResult.take_profit.toFixed(2)}
                        </div>
                      </div>
                    )}
                  </div>
                )}
              </div>

              <button
                onClick={() => setDebateResult(null)}
                style={{
                  width: '100%',
                  background: 'transparent',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.GRAY,
                  padding: '6px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '600',
                  cursor: 'pointer',
                  letterSpacing: '0.3px'
                }}
              >
                NEW DEBATE
              </button>
            </div>

            {/* Arguments */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {/* Bull Argument */}
              <DebateArgumentCard
                role="BULL"
                model={bullModel}
                argument={debateResult.bull_argument}
                color={FINCEPT.GREEN}
              />

              {/* Bear Argument */}
              <DebateArgumentCard
                role="BEAR"
                model={bearModel}
                argument={debateResult.bear_argument}
                color={FINCEPT.RED}
              />

              {/* Analyst Decision */}
              <DebateArgumentCard
                role="ANALYST"
                model={analystModel}
                argument={debateResult.analyst_decision}
                color={FINCEPT.ORANGE}
              />
            </div>

            {/* Execution Time */}
            <div style={{
              textAlign: 'center',
              color: FINCEPT.MUTED,
              fontSize: '8px',
              fontFamily: '"IBM Plex Mono", monospace'
            }}>
              Execution time: {(debateResult.execution_time_ms / 1000).toFixed(1)}s
            </div>
          </div>
        )}

        {/* Recent Debates */}
        {!debateResult && recentDebates.length > 0 && (
          <div style={{ marginTop: '10px' }}>
            <div style={{
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: '600',
              marginBottom: '6px',
              letterSpacing: '0.5px'
            }}>
              RECENT DEBATES
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {recentDebates.slice(0, 5).map((debate, idx) => (
                <div
                  key={idx}
                  style={{
                    background: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    padding: '6px 8px',
                    fontSize: '8px',
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center'
                  }}
                >
                  <span style={{ color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace' }}>
                    {debate.symbol}
                  </span>
                  <span style={{
                    color: debate.final_action === 'BUY' ? FINCEPT.GREEN :
                           debate.final_action === 'SELL' ? FINCEPT.RED : FINCEPT.YELLOW,
                    fontWeight: '700'
                  }}>
                    {debate.final_action}
                  </span>
                  <span style={{ color: FINCEPT.GRAY }}>
                    {new Date(debate.timestamp * 1000).toLocaleTimeString()}
                  </span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

// ============================================================================
// Debate Argument Card Component
// ============================================================================

interface DebateArgumentCardProps {
  role: string;
  model: string;
  argument: string;
  color: string;
}

function DebateArgumentCard({ role, model, argument, color }: DebateArgumentCardProps) {
  const [isExpanded, setIsExpanded] = useState(false);

  return (
    <div style={{
      background: FINCEPT.DARK_BG,
      border: `1px solid ${color}`,
      borderRadius: '2px',
      overflow: 'hidden'
    }}>
      <div
        onClick={() => setIsExpanded(!isExpanded)}
        style={{
          padding: '6px 8px',
          background: `${color}10`,
          borderBottom: isExpanded ? `1px solid ${FINCEPT.BORDER}` : 'none',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between'
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          {role === 'BULL' && <TrendingUp size={10} color={color} />}
          {role === 'BEAR' && <TrendingDown size={10} color={color} />}
          {role === 'ANALYST' && <Brain size={10} color={color} />}
          <span style={{ color, fontSize: '9px', fontWeight: '700', letterSpacing: '0.3px' }}>
            {role}
          </span>
        </div>
        <span style={{ color: FINCEPT.GRAY, fontSize: '8px', fontFamily: '"IBM Plex Mono", monospace' }}>
          {model.split(':')[1] || model}
        </span>
      </div>
      {isExpanded && (
        <div style={{
          padding: '8px',
          color: FINCEPT.WHITE,
          fontSize: '9px',
          lineHeight: '1.5',
          maxHeight: '200px',
          overflow: 'auto',
          whiteSpace: 'pre-wrap',
          fontFamily: '"IBM Plex Mono", monospace'
        }}>
          {argument.substring(0, 500)}{argument.length > 500 ? '...' : ''}
        </div>
      )}
    </div>
  );
}

export default DebateArenaPanel;
