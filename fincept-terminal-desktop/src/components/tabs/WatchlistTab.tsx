import React, { useState, useEffect } from 'react';
import { RefreshCw, Download } from 'lucide-react';
import {
  watchlistService,
  Watchlist,
  WatchlistStockWithQuote
} from '../../services/watchlistService';
import { contextRecorderService } from '../../services/contextRecorderService';
import WatchlistSidebar from './watchlist/WatchlistSidebar';
import StockListView from './watchlist/StockListView';
import StockDetailPanel from './watchlist/StockDetailPanel';
import CreateWatchlistModal from './watchlist/CreateWatchlistModal';
import AddStockModal from './watchlist/AddStockModal';
import RecordingControlPanel from '../common/RecordingControlPanel';
import { getBloombergColors, SortCriteria, sortStocks, getNextWatchlistColor } from './watchlist/utils';
import { useTerminalTheme } from '@/contexts/ThemeContext';

const WatchlistTab: React.FC = () => {
  const { colors: themeColors } = useTerminalTheme();
  const BLOOMBERG_COLORS = getBloombergColors();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [watchlists, setWatchlists] = useState<Array<Watchlist & { stock_count: number }>>([]);
  const [selectedWatchlist, setSelectedWatchlist] = useState<(Watchlist & { stock_count: number }) | null>(null);
  const [stocks, setStocks] = useState<WatchlistStockWithQuote[]>([]);
  const [selectedStock, setSelectedStock] = useState<WatchlistStockWithQuote | null>(null);
  const [loading, setLoading] = useState(false);
  const [refreshing, setRefreshing] = useState(false);

  // Sort and view states
  const [sortBy, setSortBy] = useState<SortCriteria>('CHANGE');

  // Modal states
  const [showCreateWatchlist, setShowCreateWatchlist] = useState(false);
  const [showAddStock, setShowAddStock] = useState(false);

  // Recording state
  const [isRecording, setIsRecording] = useState(false);

  // Function to record current watchlist data
  const recordCurrentData = async () => {
    if (selectedWatchlist && stocks.length > 0) {
      try {
        const recordData = {
          watchlistId: selectedWatchlist.id,
          watchlistName: selectedWatchlist.name,
          stocks: stocks.map(s => ({
            symbol: s.symbol,
            quote: s.quote,
            notes: s.notes
          })),
          timestamp: new Date().toISOString()
        };
        console.log('[WatchlistTab] Recording current data:', recordData);
        await contextRecorderService.recordApiResponse(
          'Watchlist',
          'watchlist-stocks',
          recordData,
          `Watchlist (Snapshot): ${selectedWatchlist.name} - ${new Date().toLocaleString()}`,
          ['watchlist', 'stocks', 'snapshot']
        );
        console.log('[WatchlistTab] Current data recorded successfully');
      } catch (error) {
        console.error('[WatchlistTab] Failed to record current data:', error);
      }
    }
  };

  // Market data
  const [marketMovers, setMarketMovers] = useState<{
    gainers: WatchlistStockWithQuote[];
    losers: WatchlistStockWithQuote[];
  } | null>(null);
  const [volumeLeaders, setVolumeLeaders] = useState<WatchlistStockWithQuote[]>([]);

  const { ORANGE, WHITE, RED, GREEN, GRAY, DARK_BG, PANEL_BG, CYAN, YELLOW } = BLOOMBERG_COLORS;

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

  // Update time
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Auto-refresh data every 10 minutes (matches cache expiry)
  useEffect(() => {
    if (selectedWatchlist) {
      const refreshTimer = setInterval(() => {
        refreshWatchlistData();
      }, 600000); // Refresh every 10 minutes (600000ms)
      return () => clearInterval(refreshTimer);
    }
  }, [selectedWatchlist]);

  // Load watchlists
  const loadWatchlists = async () => {
    try {
      const result = await watchlistService.getWatchlistsWithCounts();
      setWatchlists(result);

      // Select first watchlist if none selected
      if (result.length > 0 && !selectedWatchlist) {
        setSelectedWatchlist(result[0]);
      }
    } catch (error) {
      console.error('[WatchlistTab] Error loading watchlists:', error);
    }
  };

  // Load watchlist stocks
  const loadWatchlistStocks = async (watchlistId: string) => {
    try {
      setRefreshing(true);
      const stocksData = await watchlistService.getWatchlistStocksWithQuotes(watchlistId);
      setStocks(stocksData);

      // Update selected stock if it exists
      if (selectedStock) {
        const updated = stocksData.find(s => s.symbol === selectedStock.symbol);
        if (updated) {
          setSelectedStock(updated);
        }
      }

      // Record data if recording is active
      if (isRecording && selectedWatchlist) {
        try {
          const recordData = {
            watchlistId: watchlistId,
            watchlistName: selectedWatchlist.name,
            stocks: stocksData.map(s => ({
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
            `Watchlist: ${selectedWatchlist.name} - ${new Date().toLocaleString()}`,
            ['watchlist', 'stocks', 'quotes']
          );
        } catch (error) {
          console.error('[WatchlistTab] Failed to record data:', error);
        }
      }
    } catch (error) {
      console.error('[WatchlistTab] Error loading stocks:', error);
    } finally {
      setRefreshing(false);
    }
  };

  // Load market movers and volume leaders
  const loadMarketData = async () => {
    try {
      const [movers, leaders] = await Promise.all([
        watchlistService.getMarketMovers(5),
        watchlistService.getVolumeLeaders(5)
      ]);
      setMarketMovers(movers);
      setVolumeLeaders(leaders);
    } catch (error) {
      console.error('[WatchlistTab] Error loading market data:', error);
    }
  };

  // Refresh watchlist data
  const refreshWatchlistData = () => {
    if (selectedWatchlist) {
      loadWatchlistStocks(selectedWatchlist.id);
      loadMarketData();
    }
  };

  // Handle watchlist selection
  useEffect(() => {
    if (selectedWatchlist) {
      loadWatchlistStocks(selectedWatchlist.id);
      loadMarketData();
    }
  }, [selectedWatchlist]);

  // Create new watchlist
  const handleCreateWatchlist = async (name: string, description: string, color: string) => {
    try {
      const newWatchlist = await watchlistService.createWatchlist(name, description, color);
      await loadWatchlists();

      // Find and select the newly created watchlist
      const updated = await watchlistService.getWatchlistsWithCounts();
      const created = updated.find(w => w.id === newWatchlist.id);
      if (created) {
        setSelectedWatchlist(created);
      }
    } catch (error) {
      console.error('[WatchlistTab] Error creating watchlist:', error);
      throw error;
    }
  };

  // Delete watchlist
  const handleDeleteWatchlist = async (watchlistId: string) => {
    if (!confirm('Are you sure you want to delete this watchlist? This action cannot be undone.')) {
      return;
    }

    try {
      await watchlistService.deleteWatchlist(watchlistId);
      await loadWatchlists();

      if (selectedWatchlist?.id === watchlistId) {
        setSelectedWatchlist(watchlists.length > 1 ? watchlists[0] : null);
        setStocks([]);
        setSelectedStock(null);
      }
    } catch (error) {
      console.error('[WatchlistTab] Error deleting watchlist:', error);
      alert('Failed to delete watchlist');
    }
  };

  // Add stock to watchlist
  const handleAddStock = async (symbol: string, notes: string) => {
    if (!selectedWatchlist) return;

    try {
      await watchlistService.addStock(selectedWatchlist.id, symbol, notes);
      await loadWatchlistStocks(selectedWatchlist.id);
      await loadWatchlists(); // Update counts
    } catch (error) {
      console.error('[WatchlistTab] Error adding stock:', error);
      throw error;
    }
  };

  // Remove stock from watchlist
  const handleRemoveStock = async (symbol: string) => {
    if (!selectedWatchlist) return;

    try {
      await watchlistService.removeStock(selectedWatchlist.id, symbol);
      await loadWatchlistStocks(selectedWatchlist.id);
      await loadWatchlists(); // Update counts

      if (selectedStock?.symbol === symbol) {
        setSelectedStock(null);
      }
    } catch (error) {
      console.error('[WatchlistTab] Error removing stock:', error);
      alert('Failed to remove stock');
    }
  };

  // Export watchlist to CSV
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
    } catch (error) {
      console.error('[WatchlistTab] Error exporting CSV:', error);
      alert('Failed to export CSV');
    }
  };

  // Get sorted stocks
  const sortedStocks = sortStocks(stocks, sortBy);

  return (
    <div style={{
      height: '100%',
      backgroundColor: DARK_BG,
      color: WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        *::-webkit-scrollbar { width: 8px; height: 8px; }
        *::-webkit-scrollbar-track { background: #1a1a1a; }
        *::-webkit-scrollbar-thumb { background: #404040; border-radius: 4px; }
        *::-webkit-scrollbar-thumb:hover { background: #525252; }
      `}</style>

      {/* Header Bar */}
      <div style={{
        backgroundColor: PANEL_BG,
        borderBottom: `1px solid ${GRAY}`,
        padding: '8px 12px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: ORANGE, fontWeight: 'bold', fontSize: '14px' }}>
              FINCEPT WATCHLIST MONITOR
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: refreshing ? ORANGE : GREEN, fontSize: '10px' }}>
              ● {refreshing ? 'UPDATING' : 'LIVE'}
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: CYAN, fontSize: '11px' }}>
              {currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC
            </span>
          </div>

          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
            <RecordingControlPanel
              tabName="Watchlist"
              onRecordingChange={setIsRecording}
              onRecordingStart={recordCurrentData}
            />
            <button
              onClick={refreshWatchlistData}
              disabled={!selectedWatchlist || refreshing}
              style={{
                background: selectedWatchlist && !refreshing ? GREEN : GRAY,
                color: 'black',
                border: 'none',
                padding: '4px 12px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: selectedWatchlist && !refreshing ? 'pointer' : 'not-allowed',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                opacity: selectedWatchlist && !refreshing ? 1 : 0.5
              }}
            >
              <RefreshCw size={12} />
              REFRESH
            </button>
            <button
              onClick={handleExportCSV}
              disabled={!selectedWatchlist}
              style={{
                background: selectedWatchlist ? CYAN : GRAY,
                color: 'black',
                border: 'none',
                padding: '4px 12px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: selectedWatchlist ? 'pointer' : 'not-allowed',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                opacity: selectedWatchlist ? 1 : 0.5
              }}
            >
              <Download size={12} />
              EXPORT CSV
            </button>
          </div>
        </div>

        {/* Summary Bar */}
        {selectedWatchlist && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px', marginTop: '8px' }}>
            <span style={{ color: GRAY }}>WATCHLIST:</span>
            <span style={{ color: YELLOW, fontWeight: 'bold' }}>
              {selectedWatchlist.name}
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: GRAY }}>STOCKS:</span>
            <span style={{ color: CYAN }}>{stocks.length}</span>
          </div>
        )}
      </div>

      {/* Function Keys Bar */}
      <div style={{
        backgroundColor: PANEL_BG,
        borderBottom: `1px solid ${GRAY}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '4px' }}>
          {[
            { key: "F1", label: "ADD STOCK", action: () => setShowAddStock(true) },
            { key: "F2", label: "REFRESH", action: refreshWatchlistData },
            { key: "F3", label: "EXPORT", action: handleExportCSV },
            { key: "F4", label: "SORT", action: () => setSortBy('CHANGE') },
            { key: "F5", label: "CREATE", action: () => setShowCreateWatchlist(true) },
            { key: "F6", label: "TICKER", action: () => setSortBy('TICKER') },
            { key: "F7", label: "PRICE", action: () => setSortBy('PRICE') },
            { key: "F8", label: "VOLUME", action: () => setSortBy('VOLUME') },
            { key: "F9", label: "UNUSED", action: () => {} },
            { key: "F10", label: "UNUSED", action: () => {} },
            { key: "F11", label: "UNUSED", action: () => {} },
            { key: "F12", label: "HELP", action: () => {} }
          ].map(item => (
            <button
              key={item.key}
              onClick={() => {
                if (item.label === "ADD STOCK" || item.label === "CREATE") {
                  item.action();
                } else if (selectedWatchlist) {
                  item.action();
                }
              }}
              disabled={!selectedWatchlist && item.label !== "CREATE"}
              style={{
                backgroundColor: DARK_BG,
                border: `1px solid ${GRAY}`,
                color: WHITE,
                padding: '4px 6px',
                fontSize: '9px',
                cursor: (selectedWatchlist || item.label === "CREATE") ? 'pointer' : 'not-allowed',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '2px',
                opacity: (!selectedWatchlist && item.label !== "CREATE") ? 0.5 : 1
              }}
            >
              <span style={{ color: YELLOW }}>{item.key}:</span>
              <span>{item.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Sidebar - Watchlist Groups */}
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
          volumeLeaders={volumeLeaders}
        />

        {/* Center Panel - Stock List */}
        {!selectedWatchlist ? (
          <div style={{
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '16px'
          }}>
            <div style={{ color: GRAY, fontSize: '14px' }}>
              Select a watchlist or create a new one to get started
            </div>
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
          />
        )}

        {/* Right Panel - Stock Details */}
        <StockDetailPanel stock={selectedStock} />
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${GRAY}`,
        backgroundColor: PANEL_BG,
        padding: '4px 12px',
        fontSize: '9px',
        color: GRAY,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span>Fincept Watchlist Monitor v2.0.0 | Real-time tracking with yfinance integration</span>
          {selectedWatchlist && (
            <span>
              Watchlist: {selectedWatchlist.name} | Stocks: {stocks.length} |
              Status: <span style={{ color: GREEN }}>CONNECTED</span>
            </span>
          )}
        </div>
      </div>

      {/* Create Watchlist Modal */}
      {showCreateWatchlist && (
        <CreateWatchlistModal
          onClose={() => setShowCreateWatchlist(false)}
          onCreate={handleCreateWatchlist}
          existingColors={watchlists.map(w => w.color)}
        />
      )}

      {/* Add Stock Modal */}
      {showAddStock && selectedWatchlist && (
        <AddStockModal
          onClose={() => setShowAddStock(false)}
          onAdd={handleAddStock}
          existingSymbols={stocks.map(s => s.symbol)}
        />
      )}
    </div>
  );
};

export default WatchlistTab;
