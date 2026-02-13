/**
 * Broker Selector Component
 *
 * Allows users to select and configure brokers/exchanges for
 * trading in Alpha Arena. Supports multiple broker types.
 */

import React, { useState, useEffect, useReducer, useRef } from 'react';
import {
  Building, RefreshCw, Check, AlertTriangle, Loader2,
  Globe, Zap, ChevronDown,
  ChevronRight, Search, Star, StarOff,
} from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import { useCache, cacheKey } from '@/hooks/useCache';
import { validateString } from '@/services/core/validators';
import {
  alphaArenaEnhancedService,
  type BrokerInfo,
  type TickerData,
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

const BROKER_STYLES = `
  .broker-panel *::-webkit-scrollbar { width: 6px; height: 6px; }
  .broker-panel *::-webkit-scrollbar-track { background: #000000; }
  .broker-panel *::-webkit-scrollbar-thumb { background: #2A2A2A; }
  @keyframes broker-spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
`;

const BROKER_TYPE_COLORS: Record<string, string> = {
  crypto: FINCEPT.ORANGE,
  stocks: FINCEPT.BLUE,
  forex: FINCEPT.GREEN,
};

const REGION_FLAGS: Record<string, string> = {
  global: '🌍',
  us: '🇺🇸',
  india: '🇮🇳',
  europe: '🇪🇺',
  asia: '🌏',
};

interface BrokerSelectorProps {
  selectedBrokerId?: string;
  onBrokerSelect?: (broker: BrokerInfo) => void;
  filterType?: 'crypto' | 'stocks' | 'forex';
  filterRegion?: 'global' | 'us' | 'india' | 'europe' | 'asia';
}

// Ticker fetch state machine
type TickerState =
  | { status: 'idle'; data: TickerData | null }
  | { status: 'loading'; data: TickerData | null }
  | { status: 'success'; data: TickerData }
  | { status: 'error'; data: TickerData | null; error: string };

type TickerAction =
  | { type: 'FETCH_START' }
  | { type: 'FETCH_SUCCESS'; data: TickerData }
  | { type: 'FETCH_ERROR'; error: string }
  | { type: 'RESET' };

function tickerReducer(state: TickerState, action: TickerAction): TickerState {
  switch (action.type) {
    case 'FETCH_START':
      return { status: 'loading', data: state.data };
    case 'FETCH_SUCCESS':
      return { status: 'success', data: action.data };
    case 'FETCH_ERROR':
      return { status: 'error', data: state.data, error: action.error };
    case 'RESET':
      return { status: 'idle', data: null };
    default:
      return state;
  }
}

const BrokerSelector: React.FC<BrokerSelectorProps> = ({
  selectedBrokerId,
  onBrokerSelect,
  filterType,
  filterRegion,
}) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedBroker, setSelectedBroker] = useState<BrokerInfo | null>(null);
  const [tickerState, dispatchTicker] = useReducer(tickerReducer, { status: 'idle', data: null });
  const [expandedBroker, setExpandedBroker] = useState<string | null>(null);
  const [favorites, setFavorites] = useState<string[]>(() => {
    const saved = localStorage.getItem('alpha-arena-favorite-brokers');
    return saved ? JSON.parse(saved) : [];
  });
  const [typeFilter, setTypeFilter] = useState<'crypto' | 'stocks' | 'forex' | null>(filterType || null);
  const [regionFilter, setRegionFilter] = useState<string | null>(filterRegion || null);
  const mountedRef = useRef(true);
  const [hoveredBtn, setHoveredBtn] = useState<string | null>(null);
  const [hoveredBrokerId, setHoveredBrokerId] = useState<string | null>(null);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  // Cache: broker list keyed on filters
  const brokersCache = useCache<BrokerInfo[]>({
    key: cacheKey('alpha-arena', 'brokers', typeFilter || 'all', regionFilter || 'all'),
    category: 'alpha-arena',
    fetcher: async () => {
      const result = await alphaArenaEnhancedService.listBrokers(
        typeFilter || undefined,
        regionFilter as 'global' | 'us' | 'india' | 'europe' | 'asia' | undefined
      );
      if (result.success && result.brokers) {
        return result.brokers;
      }
      throw new Error(result.error || 'Failed to fetch brokers');
    },
    ttl: 300,
    enabled: true,
    staleWhileRevalidate: true,
  });

  const brokers = brokersCache.data || [];
  const isLoading = brokersCache.isLoading;
  const error = brokersCache.error;

  // Auto-select broker when data loads and selectedBrokerId is provided
  useEffect(() => {
    if (selectedBrokerId && brokers.length > 0 && !selectedBroker) {
      const broker = brokers.find(b => b.id === selectedBrokerId);
      if (broker) {
        setSelectedBroker(broker);
      }
    }
  }, [selectedBrokerId, brokers, selectedBroker]);

  useEffect(() => {
    localStorage.setItem('alpha-arena-favorite-brokers', JSON.stringify(favorites));
  }, [favorites]);

  const handleBrokerSelect = async (broker: BrokerInfo) => {
    setSelectedBroker(broker);
    onBrokerSelect?.(broker);

    if (broker.default_symbols.length > 0) {
      dispatchTicker({ type: 'FETCH_START' });
      try {
        const result = await alphaArenaEnhancedService.getBrokerTicker(
          broker.default_symbols[0],
          broker.id
        );
        if (!mountedRef.current) return;
        if (result.success && result.ticker) {
          dispatchTicker({ type: 'FETCH_SUCCESS', data: result.ticker });
        } else {
          dispatchTicker({ type: 'FETCH_ERROR', error: result.error || 'Failed to fetch ticker' });
        }
      } catch (err) {
        if (!mountedRef.current) return;
        dispatchTicker({ type: 'FETCH_ERROR', error: err instanceof Error ? err.message : 'Failed to fetch ticker' });
      }
    }
  };

  const toggleFavorite = (brokerId: string, e: React.MouseEvent) => {
    e.stopPropagation();
    setFavorites(prev =>
      prev.includes(brokerId)
        ? prev.filter(id => id !== brokerId)
        : [...prev, brokerId]
    );
  };

  const filteredBrokers = brokers.filter(broker => {
    const matchesSearch = broker.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      broker.display_name.toLowerCase().includes(searchQuery.toLowerCase());
    return matchesSearch;
  });

  const sortedBrokers = [...filteredBrokers].sort((a, b) => {
    const aFav = favorites.includes(a.id) ? -1 : 0;
    const bFav = favorites.includes(b.id) ? -1 : 0;
    if (aFav !== bFav) return aFav - bFav;
    return a.display_name.localeCompare(b.display_name);
  });

  const renderBrokerFeatures = (broker: BrokerInfo) => (
    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px', marginTop: '8px' }}>
      {broker.supports_spot && (
        <span style={{ fontSize: '9px', padding: '1px 6px', backgroundColor: FINCEPT.GREEN + '20', color: FINCEPT.GREEN, fontFamily: TERMINAL_FONT }}>
          Spot
        </span>
      )}
      {broker.supports_margin && (
        <span style={{ fontSize: '9px', padding: '1px 6px', backgroundColor: FINCEPT.YELLOW + '20', color: FINCEPT.YELLOW, fontFamily: TERMINAL_FONT }}>
          Margin
        </span>
      )}
      {broker.supports_futures && (
        <span style={{ fontSize: '9px', padding: '1px 6px', backgroundColor: FINCEPT.PURPLE + '20', color: FINCEPT.PURPLE, fontFamily: TERMINAL_FONT }}>
          Futures
        </span>
      )}
      {broker.supports_options && (
        <span style={{ fontSize: '9px', padding: '1px 6px', backgroundColor: FINCEPT.BLUE + '20', color: FINCEPT.BLUE, fontFamily: TERMINAL_FONT }}>
          Options
        </span>
      )}
      {broker.websocket_enabled && (
        <span style={{ fontSize: '9px', padding: '1px 6px', backgroundColor: FINCEPT.ORANGE + '20', color: FINCEPT.ORANGE, fontFamily: TERMINAL_FONT, display: 'flex', alignItems: 'center', gap: '2px' }}>
          <Zap size={8} /> WS
        </span>
      )}
    </div>
  );

  const tickerData = tickerState.data;
  const isLoadingTicker = tickerState.status === 'loading';

  const selectStyle: React.CSSProperties = {
    flex: 1,
    padding: '4px 8px',
    fontSize: '10px',
    fontFamily: TERMINAL_FONT,
    backgroundColor: FINCEPT.CARD_BG,
    color: FINCEPT.WHITE,
    border: `1px solid ${FINCEPT.BORDER}`,
    outline: 'none',
  };

  return (
    <div className="broker-panel" style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      overflow: 'hidden',
      fontFamily: TERMINAL_FONT,
    }}>
      <style>{BROKER_STYLES}</style>

      {/* Header */}
      <div style={{
        padding: '10px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Building size={16} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontWeight: 600, fontSize: '12px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            BROKER SELECTION
          </span>
          {selectedBroker && (
            <span style={{
              padding: '1px 8px',
              fontSize: '10px',
              backgroundColor: FINCEPT.GREEN + '20',
              color: FINCEPT.GREEN,
              fontFamily: TERMINAL_FONT,
            }}>
              {selectedBroker.display_name}
            </span>
          )}
        </div>
        <button
          onClick={() => brokersCache.refresh()}
          disabled={isLoading}
          onMouseEnter={() => setHoveredBtn('refresh')}
          onMouseLeave={() => setHoveredBtn(null)}
          style={{
            padding: '4px',
            backgroundColor: hoveredBtn === 'refresh' ? FINCEPT.HOVER : 'transparent',
            border: 'none',
            cursor: isLoading ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
          }}
        >
          <RefreshCw
            size={14}
            style={{
              color: FINCEPT.GRAY,
              animation: isLoading ? 'broker-spin 1s linear infinite' : 'none',
            }}
          />
        </button>
      </div>

      {/* Search and Filters */}
      <div style={{ padding: '12px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ position: 'relative', marginBottom: '8px' }}>
          <Search size={14} style={{ position: 'absolute', left: '8px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.GRAY }} />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search brokers..."
            style={{
              width: '100%',
              paddingLeft: '30px',
              paddingRight: '12px',
              paddingTop: '6px',
              paddingBottom: '6px',
              fontSize: '12px',
              fontFamily: TERMINAL_FONT,
              backgroundColor: FINCEPT.CARD_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              outline: 'none',
            }}
          />
        </div>
        <div style={{ display: 'flex', gap: '8px' }}>
          <select
            value={typeFilter || ''}
            onChange={(e) => setTypeFilter(e.target.value ? e.target.value as 'crypto' | 'stocks' | 'forex' : null)}
            style={selectStyle}
          >
            <option value="">All Types</option>
            <option value="crypto">Crypto</option>
            <option value="stocks">Stocks</option>
            <option value="forex">Forex</option>
          </select>
          <select
            value={regionFilter || ''}
            onChange={(e) => setRegionFilter(e.target.value || null)}
            style={selectStyle}
          >
            <option value="">All Regions</option>
            <option value="global">Global</option>
            <option value="us">United States</option>
            <option value="india">India</option>
            <option value="europe">Europe</option>
            <option value="asia">Asia</option>
          </select>
        </div>
      </div>

      {/* Error */}
      {error && (
        <div style={{
          padding: '8px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          backgroundColor: FINCEPT.RED + '10',
        }}>
          <AlertTriangle size={14} style={{ color: FINCEPT.RED }} />
          <span style={{ fontSize: '11px', color: FINCEPT.RED, fontFamily: TERMINAL_FONT }}>{error.message}</span>
        </div>
      )}

      {/* Broker List */}
      <div style={{ maxHeight: '256px', overflowY: 'auto' }}>
        {isLoading ? (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '32px 0' }}>
            <Loader2 size={24} style={{ color: FINCEPT.ORANGE, animation: 'broker-spin 1s linear infinite' }} />
          </div>
        ) : sortedBrokers.length === 0 ? (
          <div style={{ padding: '32px 16px', textAlign: 'center' }}>
            <Building size={32} style={{ color: FINCEPT.GRAY, opacity: 0.3, margin: '0 auto 8px' }} />
            <p style={{ fontSize: '12px', color: FINCEPT.GRAY }}>No brokers found</p>
            <p style={{ fontSize: '10px', marginTop: '4px', color: FINCEPT.GRAY }}>
              Try adjusting your filters
            </p>
          </div>
        ) : (
          <div>
            {sortedBrokers.map((broker, idx) => (
              <div key={broker.id} style={{
                padding: '12px',
                borderBottom: idx < sortedBrokers.length - 1 ? `1px solid ${FINCEPT.BORDER}` : 'none',
                backgroundColor: hoveredBrokerId === broker.id ? FINCEPT.HOVER : 'transparent',
              }}
              onMouseEnter={() => setHoveredBrokerId(broker.id)}
              onMouseLeave={() => setHoveredBrokerId(null)}
              >
                <button
                  onClick={() => handleBrokerSelect(broker)}
                  style={{
                    width: '100%',
                    textAlign: 'left',
                    background: 'none',
                    border: selectedBroker?.id === broker.id ? `1px solid ${FINCEPT.ORANGE}` : 'none',
                    cursor: 'pointer',
                    fontFamily: TERMINAL_FONT,
                    padding: selectedBroker?.id === broker.id ? '4px' : 0,
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <span style={{ fontSize: '16px' }}>{REGION_FLAGS[broker.region] || '🌐'}</span>
                      <div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                          <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>
                            {broker.display_name}
                          </span>
                          <span style={{
                            fontSize: '9px',
                            padding: '1px 6px',
                            backgroundColor: BROKER_TYPE_COLORS[broker.broker_type] + '20',
                            color: BROKER_TYPE_COLORS[broker.broker_type],
                            fontFamily: TERMINAL_FONT,
                          }}>
                            {broker.broker_type.toUpperCase()}
                          </span>
                          {selectedBroker?.id === broker.id && (
                            <Check size={14} style={{ color: FINCEPT.GREEN }} />
                          )}
                        </div>
                        <div style={{ fontSize: '10px', marginTop: '2px', color: FINCEPT.GRAY }}>
                          Fees: {(broker.maker_fee * 100).toFixed(2)}% / {(broker.taker_fee * 100).toFixed(2)}%
                          {broker.max_leverage > 1 && ` | ${broker.max_leverage}x leverage`}
                        </div>
                      </div>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <button
                        onClick={(e) => toggleFavorite(broker.id, e)}
                        onMouseEnter={() => setHoveredBtn(`fav-${broker.id}`)}
                        onMouseLeave={() => setHoveredBtn(null)}
                        style={{
                          padding: '4px',
                          backgroundColor: hoveredBtn === `fav-${broker.id}` ? FINCEPT.HEADER_BG : 'transparent',
                          border: 'none',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                        }}
                      >
                        {favorites.includes(broker.id) ? (
                          <Star size={14} style={{ color: FINCEPT.YELLOW }} fill={FINCEPT.YELLOW} />
                        ) : (
                          <StarOff size={14} style={{ color: FINCEPT.GRAY }} />
                        )}
                      </button>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setExpandedBroker(expandedBroker === broker.id ? null : broker.id);
                        }}
                        onMouseEnter={() => setHoveredBtn(`expand-${broker.id}`)}
                        onMouseLeave={() => setHoveredBtn(null)}
                        style={{
                          padding: '4px',
                          backgroundColor: hoveredBtn === `expand-${broker.id}` ? FINCEPT.HEADER_BG : 'transparent',
                          border: 'none',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                        }}
                      >
                        {expandedBroker === broker.id ? (
                          <ChevronDown size={14} style={{ color: FINCEPT.GRAY }} />
                        ) : (
                          <ChevronRight size={14} style={{ color: FINCEPT.GRAY }} />
                        )}
                      </button>
                    </div>
                  </div>

                  {/* Features Tags */}
                  {renderBrokerFeatures(broker)}
                </button>

                {/* Expanded Details */}
                {expandedBroker === broker.id && (
                  <div style={{ marginTop: '12px', paddingTop: '12px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', fontSize: '11px' }}>
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Region: </span>
                        <span style={{ color: FINCEPT.WHITE }}>
                          {REGION_FLAGS[broker.region]} {broker.region.charAt(0).toUpperCase() + broker.region.slice(1)}
                        </span>
                      </div>
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Max Leverage: </span>
                        <span style={{ color: FINCEPT.WHITE }}>{broker.max_leverage}x</span>
                      </div>
                    </div>
                    <div style={{ marginTop: '8px' }}>
                      <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Default Symbols:</span>
                      <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px', marginTop: '4px' }}>
                        {broker.default_symbols.slice(0, 5).map((symbol) => (
                          <span
                            key={symbol}
                            style={{
                              fontSize: '9px',
                              padding: '1px 6px',
                              backgroundColor: FINCEPT.BORDER,
                              color: FINCEPT.WHITE,
                              fontFamily: TERMINAL_FONT,
                            }}
                          >
                            {symbol}
                          </span>
                        ))}
                        {broker.default_symbols.length > 5 && (
                          <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                            +{broker.default_symbols.length - 5} more
                          </span>
                        )}
                      </div>
                    </div>
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Selected Broker Ticker Preview */}
      {selectedBroker && tickerData && (
        <div style={{
          padding: '12px 16px',
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.CARD_BG,
        }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Sample Price:</span>
              <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>
                {tickerData.symbol}
              </span>
            </div>
            {isLoadingTicker ? (
              <Loader2 size={14} style={{ color: FINCEPT.GRAY, animation: 'broker-spin 1s linear infinite' }} />
            ) : (
              <div style={{ textAlign: 'right' }}>
                <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>
                  ${tickerData.price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                  24h Vol: {tickerData.volume_24h.toLocaleString()}
                </div>
              </div>
            )}
          </div>
        </div>
      )}

      {/* Footer */}
      <div style={{
        padding: '6px 16px',
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '9px',
        backgroundColor: FINCEPT.HEADER_BG,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <span style={{ color: FINCEPT.GRAY }}>
          {sortedBrokers.length} broker{sortedBrokers.length !== 1 ? 's' : ''} available
        </span>
        {favorites.length > 0 && (
          <span style={{ color: FINCEPT.YELLOW, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <Star size={10} fill={FINCEPT.YELLOW} />
            {favorites.length} favorite{favorites.length !== 1 ? 's' : ''}
          </span>
        )}
      </div>
    </div>
  );
};

export default withErrorBoundary(BrokerSelector, { name: 'AlphaArena.BrokerSelector' });
