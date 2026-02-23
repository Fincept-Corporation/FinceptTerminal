/**
 * Alpha Arena Right Panel
 *
 * 7-tab side panel: Decisions, HITL, Sentiment, Metrics, Grid, Research, Broker
 * Fincept Terminal Design - sharp edges, inline styles, monospace font.
 */

import React from 'react';
import {
  Brain, Shield, Newspaper, BarChart3, Grid3X3, FileSearch, Building,
  Sparkles,
} from 'lucide-react';
import {
  HITLApprovalPanel,
  SentimentPanel,
  PortfolioMetricsPanel,
  GridStrategyPanel,
  ResearchPanel,
  BrokerSelector,
} from './components';
import type { ModelDecision } from './types';
import type { RightPanelTab } from './useAlphaArena';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  PURPLE: '#9D4EDD',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
} as const;

const TAB_ITEMS: { key: RightPanelTab; icon: React.ComponentType<{ size?: number }>; label: string }[] = [
  { key: 'decisions', icon: Brain, label: 'DECISIONS' },
  { key: 'hitl', icon: Shield, label: 'HITL' },
  { key: 'sentiment', icon: Newspaper, label: 'SENTIMENT' },
  { key: 'metrics', icon: BarChart3, label: 'METRICS' },
  { key: 'grid', icon: Grid3X3, label: 'GRID' },
  { key: 'research', icon: FileSearch, label: 'RESEARCH' },
  { key: 'broker', icon: Building, label: 'BROKER' },
];

interface AlphaArenaRightPanelProps {
  rightPanelTab: RightPanelTab;
  setRightPanelTab: (tab: RightPanelTab) => void;
  decisions: ModelDecision[];
  competitionId: string | null;
  symbol: string;
  latestPrice: number | null;
  selectedBroker: string | null;
  setSelectedBroker: (broker: string | null) => void;
  refreshCompetitionData: () => void;
}

const AlphaArenaRightPanel: React.FC<AlphaArenaRightPanelProps> = ({
  rightPanelTab,
  setRightPanelTab,
  decisions,
  competitionId,
  symbol,
  latestPrice,
  selectedBroker,
  setSelectedBroker,
  refreshCompetitionData,
}) => {
  return (
    <div style={{
      width: '400px',
      borderLeft: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
    }}>
      {/* Tab Bar */}
      <div style={{
        display: 'flex',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        overflowX: 'auto',
        backgroundColor: FINCEPT.HEADER_BG,
        flexShrink: 0,
      }}>
        {TAB_ITEMS.map(({ key, icon: Icon, label }) => (
          <button
            key={key}
            onClick={() => setRightPanelTab(key)}
            style={{
              padding: '6px 8px',
              fontSize: '9px',
              fontWeight: rightPanelTab === key ? 700 : 600,
              whiteSpace: 'nowrap',
              display: 'flex',
              alignItems: 'center',
              gap: '3px',
              backgroundColor: rightPanelTab === key ? FINCEPT.ORANGE : 'transparent',
              color: rightPanelTab === key ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: 'none',
              borderBottom: rightPanelTab === key ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
              cursor: 'pointer',
              transition: 'all 0.15s',
              letterSpacing: '0.3px',
            }}
            onMouseEnter={(e) => {
              if (rightPanelTab !== key) {
                e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                e.currentTarget.style.color = FINCEPT.WHITE;
              }
            }}
            onMouseLeave={(e) => {
              if (rightPanelTab !== key) {
                e.currentTarget.style.backgroundColor = 'transparent';
                e.currentTarget.style.color = FINCEPT.GRAY;
              }
            }}
          >
            <Icon size={10} />
            {label}
          </button>
        ))}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {rightPanelTab === 'decisions' && (
          <DecisionsContent decisions={decisions} />
        )}
        {rightPanelTab === 'hitl' && (
          <div style={{ padding: '8px' }}>
            <HITLApprovalPanel
              competitionId={competitionId || undefined}
              onApprovalChange={() => refreshCompetitionData()}
            />
          </div>
        )}
        {rightPanelTab === 'sentiment' && (
          <div style={{ padding: '8px' }}>
            <SentimentPanel
              symbol={symbol.split('/')[0]}
              showMarketMood={true}
            />
          </div>
        )}
        {rightPanelTab === 'metrics' && competitionId && (
          <div style={{ padding: '8px' }}>
            <PortfolioMetricsPanel competitionId={competitionId} />
          </div>
        )}
        {rightPanelTab === 'metrics' && !competitionId && (
          <div style={{ padding: '32px', textAlign: 'center', color: FINCEPT.GRAY }}>
            <BarChart3 size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
            <p style={{ fontSize: '11px' }}>Start a competition to view metrics</p>
          </div>
        )}
        {rightPanelTab === 'grid' && (
          <div style={{ padding: '8px' }}>
            <GridStrategyPanel
              symbol={symbol.split('/')[0]}
              currentPrice={latestPrice || undefined}
            />
          </div>
        )}
        {rightPanelTab === 'research' && (
          <div style={{ padding: '8px' }}>
            <ResearchPanel />
          </div>
        )}
        {rightPanelTab === 'broker' && (
          <div style={{ padding: '8px' }}>
            <BrokerSelector
              selectedBrokerId={selectedBroker || undefined}
              onBrokerSelect={(broker) => setSelectedBroker(broker.id)}
            />
          </div>
        )}
      </div>
    </div>
  );
};

// Get action color for Polymarket or crypto decisions
const getActionColor = (action: string): string => {
  switch (action?.toLowerCase()) {
    case 'buy': case 'buy_yes': return '#00D66F';
    case 'sell': case 'sell_yes': return '#FF3B3B';
    case 'buy_no': return '#FF3B3B';
    case 'sell_no': return '#00D66F';
    case 'short': return '#9D4EDD';
    default: return '#787878';
  }
};

const getActionBg = (action: string): string => {
  const color = getActionColor(action);
  return `${color}20`;
};

// Decisions sub-content - Terminal style
const DecisionsContent: React.FC<{ decisions: ModelDecision[] }> = ({ decisions }) => {
  const safeDecisions = Array.isArray(decisions) ? decisions : [];

  // Check if decisions are Polymarket-style (action contains buy_yes, buy_no, etc.)
  const isPolymarket = safeDecisions.some(d =>
    ['buy_yes', 'buy_no', 'sell_yes', 'sell_no'].includes(d.action)
  );

  return (
    <div style={{ display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.CARD_BG }}>
      {/* Header */}
      <div style={{
        padding: '8px 12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        backgroundColor: FINCEPT.HEADER_BG,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Brain size={14} style={{ color: FINCEPT.PURPLE }} />
          <span style={{ fontWeight: 700, fontSize: '11px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            {isPolymarket ? 'PM DECISIONS' : 'AI DECISIONS'}
          </span>
        </div>
        <span style={{
          fontSize: '9px',
          padding: '1px 6px',
          backgroundColor: FINCEPT.BORDER,
          color: FINCEPT.GRAY,
          fontWeight: 600,
        }}>
          {safeDecisions.length}
        </span>
      </div>

      {/* List */}
      <div style={{ overflowY: 'auto' }}>
        {safeDecisions.length === 0 ? (
          <div style={{ padding: '32px', textAlign: 'center', color: FINCEPT.GRAY }}>
            <Sparkles size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
            <p style={{ fontSize: '11px' }}>No decisions yet</p>
            <p style={{ fontSize: '9px', marginTop: '4px' }}>Run a cycle to see AI trading decisions</p>
          </div>
        ) : (
          <div>
            {safeDecisions.slice(0, 50).map((decision, idx) => {
              const confidence = decision.confidence ?? 0;
              const actionColor = getActionColor(decision.action);
              const isHighConfidence = confidence >= 0.7;

              return (
                <div
                  key={`${decision.model_name}-${decision.cycle_number}-${idx}`}
                  style={{
                    padding: '8px 10px',
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    borderLeft: `3px solid ${actionColor}`,
                    transition: 'background-color 0.15s',
                    cursor: 'default',
                  }}
                  onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG; }}
                  onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
                    <span style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.WHITE }}>
                      {decision.model_name}
                    </span>
                    <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                      Cycle {decision.cycle_number}
                    </span>
                  </div>

                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexWrap: 'wrap' }}>
                    <span style={{
                      fontSize: '9px',
                      padding: '1px 6px',
                      fontWeight: 700,
                      letterSpacing: '0.5px',
                      backgroundColor: getActionBg(decision.action),
                      color: actionColor,
                    }}>
                      {(decision.action || 'hold').toUpperCase().replace('_', ' ')}
                    </span>
                    <span style={{ fontSize: '9px', color: FINCEPT.GRAY, maxWidth: '180px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                      {decision.symbol}
                    </span>
                    {decision.quantity > 0 && (
                      <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {isPolymarket ? `$${(decision.quantity ?? 0).toFixed(2)}` : `Qty: ${(decision.quantity ?? 0).toFixed(4)}`}
                      </span>
                    )}
                    {isHighConfidence && decision.action !== 'hold' && (
                      <span style={{
                        fontSize: '8px',
                        padding: '0 4px',
                        fontWeight: 700,
                        backgroundColor: `${FINCEPT.ORANGE}20`,
                        color: FINCEPT.ORANGE,
                      }}>
                        HIGH EDGE
                      </span>
                    )}
                  </div>

                  {/* Confidence bar */}
                  <div style={{ marginTop: '5px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <div style={{
                        flex: 1,
                        height: '3px',
                        backgroundColor: FINCEPT.BORDER,
                        overflow: 'hidden',
                      }}>
                        <div style={{
                          width: `${confidence * 100}%`,
                          height: '100%',
                          backgroundColor: confidence >= 0.7 ? FINCEPT.GREEN : confidence >= 0.4 ? FINCEPT.ORANGE : FINCEPT.RED,
                          transition: 'width 0.3s',
                        }} />
                      </div>
                      <span style={{
                        fontSize: '9px',
                        fontWeight: 600,
                        color: confidence >= 0.7 ? FINCEPT.GREEN : confidence >= 0.4 ? FINCEPT.ORANGE : FINCEPT.RED,
                        minWidth: '30px',
                        textAlign: 'right',
                      }}>
                        {(confidence * 100).toFixed(0)}%
                      </span>
                    </div>
                  </div>

                  {decision.reasoning && (
                    <p style={{
                      fontSize: '9px',
                      marginTop: '4px',
                      color: FINCEPT.MUTED,
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      display: '-webkit-box',
                      WebkitLineClamp: 2,
                      WebkitBoxOrient: 'vertical',
                    }}>
                      {decision.reasoning}
                    </p>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
};

export default AlphaArenaRightPanel;
