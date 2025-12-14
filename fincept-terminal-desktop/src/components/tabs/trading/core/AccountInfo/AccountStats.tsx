/**
 * AccountStats - Trading statistics dashboard
 * Shows P&L, win rate, trade count, etc.
 */

import React from 'react';
import { TrendingUp, TrendingDown, Target, DollarSign } from 'lucide-react';
import { useTradingStats } from '../../hooks/useAccountInfo';

// Bloomberg color palette
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

export function AccountStats() {
  const { stats, isLoading } = useTradingStats();

  if (isLoading || !stats) {
    return (
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        padding: '40px',
        background: BLOOMBERG.PANEL_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        color: BLOOMBERG.GRAY,
        fontSize: '12px'
      }}>
        {isLoading ? (
          <div style={{
            width: '24px',
            height: '24px',
            border: `2px solid ${BLOOMBERG.BORDER}`,
            borderTopColor: BLOOMBERG.ORANGE,
            borderRadius: '50%',
            animation: 'spin 1s linear infinite'
          }} />
        ) : (
          'No statistics available'
        )}
      </div>
    );
  }

  const statCards = [
    {
      label: 'Total P&L',
      value: `${stats.totalPnL >= 0 ? '+' : ''}$${(typeof stats.totalPnL === 'number' ? stats.totalPnL : 0).toFixed(2)}`,
      color: stats.totalPnL >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
      icon: stats.totalPnL >= 0 ? TrendingUp : TrendingDown,
    },
    {
      label: 'Return',
      value: `${stats.returnPercent >= 0 ? '+' : ''}${(typeof stats.returnPercent === 'number' ? stats.returnPercent : 0).toFixed(2)}%`,
      color: stats.returnPercent >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
      icon: Target,
    },
    {
      label: 'Win Rate',
      value: `${(typeof stats.winRate === 'number' ? stats.winRate : 0).toFixed(1)}%`,
      color: stats.winRate >= 50 ? BLOOMBERG.GREEN : BLOOMBERG.YELLOW,
      icon: Target,
    },
    {
      label: 'Total Trades',
      value: (stats.totalTrades || 0).toString(),
      color: BLOOMBERG.BLUE,
      icon: DollarSign,
    },
    {
      label: 'Winning Trades',
      value: (stats.winningTrades || 0).toString(),
      color: BLOOMBERG.GREEN,
      icon: TrendingUp,
    },
    {
      label: 'Losing Trades',
      value: (stats.losingTrades || 0).toString(),
      color: BLOOMBERG.RED,
      icon: TrendingDown,
    },
    {
      label: 'Realized P&L',
      value: `${stats.realizedPnL >= 0 ? '+' : ''}$${(typeof stats.realizedPnL === 'number' ? stats.realizedPnL : 0).toFixed(2)}`,
      color: stats.realizedPnL >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
      icon: DollarSign,
    },
    {
      label: 'Unrealized P&L',
      value: `${stats.unrealizedPnL >= 0 ? '+' : ''}$${(typeof stats.unrealizedPnL === 'number' ? stats.unrealizedPnL : 0).toFixed(2)}`,
      color: stats.unrealizedPnL >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
      icon: DollarSign,
    },
  ];

  return (
    <div style={{
      height: '100%',
      overflow: 'auto',
      padding: '12px',
      background: BLOOMBERG.PANEL_BG
    }}>
      {/* Main Stats Grid */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(4, 1fr)',
        gap: '12px',
        marginBottom: '16px'
      }}>
        {statCards.map((stat, idx) => {
          const Icon = stat.icon;
          return (
            <div
              key={idx}
              style={{
                background: BLOOMBERG.DARK_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                borderRadius: '4px',
                padding: '12px',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => e.currentTarget.style.borderColor = BLOOMBERG.MUTED}
              onMouseLeave={(e) => e.currentTarget.style.borderColor = BLOOMBERG.BORDER}
            >
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                marginBottom: '8px'
              }}>
                <span style={{
                  fontSize: '9px',
                  color: BLOOMBERG.GRAY,
                  fontWeight: 600,
                  letterSpacing: '0.5px'
                }}>
                  {stat.label.toUpperCase()}
                </span>
                <Icon size={14} color={stat.color} />
              </div>
              <div style={{
                fontSize: '18px',
                fontWeight: 700,
                fontFamily: '"IBM Plex Mono", monospace',
                color: stat.color
              }}>
                {stat.value}
              </div>
            </div>
          );
        })}
      </div>

      {/* Advanced Stats */}
      {stats.averageWin !== undefined && (
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(3, 1fr)',
          gap: '12px',
          marginBottom: '16px'
        }}>
          {stats.averageWin !== undefined && typeof stats.averageWin === 'number' && (
            <div style={{
              background: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '4px',
              padding: '12px'
            }}>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>AVG WIN</div>
              <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: BLOOMBERG.GREEN, fontWeight: 600 }}>
                ${stats.averageWin.toFixed(2)}
              </div>
            </div>
          )}
          {stats.averageLoss !== undefined && typeof stats.averageLoss === 'number' && (
            <div style={{
              background: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '4px',
              padding: '12px'
            }}>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>AVG LOSS</div>
              <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: BLOOMBERG.RED, fontWeight: 600 }}>
                ${stats.averageLoss.toFixed(2)}
              </div>
            </div>
          )}
          {stats.profitFactor !== undefined && typeof stats.profitFactor === 'number' && (
            <div style={{
              background: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '4px',
              padding: '12px'
            }}>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>PROFIT FACTOR</div>
              <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: BLOOMBERG.BLUE, fontWeight: 600 }}>
                {stats.profitFactor.toFixed(2)}
              </div>
            </div>
          )}
          {stats.largestWin !== undefined && typeof stats.largestWin === 'number' && (
            <div style={{
              background: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '4px',
              padding: '12px'
            }}>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>LARGEST WIN</div>
              <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: BLOOMBERG.GREEN, fontWeight: 600 }}>
                ${stats.largestWin.toFixed(2)}
              </div>
            </div>
          )}
          {stats.largestLoss !== undefined && typeof stats.largestLoss === 'number' && (
            <div style={{
              background: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '4px',
              padding: '12px'
            }}>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>LARGEST LOSS</div>
              <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: BLOOMBERG.RED, fontWeight: 600 }}>
                ${stats.largestLoss.toFixed(2)}
              </div>
            </div>
          )}
          {stats.sharpeRatio !== undefined && typeof stats.sharpeRatio === 'number' && (
            <div style={{
              background: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '4px',
              padding: '12px'
            }}>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>SHARPE RATIO</div>
              <div style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: BLOOMBERG.PURPLE, fontWeight: 600 }}>
                {stats.sharpeRatio.toFixed(2)}
              </div>
            </div>
          )}
        </div>
      )}

      {/* Total Fees */}
      <div style={{
        background: BLOOMBERG.DARK_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        borderRadius: '4px',
        padding: '12px'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between'
        }}>
          <span style={{ fontSize: '9px', color: BLOOMBERG.GRAY, fontWeight: 600 }}>TOTAL FEES PAID</span>
          <span style={{ fontSize: '14px', fontFamily: '"IBM Plex Mono", monospace', color: BLOOMBERG.ORANGE, fontWeight: 600 }}>
            ${(typeof stats.totalFees === 'number' ? stats.totalFees : 0).toFixed(2)}
          </span>
        </div>
      </div>
    </div>
  );
}
