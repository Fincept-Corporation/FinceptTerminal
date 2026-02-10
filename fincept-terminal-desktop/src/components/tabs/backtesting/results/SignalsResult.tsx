/**
 * SignalsResult - Entry/exit signal display for indicator_signals command
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from '../constants';
import { ResultRaw } from './ResultRaw';

interface SignalsResultProps {
  result: any;
}

export const SignalsResult: React.FC<SignalsResultProps> = ({ result }) => {
  if (!result?.data) return <ResultRaw result={result} />;

  const data = result.data;
  const entrySignals = data.entrySignals ?? data.entries ?? [];
  const exitSignals = data.exitSignals ?? data.exits ?? [];
  const hasMetrics = data.avgReturn !== undefined;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      <div style={{
        padding: '4px 8px',
        backgroundColor: `${FINCEPT.GREEN}20`,
        color: FINCEPT.GREEN,
        fontSize: '8px',
        fontWeight: 700,
        borderRadius: '2px',
        display: 'inline-block',
        alignSelf: 'flex-start',
        fontFamily: FONT_FAMILY,
      }}>
        SIGNALS GENERATED{data.symbol ? ` - ${data.symbol}` : ''}
      </div>

      {/* Performance Metrics */}
      {hasMetrics && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '12px', fontFamily: FONT_FAMILY }}>
            SIGNAL PERFORMANCE METRICS
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px' }}>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>WIN RATE</div>
              <div style={{ fontSize: '16px', fontWeight: 700, color: data.winRate >= 50 ? FINCEPT.GREEN : FINCEPT.RED, fontFamily: FONT_FAMILY }}>
                {data.winRate?.toFixed(1) ?? 'N/A'}%
              </div>
            </div>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>AVG RETURN</div>
              <div style={{ fontSize: '16px', fontWeight: 700, color: data.avgReturn >= 0 ? FINCEPT.GREEN : FINCEPT.RED, fontFamily: FONT_FAMILY }}>
                {data.avgReturn?.toFixed(2) ?? 'N/A'}%
              </div>
            </div>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>PROFIT FACTOR</div>
              <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.CYAN, fontFamily: FONT_FAMILY }}>
                {data.profitFactor !== null && data.profitFactor !== undefined
                  ? (data.profitFactor === Infinity ? '\u221E' : data.profitFactor.toFixed(2))
                  : 'N/A'}
              </div>
            </div>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>SIGNALS</div>
              <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.WHITE, fontFamily: FONT_FAMILY }}>
                {data.totalSignals ?? 'N/A'}
              </div>
            </div>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>BEST TRADE</div>
              <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.GREEN, fontFamily: FONT_FAMILY }}>
                +{data.bestTrade?.toFixed(2) ?? 'N/A'}%
              </div>
            </div>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>WORST TRADE</div>
              <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.RED, fontFamily: FONT_FAMILY }}>
                {data.worstTrade?.toFixed(2) ?? 'N/A'}%
              </div>
            </div>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>AVG HOLD (DAYS)</div>
              <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.WHITE, fontFamily: FONT_FAMILY }}>
                {data.avgHoldingPeriod?.toFixed(0) ?? 'N/A'}
              </div>
            </div>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>SIGNAL DENSITY</div>
              <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.YELLOW, fontFamily: FONT_FAMILY }}>
                {data.signalDensity?.toFixed(1) ?? 'N/A'}%
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Signal Counts */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        display: 'grid',
        gridTemplateColumns: '1fr 1fr 1fr',
        gap: '12px',
      }}>
        <div>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>ENTRY SIGNALS</div>
          <div style={{ fontSize: '20px', fontWeight: 700, color: FINCEPT.GREEN, fontFamily: FONT_FAMILY }}>
            {data.entryCount ?? 0}
          </div>
        </div>
        <div>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>EXIT SIGNALS</div>
          <div style={{ fontSize: '20px', fontWeight: 700, color: FINCEPT.RED, fontFamily: FONT_FAMILY }}>
            {data.exitCount ?? 0}
          </div>
        </div>
        <div>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: FONT_FAMILY }}>TOTAL BARS</div>
          <div style={{ fontSize: '20px', fontWeight: 700, color: FINCEPT.WHITE, fontFamily: FONT_FAMILY }}>
            {data.totalBars ?? 0}
          </div>
        </div>
      </div>

      {/* Entry Signals Detail */}
      {entrySignals.length > 0 && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '8px', fontWeight: 600, fontFamily: FONT_FAMILY }}>
            ENTRY SIGNALS (showing first 10)
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            {entrySignals.slice(0, 10).map((signal: any, idx: number) => (
              <div key={idx} style={{
                padding: '6px 8px',
                backgroundColor: `${FINCEPT.GREEN}15`,
                borderLeft: `2px solid ${FINCEPT.GREEN}`,
                borderRadius: '2px',
                fontSize: '8px',
                display: 'grid',
                gridTemplateColumns: typeof signal === 'object' ? '2fr 1fr 1fr 1fr' : '1fr',
                gap: '8px',
                fontFamily: FONT_FAMILY,
              }}>
                {typeof signal === 'object' ? (
                  <>
                    <div>
                      <span style={{ color: FINCEPT.GRAY }}>Date: </span>
                      <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{signal.date}</span>
                    </div>
                    <div>
                      <span style={{ color: FINCEPT.GRAY }}>Price: </span>
                      <span style={{ color: FINCEPT.CYAN }}>${signal.price?.toFixed(2)}</span>
                    </div>
                    {signal.fastMA !== undefined && (
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Fast: </span>
                        <span style={{ color: FINCEPT.YELLOW }}>{signal.fastMA?.toFixed(2)}</span>
                      </div>
                    )}
                    {signal.indicatorValue !== undefined && (
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Indicator: </span>
                        <span style={{ color: FINCEPT.PURPLE }}>{signal.indicatorValue?.toFixed(2)}</span>
                      </div>
                    )}
                  </>
                ) : (
                  <span style={{ color: FINCEPT.WHITE }}>{signal}</span>
                )}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Exit Signals Detail */}
      {exitSignals.length > 0 && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '8px', fontWeight: 600, fontFamily: FONT_FAMILY }}>
            EXIT SIGNALS (showing first 10)
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            {exitSignals.slice(0, 10).map((signal: any, idx: number) => (
              <div key={idx} style={{
                padding: '6px 8px',
                backgroundColor: `${FINCEPT.RED}15`,
                borderLeft: `2px solid ${FINCEPT.RED}`,
                borderRadius: '2px',
                fontSize: '8px',
                display: 'grid',
                gridTemplateColumns: typeof signal === 'object' ? '2fr 1fr 1fr 1fr' : '1fr',
                gap: '8px',
                fontFamily: FONT_FAMILY,
              }}>
                {typeof signal === 'object' ? (
                  <>
                    <div>
                      <span style={{ color: FINCEPT.GRAY }}>Date: </span>
                      <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{signal.date}</span>
                    </div>
                    <div>
                      <span style={{ color: FINCEPT.GRAY }}>Price: </span>
                      <span style={{ color: FINCEPT.CYAN }}>${signal.price?.toFixed(2)}</span>
                    </div>
                    {signal.slowMA !== undefined && (
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Slow: </span>
                        <span style={{ color: FINCEPT.ORANGE }}>{signal.slowMA?.toFixed(2)}</span>
                      </div>
                    )}
                    {signal.indicatorValue !== undefined && (
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Indicator: </span>
                        <span style={{ color: FINCEPT.PURPLE }}>{signal.indicatorValue?.toFixed(2)}</span>
                      </div>
                    )}
                  </>
                ) : (
                  <span style={{ color: FINCEPT.WHITE }}>{signal}</span>
                )}
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};
