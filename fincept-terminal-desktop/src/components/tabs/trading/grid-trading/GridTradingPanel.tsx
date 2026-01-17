/**
 * Grid Trading Panel
 *
 * Main component for grid trading that integrates all grid trading sub-components.
 * Can be embedded in TradingTab or used standalone.
 */

import React, { useState, useEffect, useCallback } from 'react';
import { Grid, Plus, List, BarChart3, Settings, X, ChevronDown, AlertCircle } from 'lucide-react';
import { GridConfigPanel } from './GridConfigPanel';
import { GridStatusPanel } from './GridStatusPanel';
import { GridVisualization } from './GridVisualization';
import {
  getGridTradingService,
  CryptoGridAdapter,
  StockGridAdapter,
} from '../../../../services/gridTrading';
import type {
  GridConfig,
  GridState,
  IGridBrokerAdapter,
} from '../../../../services/gridTrading/types';
import type { IExchangeAdapter } from '../../../../brokers/crypto/types';
import type { IStockBrokerAdapter } from '../../../../brokers/stocks/types';

interface GridTradingPanelProps {
  // Current symbol/price context
  symbol: string;
  exchange?: string;
  currentPrice: number;

  // Broker info
  brokerType: 'crypto' | 'stock';
  brokerId: string;

  // Broker adapters - pass one of these
  cryptoAdapter?: IExchangeAdapter;
  stockAdapter?: IStockBrokerAdapter;

  // Layout
  variant?: 'compact' | 'full';

  // Callbacks
  onGridCreated?: (grid: GridState) => void;
  onGridStarted?: (gridId: string) => void;
  onGridStopped?: (gridId: string) => void;
}

type View = 'list' | 'create' | 'details';

export function GridTradingPanel({
  symbol,
  exchange,
  currentPrice,
  brokerType,
  brokerId,
  cryptoAdapter,
  stockAdapter,
  variant = 'full',
  onGridCreated,
  onGridStarted,
  onGridStopped,
}: GridTradingPanelProps) {
  const [view, setView] = useState<View>('list');
  const [grids, setGrids] = useState<GridState[]>([]);
  const [selectedGridId, setSelectedGridId] = useState<string | null>(null);
  const [availableBalance, setAvailableBalance] = useState(0);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const service = getGridTradingService();

  // Register adapter on mount
  useEffect(() => {
    let adapter: IGridBrokerAdapter | null = null;

    if (brokerType === 'crypto' && cryptoAdapter) {
      adapter = new CryptoGridAdapter(cryptoAdapter);
    } else if (brokerType === 'stock' && stockAdapter) {
      adapter = new StockGridAdapter(stockAdapter);
    }

    if (adapter) {
      service.registerAdapter(adapter);

      // Fetch available balance
      adapter.getAvailableBalance().then(balance => {
        setAvailableBalance(balance);
      }).catch(console.error);
    }

    return () => {
      if (adapter) {
        service.unregisterAdapter(adapter.brokerId);
        if ('dispose' in adapter) {
          (adapter as any).dispose();
        }
      }
    };
  }, [brokerType, cryptoAdapter, stockAdapter, service]);

  // Load grids for this broker
  useEffect(() => {
    const loadGrids = () => {
      const allGrids = service.getGridsForBroker(brokerId);
      setGrids(allGrids);
    };

    loadGrids();

    // Subscribe to updates
    const unsubscribe = service.onGridUpdate((grid) => {
      if (grid.config.brokerId === brokerId) {
        loadGrids();
      }
    });

    return unsubscribe;
  }, [brokerId, service]);

  // Get selected grid
  const selectedGrid = selectedGridId ? grids.find(g => g.config.id === selectedGridId) : null;

  // Handlers
  const handleCreateGrid = useCallback(async (config: GridConfig) => {
    setIsLoading(true);
    setError(null);

    try {
      const grid = await service.createGrid(config);
      setGrids(service.getGridsForBroker(brokerId));
      setSelectedGridId(grid.config.id);
      setView('details');
      onGridCreated?.(grid);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to create grid');
    } finally {
      setIsLoading(false);
    }
  }, [service, brokerId, onGridCreated]);

  const handleStartGrid = useCallback(async (gridId: string) => {
    setIsLoading(true);
    setError(null);

    try {
      await service.startGrid(gridId);
      setGrids(service.getGridsForBroker(brokerId));
      onGridStarted?.(gridId);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to start grid');
    } finally {
      setIsLoading(false);
    }
  }, [service, brokerId, onGridStarted]);

  const handlePauseGrid = useCallback(async (gridId: string) => {
    try {
      await service.pauseGrid(gridId);
      setGrids(service.getGridsForBroker(brokerId));
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to pause grid');
    }
  }, [service, brokerId]);

  const handleStopGrid = useCallback(async (gridId: string) => {
    try {
      await service.stopGrid(gridId);
      setGrids(service.getGridsForBroker(brokerId));
      onGridStopped?.(gridId);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to stop grid');
    }
  }, [service, brokerId, onGridStopped]);

  const handleDeleteGrid = useCallback(async (gridId: string) => {
    try {
      await service.deleteGrid(gridId);
      setGrids(service.getGridsForBroker(brokerId));
      if (selectedGridId === gridId) {
        setSelectedGridId(null);
        setView('list');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to delete grid');
    }
  }, [service, brokerId, selectedGridId]);

  // Render list view
  const renderList = () => (
    <div className="space-y-3">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Grid className="w-5 h-5 text-[#FF8800]" />
          <span className="text-white font-semibold">Grid Bots</span>
          <span className="text-xs text-[#787878]">({grids.length})</span>
        </div>
        <button
          onClick={() => setView('create')}
          className="flex items-center gap-1 px-3 py-1.5 bg-[#FF8800] hover:bg-[#FF9900] text-black text-sm font-semibold rounded transition-colors"
        >
          <Plus className="w-4 h-4" />
          New Grid
        </button>
      </div>

      {/* Grid list */}
      {grids.length === 0 ? (
        <div className="bg-[#1A1A1A] rounded-lg p-8 text-center">
          <Grid className="w-12 h-12 text-[#4A4A4A] mx-auto mb-3" />
          <div className="text-white mb-1">No Grid Bots</div>
          <div className="text-sm text-[#787878] mb-4">
            Create your first grid trading bot to automate trading
          </div>
          <button
            onClick={() => setView('create')}
            className="px-4 py-2 bg-[#FF8800] hover:bg-[#FF9900] text-black font-semibold rounded transition-colors"
          >
            Create Grid Bot
          </button>
        </div>
      ) : (
        <div className="space-y-2">
          {grids.map(grid => (
            <div
              key={grid.config.id}
              onClick={() => {
                setSelectedGridId(grid.config.id);
                setView('details');
              }}
              className="bg-[#1A1A1A] hover:bg-[#2A2A2A] border border-[#2A2A2A] rounded-lg p-3 cursor-pointer transition-colors"
            >
              <div className="flex items-center justify-between mb-2">
                <div className="flex items-center gap-2">
                  <span className="text-white font-medium">{grid.config.symbol}</span>
                  <div
                    className="px-2 py-0.5 rounded text-xs font-medium"
                    style={{
                      backgroundColor: grid.status === 'running' ? '#00D66F20' : '#78787820',
                      color: grid.status === 'running' ? '#00D66F' : '#787878',
                    }}
                  >
                    {grid.status.toUpperCase()}
                  </div>
                </div>
                <ChevronDown className="w-4 h-4 text-[#787878] -rotate-90" />
              </div>
              <div className="grid grid-cols-3 gap-2 text-xs">
                <div>
                  <div className="text-[#787878]">P&L</div>
                  <div className={grid.totalPnl >= 0 ? 'text-[#00D66F]' : 'text-[#FF3B3B]'}>
                    {grid.totalPnl >= 0 ? '+' : ''}{grid.totalPnl.toFixed(2)}
                  </div>
                </div>
                <div>
                  <div className="text-[#787878]">Trades</div>
                  <div className="text-white">{grid.totalBuys + grid.totalSells}</div>
                </div>
                <div>
                  <div className="text-[#787878]">Levels</div>
                  <div className="text-white">{grid.config.gridLevels}</div>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );

  // Render create view
  const renderCreate = () => (
    <div>
      <div className="flex items-center gap-2 mb-4">
        <button
          onClick={() => setView('list')}
          className="p-1 hover:bg-[#2A2A2A] rounded transition-colors"
        >
          <X className="w-5 h-5 text-[#787878]" />
        </button>
        <span className="text-white font-semibold">Create Grid Bot</span>
      </div>
      <GridConfigPanel
        symbol={symbol}
        exchange={exchange}
        currentPrice={currentPrice}
        brokerType={brokerType}
        brokerId={brokerId}
        availableBalance={availableBalance}
        onCreateGrid={handleCreateGrid}
        onCancel={() => setView('list')}
      />
    </div>
  );

  // Render details view
  const renderDetails = () => {
    if (!selectedGrid) {
      setView('list');
      return null;
    }

    return (
      <div>
        <div className="flex items-center gap-2 mb-4">
          <button
            onClick={() => {
              setSelectedGridId(null);
              setView('list');
            }}
            className="p-1 hover:bg-[#2A2A2A] rounded transition-colors"
          >
            <X className="w-5 h-5 text-[#787878]" />
          </button>
          <span className="text-white font-semibold">Grid Details</span>
        </div>

        <div className={variant === 'full' ? 'grid grid-cols-2 gap-4' : 'space-y-4'}>
          <GridStatusPanel
            grid={selectedGrid}
            onStart={() => handleStartGrid(selectedGrid.config.id)}
            onPause={() => handlePauseGrid(selectedGrid.config.id)}
            onStop={() => handleStopGrid(selectedGrid.config.id)}
            onDelete={() => handleDeleteGrid(selectedGrid.config.id)}
          />
          {variant === 'full' && (
            <GridVisualization grid={selectedGrid} height={400} />
          )}
        </div>
      </div>
    );
  };

  return (
    <div className="bg-[#0F0F0F] border border-[#2A2A2A] rounded-lg p-4">
      {/* Error display */}
      {error && (
        <div className="bg-[#FF3B3B]/10 border border-[#FF3B3B]/30 rounded p-3 mb-4 flex items-start gap-2">
          <AlertCircle className="w-4 h-4 text-[#FF3B3B] flex-shrink-0 mt-0.5" />
          <div className="flex-1">
            <div className="text-sm text-[#FF3B3B]">{error}</div>
          </div>
          <button
            onClick={() => setError(null)}
            className="text-[#FF3B3B] hover:text-white"
          >
            <X className="w-4 h-4" />
          </button>
        </div>
      )}

      {/* Loading overlay */}
      {isLoading && (
        <div className="absolute inset-0 bg-black/50 flex items-center justify-center z-10">
          <div className="animate-spin w-8 h-8 border-2 border-[#FF8800] border-t-transparent rounded-full" />
        </div>
      )}

      {/* Content */}
      {view === 'list' && renderList()}
      {view === 'create' && renderCreate()}
      {view === 'details' && renderDetails()}
    </div>
  );
}

export default GridTradingPanel;
