/**
 * VBMetricsCards - Horizontal row of metric cards
 * AI Quant Lab style: label on top, big value below, compact dark cards.
 */

import React from 'react';
import {
  FINCEPT, TYPOGRAPHY,
  getValueColor, formatPercentage, formatCurrency,
} from '../../../portfolio-tab/finceptStyles';
import type { BacktestingState } from '../../types';

interface VBMetricsCardsProps {
  state: BacktestingState;
}

interface MetricCardDef {
  key: string;
  label: string;
  sublabel: string;
  format: (v: number) => string;
  color: (v: number) => string;
}

const METRIC_CARDS: MetricCardDef[] = [
  {
    key: 'sharpeRatio',
    label: 'SHARPE',
    sublabel: 'Risk-Adjusted',
    format: v => v.toFixed(2),
    color: v => getValueColor(v),
  },
  {
    key: 'maxDrawdown',
    label: 'MAX DD',
    sublabel: 'Peak Decline',
    format: v => `${(Math.abs(v) * 100).toFixed(2)}%`,
    color: () => FINCEPT.RED,
  },
  {
    key: 'winRate',
    label: 'WIN RATE',
    sublabel: 'Trade Success',
    format: v => `${(v * 100).toFixed(1)}%`,
    color: v => v > 0.5 ? FINCEPT.GREEN : v > 0 ? FINCEPT.YELLOW : FINCEPT.RED,
  },
  {
    key: 'totalTrades',
    label: 'TRADES',
    sublabel: 'Executed',
    format: v => Math.round(v).toString(),
    color: () => FINCEPT.CYAN,
  },
  {
    key: 'profitFactor',
    label: 'P. FACTOR',
    sublabel: 'Profit / Loss',
    format: v => v.toFixed(2),
    color: v => getValueColor(v - 1),
  },
];

export const VBMetricsCards: React.FC<VBMetricsCardsProps> = ({ state }) => {
  const { result, isRunning, initialCapital } = state;
  const perf = result?.data?.performance;

  const totalReturn = perf?.totalReturn;
  const hasReturn = typeof totalReturn === 'number' && isFinite(totalReturn);
  const endValue = hasReturn ? initialCapital * (1 + totalReturn) : null;
  const returnColor = hasReturn ? getValueColor(totalReturn) : FINCEPT.MUTED;

  return (
    <div style={{
      flexShrink: 0,
      padding: '8px 10px',
      display: 'flex',
      gap: '6px',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      backgroundColor: FINCEPT.PANEL_BG,
      overflowX: 'auto',
    }}>
      {/* ── Return card (wider) ── */}
      <div style={{
        minWidth: '130px',
        flex: '0 0 auto',
        padding: '10px 12px',
        backgroundColor: FINCEPT.DARK_BG,
        borderRadius: '3px',
        borderLeft: `3px solid ${returnColor}`,
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'center',
        gap: '3px',
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
        }}>
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            fontFamily: TYPOGRAPHY.MONO,
          }}>RETURN</span>
          {hasReturn && (
            <span style={{
              fontSize: '8px',
              fontWeight: 700,
              padding: '1px 5px',
              backgroundColor: `${FINCEPT.GREEN}20`,
              color: FINCEPT.GREEN,
              borderRadius: '2px',
              fontFamily: TYPOGRAPHY.MONO,
            }}>DONE</span>
          )}
        </div>
        <div style={{
          fontSize: '18px',
          fontWeight: 800,
          color: isRunning ? FINCEPT.ORANGE : returnColor,
          fontFamily: TYPOGRAPHY.MONO,
          letterSpacing: '-0.5px',
          lineHeight: 1.1,
          animation: isRunning ? 'pulse 1s infinite' : 'none',
        }}>
          {isRunning ? '...' : hasReturn ? formatPercentage(totalReturn * 100) : '--'}
        </div>
        {hasReturn && endValue !== null && (
          <div style={{
            fontSize: '8px',
            color: FINCEPT.MUTED,
            fontFamily: TYPOGRAPHY.MONO,
          }}>
            {formatCurrency(initialCapital)} → {formatCurrency(endValue)}
          </div>
        )}
      </div>

      {/* ── Individual metric cards ── */}
      {METRIC_CARDS.map(card => {
        const rawValue = perf?.[card.key];
        const hasValue = typeof rawValue === 'number' && isFinite(rawValue);
        const valueColor = isRunning ? FINCEPT.ORANGE : hasValue ? card.color(rawValue) : FINCEPT.MUTED;

        return (
          <div
            key={card.key}
            style={{
              minWidth: '80px',
              flex: '1 1 0',
              padding: '10px 12px',
              backgroundColor: FINCEPT.DARK_BG,
              borderRadius: '3px',
              display: 'flex',
              flexDirection: 'column',
              justifyContent: 'center',
              gap: '3px',
            }}
          >
            {/* Label row */}
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}>
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px',
                fontFamily: TYPOGRAPHY.MONO,
              }}>{card.label}</span>
            </div>
            {/* Value */}
            <div style={{
              fontSize: '16px',
              fontWeight: 800,
              color: valueColor,
              fontFamily: TYPOGRAPHY.MONO,
              letterSpacing: '-0.5px',
              lineHeight: 1.1,
              animation: isRunning ? 'pulse 1s infinite' : 'none',
            }}>
              {isRunning ? '...' : hasValue ? card.format(rawValue) : '--'}
            </div>
            {/* Sublabel */}
            <div style={{
              fontSize: '8px',
              color: FINCEPT.MUTED,
              fontFamily: TYPOGRAPHY.MONO,
            }}>
              {card.sublabel}
            </div>
          </div>
        );
      })}
    </div>
  );
};
