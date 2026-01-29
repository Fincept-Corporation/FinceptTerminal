/**
 * Surface Analytics - Order Book Panel Component
 * Display order book data (MBP-1, MBP-10, MBO)
 */

import React, { useState, useCallback } from 'react';
import {
  BookOpen,
  RefreshCw,
  AlertCircle,
  TrendingUp,
  TrendingDown,
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { FINCEPT_COLORS } from '../constants';

interface OrderBookLevel {
  bid_px: number;
  ask_px: number;
  bid_sz: number;
  ask_sz: number;
  bid_ct: number;
  ask_ct: number;
}

interface OrderBookSnapshot {
  symbol: string;
  ts_event: string;
  levels: OrderBookLevel[];
}

interface OrderBookPanelProps {
  apiKey: string | null;
  symbols: string[];
  accentColor: string;
  textColor: string;
}

type SchemaType = 'mbp-1' | 'mbp-10' | 'tbbo';

export const OrderBookPanel: React.FC<OrderBookPanelProps> = ({
  apiKey,
  symbols,
  accentColor,
  textColor,
}) => {
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [selectedSchema, setSelectedSchema] = useState<SchemaType>('mbp-10');
  const [selectedSymbol, setSelectedSymbol] = useState(symbols[0] || 'SPY');
  const [snapshots, setSnapshots] = useState<OrderBookSnapshot[]>([]);

  const fetchOrderBook = useCallback(async () => {
    if (!apiKey || !selectedSymbol) return;

    setLoading(true);
    setError(null);

    try {
      const levels = selectedSchema === 'mbp-1' ? 1 : 10;
      const result = await invoke<string>('databento_get_order_book_snapshot', {
        apiKey,
        symbols: [selectedSymbol],
        dataset: null,
        levels,
        snapshotTime: null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setSnapshots(parsed.snapshots || []);
      }
    } catch (err) {
      setError(`Failed to fetch order book: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, selectedSymbol, selectedSchema]);

  const formatPrice = (price: number): string => {
    if (!price || price === 0) return '--';
    return price.toFixed(2);
  };

  const formatSize = (size: number): string => {
    if (!size || size === 0) return '--';
    if (size >= 1000000) return `${(size / 1000000).toFixed(1)}M`;
    if (size >= 1000) return `${(size / 1000).toFixed(1)}K`;
    return size.toString();
  };

  const getSpread = (levels: OrderBookLevel[]): { spread: number; spreadPct: number } => {
    if (!levels || levels.length === 0) return { spread: 0, spreadPct: 0 };
    const bid = levels[0].bid_px;
    const ask = levels[0].ask_px;
    if (!bid || !ask) return { spread: 0, spreadPct: 0 };
    const spread = ask - bid;
    const mid = (bid + ask) / 2;
    const spreadPct = (spread / mid) * 100;
    return { spread, spreadPct };
  };

  const getMaxSize = (levels: OrderBookLevel[]): number => {
    if (!levels || levels.length === 0) return 1;
    return Math.max(
      ...levels.map(l => Math.max(l.bid_sz || 0, l.ask_sz || 0)),
      1
    );
  };

  if (!apiKey) {
    return (
      <div
        className="p-4 text-center"
        style={{
          backgroundColor: FINCEPT_COLORS.DARK_BG,
          border: `1px solid ${FINCEPT_COLORS.BORDER}`,
          borderRadius: '2px',
        }}
      >
        <BookOpen size={24} style={{ color: FINCEPT_COLORS.MUTED, margin: '0 auto 8px' }} />
        <div className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
          Configure Databento API key to view order book
        </div>
      </div>
    );
  }

  const currentSnapshot = snapshots.find(s => s.symbol === selectedSymbol);
  const { spread, spreadPct } = currentSnapshot ? getSpread(currentSnapshot.levels) : { spread: 0, spreadPct: 0 };
  const maxSize = currentSnapshot ? getMaxSize(currentSnapshot.levels) : 1;

  return (
    <div
      style={{
        backgroundColor: FINCEPT_COLORS.DARK_BG,
        border: `1px solid ${FINCEPT_COLORS.BORDER}`,
        borderRadius: '2px',
        overflow: 'hidden',
      }}
    >
      {/* Header */}
      <div
        className="flex items-center justify-between p-2"
        style={{
          backgroundColor: FINCEPT_COLORS.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}`,
        }}
      >
        <div className="flex items-center gap-2">
          <BookOpen size={14} style={{ color: accentColor }} />
          <span className="text-xs font-bold" style={{ color: accentColor }}>
            ORDER BOOK
          </span>
        </div>
        <button
          onClick={fetchOrderBook}
          disabled={loading}
          style={{ color: loading ? FINCEPT_COLORS.MUTED : accentColor }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
        </button>
      </div>

      {/* Controls */}
      <div className="flex items-center justify-between p-2" style={{ borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}` }}>
        {/* Symbol selector */}
        <div className="flex items-center gap-1">
          {symbols.slice(0, 4).map(sym => (
            <button
              key={sym}
              onClick={() => setSelectedSymbol(sym)}
              className="px-2 py-0.5 text-[9px] font-bold"
              style={{
                backgroundColor: selectedSymbol === sym ? accentColor : FINCEPT_COLORS.BORDER,
                color: selectedSymbol === sym ? FINCEPT_COLORS.BLACK : textColor,
                borderRadius: '2px',
              }}
            >
              {sym}
            </button>
          ))}
        </div>

        {/* Schema selector */}
        <div className="flex items-center gap-1">
          {(['mbp-1', 'mbp-10'] as SchemaType[]).map(schema => (
            <button
              key={schema}
              onClick={() => setSelectedSchema(schema)}
              className="px-2 py-0.5 text-[9px]"
              style={{
                backgroundColor: selectedSchema === schema ? FINCEPT_COLORS.BORDER : 'transparent',
                color: selectedSchema === schema ? textColor : FINCEPT_COLORS.MUTED,
                border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                borderRadius: '2px',
              }}
            >
              {schema === 'mbp-1' ? 'TOP' : '10-LVL'}
            </button>
          ))}
        </div>
      </div>

      {/* Error */}
      {error && (
        <div
          className="flex items-center gap-2 m-2 p-2 text-xs"
          style={{
            backgroundColor: `${FINCEPT_COLORS.RED}15`,
            border: `1px solid ${FINCEPT_COLORS.RED}50`,
            borderRadius: '2px',
            color: FINCEPT_COLORS.RED,
          }}
        >
          <AlertCircle size={12} />
          <span>{error}</span>
        </div>
      )}

      {/* Spread Info */}
      {currentSnapshot && (
        <div
          className="flex items-center justify-between p-2 text-xs"
          style={{ backgroundColor: FINCEPT_COLORS.BLACK, borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}` }}
        >
          <span style={{ color: FINCEPT_COLORS.MUTED }}>Spread:</span>
          <span style={{ color: FINCEPT_COLORS.YELLOW }}>
            ${spread.toFixed(2)} ({spreadPct.toFixed(3)}%)
          </span>
        </div>
      )}

      {/* Order Book Display */}
      <div className="p-2">
        {loading ? (
          <div className="flex items-center justify-center p-4">
            <RefreshCw size={16} className="animate-spin" style={{ color: accentColor }} />
          </div>
        ) : !currentSnapshot || currentSnapshot.levels.length === 0 ? (
          <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
            Click refresh to load order book data
          </div>
        ) : (
          <div>
            {/* Header Row */}
            <div className="grid grid-cols-4 gap-1 mb-1 text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>
              <div className="text-right">BID SIZE</div>
              <div className="text-right">BID</div>
              <div className="text-left">ASK</div>
              <div className="text-left">ASK SIZE</div>
            </div>

            {/* Order Book Levels */}
            {currentSnapshot.levels.map((level, idx) => {
              const bidWidth = (level.bid_sz / maxSize) * 100;
              const askWidth = (level.ask_sz / maxSize) * 100;

              return (
                <div
                  key={idx}
                  className="grid grid-cols-4 gap-1 py-0.5 text-[10px] relative"
                  style={{
                    backgroundColor: idx === 0 ? `${FINCEPT_COLORS.BORDER}50` : 'transparent',
                  }}
                >
                  {/* Bid side visualization */}
                  <div className="relative text-right pr-1" style={{ color: FINCEPT_COLORS.GREEN }}>
                    <div
                      className="absolute right-0 top-0 bottom-0"
                      style={{
                        width: `${bidWidth}%`,
                        backgroundColor: `${FINCEPT_COLORS.GREEN}20`,
                      }}
                    />
                    <span className="relative z-10">{formatSize(level.bid_sz)}</span>
                  </div>
                  <div className="text-right font-mono" style={{ color: FINCEPT_COLORS.GREEN }}>
                    {formatPrice(level.bid_px)}
                  </div>
                  <div className="text-left font-mono" style={{ color: FINCEPT_COLORS.RED }}>
                    {formatPrice(level.ask_px)}
                  </div>
                  {/* Ask side visualization */}
                  <div className="relative text-left pl-1" style={{ color: FINCEPT_COLORS.RED }}>
                    <div
                      className="absolute left-0 top-0 bottom-0"
                      style={{
                        width: `${askWidth}%`,
                        backgroundColor: `${FINCEPT_COLORS.RED}20`,
                      }}
                    />
                    <span className="relative z-10">{formatSize(level.ask_sz)}</span>
                  </div>
                </div>
              );
            })}

            {/* Summary Stats */}
            <div
              className="grid grid-cols-2 gap-2 mt-2 pt-2 text-[9px]"
              style={{ borderTop: `1px solid ${FINCEPT_COLORS.BORDER}` }}
            >
              <div className="flex items-center gap-1">
                <TrendingUp size={10} style={{ color: FINCEPT_COLORS.GREEN }} />
                <span style={{ color: FINCEPT_COLORS.MUTED }}>Total Bid:</span>
                <span style={{ color: FINCEPT_COLORS.GREEN }}>
                  {formatSize(currentSnapshot.levels.reduce((sum, l) => sum + l.bid_sz, 0))}
                </span>
              </div>
              <div className="flex items-center gap-1">
                <TrendingDown size={10} style={{ color: FINCEPT_COLORS.RED }} />
                <span style={{ color: FINCEPT_COLORS.MUTED }}>Total Ask:</span>
                <span style={{ color: FINCEPT_COLORS.RED }}>
                  {formatSize(currentSnapshot.levels.reduce((sum, l) => sum + l.ask_sz, 0))}
                </span>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
