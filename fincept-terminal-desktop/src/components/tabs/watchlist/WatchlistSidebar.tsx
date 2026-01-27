import React from 'react';
import { useTranslation } from 'react-i18next';
import { Watchlist } from '../../../services/core/watchlistService';
import { FINCEPT, FONT_FAMILY } from './utils';
import { Trash2, Plus, TrendingUp, TrendingDown, BarChart2, List } from 'lucide-react';

interface WatchlistSidebarProps {
  watchlists: Array<Watchlist & { stock_count: number }>;
  selectedWatchlistId: string | null;
  onSelectWatchlist: (watchlistId: string) => void;
  onCreateWatchlist: () => void;
  onDeleteWatchlist: (watchlistId: string) => void;
  marketMovers?: {
    gainers: Array<{ symbol: string; quote: any }>;
    losers: Array<{ symbol: string; quote: any }>;
  };
  volumeLeaders?: Array<{ symbol: string; quote: any }>;
}

const WatchlistSidebar: React.FC<WatchlistSidebarProps> = ({
  watchlists,
  selectedWatchlistId,
  onSelectWatchlist,
  onCreateWatchlist,
  onDeleteWatchlist,
  marketMovers,
  volumeLeaders
}) => {
  const { t } = useTranslation('watchlist');

  const handleDelete = (e: React.MouseEvent, watchlistId: string) => {
    e.stopPropagation();
    onDeleteWatchlist(watchlistId);
  };

  return (
    <div style={{
      width: '280px',
      backgroundColor: FINCEPT.PANEL_BG,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontFamily: FONT_FAMILY,
      flexShrink: 0
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
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <List size={12} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            WATCHLISTS ({watchlists.length})
          </span>
        </div>
        <button
          onClick={onCreateWatchlist}
          style={{
            padding: '4px 8px',
            backgroundColor: FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG,
            border: 'none',
            borderRadius: '2px',
            fontSize: '8px',
            fontWeight: 700,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '2px'
          }}
        >
          <Plus size={10} /> NEW
        </button>
      </div>

      {/* Watchlist Items */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px 0' }}>
        {watchlists.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center',
            padding: '16px'
          }}>
            <List size={20} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>{t('sidebar.noWatchlists')}</span>
            <span style={{ marginTop: '4px', fontSize: '9px' }}>{t('sidebar.clickNew')}</span>
          </div>
        ) : (
          watchlists.map((watchlist) => (
            <div
              key={watchlist.id}
              style={{
                padding: '10px 12px',
                backgroundColor: selectedWatchlistId === watchlist.id ? `${FINCEPT.ORANGE}15` : 'transparent',
                borderLeft: selectedWatchlistId === watchlist.id
                  ? `2px solid ${FINCEPT.ORANGE}`
                  : '2px solid transparent',
                cursor: 'pointer',
                transition: 'all 0.2s',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}
              onClick={() => onSelectWatchlist(watchlist.id)}
              onMouseEnter={(e) => {
                if (selectedWatchlistId !== watchlist.id) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }
              }}
              onMouseLeave={(e) => {
                if (selectedWatchlistId !== watchlist.id) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }
              }}
            >
              <div>
                <div style={{
                  color: selectedWatchlistId === watchlist.id ? FINCEPT.WHITE : watchlist.color,
                  fontWeight: 700,
                  fontSize: '10px',
                  marginBottom: '2px',
                  letterSpacing: '0.3px'
                }}>
                  {watchlist.name}
                </div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
                  {watchlist.stock_count} {t('sidebar.stocks')}
                </div>
              </div>
              <button
                onClick={(e) => handleDelete(e, watchlist.id)}
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
                title={`Delete ${watchlist.name}`}
              >
                <Trash2 size={12} />
              </button>
            </div>
          ))
        )}
      </div>

      {/* Market Movers Section */}
      {marketMovers && marketMovers.gainers.length > 0 && (
        <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{
            padding: '8px 12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              MARKET MOVERS
            </span>
          </div>
          <div style={{ padding: '4px 8px' }}>
            {marketMovers.gainers.slice(0, 3).map((item, index) => (
              <div
                key={`gainer-${index}`}
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  padding: '4px 4px',
                  fontSize: '9px'
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <TrendingUp size={10} style={{ color: FINCEPT.GREEN }} />
                  <span style={{ color: FINCEPT.CYAN }}>{item.symbol}</span>
                </div>
                <span style={{ color: FINCEPT.GREEN, fontWeight: 700 }}>
                  +{item.quote?.change_percent.toFixed(2)}%
                </span>
              </div>
            ))}
            {marketMovers.losers.slice(0, 2).map((item, index) => (
              <div
                key={`loser-${index}`}
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  padding: '4px 4px',
                  fontSize: '9px'
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <TrendingDown size={10} style={{ color: FINCEPT.RED }} />
                  <span style={{ color: FINCEPT.CYAN }}>{item.symbol}</span>
                </div>
                <span style={{ color: FINCEPT.RED, fontWeight: 700 }}>
                  {item.quote?.change_percent.toFixed(2)}%
                </span>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Volume Leaders Section */}
      {volumeLeaders && volumeLeaders.length > 0 && (
        <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{
            padding: '8px 12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              VOLUME LEADERS
            </span>
          </div>
          <div style={{ padding: '4px 8px 8px' }}>
            {volumeLeaders.slice(0, 4).map((item, index) => {
              const volume = item.quote?.volume || 0;
              const volumeStr = volume >= 1_000_000
                ? `${(volume / 1_000_000).toFixed(1)}M`
                : volume >= 1_000
                  ? `${(volume / 1_000).toFixed(1)}K`
                  : volume.toString();

              return (
                <div
                  key={`volume-${index}`}
                  style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    padding: '4px 4px',
                    fontSize: '9px'
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <BarChart2 size={10} style={{ color: FINCEPT.YELLOW }} />
                    <span style={{ color: FINCEPT.CYAN }}>{item.symbol}</span>
                  </div>
                  <span style={{ color: FINCEPT.WHITE, fontWeight: 700 }}>{volumeStr}</span>
                </div>
              );
            })}
          </div>
        </div>
      )}
    </div>
  );
};

export default WatchlistSidebar;
