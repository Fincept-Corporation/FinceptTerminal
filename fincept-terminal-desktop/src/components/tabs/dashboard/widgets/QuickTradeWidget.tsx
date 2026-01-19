import React, { useState } from 'react';
import { TrendingUp, TrendingDown, Zap, DollarSign, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';

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

const FINCEPT_GREEN = '#00FF00';
const FINCEPT_RED = '#FF0000';
const FINCEPT_ORANGE = '#FFA500';
const FINCEPT_WHITE = '#FFFFFF';
const FINCEPT_GRAY = '#787878';
const FINCEPT_CYAN = '#00E5FF';

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
      headerColor={FINCEPT_CYAN}
    >
      <div style={{ padding: '4px' }}>
        {/* Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 80px 80px',
          gap: '4px',
          padding: '4px 8px',
          borderBottom: '1px solid #333',
          color: FINCEPT_GRAY,
          fontSize: '8px'
        }}>
          <span>SYMBOL</span>
          <span style={{ textAlign: 'center' }}>PRICE</span>
          <span style={{ textAlign: 'center' }}>ACTION</span>
        </div>

        {QUICK_SYMBOLS.map((asset) => (
          <div
            key={asset.symbol}
            style={{
              display: 'grid',
              gridTemplateColumns: '1fr 80px 80px',
              gap: '4px',
              padding: '6px 8px',
              borderBottom: '1px solid #222',
              alignItems: 'center'
            }}
          >
            <div>
              <div style={{ fontSize: '10px', color: FINCEPT_WHITE, fontWeight: 'bold' }}>
                {asset.symbol}
              </div>
              <div style={{
                fontSize: '9px',
                color: asset.change >= 0 ? FINCEPT_GREEN : FINCEPT_RED,
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
              fontSize: '10px',
              color: FINCEPT_ORANGE,
              fontWeight: 'bold',
              fontFamily: 'monospace'
            }}>
              ${asset.price.toLocaleString()}
            </div>
            <div style={{ display: 'flex', gap: '4px', justifyContent: 'center' }}>
              <button
                onClick={() => handleQuickTrade(asset.symbol, 'buy')}
                style={{
                  backgroundColor: FINCEPT_GREEN,
                  color: '#000',
                  border: 'none',
                  padding: '3px 8px',
                  fontSize: '9px',
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
                  backgroundColor: FINCEPT_RED,
                  color: '#000',
                  border: 'none',
                  padding: '3px 8px',
                  fontSize: '9px',
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
            backgroundColor: '#111',
            padding: '8px',
            margin: '8px',
            borderRadius: '2px',
            border: `1px solid ${orderSide === 'buy' ? FINCEPT_GREEN : FINCEPT_RED}`
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px'
            }}>
              <span style={{ fontSize: '10px', color: FINCEPT_WHITE }}>
                {orderSide.toUpperCase()} {selectedSymbol}
              </span>
              <button
                onClick={() => setSelectedSymbol(null)}
                style={{
                  background: 'none',
                  border: 'none',
                  color: FINCEPT_GRAY,
                  cursor: 'pointer',
                  fontSize: '10px'
                }}
              >
                ✕
              </button>
            </div>
            <div style={{ display: 'flex', gap: '8px' }}>
              <input
                type="number"
                placeholder="Qty"
                value={quantity}
                onChange={(e) => setQuantity(e.target.value)}
                style={{
                  flex: 1,
                  backgroundColor: '#000',
                  border: '1px solid #333',
                  color: FINCEPT_WHITE,
                  padding: '4px 8px',
                  fontSize: '10px'
                }}
              />
              <button
                onClick={() => {
                  console.log(`Executing ${orderSide} ${quantity} ${selectedSymbol}`);
                  setSelectedSymbol(null);
                  setQuantity('');
                }}
                style={{
                  backgroundColor: orderSide === 'buy' ? FINCEPT_GREEN : FINCEPT_RED,
                  color: '#000',
                  border: 'none',
                  padding: '4px 12px',
                  fontSize: '10px',
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
          borderTop: '1px solid #333',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <span style={{ fontSize: '8px', color: FINCEPT_GRAY }}>
            <Zap size={10} style={{ verticalAlign: 'middle', marginRight: '4px' }} />
            One-click trading enabled
          </span>
          {onNavigate && (
            <div
              onClick={onNavigate}
              style={{
                color: FINCEPT_CYAN,
                fontSize: '9px',
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
