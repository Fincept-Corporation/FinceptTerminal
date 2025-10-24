import React from 'react';
import { WatchlistStockWithQuote } from '../../../services/watchlistService';
import { BLOOMBERG_COLORS, formatCurrency, formatPercent, formatVolume, getChangeColor } from './utils';

interface StockDetailPanelProps {
  stock: WatchlistStockWithQuote | null;
}

const StockDetailPanel: React.FC<StockDetailPanelProps> = ({ stock }) => {
  const { ORANGE, WHITE, GRAY, GREEN, RED, YELLOW, CYAN, PANEL_BG } = BLOOMBERG_COLORS;

  if (!stock) {
    return (
      <div style={{
        width: '320px',
        backgroundColor: PANEL_BG,
        border: `1px solid ${GRAY}`,
        padding: '12px',
        overflow: 'auto',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        color: GRAY,
        fontSize: '11px',
        textAlign: 'center'
      }}>
        <div style={{ marginBottom: '8px', fontSize: '12px' }}>
          NO STOCK SELECTED
        </div>
        <div>
          Click on a stock to view details
        </div>
      </div>
    );
  }

  const quote = stock.quote;

  return (
    <div style={{
      width: '320px',
      backgroundColor: PANEL_BG,
      border: `1px solid ${GRAY}`,
      padding: '4px',
      overflow: 'auto'
    }}>
      <div style={{
        color: ORANGE,
        fontSize: '11px',
        fontWeight: 'bold',
        marginBottom: '4px'
      }}>
        STOCK DETAIL: {stock.symbol}
      </div>
      <div style={{ borderBottom: `1px solid ${GRAY}`, marginBottom: '8px' }}></div>

      {!quote ? (
        <div style={{
          padding: '16px',
          textAlign: 'center',
          color: RED,
          fontSize: '10px'
        }}>
          Failed to load market data
        </div>
      ) : (
        <>
          {/* Price Info */}
          <div style={{ marginBottom: '12px' }}>
            <div style={{
              padding: '6px',
              backgroundColor: 'rgba(255,165,0,0.1)',
              marginBottom: '4px'
            }}>
              <div style={{
                color: GRAY,
                fontSize: '9px',
                marginBottom: '2px'
              }}>
                LAST PRICE
              </div>
              <div style={{
                color: WHITE,
                fontSize: '20px',
                fontWeight: 'bold'
              }}>
                {formatCurrency(quote.price)}
              </div>
              <div style={{
                color: getChangeColor(quote.change),
                fontSize: '12px'
              }}>
                {formatCurrency(quote.change)} ({formatPercent(quote.change_percent)})
              </div>
            </div>
          </div>

          {/* Trading Stats */}
          <div style={{
            fontSize: '9px',
            lineHeight: '1.5',
            marginBottom: '12px'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '2px'
            }}>
              <span style={{ color: GRAY }}>Open:</span>
              <span style={{ color: WHITE }}>{formatCurrency(quote.open)}</span>
            </div>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '2px'
            }}>
              <span style={{ color: GRAY }}>Prev Close:</span>
              <span style={{ color: WHITE }}>{formatCurrency(quote.previous_close)}</span>
            </div>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '2px'
            }}>
              <span style={{ color: GRAY }}>Day High:</span>
              <span style={{ color: GREEN }}>{formatCurrency(quote.high)}</span>
            </div>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '2px'
            }}>
              <span style={{ color: GRAY }}>Day Low:</span>
              <span style={{ color: RED }}>{formatCurrency(quote.low)}</span>
            </div>
            <div style={{
              borderTop: `1px solid ${GRAY}`,
              marginTop: '4px',
              paddingTop: '4px'
            }}>
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                marginBottom: '2px'
              }}>
                <span style={{ color: GRAY }}>Volume:</span>
                <span style={{ color: YELLOW }}>{formatVolume(quote.volume)}</span>
              </div>
            </div>
          </div>

          {/* Notes Section */}
          {stock.notes && (
            <div style={{
              borderTop: `1px solid ${GRAY}`,
              paddingTop: '8px',
              marginTop: '8px'
            }}>
              <div style={{
                color: YELLOW,
                fontSize: '10px',
                fontWeight: 'bold',
                marginBottom: '4px'
              }}>
                NOTES
              </div>
              <div style={{
                fontSize: '9px',
                color: WHITE,
                backgroundColor: 'rgba(255,255,255,0.05)',
                padding: '6px',
                borderLeft: `2px solid ${CYAN}`,
                whiteSpace: 'pre-wrap',
                wordBreak: 'break-word'
              }}>
                {stock.notes}
              </div>
            </div>
          )}

          {/* Timestamp */}
          <div style={{
            borderTop: `1px solid ${GRAY}`,
            paddingTop: '8px',
            marginTop: '12px',
            fontSize: '8px',
            color: GRAY,
            textAlign: 'center'
          }}>
            Last Updated: {new Date(quote.timestamp * 1000).toLocaleString()}
          </div>
        </>
      )}
    </div>
  );
};

export default StockDetailPanel;
