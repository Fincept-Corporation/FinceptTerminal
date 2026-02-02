import React, { useState, useEffect, useCallback } from 'react';
import { RefreshCw, Download, WifiOff, Plus, BarChart3 } from 'lucide-react';
import {
  watchlistService,
  Watchlist,
  WatchlistStockWithQuote
} from '@/services/core/watchlistService';
import { contextRecorderService } from '@/services/data-sources/contextRecorderService';
import { useWatchlistStocks, useMarketMovers, useVolumeLeaders } from '@/hooks/useWatchlist';
import WatchlistSidebar from './WatchlistSidebar';
import StockListView from './StockListView';
import StockDetailPanel from './StockDetailPanel';
import CreateWatchlistModal from './CreateWatchlistModal';
import AddStockModal from './AddStockModal';
import RecordingControlPanel from '@/components/common/RecordingControlPanel';
import { FINCEPT, FONT_FAMILY, SortCriteria, sortStocks, getNextWatchlistColor } from './utils';
import { useTranslation } from 'react-i18next';
import { showConfirm, showSuccess, showError } from '@/utils/notifications';

const WatchlistTab: React.FC = () => {
  const { t } = useTranslation('watchlist');
  const [currentTime, setCurrentTime] = useState(new Date());
  const [watchlists, setWatchlists] = useState<Array<Watchlist & { stock_count: number }>>([]);
  const [selectedWatchlist, setSelectedWatchlist] = useState<(Watchlist & { stock_count: number }) | null>(null);
  const [selectedStock, setSelectedStock] = useState<WatchlistStockWithQuote | null>(null);
  const [loading, setLoading] = useState(false);
  const [sortBy, setSortBy] = useState<SortCriteria>('CHANGE');
  const [showCreateWatchlist, setShowCreateWatchlist] = useState(false);
  const [showAddStock, setShowAddStock] = useState(false);
  const [isRecording, setIsRecording] = useState(false);

  // Cached data fetching
  const {
    data: stocks,
    isLoading: stocksLoading,
    isFetching: stocksRefreshing,
    isOffline,
    isStale,
    refresh: refreshStocks,
    invalidate: invalidateStocks
  } = useWatchlistStocks(selectedWatchlist?.id || null, {
    enabled: !!selectedWatchlist,
    refetchInterval: 10 * 60 * 1000
  });

  const {
    data: marketMovers,
    refresh: refreshMovers
  } = useMarketMovers(5, {
    enabled: !!selectedWatchlist,
    refetchInterval: 10 * 60 * 1000
  });

  const {
    data: volumeLeaders,
    refresh: refreshVolume
  } = useVolumeLeaders(5, {
    enabled: !!selectedWatchlist,
    refetchInterval: 10 * 60 * 1000
  });

  const refreshWatchlistData = useCallback(async () => {
    await Promise.all([refreshStocks(), refreshMovers(), refreshVolume()]);
  }, [refreshStocks, refreshMovers, refreshVolume]);

  const stocksList = stocks || [];

  const recordCurrentData = async () => {
    if (selectedWatchlist && stocksList.length > 0) {
      try {
        const recordData = {
          watchlistId: selectedWatchlist.id,
          watchlistName: selectedWatchlist.name,
          stocks: stocksList.map(s => ({
            symbol: s.symbol,
            quote: s.quote,
            notes: s.notes
          })),
          timestamp: new Date().toISOString()
        };
        await contextRecorderService.recordApiResponse(
          'Watchlist',
          'watchlist-stocks',
          recordData,
          `Watchlist (Snapshot): ${selectedWatchlist.name} - ${new Date().toLocaleString()}`,
          ['watchlist', 'stocks', 'snapshot']
        );
      } catch (error) {
        console.error('[WatchlistTab] Failed to record current data:', error);
      }
    }
  };

  // Initialize service and load watchlists
  useEffect(() => {
    const initService = async () => {
      setLoading(true);
      try {
        await watchlistService.initialize();
        await loadWatchlists();
      } catch (error) {
        console.error('[WatchlistTab] Initialization error:', error);
      } finally {
        setLoading(false);
      }
    };
    initService();
  }, []);

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  const loadWatchlists = async () => {
    try {
      const result = await watchlistService.getWatchlistsWithCounts();
      setWatchlists(result);
      if (result.length > 0 && !selectedWatchlist) {
        setSelectedWatchlist(result[0]);
      }
    } catch (error) {
      console.error('[WatchlistTab] Error loading watchlists:', error);
    }
  };

  useEffect(() => {
    if (selectedStock && stocksList.length > 0) {
      const updated = stocksList.find(s => s.symbol === selectedStock.symbol);
      if (updated) setSelectedStock(updated);
    }
  }, [stocksList]);

  useEffect(() => {
    if (isRecording && selectedWatchlist && stocksList.length > 0) {
      const recordData = {
        watchlistId: selectedWatchlist.id,
        watchlistName: selectedWatchlist.name,
        stocks: stocksList.map(s => ({
          symbol: s.symbol,
          quote: s.quote,
          notes: s.notes
        })),
        timestamp: new Date().toISOString()
      };
      contextRecorderService.recordApiResponse(
        'Watchlist',
        'watchlist-stocks',
        recordData,
        `Watchlist: ${selectedWatchlist.name} - ${new Date().toLocaleString()}`,
        ['watchlist', 'stocks', 'quotes']
      ).catch(error => {
        console.error('[WatchlistTab] Failed to record data:', error);
      });
    }
  }, [stocksList, isRecording, selectedWatchlist]);

  const handleCreateWatchlist = async (name: string, description: string, color: string) => {
    try {
      const newWatchlist = await watchlistService.createWatchlist(name, description, color);
      await loadWatchlists();
      const updated = await watchlistService.getWatchlistsWithCounts();
      const created = updated.find(w => w.id === newWatchlist.id);
      if (created) setSelectedWatchlist(created);
    } catch (error) {
      console.error('[WatchlistTab] Error creating watchlist:', error);
      throw error;
    }
  };

  const handleDeleteWatchlist = async (watchlistId: string) => {
    const confirmed = await showConfirm(
      'This action cannot be undone.',
      {
        title: 'Delete this watchlist?',
        type: 'danger'
      }
    );
    if (!confirmed) return;

    try {
      await watchlistService.deleteWatchlist(watchlistId);
      await loadWatchlists();
      if (selectedWatchlist?.id === watchlistId) {
        setSelectedWatchlist(watchlists.length > 1 ? watchlists[0] : null);
        setSelectedStock(null);
        await invalidateStocks();
      }
      showSuccess('Watchlist deleted successfully');
    } catch (error) {
      console.error('[WatchlistTab] Error deleting watchlist:', error);
      showError('Failed to delete watchlist');
    }
  };

  const handleAddStock = async (symbol: string, notes: string) => {
    if (!selectedWatchlist) return;
    try {
      await watchlistService.addStock(selectedWatchlist.id, symbol, notes);
      await invalidateStocks();
      await refreshStocks();
      await loadWatchlists();
    } catch (error) {
      console.error('[WatchlistTab] Error adding stock:', error);
      throw error;
    }
  };

  const handleRemoveStock = async (symbol: string) => {
    if (!selectedWatchlist) return;
    try {
      await watchlistService.removeStock(selectedWatchlist.id, symbol);
      await invalidateStocks();
      await refreshStocks();
      await loadWatchlists();
      if (selectedStock?.symbol === symbol) setSelectedStock(null);
      showSuccess('Stock removed from watchlist', [
        { label: 'Symbol', value: symbol },
      ]);
    } catch (error) {
      console.error('[WatchlistTab] Error removing stock:', error);
      showError('Failed to remove stock');
    }
  };

  const handleExportCSV = async () => {
    if (!selectedWatchlist) return;
    try {
      const csv = await watchlistService.exportWatchlistCSV(selectedWatchlist.id);
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
        { label: 'File', value: `${selectedWatchlist?.name || 'watchlist'}.csv` },
      ]);
    } catch (error) {
      console.error('[WatchlistTab] Error exporting CSV:', error);
      showError('Failed to export CSV');
    }
  };

  const sortedStocks = sortStocks(stocksList, sortBy);
  const refreshing = stocksRefreshing;

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: FONT_FAMILY,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
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
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <BarChart3 size={16} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ color: FINCEPT.ORANGE, fontWeight: 700, fontSize: '12px', letterSpacing: '0.5px' }}>
            WATCHLIST MONITOR
          </span>
          <span style={{ color: FINCEPT.BORDER }}>|</span>
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: refreshing ? FINCEPT.ORANGE : FINCEPT.GREEN,
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}>
            <span style={{
              width: '6px',
              height: '6px',
              borderRadius: '50%',
              backgroundColor: refreshing ? FINCEPT.ORANGE : FINCEPT.GREEN,
              display: 'inline-block'
            }} />
            {refreshing ? 'UPDATING' : 'LIVE'}
          </span>
          <span style={{ color: FINCEPT.BORDER }}>|</span>
          <span style={{ color: FINCEPT.CYAN, fontSize: '10px' }}>
            {currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC
          </span>
          {isOffline && (
            <>
              <span style={{ color: FINCEPT.BORDER }}>|</span>
              <span style={{
                padding: '2px 6px',
                backgroundColor: `${FINCEPT.ORANGE}20`,
                color: FINCEPT.ORANGE,
                fontSize: '8px',
                fontWeight: 700,
                borderRadius: '2px',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}>
                <WifiOff size={10} /> OFFLINE
              </span>
            </>
          )}
          {isStale && !isOffline && (
            <>
              <span style={{ color: FINCEPT.BORDER }}>|</span>
              <span style={{
                padding: '2px 6px',
                backgroundColor: `${FINCEPT.YELLOW}20`,
                color: FINCEPT.YELLOW,
                fontSize: '8px',
                fontWeight: 700,
                borderRadius: '2px'
              }}>
                STALE
              </span>
            </>
          )}
        </div>

        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <RecordingControlPanel
            tabName="Watchlist"
            onRecordingChange={setIsRecording}
            onRecordingStart={recordCurrentData}
          />
          {selectedWatchlist && (
            <button
              onClick={() => setShowAddStock(true)}
              style={{
                padding: '6px 10px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                borderRadius: '2px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                e.currentTarget.style.color = FINCEPT.WHITE;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
                e.currentTarget.style.color = FINCEPT.GRAY;
              }}
            >
              <Plus size={10} /> ADD STOCK
            </button>
          )}
          <button
            onClick={refreshWatchlistData}
            disabled={!selectedWatchlist || refreshing}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: selectedWatchlist && !refreshing ? 'pointer' : 'not-allowed',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: selectedWatchlist && !refreshing ? 1 : 0.5,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (selectedWatchlist && !refreshing) {
                e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                e.currentTarget.style.color = FINCEPT.WHITE;
              }
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
              e.currentTarget.style.color = FINCEPT.GRAY;
            }}
          >
            <RefreshCw size={10} className={refreshing ? 'wl-spin' : ''} /> REFRESH
          </button>
          <button
            onClick={handleExportCSV}
            disabled={!selectedWatchlist}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: selectedWatchlist ? 'pointer' : 'not-allowed',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: selectedWatchlist ? 1 : 0.5,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (selectedWatchlist) {
                e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                e.currentTarget.style.color = FINCEPT.WHITE;
              }
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
              e.currentTarget.style.color = FINCEPT.GRAY;
            }}
          >
            <Download size={10} /> EXPORT
          </button>
        </div>
      </div>

      {/* Summary Info Bar */}
      {selectedWatchlist && (
        <div style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          padding: '6px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '16px',
          fontSize: '9px',
          flexShrink: 0
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
        {/* Left Panel - Sidebar */}
        <WatchlistSidebar
          watchlists={watchlists}
          selectedWatchlistId={selectedWatchlist?.id || null}
          onSelectWatchlist={(id) => {
            const wl = watchlists.find(w => w.id === id);
            if (wl) setSelectedWatchlist(wl);
          }}
          onCreateWatchlist={() => setShowCreateWatchlist(true)}
          onDeleteWatchlist={handleDeleteWatchlist}
          marketMovers={marketMovers || undefined}
          volumeLeaders={volumeLeaders || []}
        />

        {/* Center Panel - Stock List */}
        {!selectedWatchlist ? (
          <div style={{
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <BarChart3 size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>SELECT A WATCHLIST TO VIEW</span>
          </div>
        ) : (
          <StockListView
            stocks={sortedStocks}
            watchlistName={selectedWatchlist.name}
            sortBy={sortBy}
            onSortChange={setSortBy}
            onStockClick={setSelectedStock}
            onRemoveStock={handleRemoveStock}
            selectedSymbol={selectedStock?.symbol}
            isLoading={stocksLoading}
          />
        )}

        {/* Right Panel - Stock Details */}
        <StockDetailPanel stock={selectedStock} />
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
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
          onClose={() => setShowCreateWatchlist(false)}
          onCreate={handleCreateWatchlist}
          existingColors={watchlists.map(w => w.color)}
        />
      )}
      {showAddStock && selectedWatchlist && (
        <AddStockModal
          onClose={() => setShowAddStock(false)}
          onAdd={handleAddStock}
          existingSymbols={stocksList.map(s => s.symbol)}
        />
      )}
    </div>
  );
};

export default WatchlistTab;
