/**
 * ModelChat Panel
 *
 * Real-time AI agent decision stream (Alpha Arena style)
 * Shows what each model is thinking, deciding, and trading
 */

import React, { useState, useEffect, useRef } from 'react';
import { Brain, TrendingUp, TrendingDown, Circle, Filter } from 'lucide-react';

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

interface ModelDecision {
  model: string;
  timestamp: string;
  content: string;
  action?: 'long' | 'short' | 'hold' | 'close';
  symbol?: string;
  confidence?: number;
  entry?: number;
  target?: number;
  stopLoss?: number;
}

interface ModelChatPanelProps {
  teamId?: string;
  refreshInterval?: number;
}

export function ModelChatPanel({ teamId, refreshInterval = 5000 }: ModelChatPanelProps) {
  const [decisions, setDecisions] = useState<ModelDecision[]>([]);
  const [selectedModel, setSelectedModel] = useState<string>('all');
  const [isLoading, setIsLoading] = useState(false);
  const [uniqueModels, setUniqueModels] = useState<string[]>([]);
  const chatEndRef = useRef<HTMLDivElement>(null);

  // Auto-scroll to bottom
  const scrollToBottom = () => {
    chatEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  useEffect(() => {
    scrollToBottom();
  }, [decisions]);

  // Fetch recent decisions
  const fetchDecisions = async () => {
    try {
      setIsLoading(true);
      const { invoke } = await import('@tauri-apps/api/core');
      const response = await invoke('agno_get_recent_decisions', { limit: 100 });
      const data = JSON.parse(response as string);

      if (data.success && data.data?.decisions) {
        setDecisions(data.data.decisions);

        // Extract unique models
        const models = [...new Set(data.data.decisions.map((d: any) => d.model))];
        setUniqueModels(models as string[]);
      }
    } catch (err) {
      console.error('Failed to fetch decisions:', err);
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => {
    fetchDecisions();

    // Refresh periodically
    const interval = setInterval(fetchDecisions, refreshInterval);
    return () => clearInterval(interval);
  }, [teamId, refreshInterval]);

  // Filter decisions by model
  const filteredDecisions = selectedModel === 'all'
    ? decisions
    : decisions.filter(d => d.model === selectedModel);

  const getModelColor = (model: string) => {
    const colors = [BLOOMBERG.ORANGE, BLOOMBERG.CYAN, BLOOMBERG.GREEN, BLOOMBERG.PURPLE, BLOOMBERG.YELLOW];
    const hash = model.split('').reduce((acc, char) => acc + char.charCodeAt(0), 0);
    return colors[hash % colors.length];
  };

  const getActionColor = (action?: string) => {
    switch (action) {
      case 'long': return BLOOMBERG.GREEN;
      case 'short': return BLOOMBERG.RED;
      case 'close': return BLOOMBERG.ORANGE;
      default: return BLOOMBERG.GRAY;
    }
  };

  const formatTimestamp = (timestamp: string) => {
    const date = new Date(timestamp);
    const now = new Date();
    const diffSeconds = Math.floor((now.getTime() - date.getTime()) / 1000);

    if (diffSeconds < 60) return `${diffSeconds}s ago`;
    if (diffSeconds < 3600) return `${Math.floor(diffSeconds / 60)}m ago`;
    return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
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
          <Brain size={14} color={BLOOMBERG.ORANGE} />
          <span style={{
            color: BLOOMBERG.WHITE,
            fontSize: '11px',
            fontWeight: '600',
            letterSpacing: '0.5px'
          }}>
            MODELCHAT
          </span>
          {isLoading && (
            <Circle size={8} color={BLOOMBERG.ORANGE} style={{ animation: 'pulse 2s infinite' }} />
          )}
        </div>

        {/* Model Filter */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Filter size={12} color={BLOOMBERG.GRAY} />
          <select
            value={selectedModel}
            onChange={(e) => setSelectedModel(e.target.value)}
            style={{
              background: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.WHITE,
              padding: '3px 6px',
              borderRadius: '2px',
              fontSize: '9px',
              cursor: 'pointer',
              fontFamily: '"IBM Plex Mono", monospace'
            }}
          >
            <option value="all">ALL MODELS</option>
            {uniqueModels.map(model => (
              <option key={model} value={model}>{model}</option>
            ))}
          </select>
        </div>
      </div>

      {/* Chat Stream */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '10px',
        display: 'flex',
        flexDirection: 'column',
        gap: '8px'
      }}>
        {filteredDecisions.length === 0 ? (
          <div style={{
            textAlign: 'center',
            color: BLOOMBERG.GRAY,
            fontSize: '10px',
            marginTop: '40px'
          }}>
            {isLoading ? 'Loading decisions...' : 'No decisions yet. Start a competition to see model reasoning.'}
          </div>
        ) : (
          filteredDecisions.map((decision, idx) => (
            <DecisionCard
              key={`${decision.model}-${decision.timestamp}-${idx}`}
              decision={decision}
              modelColor={getModelColor(decision.model)}
              actionColor={getActionColor(decision.action)}
              formatTimestamp={formatTimestamp}
            />
          ))
        )}
        <div ref={chatEndRef} />
      </div>
    </div>
  );
}

interface DecisionCardProps {
  decision: ModelDecision;
  modelColor: string;
  actionColor: string;
  formatTimestamp: (timestamp: string) => string;
}

function DecisionCard({ decision, modelColor, actionColor, formatTimestamp }: DecisionCardProps) {
  return (
    <div style={{
      background: BLOOMBERG.DARK_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderLeft: `3px solid ${modelColor}`,
      borderRadius: '2px',
      padding: '8px 10px'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '6px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <div style={{
            width: '6px',
            height: '6px',
            borderRadius: '50%',
            background: modelColor
          }} />
          <span style={{
            color: modelColor,
            fontSize: '10px',
            fontWeight: '700',
            fontFamily: '"IBM Plex Mono", monospace',
            textTransform: 'uppercase'
          }}>
            {decision.model.split(':')[1] || decision.model}
          </span>

          {decision.action && (
            <div style={{
              background: `${actionColor}20`,
              border: `1px solid ${actionColor}`,
              borderRadius: '2px',
              padding: '2px 6px',
              display: 'flex',
              alignItems: 'center',
              gap: '3px'
            }}>
              {decision.action === 'long' ? (
                <TrendingUp size={10} color={actionColor} />
              ) : decision.action === 'short' ? (
                <TrendingDown size={10} color={actionColor} />
              ) : null}
              <span style={{
                color: actionColor,
                fontSize: '8px',
                fontWeight: '700',
                letterSpacing: '0.5px'
              }}>
                {decision.action.toUpperCase()}
              </span>
            </div>
          )}

          {decision.symbol && (
            <span style={{
              color: BLOOMBERG.GRAY,
              fontSize: '8px',
              fontFamily: '"IBM Plex Mono", monospace'
            }}>
              {decision.symbol}
            </span>
          )}
        </div>

        <span style={{
          color: BLOOMBERG.MUTED,
          fontSize: '8px',
          fontFamily: '"IBM Plex Mono", monospace'
        }}>
          {formatTimestamp(decision.timestamp)}
        </span>
      </div>

      {/* Content */}
      <div style={{
        color: BLOOMBERG.WHITE,
        fontSize: '10px',
        lineHeight: '1.5',
        fontFamily: '"IBM Plex Mono", monospace',
        whiteSpace: 'pre-wrap'
      }}>
        {decision.content}
      </div>

      {/* Trade Details */}
      {(decision.entry || decision.target || decision.stopLoss) && (
        <div style={{
          marginTop: '6px',
          paddingTop: '6px',
          borderTop: `1px solid ${BLOOMBERG.BORDER}`,
          display: 'flex',
          gap: '12px',
          fontSize: '9px',
          fontFamily: '"IBM Plex Mono", monospace'
        }}>
          {decision.entry && (
            <div>
              <span style={{ color: BLOOMBERG.GRAY }}>ENTRY: </span>
              <span style={{ color: BLOOMBERG.CYAN, fontWeight: '600' }}>${decision.entry}</span>
            </div>
          )}
          {decision.target && (
            <div>
              <span style={{ color: BLOOMBERG.GRAY }}>TARGET: </span>
              <span style={{ color: BLOOMBERG.GREEN, fontWeight: '600' }}>${decision.target}</span>
            </div>
          )}
          {decision.stopLoss && (
            <div>
              <span style={{ color: BLOOMBERG.GRAY }}>STOP: </span>
              <span style={{ color: BLOOMBERG.RED, fontWeight: '600' }}>${decision.stopLoss}</span>
            </div>
          )}
          {decision.confidence && (
            <div>
              <span style={{ color: BLOOMBERG.GRAY }}>CONF: </span>
              <span style={{ color: BLOOMBERG.YELLOW, fontWeight: '600' }}>{(decision.confidence * 100).toFixed(0)}%</span>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
