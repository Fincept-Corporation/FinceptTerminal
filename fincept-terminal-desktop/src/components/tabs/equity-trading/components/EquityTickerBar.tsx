/**
 * EquityTickerBar - Price ticker bar with symbol search and market stats
 */
import React from 'react';
import { TrendingUp, TrendingDown } from 'lucide-react';
import { SymbolSearch } from './SymbolSearch';
import { FINCEPT } from '../constants';
import type { EquityTickerBarProps } from '../types';
import type { SupportedBroker } from '@/services/trading/masterContractService';

export const EquityTickerBar: React.FC<EquityTickerBarProps> = ({
  selectedSymbol,
  selectedExchange,
  activeBroker,
  quote,
  funds,
  isAuthenticated,
  totalPositionPnl,
  priceChange,
  priceChangePercent,
  currentPrice,
  dayRange,
  dayRangePercent,
  onSymbolSelect,
  fmtPrice,
}) => {
  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      padding: '10px 12px',
      display: 'flex',
      alignItems: 'center',
      gap: '20px',
      flexShrink: 0
    }}>
      {/* Symbol Selector */}
      <SymbolSearch
        selectedSymbol={selectedSymbol}
        selectedExchange={selectedExchange}
        onSymbolSelect={onSymbolSelect}
        brokerId={(activeBroker || 'angelone') as SupportedBroker}
        placeholder="Search stocks, F&O, commodities..."
        showDownloadStatus={true}
      />

      {/* Price Display */}
      <div style={{ display: 'flex', alignItems: 'baseline', gap: '8px' }}>
        <span style={{
          fontSize: '24px',
          fontWeight: 700,
          color: FINCEPT.YELLOW,
          willChange: 'contents',
          transition: 'none'
        }}>
          {currentPrice > 0 ? fmtPrice(currentPrice) : '--'}
        </span>
        {quote && (priceChange !== 0 || priceChangePercent !== 0) && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            {priceChange >= 0 ? (
              <TrendingUp size={16} color={FINCEPT.GREEN} />
            ) : (
              <TrendingDown size={16} color={FINCEPT.RED} />
            )}
            <span style={{
              fontSize: '13px',
              fontWeight: 600,
              color: priceChange >= 0 ? FINCEPT.GREEN : FINCEPT.RED
            }}>
              {priceChange >= 0 ? '+' : ''}{priceChange.toFixed(2)} ({priceChangePercent >= 0 ? '+' : ''}{priceChangePercent.toFixed(2)}%)
            </span>
          </div>
        )}
      </div>

      <div style={{ height: '24px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

      {/* Market Stats */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '24px', fontSize: '11px', willChange: 'contents' }}>
        <div style={{ minWidth: '60px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>BID</div>
          <div style={{ color: FINCEPT.GREEN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {quote?.bid ? fmtPrice(quote.bid) : '--'}
          </div>
        </div>
        <div style={{ minWidth: '60px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>ASK</div>
          <div style={{ color: FINCEPT.RED, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {quote?.ask ? fmtPrice(quote.ask) : '--'}
          </div>
        </div>
        <div style={{ minWidth: '100px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>DAY RANGE</div>
          <div style={{ color: FINCEPT.CYAN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {dayRange > 0 ? `${fmtPrice(dayRange)} (${dayRangePercent.toFixed(2)}%)` : '--'}
          </div>
        </div>
        <div style={{ minWidth: '80px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>HIGH</div>
          <div style={{ color: FINCEPT.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {quote?.high ? fmtPrice(quote.high) : '--'}
          </div>
        </div>
        <div style={{ minWidth: '80px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>LOW</div>
          <div style={{ color: FINCEPT.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {quote?.low ? fmtPrice(quote.low) : '--'}
          </div>
        </div>
        <div style={{ minWidth: '100px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>VOLUME</div>
          <div style={{ color: FINCEPT.PURPLE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {quote?.volume ? quote.volume.toLocaleString('en-IN') : '--'}
          </div>
        </div>
      </div>

      {/* Account Summary */}
      {isAuthenticated && (
        <>
          <div style={{ height: '24px', width: '1px', backgroundColor: FINCEPT.BORDER }} />
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px' }}>
            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>MARGIN</div>
              <div style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>
                {fmtPrice(funds?.availableMargin || 0)}
              </div>
            </div>
            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>P&L</div>
              <div style={{ color: totalPositionPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED, fontWeight: 600 }}>
                {totalPositionPnl >= 0 ? '+' : ''}{fmtPrice(Math.abs(totalPositionPnl))}
              </div>
            </div>
          </div>
        </>
      )}
    </div>
  );
};
