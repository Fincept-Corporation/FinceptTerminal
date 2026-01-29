/**
 * Surface Analytics - Futures Panel Component
 * Display CME futures data with SMART symbology (GLBX.MDP3)
 */

import React, { useState, useCallback, useEffect } from 'react';
import {
  TrendingUp,
  TrendingDown,
  RefreshCw,
  AlertCircle,
  LineChart,
  Info,
  ChevronRight,
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { FINCEPT_COLORS } from '../constants';

interface FuturesContract {
  symbol: string;
  smart_symbol: string;
  name: string;
  exchange: string;
  tick_size: number;
  multiplier: number;
}

interface TermStructurePoint {
  contract_month: number;
  smart_symbol: string;
  close: number;
  volume: number;
}

interface FuturesPanelProps {
  apiKey: string | null;
  accentColor: string;
  textColor: string;
}

const POPULAR_FUTURES = [
  { symbol: 'ES', name: 'E-mini S&P 500', category: 'Index' },
  { symbol: 'NQ', name: 'E-mini NASDAQ', category: 'Index' },
  { symbol: 'CL', name: 'Crude Oil', category: 'Energy' },
  { symbol: 'GC', name: 'Gold', category: 'Metals' },
  { symbol: 'ZN', name: '10-Year T-Note', category: 'Rates' },
  { symbol: '6E', name: 'Euro FX', category: 'Currency' },
];

export const FuturesPanel: React.FC<FuturesPanelProps> = ({
  apiKey,
  accentColor,
  textColor,
}) => {
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [selectedFuture, setSelectedFuture] = useState('ES');
  const [contracts, setContracts] = useState<FuturesContract[]>([]);
  const [termStructure, setTermStructure] = useState<TermStructurePoint[]>([]);
  const [showAllContracts, setShowAllContracts] = useState(false);

  const fetchContracts = useCallback(async () => {
    if (!apiKey) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_list_futures', {
        apiKey,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setContracts(parsed.contracts || []);
      }
    } catch (err) {
      setError(`Failed to fetch contracts: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  const fetchTermStructure = useCallback(async (rootSymbol: string) => {
    if (!apiKey) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_futures_term_structure', {
        apiKey,
        rootSymbol,
        numContracts: 8,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setTermStructure(parsed.term_structure || []);
      }
    } catch (err) {
      setError(`Failed to fetch term structure: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  useEffect(() => {
    if (apiKey) {
      fetchContracts();
    }
  }, [apiKey, fetchContracts]);

  const formatPrice = (price: number): string => {
    if (!price || price === 0) return '--';
    if (price >= 1000) return price.toFixed(2);
    if (price >= 100) return price.toFixed(3);
    return price.toFixed(4);
  };

  const formatVolume = (volume: number): string => {
    if (!volume || volume === 0) return '--';
    if (volume >= 1000000) return `${(volume / 1000000).toFixed(1)}M`;
    if (volume >= 1000) return `${(volume / 1000).toFixed(1)}K`;
    return volume.toString();
  };

  const getContango = (): { isContango: boolean; spread: number } | null => {
    if (termStructure.length < 2) return null;
    const front = termStructure[0]?.close || 0;
    const back = termStructure[termStructure.length - 1]?.close || 0;
    if (!front || !back) return null;
    return {
      isContango: back > front,
      spread: ((back - front) / front) * 100,
    };
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
        <LineChart size={24} style={{ color: FINCEPT_COLORS.MUTED, margin: '0 auto 8px' }} />
        <div className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
          Configure Databento API key for futures data
        </div>
      </div>
    );
  }

  const contango = getContango();

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
          <LineChart size={14} style={{ color: accentColor }} />
          <span className="text-xs font-bold" style={{ color: accentColor }}>
            FUTURES (CME GLOBEX)
          </span>
        </div>
        <button
          onClick={fetchContracts}
          disabled={loading}
          style={{ color: loading ? FINCEPT_COLORS.MUTED : accentColor }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
        </button>
      </div>

      {/* Popular Futures Selector */}
      <div className="flex flex-wrap items-center gap-1 p-2" style={{ borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}` }}>
        <span className="text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>Popular:</span>
        {POPULAR_FUTURES.map((f) => (
          <button
            key={f.symbol}
            onClick={() => {
              setSelectedFuture(f.symbol);
              fetchTermStructure(f.symbol);
            }}
            className="px-2 py-0.5 text-[9px] font-bold"
            style={{
              backgroundColor: selectedFuture === f.symbol ? accentColor : FINCEPT_COLORS.BORDER,
              color: selectedFuture === f.symbol ? FINCEPT_COLORS.BLACK : textColor,
              borderRadius: '2px',
            }}
            title={f.name}
          >
            {f.symbol}
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

      {/* Term Structure */}
      <div className="p-2">
        {loading ? (
          <div className="flex items-center justify-center p-4">
            <RefreshCw size={16} className="animate-spin" style={{ color: accentColor }} />
          </div>
        ) : termStructure.length === 0 ? (
          <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
            Click a futures symbol to view term structure
          </div>
        ) : (
          <div>
            {/* Market Structure Info */}
            {contango && (
              <div
                className="flex items-center justify-between mb-2 p-2 text-xs"
                style={{
                  backgroundColor: FINCEPT_COLORS.BLACK,
                  borderRadius: '2px',
                }}
              >
                <div className="flex items-center gap-2">
                  {contango.isContango ? (
                    <TrendingUp size={12} style={{ color: FINCEPT_COLORS.YELLOW }} />
                  ) : (
                    <TrendingDown size={12} style={{ color: FINCEPT_COLORS.CYAN }} />
                  )}
                  <span style={{ color: FINCEPT_COLORS.MUTED }}>Market Structure:</span>
                  <span
                    style={{
                      color: contango.isContango ? FINCEPT_COLORS.YELLOW : FINCEPT_COLORS.CYAN,
                      fontWeight: 'bold',
                    }}
                  >
                    {contango.isContango ? 'CONTANGO' : 'BACKWARDATION'}
                  </span>
                </div>
                <span style={{ color: contango.isContango ? FINCEPT_COLORS.YELLOW : FINCEPT_COLORS.CYAN }}>
                  {contango.spread > 0 ? '+' : ''}{contango.spread.toFixed(2)}%
                </span>
              </div>
            )}

            {/* Header Row */}
            <div className="grid grid-cols-4 gap-2 mb-1 text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>
              <div>CONTRACT</div>
              <div className="text-right">PRICE</div>
              <div className="text-right">VOLUME</div>
              <div className="text-right">SPREAD</div>
            </div>

            {/* Term Structure Rows */}
            {termStructure.map((point, idx) => {
              const prevClose = idx > 0 ? termStructure[idx - 1]?.close || 0 : point.close;
              const spread = prevClose ? ((point.close - prevClose) / prevClose) * 100 : 0;

              return (
                <div
                  key={point.smart_symbol}
                  className="grid grid-cols-4 gap-2 py-1 text-[10px]"
                  style={{
                    backgroundColor: idx === 0 ? `${accentColor}15` : 'transparent',
                    borderRadius: '2px',
                  }}
                >
                  <div className="flex items-center gap-1">
                    {idx === 0 && <ChevronRight size={10} style={{ color: accentColor }} />}
                    <span style={{ color: idx === 0 ? accentColor : textColor, fontWeight: idx === 0 ? 'bold' : 'normal' }}>
                      {point.smart_symbol}
                    </span>
                    {idx === 0 && (
                      <span className="text-[8px] px-1" style={{ backgroundColor: accentColor, color: FINCEPT_COLORS.BLACK, borderRadius: '2px' }}>
                        FRONT
                      </span>
                    )}
                  </div>
                  <div className="text-right font-mono" style={{ color: FINCEPT_COLORS.WHITE }}>
                    {formatPrice(point.close)}
                  </div>
                  <div className="text-right" style={{ color: FINCEPT_COLORS.MUTED }}>
                    {formatVolume(point.volume)}
                  </div>
                  <div
                    className="text-right font-mono"
                    style={{
                      color: idx === 0 ? FINCEPT_COLORS.MUTED : spread > 0 ? FINCEPT_COLORS.GREEN : spread < 0 ? FINCEPT_COLORS.RED : FINCEPT_COLORS.MUTED,
                    }}
                  >
                    {idx === 0 ? '--' : `${spread > 0 ? '+' : ''}${spread.toFixed(3)}%`}
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* All Contracts Section */}
      <div style={{ borderTop: `1px solid ${FINCEPT_COLORS.BORDER}` }}>
        <button
          onClick={() => setShowAllContracts(!showAllContracts)}
          className="flex items-center justify-between w-full p-2 text-[10px]"
          style={{ color: FINCEPT_COLORS.MUTED }}
        >
          <div className="flex items-center gap-1">
            <Info size={10} />
            <span>Available Contracts ({contracts.length})</span>
          </div>
          <ChevronRight
            size={12}
            style={{
              transform: showAllContracts ? 'rotate(90deg)' : 'none',
              transition: 'transform 0.2s',
            }}
          />
        </button>

        {showAllContracts && (
          <div className="p-2 max-h-48 overflow-y-auto" style={{ backgroundColor: FINCEPT_COLORS.BLACK }}>
            <div className="grid grid-cols-3 gap-1">
              {contracts.map((c) => (
                <button
                  key={c.symbol}
                  onClick={() => {
                    setSelectedFuture(c.symbol);
                    fetchTermStructure(c.symbol);
                    setShowAllContracts(false);
                  }}
                  className="text-left p-1 text-[9px]"
                  style={{
                    backgroundColor: selectedFuture === c.symbol ? `${accentColor}30` : 'transparent',
                    borderRadius: '2px',
                  }}
                  title={`${c.name} (${c.exchange})`}
                >
                  <div className="font-bold" style={{ color: selectedFuture === c.symbol ? accentColor : textColor }}>
                    {c.symbol}
                  </div>
                  <div style={{ color: FINCEPT_COLORS.MUTED, fontSize: '8px' }}>
                    {c.name.length > 15 ? c.name.substring(0, 15) + '...' : c.name}
                  </div>
                </button>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
