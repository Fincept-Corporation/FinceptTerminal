import React, { useState, useEffect } from 'react';
import { Edit2 } from 'lucide-react';
import { marketDataService, QuoteData } from '../../services/marketDataService';
import { tickerStorage } from '../../services/tickerStorageService';
import { sqliteService } from '../../services/sqliteService';
import TickerEditModal from './TickerEditModal';

const MarketsTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [lastUpdate, setLastUpdate] = useState(new Date());
  const [isUpdating, setIsUpdating] = useState(false);
  const [autoUpdate, setAutoUpdate] = useState(true);
  const [updateInterval, setUpdateInterval] = useState(600000); // 10 minutes default
  const [dbInitialized, setDbInitialized] = useState(false);

  // Market data state
  const [marketData, setMarketData] = useState<Record<string, QuoteData[]>>({});
  const [regionalData, setRegionalData] = useState<Record<string, QuoteData[]>>({});

  // Edit modal state
  const [editModalOpen, setEditModalOpen] = useState(false);
  const [editingCategory, setEditingCategory] = useState<string>('');
  const [editingTickers, setEditingTickers] = useState<string[]>([]);

  // User preferences
  const [preferences, setPreferences] = useState(tickerStorage.loadPreferences());

  // Bloomberg colors
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#1a1a1a';
  const BLOOMBERG_PANEL_BG = '#000000';

  // Fetch market data
  const fetchMarketData = async () => {
    setIsUpdating(true);

    // Calculate cache age in minutes (10 minutes by default from updateInterval)
    const cacheAgeMinutes = updateInterval / 60000;

    // Fetch global markets with caching
    const globalPromises = preferences.globalMarkets.map(async (market) => {
      const quotes = await marketDataService.getEnhancedQuotesWithCache(
        market.tickers,
        market.category,
        cacheAgeMinutes
      );
      return { category: market.category, quotes };
    });

    // Fetch regional markets with caching
    const regionalPromises = preferences.regionalMarkets.map(async (market) => {
      const symbols = market.tickers.map(t => t.symbol);
      const quotes = await marketDataService.getEnhancedQuotesWithCache(
        symbols,
        market.region,
        cacheAgeMinutes
      );
      return { region: market.region, quotes };
    });

    const globalResults = await Promise.all(globalPromises);
    const regionalResults = await Promise.all(regionalPromises);

    // Update state
    const newMarketData: Record<string, QuoteData[]> = {};
    globalResults.forEach(({ category, quotes }) => {
      newMarketData[category] = quotes;
    });
    setMarketData(newMarketData);

    const newRegionalData: Record<string, QuoteData[]> = {};
    regionalResults.forEach(({ region, quotes }) => {
      newRegionalData[region] = quotes;
    });
    setRegionalData(newRegionalData);

    setLastUpdate(new Date());
    setIsUpdating(false);
  };

  // Initialize database on mount
  useEffect(() => {
    const initDatabase = async () => {
      try {
        await sqliteService.initialize();
        const healthCheck = await sqliteService.healthCheck();
        if (healthCheck.healthy) {
          setDbInitialized(true);
          console.log('[MarketsTab] Database initialized and ready for caching');
        } else {
          console.warn('[MarketsTab] Database not healthy:', healthCheck.message);
        }
      } catch (error) {
        console.error('[MarketsTab] Database initialization error:', error);
      }
    };
    initDatabase();
  }, []);

  // Initial fetch
  useEffect(() => {
    fetchMarketData();
  }, []);

  // Auto-update timer
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
      if (autoUpdate) {
        fetchMarketData();
      }
    }, updateInterval);

    return () => clearInterval(timer);
  }, [autoUpdate, updateInterval, preferences]);

  // Open edit modal
  const openEditModal = (category: string) => {
    const tickers = tickerStorage.getCategoryTickers(category);
    setEditingCategory(category);
    setEditingTickers(tickers);
    setEditModalOpen(true);
  };

  // Save edited tickers
  const handleSaveTickers = (tickers: string[]) => {
    tickerStorage.updateCategoryTickers(editingCategory, tickers);
    setPreferences(tickerStorage.loadPreferences());
    fetchMarketData(); // Refresh data
  };

  // Format helpers
  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  // Create market panel
  const createMarketPanel = (title: string, quotes: QuoteData[]) => (
    <div style={{
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      flex: '1 1 calc(33.333% - 16px)',
      minWidth: '300px',
      maxWidth: '600px',
      height: '280px',
      margin: '8px',
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Panel Header with Edit Icon */}
      <div style={{
        backgroundColor: BLOOMBERG_DARK_BG,
        color: BLOOMBERG_ORANGE,
        padding: '8px',
        fontSize: '14px',
        fontWeight: 'bold',
        textAlign: 'center',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        gap: '8px'
      }}>
        {title.toUpperCase()}
        <Edit2
          size={14}
          style={{ cursor: 'pointer', color: BLOOMBERG_WHITE }}
          onClick={() => openEditModal(title)}
        />
      </div>

      {/* Table Header */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '2fr 1fr 1fr 1fr 1fr 1fr',
        gap: '4px',
        padding: '4px 8px',
        backgroundColor: BLOOMBERG_DARK_BG,
        color: BLOOMBERG_WHITE,
        fontSize: '12px',
        fontWeight: 'bold',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        <div>SYMBOL</div>
        <div style={{ textAlign: 'right' }}>PRICE</div>
        <div style={{ textAlign: 'right' }}>CHANGE</div>
        <div style={{ textAlign: 'right' }}>%CHG</div>
        <div style={{ textAlign: 'right' }}>7D</div>
        <div style={{ textAlign: 'right' }}>30D</div>
      </div>

      {/* Data Rows */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {quotes.length === 0 ? (
          <div style={{
            color: BLOOMBERG_GRAY,
            fontSize: '11px',
            textAlign: 'center',
            padding: '16px'
          }}>
            {isUpdating ? 'Loading...' : 'No data available'}
          </div>
        ) : (
          quotes.map((quote, index) => (
            <div key={index} style={{
              display: 'grid',
              gridTemplateColumns: '2fr 1fr 1fr 1fr 1fr 1fr',
              gap: '4px',
              padding: '2px 8px',
              fontSize: '10px',
              backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.05)' : 'transparent',
              borderBottom: `1px solid rgba(120,120,120,0.3)`,
              minHeight: '18px',
              alignItems: 'center'
            }}>
              <div style={{ color: BLOOMBERG_WHITE }}>{quote.symbol}</div>
              <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>{quote.price.toFixed(2)}</div>
              <div style={{
                color: quote.change >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                textAlign: 'right'
              }}>{formatChange(quote.change)}</div>
              <div style={{
                color: quote.change_percent >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                textAlign: 'right'
              }}>{formatPercent(quote.change_percent)}</div>
              <div style={{
                color: (quote as any).seven_day >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                textAlign: 'right'
              }}>{(quote as any).seven_day ? formatPercent((quote as any).seven_day) : '-'}</div>
              <div style={{
                color: (quote as any).thirty_day >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                textAlign: 'right'
              }}>{(quote as any).thirty_day ? formatPercent((quote as any).thirty_day) : '-'}</div>
            </div>
          ))
        )}
      </div>
    </div>
  );

  // Create regional panel
  const createRegionalPanel = (title: string, quotes: QuoteData[]) => {
    const regionConfig = preferences.regionalMarkets.find(r => r.region === title);

    return (
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `1px solid ${BLOOMBERG_GRAY}`,
        flex: '1 1 calc(33.333% - 16px)',
        minWidth: '300px',
        maxWidth: '600px',
        height: '300px',
        margin: '8px',
        display: 'flex',
        flexDirection: 'column'
      }}>
        {/* Panel Header */}
        <div style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          color: BLOOMBERG_ORANGE,
          padding: '8px',
          fontSize: '14px',
          fontWeight: 'bold',
          textAlign: 'center',
          borderBottom: `1px solid ${BLOOMBERG_GRAY}`
        }}>
          {title.toUpperCase()} - LIVE DATA
        </div>

        {/* Table Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 2fr 1fr 1fr 1fr',
          gap: '4px',
          padding: '4px 8px',
          backgroundColor: BLOOMBERG_DARK_BG,
          color: BLOOMBERG_WHITE,
          fontSize: '12px',
          fontWeight: 'bold',
          borderBottom: `1px solid ${BLOOMBERG_GRAY}`
        }}>
          <div>SYMBOL</div>
          <div>NAME</div>
          <div style={{ textAlign: 'right' }}>PRICE</div>
          <div style={{ textAlign: 'right' }}>CHANGE</div>
          <div style={{ textAlign: 'right' }}>%CHG</div>
        </div>

        {/* Data Rows */}
        <div style={{ flex: 1, overflow: 'auto' }}>
          {quotes.length === 0 ? (
            <div style={{
              color: BLOOMBERG_GRAY,
              fontSize: '11px',
              textAlign: 'center',
              padding: '16px'
            }}>
              {isUpdating ? 'Loading...' : 'No data available'}
            </div>
          ) : (
            quotes.map((quote, index) => {
              const tickerInfo = regionConfig?.tickers.find(t => t.symbol === quote.symbol);
              return (
                <div key={index} style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 2fr 1fr 1fr 1fr',
                  gap: '4px',
                  padding: '2px 8px',
                  fontSize: '10px',
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.05)' : 'transparent',
                  borderBottom: `1px solid rgba(120,120,120,0.3)`,
                  minHeight: '18px',
                  alignItems: 'center'
                }}>
                  <div style={{ color: BLOOMBERG_WHITE }}>{quote.symbol}</div>
                  <div style={{ color: BLOOMBERG_WHITE, overflow: 'hidden', textOverflow: 'ellipsis' }}>
                    {tickerInfo?.name || quote.symbol}
                  </div>
                  <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>{quote.price.toFixed(2)}</div>
                  <div style={{
                    color: quote.change >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                    textAlign: 'right'
                  }}>{formatChange(quote.change)}</div>
                  <div style={{
                    color: quote.change_percent >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                    textAlign: 'right'
                  }}>{formatPercent(quote.change_percent)}</div>
                </div>
              );
            })
          )}
        </div>
      </div>
    );
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        *::-webkit-scrollbar-track {
          background: #1a1a1a;
        }
        *::-webkit-scrollbar-thumb {
          background: #404040;
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: #525252;
        }
      `}</style>

      {/* Header Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        flexWrap: 'wrap',
        padding: '8px 12px',
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        fontSize: '13px',
        gap: '8px',
        flexShrink: 0
      }}>
        <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>FINCEPT</span>
        <span style={{ color: BLOOMBERG_WHITE }}>MARKET TERMINAL - LIVE DATA</span>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <span style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>
          {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
        </span>
      </div>

      {/* Control Panel */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        flexWrap: 'wrap',
        padding: '8px 12px',
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        fontSize: '12px',
        gap: '8px',
        flexShrink: 0
      }}>
        <button
          onClick={fetchMarketData}
          style={{
            backgroundColor: BLOOMBERG_ORANGE,
            color: 'black',
            border: 'none',
            padding: '4px 12px',
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: 'pointer'
          }}
        >
          REFRESH
        </button>
        <button
          onClick={() => setAutoUpdate(!autoUpdate)}
          style={{
            backgroundColor: autoUpdate ? BLOOMBERG_GREEN : BLOOMBERG_GRAY,
            color: 'black',
            border: 'none',
            padding: '4px 12px',
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: 'pointer'
          }}
        >
          AUTO {autoUpdate ? 'ON' : 'OFF'}
        </button>
        <select
          value={updateInterval}
          onChange={(e) => setUpdateInterval(Number(e.target.value))}
          style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '4px 8px',
            fontSize: '11px',
            cursor: 'pointer'
          }}
        >
          <option value={600000}>10 min</option>
          <option value={900000}>15 min</option>
          <option value={1800000}>30 min</option>
          <option value={3600000}>1 hour</option>
        </select>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <span style={{ color: BLOOMBERG_GRAY, fontSize: '11px' }}>LAST UPDATE:</span>
        <span style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>
          {lastUpdate.toTimeString().substring(0, 8)}
        </span>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <span style={{ color: isUpdating ? BLOOMBERG_ORANGE : BLOOMBERG_GREEN, fontSize: '14px' }}>●</span>
        <span style={{ color: isUpdating ? BLOOMBERG_ORANGE : BLOOMBERG_GREEN, fontSize: '11px', fontWeight: 'bold' }}>
          {isUpdating ? 'UPDATING' : 'LIVE'}
        </span>
      </div>

      {/* Main Content */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '12px',
        display: 'flex',
        flexDirection: 'column'
      }}>
        {/* Global Markets */}
        <div style={{
          color: BLOOMBERG_ORANGE,
          fontSize: '14px',
          fontWeight: 'bold',
          marginBottom: '8px',
          letterSpacing: '0.5px'
        }}>
          GLOBAL MARKETS
        </div>
        <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '16px' }}></div>

        <div style={{
          display: 'flex',
          flexWrap: 'wrap',
          gap: '0',
          marginBottom: '24px'
        }}>
          {preferences.globalMarkets.map(market =>
            createMarketPanel(market.category, marketData[market.category] || [])
          )}
        </div>

        {/* Regional Markets */}
        <div style={{
          color: BLOOMBERG_ORANGE,
          fontSize: '14px',
          fontWeight: 'bold',
          marginBottom: '8px',
          marginTop: '16px',
          letterSpacing: '0.5px'
        }}>
          REGIONAL MARKETS - LIVE DATA
        </div>
        <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '16px' }}></div>

        <div style={{
          display: 'flex',
          flexWrap: 'wrap',
          gap: '0'
        }}>
          {preferences.regionalMarkets.map(market =>
            createRegionalPanel(market.region, regionalData[market.region] || [])
          )}
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${BLOOMBERG_GRAY}`,
        backgroundColor: BLOOMBERG_PANEL_BG,
        padding: '6px 12px',
        fontSize: '10px',
        color: BLOOMBERG_GRAY,
        flexShrink: 0
      }}>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexWrap: 'wrap',
          gap: '8px'
        }}>
          <span>Data provided by Yahoo Finance API | Real-time updates</span>
          <span style={{ whiteSpace: 'nowrap' }}>
            Connected: {Object.keys(regionalData).length} regional markets
            {dbInitialized && (
              <span style={{ marginLeft: '8px', color: BLOOMBERG_GREEN }}>
                | Cache: ENABLED
              </span>
            )}
          </span>
        </div>
      </div>

      {/* Edit Modal */}
      <TickerEditModal
        isOpen={editModalOpen}
        onClose={() => setEditModalOpen(false)}
        title={editingCategory}
        tickers={editingTickers}
        onSave={handleSaveTickers}
      />
    </div>
  );
};

export default MarketsTab;
