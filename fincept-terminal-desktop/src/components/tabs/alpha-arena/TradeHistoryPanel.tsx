/**
 * Trade History Panel - Shows executed trades with full details
 */

import React from 'react';
import { TrendingUp, TrendingDown, Circle, CheckCircle, XCircle } from 'lucide-react';
import { AlphaDecisionLog } from '@/services/core/sqliteService';

interface TradeHistoryPanelProps {
  decisions: AlphaDecisionLog[];
}

const BLOOMBERG = {
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#555555',
  WHITE: '#FFFFFF',
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
};

export const TradeHistoryPanel: React.FC<TradeHistoryPanelProps> = ({ decisions }) => {
  const formatTime = (timestamp: string) => {
    const date = new Date(timestamp);
    return date.toLocaleTimeString('en-US', {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  };

  const parseTradeExecuted = (trade: string | null) => {
    if (!trade) return null;
    try {
      return JSON.parse(trade);
    } catch {
      return null;
    }
  };

  if (decisions.length === 0) {
    return (
      <div style={{
        padding: '24px',
        textAlign: 'center',
        color: BLOOMBERG.GRAY,
        fontSize: '10px',
        fontFamily: '"IBM Plex Mono", monospace'
      }}>
        No trade history yet. Start the competition to see executed trades.
      </div>
    );
  }

  return (
    <div style={{
      flex: 1,
      overflow: 'auto',
      fontFamily: '"IBM Plex Mono", monospace'
    }}>
      {/* Header */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '100px 80px 120px 100px 100px 100px 1fr 80px',
        padding: '8px 12px',
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
        fontSize: '8px',
        fontWeight: 700,
        color: BLOOMBERG.GRAY,
        letterSpacing: '0.5px',
        position: 'sticky',
        top: 0
      }}>
        <div>TIME</div>
        <div>CYCLE</div>
        <div>MODEL</div>
        <div>ACTION</div>
        <div>QUANTITY</div>
        <div>PRICE</div>
        <div>REASON</div>
        <div>STATUS</div>
      </div>

      {/* Trade Rows */}
      {decisions.map((decision, idx) => {
        const tradeExecuted = parseTradeExecuted(decision.trade_executed);
        const isExecuted = tradeExecuted?.status === 'executed';
        const isRejected = tradeExecuted?.status === 'rejected';
        const isHold = decision.action === 'hold';

        return (
          <div
            key={decision.id || idx}
            style={{
              display: 'grid',
              gridTemplateColumns: '100px 80px 120px 100px 100px 100px 1fr 80px',
              padding: '10px 12px',
              borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
              fontSize: '9px',
              color: BLOOMBERG.WHITE,
              alignItems: 'center',
              backgroundColor: idx % 2 === 0 ? BLOOMBERG.DARK_BG : BLOOMBERG.PANEL_BG
            }}
          >
            {/* Time */}
            <div style={{ color: BLOOMBERG.GRAY }}>
              {formatTime(decision.timestamp)}
            </div>

            {/* Cycle */}
            <div style={{ color: BLOOMBERG.CYAN, fontWeight: 600 }}>
              #{decision.cycle_number}
            </div>

            {/* Model */}
            <div style={{
              color: BLOOMBERG.ORANGE,
              fontSize: '8px',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              whiteSpace: 'nowrap'
            }}>
              {decision.model_name}
            </div>

            {/* Action */}
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontWeight: 700
            }}>
              {decision.action === 'buy' && (
                <>
                  <TrendingUp size={12} color={BLOOMBERG.GREEN} />
                  <span style={{ color: BLOOMBERG.GREEN }}>BUY</span>
                </>
              )}
              {decision.action === 'sell' && (
                <>
                  <TrendingDown size={12} color={BLOOMBERG.RED} />
                  <span style={{ color: BLOOMBERG.RED }}>SELL</span>
                </>
              )}
              {decision.action === 'hold' && (
                <>
                  <Circle size={12} color={BLOOMBERG.GRAY} />
                  <span style={{ color: BLOOMBERG.GRAY }}>HOLD</span>
                </>
              )}
            </div>

            {/* Quantity */}
            <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>
              {decision.quantity ? `×${decision.quantity.toFixed(3)}` : '-'}
            </div>

            {/* Price */}
            <div style={{ color: BLOOMBERG.CYAN }}>
              {decision.price_at_decision
                ? `$${decision.price_at_decision.toLocaleString('en-US', { minimumFractionDigits: 2 })}`
                : '-'}
            </div>

            {/* Reasoning */}
            <div style={{
              color: BLOOMBERG.GRAY,
              fontSize: '8px',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              whiteSpace: 'nowrap'
            }}>
              {decision.reasoning || '-'}
            </div>

            {/* Status */}
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontSize: '8px',
              fontWeight: 600
            }}>
              {isExecuted && (
                <>
                  <CheckCircle size={10} color={BLOOMBERG.GREEN} />
                  <span style={{ color: BLOOMBERG.GREEN }}>EXEC</span>
                </>
              )}
              {isRejected && (
                <>
                  <XCircle size={10} color={BLOOMBERG.RED} />
                  <span style={{ color: BLOOMBERG.RED }}>REJ</span>
                </>
              )}
              {isHold && (
                <>
                  <Circle size={10} color={BLOOMBERG.GRAY} />
                  <span style={{ color: BLOOMBERG.GRAY }}>HOLD</span>
                </>
              )}
              {!isExecuted && !isRejected && !isHold && (
                <span style={{ color: BLOOMBERG.YELLOW }}>PEND</span>
              )}
            </div>
          </div>
        );
      })}
    </div>
  );
};

export default TradeHistoryPanel;
