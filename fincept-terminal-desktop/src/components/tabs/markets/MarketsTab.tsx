import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { Edit2, Info, RefreshCw } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { marketDataService, QuoteData } from '@/services/markets/marketDataService';
import { tickerStorage, UserMarketPreferences } from '@/services/core/tickerStorageService';
import { sqliteService } from '@/services/core/sqliteService';
import { contextRecorderService } from '@/services/data-sources/contextRecorderService';
import { useCache } from '@/hooks/useCache';
import TickerEditModal from '@/components/tabs/ticker-edit-modal';
import RecordingControlPanel from '@/components/common/RecordingControlPanel';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';
import { createMarketsTabTour } from '@/components/tabs/tours/marketsTabTour';

// Hook to fetch market data for a category
// Cache key includes ticker count and first/last ticker to detect changes
const useMarketCategory = (
  category: string,
  tickers: string[],
  enabled: boolean,
  refetchInterval: number
) => {
  // Create a cache key that changes when tickers change
  const tickerHash = useMemo(() => {
    if (tickers.length === 0) return 'empty';
    return `${tickers.length}:${tickers[0]}:${tickers[tickers.length - 1]}`;
  }, [tickers]);

  return useCache<QuoteData[]>({
    key: `markets:global:${category}:${tickerHash}`,
    category: 'market-quotes',
    fetcher: () => marketDataService.getQuotes(tickers),
    ttl: '10m',
    enabled: enabled && tickers.length > 0,
    refetchInterval,
    staleWhileRevalidate: true
  });
};

// Hook to fetch regional market data
const useRegionalMarket = (
  region: string,
  tickers: Array<{ symbol: string; name: string }>,
  enabled: boolean,
  refetchInterval: number
) => {
  const symbols = useMemo(() => tickers.map(t => t.symbol), [tickers]);

  // Create a cache key that changes when tickers change
  const tickerHash = useMemo(() => {
    if (symbols.length === 0) return 'empty';
    return `${symbols.length}:${symbols[0]}:${symbols[symbols.length - 1]}`;
  }, [symbols]);

  return useCache<QuoteData[]>({
    key: `markets:regional:${region}:${tickerHash}`,
    category: 'market-quotes',
    fetcher: () => marketDataService.getQuotes(symbols),
    ttl: '10m',
    enabled: enabled && symbols.length > 0,
    refetchInterval,
    staleWhileRevalidate: true
  });
};

const MarketsTab: React.FC = () => {
  const { t } = useTranslation('markets');
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [autoUpdate, setAutoUpdate] = useState(true);
  const [updateInterval, setUpdateInterval] = useState(600000); // 10 minutes default
  const [dbInitialized, setDbInitialized] = useState(false);
  const [preferencesLoaded, setPreferencesLoaded] = useState(false);

  // Edit modal state
  const [editModalOpen, setEditModalOpen] = useState(false);
  const [editingCategory, setEditingCategory] = useState<string>('');
  const [editingTickers, setEditingTickers] = useState<string[]>([]);
  const [editingType, setEditingType] = useState<'global' | 'regional'>('global');

  // User preferences
  const [preferences, setPreferences] = useState<UserMarketPreferences>({
    globalMarkets: [],
    regionalMarkets: [],
    lastUpdated: Date.now()
  });
  const [isRecording, setIsRecording] = useState(false);

  // Load preferences on mount
  useEffect(() => {
    const loadPreferences = async () => {
      const prefs = await tickerStorage.loadPreferences();
      setPreferences(prefs);
      setPreferencesLoaded(true);
    };
    loadPreferences();
  }, []);

  // Calculate effective refetch interval (0 = disabled)
  const effectiveRefetchInterval = autoUpdate ? updateInterval : 0;

  // Use cache hooks for each global market category
  const stockIndicesCache = useMarketCategory(
    'Stock Indices',
    preferences.globalMarkets.find(m => m.category === 'Stock Indices')?.tickers || [],
    preferencesLoaded && dbInitialized,
    effectiveRefetchInterval
  );

  const forexCache = useMarketCategory(
    'Forex',
    preferences.globalMarkets.find(m => m.category === 'Forex')?.tickers || [],
    preferencesLoaded && dbInitialized,
    effectiveRefetchInterval
  );

  const commoditiesCache = useMarketCategory(
    'Commodities',
    preferences.globalMarkets.find(m => m.category === 'Commodities')?.tickers || [],
    preferencesLoaded && dbInitialized,
    effectiveRefetchInterval
  );

  const bondsCache = useMarketCategory(
    'Bonds',
    preferences.globalMarkets.find(m => m.category === 'Bonds')?.tickers || [],
    preferencesLoaded && dbInitialized,
    effectiveRefetchInterval
  );

  const etfsCache = useMarketCategory(
    'ETFs',
    preferences.globalMarkets.find(m => m.category === 'ETFs')?.tickers || [],
    preferencesLoaded && dbInitialized,
    effectiveRefetchInterval
  );

  const cryptoCache = useMarketCategory(
    'Cryptocurrencies',
    preferences.globalMarkets.find(m => m.category === 'Cryptocurrencies')?.tickers || [],
    preferencesLoaded && dbInitialized,
    effectiveRefetchInterval
  );

  // Use cache hooks for regional markets
  const indiaCache = useRegionalMarket(
    'India',
    preferences.regionalMarkets.find(m => m.region === 'India')?.tickers || [],
    preferencesLoaded && dbInitialized,
    effectiveRefetchInterval
  );

  const chinaCache = useRegionalMarket(
    'China',
    preferences.regionalMarkets.find(m => m.region === 'China')?.tickers || [],
    preferencesLoaded && dbInitialized,
    effectiveRefetchInterval
  );

  const usCache = useRegionalMarket(
    'United States',
    preferences.regionalMarkets.find(m => m.region === 'United States')?.tickers || [],
    preferencesLoaded && dbInitialized,
    effectiveRefetchInterval
  );

  // Aggregate market data from cache hooks
  const marketData: Record<string, QuoteData[]> = useMemo(() => ({
    'Stock Indices': stockIndicesCache.data || [],
    'Forex': forexCache.data || [],
    'Commodities': commoditiesCache.data || [],
    'Bonds': bondsCache.data || [],
    'ETFs': etfsCache.data || [],
    'Cryptocurrencies': cryptoCache.data || [],
  }), [stockIndicesCache.data, forexCache.data, commoditiesCache.data, bondsCache.data, etfsCache.data, cryptoCache.data]);

  const regionalData: Record<string, QuoteData[]> = useMemo(() => ({
    'India': indiaCache.data || [],
    'China': chinaCache.data || [],
    'United States': usCache.data || [],
  }), [indiaCache.data, chinaCache.data, usCache.data]);

  // Track if any market is updating
  const isUpdating = stockIndicesCache.isFetching || forexCache.isFetching ||
    commoditiesCache.isFetching || bondsCache.isFetching ||
    etfsCache.isFetching || cryptoCache.isFetching ||
    indiaCache.isFetching || chinaCache.isFetching || usCache.isFetching;

  // Get last update time from any successful fetch
  const lastUpdate = useMemo(() => {
    // Return current time when data is available
    if (stockIndicesCache.data || forexCache.data) {
      return new Date();
    }
    return new Date();
  }, [stockIndicesCache.data, forexCache.data]);

  // Refresh all markets
  const refreshAllMarkets = useCallback(async () => {
    await Promise.all([
      stockIndicesCache.refresh(),
      forexCache.refresh(),
      commoditiesCache.refresh(),
      bondsCache.refresh(),
      etfsCache.refresh(),
      cryptoCache.refresh(),
      indiaCache.refresh(),
      chinaCache.refresh(),
      usCache.refresh(),
    ]);
  }, [stockIndicesCache, forexCache, commoditiesCache, bondsCache, etfsCache, cryptoCache, indiaCache, chinaCache, usCache]);

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
          // Still set initialized to allow data fetching
          setDbInitialized(true);
        }
      } catch (error) {
        console.error('[MarketsTab] Database initialization error:', error);
        // Still set initialized to allow data fetching even without cache
        setDbInitialized(true);
      }
    };
    initDatabase();
  }, []);

  // Update current time display
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Open edit modal for global markets
  const openEditModal = async (category: string) => {
    const tickers = await tickerStorage.getCategoryTickers(category);
    setEditingCategory(category);
    setEditingTickers(tickers);
    setEditingType('global');
    setEditModalOpen(true);
  };

  // Open edit modal for regional markets
  const openRegionalEditModal = async (region: string) => {
    const regionalTickers = await tickerStorage.getRegionalTickers(region);
    const symbols = regionalTickers.map((t: { symbol: string; name: string }) => t.symbol);
    setEditingCategory(region);
    setEditingTickers(symbols);
    setEditingType('regional');
    setEditModalOpen(true);
  };

  // Save edited tickers
  const handleSaveTickers = async (tickers: string[]) => {
    if (editingType === 'global') {
      await tickerStorage.updateCategoryTickers(editingCategory, tickers);
    } else {
      // For regional markets, convert back to {symbol, name} format
      const tickersWithNames = tickers.map(symbol => ({
        symbol,
        name: symbol // Use symbol as name for new tickers, actual names will be fetched from API
      }));
      await tickerStorage.updateRegionalTickers(editingCategory, tickersWithNames);
    }
    const prefs = await tickerStorage.loadPreferences();
    setPreferences(prefs);
    // Refresh the specific category after save
    setTimeout(() => refreshAllMarkets(), 100);
  };

  // Format helpers
  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  // Skeleton loading row for global markets (6 columns)
  const SkeletonRowGlobal = ({ delay = 0 }: { delay?: number }) => (
    <div style={{
      display: 'grid',
      gridTemplateColumns: '2fr 1fr 1fr 1fr 1fr 1fr',
      gap: '4px',
      padding: '2px 8px',
      minHeight: '18px',
      alignItems: 'center',
      borderBottom: '1px solid rgba(120,120,120,0.3)'
    }}>
      {[0, 1, 2, 3, 4, 5].map((i) => (
        <div key={i} style={{
          backgroundColor: 'rgba(120,120,120,0.3)',
          height: '12px',
          borderRadius: '2px',
          animation: 'pulse 1.5s infinite',
          animationDelay: `${delay + i * 0.1}s`
        }} />
      ))}
    </div>
  );

  // Skeleton loading row for regional markets (5 columns)
  const SkeletonRowRegional = ({ delay = 0 }: { delay?: number }) => (
    <div style={{
      display: 'grid',
      gridTemplateColumns: '1fr 2fr 1fr 1fr 1fr',
      gap: '4px',
      padding: '2px 8px',
      minHeight: '18px',
      alignItems: 'center',
      borderBottom: '1px solid rgba(120,120,120,0.3)'
    }}>
      {[0, 1, 2, 3, 4].map((i) => (
        <div key={i} style={{
          backgroundColor: 'rgba(120,120,120,0.3)',
          height: '12px',
          borderRadius: '2px',
          animation: 'pulse 1.5s infinite',
          animationDelay: `${delay + i * 0.1}s`
        }} />
      ))}
    </div>
  );

  // Get loading state for a specific category
  const getCategoryLoading = (category: string): boolean => {
    switch (category) {
      case 'Stock Indices': return stockIndicesCache.isLoading;
      case 'Forex': return forexCache.isLoading;
      case 'Commodities': return commoditiesCache.isLoading;
      case 'Bonds': return bondsCache.isLoading;
      case 'ETFs': return etfsCache.isLoading;
      case 'Cryptocurrencies': return cryptoCache.isLoading;
      default: return false;
    }
  };

  // Get loading state for a specific region
  const getRegionLoading = (region: string): boolean => {
    switch (region) {
      case 'India': return indiaCache.isLoading;
      case 'China': return chinaCache.isLoading;
      case 'United States': return usCache.isLoading;
      default: return false;
    }
  };

  // Create market panel
  const createMarketPanel = (title: string, quotes: QuoteData[]) => {
    const isLoading = getCategoryLoading(title);
    const tickerCount = preferences.globalMarkets.find(m => m.category === title)?.tickers.length || 12;

    return (
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
          <div>{t('symbol')}</div>
          <div style={{ textAlign: 'right' }}>{t('price')}</div>
          <div style={{ textAlign: 'right' }}>{t('change')}</div>
          <div style={{ textAlign: 'right' }}>{t('percentChange')}</div>
          <div style={{ textAlign: 'right' }}>{t('high')}</div>
          <div style={{ textAlign: 'right' }}>{t('low')}</div>
        </div>

        {/* Data Rows */}
        <div style={{ flex: 1, overflow: 'auto' }}>
          {isLoading && quotes.length === 0 ? (
            // Show skeleton loading animation
            <>
              {Array.from({ length: tickerCount }).map((_, idx) => (
                <SkeletonRowGlobal key={idx} delay={idx * 0.05} />
              ))}
            </>
          ) : quotes.length === 0 ? (
            <div style={{
              color: colors.textMuted,
              fontSize: fontSize.body,
              textAlign: 'center',
              padding: '16px'
            }}>
              {t('noData')}
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
  };

  // Create regional panel
  const createRegionalPanel = (title: string, quotes: QuoteData[]) => {
    const regionConfig = preferences.regionalMarkets.find(r => r.region === title);
    const isLoading = getRegionLoading(title);
    const tickerCount = regionConfig?.tickers.length || 12;

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
          <div>{t('symbol')}</div>
          <div>{t('name')}</div>
          <div style={{ textAlign: 'right' }}>{t('price')}</div>
          <div style={{ textAlign: 'right' }}>{t('change')}</div>
          <div style={{ textAlign: 'right' }}>{t('percentChange')}</div>
        </div>

        {/* Data Rows */}
        <div style={{ flex: 1, overflow: 'auto' }}>
          {isLoading && quotes.length === 0 ? (
            // Show skeleton loading animation
            <>
              {Array.from({ length: tickerCount }).map((_, idx) => (
                <SkeletonRowRegional key={idx} delay={idx * 0.05} />
              ))}
            </>
          ) : quotes.length === 0 ? (
            <div style={{
              color: colors.textMuted,
              fontSize: fontSize.body,
              textAlign: 'center',
              padding: '16px'
            }}>
              {t('noData')}
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

  // Show loading screen while database initializes
  if (!dbInitialized) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: colors.background,
        color: colors.text,
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        gap: '20px'
      }}>
        <div style={{
          width: '60px',
          height: '60px',
          border: '4px solid #404040',
          borderTop: '4px solid #ea580c',
          borderRadius: '50%',
          animation: 'spin 1s linear infinite'
        }} />
        <style>{`
          @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
          }
          @keyframes pulse {
            0%, 100% { opacity: 0.4; }
            50% { opacity: 0.8; }
          }
        `}</style>
        <div style={{ textAlign: 'center', maxWidth: '500px' }}>
          <h3 style={{ color: '#ea580c', fontSize: '18px', marginBottom: '10px' }}>
            {t('loading.title', 'Initializing Markets Terminal')}
          </h3>
          <p style={{ color: '#a3a3a3', fontSize: '13px', lineHeight: '1.5' }}>
            {t('loading.description', 'Setting up database and market data connections...')}
          </p>
          <p style={{ color: '#787878', fontSize: '11px', marginTop: '10px' }}>
            {t('loading.note', 'This may take a few moments on first launch')}
          </p>
        </div>
      </div>
    );
  }

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
        @keyframes pulse {
          0%, 100% { opacity: 0.4; }
          50% { opacity: 0.8; }
        }
        @keyframes spin {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(360deg); }
        }
        .animate-spin {
          animation: spin 1s linear infinite;
        }
      `}</style>

      {/* Header Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexWrap: 'wrap',
        padding: '8px 12px',
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        fontSize: '13px',
        gap: '8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: colors.primary, fontWeight: 'bold' }}>FINCEPT</span>
          <span style={{ color: colors.text }}>{t('marketTerminalLive')}</span>
          <span style={{ color: colors.textMuted }}>|</span>
          <span style={{ color: colors.text, fontSize: '11px' }}>
            {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
          </span>
        </div>
        <button
          onClick={() => {
            const tour = createMarketsTabTour();
            tour.drive();
          }}
          style={{
            backgroundColor: colors.info,
            color: colors.background,
            border: 'none',
            padding: '4px 8px',
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            borderRadius: '2px'
          }}
          title="Start interactive tour"
        >
          <Info size={14} />
          HELP
        </button>
      </div>

      {/* Control Panel */}
      <div
        id="markets-control-panel"
        style={{
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
            id="markets-refresh"
            onClick={refreshAllMarkets}
            disabled={isUpdating}
            style={{
              backgroundColor: colors.primary,
              color: 'black',
              border: 'none',
              padding: '4px 12px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: isUpdating ? 'wait' : 'pointer',
              opacity: isUpdating ? 0.7 : 1,
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <RefreshCw size={12} className={isUpdating ? 'animate-spin' : ''} />
            {t('refresh')}
          </button>
          <button
            id="markets-auto-update"
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
            {t('auto')} {autoUpdate ? t('on') : t('off')}
          </button>
          <select
            id="markets-timeframe"
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
          <span style={{ color: colors.textMuted, fontSize: '11px' }}>{t('lastUpdate')}:</span>
          <span style={{ color: colors.text, fontSize: '11px' }}>
            {lastUpdate.toTimeString().substring(0, 8)}
          </span>
          <span style={{ color: colors.textMuted }}>|</span>
          <span style={{ color: isUpdating ? colors.primary : colors.secondary, fontSize: '14px' }}>●</span>
          <span style={{ color: isUpdating ? colors.primary : colors.secondary, fontSize: '11px', fontWeight: 'bold' }}>
            {isUpdating ? t('updating') : t('live')}
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
          {t('globalMarkets')}
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '16px' }}></div>

        <div
          id="markets-global-section"
          style={{
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
          {t('regionalMarketsLive')}
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '16px' }}></div>

        <div
          id="markets-regional-section"
          style={{
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
      <TabFooter
        tabName="LIVE MARKETS"
        leftInfo={[
          { label: 'Data provided by Yahoo Finance API', color: colors.textMuted },
          { label: 'Real-time updates', color: colors.textMuted },
        ]}
        statusInfo={
          <span style={{ whiteSpace: 'nowrap' }}>
            Connected: {Object.keys(regionalData).length} regional markets
            {dbInitialized && (
              <span style={{ marginLeft: '8px', color: colors.secondary }}>
                | Cache: ENABLED
              </span>
            )}
          </span>
        }
        backgroundColor={colors.panel}
        borderColor={colors.textMuted}
      />

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
