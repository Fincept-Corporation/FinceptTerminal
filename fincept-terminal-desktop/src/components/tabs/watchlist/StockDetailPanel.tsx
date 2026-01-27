import React from 'react';
import { WatchlistStockWithQuote } from '../../../services/core/watchlistService';
import { FINCEPT, FONT_FAMILY, formatCurrency, formatPercent, formatVolume, getChangeColor } from './utils';
import { Eye, StickyNote, Clock } from 'lucide-react';

interface StockDetailPanelProps {
  stock: WatchlistStockWithQuote | null;
}

const StatRow: React.FC<{ label: string; value: string; color?: string }> = ({ label, value, color }) => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    padding: '6px 0',
    borderBottom: `1px solid ${FINCEPT.BORDER}`
  }}>
    <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>{label}</span>
    <span style={{ fontSize: '10px', color: color || FINCEPT.CYAN, fontWeight: 700 }}>{value}</span>
  </div>
);

const StockDetailPanel: React.FC<StockDetailPanelProps> = ({ stock }) => {
  if (!stock) {
    return (
      <div style={{
        width: '300px',
        backgroundColor: FINCEPT.PANEL_BG,
        overflow: 'auto',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        color: FINCEPT.MUTED,
        fontSize: '10px',
        textAlign: 'center',
        fontFamily: FONT_FAMILY,
        flexShrink: 0
      }}>
        <Eye size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
        <span>SELECT A STOCK TO VIEW DETAILS</span>
      </div>
    );
  }

  const quote = stock.quote;

  return (
    <div style={{
      width: '300px',
      backgroundColor: FINCEPT.PANEL_BG,
      overflow: 'auto',
      display: 'flex',
      flexDirection: 'column',
      fontFamily: FONT_FAMILY,
      flexShrink: 0
    }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
          STOCK DETAIL
        </span>
      </div>

      {/* Symbol & Price Hero */}
      <div style={{
        padding: '16px 12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{
          color: FINCEPT.CYAN,
          fontSize: '14px',
          fontWeight: 700,
          letterSpacing: '1px',
          marginBottom: '8px'
        }}>
          {stock.symbol}
        </div>

        {!quote ? (
          <div style={{
            padding: '12px',
            backgroundColor: `${FINCEPT.RED}20`,
            color: FINCEPT.RED,
            fontSize: '9px',
            fontWeight: 700,
            borderRadius: '2px',
            textAlign: 'center'
          }}>
            FAILED TO LOAD MARKET DATA
          </div>
        ) : (
          <>
            <div style={{
              color: FINCEPT.WHITE,
              fontSize: '24px',
              fontWeight: 700,
              marginBottom: '4px'
            }}>
              {formatCurrency(quote.price)}
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span style={{
                padding: '2px 6px',
                backgroundColor: `${getChangeColor(quote.change)}20`,
                color: getChangeColor(quote.change),
                fontSize: '10px',
                fontWeight: 700,
                borderRadius: '2px'
              }}>
                {formatPercent(quote.change_percent)}
              </span>
              <span style={{
                color: getChangeColor(quote.change),
                fontSize: '10px'
              }}>
                {formatCurrency(quote.change)}
              </span>
            </div>
          </>
        )}
      </div>

      {/* Trading Stats */}
      {quote && (
        <div style={{ padding: '12px' }}>
          <StatRow label="OPEN" value={formatCurrency(quote.open)} color={FINCEPT.WHITE} />
          <StatRow label="PREV CLOSE" value={formatCurrency(quote.previous_close)} color={FINCEPT.WHITE} />
          <StatRow label="DAY HIGH" value={formatCurrency(quote.high)} color={FINCEPT.GREEN} />
          <StatRow label="DAY LOW" value={formatCurrency(quote.low)} color={FINCEPT.RED} />
          <StatRow label="VOLUME" value={formatVolume(quote.volume)} color={FINCEPT.YELLOW} />

          {/* Day Range Visual */}
          {quote.high && quote.low && quote.price && (
            <div style={{ marginTop: '12px' }}>
              <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>
                DAY RANGE
              </div>
              <div style={{ position: 'relative', height: '4px', backgroundColor: FINCEPT.BORDER, borderRadius: '2px' }}>
                <div style={{
                  position: 'absolute',
                  left: '0',
                  right: '0',
                  height: '100%',
                  background: `linear-gradient(to right, ${FINCEPT.RED}, ${FINCEPT.GREEN})`,
                  borderRadius: '2px',
                  opacity: 0.5
                }} />
                <div style={{
                  position: 'absolute',
                  left: `${Math.min(100, Math.max(0, ((quote.price - quote.low) / (quote.high - quote.low)) * 100))}%`,
                  top: '-3px',
                  width: '10px',
                  height: '10px',
                  backgroundColor: FINCEPT.ORANGE,
                  borderRadius: '50%',
                  transform: 'translateX(-5px)',
                  boxShadow: `0 0 4px ${FINCEPT.ORANGE}80`
                }} />
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '4px' }}>
                <span style={{ fontSize: '8px', color: FINCEPT.RED }}>{formatCurrency(quote.low)}</span>
                <span style={{ fontSize: '8px', color: FINCEPT.GREEN }}>{formatCurrency(quote.high)}</span>
              </div>
            </div>
          )}
        </div>
      )}

      {/* Notes Section */}
      {stock.notes && (
        <div style={{
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          padding: '12px'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            marginBottom: '8px'
          }}>
            <StickyNote size={10} style={{ color: FINCEPT.YELLOW }} />
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              NOTES
            </span>
          </div>
          <div style={{
            fontSize: '10px',
            color: FINCEPT.WHITE,
            backgroundColor: FINCEPT.DARK_BG,
            padding: '8px',
            borderLeft: `2px solid ${FINCEPT.CYAN}`,
            borderRadius: '2px',
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-word',
            lineHeight: '1.5'
          }}>
            {stock.notes}
          </div>
        </div>
      )}

      {/* Timestamp */}
      {quote && (
        <div style={{
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          padding: '8px 12px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '4px',
          marginTop: 'auto'
        }}>
          <Clock size={10} style={{ color: FINCEPT.MUTED }} />
          <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
            {new Date(quote.timestamp * 1000).toLocaleString()}
          </span>
        </div>
      )}
    </div>
  );
};

export default StockDetailPanel;
