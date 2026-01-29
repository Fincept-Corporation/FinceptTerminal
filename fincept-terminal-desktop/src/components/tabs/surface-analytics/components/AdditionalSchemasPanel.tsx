/**
 * Surface Analytics - Additional Schemas Panel Component
 * Display TRADES, IMBALANCE, STATISTICS, and STATUS data
 */

import React, { useState, useCallback } from 'react';
import {
  RefreshCw,
  AlertCircle,
  Activity,
  BarChart3,
  FileText,
  AlertTriangle,
  TrendingUp,
  TrendingDown,
  ArrowRight,
  ChevronDown,
  ChevronUp,
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { FINCEPT_COLORS } from '../constants';

interface TradeRecord {
  ts_event: string;
  symbol: string;
  price: number;
  size: number;
  action: string | null;
  side: string | null;
  sequence: number;
}

interface ImbalanceRecord {
  ts_event: string;
  symbol: string;
  ref_price: number;
  paired_qty: number;
  imbalance_qty: number;
  imbalance_side: string | null;
  auction_type: string | null;
}

interface StatisticsRecord {
  ts_event: string;
  symbol: string;
  stat_type: string | null;
  price: number;
  quantity: number;
  sequence: number;
  ts_ref: string | null;
}

interface StatusRecord {
  ts_event: string;
  symbol: string;
  action: string | null;
  reason: string | null;
  trading_event: string | null;
  is_short_sell_restricted: boolean | null;
}

interface AdditionalSchemasPanelProps {
  apiKey: string | null;
  symbols: string[];
  accentColor: string;
  textColor: string;
}

type SchemaType = 'trades' | 'imbalance' | 'statistics' | 'status';

const SCHEMA_TABS: { id: SchemaType; label: string; icon: React.ReactNode }[] = [
  { id: 'trades', label: 'Trades', icon: <Activity size={12} /> },
  { id: 'imbalance', label: 'Imbalance', icon: <BarChart3 size={12} /> },
  { id: 'statistics', label: 'Statistics', icon: <FileText size={12} /> },
  { id: 'status', label: 'Status', icon: <AlertTriangle size={12} /> },
];

export const AdditionalSchemasPanel: React.FC<AdditionalSchemasPanelProps> = ({
  apiKey,
  symbols,
  accentColor,
  textColor,
}) => {
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [activeSchema, setActiveSchema] = useState<SchemaType>('trades');
  const [trades, setTrades] = useState<TradeRecord[]>([]);
  const [imbalances, setImbalances] = useState<ImbalanceRecord[]>([]);
  const [statistics, setStatistics] = useState<StatisticsRecord[]>([]);
  const [statuses, setStatuses] = useState<StatusRecord[]>([]);
  const [expandedSection, setExpandedSection] = useState<string | null>(null);

  const fetchTrades = useCallback(async () => {
    if (!apiKey) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_trades', {
        apiKey,
        symbols,
        dataset: null,
        days: 1,
        limit: 100,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setTrades(parsed.trades || []);
      }
    } catch (err) {
      setError(`Failed to fetch trades: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, symbols]);

  const fetchImbalances = useCallback(async () => {
    if (!apiKey) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_imbalance', {
        apiKey,
        symbols,
        dataset: null,
        days: 5,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setImbalances(parsed.imbalances || []);
      }
    } catch (err) {
      setError(`Failed to fetch imbalances: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, symbols]);

  const fetchStatistics = useCallback(async () => {
    if (!apiKey) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_statistics', {
        apiKey,
        symbols,
        dataset: null,
        days: 5,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setStatistics(parsed.statistics || []);
      }
    } catch (err) {
      setError(`Failed to fetch statistics: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, symbols]);

  const fetchStatuses = useCallback(async () => {
    if (!apiKey) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_status', {
        apiKey,
        symbols,
        dataset: null,
        days: 5,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setStatuses(parsed.statuses || []);
      }
    } catch (err) {
      setError(`Failed to fetch statuses: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, symbols]);

  const handleSchemaChange = (schema: SchemaType) => {
    setActiveSchema(schema);
    setError(null);

    switch (schema) {
      case 'trades':
        if (trades.length === 0) fetchTrades();
        break;
      case 'imbalance':
        if (imbalances.length === 0) fetchImbalances();
        break;
      case 'statistics':
        if (statistics.length === 0) fetchStatistics();
        break;
      case 'status':
        if (statuses.length === 0) fetchStatuses();
        break;
    }
  };

  const handleRefresh = () => {
    switch (activeSchema) {
      case 'trades':
        fetchTrades();
        break;
      case 'imbalance':
        fetchImbalances();
        break;
      case 'statistics':
        fetchStatistics();
        break;
      case 'status':
        fetchStatuses();
        break;
    }
  };

  const formatPrice = (price: number): string => {
    if (!price || price === 0) return '--';
    return price.toFixed(4);
  };

  const formatTime = (ts: string): string => {
    if (!ts) return '--';
    try {
      const date = new Date(parseInt(ts) / 1000000);
      const timeStr = date.toLocaleTimeString('en-US', { hour12: false });
      const ms = date.getMilliseconds().toString().padStart(3, '0');
      return `${timeStr}.${ms}`;
    } catch {
      return ts;
    }
  };

  const formatQty = (qty: number): string => {
    if (!qty || qty === 0) return '--';
    if (qty >= 1000000) return `${(qty / 1000000).toFixed(1)}M`;
    if (qty >= 1000) return `${(qty / 1000).toFixed(1)}K`;
    return qty.toString();
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
        <Activity size={24} style={{ color: FINCEPT_COLORS.MUTED, margin: '0 auto 8px' }} />
        <div className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
          Configure Databento API key for additional schemas
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
          <Activity size={14} style={{ color: accentColor }} />
          <span className="text-xs font-bold" style={{ color: accentColor }}>
            ADDITIONAL SCHEMAS
          </span>
        </div>
        <button
          onClick={handleRefresh}
          disabled={loading}
          style={{ color: loading ? FINCEPT_COLORS.MUTED : accentColor }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
        </button>
      </div>

      {/* Schema Tabs */}
      <div
        className="flex items-center gap-1 p-2"
        style={{ borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}` }}
      >
        {SCHEMA_TABS.map((tab) => (
          <button
            key={tab.id}
            onClick={() => handleSchemaChange(tab.id)}
            className="flex items-center gap-1 px-2 py-1 text-[10px] font-bold"
            style={{
              backgroundColor: activeSchema === tab.id ? accentColor : FINCEPT_COLORS.BORDER,
              color: activeSchema === tab.id ? FINCEPT_COLORS.BLACK : textColor,
              borderRadius: '2px',
            }}
          >
            {tab.icon}
            {tab.label}
          </button>
        ))}
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

      {/* Content */}
      <div className="p-2 max-h-64 overflow-y-auto">
        {loading ? (
          <div className="flex items-center justify-center p-4">
            <RefreshCw size={16} className="animate-spin" style={{ color: accentColor }} />
          </div>
        ) : (
          <>
            {/* Trades View */}
            {activeSchema === 'trades' && (
              trades.length === 0 ? (
                <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
                  Click refresh to load trades data
                </div>
              ) : (
                <div>
                  <div className="grid grid-cols-5 gap-2 mb-1 text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>
                    <div>TIME</div>
                    <div>SYMBOL</div>
                    <div className="text-right">PRICE</div>
                    <div className="text-right">SIZE</div>
                    <div className="text-right">SIDE</div>
                  </div>
                  {trades.slice(0, 20).map((trade, idx) => (
                    <div key={idx} className="grid grid-cols-5 gap-2 py-0.5 text-[10px]">
                      <div style={{ color: FINCEPT_COLORS.MUTED }}>{formatTime(trade.ts_event)}</div>
                      <div style={{ color: accentColor, fontWeight: 'bold' }}>{trade.symbol}</div>
                      <div className="text-right font-mono" style={{ color: FINCEPT_COLORS.WHITE }}>{formatPrice(trade.price)}</div>
                      <div className="text-right" style={{ color: FINCEPT_COLORS.MUTED }}>{formatQty(trade.size)}</div>
                      <div className="text-right flex items-center justify-end">
                        {trade.side === 'B' || trade.side === 'BUY' ? (
                          <TrendingUp size={10} style={{ color: FINCEPT_COLORS.GREEN }} />
                        ) : trade.side === 'S' || trade.side === 'SELL' ? (
                          <TrendingDown size={10} style={{ color: FINCEPT_COLORS.RED }} />
                        ) : (
                          <ArrowRight size={10} style={{ color: FINCEPT_COLORS.MUTED }} />
                        )}
                      </div>
                    </div>
                  ))}
                </div>
              )
            )}

            {/* Imbalance View */}
            {activeSchema === 'imbalance' && (
              imbalances.length === 0 ? (
                <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
                  Click refresh to load imbalance data (available at market open/close)
                </div>
              ) : (
                <div>
                  <div className="grid grid-cols-5 gap-2 mb-1 text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>
                    <div>TIME</div>
                    <div>SYMBOL</div>
                    <div className="text-right">REF PRICE</div>
                    <div className="text-right">IMBALANCE</div>
                    <div className="text-right">TYPE</div>
                  </div>
                  {imbalances.slice(0, 20).map((imb, idx) => (
                    <div key={idx} className="grid grid-cols-5 gap-2 py-0.5 text-[10px]">
                      <div style={{ color: FINCEPT_COLORS.MUTED }}>{formatTime(imb.ts_event)}</div>
                      <div style={{ color: accentColor, fontWeight: 'bold' }}>{imb.symbol}</div>
                      <div className="text-right font-mono" style={{ color: FINCEPT_COLORS.WHITE }}>{formatPrice(imb.ref_price)}</div>
                      <div className="text-right" style={{ color: imb.imbalance_side === 'B' ? FINCEPT_COLORS.GREEN : FINCEPT_COLORS.RED }}>
                        {formatQty(imb.imbalance_qty)} {imb.imbalance_side}
                      </div>
                      <div className="text-right" style={{ color: FINCEPT_COLORS.MUTED }}>{imb.auction_type || '--'}</div>
                    </div>
                  ))}
                </div>
              )
            )}

            {/* Statistics View */}
            {activeSchema === 'statistics' && (
              statistics.length === 0 ? (
                <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
                  Click refresh to load statistics (trading halts, circuit breakers)
                </div>
              ) : (
                <div>
                  <div className="grid grid-cols-4 gap-2 mb-1 text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>
                    <div>TIME</div>
                    <div>SYMBOL</div>
                    <div>TYPE</div>
                    <div className="text-right">VALUE</div>
                  </div>
                  {statistics.slice(0, 20).map((stat, idx) => (
                    <div key={idx} className="grid grid-cols-4 gap-2 py-0.5 text-[10px]">
                      <div style={{ color: FINCEPT_COLORS.MUTED }}>{formatTime(stat.ts_event)}</div>
                      <div style={{ color: accentColor, fontWeight: 'bold' }}>{stat.symbol}</div>
                      <div style={{ color: FINCEPT_COLORS.YELLOW }}>{stat.stat_type || '--'}</div>
                      <div className="text-right font-mono" style={{ color: FINCEPT_COLORS.WHITE }}>
                        {stat.price > 0 ? formatPrice(stat.price) : formatQty(stat.quantity)}
                      </div>
                    </div>
                  ))}
                </div>
              )
            )}

            {/* Status View */}
            {activeSchema === 'status' && (
              statuses.length === 0 ? (
                <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
                  Click refresh to load trading status (halts, Reg SHO)
                </div>
              ) : (
                <div>
                  <div className="grid grid-cols-4 gap-2 mb-1 text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>
                    <div>TIME</div>
                    <div>SYMBOL</div>
                    <div>ACTION</div>
                    <div>REASON</div>
                  </div>
                  {statuses.slice(0, 20).map((status, idx) => (
                    <div key={idx} className="grid grid-cols-4 gap-2 py-0.5 text-[10px]">
                      <div style={{ color: FINCEPT_COLORS.MUTED }}>{formatTime(status.ts_event)}</div>
                      <div style={{ color: accentColor, fontWeight: 'bold' }}>{status.symbol}</div>
                      <div style={{ color: status.action === 'HALT' ? FINCEPT_COLORS.RED : FINCEPT_COLORS.GREEN }}>
                        {status.action || '--'}
                      </div>
                      <div style={{ color: FINCEPT_COLORS.MUTED }}>{status.reason || status.trading_event || '--'}</div>
                    </div>
                  ))}
                  {statuses.some(s => s.is_short_sell_restricted) && (
                    <div className="mt-2 p-1 text-[9px]" style={{ backgroundColor: FINCEPT_COLORS.RED + '20', borderRadius: '2px', color: FINCEPT_COLORS.RED }}>
                      Some symbols have Reg SHO restrictions
                    </div>
                  )}
                </div>
              )
            )}
          </>
        )}
      </div>

      {/* Data Count Footer */}
      <div
        className="flex items-center justify-between p-2 text-[9px]"
        style={{
          borderTop: `1px solid ${FINCEPT_COLORS.BORDER}`,
          color: FINCEPT_COLORS.MUTED,
        }}
      >
        <span>Symbols: {symbols.join(', ')}</span>
        <span>
          {activeSchema === 'trades' && `${trades.length} trades`}
          {activeSchema === 'imbalance' && `${imbalances.length} imbalances`}
          {activeSchema === 'statistics' && `${statistics.length} stats`}
          {activeSchema === 'status' && `${statuses.length} statuses`}
        </span>
      </div>
    </div>
  );
};
