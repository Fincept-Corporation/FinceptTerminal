// MarketDepthChart.tsx - Market depth table with visual depth bars
import React, { useState, useEffect } from 'react';
import { useWebSocket } from '../../../hooks/useWebSocket';
import { isProviderAvailable } from '../../../services/websocket';

interface MarketDepthChartProps {
  symbol: string;
  provider: string;
}

interface DepthLevel {
  price: number;
  size: number;
  cumulative: number;
}

export function MarketDepthChart({ symbol, provider }: MarketDepthChartProps) {
  const [bids, setBids] = useState<DepthLevel[]>([]);
  const [asks, setAsks] = useState<DepthLevel[]>([]);
  const [maxCumulative, setMaxCumulative] = useState(0);

  // Only subscribe if WebSocket adapter exists for this provider
  const hasWebSocket = isProviderAvailable(provider);

  const { message, isLoading } = useWebSocket(
    hasWebSocket ? `${provider}.book.${symbol}` : null,
    null,
    { autoSubscribe: hasWebSocket, params: { depth: 50 } }
  );

  useEffect(() => {
    if (!message || message.type !== 'orderbook') return;

    const data = message.data;
    if (!data || !Array.isArray(data.bids) || !Array.isArray(data.asks)) return;

    try {
      // Process bids (buy orders) - highest to lowest
      const rawBids = data.bids
        .filter((level: any) => level && typeof level.price === 'number' && typeof level.size === 'number')
        .sort((a: any, b: any) => b.price - a.price)
        .slice(0, 25);

      let bidCumulative = 0;
      const processedBids: DepthLevel[] = rawBids.map((level: any) => {
        bidCumulative += level.size;
        return {
          price: level.price,
          size: level.size,
          cumulative: bidCumulative,
        };
      });

      // Process asks (sell orders) - lowest to highest
      const rawAsks = data.asks
        .filter((level: any) => level && typeof level.price === 'number' && typeof level.size === 'number')
        .sort((a: any, b: any) => a.price - b.price)
        .slice(0, 25);

      let askCumulative = 0;
      const processedAsks: DepthLevel[] = rawAsks.map((level: any) => {
        askCumulative += level.size;
        return {
          price: level.price,
          size: level.size,
          cumulative: askCumulative,
        };
      });

      setBids(processedBids);
      setAsks(processedAsks);

      // Calculate max cumulative for bar sizing
      const maxBid = processedBids.length > 0 ? processedBids[processedBids.length - 1].cumulative : 0;
      const maxAsk = processedAsks.length > 0 ? processedAsks[processedAsks.length - 1].cumulative : 0;
      setMaxCumulative(Math.max(maxBid, maxAsk));
    } catch (error) {
      console.error('[MarketDepthChart] Failed to process data:', error);
    }
  }, [message]);

  if (isLoading) {
    return (
      <div className="bg-[#0d0d0d] border-t border-gray-800 p-3">
        <h3 className="text-[10px] font-bold text-gray-400 tracking-wider mb-2">MARKET DEPTH</h3>
        <div className="flex items-center justify-center h-40 text-gray-500">
          <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-orange-500 mr-2"></div>
          <span className="text-[10px]">Loading market depth...</span>
        </div>
      </div>
    );
  }

  return (
    <div className="bg-[#0d0d0d] border-t border-gray-800 p-3">
      <h3 className="text-[10px] font-bold text-gray-400 tracking-wider mb-2">MARKET DEPTH</h3>

      <div className="grid grid-cols-2 gap-2">
        {/* BIDS (Buy Orders) */}
        <div className="bg-[#1a1a1a] rounded">
          <div className="px-2 py-1 border-b border-gray-800">
            <h4 className="text-[9px] font-bold text-green-400 uppercase tracking-wide">Bids (Buy)</h4>
          </div>
          <div className="px-2 py-0.5 bg-[#0d0d0d] text-gray-500 text-[8px] font-bold uppercase tracking-wide flex justify-between">
            <span className="w-1/3">Price</span>
            <span className="w-1/3 text-right">Size</span>
            <span className="w-1/3 text-right">Total</span>
          </div>
          <div className="max-h-48 overflow-y-auto">
            {bids.length === 0 ? (
              <div className="p-2 text-center text-gray-600 text-[10px]">No bid data</div>
            ) : (
              bids.map((level, idx) => (
                <DepthRow
                  key={`bid-${idx}`}
                  level={level}
                  maxCumulative={maxCumulative}
                  side="bid"
                />
              ))
            )}
          </div>
        </div>

        {/* ASKS (Sell Orders) */}
        <div className="bg-[#1a1a1a] rounded">
          <div className="px-2 py-1 border-b border-gray-800">
            <h4 className="text-[9px] font-bold text-red-400 uppercase tracking-wide">Asks (Sell)</h4>
          </div>
          <div className="px-2 py-0.5 bg-[#0d0d0d] text-gray-500 text-[8px] font-bold uppercase tracking-wide flex justify-between">
            <span className="w-1/3">Price</span>
            <span className="w-1/3 text-right">Size</span>
            <span className="w-1/3 text-right">Total</span>
          </div>
          <div className="max-h-48 overflow-y-auto">
            {asks.length === 0 ? (
              <div className="p-2 text-center text-gray-600 text-[10px]">No ask data</div>
            ) : (
              asks.map((level, idx) => (
                <DepthRow
                  key={`ask-${idx}`}
                  level={level}
                  maxCumulative={maxCumulative}
                  side="ask"
                />
              ))
            )}
          </div>
        </div>
      </div>

      {/* Summary Stats */}
      {bids.length > 0 && asks.length > 0 && (
        <div className="mt-2 grid grid-cols-3 gap-2 pt-2 border-t border-gray-800">
          <div className="text-center">
            <div className="text-[8px] text-gray-500 uppercase">Best Bid</div>
            <div className="text-[11px] font-bold text-green-400 tabular-nums">
              ${bids[0].price.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </div>
          </div>
          <div className="text-center">
            <div className="text-[8px] text-gray-500 uppercase">Spread</div>
            <div className="text-[11px] font-bold text-orange-400 tabular-nums">
              ${(asks[0].price - bids[0].price).toFixed(2)}
            </div>
          </div>
          <div className="text-center">
            <div className="text-[8px] text-gray-500 uppercase">Best Ask</div>
            <div className="text-[11px] font-bold text-red-400 tabular-nums">
              ${asks[0].price.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

interface DepthRowProps {
  level: DepthLevel;
  maxCumulative: number;
  side: 'bid' | 'ask';
}

function DepthRow({ level, maxCumulative, side }: DepthRowProps) {
  const percentage = maxCumulative > 0 ? (level.cumulative / maxCumulative) * 100 : 0;
  const bgColor = side === 'bid' ? 'bg-green-900/30' : 'bg-red-900/30';

  return (
    <div className="relative px-2 py-[1px] hover:bg-gray-800/70 cursor-pointer transition-colors">
      {/* Background depth bar */}
      <div
        className={`absolute right-0 top-0 bottom-0 ${bgColor} transition-all`}
        style={{ width: `${percentage}%` }}
      />

      {/* Content */}
      <div className="relative flex justify-between items-center text-[10px]">
        <div className={`w-1/3 font-bold tabular-nums ${side === 'bid' ? 'text-green-400' : 'text-red-400'}`}>
          {level.price.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
        </div>
        <div className="w-1/3 text-right text-gray-200 tabular-nums">
          {level.size.toLocaleString('en-US', { minimumFractionDigits: 4, maximumFractionDigits: 4 })}
        </div>
        <div className="w-1/3 text-right text-gray-500 tabular-nums">
          {level.cumulative.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
        </div>
      </div>
    </div>
  );
}
