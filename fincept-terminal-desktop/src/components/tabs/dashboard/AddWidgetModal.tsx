import React, { useState, useEffect } from 'react';
import { X } from 'lucide-react';
import { WidgetType, DEFAULT_WIDGET_CONFIGS } from './widgets';
import { watchlistService } from '../../../services/watchlistService';
import { getAllDataSources } from '../../../services/dataSourceRegistry';
import { DataSource } from '../../../services/sqliteService';

const BLOOMBERG_ORANGE = '#FFA500';
const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_DARK_BG = '#000000';
const BLOOMBERG_PANEL_BG = '#0a0a0a';

interface AddWidgetModalProps {
  isOpen: boolean;
  onClose: () => void;
  onAdd: (widgetType: WidgetType, config?: any) => void;
}

export const AddWidgetModal: React.FC<AddWidgetModalProps> = ({
  isOpen,
  onClose,
  onAdd
}) => {
  const [selectedType, setSelectedType] = useState<WidgetType>('news');
  const [watchlists, setWatchlists] = useState<any[]>([]);
  const [dataSources, setDataSources] = useState<DataSource[]>([]);

  // Widget-specific configs
  const [newsCategory, setNewsCategory] = useState('ALL');
  const [marketCategory, setMarketCategory] = useState('Indices');
  const [selectedWatchlist, setSelectedWatchlist] = useState('');
  const [forumCategory, setForumCategory] = useState('Trending');
  const [selectedDataSource, setSelectedDataSource] = useState('');

  useEffect(() => {
    if (isOpen && selectedType === 'watchlist') {
      loadWatchlists();
    }
    if (isOpen && selectedType === 'datasource') {
      loadDataSources();
    }
  }, [isOpen, selectedType]);

  const loadWatchlists = async () => {
    try {
      await watchlistService.initialize();
      const wls = await watchlistService.getWatchlistsWithCounts();
      setWatchlists(wls);
      if (wls.length > 0) {
        setSelectedWatchlist(wls[0].id);
      }
    } catch (error) {
      console.error('Failed to load watchlists:', error);
    }
  };

  const loadDataSources = async () => {
    try {
      const sources = await getAllDataSources();
      const enabled = sources.filter(s => s.enabled);
      setDataSources(enabled);
      if (enabled.length > 0) {
        setSelectedDataSource(enabled[0].alias);
      }
    } catch (error) {
      console.error('Failed to load data sources:', error);
    }
  };

  const handleAdd = () => {
    let config = {};

    switch (selectedType) {
      case 'news':
        config = { newsCategory, newsLimit: 5 };
        break;
      case 'market':
        const tickers = marketCategory === 'Indices'
          ? ['^GSPC', '^IXIC', '^DJI', '^RUT']
          : marketCategory === 'Tech'
          ? ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA']
          : ['GC=F', 'CL=F', 'SI=F'];
        config = { marketCategory, marketTickers: tickers };
        break;
      case 'watchlist':
        const wl = watchlists.find(w => w.id === selectedWatchlist);
        config = {
          watchlistId: selectedWatchlist,
          watchlistName: wl?.name || 'Watchlist'
        };
        break;
      case 'forum':
        config = { forumCategoryName: forumCategory, forumLimit: 5 };
        break;
      case 'datasource':
        const ds = dataSources.find(d => d.alias === selectedDataSource);
        config = {
          dataSourceAlias: selectedDataSource,
          dataSourceDisplayName: ds?.display_name
        };
        break;
    }

    onAdd(selectedType, config);
    onClose();
  };

  if (!isOpen) return null;

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        borderRadius: '4px',
        width: '500px',
        maxHeight: '80vh',
        overflow: 'auto',
        boxShadow: `0 0 20px ${BLOOMBERG_ORANGE}`
      }}>
        {/* Header */}
        <div style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          borderBottom: `2px solid ${BLOOMBERG_ORANGE}`,
          padding: '12px 16px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <div style={{
            color: BLOOMBERG_ORANGE,
            fontSize: '14px',
            fontWeight: 'bold'
          }}>
            ADD WIDGET
          </div>
          <button
            onClick={onClose}
            style={{
              background: 'none',
              border: 'none',
              color: BLOOMBERG_GRAY,
              cursor: 'pointer',
              padding: '0',
              display: 'flex'
            }}
          >
            <X size={20} />
          </button>
        </div>

        {/* Content */}
        <div style={{ padding: '16px' }}>
          {/* Widget Type Selection */}
          <div style={{ marginBottom: '16px' }}>
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', marginBottom: '8px', fontWeight: 'bold' }}>
              WIDGET TYPE
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
              {(['news', 'market', 'watchlist', 'forum', 'crypto', 'commodities', 'indices', 'forex', 'maritime', 'datasource'] as WidgetType[]).map(type => (
                <button
                  key={type}
                  onClick={() => setSelectedType(type)}
                  style={{
                    backgroundColor: selectedType === type ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                    color: selectedType === type ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                    border: `1px solid ${selectedType === type ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY}`,
                    padding: '8px',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    textTransform: 'uppercase'
                  }}
                >
                  {type}
                </button>
              ))}
            </div>
          </div>

          {/* Widget Configuration */}
          <div style={{ marginBottom: '16px' }}>
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', marginBottom: '8px', fontWeight: 'bold' }}>
              CONFIGURATION
            </div>

            {/* News Config */}
            {selectedType === 'news' && (
              <select
                value={newsCategory}
                onChange={(e) => setNewsCategory(e.target.value)}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
              >
                <option value="ALL">All News</option>
                <option value="MARKETS">Markets</option>
                <option value="TECH">Technology</option>
                <option value="EARNINGS">Earnings</option>
                <option value="ECONOMIC">Economic</option>
              </select>
            )}

            {/* Market Config */}
            {selectedType === 'market' && (
              <select
                value={marketCategory}
                onChange={(e) => setMarketCategory(e.target.value)}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
              >
                <option value="Indices">Market Indices</option>
                <option value="Tech">Tech Stocks</option>
                <option value="Commodities">Commodities</option>
              </select>
            )}

            {/* Watchlist Config */}
            {selectedType === 'watchlist' && (
              watchlists.length > 0 ? (
                <select
                  value={selectedWatchlist}
                  onChange={(e) => setSelectedWatchlist(e.target.value)}
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '8px',
                    fontSize: '11px'
                  }}
                >
                  {watchlists.map(wl => (
                    <option key={wl.id} value={wl.id}>{wl.name}</option>
                  ))}
                </select>
              ) : (
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', padding: '8px' }}>
                  No watchlists available. Create one in the Watchlist tab first.
                </div>
              )
            )}

            {/* Forum Config */}
            {selectedType === 'forum' && (
              <select
                value={forumCategory}
                onChange={(e) => setForumCategory(e.target.value)}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
              >
                <option value="Trending">Trending Posts</option>
                <option value="Recent">Recent Posts</option>
              </select>
            )}

            {/* Data Source Config */}
            {selectedType === 'datasource' && (
              dataSources.length > 0 ? (
                <select
                  value={selectedDataSource}
                  onChange={(e) => setSelectedDataSource(e.target.value)}
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '8px',
                    fontSize: '11px'
                  }}
                >
                  {dataSources.map(ds => (
                    <option key={ds.alias} value={ds.alias}>
                      {ds.display_name} ({ds.alias}) - {ds.type === 'websocket' ? 'WebSocket' : 'REST API'}
                    </option>
                  ))}
                </select>
              ) : (
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', padding: '8px' }}>
                  No data sources available. Create one in Settings → Data Sources first.
                </div>
              )
            )}

            {/* Crypto, Commodities, Indices, Forex, Maritime - No Config */}
            {['crypto', 'commodities', 'indices', 'forex', 'maritime'].includes(selectedType) && (
              <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', padding: '8px', fontStyle: 'italic' }}>
                This widget shows live {selectedType === 'maritime' ? 'maritime intelligence' : selectedType} data with automatic updates. No configuration required.
              </div>
            )}
          </div>

          {/* Action Buttons */}
          <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
            <button
              onClick={onClose}
              style={{
                backgroundColor: BLOOMBERG_GRAY,
                color: BLOOMBERG_DARK_BG,
                border: 'none',
                padding: '8px 16px',
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                borderRadius: '2px'
              }}
            >
              CANCEL
            </button>
            <button
              onClick={handleAdd}
              disabled={(selectedType === 'watchlist' && watchlists.length === 0) || (selectedType === 'datasource' && dataSources.length === 0)}
              style={{
                backgroundColor: ((selectedType === 'watchlist' && watchlists.length === 0) || (selectedType === 'datasource' && dataSources.length === 0)) ? BLOOMBERG_GRAY : BLOOMBERG_ORANGE,
                color: BLOOMBERG_DARK_BG,
                border: 'none',
                padding: '8px 16px',
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: ((selectedType === 'watchlist' && watchlists.length === 0) || (selectedType === 'datasource' && dataSources.length === 0)) ? 'not-allowed' : 'pointer',
                borderRadius: '2px'
              }}
            >
              ADD WIDGET
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
