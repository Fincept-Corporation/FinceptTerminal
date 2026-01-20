/**
 * Grid Strategy Configuration Panel
 *
 * Allows users to create and manage grid trading strategies.
 * Grid trading places buy/sell orders at regular price intervals.
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  Grid3X3, Settings, RefreshCw, Play, Pause, AlertTriangle,
  TrendingUp, TrendingDown, DollarSign, Loader2, Info,
  ChevronDown, ChevronUp, Plus, Trash2, Check,
} from 'lucide-react';
import {
  alphaArenaEnhancedService,
  type GridConfig,
} from '../services/alphaArenaEnhancedService';

const COLORS = {
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
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
};

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

const GridStrategyPanel: React.FC<GridStrategyPanelProps> = ({
  symbol = 'BTC',
  currentPrice = 50000,
  onGridCreated,
}) => {
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);
  const [activeGrids, setActiveGrids] = useState<GridAgentStatus[]>([]);
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
    return range / (gridConfig.num_grids - 1);
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
    if (!gridName.trim()) {
      setError('Please enter a name for the grid strategy');
      return;
    }

    setIsLoading(true);
    setError(null);
    setSuccess(null);

    try {
      const result = await alphaArenaEnhancedService.createGridAgent(gridName, gridConfig);

      if (result.success) {
        setSuccess(`Grid strategy "${gridName}" created successfully!`);
        setShowCreateForm(false);
        setGridName('');
        onGridCreated?.(gridName);

        // Add to active grids
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
        setError(result.error || 'Failed to create grid strategy');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to create grid strategy');
    } finally {
      setIsLoading(false);
    }
  };

  const fetchGridStatus = async (name: string) => {
    try {
      const result = await alphaArenaEnhancedService.getGridStatus(name);
      if (result.success && result.grid_status) {
        setActiveGrids(prev => prev.map(grid =>
          grid.name === name
            ? {
                ...grid,
                gridLevels: result.grid_status!.grid_levels,
                triggeredLevels: result.grid_status!.triggered_levels,
                activeOrders: result.grid_status!.active_orders,
                totalProfit: result.grid_status!.total_profit,
                tradesExecuted: result.grid_status!.trades_executed,
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
      <div className="relative h-32 my-2">
        {levels.map((level, idx) => {
          const position = ((maxPrice - level) / range) * 100;
          const isTriggered = triggeredLevels.includes(level);
          const isCurrentPrice = Math.abs(level - currentPrice) < range * 0.05;

          return (
            <div
              key={idx}
              className="absolute left-0 right-0 flex items-center"
              style={{ top: `${position}%` }}
            >
              <div
                className="h-0.5 flex-1"
                style={{
                  backgroundColor: isTriggered ? COLORS.GREEN : isCurrentPrice ? COLORS.ORANGE : COLORS.BORDER,
                }}
              />
              <span
                className="text-xs ml-2 w-16 text-right"
                style={{
                  color: isTriggered ? COLORS.GREEN : isCurrentPrice ? COLORS.ORANGE : COLORS.GRAY,
                }}
              >
                ${level.toFixed(0)}
              </span>
            </div>
          );
        })}
        {/* Current price indicator */}
        <div
          className="absolute left-0 flex items-center"
          style={{
            top: `${((maxPrice - currentPrice) / range) * 100}%`,
            transform: 'translateY(-50%)',
          }}
        >
          <div
            className="w-2 h-2 rounded-full"
            style={{ backgroundColor: COLORS.ORANGE }}
          />
          <span className="text-xs ml-1" style={{ color: COLORS.ORANGE }}>
            Current
          </span>
        </div>
      </div>
    );
  };

  return (
    <div
      className="rounded-lg overflow-hidden"
      style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}
    >
      {/* Header */}
      <div className="px-4 py-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-2">
          <Grid3X3 size={16} style={{ color: COLORS.BLUE }} />
          <span className="font-semibold text-sm" style={{ color: COLORS.WHITE }}>
            Grid Trading
          </span>
          {activeGrids.length > 0 && (
            <span
              className="px-2 py-0.5 rounded-full text-xs"
              style={{ backgroundColor: COLORS.BLUE + '20', color: COLORS.BLUE }}
            >
              {activeGrids.length} active
            </span>
          )}
        </div>
        <button
          onClick={() => setShowCreateForm(!showCreateForm)}
          className="p-1.5 rounded transition-colors hover:bg-[#1A1A1A]"
        >
          <Plus size={14} style={{ color: COLORS.GRAY }} />
        </button>
      </div>

      {/* Messages */}
      {error && (
        <div className="px-4 py-2 flex items-center gap-2" style={{ backgroundColor: COLORS.RED + '10' }}>
          <AlertTriangle size={14} style={{ color: COLORS.RED }} />
          <span className="text-xs" style={{ color: COLORS.RED }}>{error}</span>
        </div>
      )}
      {success && (
        <div className="px-4 py-2 flex items-center gap-2" style={{ backgroundColor: COLORS.GREEN + '10' }}>
          <Check size={14} style={{ color: COLORS.GREEN }} />
          <span className="text-xs" style={{ color: COLORS.GREEN }}>{success}</span>
        </div>
      )}

      {/* Create Form */}
      {showCreateForm && (
        <div className="px-4 py-3 border-b" style={{ borderColor: COLORS.BORDER, backgroundColor: COLORS.CARD_BG }}>
          <h4 className="text-xs font-medium mb-3" style={{ color: COLORS.WHITE }}>Create Grid Strategy</h4>

          {/* Name Input */}
          <div className="mb-3">
            <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Strategy Name</label>
            <input
              type="text"
              value={gridName}
              onChange={(e) => setGridName(e.target.value)}
              placeholder="My Grid Strategy"
              className="w-full px-2 py-1.5 rounded text-xs"
              style={{
                backgroundColor: COLORS.DARK_BG,
                color: COLORS.WHITE,
                border: `1px solid ${COLORS.BORDER}`,
              }}
            />
          </div>

          {/* Price Range */}
          <div className="grid grid-cols-2 gap-2 mb-3">
            <div>
              <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Lower Price</label>
              <input
                type="number"
                value={gridConfig.lower_price}
                onChange={(e) => setGridConfig(prev => ({ ...prev, lower_price: parseFloat(e.target.value) || 0 }))}
                className="w-full px-2 py-1.5 rounded text-xs"
                style={{
                  backgroundColor: COLORS.DARK_BG,
                  color: COLORS.WHITE,
                  border: `1px solid ${COLORS.BORDER}`,
                }}
              />
            </div>
            <div>
              <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Upper Price</label>
              <input
                type="number"
                value={gridConfig.upper_price}
                onChange={(e) => setGridConfig(prev => ({ ...prev, upper_price: parseFloat(e.target.value) || 0 }))}
                className="w-full px-2 py-1.5 rounded text-xs"
                style={{
                  backgroundColor: COLORS.DARK_BG,
                  color: COLORS.WHITE,
                  border: `1px solid ${COLORS.BORDER}`,
                }}
              />
            </div>
          </div>

          {/* Grid Settings */}
          <div className="grid grid-cols-2 gap-2 mb-3">
            <div>
              <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Number of Grids</label>
              <input
                type="number"
                value={gridConfig.num_grids}
                onChange={(e) => setGridConfig(prev => ({ ...prev, num_grids: parseInt(e.target.value) || 5 }))}
                min={3}
                max={50}
                className="w-full px-2 py-1.5 rounded text-xs"
                style={{
                  backgroundColor: COLORS.DARK_BG,
                  color: COLORS.WHITE,
                  border: `1px solid ${COLORS.BORDER}`,
                }}
              />
            </div>
            <div>
              <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Qty per Grid</label>
              <input
                type="number"
                value={gridConfig.quantity_per_grid}
                onChange={(e) => setGridConfig(prev => ({ ...prev, quantity_per_grid: parseFloat(e.target.value) || 0.001 }))}
                step={0.001}
                className="w-full px-2 py-1.5 rounded text-xs"
                style={{
                  backgroundColor: COLORS.DARK_BG,
                  color: COLORS.WHITE,
                  border: `1px solid ${COLORS.BORDER}`,
                }}
              />
            </div>
          </div>

          {/* Summary */}
          <div className="p-2 rounded mb-3" style={{ backgroundColor: COLORS.DARK_BG }}>
            <div className="grid grid-cols-2 gap-2 text-xs">
              <div>
                <span style={{ color: COLORS.GRAY }}>Grid Spacing: </span>
                <span style={{ color: COLORS.WHITE }}>${calculateGridSpacing().toFixed(2)}</span>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Est. Investment: </span>
                <span style={{ color: COLORS.WHITE }}>${calculateTotalInvestment().toFixed(2)}</span>
              </div>
            </div>
          </div>

          {/* Preview */}
          <div className="mb-3">
            <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Grid Preview</label>
            {renderGridVisualization(generateGridLevels(), [])}
          </div>

          {/* Actions */}
          <div className="flex gap-2">
            <button
              onClick={() => setShowCreateForm(false)}
              className="flex-1 py-1.5 rounded text-xs font-medium"
              style={{
                backgroundColor: COLORS.BORDER,
                color: COLORS.WHITE,
              }}
            >
              Cancel
            </button>
            <button
              onClick={handleCreateGrid}
              disabled={isLoading}
              className="flex-1 py-1.5 rounded text-xs font-medium flex items-center justify-center gap-1"
              style={{
                backgroundColor: COLORS.BLUE,
                color: COLORS.WHITE,
                opacity: isLoading ? 0.5 : 1,
              }}
            >
              {isLoading ? (
                <Loader2 size={12} className="animate-spin" />
              ) : (
                <Play size={12} />
              )}
              Create Grid
            </button>
          </div>
        </div>
      )}

      {/* Active Grids */}
      <div className="max-h-64 overflow-y-auto">
        {activeGrids.length === 0 ? (
          <div className="p-8 text-center">
            <Grid3X3 size={32} className="mx-auto mb-2 opacity-30" style={{ color: COLORS.GRAY }} />
            <p className="text-sm" style={{ color: COLORS.GRAY }}>No active grid strategies</p>
            <p className="text-xs mt-1" style={{ color: COLORS.GRAY }}>
              Create a grid to start automated trading
            </p>
          </div>
        ) : (
          <div className="divide-y" style={{ borderColor: COLORS.BORDER }}>
            {activeGrids.map((grid) => (
              <div key={grid.name} className="p-3">
                <div className="flex items-center justify-between mb-2">
                  <button
                    onClick={() => setExpandedGrid(expandedGrid === grid.name ? null : grid.name)}
                    className="flex items-center gap-2"
                  >
                    {expandedGrid === grid.name ? (
                      <ChevronUp size={12} style={{ color: COLORS.GRAY }} />
                    ) : (
                      <ChevronDown size={12} style={{ color: COLORS.GRAY }} />
                    )}
                    <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>{grid.name}</span>
                    <span
                      className="px-1.5 py-0.5 rounded text-xs"
                      style={{
                        backgroundColor: grid.isActive ? COLORS.GREEN + '20' : COLORS.GRAY + '20',
                        color: grid.isActive ? COLORS.GREEN : COLORS.GRAY,
                      }}
                    >
                      {grid.isActive ? 'Active' : 'Paused'}
                    </span>
                  </button>
                  <div className="flex items-center gap-2">
                    <button
                      onClick={() => fetchGridStatus(grid.name)}
                      className="p-1 rounded hover:bg-[#1A1A1A]"
                    >
                      <RefreshCw size={12} style={{ color: COLORS.GRAY }} />
                    </button>
                  </div>
                </div>

                {/* Quick Stats */}
                <div className="grid grid-cols-3 gap-2 text-xs">
                  <div>
                    <span style={{ color: COLORS.GRAY }}>Trades: </span>
                    <span style={{ color: COLORS.WHITE }}>{grid.tradesExecuted}</span>
                  </div>
                  <div>
                    <span style={{ color: COLORS.GRAY }}>Profit: </span>
                    <span style={{ color: grid.totalProfit >= 0 ? COLORS.GREEN : COLORS.RED }}>
                      ${grid.totalProfit.toFixed(2)}
                    </span>
                  </div>
                  <div>
                    <span style={{ color: COLORS.GRAY }}>Active: </span>
                    <span style={{ color: COLORS.WHITE }}>{grid.activeOrders}</span>
                  </div>
                </div>

                {/* Expanded View */}
                {expandedGrid === grid.name && (
                  <div className="mt-3 pt-3 border-t" style={{ borderColor: COLORS.BORDER }}>
                    <div className="text-xs mb-2" style={{ color: COLORS.GRAY }}>Grid Levels</div>
                    {renderGridVisualization(grid.gridLevels, grid.triggeredLevels)}
                    <div className="text-xs mt-2" style={{ color: COLORS.GRAY }}>
                      <Info size={10} className="inline mr-1" />
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
      <div className="px-4 py-2 border-t text-xs" style={{ borderColor: COLORS.BORDER, color: COLORS.GRAY }}>
        <Info size={10} className="inline mr-1" />
        Grid strategies execute automatically based on price levels
      </div>
    </div>
  );
};

export default GridStrategyPanel;
