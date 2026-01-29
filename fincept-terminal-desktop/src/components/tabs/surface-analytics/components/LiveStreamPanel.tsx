/**
 * Surface Analytics - Live Stream Panel Component
 * Real-time market data streaming from Databento
 */

import React, { useState, useCallback, useRef, useEffect } from 'react';
import {
  Radio,
  Play,
  Square,
  AlertCircle,
  Wifi,
  WifiOff,
  Activity,
  TrendingUp,
  TrendingDown,
  Clock,
  Trash2,
} from 'lucide-react';
import { FINCEPT_COLORS } from '../constants';

interface LiveStreamRecord {
  type: 'data' | 'status' | 'error' | 'info';
  timestamp: number;
  record?: {
    record_type: string;
    ts_event: string;
    symbol: string;
    schema: string;
    price?: number;
    size?: number;
    side?: string;
    levels?: Array<{
      bid_px: number;
      ask_px: number;
      bid_sz: number;
      ask_sz: number;
    }>;
    [key: string]: unknown;
  };
  message?: string;
  connected?: boolean;
}

interface LiveStreamPanelProps {
  apiKey: string | null;
  symbols: string[];
  accentColor: string;
  textColor: string;
  onStartStream: (
    dataset: string,
    schema: string,
    symbols: string[],
    callback: (record: LiveStreamRecord) => void,
    options?: { stypeIn?: string; snapshot?: boolean }
  ) => Promise<{ streamId: string } | { error: true; message: string }>;
  onStopStream: (streamId: string) => Promise<unknown>;
  onStopAllStreams: () => Promise<unknown>;
  activeStreams: string[];
}

const LIVE_DATASETS = [
  { id: 'XNAS.ITCH', name: 'NASDAQ', description: 'NASDAQ TotalView ITCH' },
  { id: 'XNYS.PILLAR', name: 'NYSE', description: 'NYSE Pillar' },
  { id: 'GLBX.MDP3', name: 'CME', description: 'CME Globex MDP 3.0' },
  { id: 'OPRA.PILLAR', name: 'Options', description: 'US Options OPRA' },
  { id: 'DBEQ.BASIC', name: 'Sample', description: 'Free Sample Data' },
];

const LIVE_SCHEMAS = [
  { id: 'trades', name: 'Trades', description: 'Trade executions' },
  { id: 'mbp-1', name: 'BBO', description: 'Best bid/offer' },
  { id: 'mbp-10', name: 'Depth', description: '10-level depth' },
  { id: 'tbbo', name: 'TBBO', description: 'Top BBO consolidated' },
  { id: 'ohlcv-1s', name: '1s Bars', description: '1-second OHLCV' },
  { id: 'ohlcv-1m', name: '1m Bars', description: '1-minute OHLCV' },
];

const MAX_RECORDS = 100;

export const LiveStreamPanel: React.FC<LiveStreamPanelProps> = ({
  apiKey,
  symbols,
  accentColor,
  textColor,
  onStartStream,
  onStopStream,
  onStopAllStreams,
  activeStreams,
}) => {
  const [selectedDataset, setSelectedDataset] = useState('XNAS.ITCH');
  const [selectedSchema, setSelectedSchema] = useState('trades');
  const [selectedSymbol, setSelectedSymbol] = useState(symbols[0] || 'SPY');
  const [customSymbol, setCustomSymbol] = useState('');
  const [isConnected, setIsConnected] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [records, setRecords] = useState<LiveStreamRecord[]>([]);
  const [recordCount, setRecordCount] = useState(0);
  const [currentStreamId, setCurrentStreamId] = useState<string | null>(null);

  const recordsContainerRef = useRef<HTMLDivElement>(null);

  // Auto-scroll to bottom when new records arrive
  useEffect(() => {
    if (recordsContainerRef.current) {
      recordsContainerRef.current.scrollTop = recordsContainerRef.current.scrollHeight;
    }
  }, [records]);

  const handleStreamData = useCallback((record: LiveStreamRecord) => {
    if (record.type === 'status') {
      setIsConnected(record.connected ?? false);
      if (!record.connected) {
        setIsConnecting(false);
      }
    } else if (record.type === 'error') {
      setError(record.message || 'Unknown error');
      setIsConnecting(false);
    } else if (record.type === 'data') {
      setRecordCount((prev) => prev + 1);
      setRecords((prev) => {
        const newRecords = [...prev, record];
        // Keep only the last MAX_RECORDS
        if (newRecords.length > MAX_RECORDS) {
          return newRecords.slice(-MAX_RECORDS);
        }
        return newRecords;
      });
    }
  }, []);

  const startStream = useCallback(async () => {
    if (!apiKey) return;

    setIsConnecting(true);
    setError(null);
    setRecords([]);
    setRecordCount(0);

    const symbolToUse = customSymbol.trim() || selectedSymbol;
    const result = await onStartStream(
      selectedDataset,
      selectedSchema,
      [symbolToUse],
      handleStreamData,
      { snapshot: false }
    );

    if ('error' in result && result.error) {
      setError(result.message);
      setIsConnecting(false);
    } else if ('streamId' in result) {
      setCurrentStreamId(result.streamId);
    }
  }, [apiKey, selectedDataset, selectedSchema, selectedSymbol, customSymbol, onStartStream, handleStreamData]);

  const stopStream = useCallback(async () => {
    if (currentStreamId) {
      await onStopStream(currentStreamId);
      setCurrentStreamId(null);
      setIsConnected(false);
    }
  }, [currentStreamId, onStopStream]);

  const clearRecords = useCallback(() => {
    setRecords([]);
    setRecordCount(0);
  }, []);

  const formatPrice = (price: number | undefined): string => {
    if (price === undefined || price === 0) return '--';
    return price.toFixed(4);
  };

  const formatSize = (size: number | undefined): string => {
    if (size === undefined || size === 0) return '--';
    if (size >= 1000000) return `${(size / 1000000).toFixed(1)}M`;
    if (size >= 1000) return `${(size / 1000).toFixed(1)}K`;
    return size.toString();
  };

  const formatTime = (ts: string | number): string => {
    if (!ts) return '--:--:--';
    const date = new Date(typeof ts === 'string' ? parseInt(ts) / 1000000 : ts);
    const timeStr = date.toLocaleTimeString('en-US', { hour12: false });
    const ms = date.getMilliseconds().toString().padStart(3, '0');
    return `${timeStr}.${ms}`;
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
        <Radio size={24} style={{ color: FINCEPT_COLORS.MUTED, margin: '0 auto 8px' }} />
        <div className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
          Configure Databento API key for live streaming
        </div>
      </div>
    );
  }

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
          {isConnected ? (
            <Wifi size={14} style={{ color: FINCEPT_COLORS.GREEN }} />
          ) : (
            <WifiOff size={14} style={{ color: FINCEPT_COLORS.MUTED }} />
          )}
          <span className="text-xs font-bold" style={{ color: accentColor }}>
            LIVE STREAM
          </span>
          {isConnected && (
            <span className="text-[9px] px-1" style={{ color: FINCEPT_COLORS.GREEN, backgroundColor: `${FINCEPT_COLORS.GREEN}20`, borderRadius: '2px' }}>
              CONNECTED
            </span>
          )}
        </div>
        <div className="flex items-center gap-2">
          <Activity size={12} style={{ color: isConnected ? FINCEPT_COLORS.GREEN : FINCEPT_COLORS.MUTED }} />
          <span className="text-[10px]" style={{ color: FINCEPT_COLORS.MUTED }}>
            {recordCount} msgs
          </span>
        </div>
      </div>

      {/* Controls */}
      <div className="p-2 space-y-2" style={{ borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}` }}>
        {/* Dataset Selection */}
        <div className="flex items-center gap-1 flex-wrap">
          <span className="text-[9px]" style={{ color: FINCEPT_COLORS.MUTED, width: '45px' }}>Dataset:</span>
          {LIVE_DATASETS.slice(0, 4).map((ds) => (
            <button
              key={ds.id}
              onClick={() => setSelectedDataset(ds.id)}
              disabled={isConnected}
              className="px-2 py-0.5 text-[9px]"
              style={{
                backgroundColor: selectedDataset === ds.id ? accentColor : FINCEPT_COLORS.BORDER,
                color: selectedDataset === ds.id ? FINCEPT_COLORS.BLACK : textColor,
                borderRadius: '2px',
                opacity: isConnected ? 0.5 : 1,
              }}
              title={ds.description}
            >
              {ds.name}
            </button>
          ))}
        </div>

        {/* Schema Selection */}
        <div className="flex items-center gap-1 flex-wrap">
          <span className="text-[9px]" style={{ color: FINCEPT_COLORS.MUTED, width: '45px' }}>Schema:</span>
          {LIVE_SCHEMAS.map((schema) => (
            <button
              key={schema.id}
              onClick={() => setSelectedSchema(schema.id)}
              disabled={isConnected}
              className="px-2 py-0.5 text-[9px]"
              style={{
                backgroundColor: selectedSchema === schema.id ? FINCEPT_COLORS.BORDER : 'transparent',
                color: selectedSchema === schema.id ? textColor : FINCEPT_COLORS.MUTED,
                border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                borderRadius: '2px',
                opacity: isConnected ? 0.5 : 1,
              }}
              title={schema.description}
            >
              {schema.name}
            </button>
          ))}
        </div>

        {/* Symbol Input */}
        <div className="flex items-center gap-2">
          <span className="text-[9px]" style={{ color: FINCEPT_COLORS.MUTED, width: '45px' }}>Symbol:</span>
          <div className="flex items-center gap-1">
            {symbols.slice(0, 3).map((sym) => (
              <button
                key={sym}
                onClick={() => {
                  setSelectedSymbol(sym);
                  setCustomSymbol('');
                }}
                disabled={isConnected}
                className="px-2 py-0.5 text-[9px] font-bold"
                style={{
                  backgroundColor: selectedSymbol === sym && !customSymbol ? accentColor : FINCEPT_COLORS.BORDER,
                  color: selectedSymbol === sym && !customSymbol ? FINCEPT_COLORS.BLACK : textColor,
                  borderRadius: '2px',
                  opacity: isConnected ? 0.5 : 1,
                }}
              >
                {sym}
              </button>
            ))}
          </div>
          <input
            type="text"
            value={customSymbol}
            onChange={(e) => setCustomSymbol(e.target.value.toUpperCase())}
            placeholder="Custom..."
            disabled={isConnected}
            className="px-2 py-0.5 text-[10px] w-20"
            style={{
              backgroundColor: FINCEPT_COLORS.BLACK,
              border: `1px solid ${FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: textColor,
              opacity: isConnected ? 0.5 : 1,
            }}
          />
        </div>

        {/* Start/Stop Buttons */}
        <div className="flex items-center gap-2">
          {!isConnected ? (
            <button
              onClick={startStream}
              disabled={isConnecting}
              className="flex items-center gap-1 px-3 py-1 text-xs font-bold"
              style={{
                backgroundColor: FINCEPT_COLORS.GREEN,
                color: FINCEPT_COLORS.BLACK,
                borderRadius: '2px',
                opacity: isConnecting ? 0.5 : 1,
              }}
            >
              <Play size={12} />
              {isConnecting ? 'Connecting...' : 'Start Stream'}
            </button>
          ) : (
            <button
              onClick={stopStream}
              className="flex items-center gap-1 px-3 py-1 text-xs font-bold"
              style={{
                backgroundColor: FINCEPT_COLORS.RED,
                color: FINCEPT_COLORS.WHITE,
                borderRadius: '2px',
              }}
            >
              <Square size={12} />
              Stop Stream
            </button>
          )}

          <button
            onClick={clearRecords}
            className="flex items-center gap-1 px-2 py-1 text-xs"
            style={{
              backgroundColor: FINCEPT_COLORS.BORDER,
              color: textColor,
              borderRadius: '2px',
            }}
          >
            <Trash2 size={10} />
            Clear
          </button>

          {activeStreams.length > 1 && (
            <button
              onClick={onStopAllStreams}
              className="px-2 py-1 text-[9px]"
              style={{
                backgroundColor: `${FINCEPT_COLORS.RED}30`,
                color: FINCEPT_COLORS.RED,
                borderRadius: '2px',
              }}
            >
              Stop All ({activeStreams.length})
            </button>
          )}
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

      {/* Live Data Feed */}
      <div
        ref={recordsContainerRef}
        className="p-2 overflow-y-auto"
        style={{
          maxHeight: '300px',
          minHeight: '150px',
        }}
      >
        {records.length === 0 ? (
          <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
            {isConnected ? 'Waiting for data...' : 'Click Start Stream to begin receiving live data'}
          </div>
        ) : (
          <div className="space-y-1">
            {records.map((record, idx) => (
              <RecordRow key={idx} record={record} formatPrice={formatPrice} formatSize={formatSize} formatTime={formatTime} />
            ))}
          </div>
        )}
      </div>

      {/* Footer Stats */}
      {isConnected && (
        <div
          className="flex items-center justify-between p-2 text-[9px]"
          style={{
            backgroundColor: FINCEPT_COLORS.BLACK,
            borderTop: `1px solid ${FINCEPT_COLORS.BORDER}`,
            color: FINCEPT_COLORS.MUTED,
          }}
        >
          <div className="flex items-center gap-2">
            <Clock size={10} />
            <span>Live</span>
          </div>
          <div>
            {recordCount} messages | Buffer: {records.length}/{MAX_RECORDS}
          </div>
        </div>
      )}
    </div>
  );
};

// Record Row Component
const RecordRow: React.FC<{
  record: LiveStreamRecord;
  formatPrice: (p: number | undefined) => string;
  formatSize: (s: number | undefined) => string;
  formatTime: (t: string | number) => string;
}> = ({ record, formatPrice, formatSize, formatTime }) => {
  if (record.type !== 'data' || !record.record) return null;

  const { record: data } = record;
  const isBuy = data.side === 'B' || data.side === 'BUY';
  const isSell = data.side === 'A' || data.side === 'S' || data.side === 'SELL';

  // Trade record
  if (data.schema === 'trades') {
    return (
      <div
        className="flex items-center justify-between py-0.5 px-1 text-[10px]"
        style={{
          backgroundColor: isBuy ? `${FINCEPT_COLORS.GREEN}10` : isSell ? `${FINCEPT_COLORS.RED}10` : 'transparent',
          borderRadius: '2px',
        }}
      >
        <div className="flex items-center gap-2">
          {isBuy ? (
            <TrendingUp size={10} style={{ color: FINCEPT_COLORS.GREEN }} />
          ) : isSell ? (
            <TrendingDown size={10} style={{ color: FINCEPT_COLORS.RED }} />
          ) : (
            <Activity size={10} style={{ color: FINCEPT_COLORS.MUTED }} />
          )}
          <span className="font-bold" style={{ color: FINCEPT_COLORS.CYAN }}>{data.symbol}</span>
        </div>
        <div className="flex items-center gap-3">
          <span style={{ color: isBuy ? FINCEPT_COLORS.GREEN : isSell ? FINCEPT_COLORS.RED : FINCEPT_COLORS.WHITE }}>
            {formatPrice(data.price)}
          </span>
          <span style={{ color: FINCEPT_COLORS.MUTED }}>{formatSize(data.size)}</span>
          <span style={{ color: FINCEPT_COLORS.MUTED }}>{formatTime(data.ts_event)}</span>
        </div>
      </div>
    );
  }

  // Quote/BBO record
  if (data.schema === 'mbp' || data.schema === 'tbbo') {
    const level = data.levels?.[0];
    if (!level) return null;

    return (
      <div
        className="flex items-center justify-between py-0.5 px-1 text-[10px]"
        style={{ backgroundColor: `${FINCEPT_COLORS.BORDER}50`, borderRadius: '2px' }}
      >
        <span className="font-bold" style={{ color: FINCEPT_COLORS.CYAN }}>{data.symbol}</span>
        <div className="flex items-center gap-2">
          <span style={{ color: FINCEPT_COLORS.GREEN }}>{formatPrice(level.bid_px)}</span>
          <span style={{ color: FINCEPT_COLORS.MUTED }}>×</span>
          <span style={{ color: FINCEPT_COLORS.RED }}>{formatPrice(level.ask_px)}</span>
          <span style={{ color: FINCEPT_COLORS.MUTED }}>{formatTime(data.ts_event)}</span>
        </div>
      </div>
    );
  }

  // OHLCV record
  if (data.schema === 'ohlcv') {
    const ohlcv = data as { open?: number; high?: number; low?: number; close?: number; volume?: number } & typeof data;
    return (
      <div
        className="flex items-center justify-between py-0.5 px-1 text-[10px]"
        style={{ backgroundColor: `${FINCEPT_COLORS.BORDER}30`, borderRadius: '2px' }}
      >
        <span className="font-bold" style={{ color: FINCEPT_COLORS.CYAN }}>{data.symbol}</span>
        <div className="flex items-center gap-2 font-mono">
          <span style={{ color: FINCEPT_COLORS.MUTED }}>O:{formatPrice(ohlcv.open)}</span>
          <span style={{ color: FINCEPT_COLORS.GREEN }}>H:{formatPrice(ohlcv.high)}</span>
          <span style={{ color: FINCEPT_COLORS.RED }}>L:{formatPrice(ohlcv.low)}</span>
          <span style={{ color: FINCEPT_COLORS.WHITE }}>C:{formatPrice(ohlcv.close)}</span>
          <span style={{ color: FINCEPT_COLORS.MUTED }}>V:{formatSize(ohlcv.volume)}</span>
        </div>
      </div>
    );
  }

  // Generic record
  return (
    <div className="py-0.5 px-1 text-[10px]" style={{ color: FINCEPT_COLORS.MUTED }}>
      {data.symbol}: {data.record_type}
    </div>
  );
};
