/**
 * Position History Table
 * Displays closed position history with P&L
 */

import React, { useState } from 'react';
import { History, RefreshCw, AlertCircle, Info, TrendingUp, TrendingDown, Filter } from 'lucide-react';
import { usePositionHistory } from '../../hooks/useAdvancedMarketData';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  YELLOW: '#FFD700',
  CYAN: '#00E5FF',
};

interface PositionHistoryTableProps {
  symbol?: string;
  limit?: number;
}

export function PositionHistoryTable({ symbol, limit = 50 }: PositionHistoryTableProps) {
  const { tradingMode, activeBroker } = useBrokerContext();
  const { history, isLoading, error, isSupported, refresh } = usePositionHistory(symbol, undefined, limit);
  const [filterSide, setFilterSide] = useState<'all' | 'long' | 'short'>('all');

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <AlertCircle size={32} color={FINCEPT.YELLOW} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px' }}>Position history not available in paper trading</div>
      </div>
    );
  }

  if (!isSupported) {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <Info size={32} color={FINCEPT.GRAY} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px' }}>Position history not supported by {activeBroker}</div>
      </div>
    );
  }

  // Filter positions
  const filteredHistory = history.filter((pos) => filterSide === 'all' || pos.side === filterSide);

  // Calculate totals
  const totalPnl = filteredHistory.reduce((sum, pos) => sum + pos.realizedPnl, 0);
  const winCount = filteredHistory.filter((pos) => pos.realizedPnl > 0).length;
  const lossCount = filteredHistory.filter((pos) => pos.realizedPnl < 0).length;
  const winRate = filteredHistory.length > 0 ? (winCount / filteredHistory.length) * 100 : 0;

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '12px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <History size={14} color={FINCEPT.CYAN} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>Position History</span>
          {symbol && (
            <span style={{ fontSize: '10px', color: FINCEPT.GRAY, marginLeft: '8px' }}>({symbol})</span>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {/* Filter */}
          <div style={{ display: 'flex', gap: '4px' }}>
            {(['all', 'long', 'short'] as const).map((side) => (
              <button
                key={side}
                onClick={() => setFilterSide(side)}
                style={{
                  padding: '4px 8px',
                  backgroundColor: filterSide === side ? FINCEPT.ORANGE : 'transparent',
                  border: `1px solid ${filterSide === side ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  color: filterSide === side ? FINCEPT.WHITE : FINCEPT.GRAY,
                  fontSize: '9px',
                  cursor: 'pointer',
                  textTransform: 'uppercase',
                }}
              >
                {side}
              </button>
            ))}
          </div>
          <button
            onClick={refresh}
            disabled={isLoading}
            style={{
              padding: '4px',
              backgroundColor: 'transparent',
              border: 'none',
              cursor: 'pointer',
            }}
          >
            <RefreshCw size={12} color={FINCEPT.GRAY} style={{ animation: isLoading ? 'spin 1s linear infinite' : 'none' }} />
          </button>
        </div>
      </div>

      {/* Summary Stats */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '1px', backgroundColor: FINCEPT.BORDER }}>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TOTAL P&L</div>
          <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: totalPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED }}>
            {totalPnl >= 0 ? '+' : ''}{totalPnl.toFixed(2)}
          </div>
        </div>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>WIN RATE</div>
          <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: winRate >= 50 ? FINCEPT.GREEN : FINCEPT.RED }}>
            {winRate.toFixed(1)}%
          </div>
        </div>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>WINS</div>
          <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: FINCEPT.GREEN }}>
            {winCount}
          </div>
        </div>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>LOSSES</div>
          <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: FINCEPT.RED }}>
            {lossCount}
          </div>
        </div>
      </div>

      {/* Error */}
      {error && (
        <div style={{ padding: '10px 16px', backgroundColor: `${FINCEPT.RED}20`, fontSize: '11px', color: FINCEPT.RED }}>
          {error.message}
        </div>
      )}

      {/* Table Header */}
      <div style={{ display: 'grid', gridTemplateColumns: '100px 60px 80px 80px 80px 90px 80px', gap: '8px', padding: '10px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>SYMBOL</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>SIDE</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>ENTRY</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>EXIT</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>SIZE</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>P&L</span>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>CLOSED</span>
      </div>

      {/* Table Body */}
      <div style={{ maxHeight: '400px', overflowY: 'auto' }}>
        {filteredHistory.map((position, index) => {
          const pnlPercent = position.entryPrice > 0 ? ((position.exitPrice || 0) - position.entryPrice) / position.entryPrice * 100 * (position.side === 'long' ? 1 : -1) : 0;

          return (
            <div
              key={index}
              style={{
                display: 'grid',
                gridTemplateColumns: '100px 60px 80px 80px 80px 90px 80px',
                gap: '8px',
                padding: '10px 16px',
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                backgroundColor: index % 2 === 0 ? 'transparent' : `${FINCEPT.DARK_BG}50`,
              }}
            >
              {/* Symbol */}
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                <span style={{ fontSize: '11px', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace' }}>
                  {position.symbol.replace('/USDT', '').slice(0, 8)}
                </span>
              </div>

              {/* Side */}
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                {position.side === 'long' ? (
                  <TrendingUp size={10} color={FINCEPT.GREEN} />
                ) : (
                  <TrendingDown size={10} color={FINCEPT.RED} />
                )}
                <span style={{ fontSize: '10px', color: position.side === 'long' ? FINCEPT.GREEN : FINCEPT.RED, textTransform: 'uppercase' }}>
                  {position.side}
                </span>
              </div>

              {/* Entry Price */}
              <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace', textAlign: 'right' }}>
                {position.entryPrice.toFixed(position.entryPrice < 1 ? 6 : 2)}
              </span>

              {/* Exit Price */}
              <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace', textAlign: 'right' }}>
                {position.exitPrice ? position.exitPrice.toFixed(position.exitPrice < 1 ? 6 : 2) : '--'}
              </span>

              {/* Size */}
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY, fontFamily: '"IBM Plex Mono", monospace', textAlign: 'right' }}>
                {position.amount.toFixed(position.amount < 1 ? 6 : 2)}
              </span>

              {/* P&L */}
              <div style={{ textAlign: 'right' }}>
                <div style={{ fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', color: position.realizedPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED }}>
                  {position.realizedPnl >= 0 ? '+' : ''}{position.realizedPnl.toFixed(2)}
                </div>
                <div style={{ fontSize: '8px', color: position.realizedPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED, opacity: 0.7 }}>
                  ({pnlPercent >= 0 ? '+' : ''}{pnlPercent.toFixed(2)}%)
                </div>
              </div>

              {/* Closed Time */}
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>
                {position.closeTimestamp
                  ? new Date(position.closeTimestamp).toLocaleDateString([], { month: 'short', day: 'numeric' })
                  : '--'}
              </span>
            </div>
          );
        })}
      </div>

      {/* Empty State */}
      {!isLoading && filteredHistory.length === 0 && !error && (
        <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
          No closed positions found
        </div>
      )}

      {/* Loading */}
      {isLoading && history.length === 0 && (
        <div style={{ padding: '40px', textAlign: 'center' }}>
          <RefreshCw size={20} color={FINCEPT.GRAY} style={{ animation: 'spin 1s linear infinite' }} />
        </div>
      )}

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}
