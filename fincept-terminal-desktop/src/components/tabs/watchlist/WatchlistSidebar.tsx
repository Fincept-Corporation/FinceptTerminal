import React from 'react';
import { Watchlist } from '../../../services/watchlistService';
import { BLOOMBERG_COLORS } from './utils';
import { Trash2, Plus } from 'lucide-react';

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
  const { ORANGE, WHITE, GRAY, GREEN, RED, CYAN, YELLOW, DARK_BG, PANEL_BG } = BLOOMBERG_COLORS;

  const handleDelete = (e: React.MouseEvent, watchlistId: string) => {
    e.stopPropagation();
    onDeleteWatchlist(watchlistId);
  };

  return (
    <div style={{
      width: '280px',
      backgroundColor: PANEL_BG,
      border: `1px solid ${GRAY}`,
      padding: '4px',
      overflow: 'auto',
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Watchlist Groups */}
      <div style={{
        color: ORANGE,
        fontSize: '11px',
        fontWeight: 'bold',
        marginBottom: '4px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <span>WATCHLIST GROUPS ({watchlists.length})</span>
        <button
          onClick={onCreateWatchlist}
          style={{
            background: 'transparent',
            border: `1px solid ${ORANGE}`,
            color: ORANGE,
            padding: '2px 4px',
            fontSize: '8px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '2px'
          }}
          title="Create new watchlist"
        >
          <Plus size={10} />
          NEW
        </button>
      </div>
      <div style={{ borderBottom: `1px solid ${GRAY}`, marginBottom: '8px' }}></div>

      <div style={{ flex: 1, overflow: 'auto', marginBottom: '8px' }}>
        {watchlists.length === 0 ? (
          <div style={{
            padding: '16px',
            textAlign: 'center',
            color: GRAY,
            fontSize: '10px'
          }}>
            No watchlists yet.
            <br />
            Click NEW to create one.
          </div>
        ) : (
          watchlists.map((watchlist) => (
            <div
              key={watchlist.id}
              style={{
                width: '100%',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                padding: '8px',
                marginBottom: '3px',
                backgroundColor: selectedWatchlistId === watchlist.id
                  ? 'rgba(255,165,0,0.1)'
                  : 'rgba(255,255,255,0.02)',
                border: `1px solid ${selectedWatchlistId === watchlist.id ? ORANGE : 'transparent'}`,
                cursor: 'pointer',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace',
                transition: 'all 0.2s'
              }}
              onClick={() => onSelectWatchlist(watchlist.id)}
              onMouseEnter={(e) => {
                if (selectedWatchlistId !== watchlist.id) {
                  e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.05)';
                }
              }}
              onMouseLeave={(e) => {
                if (selectedWatchlistId !== watchlist.id) {
                  e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.02)';
                }
              }}
            >
              <div style={{ flex: 1 }}>
                <div style={{
                  color: watchlist.color,
                  fontWeight: 'bold',
                  marginBottom: '2px'
                }}>
                  {watchlist.name}
                </div>
                <div style={{
                  color: CYAN,
                  fontSize: '9px'
                }}>
                  {watchlist.stock_count} stocks
                </div>
              </div>
              <button
                onClick={(e) => handleDelete(e, watchlist.id)}
                style={{
                  background: 'transparent',
                  border: 'none',
                  color: RED,
                  cursor: 'pointer',
                  padding: '2px',
                  display: 'flex'
                }}
                title={`Delete ${watchlist.name}`}
              >
                <Trash2 size={12} />
              </button>
            </div>
          ))
        )}
      </div>

      {/* Market Movers */}
      {marketMovers && marketMovers.gainers.length > 0 && (
        <div style={{
          borderTop: `1px solid ${GRAY}`,
          marginTop: '8px',
          paddingTop: '8px',
          marginBottom: '8px'
        }}>
          <div style={{
            color: YELLOW,
            fontSize: '10px',
            fontWeight: 'bold',
            marginBottom: '4px'
          }}>
            MARKET MOVERS
          </div>
          {marketMovers.gainers.slice(0, 3).map((item, index) => (
            <div
              key={`gainer-${index}`}
              style={{
                display: 'flex',
                justifyContent: 'space-between',
                padding: '3px',
                marginBottom: '2px',
                backgroundColor: 'rgba(255,255,255,0.02)',
                fontSize: '9px'
              }}
            >
              <div>
                <span style={{ color: CYAN }}>{item.symbol}</span>
                <span style={{
                  color: GRAY,
                  marginLeft: '4px',
                  fontSize: '8px'
                }}>
                  [GAINER]
                </span>
              </div>
              <span style={{
                color: GREEN,
                fontWeight: 'bold'
              }}>
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
                padding: '3px',
                marginBottom: '2px',
                backgroundColor: 'rgba(255,255,255,0.02)',
                fontSize: '9px'
              }}
            >
              <div>
                <span style={{ color: CYAN }}>{item.symbol}</span>
                <span style={{
                  color: GRAY,
                  marginLeft: '4px',
                  fontSize: '8px'
                }}>
                  [LOSER]
                </span>
              </div>
              <span style={{
                color: RED,
                fontWeight: 'bold'
              }}>
                {item.quote?.change_percent.toFixed(2)}%
              </span>
            </div>
          ))}
        </div>
      )}

      {/* Volume Leaders */}
      {volumeLeaders && volumeLeaders.length > 0 && (
        <div style={{
          borderTop: `1px solid ${GRAY}`,
          marginTop: '8px',
          paddingTop: '8px'
        }}>
          <div style={{
            color: YELLOW,
            fontSize: '10px',
            fontWeight: 'bold',
            marginBottom: '4px'
          }}>
            VOLUME LEADERS
          </div>
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
                  padding: '3px',
                  marginBottom: '2px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  fontSize: '9px'
                }}
              >
                <span style={{ color: CYAN }}>{item.symbol}</span>
                <span style={{ color: WHITE }}>{volumeStr}</span>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
};

export default WatchlistSidebar;
