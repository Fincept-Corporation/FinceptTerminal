/**
 * ResultSummary - Performance card + key metrics for backtest results
 */

import React from 'react';
import { CheckCircle, TrendingUp, AlertCircle } from 'lucide-react';
import { FINCEPT, FONT_FAMILY } from '../constants';

interface ResultSummaryProps {
  result: any;
}

export const ResultSummary: React.FC<ResultSummaryProps> = ({ result }) => {
  if (!result?.data?.performance) return null;
  const perf = result.data.performance;
  const stats = result.data.statistics;

  const totalReturn = (perf.totalReturn ?? 0) * 100;
  const isProfit = totalReturn > 0;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Status Badge */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        padding: '4px 8px',
        backgroundColor: `${FINCEPT.GREEN}20`,
        borderRadius: '2px',
        alignSelf: 'flex-start',
      }}>
        <CheckCircle size={12} color={FINCEPT.GREEN} />
        <span style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.GREEN,
          letterSpacing: '0.5px',
          fontFamily: FONT_FAMILY,
        }}>
          BACKTEST COMPLETED
        </span>
      </div>

      {/* Main Performance Card */}
      <div style={{
        padding: '16px',
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${isProfit ? FINCEPT.GREEN : FINCEPT.RED}40`,
        borderLeft: `3px solid ${isProfit ? FINCEPT.GREEN : FINCEPT.RED}`,
        borderRadius: '2px',
      }}>
        <div style={{
          fontSize: '9px',
          color: FINCEPT.GRAY,
          marginBottom: '6px',
          fontWeight: 700,
          letterSpacing: '1px',
          fontFamily: FONT_FAMILY,
        }}>
          TOTAL RETURN
        </div>
        <div style={{
          fontSize: '28px',
          color: isProfit ? FINCEPT.GREEN : FINCEPT.RED,
          fontWeight: 800,
          fontFamily: FONT_FAMILY,
          letterSpacing: '-1px',
        }}>
          {totalReturn >= 0 ? '+' : ''}{totalReturn.toFixed(2)}%
        </div>
        <div style={{
          fontSize: '10px',
          color: FINCEPT.MUTED,
          marginTop: '8px',
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          fontFamily: FONT_FAMILY,
        }}>
          <span style={{ color: FINCEPT.GRAY }}>
            ${(stats?.initialCapital ?? 0).toLocaleString()}
          </span>
          <TrendingUp size={12} color={isProfit ? FINCEPT.GREEN : FINCEPT.RED} />
          <span style={{ color: isProfit ? FINCEPT.GREEN : FINCEPT.RED, fontWeight: 600 }}>
            ${(stats?.finalCapital ?? 0).toLocaleString()}
          </span>
        </div>
      </div>

      {/* Key Metrics Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
        {/* Sharpe Ratio */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderTop: `2px solid ${(perf.sharpeRatio ?? 0) > 1 ? FINCEPT.GREEN : FINCEPT.CYAN}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 600, fontFamily: FONT_FAMILY }}>SHARPE RATIO</div>
          <div style={{
            fontSize: '18px',
            color: (perf.sharpeRatio ?? 0) > 1 ? FINCEPT.GREEN : FINCEPT.CYAN,
            fontWeight: 800,
            fontFamily: FONT_FAMILY,
          }}>
            {(perf.sharpeRatio ?? 0).toFixed(2)}
          </div>
        </div>

        {/* Max Drawdown */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderTop: `2px solid ${FINCEPT.RED}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 600, fontFamily: FONT_FAMILY }}>MAX DRAWDOWN</div>
          <div style={{
            fontSize: '18px',
            color: FINCEPT.RED,
            fontWeight: 800,
            fontFamily: FONT_FAMILY,
          }}>
            {((perf.maxDrawdown ?? 0) * 100).toFixed(2)}%
          </div>
        </div>

        {/* Win Rate */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderTop: `2px solid ${(perf.winRate ?? 0) > 0.5 ? FINCEPT.GREEN : FINCEPT.YELLOW}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 600, fontFamily: FONT_FAMILY }}>WIN RATE</div>
          <div style={{
            fontSize: '18px',
            color: (perf.winRate ?? 0) > 0.5 ? FINCEPT.GREEN : FINCEPT.YELLOW,
            fontWeight: 800,
            fontFamily: FONT_FAMILY,
          }}>
            {((perf.winRate ?? 0) * 100).toFixed(1)}%
          </div>
        </div>

        {/* Total Trades */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderTop: `2px solid ${FINCEPT.CYAN}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 600, fontFamily: FONT_FAMILY }}>TOTAL TRADES</div>
          <div style={{
            fontSize: '18px',
            color: FINCEPT.CYAN,
            fontWeight: 800,
            fontFamily: FONT_FAMILY,
          }}>
            {perf.totalTrades ?? 0}
          </div>
        </div>
      </div>

      {/* Trade Breakdown */}
      {(perf.totalTrades ?? 0) > 0 && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '10px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: FONT_FAMILY }}>
            TRADE BREAKDOWN
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontFamily: FONT_FAMILY }}>Winning</span>
              <span style={{
                fontSize: '11px',
                color: FINCEPT.GREEN,
                fontWeight: 700,
                backgroundColor: `${FINCEPT.GREEN}15`,
                padding: '2px 8px',
                borderRadius: '2px',
                fontFamily: FONT_FAMILY,
              }}>
                {perf.winningTrades ?? 0}
              </span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontFamily: FONT_FAMILY }}>Losing</span>
              <span style={{
                fontSize: '11px',
                color: FINCEPT.RED,
                fontWeight: 700,
                backgroundColor: `${FINCEPT.RED}15`,
                padding: '2px 8px',
                borderRadius: '2px',
                fontFamily: FONT_FAMILY,
              }}>
                {perf.losingTrades ?? 0}
              </span>
            </div>
            <div style={{
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              paddingTop: '8px',
              marginTop: '4px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
            }}>
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontFamily: FONT_FAMILY }}>Profit Factor</span>
              <span style={{
                fontSize: '12px',
                color: (perf.profitFactor ?? 0) > 1 ? FINCEPT.GREEN : FINCEPT.RED,
                fontWeight: 800,
                fontFamily: FONT_FAMILY,
              }}>
                {(perf.profitFactor ?? 0).toFixed(2)}
              </span>
            </div>
          </div>
        </div>
      )}

      {/* Warning for zero trades */}
      {(perf.totalTrades ?? 0) === 0 && (
        <div style={{
          padding: '12px',
          backgroundColor: `${FINCEPT.YELLOW}15`,
          border: `1px solid ${FINCEPT.YELLOW}40`,
          borderRadius: '2px',
          display: 'flex',
          alignItems: 'flex-start',
          gap: '10px',
        }}>
          <AlertCircle size={14} color={FINCEPT.YELLOW} style={{ marginTop: '1px' }} />
          <div style={{ fontSize: '9px', color: FINCEPT.YELLOW, lineHeight: 1.4, fontFamily: FONT_FAMILY }}>
            No trades executed. Strategy generated no signals or all entries were skipped.
          </div>
        </div>
      )}
    </div>
  );
};
