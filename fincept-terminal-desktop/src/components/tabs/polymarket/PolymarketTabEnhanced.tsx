// File: src/components/tabs/PolymarketTab.tsx
// Polymarket prediction markets integration - Fincept Terminal Style

import React, { useState, useEffect, useRef, useCallback } from 'react';
import {
  TrendingUp,
  TrendingDown,
  Search,
  Filter,
  Calendar,
  DollarSign,
  Activity,
  BarChart3,
  Clock,
  Tag,
  MessageSquare,
  ExternalLink,
  RefreshCw,
  Info,
  ChevronDown,
  ChevronUp,
  AlertCircle,
  Trophy,
  Users,
  Percent,
  X
} from 'lucide-react';
import polymarketService, {
  PolymarketMarket,
  PolymarketEvent,
  PolymarketTag,
  PolymarketSport,
  PolymarketTrade
} from '@/services/polymarket/polymarketService';
import polymarketServiceEnhanced, {
  HistoricalPrice,
  PricePoint,
  WSMarketUpdate,
  PolymarketOrderBookEnriched,
  PolymarketTopHolders,
  PolymarketOpenInterest,
} from '@/services/polymarket/polymarketServiceEnhanced';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  Legend
} from 'recharts';

const PolymarketTabEnhanced: React.FC = () => {
  // State management
  const [activeView, setActiveView] = useState<'markets' | 'events' | 'sports' | 'portfolio'>('markets');
  const [markets, setMarkets] = useState<PolymarketMarket[]>([]);
  const [events, setEvents] = useState<PolymarketEvent[]>([]);
  const [tags, setTags] = useState<PolymarketTag[]>([]);
  const [sports, setSports] = useState<PolymarketSport[]>([]);
  const [selectedMarket, setSelectedMarket] = useState<PolymarketMarket | null>(null);
  const [selectedEvent, setSelectedEvent] = useState<PolymarketEvent | null>(null);
  const [selectedSport, setSelectedSport] = useState<PolymarketSport | null>(null);
  const [recentTrades, setRecentTrades] = useState<PolymarketTrade[]>([]);
  const [priceHistory, setPriceHistory] = useState<PricePoint[]>([]);
  const [noPriceHistory, setNoPriceHistory] = useState<PricePoint[]>([]);
  const [chartInterval, setChartInterval] = useState<string>('1d');
  const [priceChartLoading, setPriceChartLoading] = useState(false);
  const [priceChartError, setPriceChartError] = useState<string | null>(null);
  // Trade calculator state
  const [tradeAmount, setTradeAmount] = useState<string>('');
  const [selectedOutcomeIndex, setSelectedOutcomeIndex] = useState<number>(0);
  // Order book state (YES + NO token books)
  const [yesOrderBook, setYesOrderBook] = useState<PolymarketOrderBookEnriched | null>(null);
  const [noOrderBook, setNoOrderBook] = useState<PolymarketOrderBookEnriched | null>(null);
  const [orderBookLoading, setOrderBookLoading] = useState(false);
  const [orderBookError, setOrderBookError] = useState<string | null>(null);
  const [activeOrderBookSide, setActiveOrderBookSide] = useState<'yes' | 'no'>('yes');
  // Top holders state
  const [topHolders, setTopHolders] = useState<PolymarketTopHolders | null>(null);
  const [holdersLoading, setHoldersLoading] = useState(false);
  const [holdersError, setHoldersError] = useState<string | null>(null);
  // Open interest
  const [openInterest, setOpenInterest] = useState<PolymarketOpenInterest | null>(null);
  // Live outcome prices from WebSocket — keyed by tokenId
  const [livePrices, setLivePrices] = useState<Record<string, number>>({});
  const [wsConnected, setWsConnected] = useState(false);
  // Ref to active market WebSocket so we can close it on unmount / market change
  const marketWsRef = useRef<WebSocket | null>(null);

  // Filters and search
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedTag, setSelectedTag] = useState<string>('');
  const [showClosed, setShowClosed] = useState(false);
  const [sortBy, setSortBy] = useState<'volume' | 'liquidity' | 'recent'>('volume');

  // Loading states
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Pagination
  const [currentPage, setCurrentPage] = useState(0);
  const pageSize = 20;

  // Load initial data
  useEffect(() => {
    loadInitialData();
  }, []);

  // Load markets when filters change
  useEffect(() => {
    if (activeView === 'markets') {
      loadMarkets();
    } else if (activeView === 'events') {
      loadEvents();
    } else if (activeView === 'sports') {
      loadSports();
    }
  }, [activeView, selectedTag, showClosed, currentPage, sortBy]);

  // Load markets when a sport is selected
  useEffect(() => {
    if (selectedSport) {
      loadSportMarkets(selectedSport);
    }
  }, [selectedSport, showClosed, currentPage, sortBy]);

  // Connect market WebSocket when a market is selected; disconnect on close
  useEffect(() => {
    // Close any existing WebSocket first
    if (marketWsRef.current) {
      marketWsRef.current.close();
      marketWsRef.current = null;
      setWsConnected(false);
    }

    if (!selectedMarket) return;

    // Parse token IDs from the selected market
    let tokenIds: string[] = [];
    try {
      const raw = selectedMarket.clobTokenIds;
      if (typeof raw === 'string') {
        tokenIds = JSON.parse(raw);
      } else if (Array.isArray(raw)) {
        tokenIds = raw as string[];
      }
    } catch { /* ignore parse errors */ }

    if (tokenIds.length === 0) return;

    const yesTokenId = tokenIds[0];
    const noTokenId = tokenIds[1];

    try {
      const ws = polymarketServiceEnhanced.connectMarketWebSocket(
        tokenIds,
        (update: WSMarketUpdate) => {
          const assetId: string | undefined = update.asset_id;
          const eventType = update.event_type;

          // --- Live order book snapshot (flat fields: bids, asks, tick_size, last_trade_price) ---
          if (eventType === 'book') {
            const bids: Array<{ price: string; size: string }> = Array.isArray(update.bids) ? update.bids : [];
            const asks: Array<{ price: string; size: string }> = Array.isArray(update.asks) ? update.asks : [];
            // Sort bids descending by price, asks ascending
            bids.sort((a, b) => parseFloat(b.price) - parseFloat(a.price));
            asks.sort((a, b) => parseFloat(a.price) - parseFloat(b.price));
            const bookSnapshot = {
              market: update.market,
              asset_id: assetId ?? '',
              bids,
              asks,
              timestamp: Number(update.timestamp),
              tick_size: update.tick_size,
              last_trade_price: update.last_trade_price,
            };
            if (assetId === yesTokenId) {
              setYesOrderBook(prev => ({ ...prev, ...bookSnapshot }));
            } else if (assetId === noTokenId) {
              setNoOrderBook(prev => ({ ...prev, ...bookSnapshot }));
            }
            return;
          }

          // --- price_change: flat price + side fields ---
          if (eventType === 'price_change') {
            const p = parseFloat(update.price ?? '');
            if (assetId && !isNaN(p)) {
              setLivePrices(prev => ({ ...prev, [assetId]: p }));
            }
            return;
          }

          // --- last_trade_price: flat price field ---
          if (eventType === 'last_trade_price') {
            const ltp = parseFloat(update.last_trade_price ?? update.price ?? '');
            if (!isNaN(ltp) && assetId) {
              setLivePrices(prev => ({ ...prev, [assetId]: ltp }));
              const ltpStr = ltp.toFixed(4);
              if (assetId === yesTokenId) {
                setYesOrderBook(prev => prev ? { ...prev, last_trade_price: ltpStr } : prev);
              } else if (assetId === noTokenId) {
                setNoOrderBook(prev => prev ? { ...prev, last_trade_price: ltpStr } : prev);
              }
            }
            return;
          }

          // --- best_bid_ask: flat best_bid, best_ask fields ---
          if (eventType === 'best_bid_ask') {
            const bid = parseFloat(update.best_bid ?? '');
            const ask = parseFloat(update.best_ask ?? '');
            if (!isNaN(bid) && !isNaN(ask) && assetId) {
              const mid = (bid + ask) / 2;
              setLivePrices(prev => ({ ...prev, [assetId]: mid }));
              const midStr = mid.toFixed(4);
              if (assetId === yesTokenId) {
                setYesOrderBook(prev => prev ? { ...prev, last_trade_price: midStr } : prev);
              } else if (assetId === noTokenId) {
                setNoOrderBook(prev => prev ? { ...prev, last_trade_price: midStr } : prev);
              }
            }
            return;
          }
        }
      );
      marketWsRef.current = ws;

      // Track connected state using onopen/onclose on the returned ws
      const origOnOpen = ws.onopen;
      ws.onopen = (e) => {
        setWsConnected(true);
        if (origOnOpen) (origOnOpen as any).call(ws, e);
      };
      const origOnClose = ws.onclose;
      ws.onclose = (e) => {
        setWsConnected(false);
        if (origOnClose) (origOnClose as any).call(ws, e);
      };
    } catch (err) {
      console.warn('Market WebSocket failed to connect:', err);
    }

    return () => {
      if (marketWsRef.current) {
        marketWsRef.current.close();
        marketWsRef.current = null;
        setWsConnected(false);
      }
    };
  }, [selectedMarket?.id]);

  // Cleanup WebSocket on component unmount
  useEffect(() => {
    return () => {
      polymarketServiceEnhanced.disconnectWebSocket('market');
    };
  }, []);

  const loadInitialData = async () => {
    try {
      setLoading(true);
      const [tagsData, sportsData] = await Promise.all([
        polymarketService.getTags(),
        polymarketService.getSports()
      ]);
      setTags(tagsData || []);
      setSports(sportsData || []);
    } catch (err) {
      console.error('Error loading initial data:', err);
      setError('Failed to load initial data');
    } finally {
      setLoading(false);
    }
  };

  const loadMarkets = async () => {
    try {
      setLoading(true);
      setError(null);

      const params = {
        limit: pageSize,
        offset: currentPage * pageSize,
        closed: showClosed ? undefined : false,
        active: showClosed ? undefined : true,
        tag_id: selectedTag || undefined,
        order: sortBy === 'volume' ? 'volume' : sortBy === 'liquidity' ? 'liquidity' : 'id',
        ascending: false
      };

      const data = await polymarketService.getMarkets(params);
      setMarkets(data || []);
    } catch (err) {
      console.error('Error loading markets:', err);
      setError('Failed to load markets');
      setMarkets([]);
    } finally {
      setLoading(false);
    }
  };

  const loadEvents = async () => {
    try {
      setLoading(true);
      setError(null);

      const params = {
        limit: pageSize,
        offset: currentPage * pageSize,
        closed: showClosed ? undefined : false,
        active: showClosed ? undefined : true,
        tag_id: selectedTag || undefined,
        order: 'id',
        ascending: false
      };

      const data = await polymarketService.getEvents(params);
      setEvents(data || []);
    } catch (err) {
      console.error('Error loading events:', err);
      setError('Failed to load events');
      setEvents([]);
    } finally {
      setLoading(false);
    }
  };

  const loadSports = async () => {
    try {
      setLoading(true);
      setError(null);
      const data = await polymarketService.getSports();
      setSports(data || []);
    } catch (err) {
      console.error('Error loading sports:', err);
      setError('Failed to load sports');
      setSports([]);
    } finally {
      setLoading(false);
    }
  };

  const loadMarketDetails = async (market: PolymarketMarket, interval: string = chartInterval) => {
    const isNewMarket = market.id !== selectedMarket?.id;

    setSelectedMarket(market);
    setPriceHistory([]);
    setNoPriceHistory([]);
    setPriceChartLoading(true);
    setPriceChartError(null);
    setTradeAmount('');
    setSelectedOutcomeIndex(0);

    // Reset secondary panels only when switching to a different market
    if (isNewMarket) {
      setRecentTrades([]);
      setYesOrderBook(null);
      setNoOrderBook(null);
      setOrderBookLoading(true);
      setOrderBookError(null);
      setActiveOrderBookSide('yes');
      setTopHolders(null);
      setHoldersLoading(true);
      setHoldersError(null);
      setOpenInterest(null);
    }

    // Parse token IDs — Gamma API returns clobTokenIds as a JSON string
    let tokenIds: string[] = [];
    try {
      if (market.clobTokenIds) {
        if (typeof market.clobTokenIds === 'string') {
          tokenIds = JSON.parse(market.clobTokenIds);
        } else if (Array.isArray(market.clobTokenIds)) {
          tokenIds = market.clobTokenIds;
        }
      }
    } catch (err) {
      console.error('Error parsing clobTokenIds:', err);
    }

    const yesTokenId = tokenIds[0];
    const noTokenId = tokenIds[1]; // binary markets always have YES[0] and NO[1]

    // Fire non-blocking parallel fetches for order book, top holders, open interest
    // Only on market change (not on interval change)
    if (isNewMarket) {
      // Order book — both YES and NO tokens
      if (yesTokenId || noTokenId) {
        Promise.all([
          yesTokenId ? polymarketServiceEnhanced.getOrderBookEnriched(yesTokenId).catch(() => null) : Promise.resolve(null),
          noTokenId  ? polymarketServiceEnhanced.getOrderBookEnriched(noTokenId).catch(() => null)  : Promise.resolve(null),
        ]).then(([yesBook, noBook]) => {
          setYesOrderBook(yesBook);
          setNoOrderBook(noBook);
          if (!yesBook && !noBook) setOrderBookError('Order book unavailable');
        }).finally(() => setOrderBookLoading(false));
      } else {
        setOrderBookLoading(false);
        setOrderBookError('No token IDs available');
      }

      // Top holders + open interest require conditionId
      const condId = market.conditionId ?? '';
      if (condId) {
        polymarketServiceEnhanced.getTopHolders(condId, 20)
          .then(data => setTopHolders(data))
          .catch(() => setHoldersError('Failed to load holders'))
          .finally(() => setHoldersLoading(false));

        polymarketServiceEnhanced.getOpenInterest([condId])
          .then(data => setOpenInterest(data.find(d => d.market === condId) ?? data[0] ?? null))
          .catch((e) => console.warn('[Polymarket] Failed to load open interest:', e));
      } else {
        setHoldersLoading(false);
        setHoldersError('conditionId not available for this market');
      }
    }

    // Load trades for YES token
    if (yesTokenId) {
      try {
        const result = await polymarketService.getTrades({ token_id: yesTokenId, limit: 50 });
        const trades = Array.isArray(result) ? result : ((result as any).data ?? []);
        setRecentTrades(trades);
      } catch (err) {
        console.error('Error loading trades:', err);
      }
    }

    // Load YES + NO price histories in parallel
    if (yesTokenId) {
      try {
        // Fidelity = candle size in minutes. Coarser fidelity for longer intervals
        // so we get a reasonable number of data points (not too few, not thousands)
        const fidelityMap: Record<string, number> = {
          '1h': 1,    // 1-min candles → ~60 pts
          '6h': 5,    // 5-min candles → ~72 pts
          '1d': 60,   // 1-hour candles → ~24 pts
          '1w': 360,  // 6-hour candles → ~28 pts
          '1m': 1440, // 1-day candles → ~30 pts
          'max': 1440,
          'all': 1440,
        };
        const fidelity = fidelityMap[interval] ?? 60;

        const fetches: Promise<any>[] = [
          polymarketServiceEnhanced.getPriceHistory({ token_id: yesTokenId, interval, fidelity }),
        ];
        if (noTokenId) {
          fetches.push(polymarketServiceEnhanced.getPriceHistory({ token_id: noTokenId, interval, fidelity }));
        }

        const [yesHistory, noHistory] = await Promise.all(fetches);
        const yesPrices = yesHistory?.prices ?? [];
        const noPrices = noHistory?.prices ?? [];

        if (yesPrices.length === 0) {
          setPriceChartError('No price history available for this market');
        } else {
          setPriceHistory(yesPrices);
          if (noPrices.length > 0) setNoPriceHistory(noPrices);
        }
      } catch (err: any) {
        console.error('Error loading price history:', err);
        setPriceChartError('Failed to load price history');
      }
    } else {
      setPriceChartError('No token ID available for this market');
    }

    setPriceChartLoading(false);
  };

  const loadSportMarkets = async (sport: PolymarketSport) => {
    try {
      setLoading(true);
      setError(null);

      // Extract first tag ID from comma-separated tags
      const firstTagId = sport.tags?.split(',')[0] || '';

      const params = {
        limit: pageSize,
        offset: currentPage * pageSize,
        closed: showClosed ? undefined : false,
        active: showClosed ? undefined : true,
        tag_id: firstTagId,
        order: sortBy === 'volume' ? 'volume' : sortBy === 'liquidity' ? 'liquidity' : 'id',
        ascending: false
      };

      const data = await polymarketService.getMarkets(params);
      setMarkets(data || []);
    } catch (err) {
      console.error('Error loading sport markets:', err);
      setError('Failed to load markets');
      setMarkets([]);
    } finally {
      setLoading(false);
    }
  };

  const handleSearch = async () => {
    if (!searchQuery.trim()) {
      loadMarkets();
      return;
    }

    try {
      setLoading(true);
      const results = await polymarketService.search(searchQuery, activeView === 'markets' ? 'markets' : 'events');

      if (activeView === 'markets') {
        setMarkets(results.markets || []);
      } else {
        setEvents(results.events || []);
      }
    } catch (err) {
      console.error('Error searching:', err);
      setError('Search failed');
    } finally {
      setLoading(false);
    }
  };

  const handleRefresh = () => {
    setCurrentPage(0);
    if (activeView === 'markets') {
      loadMarkets();
    } else if (activeView === 'events') {
      loadEvents();
    } else if (activeView === 'sports') {
      loadSports();
    }
  };

  // Outcome color palette — indexed by position; first/leading outcome is green, rest red/orange
  const OUTCOME_COLORS = ['#00D66F', '#FF3B3B', '#FF8800', '#4F8EF7', '#A855F7'];

  // Format dollar volume: 1234567 → "$1.23M", 12345 → "$12.35K"
  const formatDollarVolume = (val: any): string => {
    const n = typeof val === 'number' ? val : parseFloat(val) || 0;
    if (n >= 1_000_000) return `$${(n / 1_000_000).toFixed(2)}M`;
    if (n >= 1_000) return `$${(n / 1_000).toFixed(2)}K`;
    return `$${n.toFixed(2)}`;
  };

  // Render helpers
  const renderMarketCard = (market: PolymarketMarket) => {
    const status = polymarketService.getMarketStatus(market);
    const isTradeable = polymarketService.isMarketTradeable(market);

    // Parse outcome names — Gamma returns JSON string e.g. '["Yes","No"]'
    let outcomeNames: string[] = [];
    try {
      let raw: any = market.outcomes;
      if (typeof raw === 'string') raw = JSON.parse(raw);
      if (Array.isArray(raw)) outcomeNames = raw.map(String);
    } catch { /* ignore */ }

    // Parse outcome prices — Gamma returns JSON string e.g. '["0.97","0.03"]'
    let outcomePrices: number[] = [];
    try {
      let raw: any = market.outcomePrices;
      if (typeof raw === 'string') raw = JSON.parse(raw);
      if (Array.isArray(raw)) outcomePrices = raw.map((p: any) => parseFloat(p) || 0);
    } catch { /* ignore */ }

    // Parse token IDs so we can check live prices
    let tokenIds: string[] = [];
    try {
      const raw = market.clobTokenIds;
      if (typeof raw === 'string') tokenIds = JSON.parse(raw);
      else if (Array.isArray(raw)) tokenIds = raw as string[];
    } catch { /* ignore */ }

    // Determine leading outcome index (highest price)
    const maxPrice = outcomePrices.length > 0 ? Math.max(...outcomePrices) : 0;

    return (
      <div
        key={market.id}
        onClick={() => loadMarketDetails(market)}
        style={{
          backgroundColor: '#000000',
          border: '1px solid #333333',
          padding: '12px',
          marginBottom: '1px',
          cursor: 'pointer',
          fontFamily: 'IBM Plex Mono, monospace',
          fontSize: '11px',
          transition: 'background-color 0.1s',
        }}
        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#0A0A0A'}
        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#000000'}
      >
        <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', marginBottom: '8px' }}>
          <div style={{ flex: 1 }}>
            <div style={{ color: '#FFFFFF', fontWeight: 'bold', marginBottom: '4px', fontSize: '12px' }}>
              {market.question}
            </div>
            {(market.featured || market.new) && (
              <div style={{ display: 'flex', gap: '4px', marginBottom: '4px' }}>
                {market.featured && (
                  <span style={{ padding: '1px 5px', backgroundColor: '#FF880020', color: '#FF8800', fontSize: '8px', fontWeight: 'bold', border: '1px solid #FF880066', letterSpacing: '0.05em' }}>FEATURED</span>
                )}
                {market.new && (
                  <span style={{ padding: '1px 5px', backgroundColor: '#4F8EF720', color: '#4F8EF7', fontSize: '8px', fontWeight: 'bold', border: '1px solid #4F8EF766', letterSpacing: '0.05em' }}>NEW</span>
                )}
              </div>
            )}
            {market.description && (
              <div style={{ color: '#787878', fontSize: '10px', marginBottom: '6px' }}>
                {market.description.length > 120 ? market.description.substring(0, 120) + '...' : market.description}
              </div>
            )}
          </div>
          {market.image && (
            <img
              src={market.image}
              alt=""
              style={{ width: '40px', height: '40px', borderRadius: '2px', marginLeft: '12px', objectFit: 'cover' }}
            />
          )}
        </div>

        {/* Outcome prices — dynamic names and colors */}
        {outcomeNames.length > 0 ? (
          <div style={{
            display: 'grid',
            gridTemplateColumns: `repeat(${Math.min(outcomeNames.length, 4)}, 1fr)`,
            gap: '8px',
            marginBottom: '8px'
          }}>
            {outcomeNames.map((name, i) => {
              // Use live price if available, otherwise REST price
              const tokenId = tokenIds[i];
              const livePrice = tokenId ? livePrices[tokenId] : undefined;
              const price = livePrice ?? outcomePrices[i] ?? 0;
              const isLeading = price === maxPrice && maxPrice > 0;
              const color = isLeading ? OUTCOME_COLORS[0] : OUTCOME_COLORS[Math.min(i, OUTCOME_COLORS.length - 1) > 0 ? Math.min(i, OUTCOME_COLORS.length - 1) : 1];
              return (
                <div key={i} style={{
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333333',
                  padding: '8px'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span style={{ color: '#787878', fontSize: '9px', fontWeight: 'bold', textTransform: 'uppercase' }}>{name}</span>
                    <span style={{ color, fontSize: '13px', fontWeight: 'bold' }}>
                      {(price * 100).toFixed(1)}%
                    </span>
                  </div>
                  <div style={{ color: '#787878', fontSize: '9px', marginTop: '2px' }}>
                    ${price.toFixed(4)}{livePrice !== undefined && <span style={{ color: '#FF8800', marginLeft: '4px' }}>●</span>}
                  </div>
                </div>
              );
            })}
          </div>
        ) : null}

        {/* Market stats */}
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', fontSize: '10px', color: '#787878' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <div>
              <div>VOL: {formatDollarVolume(market.volumeNum ?? market.volume ?? 0)}</div>
              {(() => {
                // 24h volume lives on events[0].series[0].volume24hr, not directly on market
                const v24h: number | null = (market as any).events?.[0]?.series?.[0]?.volume24hr ?? null;
                return v24h != null ? <div style={{ color: '#555', fontSize: '9px' }}>24H: {formatDollarVolume(v24h)}</div> : null;
              })()}
            </div>
            {(market.liquidityNum ?? market.liquidity) && (
              <div>LIQ: {formatDollarVolume(market.liquidityNum ?? market.liquidity ?? 0)}</div>
            )}
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            {market.oneDayPriceChange != null && Math.abs(market.oneDayPriceChange) > 0.001 && (
              <span style={{
                padding: '2px 6px',
                backgroundColor: market.oneDayPriceChange >= 0 ? '#003300' : '#330000',
                color: market.oneDayPriceChange >= 0 ? '#00D66F' : '#FF3B3B',
                fontSize: '9px',
                fontWeight: 'bold',
                border: `1px solid ${market.oneDayPriceChange >= 0 ? '#00D66F' : '#FF3B3B'}`,
              }}>
                {market.oneDayPriceChange >= 0 ? '▲' : '▼'} {Math.abs(market.oneDayPriceChange * 100).toFixed(1)}%
              </span>
            )}
            {isTradeable && (
              <span style={{
                padding: '2px 6px',
                backgroundColor: '#003300',
                color: '#00D66F',
                fontSize: '9px',
                fontWeight: 'bold',
                border: '1px solid #00D66F'
              }}>
                LIVE
              </span>
            )}
            {status === 'closed' && (
              <span style={{
                padding: '2px 6px',
                backgroundColor: '#1A1A1A',
                color: '#787878',
                fontSize: '9px',
                fontWeight: 'bold',
                border: '1px solid #333333'
              }}>
                CLOSED
              </span>
            )}
          </div>
        </div>

        {/* Tags */}
        {market.tags && market.tags.length > 0 && (
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px', marginTop: '8px' }}>
            {market.tags.slice(0, 3).map((tag) => (
              <span
                key={tag.id}
                style={{
                  padding: '2px 6px',
                  backgroundColor: '#1A1A1A',
                  color: '#787878',
                  fontSize: '9px',
                  border: '1px solid #333333'
                }}
              >
                {tag.label}
              </span>
            ))}
          </div>
        )}
      </div>
    );
  };

  const renderEventCard = (event: PolymarketEvent) => {
    return (
      <div
        key={event.id}
        onClick={() => setSelectedEvent(event)}
        style={{
          backgroundColor: '#000000',
          border: '1px solid #333333',
          padding: '12px',
          marginBottom: '1px',
          cursor: 'pointer',
          fontFamily: 'IBM Plex Mono, monospace',
          fontSize: '11px',
          transition: 'background-color 0.1s',
        }}
        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#0A0A0A'}
        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#000000'}
      >
        <div style={{ display: 'flex', alignItems: 'flex-start', gap: '12px', marginBottom: '8px' }}>
          {event.image && (
            <img
              src={event.image}
              alt=""
              style={{ width: '48px', height: '48px', borderRadius: '2px', objectFit: 'cover' }}
            />
          )}
          <div style={{ flex: 1 }}>
            <div style={{ color: '#FFFFFF', fontWeight: 'bold', marginBottom: '4px', fontSize: '12px' }}>
              {event.title}
            </div>
            {event.description && (
              <div style={{ color: '#787878', fontSize: '10px' }}>
                {event.description.length > 100 ? event.description.substring(0, 100) + '...' : event.description}
              </div>
            )}
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', fontSize: '10px', color: '#787878', marginBottom: '6px' }}>
          <div>
            END: {event.endDate ? new Date(event.endDate).toLocaleDateString() : 'N/A'}
          </div>
          <div>
            MARKETS: {event.markets?.length || 0}
          </div>
        </div>

        {event.volume && (
          <div style={{ fontSize: '10px', color: '#787878' }}>
            VOL: <span style={{ color: '#FFFFFF', fontWeight: 'bold' }}>
              {polymarketService.formatVolume(event.volume)}
            </span>
          </div>
        )}
      </div>
    );
  };

  const renderMarketDetails = () => {
    if (!selectedMarket) return null;

    // Parse outcomePrices — Gamma returns a JSON string e.g. '["0.0235","0.9765"]'
    let parsedPrices: number[] = [];
    try {
      let raw: any = selectedMarket.outcomePrices;
      if (typeof raw === 'string') raw = JSON.parse(raw);
      if (Array.isArray(raw)) parsedPrices = raw.map((p: any) => parseFloat(p) || 0);
    } catch { /* ignore */ }

    // Parse outcomes — Gamma returns a JSON string e.g. '["Yes","No"]'
    let outcomeNames: string[] = [];
    try {
      let raw: any = selectedMarket.outcomes;
      if (typeof raw === 'string') raw = JSON.parse(raw);
      if (Array.isArray(raw)) outcomeNames = raw.map(String);
    } catch { /* ignore */ }

    // Parse token IDs for live price lookup
    let detailTokenIds: string[] = [];
    try {
      const raw = selectedMarket.clobTokenIds;
      if (typeof raw === 'string') detailTokenIds = JSON.parse(raw);
      else if (Array.isArray(raw)) detailTokenIds = raw as string[];
    } catch { /* ignore */ }

    // Build per-outcome display items, merging live WS prices where available
    const outcomeItems = outcomeNames.map((name, i) => {
      const tokenId = detailTokenIds[i];
      const livePrice = tokenId ? livePrices[tokenId] : undefined;
      return {
        name,
        price: livePrice ?? parsedPrices[i] ?? 0,
        isLive: livePrice !== undefined,
      };
    });

    return (
      <div style={{
        position: 'fixed',
        inset: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.9)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 1000,
        padding: '20px',
        fontFamily: 'IBM Plex Mono, monospace'
      }}>
        <div style={{
          backgroundColor: '#000000',
          border: '2px solid #FF8800',
          maxWidth: '1200px',
          width: '100%',
          maxHeight: '90vh',
          overflow: 'auto',
        }}>
          {/* Header */}
          <div style={{
            position: 'sticky',
            top: 0,
            backgroundColor: '#000000',
            borderBottom: '2px solid #FF8800',
            padding: '16px',
            display: 'flex',
            alignItems: 'flex-start',
            justifyContent: 'space-between',
            zIndex: 10
          }}>
            <div style={{ flex: 1 }}>
              <div style={{ color: '#FF8800', fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                {selectedMarket.question}
              </div>
              {selectedMarket.description && (
                <div style={{ color: '#787878', fontSize: '11px' }}>
                  {selectedMarket.description}
                </div>
              )}
            </div>
            <button
              onClick={() => setSelectedMarket(null)}
              style={{
                marginLeft: '16px',
                padding: '4px 8px',
                backgroundColor: 'transparent',
                border: '1px solid #787878',
                color: '#787878',
                cursor: 'pointer',
                fontFamily: 'inherit',
                fontSize: '11px'
              }}
            >
              <X size={16} />
            </button>
          </div>

          <div style={{ padding: '16px' }}>
            {/* Outcome Price Boxes — one per outcome, dynamic names + colors */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: `repeat(${Math.min(outcomeItems.length, 4)}, 1fr)`,
              gap: '8px',
              marginBottom: '12px'
            }}>
              {outcomeItems.map((item, i) => {
                // Leading outcome uses first palette color (green); rest use their indexed color
                const detailMaxPrice = Math.max(...outcomeItems.map(o => o.price));
                const isLeading = item.price === detailMaxPrice && detailMaxPrice > 0;
                const priceColor = isLeading ? OUTCOME_COLORS[0] : OUTCOME_COLORS[Math.min(i, OUTCOME_COLORS.length - 1) > 0 ? Math.min(i, OUTCOME_COLORS.length - 1) : 1];
                return (
                  <div key={i} style={{
                    backgroundColor: '#0A0A0A',
                    border: `1px solid ${priceColor}33`,
                    padding: '14px 16px',
                  }}>
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
                      <span style={{ color: '#787878', fontSize: '10px', fontWeight: 'bold', textTransform: 'uppercase' }}>
                        {item.name}
                      </span>
                      {item.isLive && (
                        <span style={{ fontSize: '8px', color: '#FF8800', display: 'flex', alignItems: 'center', gap: '2px' }}>
                          <span style={{ display: 'inline-block', width: '5px', height: '5px', borderRadius: '50%', backgroundColor: '#FF8800' }} />
                          LIVE
                        </span>
                      )}
                    </div>
                    <div style={{ color: priceColor, fontSize: '28px', fontWeight: 'bold', marginBottom: '4px', lineHeight: 1 }}>
                      {(item.price * 100).toFixed(1)}%
                    </div>
                    <div style={{ color: '#555', fontSize: '11px' }}>
                      ${item.price.toFixed(4)} per share
                    </div>
                  </div>
                );
              })}
            </div>

            {/* Trade Calculator */}
            {outcomeItems.length > 0 && (() => {
              const selectedItem = outcomeItems[selectedOutcomeIndex] ?? outcomeItems[0];
              const price = selectedItem?.price ?? 0;
              const amount = parseFloat(tradeAmount) || 0;
              // Shares = amount / price (each share costs `price` USDC)
              const shares = price > 0 ? amount / price : 0;
              // Potential profit if outcome wins: shares pay out $1 each, minus cost
              const potentialReturn = shares; // gross payout in USDC
              const potentialProfit = potentialReturn - amount;
              const maxReturn = price > 0 ? amount / price : 0;

              return (
                <div style={{ border: '1px solid #333333', backgroundColor: '#000000', marginBottom: '12px' }}>
                  {/* Header */}
                  <div style={{ padding: '8px 12px', borderBottom: '1px solid #1e1e1e', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                    <span style={{ fontSize: '10px', color: '#787878', fontFamily: 'IBM Plex Mono, monospace' }}>TRADE CALCULATOR</span>
                    <span style={{ fontSize: '9px', color: '#555', fontFamily: 'IBM Plex Mono, monospace' }}>READ-ONLY · CONNECT WALLET ON POLYMARKET TO TRADE</span>
                  </div>

                  <div style={{ padding: '12px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                    {/* Left: outcome selector + amount input */}
                    <div>
                      {/* Outcome buttons */}
                      <div style={{ fontSize: '9px', color: '#555', marginBottom: '6px', fontFamily: 'IBM Plex Mono, monospace' }}>SELECT OUTCOME</div>
                      <div style={{ display: 'flex', gap: '4px', marginBottom: '12px', flexWrap: 'wrap' }}>
                        {outcomeItems.map((item, i) => {
                          const isSelected = selectedOutcomeIndex === i;
                          const detailMaxPrice = Math.max(...outcomeItems.map(o => o.price));
                          const isLeading = item.price === detailMaxPrice && detailMaxPrice > 0;
                          const btnColor = isLeading ? OUTCOME_COLORS[0] : OUTCOME_COLORS[Math.min(i, OUTCOME_COLORS.length - 1) > 0 ? Math.min(i, OUTCOME_COLORS.length - 1) : 1];
                          return (
                            <button
                              key={i}
                              onClick={() => setSelectedOutcomeIndex(i)}
                              style={{
                                padding: '6px 14px',
                                fontSize: '10px',
                                fontFamily: 'IBM Plex Mono, monospace',
                                fontWeight: 'bold',
                                background: isSelected ? btnColor : 'transparent',
                                color: isSelected ? '#000' : btnColor,
                                border: `1px solid ${btnColor}`,
                                cursor: 'pointer',
                                transition: 'all 0.1s',
                              }}
                            >
                              {item.name.toUpperCase()} · {(item.price * 100).toFixed(1)}%
                            </button>
                          );
                        })}
                      </div>

                      {/* Amount input */}
                      <div style={{ fontSize: '9px', color: '#555', marginBottom: '6px', fontFamily: 'IBM Plex Mono, monospace' }}>AMOUNT (USDC)</div>
                      <div style={{ display: 'flex', alignItems: 'center', backgroundColor: '#0A0A0A', border: '1px solid #333', marginBottom: '8px' }}>
                        <span style={{ padding: '0 10px', color: '#787878', fontSize: '13px', fontWeight: 'bold' }}>$</span>
                        <input
                          type="number"
                          min="0"
                          step="1"
                          placeholder="0.00"
                          value={tradeAmount}
                          onChange={(e) => setTradeAmount(e.target.value)}
                          style={{
                            flex: 1,
                            padding: '10px 8px',
                            backgroundColor: 'transparent',
                            border: 'none',
                            outline: 'none',
                            color: '#FFFFFF',
                            fontSize: '14px',
                            fontWeight: 'bold',
                            fontFamily: 'IBM Plex Mono, monospace',
                          }}
                        />
                        <span style={{ padding: '0 10px', color: '#555', fontSize: '10px' }}>USDC</span>
                      </div>
                      {/* Quick amount buttons */}
                      <div style={{ display: 'flex', gap: '4px' }}>
                        {[10, 50, 100, 500].map(v => (
                          <button
                            key={v}
                            onClick={() => setTradeAmount(String(v))}
                            style={{
                              flex: 1,
                              padding: '4px 0',
                              fontSize: '9px',
                              fontFamily: 'IBM Plex Mono, monospace',
                              background: 'transparent',
                              color: '#787878',
                              border: '1px solid #333',
                              cursor: 'pointer',
                            }}
                          >
                            ${v}
                          </button>
                        ))}
                      </div>
                    </div>

                    {/* Right: payout breakdown */}
                    <div style={{ display: 'flex', flexDirection: 'column', justifyContent: 'space-between' }}>
                      <div>
                        <div style={{ fontSize: '9px', color: '#555', marginBottom: '10px', fontFamily: 'IBM Plex Mono, monospace' }}>ORDER SUMMARY</div>

                        {[
                          { label: 'OUTCOME', value: selectedItem?.name?.toUpperCase() ?? '—' },
                          { label: 'AVG PRICE', value: price > 0 ? `$${price.toFixed(4)}` : '—' },
                          { label: 'SHARES', value: amount > 0 && price > 0 ? shares.toFixed(2) : amount === 0 ? 'ENTER AMOUNT' : '—' },
                          { label: 'MAX RETURN', value: amount > 0 && price > 0 ? `$${maxReturn.toFixed(2)}` : amount === 0 ? 'ENTER AMOUNT' : '—' },
                          { label: 'POTENTIAL PROFIT', value: amount > 0 && price > 0 ? `$${potentialProfit.toFixed(2)}` : amount === 0 ? 'ENTER AMOUNT' : '—' },
                          { label: 'ROI', value: amount > 0 && price > 0 ? `${((potentialProfit / amount) * 100).toFixed(1)}%` : amount === 0 ? 'ENTER AMOUNT' : '—' },
                        ].map(row => (
                          <div key={row.label} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '5px 0', borderBottom: '1px solid #111' }}>
                            <span style={{ fontSize: '9px', color: '#555', fontFamily: 'IBM Plex Mono, monospace' }}>{row.label}</span>
                            <span style={{
                              fontSize: '11px',
                              fontWeight: 'bold',
                              fontFamily: 'IBM Plex Mono, monospace',
                              color: row.value === 'ENTER AMOUNT' ? '#333'
                                : row.label === 'POTENTIAL PROFIT' ? (potentialProfit > 0 ? '#00D66F' : '#787878')
                                : row.label === 'ROI' ? (potentialProfit > 0 ? '#00D66F' : '#787878')
                                : row.label === 'OUTCOME' ? (OUTCOME_COLORS[selectedOutcomeIndex] ?? '#FFF')
                                : '#FFFFFF'
                            }}>
                              {row.value}
                            </span>
                          </div>
                        ))}
                      </div>

                      {/* Total you'll receive box */}
                      {amount > 0 && price > 0 && (
                        <div style={{ marginTop: '10px', padding: '10px 12px', backgroundColor: '#0A0A0A', border: '1px solid #333' }}>
                          <div style={{ fontSize: '9px', color: '#555', marginBottom: '4px', fontFamily: 'IBM Plex Mono, monospace' }}>YOU'LL RECEIVE IF {selectedItem?.name?.toUpperCase()} WINS</div>
                          <div style={{ fontSize: '22px', fontWeight: 'bold', color: '#00D66F', fontFamily: 'IBM Plex Mono, monospace' }}>
                            ${maxReturn.toFixed(2)}
                          </div>
                          <div style={{ fontSize: '9px', color: '#555', marginTop: '2px', fontFamily: 'IBM Plex Mono, monospace' }}>
                            {shares.toFixed(2)} shares × $1.00 payout
                          </div>
                        </div>
                      )}
                    </div>
                  </div>
                </div>
              );
            })()}

            {/* Chart section */}
            <div style={{ marginBottom: '12px' }}>

              {/* Price Chart */}
              <div style={{ border: '1px solid #333333', backgroundColor: '#000000' }}>
                {/* Interval selector */}
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '6px 10px', borderBottom: '1px solid #1e1e1e' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <span style={{ fontSize: '10px', color: '#787878', fontFamily: 'IBM Plex Mono, monospace' }}>PROBABILITY HISTORY</span>
                    {wsConnected && (
                      <span style={{ fontSize: '9px', color: '#00D66F', fontFamily: 'IBM Plex Mono, monospace', display: 'flex', alignItems: 'center', gap: '3px' }}>
                        <span style={{ display: 'inline-block', width: '6px', height: '6px', borderRadius: '50%', backgroundColor: '#00D66F' }} />
                        LIVE
                      </span>
                    )}
                  </div>
                  <div style={{ display: 'flex', gap: '2px' }}>
                    {(['1d', '1w', '1m', '6h', '1h', 'max'] as const).map(iv => (
                      <button
                        key={iv}
                        onClick={() => {
                          setChartInterval(iv);
                          if (selectedMarket) loadMarketDetails(selectedMarket, iv);
                        }}
                        style={{
                          padding: '2px 7px',
                          fontSize: '10px',
                          fontFamily: 'IBM Plex Mono, monospace',
                          background: chartInterval === iv ? '#FF8800' : 'transparent',
                          color: chartInterval === iv ? '#000' : '#787878',
                          border: '1px solid ' + (chartInterval === iv ? '#FF8800' : '#333'),
                          cursor: 'pointer',
                        }}
                      >
                        {iv.toUpperCase()}
                      </button>
                    ))}
                  </div>
                </div>

                {priceChartLoading ? (
                  <div style={{ height: '220px', display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#787878' }}>
                    <div style={{ textAlign: 'center' }}>
                      <BarChart3 size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
                      <div style={{ fontSize: '11px', fontFamily: 'IBM Plex Mono, monospace' }}>LOADING PRICE HISTORY...</div>
                    </div>
                  </div>
                ) : priceHistory.length > 1 ? (() => {
                  // Build a merged timestamp map so both lines share the same X points
                  const yesMap = new Map(priceHistory.map(pt => [pt.timestamp, pt.price]));
                  const noMap = new Map(noPriceHistory.map(pt => [pt.timestamp, pt.price]));

                  // Use YES timestamps as the base (they always exist)
                  const chartData = priceHistory.map(pt => {
                    const ts = typeof pt.timestamp === 'number' ? pt.timestamp : Number(pt.timestamp) || 0;
                    const ms = ts < 1e12 ? ts * 1000 : ts;
                    const d = new Date(ms);
                    // For 1d use time labels, for longer use date labels
                    const label = chartInterval === '1d'
                      ? d.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false })
                      : d.toLocaleDateString('en-US', { month: 'short', day: 'numeric' });
                    return {
                      label,
                      yes: typeof pt.price === 'number' ? pt.price : parseFloat(pt.price as any) || 0,
                      no: noMap.get(pt.timestamp) ?? null,
                    };
                  });

                  // Y domain: span actual data range with 5% padding
                  const allPrices = chartData.flatMap(d => [d.yes, d.no ?? d.yes]);
                  const minP = Math.min(...allPrices);
                  const maxP = Math.max(...allPrices);
                  const pad = (maxP - minP) * 0.05 || 0.02;
                  const yMin = Math.max(0, minP - pad);
                  const yMax = Math.min(1, maxP + pad);

                  const tickInterval = Math.max(1, Math.floor(chartData.length / 6));

                  // Use dynamic outcome labels; fall back to generic names
                  const yesLabel = outcomeNames[0] ?? 'Outcome 1';
                  const noLabel = outcomeNames[1] ?? 'Outcome 2';
                  // Chart line colors from palette (index 0 = leading/yes, index 1 = no)
                  const yesColor = OUTCOME_COLORS[0];
                  const noColor = OUTCOME_COLORS[1];

                  return (
                    <ResponsiveContainer width="100%" height={220}>
                      <LineChart data={chartData} margin={{ top: 8, right: 12, left: 4, bottom: 4 }}>
                        <CartesianGrid strokeDasharray="2 4" stroke="#1a1a1a" vertical={false} />
                        <XAxis
                          dataKey="label"
                          tick={{ fill: '#555', fontSize: 9, fontFamily: 'IBM Plex Mono, monospace' }}
                          tickLine={false}
                          axisLine={{ stroke: '#222' }}
                          interval={tickInterval}
                        />
                        <YAxis
                          domain={[yMin, yMax]}
                          tickFormatter={v => `${(v * 100).toFixed(0)}%`}
                          tick={{ fill: '#555', fontSize: 9, fontFamily: 'IBM Plex Mono, monospace' }}
                          tickLine={false}
                          axisLine={false}
                          width={38}
                          tickCount={5}
                        />
                        <Tooltip
                          contentStyle={{ backgroundColor: '#111', border: '1px solid #2a2a2a', borderRadius: 2, fontFamily: 'IBM Plex Mono, monospace', fontSize: 10, padding: '6px 10px' }}
                          labelStyle={{ color: '#555', marginBottom: 4 }}
                          formatter={(val: number, name: string) => [
                            `${(val * 100).toFixed(1)}%`,
                            name === 'yes' ? yesLabel : name === 'no' ? noLabel : name
                          ]}
                        />
                        <Legend
                          wrapperStyle={{ fontSize: '10px', fontFamily: 'IBM Plex Mono, monospace', paddingTop: '2px' }}
                          formatter={(value) => (
                            <span style={{ color: value === 'yes' ? yesColor : noColor }}>
                              {value === 'yes' ? yesLabel : value === 'no' ? noLabel : value}
                            </span>
                          )}
                        />
                        <Line
                          type="monotone"
                          dataKey="yes"
                          name="yes"
                          stroke={yesColor}
                          strokeWidth={1.5}
                          dot={false}
                          activeDot={{ r: 3, fill: yesColor, stroke: '#000', strokeWidth: 1 }}
                          isAnimationActive={false}
                          connectNulls
                        />
                        {noPriceHistory.length > 1 && (
                          <Line
                            type="monotone"
                            dataKey="no"
                            name="no"
                            stroke={noColor}
                            strokeWidth={1.5}
                            dot={false}
                            activeDot={{ r: 3, fill: noColor, stroke: '#000', strokeWidth: 1 }}
                            isAnimationActive={false}
                            connectNulls
                          />
                        )}
                      </LineChart>
                    </ResponsiveContainer>
                  );
                })() : (
                  <div style={{ height: '220px', display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#787878' }}>
                    <div style={{ textAlign: 'center' }}>
                      <BarChart3 size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
                      <div style={{ fontSize: '11px', fontFamily: 'IBM Plex Mono, monospace' }}>
                        {priceChartError ?? 'NO PRICE DATA'}
                      </div>
                    </div>
                  </div>
                )}
              </div>
            </div>

            {/* Order Book Panel — YES and NO side by side, proper ladder layout */}
            <div style={{ border: '1px solid #333333', backgroundColor: '#000000', marginBottom: '12px' }}>
              {/* Header */}
              <div style={{ padding: '8px 12px', borderBottom: '1px solid #1e1e1e', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                <span style={{ fontSize: '10px', color: '#787878', fontWeight: 'bold', letterSpacing: '0.05em' }}>ORDER BOOK</span>
                <div style={{ display: 'flex', gap: '16px' }}>
                  {[yesOrderBook, noOrderBook].map((book, i) => {
                    if (!book?.last_trade_price) return null;
                    const ltpNum = parseFloat(book.last_trade_price);
                    if (isNaN(ltpNum)) return null;
                    const name = outcomeNames[i] ?? (i === 0 ? 'YES' : 'NO');
                    const color = OUTCOME_COLORS[i] ?? '#787878';
                    return (
                      <span key={i} style={{ fontSize: '9px', color: '#555' }}>
                        <span style={{ color, fontWeight: 'bold' }}>{name.toUpperCase()}</span>
                        {' LAST: '}
                        <span style={{ color: '#FFFFFF', fontWeight: 'bold' }}>${ltpNum.toFixed(4)}</span>
                      </span>
                    );
                  })}
                </div>
              </div>

              {orderBookLoading ? (
                <div style={{ padding: '20px', textAlign: 'center', color: '#555', fontSize: '10px' }}>LOADING ORDER BOOK...</div>
              ) : orderBookError && !yesOrderBook && !noOrderBook ? (
                <div style={{ padding: '20px', textAlign: 'center', color: '#555', fontSize: '10px' }}>{orderBookError}</div>
              ) : (
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '1px', backgroundColor: '#1a1a1a' }}>
                  {([
                    { book: yesOrderBook, outcomeIdx: 0 },
                    { book: noOrderBook, outcomeIdx: 1 },
                  ] as const).map(({ book, outcomeIdx }) => {
                    const name = outcomeNames[outcomeIdx] ?? (outcomeIdx === 0 ? 'YES' : 'NO');
                    const labelColor = OUTCOME_COLORS[outcomeIdx] ?? '#787878';

                    if (!book) {
                      return (
                        <div key={outcomeIdx} style={{ backgroundColor: '#000000', padding: '20px', textAlign: 'center', color: '#555', fontSize: '9px' }}>
                          <div style={{ color: labelColor, fontWeight: 'bold', marginBottom: '6px' }}>{name.toUpperCase()}</div>
                          NO DATA
                        </div>
                      );
                    }

                    // bids sorted descending: best bid (highest price) first
                    const bids = [...(book.bids ?? [])]
                      .sort((a, b) => parseFloat(b.price) - parseFloat(a.price))
                      .slice(0, 8);
                    // asks sorted ascending: best ask (lowest price) last — we display reversed so best ask is nearest spread
                    const asks = [...(book.asks ?? [])]
                      .sort((a, b) => parseFloat(a.price) - parseFloat(b.price))
                      .slice(0, 8);

                    // Spread between best bid and best ask
                    const bestBidPrice = bids[0] ? parseFloat(bids[0].price) : null;
                    const bestAskPrice = asks[0] ? parseFloat(asks[0].price) : null;
                    const spread = bestBidPrice != null && bestAskPrice != null ? bestAskPrice - bestBidPrice : null;

                    // Shared max size for depth bars across both sides
                    const maxSize = Math.max(
                      ...bids.map(b => parseFloat(b.size) || 0),
                      ...asks.map(a => parseFloat(a.size) || 0),
                      0.001
                    );

                    const fmtSize = (s: number) => s >= 1000 ? `${(s / 1000).toFixed(1)}K` : s.toFixed(0);
                    const fmtPrice = (p: number) => `$${p.toFixed(3)}`;

                    // cols: PRICE (fixed) | SIZE (fixed) — depth bar fills behind
                    const ROW_COLS = '80px 1fr';

                    const renderLevel = (entry: { price: string; size: string }, side: 'bid' | 'ask') => {
                      const price = parseFloat(entry.price);
                      const size = parseFloat(entry.size) || 0;
                      const pct = Math.min((size / maxSize) * 100, 100);
                      const isBid = side === 'bid';
                      return (
                        <div
                          key={`${side}-${entry.price}`}
                          style={{ position: 'relative', display: 'grid', gridTemplateColumns: ROW_COLS, padding: '3px 8px', borderBottom: '1px solid #080808' }}
                        >
                          {/* depth bar — fills from right edge for asks, right edge for bids (both right-to-left so the bar is behind the qty) */}
                          <div style={{
                            position: 'absolute', top: 0, bottom: 0, right: 0,
                            width: `${pct}%`,
                            backgroundColor: isBid ? '#00D66F0D' : '#FF3B3B0D',
                          }} />
                          <span style={{ position: 'relative', fontSize: '10px', fontWeight: 'bold', color: isBid ? '#00D66F' : '#FF3B3B', fontFamily: 'IBM Plex Mono, monospace' }}>
                            {fmtPrice(price)}
                          </span>
                          <span style={{ position: 'relative', fontSize: '10px', color: '#787878', textAlign: 'right', fontFamily: 'IBM Plex Mono, monospace' }}>
                            {fmtSize(size)}
                          </span>
                        </div>
                      );
                    };

                    return (
                      <div key={outcomeIdx} style={{ backgroundColor: '#000000' }}>
                        {/* Outcome header */}
                        <div style={{ padding: '5px 8px', borderBottom: '1px solid #1a1a1a', display: 'flex', alignItems: 'center', justifyContent: 'space-between', backgroundColor: '#050505' }}>
                          <span style={{ fontSize: '10px', color: labelColor, fontWeight: 'bold', letterSpacing: '0.05em' }}>{name.toUpperCase()}</span>
                          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                            {book.last_trade_price && (
                              <span style={{ fontSize: '8px', color: '#555' }}>
                                LAST <span style={{ color: '#FFFFFF' }}>{fmtPrice(parseFloat(book.last_trade_price))}</span>
                              </span>
                            )}
                            {book.tick_size && (
                              <span style={{ fontSize: '8px', color: '#333' }}>TICK {book.tick_size}</span>
                            )}
                          </div>
                        </div>

                        {/* Column labels */}
                        <div style={{ display: 'grid', gridTemplateColumns: ROW_COLS, padding: '2px 8px', borderBottom: '1px solid #111' }}>
                          <span style={{ fontSize: '8px', color: '#555', fontWeight: 'bold' }}>PRICE</span>
                          <span style={{ fontSize: '8px', color: '#555', fontWeight: 'bold', textAlign: 'right' }}>QTY ($)</span>
                        </div>

                        {/* ASKS — worst (highest) at top, best (lowest) at bottom just above spread */}
                        {asks.length === 0
                          ? <div style={{ padding: '6px 8px', fontSize: '9px', color: '#333', textAlign: 'center' }}>no asks</div>
                          : [...asks].reverse().map(a => renderLevel(a, 'ask'))
                        }

                        {/* Spread separator */}
                        <div style={{ display: 'grid', gridTemplateColumns: ROW_COLS, padding: '3px 8px', backgroundColor: '#0C0C0C', borderTop: '1px solid #1a1a1a', borderBottom: '1px solid #1a1a1a' }}>
                          <span style={{ fontSize: '8px', color: '#444', fontWeight: 'bold' }}>SPREAD</span>
                          <span style={{ fontSize: '9px', color: '#555', textAlign: 'right', fontFamily: 'IBM Plex Mono, monospace' }}>
                            {spread != null ? fmtPrice(spread) : '—'}
                          </span>
                        </div>

                        {/* BIDS — best (highest) at top just below spread, worst at bottom */}
                        {bids.length === 0
                          ? <div style={{ padding: '6px 8px', fontSize: '9px', color: '#333', textAlign: 'center' }}>no bids</div>
                          : bids.map(b => renderLevel(b, 'bid'))
                        }
                      </div>
                    );
                  })}
                </div>
              )}
            </div>

            {/* Market Stats Grid — 8 cells */}
            {(() => {
              // Open interest lives on events[0].openInterest (number)
              const eventOpenInterest: number | null = (selectedMarket as any).events?.[0]?.openInterest ?? null;
              // 24h volume lives on events[0].series[0].volume24hr
              const series24h: number | null = (selectedMarket as any).events?.[0]?.series?.[0]?.volume24hr ?? null;
              // 1wk / 1mo volume — API returns volume1wk / volume1mo directly on market
              const vol1wk: number | null = selectedMarket.volume1wk ?? null;
              const vol1mo: number | null = selectedMarket.volume1mo ?? null;

              return (
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(4, 1fr)',
                  gap: '1px',
                  backgroundColor: '#1a1a1a',
                  border: '1px solid #1a1a1a',
                  marginBottom: '12px'
                }}>
                  {[
                    {
                      label: 'TOTAL VOLUME',
                      value: formatDollarVolume(selectedMarket.volumeNum ?? selectedMarket.volume ?? 0),
                    },
                    {
                      label: '24H VOLUME',
                      value: series24h != null ? formatDollarVolume(series24h) : 'N/A',
                    },
                    {
                      label: 'LIQUIDITY',
                      value: formatDollarVolume(selectedMarket.liquidityNum ?? selectedMarket.liquidity ?? 0),
                    },
                    {
                      label: 'OPEN INTEREST',
                      value: eventOpenInterest != null ? formatDollarVolume(eventOpenInterest) : 'N/A',
                    },
                    {
                      label: 'END DATE',
                      value: selectedMarket.endDate
                        ? new Date(selectedMarket.endDate).toLocaleDateString('en-US', { month: 'numeric', day: 'numeric', year: 'numeric' })
                        : 'N/A',
                    },
                    {
                      label: 'SPREAD',
                      value: (() => {
                        const s = selectedMarket.spread;
                        if (s !== null && s !== undefined && !isNaN(Number(s))) return `${(Number(s) * 100).toFixed(2)}%`;
                        return 'N/A';
                      })(),
                    },
                    {
                      label: '1WK VOLUME',
                      value: vol1wk != null ? formatDollarVolume(vol1wk) : 'N/A',
                    },
                    {
                      label: '1MO VOLUME',
                      value: vol1mo != null ? formatDollarVolume(vol1mo) : 'N/A',
                    },
                  ].map(stat => (
                    <div key={stat.label} style={{ backgroundColor: '#000000', padding: '12px' }}>
                      <div style={{ color: '#555', fontSize: '9px', marginBottom: '4px', fontWeight: 'bold', letterSpacing: '0.05em' }}>
                        {stat.label}
                      </div>
                      <div style={{ color: '#FFFFFF', fontSize: '13px', fontWeight: 'bold' }}>
                        {stat.value}
                      </div>
                    </div>
                  ))}
                </div>
              );
            })()}

            {/* Price Change Row — only 1D is reliably provided by the API */}
            {selectedMarket.oneDayPriceChange != null && (
              <div style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(4, 1fr)',
                gap: '1px',
                backgroundColor: '#1a1a1a',
                border: '1px solid #1a1a1a',
                marginBottom: '12px'
              }}>
                {[
                  { label: '1D PRICE CHANGE', val: selectedMarket.oneDayPriceChange },
                  { label: 'LAST TRADE', val: selectedMarket.lastTradePrice != null ? selectedMarket.lastTradePrice : null, isPrice: true },
                  { label: 'BEST BID', val: selectedMarket.bestBid != null ? selectedMarket.bestBid : null, isPrice: true },
                  { label: 'BEST ASK', val: selectedMarket.bestAsk != null ? selectedMarket.bestAsk : null, isPrice: true },
                ].map(item => {
                  const isPrice = (item as any).isPrice;
                  const color = item.val == null ? '#555'
                    : isPrice ? '#FFFFFF'
                    : item.val >= 0 ? '#00D66F' : '#FF3B3B';
                  const displayVal = item.val == null ? 'N/A'
                    : isPrice ? `$${(item.val as number).toFixed(4)}`
                    : `${item.val >= 0 ? '+' : ''}${(item.val * 100).toFixed(2)}%`;
                  return (
                    <div key={item.label} style={{ backgroundColor: '#000000', padding: '12px' }}>
                      <div style={{ color: '#555', fontSize: '9px', marginBottom: '4px', fontWeight: 'bold', letterSpacing: '0.05em' }}>{item.label}</div>
                      <div style={{ color, fontSize: '13px', fontWeight: 'bold' }}>{displayVal}</div>
                    </div>
                  );
                })}
              </div>
            )}

            {/* Market Metadata Row */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(4, 1fr)',
              gap: '1px',
              backgroundColor: '#1a1a1a',
              border: '1px solid #1a1a1a',
              marginBottom: '12px'
            }}>
              {[
                {
                  label: 'RESOLUTION SOURCE',
                  value: selectedMarket.resolutionSource || 'N/A',
                },
                {
                  label: 'STATUS',
                  value: selectedMarket.acceptingOrders ? 'ACCEPTING ORDERS'
                    : selectedMarket.closed ? 'CLOSED'
                    : 'NOT ACCEPTING',
                },
                {
                  label: 'MIN ORDER SIZE',
                  value: (() => {
                    const minSize = yesOrderBook?.min_order_size ?? (selectedMarket.orderMinSize != null ? String(selectedMarket.orderMinSize) : null);
                    return minSize != null ? `$${parseFloat(minSize).toFixed(2)}` : 'N/A';
                  })(),
                },
                {
                  label: 'TICK SIZE',
                  value: (() => {
                    const tick = yesOrderBook?.tick_size ?? (selectedMarket.orderPriceMinTickSize != null ? String(selectedMarket.orderPriceMinTickSize) : null);
                    return tick != null ? parseFloat(tick).toFixed(6) : 'N/A';
                  })(),
                },
              ].map(stat => (
                <div key={stat.label} style={{ backgroundColor: '#000000', padding: '12px' }}>
                  <div style={{ color: '#555', fontSize: '9px', marginBottom: '4px', fontWeight: 'bold', letterSpacing: '0.05em' }}>
                    {stat.label}
                  </div>
                  <div style={{
                    color: stat.label === 'STATUS' && stat.value === 'ACCEPTING ORDERS' ? '#00D66F'
                      : stat.label === 'STATUS' ? '#FF3B3B'
                      : '#FFFFFF',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    wordBreak: 'break-all'
                  }}>
                    {stat.value}
                  </div>
                </div>
              ))}
            </div>

            {/* Top Holders Table */}
            <div style={{ border: '1px solid #333333', backgroundColor: '#000000', marginBottom: '12px' }}>
              <div style={{ padding: '8px 12px', borderBottom: '1px solid #1e1e1e' }}>
                <span style={{ fontSize: '10px', color: '#787878' }}>TOP HOLDERS</span>
              </div>
              {holdersLoading ? (
                <div style={{ padding: '20px', textAlign: 'center', color: '#555', fontSize: '10px' }}>LOADING HOLDERS...</div>
              ) : holdersError ? (
                <div style={{ padding: '20px', textAlign: 'center', color: '#555', fontSize: '10px' }}>{holdersError}</div>
              ) : !topHolders || topHolders.holders.length === 0 ? (
                <div style={{ padding: '20px', textAlign: 'center', color: '#555', fontSize: '10px' }}>NO HOLDER DATA</div>
              ) : (
                <>
                  {/* Column headers */}
                  <div style={{ display: 'grid', gridTemplateColumns: '24px 1fr 80px 80px', padding: '4px 8px', borderBottom: '1px solid #111', gap: '8px' }}>
                    <span style={{ fontSize: '9px', color: '#555', fontWeight: 'bold' }}>#</span>
                    <span style={{ fontSize: '9px', color: '#555', fontWeight: 'bold' }}>HOLDER</span>
                    <span style={{ fontSize: '9px', color: '#555', fontWeight: 'bold', textAlign: 'center' }}>OUTCOME</span>
                    <span style={{ fontSize: '9px', color: '#555', fontWeight: 'bold', textAlign: 'right' }}>POSITION</span>
                  </div>
                  <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
                    {topHolders.holders.slice(0, 20).map((holder, idx) => {
                      const outcomeName = outcomeNames[holder.outcomeIndex] ?? `Outcome ${holder.outcomeIndex}`;
                      const outcomeColor = OUTCOME_COLORS[holder.outcomeIndex] ?? '#787878';
                      const displayName = holder.name ?? holder.pseudonym ?? `${holder.proxyWallet.slice(0, 6)}...${holder.proxyWallet.slice(-4)}`;
                      return (
                        <div key={idx} style={{ display: 'grid', gridTemplateColumns: '24px 1fr 80px 80px', padding: '5px 8px', borderBottom: '1px solid #0a0a0a', gap: '8px', alignItems: 'center' }}>
                          <span style={{ fontSize: '9px', color: '#555' }}>{idx + 1}</span>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', overflow: 'hidden' }}>
                            {holder.profileImage && (
                              <img src={holder.profileImage} alt="" style={{ width: '16px', height: '16px', borderRadius: '50%', flexShrink: 0 }} />
                            )}
                            <span style={{ fontSize: '10px', color: '#FFFFFF', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{displayName}</span>
                          </div>
                          <span style={{ fontSize: '9px', color: outcomeColor, fontWeight: 'bold', textAlign: 'center', textTransform: 'uppercase' }}>{outcomeName}</span>
                          <span style={{ fontSize: '10px', color: '#FFFFFF', fontWeight: 'bold', textAlign: 'right' }}>{formatDollarVolume(holder.amount)}</span>
                        </div>
                      );
                    })}
                  </div>
                </>
              )}
            </div>

            {/* Recent Trades */}
            {recentTrades.length > 0 && (
              <div style={{ backgroundColor: '#0A0A0A', border: '1px solid #333333', padding: '16px', marginBottom: '12px' }}>
                <div style={{ color: '#FF8800', fontSize: '11px', fontWeight: 'bold', marginBottom: '12px', borderBottom: '1px solid #333333', paddingBottom: '8px' }}>
                  RECENT TRADES
                </div>
                <div style={{ maxHeight: '300px', overflow: 'auto' }}>
                  {recentTrades.slice(0, 30).map((trade) => (
                    <div
                      key={trade.id}
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'space-between',
                        fontSize: '10px',
                        borderBottom: '1px solid #1A1A1A',
                        padding: '6px 0'
                      }}
                    >
                      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                        <span style={{
                          padding: '2px 8px',
                          backgroundColor: trade.side === 'BUY' ? '#003300' : '#330000',
                          color: trade.side === 'BUY' ? '#00D66F' : '#FF3B3B',
                          fontWeight: 'bold',
                          fontSize: '9px',
                          border: `1px solid ${trade.side === 'BUY' ? '#00D66F' : '#FF3B3B'}`
                        }}>
                          {trade.side}
                        </span>
                        <span style={{ color: '#787878' }}>
                          {(() => {
                            const size = parseFloat(trade.size);
                            return !isNaN(size) ? size.toFixed(2) : '0.00';
                          })()} shares
                        </span>
                      </div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                        <span style={{ color: '#FFFFFF', fontWeight: 'bold' }}>
                          ${(() => {
                            const price = parseFloat(trade.price);
                            return !isNaN(price) ? price.toFixed(4) : '0.0000';
                          })()}
                        </span>
                        <span style={{ color: '#787878' }}>
                          {new Date(trade.timestamp * 1000).toLocaleTimeString()}
                        </span>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* External Link */}
            {(selectedMarket.slug || selectedMarket.events?.[0]?.slug) && (
              <div style={{ marginTop: '16px' }}>
                <a
                  href={`https://polymarket.com/event/${selectedMarket.events?.[0]?.slug ?? selectedMarket.slug}`}
                  target="_blank"
                  rel="noopener noreferrer"
                  style={{
                    display: 'inline-flex',
                    alignItems: 'center',
                    gap: '8px',
                    padding: '10px 16px',
                    backgroundColor: '#FF8800',
                    color: '#000000',
                    textDecoration: 'none',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    border: 'none',
                    cursor: 'pointer',
                    fontFamily: 'inherit'
                  }}
                >
                  <ExternalLink size={14} />
                  TRADE ON POLYMARKET
                </a>
              </div>
            )}
          </div>
        </div>
      </div>
    );
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: '#000000',
      color: '#FFFFFF',
      fontFamily: 'IBM Plex Mono, monospace',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div style={{
        borderBottom: '2px solid #FF8800',
        padding: '12px 16px',
        backgroundColor: '#000000'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
          <div>
            <h1 style={{
              color: '#FF8800',
              fontSize: '16px',
              fontWeight: 'bold',
              margin: 0,
              marginBottom: '4px'
            }}>
              POLYMARKET PREDICTION MARKETS
            </h1>
            <div style={{ fontSize: '10px', color: '#787878' }}>
              Real-time prediction markets • Politics • Sports • Crypto • Current Events
            </div>
          </div>
          <button
            onClick={handleRefresh}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '6px 12px',
              backgroundColor: loading ? '#1A1A1A' : '#FF8800',
              color: loading ? '#787878' : '#000000',
              border: 'none',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: loading ? 'not-allowed' : 'pointer',
              fontFamily: 'inherit'
            }}
            disabled={loading}
          >
            <RefreshCw size={14} className={loading ? 'animate-spin' : ''} />
            REFRESH
          </button>
        </div>

        {/* View Tabs */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '1px', marginBottom: '12px', backgroundColor: '#333333' }}>
          {['markets', 'events', 'sports'].map((view) => (
            <button
              key={view}
              onClick={() => {
                setActiveView(view as any);
                setCurrentPage(0);
              }}
              style={{
                flex: 1,
                padding: '8px 12px',
                backgroundColor: activeView === view ? '#FF8800' : '#000000',
                color: activeView === view ? '#000000' : '#787878',
                border: 'none',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'inherit',
                transition: 'all 0.1s'
              }}
            >
              {view.toUpperCase()}
            </button>
          ))}
        </div>

        {/* Search and Filters */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr auto auto auto', gap: '8px' }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            backgroundColor: '#1A1A1A',
            border: '1px solid #333333',
            padding: '6px 10px'
          }}>
            <Search size={14} style={{ color: '#787878', marginRight: '8px' }} />
            <input
              type="text"
              placeholder="SEARCH MARKETS..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
              style={{
                flex: 1,
                backgroundColor: 'transparent',
                border: 'none',
                outline: 'none',
                fontSize: '10px',
                color: '#FFFFFF',
                fontFamily: 'inherit'
              }}
            />
          </div>

          <select
            value={selectedTag}
            onChange={(e) => {
              setSelectedTag(e.target.value);
              setCurrentPage(0);
            }}
            style={{
              backgroundColor: '#1A1A1A',
              color: '#FFFFFF',
              fontSize: '10px',
              fontWeight: 'bold',
              border: '1px solid #333333',
              padding: '6px 10px',
              outline: 'none',
              fontFamily: 'inherit',
              cursor: 'pointer'
            }}
          >
            <option value="">ALL CATEGORIES</option>
            {tags.map((tag) => (
              <option key={tag.id} value={tag.id}>
                {tag.label ? tag.label.toUpperCase() : 'UNKNOWN'}
              </option>
            ))}
          </select>

          <select
            value={sortBy}
            onChange={(e) => setSortBy(e.target.value as any)}
            style={{
              backgroundColor: '#1A1A1A',
              color: '#FFFFFF',
              fontSize: '10px',
              fontWeight: 'bold',
              border: '1px solid #333333',
              padding: '6px 10px',
              outline: 'none',
              fontFamily: 'inherit',
              cursor: 'pointer'
            }}
          >
            <option value="volume">VOLUME</option>
            <option value="liquidity">LIQUIDITY</option>
            <option value="recent">MOST RECENT</option>
          </select>

          <label style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            backgroundColor: showClosed ? '#FF8800' : '#1A1A1A',
            color: showClosed ? '#000000' : '#FFFFFF',
            border: '1px solid #333333',
            padding: '6px 10px',
            cursor: 'pointer',
            fontSize: '10px',
            fontWeight: 'bold',
            fontFamily: 'inherit'
          }}>
            <input
              type="checkbox"
              checked={showClosed}
              onChange={(e) => {
                setShowClosed(e.target.checked);
                setCurrentPage(0);
              }}
              style={{ cursor: 'pointer' }}
            />
            SHOW CLOSED
          </label>
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', backgroundColor: '#000000' }}>
        {loading && (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '400px',
            flexDirection: 'column'
          }}>
            <RefreshCw className="animate-spin" size={32} style={{ color: '#FF8800', marginBottom: '12px' }} />
            <div style={{ color: '#787878', fontSize: '11px' }}>LOADING DATA...</div>
          </div>
        )}

        {error && (
          <div style={{
            backgroundColor: '#330000',
            border: '1px solid #FF3B3B',
            padding: '16px',
            margin: '16px',
            display: 'flex',
            alignItems: 'flex-start',
            gap: '12px'
          }}>
            <AlertCircle style={{ color: '#FF3B3B', flexShrink: 0 }} size={20} />
            <div>
              <div style={{ fontWeight: 'bold', color: '#FF3B3B', marginBottom: '4px', fontSize: '11px' }}>ERROR</div>
              <div style={{ fontSize: '10px', color: '#FF8888' }}>{error}</div>
            </div>
          </div>
        )}

        {!loading && !error && (
          <div style={{ padding: '8px' }}>
            {activeView === 'markets' && (
              <>
                {markets.length === 0 ? (
                  <div style={{
                    textAlign: 'center',
                    padding: '80px 20px',
                    color: '#787878'
                  }}>
                    <Info size={48} style={{ opacity: 0.3, margin: '0 auto 12px' }} />
                    <div style={{ fontSize: '11px' }}>NO MARKETS FOUND</div>
                  </div>
                ) : (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(400px, 1fr))', gap: '0' }}>
                    {markets.map(renderMarketCard)}
                  </div>
                )}
              </>
            )}

            {activeView === 'events' && (
              <>
                {events.length === 0 ? (
                  <div style={{
                    textAlign: 'center',
                    padding: '80px 20px',
                    color: '#787878'
                  }}>
                    <Info size={48} style={{ opacity: 0.3, margin: '0 auto 12px' }} />
                    <div style={{ fontSize: '11px' }}>NO EVENTS FOUND</div>
                  </div>
                ) : (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(450px, 1fr))', gap: '0' }}>
                    {events.map(renderEventCard)}
                  </div>
                )}
              </>
            )}

            {activeView === 'sports' && (
              <>
                {selectedSport ? (
                  <>
                    {/* Sport Detail Header */}
                    <div style={{
                      backgroundColor: '#0A0A0A',
                      border: '1px solid #333333',
                      padding: '16px',
                      marginBottom: '1px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '16px'
                    }}>
                      <button
                        onClick={() => setSelectedSport(null)}
                        style={{
                          backgroundColor: 'transparent',
                          border: '1px solid #787878',
                          color: '#787878',
                          padding: '8px 12px',
                          cursor: 'pointer',
                          fontFamily: 'IBM Plex Mono, monospace',
                          fontSize: '11px'
                        }}
                      >
                        ← BACK TO SPORTS
                      </button>
                      {selectedSport.image && (
                        <img
                          src={selectedSport.image}
                          alt={selectedSport.sport}
                          style={{
                            width: '48px',
                            height: '48px',
                            objectFit: 'cover',
                            borderRadius: '2px'
                          }}
                        />
                      )}
                      <div>
                        <div style={{ color: '#FF8800', fontSize: '16px', fontWeight: 'bold', marginBottom: '4px' }}>
                          {selectedSport.sport.toUpperCase()}
                        </div>
                        <div style={{ color: '#787878', fontSize: '10px' }}>
                          {markets.length} markets available
                        </div>
                      </div>
                    </div>

                    {/* Markets for this sport */}
                    {loading ? (
                      <div style={{ textAlign: 'center', padding: '80px 20px', color: '#787878' }}>
                        <RefreshCw size={48} style={{ opacity: 0.3, margin: '0 auto 12px', animation: 'spin 1s linear infinite' }} />
                        <div style={{ fontSize: '11px' }}>LOADING MARKETS...</div>
                      </div>
                    ) : markets.length === 0 ? (
                      <div style={{ textAlign: 'center', padding: '80px 20px', color: '#787878' }}>
                        <Info size={48} style={{ opacity: 0.3, margin: '0 auto 12px' }} />
                        <div style={{ fontSize: '11px' }}>NO MARKETS FOUND FOR THIS SPORT</div>
                      </div>
                    ) : (
                      markets.map((market) => renderMarketCard(market))
                    )}
                  </>
                ) : sports.length === 0 ? (
                  <div style={{
                    textAlign: 'center',
                    padding: '80px 20px',
                    color: '#787878'
                  }}>
                    <Info size={48} style={{ opacity: 0.3, margin: '0 auto 12px' }} />
                    <div style={{ fontSize: '11px' }}>NO SPORTS FOUND</div>
                  </div>
                ) : (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', gap: '0' }}>
                    {sports.map((sport) => (
                      <div
                        key={sport.id}
                        onClick={() => {
                          setSelectedSport(sport);
                        }}
                        style={{
                          backgroundColor: '#000000',
                          border: '1px solid #333333',
                          padding: '16px',
                          marginBottom: '1px',
                          cursor: 'pointer',
                          transition: 'background-color 0.1s',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '12px'
                        }}
                        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#0A0A0A'}
                        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#000000'}
                      >
                        {sport.image && (
                          <img
                            src={sport.image}
                            alt={sport.sport}
                            style={{
                              width: '40px',
                              height: '40px',
                              objectFit: 'cover',
                              borderRadius: '2px'
                            }}
                          />
                        )}
                        <div style={{ flex: 1 }}>
                          <div style={{ color: '#FF8800', fontSize: '13px', fontWeight: 'bold', marginBottom: '4px' }}>
                            {sport.sport ? sport.sport.toUpperCase() : 'UNKNOWN'}
                          </div>
                          <div style={{ fontSize: '10px', color: '#787878' }}>
                            Click to view markets
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </>
            )}

            {/* Pagination */}
            {(markets.length >= pageSize || events.length >= pageSize) && (
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '1px',
                marginTop: '16px',
                backgroundColor: '#333333',
                border: '1px solid #333333'
              }}>
                <button
                  onClick={() => setCurrentPage(Math.max(0, currentPage - 1))}
                  disabled={currentPage === 0}
                  style={{
                    flex: 1,
                    padding: '8px 16px',
                    backgroundColor: currentPage === 0 ? '#0A0A0A' : '#000000',
                    color: currentPage === 0 ? '#333333' : '#FFFFFF',
                    border: 'none',
                    fontSize: '10px',
                    fontWeight: 'bold',
                    cursor: currentPage === 0 ? 'not-allowed' : 'pointer',
                    fontFamily: 'inherit'
                  }}
                >
                  PREVIOUS
                </button>
                <div style={{
                  padding: '8px 16px',
                  backgroundColor: '#FF8800',
                  color: '#000000',
                  fontSize: '10px',
                  fontWeight: 'bold'
                }}>
                  PAGE {currentPage + 1}
                </div>
                <button
                  onClick={() => setCurrentPage(currentPage + 1)}
                  style={{
                    flex: 1,
                    padding: '8px 16px',
                    backgroundColor: '#000000',
                    color: '#FFFFFF',
                    border: 'none',
                    fontSize: '10px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    fontFamily: 'inherit'
                  }}
                >
                  NEXT
                </button>
              </div>
            )}
          </div>
        )}
      </div>

      {/* Market Details Modal */}
      {selectedMarket && renderMarketDetails()}
    </div>
  );
};

export default PolymarketTabEnhanced;
