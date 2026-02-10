/**
 * ResultTrades - Trade list display for backtest results
 */

import React from 'react';
import { TrendingUp } from 'lucide-react';
import { FINCEPT, FONT_FAMILY } from '../constants';

interface ResultTradesProps {
  result: any;
}

export const ResultTrades: React.FC<ResultTradesProps> = ({ result }) => {
  if (!result?.data?.trades) return null;
  const trades = result.data.trades;

  if (trades.length === 0) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        padding: '40px 20px',
        color: FINCEPT.MUTED,
        fontSize: '9px',
        textAlign: 'center',
        fontFamily: FONT_FAMILY,
      }}>
        <TrendingUp size={24} style={{ opacity: 0.3, marginBottom: '8px' }} />
        <div>No trades executed</div>
        <div style={{ fontSize: '8px', marginTop: '4px' }}>
          Strategy generated no entry/exit signals
        </div>
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
      <div style={{
        fontSize: '8px',
        color: FINCEPT.GRAY,
        padding: '4px 0',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        fontFamily: FONT_FAMILY,
      }}>
        SHOWING {Math.min(20, trades.length)} OF {trades.length} TRADES
      </div>
      {trades.slice(0, 20).map((trade: any, i: number) => {
        const isWin = (trade.pnl ?? 0) > 0;
        const pnlPercent = trade.pnlPercent ?? ((trade.exitPrice && trade.entryPrice)
          ? ((trade.exitPrice - trade.entryPrice) / trade.entryPrice * 100)
          : 0);

        return (
          <div key={i} style={{
            padding: '8px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${isWin ? FINCEPT.GREEN : FINCEPT.RED}40`,
            borderLeft: `3px solid ${isWin ? FINCEPT.GREEN : FINCEPT.RED}`,
            borderRadius: '2px',
          }}>
            {/* Header: Trade ID and P&L */}
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', alignItems: 'center' }}>
              <span style={{
                color: FINCEPT.GRAY,
                fontSize: '7px',
                fontWeight: 600,
                fontFamily: FONT_FAMILY,
              }}>
                TRADE #{i + 1}
              </span>
              <div style={{ textAlign: 'right' }}>
                <div style={{
                  color: isWin ? FINCEPT.GREEN : FINCEPT.RED,
                  fontSize: '11px',
                  fontWeight: 700,
                  fontFamily: FONT_FAMILY,
                }}>
                  {isWin ? '+' : ''}{trade.pnl ? `$${trade.pnl.toFixed(2)}` : 'N/A'}
                </div>
                <div style={{
                  color: isWin ? FINCEPT.GREEN : FINCEPT.RED,
                  fontSize: '7px',
                  fontFamily: FONT_FAMILY,
                }}>
                  {isWin ? '+' : ''}{pnlPercent.toFixed(2)}%
                </div>
              </div>
            </div>

            {/* Trade Details */}
            <div style={{ fontSize: '7px', color: FINCEPT.MUTED, display: 'flex', flexDirection: 'column', gap: '3px', fontFamily: FONT_FAMILY }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span>Symbol</span>
                <span style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>{trade.symbol}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span>Side</span>
                <span style={{
                  color: trade.side === 'long' ? FINCEPT.GREEN : FINCEPT.RED,
                  fontWeight: 600,
                  textTransform: 'uppercase',
                }}>
                  {trade.side}
                </span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span>Entry</span>
                <span style={{ color: FINCEPT.WHITE, fontFamily: FONT_FAMILY }}>
                  ${trade.entryPrice?.toFixed(2) ?? 'N/A'}
                </span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span>Exit</span>
                <span style={{ color: FINCEPT.WHITE, fontFamily: FONT_FAMILY }}>
                  ${trade.exitPrice?.toFixed(2) ?? 'N/A'}
                </span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span>Quantity</span>
                <span style={{ color: FINCEPT.CYAN }}>{trade.quantity?.toFixed(4) ?? 'N/A'}</span>
              </div>
              {trade.holdingPeriod && (
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span>Duration</span>
                  <span style={{ color: FINCEPT.YELLOW }}>{trade.holdingPeriod} bars</span>
                </div>
              )}
              {trade.exitReason && (
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span>Exit Reason</span>
                  <span style={{ color: FINCEPT.ORANGE, textTransform: 'uppercase' }}>{trade.exitReason}</span>
                </div>
              )}
            </div>
          </div>
        );
      })}
    </div>
  );
};
