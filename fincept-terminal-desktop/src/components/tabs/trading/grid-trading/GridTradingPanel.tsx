/**
 * Grid Trading Panel
 * Fincept Terminal Style - Inline styles matching the trading UI design
 */

import React, { useState, useEffect, useCallback } from 'react';
import { Grid, Plus, ChevronLeft, AlertCircle, X, Loader, RefreshCw } from 'lucide-react';
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

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
};

interface GridTradingPanelProps {
  symbol: string;
  exchange?: string;
  currentPrice: number;
  brokerType: 'crypto' | 'stock';
  brokerId: string;
  cryptoAdapter?: IExchangeAdapter;
  stockAdapter?: IStockBrokerAdapter;
  variant?: 'compact' | 'full';
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

  useEffect(() => {
    let adapter: IGridBrokerAdapter | null = null;

    if (brokerType === 'crypto' && cryptoAdapter) {
      adapter = new CryptoGridAdapter(cryptoAdapter);
    } else if (brokerType === 'stock' && stockAdapter) {
      adapter = new StockGridAdapter(stockAdapter);
    }

    if (adapter) {
      service.registerAdapter(adapter);
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

  useEffect(() => {
    const loadGrids = () => {
      const allGrids = service.getGridsForBroker(brokerId);
      setGrids(allGrids);
    };

    loadGrids();
    const unsubscribe = service.onGridUpdate((grid) => {
      if (grid.config.brokerId === brokerId) {
        loadGrids();
      }
    });

    return unsubscribe;
  }, [brokerId, service]);

  const selectedGrid = selectedGridId ? grids.find(g => g.config.id === selectedGridId) : null;

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

  const btnStyle: React.CSSProperties = {
    padding: '6px 12px',
    border: `1px solid ${FINCEPT.BORDER}`,
    backgroundColor: 'transparent',
    color: FINCEPT.GRAY,
    cursor: 'pointer',
    fontSize: '10px',
    fontWeight: 700,
    fontFamily: '"IBM Plex Mono", monospace',
    transition: 'all 0.15s',
    display: 'flex',
    alignItems: 'center',
    gap: '4px',
  };

  const renderList = () => (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Grid size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            GRID BOTS
          </span>
          <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>({grids.length})</span>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={() => setView('create')}
            style={{
              ...btnStyle,
              backgroundColor: FINCEPT.GREEN,
              color: FINCEPT.DARK_BG,
              borderColor: FINCEPT.GREEN,
            }}
          >
            <Plus size={12} />
            NEW GRID
          </button>
          <button
            onClick={() => setGrids(service.getGridsForBroker(brokerId))}
            style={{ ...btnStyle, padding: '6px 8px' }}
            title="Refresh"
          >
            <RefreshCw size={12} />
          </button>
        </div>
      </div>

      <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
        {grids.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            padding: '40px 20px',
            textAlign: 'center',
          }}>
            <Grid size={40} color={FINCEPT.BORDER} style={{ marginBottom: 12 }} />
            <div style={{ fontSize: '12px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: 4 }}>
              NO GRID BOTS
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: 16 }}>
              Create your first grid trading bot to automate trading
            </div>
            <button
              onClick={() => setView('create')}
              style={{
                ...btnStyle,
                backgroundColor: FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                borderColor: FINCEPT.ORANGE,
                padding: '8px 16px',
              }}
            >
              <Grid size={12} />
              CREATE GRID BOT
            </button>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            {grids.map(grid => {
              const statusColor = grid.status === 'running' ? FINCEPT.GREEN :
                                  grid.status === 'paused' ? FINCEPT.YELLOW :
                                  grid.status === 'error' ? FINCEPT.RED : FINCEPT.GRAY;
              return (
                <div
                  key={grid.config.id}
                  onClick={() => {
                    setSelectedGridId(grid.config.id);
                    setView('details');
                  }}
                  style={{
                    padding: '12px',
                    backgroundColor: FINCEPT.HEADER_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    cursor: 'pointer',
                    transition: 'all 0.15s',
                  }}
                  onMouseEnter={e => {
                    e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }}
                  onMouseLeave={e => {
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 8 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>
                        {grid.config.symbol}
                      </span>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: `${statusColor}20`,
                        color: statusColor,
                        fontSize: '9px',
                        fontWeight: 700,
                        textTransform: 'uppercase',
                      }}>
                        {grid.status}
                      </span>
                    </div>
                    <ChevronLeft size={14} color={FINCEPT.GRAY} style={{ transform: 'rotate(180deg)' }} />
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px', fontSize: '10px' }}>
                    <div>
                      <div style={{ color: FINCEPT.GRAY, marginBottom: 2 }}>P&L</div>
                      <div style={{ color: grid.totalPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED, fontWeight: 600 }}>
                        {grid.totalPnl >= 0 ? '+' : ''}{grid.totalPnl.toFixed(2)}
                      </div>
                    </div>
                    <div>
                      <div style={{ color: FINCEPT.GRAY, marginBottom: 2 }}>TRADES</div>
                      <div style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>
                        {grid.totalBuys + grid.totalSells}
                      </div>
                    </div>
                    <div>
                      <div style={{ color: FINCEPT.GRAY, marginBottom: 2 }}>LEVELS</div>
                      <div style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>
                        {grid.config.gridLevels}
                      </div>
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );

  const renderCreate = () => (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        padding: '8px 12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
      }}>
        <button
          onClick={() => setView('list')}
          style={{
            background: 'none',
            border: 'none',
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            padding: '4px',
            display: 'flex',
            alignItems: 'center',
          }}
        >
          <ChevronLeft size={16} />
        </button>
        <Grid size={14} color={FINCEPT.ORANGE} />
        <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
          CREATE GRID BOT
        </span>
      </div>
      <div style={{ flex: 1, overflow: 'auto' }}>
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
    </div>
  );

  const renderDetails = () => {
    if (!selectedGrid) {
      setView('list');
      return null;
    }

    return (
      <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '8px 12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.HEADER_BG,
        }}>
          <button
            onClick={() => {
              setSelectedGridId(null);
              setView('list');
            }}
            style={{
              background: 'none',
              border: 'none',
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              padding: '4px',
              display: 'flex',
              alignItems: 'center',
            }}
          >
            <ChevronLeft size={16} />
          </button>
          <Grid size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            {selectedGrid.config.symbol} GRID
          </span>
        </div>
        <div style={{ flex: 1, overflow: 'auto', padding: variant === 'full' ? '12px' : '0' }}>
          {variant === 'full' ? (
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <GridStatusPanel
                grid={selectedGrid}
                onStart={() => handleStartGrid(selectedGrid.config.id)}
                onPause={() => handlePauseGrid(selectedGrid.config.id)}
                onStop={() => handleStopGrid(selectedGrid.config.id)}
                onDelete={() => handleDeleteGrid(selectedGrid.config.id)}
              />
              <GridVisualization grid={selectedGrid} height={400} />
            </div>
          ) : (
            <GridStatusPanel
              grid={selectedGrid}
              onStart={() => handleStartGrid(selectedGrid.config.id)}
              onPause={() => handlePauseGrid(selectedGrid.config.id)}
              onStop={() => handleStopGrid(selectedGrid.config.id)}
              onDelete={() => handleDeleteGrid(selectedGrid.config.id)}
            />
          )}
        </div>
      </div>
    );
  };

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      backgroundColor: FINCEPT.PANEL_BG,
      fontFamily: '"IBM Plex Mono", monospace',
      position: 'relative',
      overflow: 'hidden',
    }}>
      {error && (
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '8px 12px',
          backgroundColor: `${FINCEPT.RED}15`,
          borderBottom: `1px solid ${FINCEPT.RED}30`,
        }}>
          <AlertCircle size={14} color={FINCEPT.RED} />
          <span style={{ flex: 1, fontSize: '10px', color: FINCEPT.RED }}>{error}</span>
          <button
            onClick={() => setError(null)}
            style={{ background: 'none', border: 'none', color: FINCEPT.RED, cursor: 'pointer', padding: '2px' }}
          >
            <X size={12} />
          </button>
        </div>
      )}

      {isLoading && (
        <div style={{
          position: 'absolute',
          inset: 0,
          backgroundColor: 'rgba(0,0,0,0.6)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 10,
        }}>
          <Loader size={24} color={FINCEPT.ORANGE} style={{ animation: 'spin 1s linear infinite' }} />
        </div>
      )}

      {view === 'list' && renderList()}
      {view === 'create' && renderCreate()}
      {view === 'details' && renderDetails()}
    </div>
  );
}

export default GridTradingPanel;
