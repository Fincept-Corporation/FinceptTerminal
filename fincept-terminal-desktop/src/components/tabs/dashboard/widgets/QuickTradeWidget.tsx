import React, { useState } from 'react';
import { TrendingUp, TrendingDown, Zap, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { useTranslation } from 'react-i18next';

interface QuickTradeWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface QuickAction {
  symbol: string;
  name: string;
  price: number;
  change: number;
  changePercent: number;
}

const QUICK_SYMBOLS: QuickAction[] = [
  { symbol: 'BTC/USD', name: 'Bitcoin', price: 97250, change: 1250, changePercent: 1.30 },
  { symbol: 'ETH/USD', name: 'Ethereum', price: 3420, change: -45, changePercent: -1.30 },
  { symbol: 'SOL/USD', name: 'Solana', price: 198.50, change: 8.20, changePercent: 4.31 },
  { symbol: 'SPY', name: 'S&P 500 ETF', price: 592.45, change: 2.15, changePercent: 0.36 },
  { symbol: 'QQQ', name: 'Nasdaq ETF', price: 515.30, change: 3.80, changePercent: 0.74 },
];

export const QuickTradeWidget: React.FC<QuickTradeWidgetProps> = ({
  id,
  onRemove,
  onNavigate
}) => {
  const { t } = useTranslation('dashboard');
  const [selectedSymbol, setSelectedSymbol] = useState<string | null>(null);
  const [orderSide, setOrderSide] = useState<'buy' | 'sell'>('buy');
  const [quantity, setQuantity] = useState('');

  const handleQuickTrade = (symbol: string, side: 'buy' | 'sell') => {
    setSelectedSymbol(symbol);
    setOrderSide(side);
    // In production, this would open a trade confirmation modal or execute via broker API
    console.log(`Quick ${side} order for ${symbol}`);
  };

  return (
    <BaseWidget
      id={id}
      title="QUICK TRADE"
      onRemove={onRemove}
      isLoading={false}
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

        {QUICK_SYMBOLS.map((asset) => (
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
                {asset.symbol}
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
              ${asset.price.toLocaleString()}
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
        ))}

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
                {orderSide.toUpperCase()} {selectedSymbol}
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
