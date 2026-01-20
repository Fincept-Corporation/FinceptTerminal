/**
 * Borrow Rates Panel
 * Displays borrow rates for margin trading
 */

import React, { useState } from 'react';
import { Percent, RefreshCw, AlertCircle, Search } from 'lucide-react';
import { useBorrowRate, useBorrowRates } from '../../hooks/useDerivatives';
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
  BLUE: '#0088FF',
};

interface BorrowRatesPanelProps {
  currency?: string;
}

export function BorrowRatesPanel({ currency }: BorrowRatesPanelProps) {
  const { tradingMode } = useBrokerContext();
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedCurrency, setSelectedCurrency] = useState(currency || '');

  const { borrowRate, isLoading: singleLoading, isSupported: singleSupported, refresh: refreshSingle } = useBorrowRate(selectedCurrency || undefined);
  const { borrowRates, isLoading: allLoading, isSupported: allSupported, refresh: refreshAll } = useBorrowRates();

  const formatRate = (rate: number) => {
    return `${(rate * 100).toFixed(4)}%`;
  };

  const formatDailyRate = (rate: number) => {
    // Assuming hourly rate, convert to daily
    const daily = rate * 24 * 100;
    return `${daily.toFixed(4)}%/day`;
  };

  const formatAnnualRate = (rate: number) => {
    // Assuming hourly rate, convert to annual
    const annual = rate * 24 * 365 * 100;
    return `${annual.toFixed(2)}% APY`;
  };

  const getRateColor = (rate: number) => {
    if (rate < 0.0001) return FINCEPT.GREEN; // Low rate
    if (rate < 0.001) return FINCEPT.YELLOW; // Medium rate
    return FINCEPT.RED; // High rate
  };

  // Filter rates by search term
  const filteredRates = borrowRates.filter((r) =>
    r.currency.toLowerCase().includes(searchTerm.toLowerCase())
  );

  if (tradingMode === 'paper') {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        <AlertCircle size={20} color={FINCEPT.YELLOW} style={{ marginBottom: '8px' }} />
        <div>Borrow rates not available in paper trading mode</div>
      </div>
    );
  }

  if (!singleSupported && !allSupported) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        Borrow rates not supported by this exchange
      </div>
    );
  }

  return (
    <div style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '12px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Percent size={14} color={FINCEPT.BLUE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>BORROW RATES</span>
        </div>
        <button
          onClick={refreshAll}
          disabled={allLoading}
          style={{ background: 'transparent', border: 'none', cursor: 'pointer', padding: '4px' }}
        >
          <RefreshCw size={12} color={FINCEPT.GRAY} style={{ animation: allLoading ? 'spin 1s linear infinite' : 'none' }} />
        </button>
      </div>

      {/* Search */}
      <div style={{ position: 'relative', marginBottom: '12px' }}>
        <Search size={12} color={FINCEPT.GRAY} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)' }} />
        <input
          type="text"
          value={searchTerm}
          onChange={(e) => setSearchTerm(e.target.value)}
          placeholder="Search currency..."
          style={{
            width: '100%',
            padding: '8px 10px 8px 30px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            color: FINCEPT.WHITE,
            fontSize: '10px',
          }}
        />
      </div>

      {/* Selected Currency Detail */}
      {selectedCurrency && borrowRate && (
        <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '4px', marginBottom: '12px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
            <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>{selectedCurrency}</span>
            <button
              onClick={() => setSelectedCurrency('')}
              style={{ background: 'transparent', border: 'none', color: FINCEPT.GRAY, cursor: 'pointer', fontSize: '10px' }}
            >
              Clear
            </button>
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
            <div>
              <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>HOURLY</div>
              <div style={{ fontSize: '14px', fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(borrowRate.rate) }}>
                {formatRate(borrowRate.rate)}
              </div>
            </div>
            <div>
              <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>DAILY</div>
              <div style={{ fontSize: '14px', fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(borrowRate.rate) }}>
                {formatDailyRate(borrowRate.rate)}
              </div>
            </div>
            <div>
              <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>ANNUAL</div>
              <div style={{ fontSize: '14px', fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(borrowRate.rate) }}>
                {formatAnnualRate(borrowRate.rate)}
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Rates Table */}
      <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
        {allLoading && borrowRates.length === 0 ? (
          <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '10px' }}>
            Loading borrow rates...
          </div>
        ) : filteredRates.length === 0 ? (
          <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '10px' }}>
            {searchTerm ? 'No matching currencies' : 'No borrow rates available'}
          </div>
        ) : (
          <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
            <thead>
              <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <th style={{ padding: '6px', textAlign: 'left', color: FINCEPT.GRAY }}>CURRENCY</th>
                <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>HOURLY</th>
                <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>DAILY</th>
                <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>APY</th>
              </tr>
            </thead>
            <tbody>
              {filteredRates.slice(0, 30).map((rate, idx) => (
                <tr
                  key={rate.currency}
                  onClick={() => setSelectedCurrency(rate.currency)}
                  style={{
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    backgroundColor: selectedCurrency === rate.currency ? `${FINCEPT.ORANGE}20` : idx % 2 === 0 ? FINCEPT.DARK_BG : 'transparent',
                    cursor: 'pointer',
                  }}
                >
                  <td style={{ padding: '6px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                    {rate.currency}
                  </td>
                  <td style={{ padding: '6px', textAlign: 'right', fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(rate.rate) }}>
                    {formatRate(rate.rate)}
                  </td>
                  <td style={{ padding: '6px', textAlign: 'right', fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(rate.rate) }}>
                    {formatDailyRate(rate.rate)}
                  </td>
                  <td style={{ padding: '6px', textAlign: 'right', fontFamily: '"IBM Plex Mono", monospace', color: getRateColor(rate.rate) }}>
                    {formatAnnualRate(rate.rate)}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>

      {/* Info */}
      <div style={{ marginTop: '12px', padding: '10px', backgroundColor: `${FINCEPT.BLUE}15`, borderRadius: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
        <strong style={{ color: FINCEPT.BLUE }}>About Borrow Rates:</strong>
        <div style={{ marginTop: '4px' }}>
          Interest charged when borrowing assets for margin trading.
          Lower rates = cheaper to hold leveraged positions.
        </div>
      </div>

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}
