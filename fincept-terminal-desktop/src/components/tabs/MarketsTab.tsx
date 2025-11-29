import React, { useState, useEffect } from 'react';
import { Edit2 } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { marketDataService, QuoteData } from '../../services/marketDataService';
import { tickerStorage } from '../../services/tickerStorageService';
import { sqliteService } from '../../services/sqliteService';
import { contextRecorderService } from '../../services/contextRecorderService';
import TickerEditModal from './TickerEditModal';
import RecordingControlPanel from '../common/RecordingControlPanel';

const MarketsTab: React.FC = () => {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
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
  const [editingType, setEditingType] = useState<'global' | 'regional'>('global');

  // User preferences
  const [preferences, setPreferences] = useState(tickerStorage.loadPreferences());
  const [isRecording, setIsRecording] = useState(false);

  // Function to record current market data
  const recordCurrentData = async () => {
    if (Object.keys(marketData).length > 0 || Object.keys(regionalData).length > 0) {
      try {
        const allData = {
          globalMarkets: marketData,
          regionalMarkets: regionalData,
          timestamp: new Date().toISOString(),
          updateInterval: updateInterval
        };
        console.log('[MarketsTab] Recording current data:', allData);
        await contextRecorderService.recordApiResponse(
          'Markets',
          'market-data',
          allData,
          `Market Data (Snapshot) - ${new Date().toLocaleString()}`,
          ['markets', 'quotes', 'snapshot']
        );
        console.log('[MarketsTab] Current data recorded successfully');
      } catch (error) {
        console.error('[MarketsTab] Failed to record current data:', error);
      }
    }
  };

  // Fetch market data progressively (display as data arrives)
  const fetchMarketData = async () => {
    setIsUpdating(true);

    // Calculate cache age in minutes (10 minutes by default from updateInterval)
    const cacheAgeMinutes = updateInterval / 60000;

    // Create an array to track completion and capture data
    const allFetches: Promise<void>[] = [];
    const capturedGlobalData: Record<string, QuoteData[]> = {};
    const capturedRegionalData: Record<string, QuoteData[]> = {};

    // Fetch global markets progressively - each updates immediately
    preferences.globalMarkets.forEach((market) => {
      const fetchPromise = (async () => {
        try {
          // Use cached quotes with enhanced data (7D/30D returns)
          const quotes = await marketDataService.getEnhancedQuotesWithCache(
            market.tickers,
            market.category,
            cacheAgeMinutes
          );

          // Capture data for recording
          capturedGlobalData[market.category] = quotes;

          // Update state immediately as data arrives
          setMarketData(prev => ({
            ...prev,
            [market.category]: quotes
          }));
        } catch (error) {
          console.error(`Failed to fetch ${market.category}:`, error);
        }
      })();
      allFetches.push(fetchPromise);
    });

    // Fetch regional markets progressively
    preferences.regionalMarkets.forEach((market) => {
      const fetchPromise = (async () => {
        try {
          const symbols = market.tickers.map(t => t.symbol);
          // Use cached quotes with enhanced data (7D/30D returns)
          const quotes = await marketDataService.getEnhancedQuotesWithCache(
            symbols,
            market.region,
            cacheAgeMinutes
          );

          // Capture data for recording
          capturedRegionalData[market.region] = quotes;

          // Update state immediately as data arrives
          setRegionalData(prev => ({
            ...prev,
            [market.region]: quotes
          }));
        } catch (error) {
          console.error(`Failed to fetch ${market.region}:`, error);
        }
      })();
      allFetches.push(fetchPromise);
    });

    // Wait for all basic quotes to load
    await Promise.all(allFetches);
    setLastUpdate(new Date());
    setIsUpdating(false);

    // Record data if recording is active (use captured data, not state)
    if (isRecording) {
      try {
        const allData = {
          globalMarkets: capturedGlobalData,
          regionalMarkets: capturedRegionalData,
          timestamp: new Date().toISOString(),
          updateInterval: updateInterval
        };
        console.log('[MarketsTab] Recording data:', allData);
        await contextRecorderService.recordApiResponse(
          'Markets',
          'market-data',
          allData,
          `Market Data - ${new Date().toLocaleString()}`,
          ['markets', 'quotes', 'live-data']
        );
        console.log('[MarketsTab] Data recorded successfully');
      } catch (error) {
        console.error('[MarketsTab] Failed to record data:', error);
      }
    }
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

  // Open edit modal for global markets
  const openEditModal = (category: string) => {
    const tickers = tickerStorage.getCategoryTickers(category);
    setEditingCategory(category);
    setEditingTickers(tickers);
    setEditingType('global');
    setEditModalOpen(true);
  };

  // Open edit modal for regional markets
  const openRegionalEditModal = (region: string) => {
    const regionalTickers = tickerStorage.getRegionalTickers(region);
    const symbols = regionalTickers.map(t => t.symbol);
    setEditingCategory(region);
    setEditingTickers(symbols);
    setEditingType('regional');
    setEditModalOpen(true);
  };

  // Save edited tickers
  const handleSaveTickers = (tickers: string[]) => {
    if (editingType === 'global') {
      tickerStorage.updateCategoryTickers(editingCategory, tickers);
    } else {
      // For regional markets, convert back to {symbol, name} format
      const tickersWithNames = tickers.map(symbol => ({
        symbol,
        name: symbol // Use symbol as name for new tickers, actual names will be fetched from API
      }));
      tickerStorage.updateRegionalTickers(editingCategory, tickersWithNames);
    }
    setPreferences(tickerStorage.loadPreferences());
    fetchMarketData(); // Refresh data
  };

  // Format helpers
  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  // Create market panel
  const createMarketPanel = (title: string, quotes: QuoteData[]) => (
    <div style={{
      backgroundColor: colors.panel,
      border: `1px solid ${colors.textMuted}`,
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
        backgroundColor: colors.background,
        color: colors.primary,
        padding: '8px',
        fontSize: fontSize.subheading,
        fontWeight: 'bold',
        textAlign: 'center',
        borderBottom: `1px solid ${colors.textMuted}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        gap: '8px'
      }}>
        {title.toUpperCase()}
        <Edit2
          size={14}
          style={{ cursor: 'pointer', color: colors.text }}
          onClick={() => openEditModal(title)}
        />
      </div>

      {/* Table Header */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '2fr 1fr 1fr 1fr 1fr 1fr',
        gap: '4px',
        padding: '4px 8px',
        backgroundColor: colors.background,
        color: colors.text,
        fontSize: fontSize.body,
        fontWeight: 'bold',
        borderBottom: `1px solid ${colors.textMuted}`
      }}>
        <div>SYMBOL</div>
        <div style={{ textAlign: 'right' }}>PRICE</div>
        <div style={{ textAlign: 'right' }}>CHANGE</div>
        <div style={{ textAlign: 'right' }}>%CHG</div>
        <div style={{ textAlign: 'right' }}>HIGH</div>
        <div style={{ textAlign: 'right' }}>LOW</div>
      </div>

      {/* Data Rows */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {quotes.length === 0 ? (
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.body,
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
              fontSize: fontSize.small,
              backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.05)' : 'transparent',
              borderBottom: `1px solid rgba(120,120,120,0.3)`,
              minHeight: '18px',
              alignItems: 'center'
            }}>
              <div style={{ color: colors.text }}>{quote.symbol}</div>
              <div style={{ color: colors.text, textAlign: 'right' }}>{quote.price.toFixed(2)}</div>
              <div style={{
                color: quote.change >= 0 ? colors.secondary : colors.alert,
                textAlign: 'right'
              }}>{formatChange(quote.change)}</div>
              <div style={{
                color: quote.change_percent >= 0 ? colors.secondary : colors.alert,
                textAlign: 'right'
              }}>{formatPercent(quote.change_percent)}</div>
              <div style={{
                color: colors.text,
                textAlign: 'right'
              }}>{quote.high ? quote.high.toFixed(2) : '-'}</div>
              <div style={{
                color: colors.text,
                textAlign: 'right'
              }}>{quote.low ? quote.low.toFixed(2) : '-'}</div>
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
        backgroundColor: colors.panel,
        border: `1px solid ${colors.textMuted}`,
        flex: '1 1 calc(33.333% - 16px)',
        minWidth: '300px',
        maxWidth: '600px',
        height: '300px',
        margin: '8px',
        display: 'flex',
        flexDirection: 'column'
      }}>
        {/* Panel Header with Edit Icon */}
        <div style={{
          backgroundColor: colors.background,
          color: colors.primary,
          padding: '8px',
          fontSize: fontSize.subheading,
          fontWeight: 'bold',
          textAlign: 'center',
          borderBottom: `1px solid ${colors.textMuted}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '8px'
        }}>
          {title.toUpperCase()} - LIVE DATA
          <Edit2
            size={14}
            style={{ cursor: 'pointer', color: colors.text }}
            onClick={() => openRegionalEditModal(title)}
          />
        </div>

        {/* Table Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 2fr 1fr 1fr 1fr',
          gap: '4px',
          padding: '4px 8px',
          backgroundColor: colors.background,
          color: colors.text,
          fontSize: fontSize.body,
          fontWeight: 'bold',
          borderBottom: `1px solid ${colors.textMuted}`
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
              color: colors.textMuted,
              fontSize: fontSize.body,
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
                  fontSize: fontSize.small,
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.05)' : 'transparent',
                  borderBottom: `1px solid rgba(120,120,120,0.3)`,
                  minHeight: '18px',
                  alignItems: 'center'
                }}>
                  <div style={{ color: colors.text }}>{quote.symbol}</div>
                  <div style={{ color: colors.text, overflow: 'hidden', textOverflow: 'ellipsis' }}>
                    {tickerInfo?.name || quote.symbol}
                  </div>
                  <div style={{ color: colors.text, textAlign: 'right' }}>{quote.price.toFixed(2)}</div>
                  <div style={{
                    color: quote.change >= 0 ? colors.secondary : colors.alert,
                    textAlign: 'right'
                  }}>{formatChange(quote.change)}</div>
                  <div style={{
                    color: quote.change_percent >= 0 ? colors.secondary : colors.alert,
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
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily: fontFamily,
      fontWeight: fontWeight,
      fontStyle: fontStyle,
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
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        fontSize: '13px',
        gap: '8px',
        flexShrink: 0
      }}>
        <span style={{ color: colors.primary, fontWeight: 'bold' }}>FINCEPT</span>
        <span style={{ color: colors.text }}>MARKET TERMINAL - LIVE DATA</span>
        <span style={{ color: colors.textMuted }}>|</span>
        <span style={{ color: colors.text, fontSize: '11px' }}>
          {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
        </span>
      </div>

      {/* Control Panel */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexWrap: 'wrap',
        padding: '8px 12px',
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        fontSize: '12px',
        gap: '8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap' }}>
          <button
            onClick={fetchMarketData}
            style={{
              backgroundColor: colors.primary,
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
              backgroundColor: autoUpdate ? colors.secondary : colors.textMuted,
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
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              color: colors.text,
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
          <span style={{ color: colors.textMuted }}>|</span>
          <span style={{ color: colors.textMuted, fontSize: '11px' }}>LAST UPDATE:</span>
          <span style={{ color: colors.text, fontSize: '11px' }}>
            {lastUpdate.toTimeString().substring(0, 8)}
          </span>
          <span style={{ color: colors.textMuted }}>|</span>
          <span style={{ color: isUpdating ? colors.primary : colors.secondary, fontSize: '14px' }}>●</span>
          <span style={{ color: isUpdating ? colors.primary : colors.secondary, fontSize: '11px', fontWeight: 'bold' }}>
            {isUpdating ? 'UPDATING' : 'LIVE'}
          </span>
        </div>
        <RecordingControlPanel
          tabName="Markets"
          onRecordingChange={setIsRecording}
          onRecordingStart={recordCurrentData}
        />
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
          color: colors.primary,
          fontSize: '14px',
          fontWeight: 'bold',
          marginBottom: '8px',
          letterSpacing: '0.5px'
        }}>
          GLOBAL MARKETS
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '16px' }}></div>

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
          color: colors.primary,
          fontSize: '14px',
          fontWeight: 'bold',
          marginBottom: '8px',
          marginTop: '16px',
          letterSpacing: '0.5px'
        }}>
          REGIONAL MARKETS - LIVE DATA
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '16px' }}></div>

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
        borderTop: `1px solid ${colors.textMuted}`,
        backgroundColor: colors.panel,
        padding: '6px 12px',
        fontSize: '10px',
        color: colors.textMuted,
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
              <span style={{ marginLeft: '8px', color: colors.secondary }}>
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
