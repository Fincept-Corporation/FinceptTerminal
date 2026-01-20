// CryptoTickerBar.tsx - Ticker/Price Bar for Crypto Trading
import React from 'react';
import { TrendingUp as ArrowUp, TrendingDown as ArrowDown, ChevronDown } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { TickerData } from '../types';

interface CryptoTickerBarProps {
  selectedSymbol: string;
  tickerData: TickerData | null;
  availableSymbols: string[];
  searchQuery: string;
  showSymbolDropdown: boolean;
  balance: number;
  equity: number;
  tradingMode: 'paper' | 'live';
  priceChange: number;
  priceChangePercent: number;
  onSymbolSelect: (symbol: string) => void;
  onSearchQueryChange: (query: string) => void;
  onSymbolDropdownToggle: () => void;
}

export function CryptoTickerBar({
  selectedSymbol,
  tickerData,
  availableSymbols,
  searchQuery,
  showSymbolDropdown,
  balance,
  equity,
  tradingMode,
  priceChange,
  priceChangePercent,
  onSymbolSelect,
  onSearchQueryChange,
  onSymbolDropdownToggle,
}: CryptoTickerBarProps) {
  const currentPrice = tickerData?.last || tickerData?.price || 0;

  // Calculate 24h price range (High - Low)
  const spread24h = (tickerData?.high && tickerData?.low) ? (tickerData.high - tickerData.low) : 0;
  const spread24hPercent = (spread24h > 0 && tickerData?.low && tickerData.low > 0)
    ? ((spread24h / tickerData.low) * 100)
    : 0;

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
      <div style={{ position: 'relative', minWidth: '180px' }} data-symbol-selector>
        <div
          onClick={onSymbolDropdownToggle}
          style={{
            padding: '6px 12px',
            backgroundColor: FINCEPT.HEADER_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            cursor: 'pointer',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center'
          }}
        >
          <span style={{ fontSize: '13px', fontWeight: 700, color: FINCEPT.ORANGE }}>{selectedSymbol}</span>
          <ChevronDown size={12} color={FINCEPT.GRAY} />
        </div>

        {showSymbolDropdown && (
          <div style={{
            position: 'absolute',
            top: '100%',
            left: 0,
            right: 0,
            marginTop: '4px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            maxHeight: '400px',
            overflow: 'hidden',
            zIndex: 1000,
            boxShadow: `0 4px 16px ${FINCEPT.DARK_BG}80`
          }}>
            <div style={{ padding: '8px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <input
                type="text"
                placeholder="Search symbols..."
                value={searchQuery}
                onChange={(e) => onSearchQueryChange(e.target.value)}
                autoFocus
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '11px',
                  outline: 'none'
                }}
              />
            </div>
            <div style={{ maxHeight: '340px', overflowY: 'auto' }}>
              {availableSymbols
                .filter(symbol => symbol.toLowerCase().includes(searchQuery.toLowerCase()))
                .map(symbol => (
                  <div
                    key={symbol}
                    onClick={() => {
                      onSymbolSelect(symbol);
                      onSearchQueryChange('');
                    }}
                    style={{
                      padding: '8px 12px',
                      cursor: 'pointer',
                      fontSize: '11px',
                      backgroundColor: symbol === selectedSymbol ? `${FINCEPT.ORANGE}20` : 'transparent',
                      color: symbol === selectedSymbol ? FINCEPT.ORANGE : FINCEPT.WHITE,
                      borderLeft: symbol === selectedSymbol ? `3px solid ${FINCEPT.ORANGE}` : 'none'
                    }}
                    onMouseEnter={(e) => {
                      if (symbol !== selectedSymbol) {
                        e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (symbol !== selectedSymbol) {
                        e.currentTarget.style.backgroundColor = 'transparent';
                      }
                    }}
                  >
                    {symbol}
                  </div>
                ))}
            </div>
          </div>
        )}
      </div>

      {/* Price Display */}
      <div style={{ display: 'flex', alignItems: 'baseline', gap: '8px' }}>
        <span style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.YELLOW, willChange: 'contents', transition: 'none' }}>
          {tickerData ? `$${currentPrice?.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 }) || '0.00'}` : '--'}
        </span>
        {tickerData && (priceChange !== 0 || priceChangePercent !== 0) && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            {priceChange >= 0 ? (
              <ArrowUp size={16} color={FINCEPT.GREEN} />
            ) : (
              <ArrowDown size={16} color={FINCEPT.RED} />
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
        {tickerData && priceChange === 0 && priceChangePercent === 0 && (
          <div style={{ fontSize: '11px', color: FINCEPT.GRAY, fontStyle: 'italic' }}>
            No change data
          </div>
        )}
      </div>

      <div style={{ height: '24px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

      {/* Market Stats */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '24px', fontSize: '11px', willChange: 'contents' }}>
        <div style={{ minWidth: '60px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>BID</div>
          <div style={{ color: FINCEPT.GREEN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {tickerData?.bid ? `$${tickerData.bid.toFixed(2)}` : '--'}
          </div>
        </div>
        <div style={{ minWidth: '60px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>ASK</div>
          <div style={{ color: FINCEPT.RED, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {tickerData?.ask ? `$${tickerData.ask.toFixed(2)}` : '--'}
          </div>
        </div>
        <div style={{ minWidth: '120px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H RANGE</div>
          <div style={{ color: FINCEPT.CYAN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {spread24h > 0 ? `$${spread24h.toFixed(2)} (${spread24hPercent.toFixed(2)}%)` : '--'}
          </div>
        </div>
        <div style={{ minWidth: '80px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H HIGH</div>
          <div style={{ color: FINCEPT.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {tickerData?.high ? `$${tickerData.high.toFixed(2)}` : '--'}
          </div>
        </div>
        <div style={{ minWidth: '80px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H LOW</div>
          <div style={{ color: FINCEPT.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {tickerData?.low ? `$${tickerData.low.toFixed(2)}` : '--'}
          </div>
        </div>
        <div style={{ minWidth: '100px' }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H VOLUME</div>
          <div style={{ color: FINCEPT.PURPLE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
            {tickerData?.volume ? (
              tickerData.quoteVolume
                ? `$${tickerData.quoteVolume.toLocaleString(undefined, { maximumFractionDigits: 0 })}`
                : currentPrice > 0
                  ? `$${(tickerData.volume * currentPrice).toLocaleString(undefined, { maximumFractionDigits: 0 })}`
                  : `${tickerData.volume.toLocaleString(undefined, { maximumFractionDigits: 2 })} ${selectedSymbol.split('/')[0]}`
            ) : '--'}
          </div>
        </div>
      </div>

      {tradingMode === 'paper' && (
        <>
          <div style={{ height: '24px', width: '1px', backgroundColor: FINCEPT.BORDER }} />
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px' }}>
            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>BALANCE</div>
              <div style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>${balance.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}</div>
            </div>
            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>EQUITY</div>
              <div style={{ color: FINCEPT.YELLOW, fontWeight: 600 }}>${equity.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}</div>
            </div>
          </div>
        </>
      )}
    </div>
  );
}

export default CryptoTickerBar;
