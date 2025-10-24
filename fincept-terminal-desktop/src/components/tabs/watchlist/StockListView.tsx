import React from 'react';
import { WatchlistStockWithQuote } from '../../../services/watchlistService';
import { BLOOMBERG_COLORS, formatCurrency, formatPercent, formatVolume, getChangeColor, SortCriteria } from './utils';
import { Trash2 } from 'lucide-react';

interface StockListViewProps {
  stocks: WatchlistStockWithQuote[];
  watchlistName: string;
  sortBy: SortCriteria;
  onSortChange: (sort: SortCriteria) => void;
  onStockClick: (stock: WatchlistStockWithQuote) => void;
  onRemoveStock: (symbol: string) => void;
  selectedSymbol?: string;
}

const StockListView: React.FC<StockListViewProps> = ({
  stocks,
  watchlistName,
  sortBy,
  onSortChange,
  onStockClick,
  onRemoveStock,
  selectedSymbol
}) => {
  const { ORANGE, WHITE, GRAY, GREEN, RED, YELLOW, CYAN, DARK_BG, PANEL_BG } = BLOOMBERG_COLORS;

  const handleRemove = (e: React.MouseEvent, symbol: string) => {
    e.stopPropagation();
    if (confirm(`Remove ${symbol} from watchlist?`)) {
      onRemoveStock(symbol);
    }
  };

  return (
    <div style={{
      flex: 1,
      backgroundColor: PANEL_BG,
      border: `1px solid ${GRAY}`,
      padding: '4px',
      overflow: 'auto'
    }}>
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '4px'
      }}>
        <div style={{
          color: ORANGE,
          fontSize: '11px',
          fontWeight: 'bold'
        }}>
          WATCHLIST: {watchlistName}
        </div>
        <div style={{
          display: 'flex',
          gap: '4px',
          fontSize: '9px'
        }}>
          <button
            onClick={() => onSortChange('TICKER')}
            style={{
              padding: '2px 6px',
              backgroundColor: sortBy === 'TICKER' ? ORANGE : DARK_BG,
              color: sortBy === 'TICKER' ? DARK_BG : WHITE,
              border: `1px solid ${GRAY}`,
              cursor: 'pointer',
              fontFamily: 'Consolas, monospace',
              fontWeight: sortBy === 'TICKER' ? 'bold' : 'normal'
            }}
          >
            TICKER
          </button>
          <button
            onClick={() => onSortChange('CHANGE')}
            style={{
              padding: '2px 6px',
              backgroundColor: sortBy === 'CHANGE' ? ORANGE : DARK_BG,
              color: sortBy === 'CHANGE' ? DARK_BG : WHITE,
              border: `1px solid ${GRAY}`,
              cursor: 'pointer',
              fontFamily: 'Consolas, monospace',
              fontWeight: sortBy === 'CHANGE' ? 'bold' : 'normal'
            }}
          >
            CHANGE
          </button>
          <button
            onClick={() => onSortChange('VOLUME')}
            style={{
              padding: '2px 6px',
              backgroundColor: sortBy === 'VOLUME' ? ORANGE : DARK_BG,
              color: sortBy === 'VOLUME' ? DARK_BG : WHITE,
              border: `1px solid ${GRAY}`,
              cursor: 'pointer',
              fontFamily: 'Consolas, monospace',
              fontWeight: sortBy === 'VOLUME' ? 'bold' : 'normal'
            }}
          >
            VOLUME
          </button>
          <button
            onClick={() => onSortChange('PRICE')}
            style={{
              padding: '2px 6px',
              backgroundColor: sortBy === 'PRICE' ? ORANGE : DARK_BG,
              color: sortBy === 'PRICE' ? DARK_BG : WHITE,
              border: `1px solid ${GRAY}`,
              cursor: 'pointer',
              fontFamily: 'Consolas, monospace',
              fontWeight: sortBy === 'PRICE' ? 'bold' : 'normal'
            }}
          >
            PRICE
          </button>
        </div>
      </div>
      <div style={{ borderBottom: `1px solid ${GRAY}`, marginBottom: '8px' }}></div>

      {stocks.length === 0 ? (
        <div style={{
          padding: '32px',
          textAlign: 'center',
          color: GRAY,
          fontSize: '11px'
        }}>
          <div style={{ marginBottom: '8px', fontSize: '12px' }}>
            No stocks in this watchlist yet
          </div>
          <div>
            Click "ADD STOCK" to add stocks
          </div>
        </div>
      ) : (
        <>
          {/* Table Header */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: '100px 90px 90px 90px 50px',
            gap: '4px',
            padding: '4px',
            backgroundColor: 'rgba(255,255,255,0.05)',
            fontSize: '9px',
            fontWeight: 'bold',
            marginBottom: '2px'
          }}>
            <div style={{ color: GRAY }}>SYMBOL</div>
            <div style={{ color: GRAY }}>PRICE</div>
            <div style={{ color: GRAY }}>CHANGE</div>
            <div style={{ color: GRAY }}>VOLUME</div>
            <div style={{ color: GRAY }}>ACTION</div>
          </div>

          {/* Stock Rows */}
          {stocks.map((stock) => {
            const quote = stock.quote;
            const isSelected = selectedSymbol === stock.symbol;

            return (
              <div
                key={stock.id}
                onClick={() => onStockClick(stock)}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '100px 90px 90px 90px 50px',
                  gap: '4px',
                  padding: '8px 4px',
                  backgroundColor: isSelected
                    ? 'rgba(255,165,0,0.1)'
                    : 'rgba(255,255,255,0.02)',
                  borderLeft: `3px solid ${quote ? getChangeColor(quote.change) : GRAY}`,
                  fontSize: '10px',
                  marginBottom: '1px',
                  cursor: 'pointer',
                  transition: 'background-color 0.2s'
                }}
                onMouseEnter={(e) => {
                  if (!isSelected) {
                    e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.05)';
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isSelected) {
                    e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.02)';
                  }
                }}
              >
                <div style={{
                  color: CYAN,
                  fontWeight: 'bold',
                  letterSpacing: '0.5px'
                }}>
                  {stock.symbol}
                </div>

                {quote ? (
                  <>
                    <div style={{
                      color: WHITE,
                      fontWeight: 'bold'
                    }}>
                      {formatCurrency(quote.price)}
                    </div>
                    <div>
                      <div style={{
                        color: getChangeColor(quote.change)
                      }}>
                        {formatCurrency(quote.change)}
                      </div>
                      <div style={{
                        color: getChangeColor(quote.change_percent),
                        fontSize: '8px'
                      }}>
                        ({formatPercent(quote.change_percent)})
                      </div>
                    </div>
                    <div style={{
                      color: YELLOW
                    }}>
                      {formatVolume(quote.volume)}
                    </div>
                  </>
                ) : (
                  <>
                    <div style={{ color: GRAY }}>N/A</div>
                    <div style={{ color: GRAY }}>N/A</div>
                    <div style={{ color: GRAY }}>N/A</div>
                  </>
                )}

                <div>
                  <button
                    onClick={(e) => handleRemove(e, stock.symbol)}
                    style={{
                      background: 'transparent',
                      border: 'none',
                      color: RED,
                      cursor: 'pointer',
                      padding: '2px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center'
                    }}
                    title={`Remove ${stock.symbol}`}
                  >
                    <Trash2 size={14} />
                  </button>
                </div>
              </div>
            );
          })}
        </>
      )}
    </div>
  );
};

export default StockListView;
