/**
 * Grid Strategy Configuration Panel
 *
 * Allows users to create and manage grid trading strategies.
 * Grid trading places buy/sell orders at regular price intervals.
 */

import React, { useState, useEffect, useReducer, useRef } from 'react';
import {
  Grid3X3, RefreshCw, Play, AlertTriangle,
  Loader2, Info,
  ChevronDown, ChevronUp, Plus, Check,
} from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import {
  alphaArenaEnhancedService,
  type GridConfig,
} from '../services/alphaArenaEnhancedService';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#3B82F6',
  YELLOW: '#EAB308',
  PURPLE: '#8B5CF6',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

const TERMINAL_FONT = '"IBM Plex Mono", "Consolas", monospace';

const GRID_STYLES = `
  .grid-panel *::-webkit-scrollbar { width: 6px; height: 6px; }
  .grid-panel *::-webkit-scrollbar-track { background: #000000; }
  .grid-panel *::-webkit-scrollbar-thumb { background: #2A2A2A; }
  @keyframes grid-spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
`;

interface GridStrategyPanelProps {
  symbol?: string;
  currentPrice?: number;
  onGridCreated?: (name: string) => void;
}

interface GridAgentStatus {
  name: string;
  symbol: string;
  gridLevels: number[];
  triggeredLevels: number[];
  activeOrders: number;
  totalProfit: number;
  tradesExecuted: number;
  isActive: boolean;
}

// Create state machine
type CreateState =
  | { status: 'idle' }
  | { status: 'creating' }
  | { status: 'success'; message: string }
  | { status: 'error'; error: string };

type CreateAction =
  | { type: 'CREATE_START' }
  | { type: 'CREATE_SUCCESS'; message: string }
  | { type: 'CREATE_ERROR'; error: string }
  | { type: 'RESET' };

function createReducer(_state: CreateState, action: CreateAction): CreateState {
  switch (action.type) {
    case 'CREATE_START': return { status: 'creating' };
    case 'CREATE_SUCCESS': return { status: 'success', message: action.message };
    case 'CREATE_ERROR': return { status: 'error', error: action.error };
    case 'RESET': return { status: 'idle' };
    default: return _state;
  }
}

const GridStrategyPanel: React.FC<GridStrategyPanelProps> = ({
  symbol = 'BTC',
  currentPrice = 50000,
  onGridCreated,
}) => {
  const [createState, dispatchCreate] = useReducer(createReducer, { status: 'idle' });
  const [activeGrids, setActiveGrids] = useState<GridAgentStatus[]>([]);
  const mountedRef = useRef(true);
  const [hoveredBtn, setHoveredBtn] = useState<string | null>(null);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);
  const [showCreateForm, setShowCreateForm] = useState(false);
  const [expandedGrid, setExpandedGrid] = useState<string | null>(null);

  // Form state
  const [gridConfig, setGridConfig] = useState<GridConfig>({
    symbol: symbol,
    lower_price: currentPrice * 0.9,
    upper_price: currentPrice * 1.1,
    num_grids: 10,
    quantity_per_grid: 0.001,
    take_profit_pct: 2.0,
    stop_loss_pct: 5.0,
  });
  const [gridName, setGridName] = useState('');

  useEffect(() => {
    setGridConfig(prev => ({
      ...prev,
      symbol,
      lower_price: currentPrice * 0.9,
      upper_price: currentPrice * 1.1,
    }));
  }, [symbol, currentPrice]);

  const calculateGridSpacing = () => {
    const range = gridConfig.upper_price - gridConfig.lower_price;
    const divisor = Math.max(gridConfig.num_grids - 1, 1);
    return range / divisor;
  };

  const calculateTotalInvestment = () => {
    return gridConfig.quantity_per_grid * gridConfig.num_grids * gridConfig.lower_price;
  };

  const generateGridLevels = () => {
    const levels: number[] = [];
    const spacing = calculateGridSpacing();
    for (let i = 0; i < gridConfig.num_grids; i++) {
      levels.push(gridConfig.lower_price + (spacing * i));
    }
    return levels;
  };

  const handleCreateGrid = async () => {
    if (createState.status === 'creating') return;

    if (!gridName.trim()) {
      dispatchCreate({ type: 'CREATE_ERROR', error: 'Please enter a name for the grid strategy' });
      return;
    }
    if (gridConfig.lower_price >= gridConfig.upper_price) {
      dispatchCreate({ type: 'CREATE_ERROR', error: 'Lower price must be less than upper price' });
      return;
    }
    if (!Number.isInteger(gridConfig.num_grids) || gridConfig.num_grids < 2) {
      dispatchCreate({ type: 'CREATE_ERROR', error: 'Number of grids must be an integer >= 2' });
      return;
    }
    if (gridConfig.quantity_per_grid <= 0) {
      dispatchCreate({ type: 'CREATE_ERROR', error: 'Quantity per grid must be positive' });
      return;
    }

    dispatchCreate({ type: 'CREATE_START' });

    try {
      const result = await alphaArenaEnhancedService.createGridAgent(gridName, gridConfig);
      if (!mountedRef.current) return;

      if (result.success) {
        dispatchCreate({ type: 'CREATE_SUCCESS', message: `Grid strategy "${gridName}" created successfully!` });
        setShowCreateForm(false);
        setGridName('');
        onGridCreated?.(gridName);

        setActiveGrids(prev => [...prev, {
          name: gridName,
          symbol: gridConfig.symbol,
          gridLevels: result.grid_levels || generateGridLevels(),
          triggeredLevels: [],
          activeOrders: 0,
          totalProfit: 0,
          tradesExecuted: 0,
          isActive: true,
        }]);
      } else {
        dispatchCreate({ type: 'CREATE_ERROR', error: result.error || 'Failed to create grid strategy' });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatchCreate({ type: 'CREATE_ERROR', error: err instanceof Error ? err.message : 'Failed to create grid strategy' });
    }
  };

  const fetchGridStatus = async (name: string) => {
    try {
      const result = await alphaArenaEnhancedService.getGridStatus(name);
      const gs = result.grid_status;
      if (result.success && gs) {
        setActiveGrids(prev => prev.map(grid =>
          grid.name === name
            ? {
                ...grid,
                gridLevels: gs.grid_levels ?? grid.gridLevels,
                triggeredLevels: gs.triggered_levels ?? grid.triggeredLevels,
                activeOrders: gs.active_orders ?? grid.activeOrders,
                totalProfit: gs.total_profit ?? grid.totalProfit,
                tradesExecuted: gs.trades_executed ?? grid.tradesExecuted,
              }
            : grid
        ));
      }
    } catch (err) {
      console.error('Error fetching grid status:', err);
    }
  };

  const renderGridVisualization = (levels: number[], triggeredLevels: number[]) => {
    const maxPrice = Math.max(...levels);
    const minPrice = Math.min(...levels);
    const range = maxPrice - minPrice;

    return (
      <div style={{ position: 'relative', height: '128px', margin: '8px 0' }}>
        {levels.map((level, idx) => {
          const position = ((maxPrice - level) / range) * 100;
          const isTriggered = triggeredLevels.includes(level);
          const isCurrentPrice = Math.abs(level - currentPrice) < range * 0.05;

          return (
            <div
              key={idx}
              style={{
                position: 'absolute',
                left: 0,
                right: 0,
                display: 'flex',
                alignItems: 'center',
                top: `${position}%`,
              }}
            >
              <div style={{
                height: '1px',
                flex: 1,
                backgroundColor: isTriggered ? FINCEPT.GREEN : isCurrentPrice ? FINCEPT.ORANGE : FINCEPT.BORDER,
              }} />
              <span style={{
                fontSize: '10px',
                marginLeft: '8px',
                width: '64px',
                textAlign: 'right',
                fontFamily: TERMINAL_FONT,
                color: isTriggered ? FINCEPT.GREEN : isCurrentPrice ? FINCEPT.ORANGE : FINCEPT.GRAY,
              }}>
                ${level.toFixed(0)}
              </span>
            </div>
          );
        })}
        {/* Current price indicator */}
        <div style={{
          position: 'absolute',
          left: 0,
          display: 'flex',
          alignItems: 'center',
          top: `${((maxPrice - currentPrice) / range) * 100}%`,
          transform: 'translateY(-50%)',
        }}>
          <div style={{
            width: '6px',
            height: '6px',
            backgroundColor: FINCEPT.ORANGE,
          }} />
          <span style={{ fontSize: '10px', marginLeft: '4px', color: FINCEPT.ORANGE, fontFamily: TERMINAL_FONT }}>
            Current
          </span>
        </div>
      </div>
    );
  };

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '6px 8px',
    fontSize: '11px',
    fontFamily: TERMINAL_FONT,
    backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE,
    border: `1px solid ${FINCEPT.BORDER}`,
    outline: 'none',
  };

  return (
    <div className="grid-panel" style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      overflow: 'hidden',
      fontFamily: TERMINAL_FONT,
    }}>
      <style>{GRID_STYLES}</style>

      {/* Header */}
      <div style={{
        padding: '10px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Grid3X3 size={16} style={{ color: FINCEPT.BLUE }} />
          <span style={{ fontWeight: 600, fontSize: '12px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            GRID TRADING
          </span>
          {activeGrids.length > 0 && (
            <span style={{
              padding: '1px 8px',
              fontSize: '10px',
              backgroundColor: FINCEPT.BLUE + '20',
              color: FINCEPT.BLUE,
              fontFamily: TERMINAL_FONT,
            }}>
              {activeGrids.length} active
            </span>
          )}
        </div>
        <button
          onClick={() => setShowCreateForm(!showCreateForm)}
          onMouseEnter={() => setHoveredBtn('add')}
          onMouseLeave={() => setHoveredBtn(null)}
          style={{
            padding: '4px',
            backgroundColor: hoveredBtn === 'add' ? FINCEPT.HOVER : 'transparent',
            border: 'none',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
          }}
        >
          <Plus size={14} style={{ color: FINCEPT.GRAY }} />
        </button>
      </div>

      {/* Messages */}
      {createState.status === 'error' && (
        <div style={{
          padding: '8px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          backgroundColor: FINCEPT.RED + '10',
        }}>
          <AlertTriangle size={14} style={{ color: FINCEPT.RED }} />
          <span style={{ fontSize: '11px', color: FINCEPT.RED, fontFamily: TERMINAL_FONT }}>{createState.error}</span>
        </div>
      )}
      {createState.status === 'success' && (
        <div style={{
          padding: '8px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          backgroundColor: FINCEPT.GREEN + '10',
        }}>
          <Check size={14} style={{ color: FINCEPT.GREEN }} />
          <span style={{ fontSize: '11px', color: FINCEPT.GREEN, fontFamily: TERMINAL_FONT }}>{createState.message}</span>
        </div>
      )}

      {/* Create Form */}
      {showCreateForm && (
        <div style={{
          padding: '12px 16px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.CARD_BG,
        }}>
          <div style={{ fontSize: '11px', fontWeight: 500, marginBottom: '12px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            CREATE GRID STRATEGY
          </div>

          {/* Name Input */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{ fontSize: '10px', marginBottom: '4px', display: 'block', color: FINCEPT.GRAY }}>Strategy Name</label>
            <input
              type="text"
              value={gridName}
              onChange={(e) => setGridName(e.target.value)}
              placeholder="My Grid Strategy"
              style={inputStyle}
            />
          </div>

          {/* Price Range */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '12px' }}>
            <div>
              <label style={{ fontSize: '10px', marginBottom: '4px', display: 'block', color: FINCEPT.GRAY }}>Lower Price</label>
              <input
                type="text"
                inputMode="decimal"
                value={gridConfig.lower_price}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setGridConfig(prev => ({ ...prev, lower_price: parseFloat(v) || 0 })); }}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={{ fontSize: '10px', marginBottom: '4px', display: 'block', color: FINCEPT.GRAY }}>Upper Price</label>
              <input
                type="text"
                inputMode="decimal"
                value={gridConfig.upper_price}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setGridConfig(prev => ({ ...prev, upper_price: parseFloat(v) || 0 })); }}
                style={inputStyle}
              />
            </div>
          </div>

          {/* Grid Settings */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '12px' }}>
            <div>
              <label style={{ fontSize: '10px', marginBottom: '4px', display: 'block', color: FINCEPT.GRAY }}>Number of Grids</label>
              <input
                type="text"
                inputMode="numeric"
                value={gridConfig.num_grids}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setGridConfig(prev => ({ ...prev, num_grids: Math.min(parseInt(v) || 5, 50) })); }}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={{ fontSize: '10px', marginBottom: '4px', display: 'block', color: FINCEPT.GRAY }}>Qty per Grid</label>
              <input
                type="text"
                inputMode="decimal"
                value={gridConfig.quantity_per_grid}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setGridConfig(prev => ({ ...prev, quantity_per_grid: parseFloat(v) || 0.001 })); }}
                style={inputStyle}
              />
            </div>
          </div>

          {/* Summary */}
          <div style={{
            padding: '8px',
            marginBottom: '12px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', fontSize: '11px' }}>
              <div>
                <span style={{ color: FINCEPT.GRAY }}>Grid Spacing: </span>
                <span style={{ color: FINCEPT.WHITE }}>${calculateGridSpacing().toFixed(2)}</span>
              </div>
              <div>
                <span style={{ color: FINCEPT.GRAY }}>Est. Investment: </span>
                <span style={{ color: FINCEPT.WHITE }}>${calculateTotalInvestment().toFixed(2)}</span>
              </div>
            </div>
          </div>

          {/* Preview */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{ fontSize: '10px', marginBottom: '4px', display: 'block', color: FINCEPT.GRAY }}>Grid Preview</label>
            {renderGridVisualization(generateGridLevels(), [])}
          </div>

          {/* Actions */}
          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={() => setShowCreateForm(false)}
              onMouseEnter={() => setHoveredBtn('cancel')}
              onMouseLeave={() => setHoveredBtn(null)}
              style={{
                flex: 1,
                padding: '6px',
                fontSize: '11px',
                fontWeight: 600,
                fontFamily: TERMINAL_FONT,
                backgroundColor: hoveredBtn === 'cancel' ? FINCEPT.HOVER : FINCEPT.BORDER,
                color: FINCEPT.WHITE,
                border: 'none',
                cursor: 'pointer',
                letterSpacing: '0.5px',
              }}
            >
              CANCEL
            </button>
            <button
              onClick={handleCreateGrid}
              disabled={createState.status === 'creating'}
              style={{
                flex: 1,
                padding: '6px',
                fontSize: '11px',
                fontWeight: 600,
                fontFamily: TERMINAL_FONT,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '4px',
                backgroundColor: FINCEPT.BLUE,
                color: FINCEPT.WHITE,
                border: 'none',
                cursor: createState.status === 'creating' ? 'not-allowed' : 'pointer',
                opacity: createState.status === 'creating' ? 0.5 : 1,
                letterSpacing: '0.5px',
              }}
            >
              {createState.status === 'creating' ? (
                <Loader2 size={12} style={{ animation: 'grid-spin 1s linear infinite' }} />
              ) : (
                <Play size={12} />
              )}
              CREATE GRID
            </button>
          </div>
        </div>
      )}

      {/* Active Grids */}
      <div style={{ maxHeight: '256px', overflowY: 'auto' }}>
        {activeGrids.length === 0 ? (
          <div style={{ padding: '32px 16px', textAlign: 'center' }}>
            <Grid3X3 size={32} style={{ color: FINCEPT.GRAY, opacity: 0.3, margin: '0 auto 8px' }} />
            <p style={{ fontSize: '12px', color: FINCEPT.GRAY }}>No active grid strategies</p>
            <p style={{ fontSize: '10px', marginTop: '4px', color: FINCEPT.GRAY }}>
              Create a grid to start automated trading
            </p>
          </div>
        ) : (
          <div>
            {activeGrids.map((grid, idx) => (
              <div key={grid.name} style={{
                padding: '12px',
                borderBottom: idx < activeGrids.length - 1 ? `1px solid ${FINCEPT.BORDER}` : 'none',
              }}>
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
                  <button
                    onClick={() => setExpandedGrid(expandedGrid === grid.name ? null : grid.name)}
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                      background: 'none',
                      border: 'none',
                      cursor: 'pointer',
                      fontFamily: TERMINAL_FONT,
                      padding: 0,
                    }}
                  >
                    {expandedGrid === grid.name ? (
                      <ChevronUp size={12} style={{ color: FINCEPT.GRAY }} />
                    ) : (
                      <ChevronDown size={12} style={{ color: FINCEPT.GRAY }} />
                    )}
                    <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>{grid.name}</span>
                    <span style={{
                      padding: '1px 6px',
                      fontSize: '9px',
                      fontFamily: TERMINAL_FONT,
                      backgroundColor: grid.isActive ? FINCEPT.GREEN + '20' : FINCEPT.GRAY + '20',
                      color: grid.isActive ? FINCEPT.GREEN : FINCEPT.GRAY,
                    }}>
                      {grid.isActive ? 'ACTIVE' : 'PAUSED'}
                    </span>
                  </button>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <button
                      onClick={() => fetchGridStatus(grid.name)}
                      onMouseEnter={() => setHoveredBtn(`refresh-${grid.name}`)}
                      onMouseLeave={() => setHoveredBtn(null)}
                      style={{
                        padding: '4px',
                        backgroundColor: hoveredBtn === `refresh-${grid.name}` ? FINCEPT.HOVER : 'transparent',
                        border: 'none',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                      }}
                    >
                      <RefreshCw size={12} style={{ color: FINCEPT.GRAY }} />
                    </button>
                  </div>
                </div>

                {/* Quick Stats */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', fontSize: '11px' }}>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Trades: </span>
                    <span style={{ color: FINCEPT.WHITE }}>{grid.tradesExecuted}</span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Profit: </span>
                    <span style={{ color: grid.totalProfit >= 0 ? FINCEPT.GREEN : FINCEPT.RED }}>
                      ${grid.totalProfit.toFixed(2)}
                    </span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Active: </span>
                    <span style={{ color: FINCEPT.WHITE }}>{grid.activeOrders}</span>
                  </div>
                </div>

                {/* Expanded View */}
                {expandedGrid === grid.name && (
                  <div style={{ marginTop: '12px', paddingTop: '12px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
                    <div style={{ fontSize: '10px', marginBottom: '8px', color: FINCEPT.GRAY }}>Grid Levels</div>
                    {renderGridVisualization(grid.gridLevels, grid.triggeredLevels)}
                    <div style={{ fontSize: '10px', marginTop: '8px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
                      <Info size={10} />
                      {grid.triggeredLevels.length} of {grid.gridLevels.length} levels triggered
                    </div>
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Footer */}
      <div style={{
        padding: '6px 16px',
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '9px',
        backgroundColor: FINCEPT.HEADER_BG,
        color: FINCEPT.GRAY,
        display: 'flex',
        alignItems: 'center',
        gap: '4px',
      }}>
        <Info size={10} />
        Grid strategies execute automatically based on price levels
      </div>
    </div>
  );
};

export default withErrorBoundary(GridStrategyPanel, { name: 'AlphaArena.GridStrategyPanel' });
