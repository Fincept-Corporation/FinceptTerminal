/**
 * Broker Selector Component
 *
 * Allows users to select and configure brokers/exchanges for
 * trading in Alpha Arena. Supports multiple broker types.
 */

import React, { useState, useEffect, useReducer, useRef } from 'react';
import {
  Building, RefreshCw, Check, AlertTriangle, Loader2,
  Globe, DollarSign, Percent, Zap, Settings, ChevronDown,
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

const BROKER_TYPE_COLORS: Record<string, string> = {
  crypto: COLORS.ORANGE,
  stocks: COLORS.BLUE,
  forex: COLORS.GREEN,
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

    // Fetch sample ticker data
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
    <div className="flex flex-wrap gap-1 mt-2">
      {broker.supports_spot && (
        <span className="text-xs px-1.5 py-0.5 rounded" style={{ backgroundColor: COLORS.GREEN + '20', color: COLORS.GREEN }}>
          Spot
        </span>
      )}
      {broker.supports_margin && (
        <span className="text-xs px-1.5 py-0.5 rounded" style={{ backgroundColor: COLORS.YELLOW + '20', color: COLORS.YELLOW }}>
          Margin
        </span>
      )}
      {broker.supports_futures && (
        <span className="text-xs px-1.5 py-0.5 rounded" style={{ backgroundColor: COLORS.PURPLE + '20', color: COLORS.PURPLE }}>
          Futures
        </span>
      )}
      {broker.supports_options && (
        <span className="text-xs px-1.5 py-0.5 rounded" style={{ backgroundColor: COLORS.BLUE + '20', color: COLORS.BLUE }}>
          Options
        </span>
      )}
      {broker.websocket_enabled && (
        <span className="text-xs px-1.5 py-0.5 rounded" style={{ backgroundColor: COLORS.ORANGE + '20', color: COLORS.ORANGE }}>
          <Zap size={10} className="inline" /> WS
        </span>
      )}
    </div>
  );

  const tickerData = tickerState.data;
  const isLoadingTicker = tickerState.status === 'loading';

  return (
    <div
      className="rounded-lg overflow-hidden"
      style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}
    >
      {/* Header */}
      <div className="px-4 py-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-2">
          <Building size={16} style={{ color: COLORS.ORANGE }} />
          <span className="font-semibold text-sm" style={{ color: COLORS.WHITE }}>
            Broker Selection
          </span>
          {selectedBroker && (
            <span
              className="px-2 py-0.5 rounded text-xs"
              style={{ backgroundColor: COLORS.GREEN + '20', color: COLORS.GREEN }}
            >
              {selectedBroker.display_name}
            </span>
          )}
        </div>
        <button
          onClick={() => brokersCache.refresh()}
          disabled={isLoading}
          className="p-1.5 rounded transition-colors hover:bg-[#1A1A1A]"
        >
          <RefreshCw
            size={14}
            className={isLoading ? 'animate-spin' : ''}
            style={{ color: COLORS.GRAY }}
          />
        </button>
      </div>

      {/* Search and Filters */}
      <div className="px-4 py-3 border-b space-y-2" style={{ borderColor: COLORS.BORDER }}>
        <div className="relative">
          <Search size={14} className="absolute left-2 top-1/2 -translate-y-1/2" style={{ color: COLORS.GRAY }} />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search brokers..."
            className="w-full pl-8 pr-3 py-1.5 rounded text-sm"
            style={{
              backgroundColor: COLORS.CARD_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
            }}
          />
        </div>
        <div className="flex gap-2">
          {/* Type Filter */}
          <select
            value={typeFilter || ''}
            onChange={(e) => setTypeFilter(e.target.value ? e.target.value as 'crypto' | 'stocks' | 'forex' : null)}
            className="flex-1 px-2 py-1 rounded text-xs"
            style={{
              backgroundColor: COLORS.CARD_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
            }}
          >
            <option value="">All Types</option>
            <option value="crypto">Crypto</option>
            <option value="stocks">Stocks</option>
            <option value="forex">Forex</option>
          </select>
          {/* Region Filter */}
          <select
            value={regionFilter || ''}
            onChange={(e) => setRegionFilter(e.target.value || null)}
            className="flex-1 px-2 py-1 rounded text-xs"
            style={{
              backgroundColor: COLORS.CARD_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
            }}
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
        <div className="px-4 py-2 flex items-center gap-2" style={{ backgroundColor: COLORS.RED + '10' }}>
          <AlertTriangle size={14} style={{ color: COLORS.RED }} />
          <span className="text-xs" style={{ color: COLORS.RED }}>{error.message}</span>
        </div>
      )}

      {/* Broker List */}
      <div className="max-h-64 overflow-y-auto">
        {isLoading ? (
          <div className="flex items-center justify-center py-8">
            <Loader2 size={24} className="animate-spin" style={{ color: COLORS.ORANGE }} />
          </div>
        ) : sortedBrokers.length === 0 ? (
          <div className="p-8 text-center">
            <Building size={32} className="mx-auto mb-2 opacity-30" style={{ color: COLORS.GRAY }} />
            <p className="text-sm" style={{ color: COLORS.GRAY }}>No brokers found</p>
            <p className="text-xs mt-1" style={{ color: COLORS.GRAY }}>
              Try adjusting your filters
            </p>
          </div>
        ) : (
          <div className="divide-y" style={{ borderColor: COLORS.BORDER }}>
            {sortedBrokers.map((broker) => (
              <div key={broker.id} className="p-3">
                <button
                  onClick={() => handleBrokerSelect(broker)}
                  className={`w-full text-left ${selectedBroker?.id === broker.id ? 'ring-1' : ''}`}
                  style={{
                    outlineColor: selectedBroker?.id === broker.id ? COLORS.ORANGE : 'transparent',
                  }}
                >
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      <span className="text-lg">{REGION_FLAGS[broker.region] || '🌐'}</span>
                      <div>
                        <div className="flex items-center gap-2">
                          <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>
                            {broker.display_name}
                          </span>
                          <span
                            className="text-xs px-1.5 py-0.5 rounded"
                            style={{
                              backgroundColor: BROKER_TYPE_COLORS[broker.broker_type] + '20',
                              color: BROKER_TYPE_COLORS[broker.broker_type],
                            }}
                          >
                            {broker.broker_type}
                          </span>
                          {selectedBroker?.id === broker.id && (
                            <Check size={14} style={{ color: COLORS.GREEN }} />
                          )}
                        </div>
                        <div className="text-xs mt-0.5" style={{ color: COLORS.GRAY }}>
                          Fees: {(broker.maker_fee * 100).toFixed(2)}% / {(broker.taker_fee * 100).toFixed(2)}%
                          {broker.max_leverage > 1 && ` • ${broker.max_leverage}x leverage`}
                        </div>
                      </div>
                    </div>
                    <div className="flex items-center gap-2">
                      <button
                        onClick={(e) => toggleFavorite(broker.id, e)}
                        className="p-1 rounded hover:bg-[#1A1A1A]"
                      >
                        {favorites.includes(broker.id) ? (
                          <Star size={14} style={{ color: COLORS.YELLOW }} fill={COLORS.YELLOW} />
                        ) : (
                          <StarOff size={14} style={{ color: COLORS.GRAY }} />
                        )}
                      </button>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setExpandedBroker(expandedBroker === broker.id ? null : broker.id);
                        }}
                        className="p-1 rounded hover:bg-[#1A1A1A]"
                      >
                        {expandedBroker === broker.id ? (
                          <ChevronDown size={14} style={{ color: COLORS.GRAY }} />
                        ) : (
                          <ChevronRight size={14} style={{ color: COLORS.GRAY }} />
                        )}
                      </button>
                    </div>
                  </div>

                  {/* Features Tags */}
                  {renderBrokerFeatures(broker)}
                </button>

                {/* Expanded Details */}
                {expandedBroker === broker.id && (
                  <div className="mt-3 pt-3 border-t" style={{ borderColor: COLORS.BORDER }}>
                    <div className="grid grid-cols-2 gap-2 text-xs">
                      <div>
                        <span style={{ color: COLORS.GRAY }}>Region: </span>
                        <span style={{ color: COLORS.WHITE }}>
                          {REGION_FLAGS[broker.region]} {broker.region.charAt(0).toUpperCase() + broker.region.slice(1)}
                        </span>
                      </div>
                      <div>
                        <span style={{ color: COLORS.GRAY }}>Max Leverage: </span>
                        <span style={{ color: COLORS.WHITE }}>{broker.max_leverage}x</span>
                      </div>
                    </div>
                    <div className="mt-2">
                      <span className="text-xs" style={{ color: COLORS.GRAY }}>Default Symbols:</span>
                      <div className="flex flex-wrap gap-1 mt-1">
                        {broker.default_symbols.slice(0, 5).map((symbol) => (
                          <span
                            key={symbol}
                            className="text-xs px-1.5 py-0.5 rounded"
                            style={{ backgroundColor: COLORS.BORDER, color: COLORS.WHITE }}
                          >
                            {symbol}
                          </span>
                        ))}
                        {broker.default_symbols.length > 5 && (
                          <span className="text-xs" style={{ color: COLORS.GRAY }}>
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
        <div className="px-4 py-3 border-t" style={{ borderColor: COLORS.BORDER, backgroundColor: COLORS.CARD_BG }}>
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <span className="text-xs" style={{ color: COLORS.GRAY }}>Sample Price:</span>
              <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>
                {tickerData.symbol}
              </span>
            </div>
            {isLoadingTicker ? (
              <Loader2 size={14} className="animate-spin" style={{ color: COLORS.GRAY }} />
            ) : (
              <div className="text-right">
                <div className="text-sm font-bold" style={{ color: COLORS.WHITE }}>
                  ${tickerData.price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                </div>
                <div className="text-xs" style={{ color: COLORS.GRAY }}>
                  24h Vol: {tickerData.volume_24h.toLocaleString()}
                </div>
              </div>
            )}
          </div>
        </div>
      )}

      {/* Footer */}
      <div className="px-4 py-2 border-t text-xs flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <span style={{ color: COLORS.GRAY }}>
          {sortedBrokers.length} broker{sortedBrokers.length !== 1 ? 's' : ''} available
        </span>
        {favorites.length > 0 && (
          <span style={{ color: COLORS.YELLOW }}>
            <Star size={10} className="inline mr-1" fill={COLORS.YELLOW} />
            {favorites.length} favorite{favorites.length !== 1 ? 's' : ''}
          </span>
        )}
      </div>
    </div>
  );
};

export default withErrorBoundary(BrokerSelector, { name: 'AlphaArena.BrokerSelector' });
