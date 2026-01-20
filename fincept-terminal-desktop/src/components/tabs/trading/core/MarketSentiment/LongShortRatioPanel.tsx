/**
 * Long/Short Ratio Panel
 * Displays market sentiment based on long/short ratio data
 */

import React from 'react';
import { TrendingUp, TrendingDown, BarChart2, RefreshCw, AlertCircle, Info } from 'lucide-react';
import { useLongShortRatioHistory } from '../../hooks/useAdvancedMarketData';
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

interface LongShortRatioPanelProps {
  symbol?: string;
  timeframe?: string;
  limit?: number;
}

export function LongShortRatioPanel({ symbol, timeframe = '1h', limit = 24 }: LongShortRatioPanelProps) {
  const { tradingMode, activeBroker } = useBrokerContext();
  const { history, isLoading, error, isSupported, refresh } = useLongShortRatioHistory(symbol, timeframe, undefined, limit);

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <AlertCircle size={32} color={FINCEPT.YELLOW} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px' }}>Market sentiment not available in paper trading</div>
      </div>
    );
  }

  if (!isSupported) {
    return (
      <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
        <Info size={32} color={FINCEPT.GRAY} style={{ marginBottom: '12px' }} />
        <div style={{ fontSize: '12px' }}>Long/Short ratio not supported by {activeBroker}</div>
      </div>
    );
  }

  // Latest ratio
  const latest = history.length > 0 ? history[0] : null;
  const prevRatio = history.length > 1 ? history[1].longShortRatio : null;
  const ratioChange = latest && prevRatio ? ((latest.longShortRatio - prevRatio) / prevRatio) * 100 : 0;

  // Calculate percentages
  const longPercent = latest ? (latest.longShortRatio / (latest.longShortRatio + 1)) * 100 : 50;
  const shortPercent = 100 - longPercent;

  // Determine sentiment
  const sentiment = latest
    ? latest.longShortRatio > 1.5
      ? 'Extremely Bullish'
      : latest.longShortRatio > 1.1
      ? 'Bullish'
      : latest.longShortRatio > 0.9
      ? 'Neutral'
      : latest.longShortRatio > 0.67
      ? 'Bearish'
      : 'Extremely Bearish'
    : 'Unknown';

  const sentimentColor =
    sentiment === 'Extremely Bullish' || sentiment === 'Bullish'
      ? FINCEPT.GREEN
      : sentiment === 'Neutral'
      ? FINCEPT.YELLOW
      : FINCEPT.RED;

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '16px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px', paddingBottom: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <BarChart2 size={14} color={FINCEPT.CYAN} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>Long/Short Ratio</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {symbol && (
            <span style={{ fontSize: '10px', color: FINCEPT.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>{symbol}</span>
          )}
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

      {/* Error */}
      {error && (
        <div style={{ padding: '10px', backgroundColor: `${FINCEPT.RED}20`, borderRadius: '4px', marginBottom: '12px', fontSize: '11px', color: FINCEPT.RED }}>
          {error.message}
        </div>
      )}

      {/* Main Ratio Display */}
      {latest && (
        <div style={{ marginBottom: '20px' }}>
          {/* Big Ratio Number */}
          <div style={{ textAlign: 'center', marginBottom: '16px' }}>
            <div style={{ fontSize: '32px', fontWeight: 700, color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace' }}>
              {latest.longShortRatio.toFixed(3)}
            </div>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', marginTop: '4px' }}>
              {ratioChange > 0 ? (
                <TrendingUp size={12} color={FINCEPT.GREEN} />
              ) : ratioChange < 0 ? (
                <TrendingDown size={12} color={FINCEPT.RED} />
              ) : null}
              <span style={{ fontSize: '11px', color: ratioChange > 0 ? FINCEPT.GREEN : ratioChange < 0 ? FINCEPT.RED : FINCEPT.GRAY }}>
                {ratioChange > 0 ? '+' : ''}{ratioChange.toFixed(2)}%
              </span>
            </div>
          </div>

          {/* Sentiment Badge */}
          <div style={{ textAlign: 'center', marginBottom: '16px' }}>
            <span
              style={{
                display: 'inline-block',
                padding: '6px 16px',
                backgroundColor: `${sentimentColor}20`,
                border: `1px solid ${sentimentColor}`,
                borderRadius: '20px',
                fontSize: '11px',
                fontWeight: 600,
                color: sentimentColor,
              }}
            >
              {sentiment}
            </span>
          </div>

          {/* Long/Short Bar */}
          <div style={{ marginBottom: '12px' }}>
            <div style={{ display: 'flex', marginBottom: '6px' }}>
              <div style={{ flex: longPercent, textAlign: 'left' }}>
                <span style={{ fontSize: '10px', color: FINCEPT.GREEN, fontWeight: 600 }}>LONG {longPercent.toFixed(1)}%</span>
              </div>
              <div style={{ flex: shortPercent, textAlign: 'right' }}>
                <span style={{ fontSize: '10px', color: FINCEPT.RED, fontWeight: 600 }}>{shortPercent.toFixed(1)}% SHORT</span>
              </div>
            </div>
            <div style={{ display: 'flex', height: '8px', borderRadius: '4px', overflow: 'hidden' }}>
              <div style={{ width: `${longPercent}%`, backgroundColor: FINCEPT.GREEN }} />
              <div style={{ width: `${shortPercent}%`, backgroundColor: FINCEPT.RED }} />
            </div>
          </div>

          {/* Account Stats */}
          {(latest.longAccount !== undefined || latest.shortAccount !== undefined) && (
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              {latest.longAccount !== undefined && (
                <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', textAlign: 'center' }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>LONG ACCOUNTS</div>
                  <div style={{ fontSize: '14px', color: FINCEPT.GREEN, fontFamily: '"IBM Plex Mono", monospace' }}>
                    {(latest.longAccount * 100).toFixed(2)}%
                  </div>
                </div>
              )}
              {latest.shortAccount !== undefined && (
                <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', textAlign: 'center' }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>SHORT ACCOUNTS</div>
                  <div style={{ fontSize: '14px', color: FINCEPT.RED, fontFamily: '"IBM Plex Mono", monospace' }}>
                    {(latest.shortAccount * 100).toFixed(2)}%
                  </div>
                </div>
              )}
            </div>
          )}
        </div>
      )}

      {/* Historical Data */}
      {history.length > 1 && (
        <div>
          <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>HISTORICAL ({timeframe})</div>
          <div style={{ maxHeight: '150px', overflowY: 'auto' }}>
            {history.slice(0, 12).map((item, index) => (
              <div
                key={index}
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  padding: '6px 0',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                }}
              >
                <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                  {new Date(item.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
                </span>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span
                    style={{
                      fontSize: '11px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      color: item.longShortRatio > 1 ? FINCEPT.GREEN : item.longShortRatio < 1 ? FINCEPT.RED : FINCEPT.WHITE,
                    }}
                  >
                    {item.longShortRatio.toFixed(3)}
                  </span>
                  {/* Mini bar */}
                  <div style={{ width: '40px', height: '4px', display: 'flex', borderRadius: '2px', overflow: 'hidden' }}>
                    <div style={{ width: `${(item.longShortRatio / (item.longShortRatio + 1)) * 100}%`, backgroundColor: FINCEPT.GREEN }} />
                    <div style={{ flex: 1, backgroundColor: FINCEPT.RED }} />
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* No Data */}
      {!isLoading && history.length === 0 && !error && (
        <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
          No long/short ratio data available
        </div>
      )}

      {/* Loading */}
      {isLoading && history.length === 0 && (
        <div style={{ padding: '20px', textAlign: 'center' }}>
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
