import React, { useReducer, useEffect, useCallback, useRef } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { RefreshCw, Download, Upload, WifiOff, Plus, BarChart3 } from 'lucide-react';
import { useTimezone } from '@/contexts/TimezoneContext';
import {
  watchlistService,
  Watchlist,
  WatchlistStockWithQuote
} from '@/services/core/watchlistService';
import { contextRecorderService } from '@/services/data-sources/contextRecorderService';
import { useWatchlistStocks, useMarketMovers, useVolumeLeaders } from '@/hooks/useWatchlist';
import { withTimeout } from '@/services/core/apiUtils';
import { validateSymbol } from '@/services/core/validators';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import WatchlistSidebar from './WatchlistSidebar';
import StockListView from './StockListView';
import StockDetailPanel from './StockDetailPanel';
import CreateWatchlistModal from './CreateWatchlistModal';
import AddStockModal from './AddStockModal';
import ImportWatchlistModal, { WatchlistImportJSON, WatchlistImportResult, ImportWatchlistMode } from './ImportWatchlistModal';
import RecordingControlPanel from '@/components/common/RecordingControlPanel';
import { FINCEPT, FONT_FAMILY, SortCriteria, sortStocks, getNextWatchlistColor } from './utils';
import { useTranslation } from 'react-i18next';
import { showConfirm, showSuccess, showError } from '@/utils/notifications';

// ─── State machine (Point 1 + 2) ─────────────────────────────────────────────

type WatchlistWithCount = Watchlist & { stock_count: number };

interface WatchlistState {
  status: 'idle' | 'loading' | 'ready' | 'error';
  watchlists: WatchlistWithCount[];
  selectedWatchlist: WatchlistWithCount | null;
  selectedStock: WatchlistStockWithQuote | null;
  sortBy: SortCriteria;
  currentTime: Date;
  showCreateWatchlist: boolean;
  showAddStock: boolean;
  showImport: boolean;
  isRecording: boolean;
  error: string | null;
}

type WatchlistAction =
  | { type: 'INIT_START' }
  | { type: 'INIT_SUCCESS'; watchlists: WatchlistWithCount[] }
  | { type: 'INIT_ERROR'; error: string }
  | { type: 'SET_WATCHLISTS'; watchlists: WatchlistWithCount[] }
  | { type: 'SELECT_WATCHLIST'; watchlist: WatchlistWithCount | null }
  | { type: 'SELECT_STOCK'; stock: WatchlistStockWithQuote | null }
  | { type: 'SET_SORT'; sortBy: SortCriteria }
  | { type: 'TICK' }
  | { type: 'SHOW_CREATE'; value: boolean }
  | { type: 'SHOW_ADD_STOCK'; value: boolean }
  | { type: 'SHOW_IMPORT'; value: boolean }
  | { type: 'SET_RECORDING'; value: boolean };

const initialState: WatchlistState = {
  status: 'idle',
  watchlists: [],
  selectedWatchlist: null,
  selectedStock: null,
  sortBy: 'CHANGE',
  currentTime: new Date(),
  showCreateWatchlist: false,
  showAddStock: false,
  showImport: false,
  isRecording: false,
  error: null,
};

function reducer(state: WatchlistState, action: WatchlistAction): WatchlistState {
  switch (action.type) {
    case 'INIT_START':
      return { ...state, status: 'loading', error: null };
    case 'INIT_SUCCESS':
      return {
        ...state,
        status: 'ready',
        watchlists: action.watchlists,
        selectedWatchlist: state.selectedWatchlist ?? (action.watchlists[0] ?? null),
      };
    case 'INIT_ERROR':
      return { ...state, status: 'error', error: action.error };
    case 'SET_WATCHLISTS':
      return { ...state, watchlists: action.watchlists };
    case 'SELECT_WATCHLIST':
      return { ...state, selectedWatchlist: action.watchlist, selectedStock: null };
    case 'SELECT_STOCK':
      return { ...state, selectedStock: action.stock };
    case 'SET_SORT':
      return { ...state, sortBy: action.sortBy };
    case 'TICK':
      return { ...state, currentTime: new Date() };
    case 'SHOW_CREATE':
      return { ...state, showCreateWatchlist: action.value };
    case 'SHOW_ADD_STOCK':
      return { ...state, showAddStock: action.value };
    case 'SHOW_IMPORT':
      return { ...state, showImport: action.value };
    case 'SET_RECORDING':
      return { ...state, isRecording: action.value };
    default:
      return state;
  }
}

const INVOKE_TIMEOUT = 15_000;

// ─── Inner component (wrapped by ErrorBoundary below) ────────────────────────

const WatchlistTabInner: React.FC = () => {
  const { t } = useTranslation('watchlist');
  const { timezone, formatTime } = useTimezone();
  const [state, dispatch] = useReducer(reducer, initialState);
  const mountedRef = useRef(true); // Point 3

  const {
    status, watchlists, selectedWatchlist, selectedStock,
    sortBy, currentTime, showCreateWatchlist, showAddStock, showImport, isRecording,
  } = state;

  // ── Cached data fetching (Points 1, 4, 7 via useCache) ───────────────────

  const {
    data: stocks,
    isLoading: stocksLoading,
    isFetching: stocksRefreshing,
    isOffline,
    isStale,
    refresh: refreshStocks,
    invalidate: invalidateStocks,
  } = useWatchlistStocks(selectedWatchlist?.id || null, {
    enabled: !!selectedWatchlist,
    refetchInterval: 10 * 60 * 1000,
  });

  const { data: marketMovers, refresh: refreshMovers } = useMarketMovers(5, {
    enabled: !!selectedWatchlist,
    refetchInterval: 10 * 60 * 1000,
  });

  const { data: volumeLeaders, refresh: refreshVolume } = useVolumeLeaders(5, {
    enabled: !!selectedWatchlist,
    refetchInterval: 10 * 60 * 1000,
  });

  // ── Workspace state persistence ───────────────────────────────────────────

  const getWorkspaceState = useCallback(() => ({ sortBy: sortBy || 'CHANGE' }), [sortBy]);
  const setWorkspaceState = useCallback((s: Record<string, unknown>) => {
    if (typeof s.sortBy === 'string') dispatch({ type: 'SET_SORT', sortBy: s.sortBy as SortCriteria });
  }, []);
  useWorkspaceTabState('watchlist', getWorkspaceState, setWorkspaceState);

  // ── Init + cleanup (Point 3) ──────────────────────────────────────────────

  useEffect(() => {
    mountedRef.current = true;
    const controller = new AbortController();

    const init = async () => {
      dispatch({ type: 'INIT_START' });
      try {
        await withTimeout(watchlistService.initialize(), INVOKE_TIMEOUT, 'Watchlist init timeout');
        if (controller.signal.aborted || !mountedRef.current) return;
        const result = await withTimeout(
          watchlistService.getWatchlistsWithCounts(),
          INVOKE_TIMEOUT,
          'Load watchlists timeout'
        );
        if (!controller.signal.aborted && mountedRef.current) {
          dispatch({ type: 'INIT_SUCCESS', watchlists: result });
        }
      } catch (err) {
        if (!controller.signal.aborted && mountedRef.current) {
          dispatch({ type: 'INIT_ERROR', error: err instanceof Error ? err.message : 'Initialization failed' });
        }
      }
    };

    init();

    return () => {
      mountedRef.current = false;
      controller.abort();
    };
  }, []);

  // ── Clock tick (Point 3 — cleanup) ───────────────────────────────────────

  useEffect(() => {
    const timer = setInterval(() => {
      if (mountedRef.current) dispatch({ type: 'TICK' });
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // ── Sync selectedStock with refreshed quotes ──────────────────────────────

  const stocksList = stocks || [];

  useEffect(() => {
    if (selectedStock && stocksList.length > 0) {
      const updated = stocksList.find(s => s.symbol === selectedStock.symbol);
      if (updated) dispatch({ type: 'SELECT_STOCK', stock: updated });
    }
  }, [stocksList]); // eslint-disable-line react-hooks/exhaustive-deps

  // ── Recording effect (Point 3 — no leak) ─────────────────────────────────

  useEffect(() => {
    if (!isRecording || !selectedWatchlist || stocksList.length === 0) return;
    const recordData = {
      watchlistId: selectedWatchlist.id,
      watchlistName: selectedWatchlist.name,
      stocks: stocksList.map(s => ({ symbol: s.symbol, quote: s.quote, notes: s.notes })),
      timestamp: new Date().toISOString(),
    };
    contextRecorderService.recordApiResponse(
      'Watchlist', 'watchlist-stocks', recordData,
      `Watchlist: ${selectedWatchlist.name} - ${new Date().toLocaleString()}`,
      ['watchlist', 'stocks', 'quotes']
    ).catch(() => { /* silent — recording is non-critical */ });
  }, [stocksList, isRecording, selectedWatchlist]);

  // ── Helpers ───────────────────────────────────────────────────────────────

  const loadWatchlists = useCallback(async () => {
    const result = await withTimeout(
      watchlistService.getWatchlistsWithCounts(),
      INVOKE_TIMEOUT,
      'Load watchlists timeout'
    );
    if (mountedRef.current) dispatch({ type: 'SET_WATCHLISTS', watchlists: result });
    return result;
  }, []);

  const refreshWatchlistData = useCallback(async () => {
    await Promise.all([refreshStocks(), refreshMovers(), refreshVolume()]);
  }, [refreshStocks, refreshMovers, refreshVolume]);

  const recordCurrentData = useCallback(async () => {
    if (!selectedWatchlist || stocksList.length === 0) return;
    try {
      await contextRecorderService.recordApiResponse(
        'Watchlist', 'watchlist-stocks',
        {
          watchlistId: selectedWatchlist.id,
          watchlistName: selectedWatchlist.name,
          stocks: stocksList.map(s => ({ symbol: s.symbol, quote: s.quote, notes: s.notes })),
          timestamp: new Date().toISOString(),
        },
        `Watchlist (Snapshot): ${selectedWatchlist.name} - ${new Date().toLocaleString()}`,
        ['watchlist', 'stocks', 'snapshot']
      );
    } catch { /* non-critical */ }
  }, [selectedWatchlist, stocksList]);

  // ── Handlers ──────────────────────────────────────────────────────────────

  const handleCreateWatchlist = async (name: string, description: string, color: string) => {
    const newWatchlist = await withTimeout(
      watchlistService.createWatchlist(name, description, color),
      INVOKE_TIMEOUT, 'Create watchlist timeout'
    );
    const updated = await loadWatchlists();
    const created = updated.find(w => w.id === newWatchlist.id);
    if (created && mountedRef.current) dispatch({ type: 'SELECT_WATCHLIST', watchlist: created });
  };

  const handleDeleteWatchlist = async (watchlistId: string) => {
    const confirmed = await showConfirm('This action cannot be undone.', {
      title: 'Delete this watchlist?', type: 'danger',
    });
    if (!confirmed) return;
    try {
      await withTimeout(
        watchlistService.deleteWatchlist(watchlistId),
        INVOKE_TIMEOUT, 'Delete watchlist timeout'
      );
      const updated = await loadWatchlists();
      if (selectedWatchlist?.id === watchlistId && mountedRef.current) {
        const next = updated.find(w => w.id !== watchlistId) ?? null;
        dispatch({ type: 'SELECT_WATCHLIST', watchlist: next });
        await invalidateStocks();
      }
      showSuccess('Watchlist deleted successfully');
    } catch (err) {
      showError(err instanceof Error ? err.message : 'Failed to delete watchlist');
    }
  };

  const handleAddStock = async (symbol: string, notes: string) => {
    if (!selectedWatchlist) return;
    // Point 10: validate input
    const validation = validateSymbol(symbol);
    if (!validation.valid) throw new Error(validation.error);
    await withTimeout(
      watchlistService.addStock(selectedWatchlist.id, validation.sanitized!, notes),
      INVOKE_TIMEOUT, 'Add stock timeout'
    );
    await invalidateStocks();
    await refreshStocks();
    await loadWatchlists();
  };

  const handleRemoveStock = async (symbol: string) => {
    if (!selectedWatchlist) return;
    try {
      await withTimeout(
        watchlistService.removeStock(selectedWatchlist.id, symbol),
        INVOKE_TIMEOUT, 'Remove stock timeout'
      );
      await invalidateStocks();
      await refreshStocks();
      await loadWatchlists();
      if (selectedStock?.symbol === symbol && mountedRef.current) {
        dispatch({ type: 'SELECT_STOCK', stock: null });
      }
      showSuccess('Stock removed from watchlist', [{ label: 'Symbol', value: symbol }]);
    } catch (err) {
      showError(err instanceof Error ? err.message : 'Failed to remove stock');
    }
  };

  const handleImportWatchlist = async (
    data: WatchlistImportJSON,
    mode: ImportWatchlistMode,
    targetWatchlistId?: string,
    nameOverride?: string,
    colorOverride?: string,
  ): Promise<WatchlistImportResult> => {
    const existingSymbols = new Set<string>();
    let watchlistId = '';
    let watchlistName = '';

    if (mode === 'new') {
      const name = nameOverride || data.watchlist_name;
      const color = colorOverride || getNextWatchlistColor(watchlists.map(w => w.color));
      const created = await withTimeout(
        watchlistService.createWatchlist(name, data.description, color),
        INVOKE_TIMEOUT, 'Create watchlist timeout'
      );
      watchlistId = created.id;
      watchlistName = created.name;
    } else {
      if (!targetWatchlistId) throw new Error('No target watchlist selected');
      const target = watchlists.find(w => w.id === targetWatchlistId);
      if (!target) throw new Error('Target watchlist not found');
      watchlistId = target.id;
      watchlistName = target.name;
      const currentStocks = await withTimeout(
        watchlistService.getWatchlistStocks(watchlistId),
        INVOKE_TIMEOUT, 'Get stocks timeout'
      );
      currentStocks.forEach(s => existingSymbols.add(s.symbol.toUpperCase()));
    }

    const skipped: string[] = [];
    const errors: string[] = [];
    let added = 0;

    for (const entry of data.stocks) {
      // Point 10: validate each symbol
      const validation = validateSymbol(entry.symbol);
      if (!validation.valid) {
        errors.push(`${entry.symbol}: ${validation.error}`);
        continue;
      }
      const sym = validation.sanitized!;
      if (existingSymbols.has(sym)) { skipped.push(sym); continue; }
      try {
        await withTimeout(
          watchlistService.addStock(watchlistId, sym, entry.notes),
          INVOKE_TIMEOUT, `Add ${sym} timeout`
        );
        existingSymbols.add(sym);
        added++;
      } catch (err: any) {
        errors.push(`${sym}: ${err?.message || 'Failed to add'}`);
      }
    }

    const updated = await loadWatchlists();
    await invalidateStocks();
    await refreshStocks();

    if (mode === 'new') {
      const created = updated.find(w => w.id === watchlistId);
      if (created && mountedRef.current) dispatch({ type: 'SELECT_WATCHLIST', watchlist: created });
    }

    return { watchlistId, watchlistName, added, skipped, errors };
  };

  const handleImportComplete = (result: WatchlistImportResult) => {
    dispatch({ type: 'SHOW_IMPORT', value: false });
    showSuccess(`Imported ${result.added} symbol${result.added !== 1 ? 's' : ''} to "${result.watchlistName}"`, [
      { label: 'Added', value: String(result.added) },
      ...(result.skipped.length > 0 ? [{ label: 'Skipped', value: String(result.skipped.length) }] : []),
    ]);
  };

  const handleExportCSV = async () => {
    if (!selectedWatchlist) return;
    try {
      const csv = await withTimeout(
        watchlistService.exportWatchlistCSV(selectedWatchlist.id),
        INVOKE_TIMEOUT, 'Export timeout'
      );
      const blob = new Blob([csv], { type: 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `${selectedWatchlist.name.replace(/\s+/g, '_')}_${new Date().toISOString().split('T')[0]}.csv`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      showSuccess('Watchlist exported successfully', [
        { label: 'File', value: `${selectedWatchlist.name}.csv` },
      ]);
    } catch (err) {
      showError(err instanceof Error ? err.message : 'Failed to export CSV');
    }
  };

  // ── Render ────────────────────────────────────────────────────────────────

  const sortedStocks = sortStocks(stocksList, sortBy);
  const refreshing = stocksRefreshing;
  const loading = status === 'loading';

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: FONT_FAMILY,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
    }}>
      <style>{`
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
        @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
        .wl-spin { animation: spin 1s linear infinite; }
      `}</style>

      {/* Top Navigation Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <BarChart3 size={16} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ color: FINCEPT.ORANGE, fontWeight: 700, fontSize: '12px', letterSpacing: '0.5px' }}>
            WATCHLIST MONITOR
          </span>
          <span style={{ color: FINCEPT.BORDER }}>|</span>
          <span style={{
            fontSize: '9px', fontWeight: 700,
            color: refreshing ? FINCEPT.ORANGE : FINCEPT.GREEN,
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <span style={{
              width: '6px', height: '6px', borderRadius: '50%',
              backgroundColor: refreshing ? FINCEPT.ORANGE : FINCEPT.GREEN,
              display: 'inline-block',
            }} />
            {refreshing ? 'UPDATING' : 'LIVE'}
          </span>
          <span style={{ color: FINCEPT.BORDER }}>|</span>
          <span style={{ color: FINCEPT.CYAN, fontSize: '10px' }}>
            {formatTime(currentTime, {
              year: 'numeric', month: '2-digit', day: '2-digit',
              hour: '2-digit', minute: '2-digit', second: '2-digit',
            })} {timezone.shortLabel}
          </span>
          {isOffline && (
            <>
              <span style={{ color: FINCEPT.BORDER }}>|</span>
              <span style={{
                padding: '2px 6px', backgroundColor: `${FINCEPT.ORANGE}20`,
                color: FINCEPT.ORANGE, fontSize: '8px', fontWeight: 700,
                borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '4px',
              }}>
                <WifiOff size={10} /> OFFLINE
              </span>
            </>
          )}
          {isStale && !isOffline && (
            <>
              <span style={{ color: FINCEPT.BORDER }}>|</span>
              <span style={{
                padding: '2px 6px', backgroundColor: `${FINCEPT.YELLOW}20`,
                color: FINCEPT.YELLOW, fontSize: '8px', fontWeight: 700, borderRadius: '2px',
              }}>
                STALE
              </span>
            </>
          )}
        </div>

        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <RecordingControlPanel
            tabName="Watchlist"
            onRecordingChange={(v) => dispatch({ type: 'SET_RECORDING', value: v })}
            onRecordingStart={recordCurrentData}
          />
          <button
            onClick={() => dispatch({ type: 'SHOW_IMPORT', value: true })}
            style={{
              padding: '6px 10px', backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY,
              fontSize: '9px', fontWeight: 700, borderRadius: '2px',
              cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => { e.currentTarget.style.borderColor = FINCEPT.CYAN; e.currentTarget.style.color = FINCEPT.CYAN; }}
            onMouseLeave={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; e.currentTarget.style.color = FINCEPT.GRAY; }}
          >
            <Upload size={10} /> IMPORT
          </button>
          {selectedWatchlist && (
            <button
              onClick={() => dispatch({ type: 'SHOW_ADD_STOCK', value: true })}
              style={{
                padding: '6px 10px', backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY,
                fontSize: '9px', fontWeight: 700, borderRadius: '2px',
                cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', transition: 'all 0.2s',
              }}
              onMouseEnter={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; e.currentTarget.style.color = FINCEPT.WHITE; }}
              onMouseLeave={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; e.currentTarget.style.color = FINCEPT.GRAY; }}
            >
              <Plus size={10} /> ADD STOCK
            </button>
          )}
          <button
            onClick={refreshWatchlistData}
            disabled={!selectedWatchlist || refreshing}
            style={{
              padding: '6px 10px', backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY,
              fontSize: '9px', fontWeight: 700, borderRadius: '2px',
              cursor: selectedWatchlist && !refreshing ? 'pointer' : 'not-allowed',
              display: 'flex', alignItems: 'center', gap: '4px',
              opacity: selectedWatchlist && !refreshing ? 1 : 0.5, transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => { if (selectedWatchlist && !refreshing) { e.currentTarget.style.borderColor = FINCEPT.ORANGE; e.currentTarget.style.color = FINCEPT.WHITE; } }}
            onMouseLeave={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; e.currentTarget.style.color = FINCEPT.GRAY; }}
          >
            <RefreshCw size={10} className={refreshing ? 'wl-spin' : ''} /> REFRESH
          </button>
          <button
            onClick={handleExportCSV}
            disabled={!selectedWatchlist}
            style={{
              padding: '6px 10px', backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY,
              fontSize: '9px', fontWeight: 700, borderRadius: '2px',
              cursor: selectedWatchlist ? 'pointer' : 'not-allowed',
              display: 'flex', alignItems: 'center', gap: '4px',
              opacity: selectedWatchlist ? 1 : 0.5, transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => { if (selectedWatchlist) { e.currentTarget.style.borderColor = FINCEPT.ORANGE; e.currentTarget.style.color = FINCEPT.WHITE; } }}
            onMouseLeave={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; e.currentTarget.style.color = FINCEPT.GRAY; }}
          >
            <Download size={10} /> EXPORT
          </button>
        </div>
      </div>

      {/* Summary Info Bar */}
      {selectedWatchlist && (
        <div style={{
          backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`,
          padding: '6px 16px', display: 'flex', alignItems: 'center',
          gap: '16px', fontSize: '9px', flexShrink: 0,
        }}>
          <span style={{ color: FINCEPT.GRAY, fontWeight: 700, letterSpacing: '0.5px' }}>ACTIVE LIST</span>
          <span style={{ color: FINCEPT.YELLOW, fontWeight: 700 }}>{selectedWatchlist.name.toUpperCase()}</span>
          <span style={{ color: FINCEPT.BORDER }}>|</span>
          <span style={{ color: FINCEPT.GRAY, fontWeight: 700, letterSpacing: '0.5px' }}>SYMBOLS</span>
          <span style={{ color: FINCEPT.CYAN }}>{stocksList.length}</span>
          <span style={{ color: FINCEPT.BORDER }}>|</span>
          <span style={{ color: FINCEPT.GRAY, fontWeight: 700, letterSpacing: '0.5px' }}>SORT</span>
          <span style={{ color: FINCEPT.CYAN }}>{sortBy}</span>
        </div>
      )}

      {/* Main Three-Panel Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        <WatchlistSidebar
          watchlists={watchlists}
          selectedWatchlistId={selectedWatchlist?.id || null}
          onSelectWatchlist={(id) => {
            const wl = watchlists.find(w => w.id === id);
            if (wl) dispatch({ type: 'SELECT_WATCHLIST', watchlist: wl });
          }}
          onCreateWatchlist={() => dispatch({ type: 'SHOW_CREATE', value: true })}
          onDeleteWatchlist={handleDeleteWatchlist}
          marketMovers={marketMovers || undefined}
          volumeLeaders={volumeLeaders || []}
        />

        {!selectedWatchlist ? (
          <div style={{
            flex: 1, display: 'flex', flexDirection: 'column',
            alignItems: 'center', justifyContent: 'center',
            color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'center',
          }}>
            <BarChart3 size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>SELECT A WATCHLIST TO VIEW</span>
          </div>
        ) : (
          <StockListView
            stocks={sortedStocks}
            watchlistName={selectedWatchlist.name}
            sortBy={sortBy}
            onSortChange={(s) => dispatch({ type: 'SET_SORT', sortBy: s })}
            onStockClick={(stock) => dispatch({ type: 'SELECT_STOCK', stock })}
            onRemoveStock={handleRemoveStock}
            selectedSymbol={selectedStock?.symbol}
            isLoading={stocksLoading}
          />
        )}

        <StockDetailPanel stock={selectedStock} />
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px', fontSize: '9px', color: FINCEPT.GRAY,
        display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexShrink: 0,
      }}>
        <span>FINCEPT WATCHLIST v2.0 | REAL-TIME TRACKING | UNIFIED CACHE</span>
        <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
          <span>LISTS: <span style={{ color: FINCEPT.CYAN }}>{watchlists.length}</span></span>
          <span>STOCKS: <span style={{ color: FINCEPT.CYAN }}>{stocksList.length}</span></span>
          <span>STATUS: <span style={{ color: isOffline ? FINCEPT.ORANGE : FINCEPT.GREEN }}>
            {loading || stocksLoading ? 'LOADING' : isOffline ? 'OFFLINE' : 'CONNECTED'}
          </span></span>
        </div>
      </div>

      {/* Modals */}
      {showCreateWatchlist && (
        <CreateWatchlistModal
          onClose={() => dispatch({ type: 'SHOW_CREATE', value: false })}
          onCreate={handleCreateWatchlist}
          existingColors={watchlists.map(w => w.color)}
        />
      )}
      {showAddStock && selectedWatchlist && (
        <AddStockModal
          onClose={() => dispatch({ type: 'SHOW_ADD_STOCK', value: false })}
          onAdd={handleAddStock}
          existingSymbols={stocksList.map(s => s.symbol)}
        />
      )}
      <ImportWatchlistModal
        show={showImport}
        existingWatchlists={watchlists.map(w => ({ id: w.id, name: w.name }))}
        selectedWatchlistId={selectedWatchlist?.id ?? null}
        onClose={() => dispatch({ type: 'SHOW_IMPORT', value: false })}
        onImport={handleImportWatchlist}
        onImportComplete={handleImportComplete}
      />
    </div>
  );
};

// ─── Exported component — wrapped with ErrorBoundary (Point 5) ───────────────

const WatchlistTab: React.FC = () => (
  <ErrorBoundary name="WatchlistTab">
    <WatchlistTabInner />
  </ErrorBoundary>
);

export default WatchlistTab;
