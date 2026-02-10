// QuickTradeWidget - Quick trading actions with real-time prices
// Uses Yahoo Finance data via the unified cache system

import React, { useState, useMemo } from 'react';
import { TrendingUp, TrendingDown, Zap, ExternalLink, RefreshCw } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { useTranslation } from 'react-i18next';
import { marketDataService, QuoteData } from '@/services/markets/marketDataService';
import { useCache } from '@/hooks/useCache';

interface QuickTradeWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

// Quick trade symbols - mix of popular stocks and crypto
const QUICK_TRADE_SYMBOLS = [
  'BTC-USD',   // Bitcoin
  'ETH-USD',   // Ethereum
  'SOL-USD',   // Solana
  'SPY',       // S&P 500 ETF
  'QQQ',       // Nasdaq ETF
];

// Display config for symbols
const SYMBOL_CONFIG: Record<string, { display: string; name: string }> = {
  'BTC-USD': { display: 'BTC/USD', name: 'Bitcoin' },
  'ETH-USD': { display: 'ETH/USD', name: 'Ethereum' },
  'SOL-USD': { display: 'SOL/USD', name: 'Solana' },
  'SPY': { display: 'SPY', name: 'S&P 500 ETF' },
  'QQQ': { display: 'QQQ', name: 'Nasdaq ETF' },
};

interface QuickAction {
  symbol: string;
  displaySymbol: string;
  name: string;
  price: number;
  change: number;
  changePercent: number;
}

export const QuickTradeWidget: React.FC<QuickTradeWidgetProps> = ({
  id,
  onRemove,
  onNavigate
}) => {
  const { t } = useTranslation('dashboard');
  const [selectedSymbol, setSelectedSymbol] = useState<string | null>(null);
  const [orderSide, setOrderSide] = useState<'buy' | 'sell'>('buy');
  const [quantity, setQuantity] = useState('');

  // Fetch real market data
  const {
    data: quotes,
    isLoading,
    error,
    refresh
  } = useCache<QuoteData[]>({
    key: 'market-widget:quick-trade',
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(QUICK_TRADE_SYMBOLS),
    ttl: '1m',
    refetchInterval: 2 * 60 * 1000, // Refresh every 2 minutes for trading
    staleWhileRevalidate: true
  });

  // Transform quotes to quick actions
  const quickAssets: QuickAction[] = useMemo(() => {
    if (!quotes || quotes.length === 0) return [];

    return quotes.map(quote => {
      const config = SYMBOL_CONFIG[quote.symbol] || { display: quote.symbol, name: quote.symbol };
      return {
        symbol: quote.symbol,
        displaySymbol: config.display,
        name: config.name,
        price: quote.price,
        change: quote.change,
        changePercent: quote.change_percent,
      };
    });
  }, [quotes]);

  const handleQuickTrade = (symbol: string, side: 'buy' | 'sell') => {
    setSelectedSymbol(symbol);
    setOrderSide(side);
    // In production, this would open a trade confirmation modal or execute via broker API
    console.log(`Quick ${side} order for ${symbol}`);
  };

  const formatPrice = (price: number) => {
    if (price >= 10000) {
      return price.toLocaleString(undefined, { minimumFractionDigits: 0, maximumFractionDigits: 0 });
    } else if (price >= 100) {
      return price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
    } else {
      return price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 4 });
    }
  };

  // Skeleton row for loading state
  const SkeletonRow = () => (
    <div style={{
      display: 'grid',
      gridTemplateColumns: '1fr 80px 80px',
      gap: '4px',
      padding: '6px 8px',
      borderBottom: '1px solid var(--ft-border-color)',
      alignItems: 'center'
    }}>
      <div>
        <div style={{ backgroundColor: 'rgba(120,120,120,0.3)', height: '12px', width: '60px', borderRadius: '2px', marginBottom: '4px' }} />
        <div style={{ backgroundColor: 'rgba(120,120,120,0.2)', height: '10px', width: '40px', borderRadius: '2px' }} />
      </div>
      <div style={{ backgroundColor: 'rgba(120,120,120,0.3)', height: '14px', borderRadius: '2px' }} />
      <div style={{ display: 'flex', gap: '4px', justifyContent: 'center' }}>
        <div style={{ backgroundColor: 'rgba(120,120,120,0.3)', height: '20px', width: '32px', borderRadius: '2px' }} />
        <div style={{ backgroundColor: 'rgba(120,120,120,0.3)', height: '20px', width: '32px', borderRadius: '2px' }} />
      </div>
    </div>
  );

  return (
    <BaseWidget
      id={id}
      title="QUICK TRADE"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={false}
      error={error?.message}
      headerColor="var(--ft-color-accent)"
    >
      <div style={{ padding: '4px' }}>
        {/* Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 80px 80px',
          gap: '4px',
          padding: '4px 8px',
          borderBottom: '1px solid var(--ft-border-color)',
          color: 'var(--ft-color-text-muted)',
          fontSize: 'var(--ft-font-size-tiny)'
        }}>
          <span>{t('widgets.symbol')}</span>
          <span style={{ textAlign: 'center' }}>{t('widgets.price')}</span>
          <span style={{ textAlign: 'center' }}>{t('widgets.action')}</span>
        </div>

        {/* Loading state */}
        {isLoading && quickAssets.length === 0 ? (
          <>
            {QUICK_TRADE_SYMBOLS.map((_, idx) => <SkeletonRow key={idx} />)}
          </>
        ) : quickAssets.length === 0 && !isLoading ? (
          <div style={{
            padding: '20px',
            textAlign: 'center',
            color: 'var(--ft-color-text-muted)',
            fontSize: 'var(--ft-font-size-small)'
          }}>
            No market data available
          </div>
        ) : (
          quickAssets.map((asset) => (
            <div
              key={asset.symbol}
              style={{
                display: 'grid',
                gridTemplateColumns: '1fr 80px 80px',
                gap: '4px',
                padding: '6px 8px',
                borderBottom: '1px solid var(--ft-border-color)',
                alignItems: 'center'
              }}
            >
              <div>
                <div style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)', fontWeight: 'bold' }}>
                  {asset.displaySymbol}
                </div>
                <div style={{
                  fontSize: 'var(--ft-font-size-tiny)',
                  color: asset.change >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '2px'
                }}>
                  {asset.change >= 0 ? <TrendingUp size={8} /> : <TrendingDown size={8} />}
                  {asset.change >= 0 ? '+' : ''}{asset.changePercent.toFixed(2)}%
                </div>
              </div>
              <div style={{
                textAlign: 'center',
                fontSize: 'var(--ft-font-size-small)',
                color: 'var(--ft-color-primary)',
                fontWeight: 'bold',
                fontFamily: 'monospace'
              }}>
                ${formatPrice(asset.price)}
              </div>
              <div style={{ display: 'flex', gap: '4px', justifyContent: 'center' }}>
                <button
                  onClick={() => handleQuickTrade(asset.symbol, 'buy')}
                  style={{
                    backgroundColor: 'var(--ft-color-success)',
                    color: '#000',
                    border: 'none',
                    padding: '3px 8px',
                    fontSize: 'var(--ft-font-size-tiny)',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    borderRadius: '2px'
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
                  onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                >
                  BUY
                </button>
                <button
                  onClick={() => handleQuickTrade(asset.symbol, 'sell')}
                  style={{
                    backgroundColor: 'var(--ft-color-alert)',
                    color: '#000',
                    border: 'none',
                    padding: '3px 8px',
                    fontSize: 'var(--ft-font-size-tiny)',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    borderRadius: '2px'
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
                  onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                >
                  SELL
                </button>
              </div>
            </div>
          ))
        )}

        {/* Quick order panel */}
        {selectedSymbol && (
          <div style={{
            backgroundColor: 'var(--ft-color-panel)',
            padding: '8px',
            margin: '8px',
            borderRadius: '2px',
            border: `1px solid ${orderSide === 'buy' ? 'var(--ft-color-success)' : 'var(--ft-color-alert)'}`
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px'
            }}>
              <span style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                {orderSide.toUpperCase()} {SYMBOL_CONFIG[selectedSymbol]?.display || selectedSymbol}
              </span>
              <button
                onClick={() => setSelectedSymbol(null)}
                style={{
                  background: 'none',
                  border: 'none',
                  color: 'var(--ft-color-text-muted)',
                  cursor: 'pointer',
                  fontSize: 'var(--ft-font-size-small)'
                }}
              >
                ✕
              </button>
            </div>
            <div style={{ display: 'flex', gap: '8px' }}>
              <input
                type="text"
                inputMode="decimal"
                placeholder="Qty"
                value={quantity}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setQuantity(v); }}
                style={{
                  flex: 1,
                  backgroundColor: 'var(--ft-color-background)',
                  border: '1px solid var(--ft-border-color)',
                  color: 'var(--ft-color-text)',
                  padding: '4px 8px',
                  fontSize: 'var(--ft-font-size-small)'
                }}
              />
              <button
                onClick={() => {
                  console.log(`Executing ${orderSide} ${quantity} ${selectedSymbol}`);
                  setSelectedSymbol(null);
                  setQuantity('');
                }}
                style={{
                  backgroundColor: orderSide === 'buy' ? 'var(--ft-color-success)' : 'var(--ft-color-alert)',
                  color: '#000',
                  border: 'none',
                  padding: '4px 12px',
                  fontSize: 'var(--ft-font-size-small)',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
              >
                EXECUTE
              </button>
            </div>
          </div>
        )}

        {/* Footer */}
        <div style={{
          padding: '6px 8px',
          borderTop: '1px solid var(--ft-border-color)',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <span style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>
            <Zap size={10} style={{ verticalAlign: 'middle', marginRight: '4px' }} />
            One-click trading enabled
          </span>
          {onNavigate && (
            <div
              onClick={onNavigate}
              style={{
                color: 'var(--ft-color-accent)',
                fontSize: 'var(--ft-font-size-tiny)',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <span>Full Trading</span>
              <ExternalLink size={10} />
            </div>
          )}
        </div>
      </div>
    </BaseWidget>
  );
};
