/**
 * Decision Feed Panel
 * Real-time feed of all agent decisions
 */

import React, { useState, useEffect } from 'react';
import { Activity, Brain, Target, Shield, TrendingUp, Filter, RefreshCw } from 'lucide-react';
import agnoTradingService, { type Decision } from '../../../../services/agnoTradingService';

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

interface DecisionFeedPanelProps {
  agentId?: string;
  autoRefresh?: boolean;
  refreshInterval?: number;
}

export function DecisionFeedPanel({
  agentId,
  autoRefresh = true,
  refreshInterval = 5000
}: DecisionFeedPanelProps) {
  const [decisions, setDecisions] = useState<Decision[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [filterType, setFilterType] = useState<string>('all');
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    fetchDecisions();

    if (autoRefresh) {
      const interval = setInterval(fetchDecisions, refreshInterval);
      return () => clearInterval(interval);
    }
  }, [agentId, filterType, autoRefresh, refreshInterval]);

  const fetchDecisions = async () => {
    try {
      setIsLoading(true);
      setError(null);

      const response = await agnoTradingService.getDbDecisions(50, agentId);

      if (response.success && response.data) {
        let filtered = response.data.decisions || [];

        // Apply filter
        if (filterType !== 'all') {
          filtered = filtered.filter(d => d.decision_type === filterType);
        }

        setDecisions(filtered);
      } else {
        setError(response.error || 'Failed to load decisions');
      }
    } catch (err) {
      console.error('Failed to fetch decisions:', err);
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const getDecisionTypeColor = (type: string) => {
    const colors: Record<string, string> = {
      analysis: BLOOMBERG.BLUE,
      signal: BLOOMBERG.GREEN,
      trade: BLOOMBERG.ORANGE,
      risk: BLOOMBERG.RED
    };
    return colors[type] || BLOOMBERG.GRAY;
  };

  const getDecisionTypeIcon = (type: string) => {
    const icons: Record<string, any> = {
      analysis: Brain,
      signal: Target,
      trade: TrendingUp,
      risk: Shield
    };
    const Icon = icons[type] || Activity;
    return <Icon size={10} />;
  };

  return (
    <div style={{
      height: '100%',
      background: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderRadius: '4px',
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
          <Activity size={14} color={BLOOMBERG.CYAN} />
          <span style={{
            color: BLOOMBERG.WHITE,
            fontSize: '11px',
            fontWeight: '600',
            letterSpacing: '0.5px'
          }}>
            DECISION FEED
          </span>
          {isLoading && (
            <RefreshCw size={10} color={BLOOMBERG.ORANGE} className="animate-spin" />
          )}
        </div>

        <div style={{ display: 'flex', gap: '4px' }}>
          {/* Filter Buttons */}
          {['all', 'analysis', 'signal', 'trade', 'risk'].map(type => (
            <button
              key={type}
              onClick={() => setFilterType(type)}
              style={{
                background: filterType === type ? BLOOMBERG.ORANGE : 'transparent',
                border: `1px solid ${filterType === type ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                color: filterType === type ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                padding: '2px 6px',
                borderRadius: '2px',
                fontSize: '8px',
                fontWeight: '700',
                cursor: 'pointer',
                transition: 'all 0.2s',
                textTransform: 'uppercase',
                letterSpacing: '0.3px'
              }}
            >
              {type}
            </button>
          ))}
        </div>
      </div>

      {/* Content */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '8px'
      }}>
        {error && (
          <div style={{
            background: `${BLOOMBERG.RED}15`,
            border: `1px solid ${BLOOMBERG.RED}`,
            borderRadius: '2px',
            padding: '8px',
            marginBottom: '8px',
            color: BLOOMBERG.RED,
            fontSize: '9px'
          }}>
            {error}
          </div>
        )}

        {decisions.length === 0 && !isLoading && (
          <div style={{
            textAlign: 'center',
            color: BLOOMBERG.GRAY,
            fontSize: '10px',
            marginTop: '40px'
          }}>
            {filterType === 'all' ? 'No decisions yet' : `No ${filterType} decisions`}
          </div>
        )}

        <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
          {decisions.map((decision, idx) => (
            <DecisionCard key={decision.id || idx} decision={decision} />
          ))}
        </div>
      </div>
    </div>
  );
}

// ============================================================================
// Decision Card Component
// ============================================================================

interface DecisionCardProps {
  decision: Decision;
}

function DecisionCard({ decision }: DecisionCardProps) {
  const [isExpanded, setIsExpanded] = useState(false);

  const getDecisionTypeColor = (type: string) => {
    const colors: Record<string, string> = {
      analysis: BLOOMBERG.BLUE,
      signal: BLOOMBERG.GREEN,
      trade: BLOOMBERG.ORANGE,
      risk: BLOOMBERG.RED
    };
    return colors[type] || BLOOMBERG.GRAY;
  };

  const getDecisionTypeIcon = (type: string) => {
    const icons: Record<string, any> = {
      analysis: Brain,
      signal: Target,
      trade: TrendingUp,
      risk: Shield
    };
    const Icon = icons[type] || Activity;
    return <Icon size={10} />;
  };

  const typeColor = getDecisionTypeColor(decision.decision_type);
  const TypeIcon = getDecisionTypeIcon(decision.decision_type);

  return (
    <div
      onClick={() => setIsExpanded(!isExpanded)}
      style={{
        background: BLOOMBERG.DARK_BG,
        border: `1px solid ${typeColor}30`,
        borderLeft: `3px solid ${typeColor}`,
        borderRadius: '2px',
        padding: '8px',
        cursor: 'pointer',
        transition: 'all 0.2s'
      }}
      onMouseEnter={(e) => {
        e.currentTarget.style.background = BLOOMBERG.HOVER;
      }}
      onMouseLeave={(e) => {
        e.currentTarget.style.background = BLOOMBERG.DARK_BG;
      }}
    >
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: isExpanded ? '8px' : '0'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <div style={{ color: typeColor }}>
            {TypeIcon}
          </div>
          <span style={{
            color: typeColor,
            fontSize: '9px',
            fontWeight: '700',
            textTransform: 'uppercase',
            letterSpacing: '0.3px'
          }}>
            {decision.decision_type}
          </span>
          {decision.symbol && (
            <span style={{
              color: BLOOMBERG.WHITE,
              fontSize: '9px',
              fontFamily: '"IBM Plex Mono", monospace',
              fontWeight: '600'
            }}>
              {decision.symbol}
            </span>
          )}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {decision.confidence !== undefined && (
            <span style={{
              color: BLOOMBERG.CYAN,
              fontSize: '8px',
              fontWeight: '700',
              fontFamily: '"IBM Plex Mono", monospace'
            }}>
              {decision.confidence}%
            </span>
          )}
          <span style={{
            color: BLOOMBERG.GRAY,
            fontSize: '8px',
            fontFamily: '"IBM Plex Mono", monospace'
          }}>
            {new Date(decision.timestamp * 1000).toLocaleTimeString()}
          </span>
        </div>
      </div>

      {/* Decision Text */}
      <div style={{
        color: BLOOMBERG.WHITE,
        fontSize: '9px',
        lineHeight: '1.4',
        fontFamily: '"IBM Plex Mono", monospace',
        marginBottom: isExpanded && decision.reasoning ? '6px' : '0'
      }}>
        {isExpanded ? decision.decision : decision.decision.substring(0, 60) + (decision.decision.length > 60 ? '...' : '')}
      </div>

      {/* Reasoning (when expanded) */}
      {isExpanded && decision.reasoning && (
        <div style={{
          background: BLOOMBERG.PANEL_BG,
          borderRadius: '2px',
          padding: '6px',
          marginTop: '6px'
        }}>
          <div style={{
            color: BLOOMBERG.GRAY,
            fontSize: '7px',
            fontWeight: '700',
            marginBottom: '4px',
            letterSpacing: '0.5px'
          }}>
            REASONING
          </div>
          <div style={{
            color: BLOOMBERG.GRAY,
            fontSize: '8px',
            lineHeight: '1.4',
            fontFamily: '"IBM Plex Mono", monospace'
          }}>
            {decision.reasoning.substring(0, 200)}{decision.reasoning.length > 200 ? '...' : ''}
          </div>
        </div>
      )}

      {/* Footer */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginTop: '6px',
        paddingTop: '6px',
        borderTop: `1px solid ${BLOOMBERG.BORDER}`
      }}>
        <span style={{
          color: BLOOMBERG.MUTED,
          fontSize: '8px',
          fontFamily: '"IBM Plex Mono", monospace'
        }}>
          {decision.model.split(':')[1] || decision.model}
        </span>
        {decision.execution_time_ms !== undefined && (
          <span style={{
            color: BLOOMBERG.MUTED,
            fontSize: '7px',
            fontFamily: '"IBM Plex Mono", monospace'
          }}>
            {decision.execution_time_ms}ms
          </span>
        )}
      </div>
    </div>
  );
}

export default DecisionFeedPanel;
