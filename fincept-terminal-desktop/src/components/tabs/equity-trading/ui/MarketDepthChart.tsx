/**
 * Market Depth Chart
 * Visual orderbook with bid/ask depth visualization
 */

import React, { useState, useEffect } from 'react';
import { BrokerType, UnifiedMarketDepth } from '../core/types';
import { webSocketManager } from '../services/WebSocketManager';

interface MarketDepthChartProps {
  brokerId: BrokerType;
  symbol: string;
  exchange: string;
}

const MarketDepthChart: React.FC<MarketDepthChartProps> = ({ brokerId, symbol, exchange }) => {
  const [depth, setDepth] = useState<UnifiedMarketDepth | null>(null);
  const [viewMode, setViewMode] = useState<'table' | 'chart'>('table');

  useEffect(() => {
    if (!symbol || !brokerId) return;

    // Subscribe to market depth updates
    const subscriptionId = webSocketManager.subscribeMarketDepth(
      brokerId,
      symbol,
      exchange,
      (message) => {
        if (message.data) {
          setDepth(message.data as UnifiedMarketDepth);
        }
      }
    );

    return () => {
      // Cleanup subscription
    };
  }, [brokerId, symbol, exchange]);

  if (!depth) {
    return (
      <div className="h-full flex items-center justify-center text-muted-foreground">
        <div className="text-center">
          <div className="text-2xl mb-2">📊</div>
          <div className="text-sm">Waiting for market depth data...</div>
        </div>
      </div>
    );
  }

  const maxBidQty = Math.max(...depth.bids.map(b => b.quantity), 1);
  const maxAskQty = Math.max(...depth.asks.map(a => a.quantity), 1);
  const maxQty = Math.max(maxBidQty, maxAskQty);

  return (
    <div className="h-full flex flex-col">
      {/* Header */}
      <div className="flex items-center justify-between px-4 py-2 border-b border-border">
        <div>
          <h3 className="font-semibold">{symbol}</h3>
          <div className="text-xs text-muted-foreground uppercase">{brokerId} - {exchange}</div>
        </div>

        {/* View Mode Toggle */}
        <div className="flex items-center gap-1">
          <button
            onClick={() => setViewMode('table')}
            className={`px-3 py-1 text-xs rounded ${
              viewMode === 'table' ? 'bg-primary text-primary-foreground' : 'bg-muted'
            }`}
          >
            Table
          </button>
          <button
            onClick={() => setViewMode('chart')}
            className={`px-3 py-1 text-xs rounded ${
              viewMode === 'chart' ? 'bg-primary text-primary-foreground' : 'bg-muted'
            }`}
          >
            Chart
          </button>
        </div>
      </div>

      {/* Content */}
      <div className="flex-1 overflow-auto p-4">
        {viewMode === 'table' ? (
          <TableView depth={depth} />
        ) : (
          <ChartView depth={depth} maxQty={maxQty} />
        )}
      </div>

      {/* Footer Stats */}
      <div className="px-4 py-2 border-t border-border">
        <div className="flex items-center justify-between text-xs">
          <div>
            <span className="text-muted-foreground">Spread: </span>
            <span className="font-medium">
              ₹{((depth.asks[0]?.price || 0) - (depth.bids[0]?.price || 0)).toFixed(2)}
            </span>
          </div>
          <div>
            <span className="text-muted-foreground">Total Bid: </span>
            <span className="font-medium text-green-500">
              {depth.bids.reduce((sum, b) => sum + b.quantity, 0).toLocaleString()}
            </span>
          </div>
          <div>
            <span className="text-muted-foreground">Total Ask: </span>
            <span className="font-medium text-red-500">
              {depth.asks.reduce((sum, a) => sum + a.quantity, 0).toLocaleString()}
            </span>
          </div>
        </div>
      </div>
    </div>
  );
};

const TableView: React.FC<{ depth: UnifiedMarketDepth }> = ({ depth }) => {
  const maxRows = Math.max(depth.bids.length, depth.asks.length);
  const rows = [];

  for (let i = 0; i < maxRows; i++) {
    rows.push({
      bid: depth.bids[i],
      ask: depth.asks[i],
    });
  }

  return (
    <table className="w-full text-sm">
      <thead>
        <tr className="border-b border-border">
          <th className="text-left py-2 text-green-500">Bid Qty</th>
          <th className="text-right py-2 text-green-500">Bid Price</th>
          <th className="text-left py-2 text-red-500">Ask Price</th>
          <th className="text-right py-2 text-red-500">Ask Qty</th>
        </tr>
      </thead>
      <tbody>
        {rows.map((row, idx) => (
          <tr key={idx} className="border-b border-border/30 hover:bg-muted/20">
            <td className="py-1.5 text-green-500">
              {row.bid ? row.bid.quantity.toLocaleString() : '-'}
            </td>
            <td className="text-right py-1.5 text-green-500 font-mono">
              {row.bid ? `₹${row.bid.price.toFixed(2)}` : '-'}
            </td>
            <td className="text-left py-1.5 text-red-500 font-mono">
              {row.ask ? `₹${row.ask.price.toFixed(2)}` : '-'}
            </td>
            <td className="text-right py-1.5 text-red-500">
              {row.ask ? row.ask.quantity.toLocaleString() : '-'}
            </td>
          </tr>
        ))}
      </tbody>
    </table>
  );
};

const ChartView: React.FC<{ depth: UnifiedMarketDepth; maxQty: number }> = ({ depth, maxQty }) => {
  return (
    <div className="space-y-1">
      {/* Asks (reversed for visual appeal - highest at top) */}
      <div className="space-y-1">
        {depth.asks.slice(0, 10).reverse().map((ask, idx) => {
          const widthPercent = (ask.quantity / maxQty) * 100;
          return (
            <div key={`ask-${idx}`} className="relative h-6">
              <div
                className="absolute right-0 h-full bg-red-500/20 border-r-2 border-red-500"
                style={{ width: `${widthPercent}%` }}
              />
              <div className="absolute inset-0 flex items-center justify-between px-2 text-xs">
                <span className="font-mono text-red-500">₹{ask.price.toFixed(2)}</span>
                <span className="text-red-500">{ask.quantity.toLocaleString()}</span>
              </div>
            </div>
          );
        })}
      </div>

      {/* Spread Separator */}
      <div className="my-2 border-t-2 border-dashed border-muted-foreground/50" />

      {/* Bids */}
      <div className="space-y-1">
        {depth.bids.slice(0, 10).map((bid, idx) => {
          const widthPercent = (bid.quantity / maxQty) * 100;
          return (
            <div key={`bid-${idx}`} className="relative h-6">
              <div
                className="absolute left-0 h-full bg-green-500/20 border-l-2 border-green-500"
                style={{ width: `${widthPercent}%` }}
              />
              <div className="absolute inset-0 flex items-center justify-between px-2 text-xs">
                <span className="font-mono text-green-500">₹{bid.price.toFixed(2)}</span>
                <span className="text-green-500">{bid.quantity.toLocaleString()}</span>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
};

export default MarketDepthChart;
