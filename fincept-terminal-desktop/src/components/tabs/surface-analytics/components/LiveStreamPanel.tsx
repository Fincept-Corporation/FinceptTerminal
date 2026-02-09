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
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';

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
  const { t } = useTranslation('surfaceAnalytics');
  const { colors } = useTerminalTheme();
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
          backgroundColor: colors.background,
          border: `1px solid ${colors.textMuted}`,
          borderRadius: 'var(--ft-border-radius)',
        }}
      >
        <Radio size={24} style={{ color: colors.textMuted, margin: '0 auto 8px' }} />
        <div className="text-xs" style={{ color: colors.textMuted }}>
          Configure Databento API key for live streaming
        </div>
      </div>
    );
  }

  return (
    <div
      style={{
        backgroundColor: colors.background,
        border: `1px solid ${colors.textMuted}`,
        borderRadius: 'var(--ft-border-radius)',
        overflow: 'hidden',
      }}
    >
      {/* Header */}
      <div
        className="flex items-center justify-between p-2"
        style={{
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${colors.textMuted}`,
        }}
      >
        <div className="flex items-center gap-2">
          {isConnected ? (
            <Wifi size={14} style={{ color: colors.success }} />
          ) : (
            <WifiOff size={14} style={{ color: colors.textMuted }} />
          )}
          <span className="text-xs font-bold" style={{ color: accentColor }}>
            LIVE STREAM
          </span>
          {isConnected && (
            <span className="text-[9px] px-1" style={{ color: colors.success, backgroundColor: `${colors.success}20`, borderRadius: 'var(--ft-border-radius)' }}>
              CONNECTED
            </span>
          )}
        </div>
        <div className="flex items-center gap-2">
          <Activity size={12} style={{ color: isConnected ? colors.success : colors.textMuted }} />
          <span className="text-[10px]" style={{ color: colors.textMuted }}>
            {recordCount} msgs
          </span>
        </div>
      </div>

      {/* Controls */}
      <div className="p-2 space-y-2" style={{ borderBottom: `1px solid ${colors.textMuted}` }}>
        {/* Dataset Selection */}
        <div className="flex items-center gap-1 flex-wrap">
          <span className="text-[9px]" style={{ color: colors.textMuted, width: '45px' }}>Dataset:</span>
          {LIVE_DATASETS.slice(0, 4).map((ds) => (
            <button
              key={ds.id}
              onClick={() => setSelectedDataset(ds.id)}
              disabled={isConnected}
              className="px-2 py-0.5 text-[9px]"
              style={{
                backgroundColor: selectedDataset === ds.id ? accentColor : colors.textMuted,
                color: selectedDataset === ds.id ? colors.background : textColor,
                borderRadius: 'var(--ft-border-radius)',
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
          <span className="text-[9px]" style={{ color: colors.textMuted, width: '45px' }}>Schema:</span>
          {LIVE_SCHEMAS.map((schema) => (
            <button
              key={schema.id}
              onClick={() => setSelectedSchema(schema.id)}
              disabled={isConnected}
              className="px-2 py-0.5 text-[9px]"
              style={{
                backgroundColor: selectedSchema === schema.id ? colors.textMuted : 'transparent',
                color: selectedSchema === schema.id ? textColor : colors.textMuted,
                border: `1px solid ${colors.textMuted}`,
                borderRadius: 'var(--ft-border-radius)',
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
          <span className="text-[9px]" style={{ color: colors.textMuted, width: '45px' }}>Symbol:</span>
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
                  backgroundColor: selectedSymbol === sym && !customSymbol ? accentColor : colors.textMuted,
                  color: selectedSymbol === sym && !customSymbol ? colors.background : textColor,
                  borderRadius: 'var(--ft-border-radius)',
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
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
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
                backgroundColor: colors.success,
                color: colors.background,
                borderRadius: 'var(--ft-border-radius)',
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
                backgroundColor: colors.alert,
                color: textColor,
                borderRadius: 'var(--ft-border-radius)',
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
              backgroundColor: colors.textMuted,
              color: textColor,
              borderRadius: 'var(--ft-border-radius)',
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
                backgroundColor: `${colors.alert}30`,
                color: colors.alert,
                borderRadius: 'var(--ft-border-radius)',
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
            backgroundColor: `${colors.alert}15`,
            border: `1px solid ${colors.alert}50`,
            borderRadius: 'var(--ft-border-radius)',
            color: colors.alert,
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
          <div className="text-xs text-center p-4" style={{ color: colors.textMuted }}>
            {isConnected ? 'Waiting for data...' : 'Click Start Stream to begin receiving live data'}
          </div>
        ) : (
          <div className="space-y-1">
            {records.map((record, idx) => (
              <RecordRow key={idx} record={record} formatPrice={formatPrice} formatSize={formatSize} formatTime={formatTime} colors={colors} textColor={textColor} />
            ))}
          </div>
        )}
      </div>

      {/* Footer Stats */}
      {isConnected && (
        <div
          className="flex items-center justify-between p-2 text-[9px]"
          style={{
            backgroundColor: colors.background,
            borderTop: `1px solid ${colors.textMuted}`,
            color: colors.textMuted,
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
  colors: ReturnType<typeof useTerminalTheme>['colors'];
  textColor: string;
}> = ({ record, formatPrice, formatSize, formatTime, colors, textColor }) => {
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
          backgroundColor: isBuy ? `${colors.success}10` : isSell ? `${colors.alert}10` : 'transparent',
          borderRadius: 'var(--ft-border-radius)',
        }}
      >
        <div className="flex items-center gap-2">
          {isBuy ? (
            <TrendingUp size={10} style={{ color: colors.success }} />
          ) : isSell ? (
            <TrendingDown size={10} style={{ color: colors.alert }} />
          ) : (
            <Activity size={10} style={{ color: colors.textMuted }} />
          )}
          <span className="font-bold" style={{ color: colors.info }}>{data.symbol}</span>
        </div>
        <div className="flex items-center gap-3">
          <span style={{ color: isBuy ? colors.success : isSell ? colors.alert : textColor }}>
            {formatPrice(data.price)}
          </span>
          <span style={{ color: colors.textMuted }}>{formatSize(data.size)}</span>
          <span style={{ color: colors.textMuted }}>{formatTime(data.ts_event)}</span>
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
        style={{ backgroundColor: `${colors.textMuted}50`, borderRadius: 'var(--ft-border-radius)' }}
      >
        <span className="font-bold" style={{ color: colors.info }}>{data.symbol}</span>
        <div className="flex items-center gap-2">
          <span style={{ color: colors.success }}>{formatPrice(level.bid_px)}</span>
          <span style={{ color: colors.textMuted }}>×</span>
          <span style={{ color: colors.alert }}>{formatPrice(level.ask_px)}</span>
          <span style={{ color: colors.textMuted }}>{formatTime(data.ts_event)}</span>
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
        style={{ backgroundColor: `${colors.textMuted}30`, borderRadius: 'var(--ft-border-radius)' }}
      >
        <span className="font-bold" style={{ color: colors.info }}>{data.symbol}</span>
        <div className="flex items-center gap-2 font-mono">
          <span style={{ color: colors.textMuted }}>O:{formatPrice(ohlcv.open)}</span>
          <span style={{ color: colors.success }}>H:{formatPrice(ohlcv.high)}</span>
          <span style={{ color: colors.alert }}>L:{formatPrice(ohlcv.low)}</span>
          <span style={{ color: textColor }}>C:{formatPrice(ohlcv.close)}</span>
          <span style={{ color: colors.textMuted }}>V:{formatSize(ohlcv.volume)}</span>
        </div>
      </div>
    );
  }

  // Generic record
  return (
    <div className="py-0.5 px-1 text-[10px]" style={{ color: colors.textMuted }}>
      {data.symbol}: {data.record_type}
    </div>
  );
};
