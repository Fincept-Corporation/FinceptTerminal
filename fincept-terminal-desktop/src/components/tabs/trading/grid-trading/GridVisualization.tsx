/**
 * Grid Visualization Component
 *
 * Visual representation of grid levels, orders, and price movement.
 */

import React, { useMemo } from 'react';
import { ArrowUp, ArrowDown, Circle, CheckCircle, XCircle, Clock, AlertCircle } from 'lucide-react';
import type { GridState, GridLevel, GridLevelStatus } from '../../../../services/gridTrading/types';

interface GridVisualizationProps {
  grid: GridState;
  height?: number;
}

export function GridVisualization({ grid, height = 400 }: GridVisualizationProps) {
  const { levels, currentPrice, config } = grid;

  // Calculate visualization dimensions
  const visualization = useMemo(() => {
    const priceRange = config.upperPrice - config.lowerPrice;
    const padding = priceRange * 0.1;
    const minPrice = config.lowerPrice - padding;
    const maxPrice = config.upperPrice + padding;
    const totalRange = maxPrice - minPrice;

    // Map price to Y position
    const priceToY = (price: number): number => {
      return ((maxPrice - price) / totalRange) * height;
    };

    // Current price position
    const currentPriceY = priceToY(currentPrice);

    // Level positions
    const levelPositions = levels.map(level => ({
      ...level,
      y: priceToY(level.price),
    }));

    // Stop loss and take profit positions
    const stopLossY = config.stopLoss ? priceToY(config.stopLoss) : null;
    const takeProfitY = config.takeProfit ? priceToY(config.takeProfit) : null;

    return {
      minPrice,
      maxPrice,
      priceToY,
      currentPriceY,
      levelPositions,
      stopLossY,
      takeProfitY,
    };
  }, [levels, currentPrice, config, height]);

  // Get status icon and color
  const getStatusVisual = (status: GridLevelStatus) => {
    switch (status) {
      case 'pending':
        return { icon: Clock, color: '#787878' };
      case 'open':
        return { icon: Circle, color: '#00E5FF' };
      case 'filled':
        return { icon: CheckCircle, color: '#00D66F' };
      case 'cancelled':
        return { icon: XCircle, color: '#FF3B3B' };
      case 'error':
        return { icon: AlertCircle, color: '#FF3B3B' };
      default:
        return { icon: Circle, color: '#787878' };
    }
  };

  return (
    <div className="bg-[#0F0F0F] border border-[#2A2A2A] rounded-lg p-4">
      {/* Header */}
      <div className="flex items-center justify-between mb-4">
        <span className="text-sm text-white font-semibold">Grid Visualization</span>
        <div className="flex items-center gap-4 text-xs">
          <div className="flex items-center gap-1">
            <div className="w-3 h-3 rounded-full bg-[#00D66F]" />
            <span className="text-[#787878]">Buy</span>
          </div>
          <div className="flex items-center gap-1">
            <div className="w-3 h-3 rounded-full bg-[#FF3B3B]" />
            <span className="text-[#787878]">Sell</span>
          </div>
        </div>
      </div>

      {/* Visualization */}
      <div className="relative" style={{ height }}>
        {/* Grid background */}
        <div className="absolute inset-0 flex flex-col justify-between">
          {[...Array(10)].map((_, i) => (
            <div key={i} className="border-t border-[#1A1A1A]" />
          ))}
        </div>

        {/* Price scale */}
        <div className="absolute left-0 top-0 bottom-0 w-16 flex flex-col justify-between text-xs text-[#787878] pr-2">
          <span>{visualization.maxPrice.toFixed(2)}</span>
          <span>{((visualization.maxPrice + visualization.minPrice) / 2).toFixed(2)}</span>
          <span>{visualization.minPrice.toFixed(2)}</span>
        </div>

        {/* Grid content area */}
        <div className="absolute left-16 right-0 top-0 bottom-0">
          {/* Upper bound line */}
          <div
            className="absolute left-0 right-0 border-t-2 border-dashed border-[#FF8800]/50"
            style={{ top: visualization.priceToY(config.upperPrice) }}
          >
            <span className="absolute right-0 -top-4 text-xs text-[#FF8800]">
              Upper: {config.upperPrice.toFixed(2)}
            </span>
          </div>

          {/* Lower bound line */}
          <div
            className="absolute left-0 right-0 border-t-2 border-dashed border-[#FF8800]/50"
            style={{ top: visualization.priceToY(config.lowerPrice) }}
          >
            <span className="absolute right-0 top-1 text-xs text-[#FF8800]">
              Lower: {config.lowerPrice.toFixed(2)}
            </span>
          </div>

          {/* Stop Loss line */}
          {visualization.stopLossY !== null && (
            <div
              className="absolute left-0 right-0 border-t-2 border-dashed border-[#FF3B3B]"
              style={{ top: visualization.stopLossY }}
            >
              <span className="absolute right-0 top-1 text-xs text-[#FF3B3B]">
                SL: {config.stopLoss?.toFixed(2)}
              </span>
            </div>
          )}

          {/* Take Profit line */}
          {visualization.takeProfitY !== null && (
            <div
              className="absolute left-0 right-0 border-t-2 border-dashed border-[#00D66F]"
              style={{ top: visualization.takeProfitY }}
            >
              <span className="absolute right-0 -top-4 text-xs text-[#00D66F]">
                TP: {config.takeProfit?.toFixed(2)}
              </span>
            </div>
          )}

          {/* Grid levels */}
          {visualization.levelPositions.map((level, index) => {
            const { icon: Icon, color } = getStatusVisual(level.status);
            const isBuy = level.side === 'buy';

            return (
              <div
                key={index}
                className="absolute left-0 right-0 flex items-center group"
                style={{ top: level.y - 10 }}
              >
                {/* Level line */}
                <div
                  className={`absolute left-0 right-0 h-0.5 ${
                    isBuy ? 'bg-[#00D66F]/30' : 'bg-[#FF3B3B]/30'
                  }`}
                />

                {/* Level indicator */}
                <div
                  className={`absolute left-4 flex items-center gap-2 px-2 py-1 rounded ${
                    isBuy ? 'bg-[#00D66F]/20' : 'bg-[#FF3B3B]/20'
                  }`}
                >
                  {isBuy ? (
                    <ArrowUp className="w-3 h-3 text-[#00D66F]" />
                  ) : (
                    <ArrowDown className="w-3 h-3 text-[#FF3B3B]" />
                  )}
                  <span className={`text-xs ${isBuy ? 'text-[#00D66F]' : 'text-[#FF3B3B]'}`}>
                    {level.price.toFixed(2)}
                  </span>
                  <Icon className="w-3 h-3" style={{ color }} />
                </div>

                {/* Quantity badge */}
                <div className="absolute right-4 text-xs text-[#787878]">
                  {level.quantity.toFixed(4)}
                </div>

                {/* Tooltip on hover */}
                <div className="absolute left-24 hidden group-hover:block bg-[#1A1A1A] border border-[#2A2A2A] rounded px-2 py-1 z-10 whitespace-nowrap">
                  <div className="text-xs">
                    <div className="text-white">
                      Level {level.levelIndex + 1}: {level.side.toUpperCase()} @ {level.price.toFixed(2)}
                    </div>
                    <div className="text-[#787878]">
                      Status: {level.status} | Qty: {level.quantity}
                    </div>
                    {level.filledPrice && (
                      <div className="text-[#00D66F]">
                        Filled @ {level.filledPrice.toFixed(2)}
                      </div>
                    )}
                  </div>
                </div>
              </div>
            );
          })}

          {/* Current price line */}
          <div
            className="absolute left-0 right-0 flex items-center"
            style={{ top: visualization.currentPriceY }}
          >
            <div className="absolute left-0 right-0 h-0.5 bg-[#FFFFFF]" />
            <div className="absolute right-0 bg-white text-black text-xs px-2 py-0.5 rounded font-semibold">
              {currentPrice.toFixed(2)}
            </div>
          </div>
        </div>
      </div>

      {/* Legend */}
      <div className="mt-4 flex items-center justify-center gap-6 text-xs text-[#787878]">
        <div className="flex items-center gap-1">
          <Clock className="w-3 h-3" />
          <span>Pending</span>
        </div>
        <div className="flex items-center gap-1">
          <Circle className="w-3 h-3 text-[#00E5FF]" />
          <span>Open</span>
        </div>
        <div className="flex items-center gap-1">
          <CheckCircle className="w-3 h-3 text-[#00D66F]" />
          <span>Filled</span>
        </div>
        <div className="flex items-center gap-1">
          <XCircle className="w-3 h-3 text-[#FF3B3B]" />
          <span>Cancelled</span>
        </div>
      </div>
    </div>
  );
}

export default GridVisualization;
