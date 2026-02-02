import React from 'react';
import { WatchlistStockWithQuote } from '../../../services/core/watchlistService';
import { FINCEPT, FONT_FAMILY, formatCurrency, formatPercent, formatVolume, getChangeColor, SortCriteria } from './utils';
import { Trash2, ArrowUpDown, Inbox } from 'lucide-react';
import { showConfirm } from '@/utils/notifications';

interface StockListViewProps {
  stocks: WatchlistStockWithQuote[];
  watchlistName: string;
  sortBy: SortCriteria;
  onSortChange: (sort: SortCriteria) => void;
  onStockClick: (stock: WatchlistStockWithQuote) => void;
  onRemoveStock: (symbol: string) => void;
  selectedSymbol?: string;
  isLoading?: boolean;
}

const SORT_OPTIONS: { key: SortCriteria; label: string }[] = [
  { key: 'TICKER', label: 'TICKER' },
  { key: 'CHANGE', label: 'CHANGE' },
  { key: 'VOLUME', label: 'VOLUME' },
  { key: 'PRICE', label: 'PRICE' },
];

const StockListView: React.FC<StockListViewProps> = ({
  stocks,
  watchlistName,
  sortBy,
  onSortChange,
  onStockClick,
  onRemoveStock,
  selectedSymbol,
  isLoading
}) => {
  const handleRemove = async (e: React.MouseEvent, symbol: string) => {
    e.stopPropagation();
    const confirmed = await showConfirm(
      `Remove ${symbol} from watchlist "${watchlistName}"?`,
      { title: 'Remove Stock', type: 'warning' }
    );
    if (confirmed) {
      onRemoveStock(symbol);
    }
  };

  return (
    <div style={{
      flex: 1,
      backgroundColor: FINCEPT.PANEL_BG,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontFamily: FONT_FAMILY
    }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
          {watchlistName.toUpperCase()} ({stocks.length})
        </span>
        <div style={{ display: 'flex', gap: '4px' }}>
          {SORT_OPTIONS.map(opt => (
            <button
              key={opt.key}
              onClick={() => onSortChange(opt.key)}
              style={{
                padding: '4px 8px',
                backgroundColor: sortBy === opt.key ? FINCEPT.ORANGE : 'transparent',
                color: sortBy === opt.key ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: sortBy === opt.key ? 'none' : `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '8px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
                transition: 'all 0.2s'
              }}
            >
              {opt.label}
            </button>
          ))}
        </div>
      </div>

      {/* Table */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {/* Table Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 90px 100px 90px 36px',
          gap: '4px',
          padding: '8px 12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          position: 'sticky',
          top: 0,
          zIndex: 1
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>SYMBOL</span>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textAlign: 'right' }}>PRICE</span>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textAlign: 'right' }}>CHANGE</span>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textAlign: 'right' }}>VOLUME</span>
          <span />
        </div>

        {isLoading && stocks.length === 0 ? (
          // Skeleton loading
          <div style={{ padding: '8px 12px' }}>
            {Array.from({ length: 8 }).map((_, i) => (
              <div key={i} style={{
                display: 'grid',
                gridTemplateColumns: '1fr 90px 100px 90px 36px',
                gap: '4px',
                padding: '10px 0',
                borderBottom: `1px solid ${FINCEPT.BORDER}`
              }}>
                {[0, 1, 2, 3].map(j => (
                  <div key={j} style={{
                    backgroundColor: FINCEPT.BORDER,
                    height: '10px',
                    borderRadius: '2px',
                    opacity: 0.5,
                    width: j === 0 ? '60%' : '70%',
                    marginLeft: j > 0 ? 'auto' : undefined
                  }} />
                ))}
                <div />
              </div>
            ))}
          </div>
        ) : stocks.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <Inbox size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>NO STOCKS IN THIS WATCHLIST</span>
            <span style={{ fontSize: '9px', marginTop: '4px' }}>CLICK "ADD STOCK" TO GET STARTED</span>
          </div>
        ) : (
          stocks.map((stock) => {
            const quote = stock.quote;
            const isSelected = selectedSymbol === stock.symbol;

            return (
              <div
                key={stock.id}
                onClick={() => onStockClick(stock)}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 90px 100px 90px 36px',
                  gap: '4px',
                  padding: '10px 12px',
                  backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: isSelected ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  alignItems: 'center'
                }}
                onMouseEnter={(e) => {
                  if (!isSelected) {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isSelected) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                  }
                }}
              >
                {/* Symbol */}
                <div style={{
                  color: FINCEPT.CYAN,
                  fontWeight: 700,
                  fontSize: '10px',
                  letterSpacing: '0.5px'
                }}>
                  {stock.symbol}
                </div>

                {quote ? (
                  <>
                    {/* Price */}
                    <div style={{ color: FINCEPT.WHITE, fontWeight: 700, fontSize: '10px', textAlign: 'right' }}>
                      {formatCurrency(quote.price)}
                    </div>

                    {/* Change */}
                    <div style={{ textAlign: 'right' }}>
                      <div style={{ color: getChangeColor(quote.change), fontSize: '10px', fontWeight: 700 }}>
                        {formatPercent(quote.change_percent)}
                      </div>
                      <div style={{ color: getChangeColor(quote.change), fontSize: '8px' }}>
                        {formatCurrency(quote.change)}
                      </div>
                    </div>

                    {/* Volume */}
                    <div style={{ color: FINCEPT.YELLOW, fontSize: '10px', textAlign: 'right' }}>
                      {formatVolume(quote.volume)}
                    </div>
                  </>
                ) : (
                  <>
                    <div style={{ color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'right' }}>--</div>
                    <div style={{ color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'right' }}>--</div>
                    <div style={{ color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'right' }}>--</div>
                  </>
                )}

                {/* Remove */}
                <div style={{ display: 'flex', justifyContent: 'center' }}>
                  <button
                    onClick={(e) => handleRemove(e, stock.symbol)}
                    style={{
                      background: 'transparent',
                      border: 'none',
                      color: FINCEPT.MUTED,
                      cursor: 'pointer',
                      padding: '4px',
                      display: 'flex',
                      borderRadius: '2px',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => { e.currentTarget.style.color = FINCEPT.RED; }}
                    onMouseLeave={(e) => { e.currentTarget.style.color = FINCEPT.MUTED; }}
                    title={`Remove ${stock.symbol}`}
                  >
                    <Trash2 size={12} />
                  </button>
                </div>
              </div>
            );
          })
        )}
      </div>
    </div>
  );
};

export default StockListView;
