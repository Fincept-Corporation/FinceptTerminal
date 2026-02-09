// MarketsTab - Production-ready Markets Terminal
// Uses shared utilities: apiUtils, validators, ErrorBoundary

import React, { useState, useEffect, useCallback, useReducer } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { Edit2, Info, RefreshCw, AlertCircle } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { marketDataService, QuoteData } from '@/services/markets/marketDataService';
import { tickerStorage, UserMarketPreferences } from '@/services/core/tickerStorageService';
import { sqliteService } from '@/services/core/sqliteService';
import { contextRecorderService } from '@/services/data-sources/contextRecorderService';
import { withTimeout, rateLimiters } from '@/services/core/apiUtils';
import { validateSymbolList } from '@/services/core/validators';
import { useCache } from '@/hooks/useCache';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import TickerEditModal from '@/components/tabs/ticker-edit-modal';
import RecordingControlPanel from '@/components/common/RecordingControlPanel';
import { TabFooter } from '@/components/common/TabFooter';
import { createMarketsTabTour } from '@/components/tabs/tours/marketsTabTour';

// ============================================================================
// Constants
// ============================================================================

const API_TIMEOUT_MS = 30000;
const STALE_DATA_WARNING_MS = 30 * 60 * 1000;

// ============================================================================
// State Reducer (lightweight, component-local)
// ============================================================================

type ModalState = {
  isOpen: boolean;
  category: string;
  type: 'global' | 'regional';
  tickers: string[];
};

type State = {
  preferences: UserMarketPreferences | null;
  autoUpdate: boolean;
  updateInterval: number;
  isRecording: boolean;
  editModal: ModalState;
};

type Action =
  | { type: 'SET_PREFERENCES'; payload: UserMarketPreferences }
  | { type: 'SET_AUTO_UPDATE'; payload: boolean }
  | { type: 'SET_UPDATE_INTERVAL'; payload: number }
  | { type: 'SET_RECORDING'; payload: boolean }
  | { type: 'OPEN_MODAL'; payload: Omit<ModalState, 'isOpen'> }
  | { type: 'CLOSE_MODAL' };

const initialState: State = {
  preferences: null,
  autoUpdate: true,
  updateInterval: 600000,
  isRecording: false,
  editModal: { isOpen: false, category: '', type: 'global', tickers: [] },
};

function reducer(state: State, action: Action): State {
  switch (action.type) {
    case 'SET_PREFERENCES':
      return { ...state, preferences: action.payload };
    case 'SET_AUTO_UPDATE':
      return { ...state, autoUpdate: action.payload };
    case 'SET_UPDATE_INTERVAL':
      return { ...state, updateInterval: action.payload };
    case 'SET_RECORDING':
      return { ...state, isRecording: action.payload };
    case 'OPEN_MODAL':
      return { ...state, editModal: { isOpen: true, ...action.payload } };
    case 'CLOSE_MODAL':
      return { ...state, editModal: { ...state.editModal, isOpen: false } };
    default:
      return state;
  }
}

// ============================================================================
// Market Panel Component
// ============================================================================

interface MarketPanelProps {
  title: string;
  tickers: string[];
  tickerNames?: Record<string, string>;
  enabled: boolean;
  refetchInterval: number;
  onEdit: () => void;
  onDataUpdate: (data: QuoteData[]) => void;
  showName?: boolean;
}

const MarketPanel: React.FC<MarketPanelProps> = ({
  title,
  tickers,
  tickerNames,
  enabled,
  refetchInterval,
  onEdit,
  onDataUpdate,
  showName = false,
}) => {
  const { t } = useTranslation('markets');
  const { colors, fontSize } = useTerminalTheme();

  const { data, isLoading, isFetching, error, isStale, refresh } = useCache<QuoteData[]>({
    key: `markets:${title}:${tickers.length}:${tickers[0] || 'empty'}`,
    category: 'market-quotes',
    fetcher: async () => {
      if (!rateLimiters.standard.canProceed()) {
        throw new Error('Rate limit exceeded. Please wait.');
      }
      rateLimiters.standard.record();

      const validation = validateSymbolList(tickers);
      if (!validation.valid) {
        throw new Error(`Invalid symbols: ${validation.error}`);
      }

      return withTimeout(
        marketDataService.getQuotes(validation.sanitized),
        API_TIMEOUT_MS,
        'Request timed out'
      );
    },
    ttl: '10m',
    enabled: enabled && tickers.length > 0,
    refetchInterval,
    staleWhileRevalidate: true,
    onSuccess: onDataUpdate,
  });

  const quotes = data || [];
  const formatChange = (v: number) => (v >= 0 ? `+${v.toFixed(2)}` : v.toFixed(2));
  const formatPercent = (v: number) => (v >= 0 ? `+${v.toFixed(2)}%` : `${v.toFixed(2)}%`);

  const columns = showName
    ? { template: '1fr 2fr 1fr 1fr 1fr', count: 5 }
    : { template: '2fr 1fr 1fr 1fr 1fr 1fr', count: 6 };

  return (
    <div
      style={{
        backgroundColor: colors.panel,
        border: `1px solid ${error ? colors.alert : colors.textMuted}`,
        flex: '1 1 calc(33.333% - 16px)',
        minWidth: '300px',
        maxWidth: '600px',
        height: showName ? '300px' : '280px',
        margin: '8px',
        display: 'flex',
        flexDirection: 'column',
      }}
    >
      {/* Header */}
      <div
        style={{
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
          gap: '8px',
        }}
      >
        {title.toUpperCase()}
        {showName && ' - LIVE DATA'}
        {isFetching && <RefreshCw size={12} className="animate-spin" />}
        <Edit2
          size={14}
          style={{ cursor: 'pointer', color: colors.text }}
          onClick={onEdit}
          aria-label={`Edit ${title}`}
        />
      </div>

      {/* Error Banner */}
      {error && (
        <div
          style={{
            backgroundColor: 'rgba(239, 68, 68, 0.1)',
            color: colors.alert,
            padding: '4px 8px',
            fontSize: '10px',
            display: 'flex',
            justifyContent: 'space-between',
          }}
        >
          <span>{error.message}</span>
          <button
            onClick={refresh}
            style={{
              background: 'none',
              border: 'none',
              color: colors.alert,
              cursor: 'pointer',
              textDecoration: 'underline',
            }}
          >
            Retry
          </button>
        </div>
      )}

      {/* Table Header */}
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: columns.template,
          gap: '4px',
          padding: '4px 8px',
          backgroundColor: colors.background,
          color: colors.text,
          fontSize: fontSize.body,
          fontWeight: 'bold',
          borderBottom: `1px solid ${colors.textMuted}`,
        }}
      >
        <div>{t('symbol')}</div>
        {showName && <div>{t('name')}</div>}
        <div style={{ textAlign: 'right' }}>{t('price')}</div>
        <div style={{ textAlign: 'right' }}>{t('change')}</div>
        <div style={{ textAlign: 'right' }}>{t('percentChange')}</div>
        {!showName && (
          <>
            <div style={{ textAlign: 'right' }}>{t('high')}</div>
            <div style={{ textAlign: 'right' }}>{t('low')}</div>
          </>
        )}
      </div>

      {/* Data Rows */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {isLoading && quotes.length === 0 ? (
          Array.from({ length: Math.min(tickers.length, 12) }).map((_, idx) => (
            <div
              key={idx}
              style={{
                display: 'grid',
                gridTemplateColumns: columns.template,
                gap: '4px',
                padding: '2px 8px',
                minHeight: '18px',
                alignItems: 'center',
                borderBottom: '1px solid rgba(120,120,120,0.3)',
              }}
            >
              {Array.from({ length: columns.count }).map((_, i) => (
                <div
                  key={i}
                  style={{
                    backgroundColor: 'rgba(120,120,120,0.3)',
                    height: '12px',
                    borderRadius: '2px',
                    animation: 'pulse 1.5s infinite',
                  }}
                />
              ))}
            </div>
          ))
        ) : quotes.length === 0 ? (
          <div style={{ color: colors.textMuted, textAlign: 'center', padding: '16px' }}>
            {error ? 'Error loading data' : t('noData')}
          </div>
        ) : (
          quotes.map((quote, idx) => (
            <div
              key={quote.symbol}
              style={{
                display: 'grid',
                gridTemplateColumns: columns.template,
                gap: '4px',
                padding: '2px 8px',
                fontSize: fontSize.small,
                backgroundColor: idx % 2 === 0 ? 'rgba(255,255,255,0.05)' : 'transparent',
                borderBottom: '1px solid rgba(120,120,120,0.3)',
                minHeight: '18px',
                alignItems: 'center',
              }}
            >
              <div style={{ color: colors.text }}>{quote.symbol}</div>
              {showName && (
                <div style={{ color: colors.text, overflow: 'hidden', textOverflow: 'ellipsis' }}>
                  {tickerNames?.[quote.symbol] || quote.symbol}
                </div>
              )}
              <div style={{ color: colors.text, textAlign: 'right' }}>{quote.price.toFixed(2)}</div>
              <div
                style={{
                  color: quote.change >= 0 ? colors.secondary : colors.alert,
                  textAlign: 'right',
                }}
              >
                {formatChange(quote.change)}
              </div>
              <div
                style={{
                  color: quote.change_percent >= 0 ? colors.secondary : colors.alert,
                  textAlign: 'right',
                }}
              >
                {formatPercent(quote.change_percent)}
              </div>
              {!showName && (
                <>
                  <div style={{ color: colors.text, textAlign: 'right' }}>
                    {quote.high?.toFixed(2) || '-'}
                  </div>
                  <div style={{ color: colors.text, textAlign: 'right' }}>
                    {quote.low?.toFixed(2) || '-'}
                  </div>
                </>
              )}
            </div>
          ))
        )}
      </div>

      {/* Stale Warning */}
      {isStale && (
        <div
          style={{
            backgroundColor: 'rgba(245, 158, 11, 0.1)',
            color: '#f59e0b',
            padding: '2px 8px',
            fontSize: '9px',
            textAlign: 'center',
          }}
        >
          Showing cached data (may be outdated)
        </div>
      )}
    </div>
  );
};

// ============================================================================
// Main Component
// ============================================================================

const MarketsTab: React.FC = () => {
  const { t } = useTranslation('markets');
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();

  const [state, dispatch] = useReducer(reducer, initialState);
  const [currentTime, setCurrentTime] = useState(new Date());
  const [dbInitialized, setDbInitialized] = useState(false);
  const [refreshKey, setRefreshKey] = useState(0);
  const [collectedData, setCollectedData] = useState<Record<string, QuoteData[]>>({});
  const [initError, setInitError] = useState<string | null>(null);

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    autoUpdate: state.autoUpdate,
    updateInterval: state.updateInterval,
  }), [state.autoUpdate, state.updateInterval]);

  const setWorkspaceState = useCallback((restored: Record<string, unknown>) => {
    if (typeof restored.autoUpdate === 'boolean') dispatch({ type: 'SET_AUTO_UPDATE', payload: restored.autoUpdate });
    if (typeof restored.updateInterval === 'number') dispatch({ type: 'SET_UPDATE_INTERVAL', payload: restored.updateInterval });
  }, []);

  useWorkspaceTabState('markets', getWorkspaceState, setWorkspaceState);

  const effectiveRefetchInterval = state.autoUpdate ? state.updateInterval : 0;

  // Initialize
  useEffect(() => {
    let mounted = true;

    const init = async () => {
      try {
        await sqliteService.initialize();
        if (!mounted) return;
        setDbInitialized(true);

        const prefs = await withTimeout(tickerStorage.loadPreferences(), 10000);
        if (!mounted) return;
        dispatch({ type: 'SET_PREFERENCES', payload: prefs });
      } catch (err) {
        if (!mounted) return;
        setInitError(err instanceof Error ? err.message : 'Failed to initialize');
        setDbInitialized(true);
      }
    };

    init();
    return () => { mounted = false; };
  }, []);

  // Clock
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Handlers
  const handleDataUpdate = useCallback((category: string) => (data: QuoteData[]) => {
    setCollectedData((prev) => ({ ...prev, [category]: data }));
  }, []);

  const handleRefreshAll = useCallback(() => setRefreshKey((k) => k + 1), []);

  const handleOpenEdit = useCallback(async (category: string, type: 'global' | 'regional') => {
    let tickers: string[] = [];
    try {
      if (type === 'global') {
        tickers = await tickerStorage.getCategoryTickers(category);
      } else {
        const regional = await tickerStorage.getRegionalTickers(category);
        tickers = regional.map((t) => t.symbol);
      }
    } catch (err) {
      console.error('Failed to load tickers:', err);
    }
    dispatch({ type: 'OPEN_MODAL', payload: { category, type, tickers } });
  }, []);

  const handleSaveTickers = useCallback(async (tickers: string[]) => {
    const { editModal } = state;
    const validation = validateSymbolList(tickers);
    if (!validation.valid) return;

    try {
      if (editModal.type === 'global') {
        await tickerStorage.updateCategoryTickers(editModal.category, validation.sanitized);
      } else {
        await tickerStorage.updateRegionalTickers(
          editModal.category,
          validation.sanitized.map((s) => ({ symbol: s, name: s }))
        );
      }
      const prefs = await tickerStorage.loadPreferences();
      dispatch({ type: 'SET_PREFERENCES', payload: prefs });
      dispatch({ type: 'CLOSE_MODAL' });
      setRefreshKey((k) => k + 1);
    } catch (err) {
      console.error('Failed to save tickers:', err);
    }
  }, [state.editModal]);

  const recordData = useCallback(async () => {
    if (Object.keys(collectedData).length === 0) return;
    try {
      await contextRecorderService.recordApiResponse(
        'Markets',
        'market-data',
        { data: collectedData, timestamp: new Date().toISOString() },
        `Market Data - ${new Date().toLocaleString()}`,
        ['markets', 'snapshot']
      );
    } catch (err) {
      console.error('Failed to record:', err);
    }
  }, [collectedData]);

  // Loading
  if (!dbInitialized || !state.preferences) {
    return (
      <div
        style={{
          height: '100%',
          backgroundColor: colors.background,
          color: colors.text,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '20px',
        }}
      >
        {initError ? (
          <>
            <AlertCircle size={48} color={colors.alert} />
            <div style={{ color: colors.alert }}>{initError}</div>
            <button
              onClick={() => window.location.reload()}
              style={{
                backgroundColor: colors.primary,
                color: '#000',
                border: 'none',
                padding: '8px 24px',
                cursor: 'pointer',
              }}
            >
              Reload
            </button>
          </>
        ) : (
          <>
            <div
              style={{
                width: '50px',
                height: '50px',
                border: '3px solid #404040',
                borderTop: '3px solid #ea580c',
                borderRadius: '50%',
                animation: 'spin 1s linear infinite',
              }}
            />
            <div>{t('loading.title', 'Initializing...')}</div>
          </>
        )}
        <style>{`@keyframes spin { to { transform: rotate(360deg); } } @keyframes pulse { 50% { opacity: 0.5; } }`}</style>
      </div>
    );
  }

  const { preferences } = state;

  return (
    <div
      style={{
        height: '100%',
        backgroundColor: colors.background,
        color: colors.text,
        fontFamily,
        fontWeight,
        fontStyle,
        overflow: 'hidden',
        display: 'flex',
        flexDirection: 'column',
      }}
    >
      <style>{`
        @keyframes spin { to { transform: rotate(360deg); } }
        @keyframes pulse { 50% { opacity: 0.5; } }
        .animate-spin { animation: spin 1s linear infinite; }
      `}</style>

      {/* Header */}
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          padding: '8px 12px',
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${colors.textMuted}`,
          fontSize: '13px',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: colors.primary, fontWeight: 'bold' }}>FINCEPT</span>
          <span>{t('marketTerminalLive')}</span>
          <span style={{ color: colors.textMuted }}>|</span>
          <span style={{ fontSize: '11px' }}>
            {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
          </span>
        </div>
        <button
          onClick={() => createMarketsTabTour().drive()}
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
          }}
        >
          <Info size={14} /> HELP
        </button>
      </div>

      {/* Controls */}
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          flexWrap: 'wrap',
          padding: '8px 12px',
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${colors.textMuted}`,
          gap: '8px',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap' }}>
          <button
            onClick={handleRefreshAll}
            style={{
              backgroundColor: colors.primary,
              color: '#000',
              border: 'none',
              padding: '4px 12px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <RefreshCw size={12} /> {t('refresh')}
          </button>
          <button
            onClick={() => dispatch({ type: 'SET_AUTO_UPDATE', payload: !state.autoUpdate })}
            style={{
              backgroundColor: state.autoUpdate ? colors.secondary : colors.textMuted,
              color: '#000',
              border: 'none',
              padding: '4px 12px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer',
            }}
          >
            {t('auto')} {state.autoUpdate ? t('on') : t('off')}
          </button>
          <select
            value={state.updateInterval}
            onChange={(e) => dispatch({ type: 'SET_UPDATE_INTERVAL', payload: Number(e.target.value) })}
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              color: colors.text,
              padding: '4px 8px',
              fontSize: '11px',
            }}
          >
            <option value={600000}>10 min</option>
            <option value={900000}>15 min</option>
            <option value={1800000}>30 min</option>
            <option value={3600000}>1 hour</option>
          </select>
        </div>
        <RecordingControlPanel
          tabName="Markets"
          onRecordingChange={(r) => dispatch({ type: 'SET_RECORDING', payload: r })}
          onRecordingStart={recordData}
        />
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {/* Global Markets */}
        <div style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
          {t('globalMarkets')}
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '16px' }} />
        <div style={{ display: 'flex', flexWrap: 'wrap', marginBottom: '24px' }}>
          {preferences.globalMarkets.map((m) => (
            <ErrorBoundary key={`${m.category}-${refreshKey}`} name={m.category} variant="minimal">
              <MarketPanel
                title={m.category}
                tickers={m.tickers}
                enabled={dbInitialized}
                refetchInterval={effectiveRefetchInterval}
                onEdit={() => handleOpenEdit(m.category, 'global')}
                onDataUpdate={handleDataUpdate(m.category)}
              />
            </ErrorBoundary>
          ))}
        </div>

        {/* Regional Markets */}
        <div style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
          {t('regionalMarketsLive')}
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '16px' }} />
        <div style={{ display: 'flex', flexWrap: 'wrap' }}>
          {preferences.regionalMarkets.map((m) => {
            const names = Object.fromEntries(m.tickers.map((t) => [t.symbol, t.name]));
            return (
              <ErrorBoundary key={`${m.region}-${refreshKey}`} name={m.region} variant="minimal">
                <MarketPanel
                  title={m.region}
                  tickers={m.tickers.map((t) => t.symbol)}
                  tickerNames={names}
                  enabled={dbInitialized}
                  refetchInterval={effectiveRefetchInterval}
                  onEdit={() => handleOpenEdit(m.region, 'regional')}
                  onDataUpdate={handleDataUpdate(m.region)}
                  showName
                />
              </ErrorBoundary>
            );
          })}
        </div>
      </div>

      {/* Footer */}
      <TabFooter
        tabName="LIVE MARKETS"
        leftInfo={[{ label: 'Yahoo Finance API', color: colors.textMuted }]}
        statusInfo={`${preferences.regionalMarkets.length} regional markets | Cache: ENABLED`}
        backgroundColor={colors.panel}
        borderColor={colors.textMuted}
      />

      {/* Edit Modal */}
      <TickerEditModal
        isOpen={state.editModal.isOpen}
        onClose={() => dispatch({ type: 'CLOSE_MODAL' })}
        title={state.editModal.category}
        tickers={state.editModal.tickers}
        onSave={handleSaveTickers}
      />
    </div>
  );
};

export default MarketsTab;
