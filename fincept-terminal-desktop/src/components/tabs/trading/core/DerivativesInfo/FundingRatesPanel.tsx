/**
 * Funding Rates Panel
 * Displays funding rates for perpetual contracts
 */

import React, { useState } from 'react';
import { TrendingUp, TrendingDown, RefreshCw, Clock, AlertCircle } from 'lucide-react';
import { useFundingRate, useFundingRates } from '../../hooks/useDerivatives';
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

interface FundingRatesPanelProps {
  symbol?: string;
  showAll?: boolean;
}

export function FundingRatesPanel({ symbol, showAll = false }: FundingRatesPanelProps) {
  const { tradingMode } = useBrokerContext();
  const { fundingRate, isLoading: singleLoading, isSupported: singleSupported, refresh: refreshSingle } = useFundingRate(symbol);
  const { fundingRates, isLoading: allLoading, isSupported: allSupported, refresh: refreshAll } = useFundingRates();

  const [viewMode, setViewMode] = useState<'single' | 'all'>(showAll ? 'all' : 'single');

  const formatRate = (rate: number) => {
    const percent = (rate * 100).toFixed(4);
    return `${rate >= 0 ? '+' : ''}${percent}%`;
  };

  const formatAnnualized = (rate: number) => {
    // 3 funding periods per day * 365 days
    const annualized = rate * 3 * 365 * 100;
    return `${annualized >= 0 ? '+' : ''}${annualized.toFixed(2)}% APR`;
  };

  const getRateColor = (rate: number) => {
    if (rate > 0.001) return FINCEPT.GREEN; // Longs pay shorts
    if (rate < -0.001) return FINCEPT.RED; // Shorts pay longs
    return FINCEPT.GRAY; // Neutral
  };

  const getNextFundingTime = (timestamp?: number) => {
    if (!timestamp) return '--';
    const now = Date.now();
    const diff = timestamp - now;
    if (diff <= 0) return 'Now';
    const hours = Math.floor(diff / 3600000);
    const minutes = Math.floor((diff % 3600000) / 60000);
    return `${hours}h ${minutes}m`;
  };

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        <AlertCircle size={20} color={FINCEPT.YELLOW} style={{ marginBottom: '8px' }} />
        <div>Funding rates not available in paper trading mode</div>
      </div>
    );
  }

  if (!singleSupported && !allSupported) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        Funding rates not supported by this exchange
      </div>
    );
  }

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '12px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <TrendingUp size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>FUNDING RATES</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {symbol && (
            <div style={{ display: 'flex', gap: '4px' }}>
              <button
                onClick={() => setViewMode('single')}
                style={{
                  padding: '4px 8px',
                  fontSize: '9px',
                  backgroundColor: viewMode === 'single' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  color: viewMode === 'single' ? FINCEPT.WHITE : FINCEPT.GRAY,
                  cursor: 'pointer',
                }}
              >
                {symbol}
              </button>
              <button
                onClick={() => setViewMode('all')}
                style={{
                  padding: '4px 8px',
                  fontSize: '9px',
                  backgroundColor: viewMode === 'all' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  color: viewMode === 'all' ? FINCEPT.WHITE : FINCEPT.GRAY,
                  cursor: 'pointer',
                }}
              >
                ALL
              </button>
            </div>
          )}
          <button
            onClick={viewMode === 'single' ? refreshSingle : refreshAll}
            disabled={singleLoading || allLoading}
            style={{ background: 'transparent', border: 'none', cursor: 'pointer', padding: '4px' }}
          >
            <RefreshCw size={12} color={FINCEPT.GRAY} style={{ animation: (singleLoading || allLoading) ? 'spin 1s linear infinite' : 'none' }} />
          </button>
        </div>
      </div>

      {/* Single Symbol View */}
      {viewMode === 'single' && fundingRate && (
        <div>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '12px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', marginBottom: '8px' }}>
            <div>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>CURRENT RATE</div>
              <div style={{ fontSize: '20px', fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(fundingRate.fundingRate) }}>
                {formatRate(fundingRate.fundingRate)}
              </div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                {formatAnnualized(fundingRate.fundingRate)}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginBottom: '4px' }}>
                <Clock size={10} color={FINCEPT.CYAN} />
                <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>NEXT IN</span>
              </div>
              <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>
                {getNextFundingTime(fundingRate.nextFundingTimestamp)}
              </div>
            </div>
          </div>

          {fundingRate.nextFundingRate !== undefined && (
            <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px' }}>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>PREDICTED NEXT</div>
              <div style={{ fontSize: '14px', fontWeight: 600, fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(fundingRate.nextFundingRate) }}>
                {formatRate(fundingRate.nextFundingRate)}
              </div>
            </div>
          )}

          {/* Rate Interpretation */}
          <div style={{ marginTop: '12px', padding: '10px', backgroundColor: `${getRateColor(fundingRate.fundingRate)}15`, borderRadius: '4px', fontSize: '10px' }}>
            {fundingRate.fundingRate > 0 ? (
              <span style={{ color: FINCEPT.GREEN }}>
                <TrendingUp size={12} style={{ marginRight: '4px', verticalAlign: 'middle' }} />
                Longs pay shorts - Bullish sentiment
              </span>
            ) : fundingRate.fundingRate < 0 ? (
              <span style={{ color: FINCEPT.RED }}>
                <TrendingDown size={12} style={{ marginRight: '4px', verticalAlign: 'middle' }} />
                Shorts pay longs - Bearish sentiment
              </span>
            ) : (
              <span style={{ color: FINCEPT.GRAY }}>Neutral funding rate</span>
            )}
          </div>
        </div>
      )}

      {/* All Symbols View */}
      {viewMode === 'all' && (
        <div style={{ maxHeight: '300px', overflowY: 'auto' }}>
          {fundingRates.length === 0 ? (
            <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '10px' }}>
              {allLoading ? 'Loading...' : 'No funding rates available'}
            </div>
          ) : (
            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
              <thead>
                <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                  <th style={{ padding: '8px', textAlign: 'left', color: FINCEPT.GRAY }}>SYMBOL</th>
                  <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY }}>RATE</th>
                  <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY }}>APR</th>
                </tr>
              </thead>
              <tbody>
                {fundingRates.slice(0, 20).map((rate, idx) => (
                  <tr key={rate.symbol} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}`, backgroundColor: idx % 2 === 0 ? FINCEPT.DARK_BG : 'transparent' }}>
                    <td style={{ padding: '8px', color: FINCEPT.WHITE, fontWeight: 600 }}>{rate.symbol}</td>
                    <td style={{ padding: '8px', textAlign: 'right', fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(rate.fundingRate) }}>
                      {formatRate(rate.fundingRate)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(rate.fundingRate) }}>
                      {formatAnnualized(rate.fundingRate)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
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
