/**
 * VBMetricsCards - Horizontal row of large metric cards
 * Replaces both VBStatsRibbon and VBMetricsPanel top section.
 * 6 key metrics displayed as prominent cards with big numbers.
 */

import React from 'react';
import {
  FINCEPT, TYPOGRAPHY, BORDERS,
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
    key: 'totalReturn',
    label: 'RETURN',
    sublabel: 'Total P&L',
    format: v => formatPercentage(v * 100),
    color: v => getValueColor(v),
  },
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
      {/* Summary card - wider, shows capital flow */}
      <div style={{
        minWidth: '120px',
        flex: '0 0 auto',
        padding: '10px 14px',
        backgroundColor: FINCEPT.HEADER_BG,
        border: BORDERS.STANDARD,
        borderRadius: '3px',
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'center',
        borderLeft: `3px solid ${hasReturn ? getValueColor(totalReturn) : FINCEPT.MUTED}`,
      }}>
        <div style={{
          fontSize: '20px',
          fontWeight: 800,
          color: isRunning
            ? FINCEPT.ORANGE
            : hasReturn
              ? getValueColor(totalReturn)
              : FINCEPT.MUTED,
          fontFamily: TYPOGRAPHY.MONO,
          letterSpacing: '-1px',
          lineHeight: 1.1,
          animation: isRunning ? 'pulse 1s infinite' : 'none',
        }}>
          {isRunning ? '...' : hasReturn ? formatPercentage(totalReturn * 100) : '--'}
        </div>
        <div style={{
          fontSize: '9px',
          color: FINCEPT.GRAY,
          marginTop: '4px',
          fontFamily: TYPOGRAPHY.MONO,
        }}>
          {hasReturn && endValue !== null
            ? `${formatCurrency(initialCapital)} \u2192 ${formatCurrency(endValue)}`
            : 'TOTAL RETURN'
          }
        </div>
        {hasReturn && (
          <div style={{
            display: 'inline-block',
            marginTop: '4px',
            padding: '1px 6px',
            backgroundColor: `${FINCEPT.GREEN}20`,
            color: FINCEPT.GREEN,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
            width: 'fit-content',
          }}>
            DONE
          </div>
        )}
      </div>

      {/* Individual metric cards */}
      {METRIC_CARDS.slice(1).map(card => {
        const rawValue = perf?.[card.key];
        const hasValue = typeof rawValue === 'number' && isFinite(rawValue);

        return (
          <div
            key={card.key}
            style={{
              minWidth: '90px',
              flex: '1 1 0',
              padding: '10px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: BORDERS.STANDARD,
              borderRadius: '3px',
              display: 'flex',
              flexDirection: 'column',
              justifyContent: 'center',
            }}
          >
            <div style={{
              fontSize: '18px',
              fontWeight: 800,
              color: isRunning
                ? FINCEPT.ORANGE
                : hasValue
                  ? card.color(rawValue)
                  : FINCEPT.MUTED,
              fontFamily: TYPOGRAPHY.MONO,
              letterSpacing: '-0.5px',
              lineHeight: 1.1,
              animation: isRunning ? 'pulse 1s infinite' : 'none',
            }}>
              {isRunning ? '...' : hasValue ? card.format(rawValue) : '--'}
            </div>
            <div style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginTop: '4px',
              letterSpacing: '0.5px',
            }}>
              {card.label}
            </div>
            <div style={{
              fontSize: '8px',
              color: FINCEPT.MUTED,
              marginTop: '1px',
            }}>
              {card.sublabel}
            </div>
          </div>
        );
      })}
    </div>
  );
};
