// @refresh reset
// File: src/components/tabs/polymarket/PolymarketTab.tsx
// Thin orchestrator — all heavy UI lives in sub-components for reliable HMR.

import React, { useState, useEffect, useRef } from 'react';
import {
  Activity, Search, RefreshCw, Wallet, Bot, CheckCircle,
} from 'lucide-react';
import polymarketApiService, {
  PolymarketMarket, PolymarketEvent, PolymarketTag, PolymarketSport, PolymarketTrade,
  PricePoint, WSMarketUpdate,
  PolymarketOrderBookEnriched, PolymarketTopHolders, PolymarketOpenInterest,
} from '@/services/polymarket/polymarketApiService';
import { C } from './tokens';
import MarketList    from './MarketList';
import MarketDetail  from './MarketDetail';
import EventDetail   from './EventDetail';
import PortfolioPanel from './PortfolioPanel';
import BotDeployView  from './BotDeployView';
import BotMonitorView from './BotMonitorView';
import { TabFooter } from '@/components/common/TabFooter';
import ResolvedDetail from './ResolvedDetail';

type ActiveView = 'markets' | 'events' | 'sports' | 'resolved' | 'portfolio' | 'bots' | 'bot-deploy';

const PolymarketTab: React.FC = () => {
  // ── Navigation ──────────────────────────────────────────────────────────────
  const [activeView, setActiveView] = useState<ActiveView>('markets');

  // ── List state ───────────────────────────────────────────────────────────────
  const [markets,    setMarkets]    = useState<PolymarketMarket[]>([]);
  const [events,     setEvents]     = useState<PolymarketEvent[]>([]);
  const [tags,       setTags]       = useState<PolymarketTag[]>([]);
  const [sports,     setSports]     = useState<PolymarketSport[]>([]);
  const [loading,    setLoading]    = useState(false);
  const [error,      setError]      = useState<string | null>(null);
  const [searchQuery,  setSearchQuery]  = useState('');
  const [selectedTag,  setSelectedTag]  = useState('');
  const [showClosed,   setShowClosed]   = useState(false);
  const [sortBy,       setSortBy]       = useState<'volume' | 'liquidity' | 'recent'>('volume');
  const [currentPage,  setCurrentPage]  = useState(0);
  const pageSize = 20;

  // ── Resolved events state ────────────────────────────────────────────────────
  const [resolvedEvents,      setResolvedEvents]      = useState<PolymarketEvent[]>([]);
  const [resolvedPage,        setResolvedPage]        = useState(0);
  const [resolvedSearch,      setResolvedSearch]      = useState('');
  const [resolvedSearchInput, setResolvedSearchInput] = useState('');
  const [selectedResolved,    setSelectedResolved]    = useState<PolymarketEvent | null>(null);
  const resolvedPageSize = 20;

  // ── Selection state ──────────────────────────────────────────────────────────
  const [selectedMarket, setSelectedMarket] = useState<PolymarketMarket | null>(null);
  const [selectedEvent,  setSelectedEvent]  = useState<PolymarketEvent | null>(null);
  const [selectedSport,  setSelectedSport]  = useState<PolymarketSport | null>(null);

  // ── Market detail state ──────────────────────────────────────────────────────
  const [recentTrades,     setRecentTrades]     = useState<PolymarketTrade[]>([]);
  const [priceHistory,     setPriceHistory]     = useState<PricePoint[]>([]);
  const [noPriceHistory,   setNoPriceHistory]   = useState<PricePoint[]>([]);
  const [chartInterval,    setChartInterval]    = useState('1d');
  const [priceChartLoading, setPriceChartLoading] = useState(false);
  const [priceChartError,  setPriceChartError]  = useState<string | null>(null);
  const [yesOrderBook,     setYesOrderBook]     = useState<PolymarketOrderBookEnriched | null>(null);
  const [noOrderBook,      setNoOrderBook]      = useState<PolymarketOrderBookEnriched | null>(null);
  const [orderBookLoading, setOrderBookLoading] = useState(false);
  const [orderBookError,   setOrderBookError]   = useState<string | null>(null);
  const [topHolders,       setTopHolders]       = useState<PolymarketTopHolders | null>(null);
  const [holdersLoading,   setHoldersLoading]   = useState(false);
  const [holdersError,     setHoldersError]     = useState<string | null>(null);
  const [openInterest,     setOpenInterest]     = useState<PolymarketOpenInterest | null>(null);
  const [livePrices,       setLivePrices]       = useState<Record<string, number>>({});
  const [wsConnected,      setWsConnected]      = useState(false);
  const marketWsRef = useRef<WebSocket | null>(null);

  // ── Effects ──────────────────────────────────────────────────────────────────
  useEffect(() => { loadInitialData(); }, []);

  useEffect(() => {
    if (activeView === 'markets') loadMarkets();
    else if (activeView === 'events') loadEvents();
    else if (activeView === 'sports') loadSports();
    else if (activeView === 'resolved') loadResolvedEvents();
  }, [activeView, selectedTag, showClosed, currentPage, sortBy]);

  useEffect(() => {
    if (activeView === 'resolved') loadResolvedEvents();
  }, [resolvedPage, resolvedSearch]);

  useEffect(() => {
    if (selectedSport) loadSportMarkets(selectedSport);
  }, [selectedSport, showClosed, currentPage, sortBy]);

  // Market WS
  useEffect(() => {
    if (marketWsRef.current) { marketWsRef.current.close(); marketWsRef.current = null; setWsConnected(false); }
    if (!selectedMarket) return;

    let tokenIds: string[] = [];
    try {
      const raw = selectedMarket.clobTokenIds;
      tokenIds = typeof raw === 'string' ? JSON.parse(raw) : Array.isArray(raw) ? raw as string[] : [];
    } catch { /* ignore */ }
    if (tokenIds.length === 0) return;

    const yesId = tokenIds[0], noId = tokenIds[1];
    try {
      const ws = polymarketApiService.connectMarketWebSocket(tokenIds, (update: WSMarketUpdate) => {
        const assetId = update.asset_id;
        const evt     = update.event_type;
        if (evt === 'book') {
          const bids = Array.isArray(update.bids) ? [...update.bids].sort((a, b) => parseFloat(b.price) - parseFloat(a.price)) : [];
          const asks = Array.isArray(update.asks) ? [...update.asks].sort((a, b) => parseFloat(a.price) - parseFloat(b.price)) : [];
          const snap = { market: update.market, asset_id: assetId ?? '', bids, asks, timestamp: Number(update.timestamp), tick_size: update.tick_size, last_trade_price: update.last_trade_price };
          if (assetId === yesId)      setYesOrderBook(prev => ({ ...prev, ...snap }));
          else if (assetId === noId)  setNoOrderBook(prev  => ({ ...prev, ...snap }));
        } else if (evt === 'price_change') {
          const p = parseFloat(update.price ?? '');
          if (assetId && !isNaN(p)) setLivePrices(prev => ({ ...prev, [assetId]: p }));
        } else if (evt === 'last_trade_price' || evt === 'best_bid_ask') {
          const ltp = parseFloat(update.last_trade_price ?? update.price ?? (update as any).best_bid ?? '');
          if (!isNaN(ltp) && assetId) setLivePrices(prev => ({ ...prev, [assetId]: ltp }));
        }
      });
      marketWsRef.current = ws;
      ws.onopen  = () => setWsConnected(true);
      ws.onclose = () => setWsConnected(false);
    } catch (err) { console.warn('Market WS failed:', err); }

    return () => { if (marketWsRef.current) { marketWsRef.current.close(); marketWsRef.current = null; setWsConnected(false); } };
  }, [selectedMarket?.id]);

  useEffect(() => () => {
    polymarketApiService.disconnectWebSocket('all');
    if (marketWsRef.current) marketWsRef.current.close();
  }, []);

  // ── Data loaders ─────────────────────────────────────────────────────────────
  const loadInitialData = async () => {
    try {
      setLoading(true);
      const [tagsData, sportsData] = await Promise.all([
        polymarketApiService.getTags().catch(() => []),
        polymarketApiService.getSports().catch(() => []),
      ]);
      setTags(tagsData || []);
      setSports(sportsData || []);
    } finally { setLoading(false); }
  };

  const loadMarkets = async () => {
    try {
      setLoading(true); setError(null);
      const data = await polymarketApiService.getMarkets({
        limit: pageSize, offset: currentPage * pageSize,
        closed: showClosed ? undefined : false,
        active: showClosed ? undefined : true,
        tag_id: selectedTag || undefined,
        order: sortBy === 'volume' ? 'volume' : sortBy === 'liquidity' ? 'liquidity' : 'id',
        ascending: false,
      });
      setMarkets(data || []);
    } catch { setError('Failed to load markets'); setMarkets([]); }
    finally { setLoading(false); }
  };

  const loadEvents = async () => {
    try {
      setLoading(true); setError(null);
      const data = await polymarketApiService.getEvents({
        limit: pageSize, offset: currentPage * pageSize,
        closed: showClosed ? undefined : false,
        active: showClosed ? undefined : true,
        tag_id: selectedTag || undefined,
        order: 'id', ascending: false,
      });
      setEvents(data || []);
    } catch { setError('Failed to load events'); setEvents([]); }
    finally { setLoading(false); }
  };

  const loadSports = async () => {
    try {
      setLoading(true); setError(null);
      const data = await polymarketApiService.getSports();
      setSports(data || []);
    } catch { setError('Failed to load sports'); setSports([]); }
    finally { setLoading(false); }
  };

  const loadResolvedEvents = async () => {
    try {
      setLoading(true); setError(null);
      const data = await polymarketApiService.getEvents({
        limit: resolvedPageSize,
        offset: resolvedPage * resolvedPageSize,
        closed: true,
        active: false,
        order: 'id',
        ascending: false,
      });
      // Client-side search filter
      const filtered = resolvedSearch.trim()
        ? (data || []).filter(e =>
            e.title?.toLowerCase().includes(resolvedSearch.toLowerCase()) ||
            e.description?.toLowerCase().includes(resolvedSearch.toLowerCase())
          )
        : (data || []);
      setResolvedEvents(filtered);
    } catch { setError('Failed to load resolved events'); setResolvedEvents([]); }
    finally { setLoading(false); }
  };

  const loadSportMarkets = async (sport: PolymarketSport) => {
    try {
      setLoading(true); setError(null);
      const tag = sport.tags?.split(',')[0] || '';
      const data = await polymarketApiService.getMarkets({
        limit: pageSize, offset: currentPage * pageSize,
        closed: showClosed ? undefined : false,
        active: showClosed ? undefined : true,
        tag_id: tag,
        order: sortBy === 'volume' ? 'volume' : sortBy === 'liquidity' ? 'liquidity' : 'id',
        ascending: false,
      });
      setMarkets(data || []);
    } catch { setError('Failed to load markets'); setMarkets([]); }
    finally { setLoading(false); }
  };

  const loadMarketDetails = async (market: PolymarketMarket, interval: string = chartInterval) => {
    const isNew = market.id !== selectedMarket?.id;
    setSelectedMarket(market);
    setPriceHistory([]); setNoPriceHistory([]);
    setPriceChartLoading(true); setPriceChartError(null);

    if (isNew) {
      setRecentTrades([]); setYesOrderBook(null); setNoOrderBook(null);
      setOrderBookLoading(true); setOrderBookError(null);
      setTopHolders(null); setHoldersLoading(true); setHoldersError(null);
      setOpenInterest(null);
    }

    let tokenIds: string[] = [];
    try {
      const raw = market.clobTokenIds;
      tokenIds = typeof raw === 'string' ? JSON.parse(raw) : Array.isArray(raw) ? raw as string[] : [];
    } catch { /* ignore */ }

    const yesId = tokenIds[0], noId = tokenIds[1];

    if (isNew) {
      if (yesId || noId) {
        Promise.all([
          yesId ? polymarketApiService.getOrderBookEnriched(yesId).catch(() => null) : Promise.resolve(null),
          noId  ? polymarketApiService.getOrderBookEnriched(noId).catch(() => null)  : Promise.resolve(null),
        ]).then(([yes, no]) => {
          setYesOrderBook(yes); setNoOrderBook(no);
          if (!yes && !no) setOrderBookError('Order book unavailable');
        }).finally(() => setOrderBookLoading(false));
      } else { setOrderBookLoading(false); setOrderBookError('No token IDs available'); }

      const condId = market.conditionId ?? '';
      if (condId) {
        polymarketApiService.getTopHolders(condId, 20).then(setTopHolders).catch(() => setHoldersError('Failed to load holders')).finally(() => setHoldersLoading(false));
        polymarketApiService.getOpenInterest([condId]).then(data => setOpenInterest(data.find(d => d.market === condId) ?? data[0] ?? null)).catch(() => {});
      } else { setHoldersLoading(false); setHoldersError('conditionId not available'); }
    }

    if (yesId) {
      try {
        const result = await polymarketApiService.getTrades({ token_id: yesId, limit: 50 });
        setRecentTrades(Array.isArray(result) ? result : ((result as any).data ?? []));
      } catch { /* non-fatal */ }
    }

    if (yesId) {
      try {
        const fidelityMap: Record<string, number> = { '1h': 1, '6h': 5, '1d': 60, '1w': 360, '1m': 1440, 'max': 1440, 'all': 1440 };
        const fidelity = fidelityMap[interval] ?? 60;
        const fetches: Promise<any>[] = [polymarketApiService.getPriceHistory({ token_id: yesId, interval, fidelity })];
        if (noId) fetches.push(polymarketApiService.getPriceHistory({ token_id: noId, interval, fidelity }));
        const [yesH, noH] = await Promise.all(fetches);
        const yesPrices = yesH?.prices ?? [];
        const noPrices  = noH?.prices  ?? [];
        if (yesPrices.length === 0) setPriceChartError('No price history available');
        else { setPriceHistory(yesPrices); if (noPrices.length > 0) setNoPriceHistory(noPrices); }
      } catch { setPriceChartError('Failed to load price history'); }
    } else { setPriceChartError('No token ID available'); }
    setPriceChartLoading(false);
  };

  const handleSearch = async () => {
    if (!searchQuery.trim()) { loadMarkets(); return; }
    try {
      setLoading(true); setError(null);
      const results = await polymarketApiService.search(searchQuery, activeView === 'markets' ? 'markets' : 'events');
      if (activeView === 'markets') {
        const m = results.markets || [];
        setMarkets(m);
        if (m.length === 0) setError(`No active markets found for "${searchQuery}"`);
      } else {
        const e = results.events || [];
        setEvents(e);
        if (e.length === 0) setError(`No active events found for "${searchQuery}"`);
      }
    } catch { setError('Search failed'); }
    finally { setLoading(false); }
  };

  const handleRefresh = () => {
    setCurrentPage(0);
    if (activeView === 'markets') loadMarkets();
    else if (activeView === 'events') loadEvents();
    else if (activeView === 'sports') loadSports();
    else if (activeView === 'resolved') { setResolvedPage(0); loadResolvedEvents(); }
  };

  // ── Derived flags ────────────────────────────────────────────────────────────
  const isBotView       = activeView === 'bots' || activeView === 'bot-deploy';
  const isFullscreen    = activeView === 'portfolio';
  const isResolved      = activeView === 'resolved';
  const showPagination  = !isBotView && activeView !== 'sports' && !isResolved
    && (markets.length >= pageSize || events.length >= pageSize);
  const showResolvedPagination = isResolved && (resolvedEvents.length >= resolvedPageSize || resolvedPage > 0);

  // ── Render ───────────────────────────────────────────────────────────────────
  return (
    <div style={{ height: '100%', backgroundColor: C.bg, color: C.white, fontFamily: C.font, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>

      {/* ── Top bar ──────────────────────────────────────────────────────────── */}
      <div style={{ borderBottom: `2px solid ${C.orange}`, backgroundColor: C.bg, flexShrink: 0 }}>
        {/* Title row */}
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '6px 12px', borderBottom: `1px solid ${C.border}` }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <Activity size={14} style={{ color: C.orange }} />
            <span style={{ fontSize: 12, fontWeight: 'bold', color: C.orange, letterSpacing: '0.5px' }}>POLYMARKET</span>
            <span style={{ fontSize: 9, color: C.faint }}>PREDICTION MARKETS</span>
          </div>
          <button onClick={handleRefresh} disabled={loading}
            style={{ display: 'flex', alignItems: 'center', gap: 5, padding: '4px 10px', backgroundColor: loading ? C.header : C.orange, color: loading ? C.muted : '#000', border: 'none', fontSize: 9, fontWeight: 'bold', cursor: loading ? 'not-allowed' : 'pointer', fontFamily: C.font, borderRadius: 1 }}>
            <RefreshCw size={11} className={loading ? 'animate-spin' : ''} /> REFRESH
          </button>
        </div>

        {/* Nav tabs */}
        <div style={{ display: 'flex', alignItems: 'center', padding: '0 12px', gap: 1 }}>
          {[
            { key: 'markets',   label: 'MARKETS'   },
            { key: 'events',    label: 'EVENTS'     },
            { key: 'sports',    label: 'SPORTS'     },
            { key: 'resolved',  label: 'RESOLVED',  icon: <CheckCircle size={10} style={{ marginRight: 3 }} /> },
            { key: 'portfolio', label: 'PORTFOLIO', icon: <Wallet size={10} style={{ marginRight: 3 }} /> },
            { key: 'bots',      label: 'AI BOTS',   icon: <Bot    size={10} style={{ marginRight: 3 }} /> },
          ].map(({ key, label, icon }) => {
            const isActive = activeView === key || (key === 'bots' && activeView === 'bot-deploy');
            return (
              <button key={key} onClick={() => { setActiveView(key as ActiveView); setCurrentPage(0); }}
                style={{
                  padding: '6px 12px', fontSize: 9, fontWeight: 'bold', fontFamily: C.font,
                  background: isActive ? C.orange : 'transparent',
                  color: isActive ? '#000' : (key === 'bots' || key === 'portfolio') ? C.orange : key === 'resolved' ? C.green : C.muted,
                  border: 'none', cursor: 'pointer',
                  borderBottom: isActive ? `2px solid ${C.orange}` : '2px solid transparent',
                  display: 'flex', alignItems: 'center',
                }}>
                {icon}{label}
              </button>
            );
          })}
        </div>

        {/* Search/filter row — hidden in fullscreen/bot views */}
        {!isFullscreen && !isBotView && !isResolved && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr auto auto auto', gap: 6, padding: '6px 12px' }}>
            <div style={{ display: 'flex', alignItems: 'center', backgroundColor: C.bg, border: `1px solid ${C.border}`, padding: '4px 8px' }}>
              <Search size={11} style={{ color: C.muted, marginRight: 6, flexShrink: 0 }} />
              <input type="text" placeholder="SEARCH MARKETS..." value={searchQuery}
                onChange={e => setSearchQuery(e.target.value)}
                onKeyPress={e => e.key === 'Enter' && handleSearch()}
                style={{ flex: 1, backgroundColor: 'transparent', border: 'none', outline: 'none', fontSize: 10, color: C.white, fontFamily: C.font }} />
            </div>
            <select value={selectedTag} onChange={e => { setSelectedTag(e.target.value); setCurrentPage(0); }}
              style={{ backgroundColor: C.bg, color: C.white, fontSize: 9, fontWeight: 'bold', border: `1px solid ${C.border}`, padding: '4px 8px', outline: 'none', fontFamily: C.font, cursor: 'pointer', borderRadius: 1 }}>
              <option value="">ALL CATEGORIES</option>
              {tags.map(tag => <option key={tag.id} value={tag.id}>{tag.label ? tag.label.toUpperCase() : 'UNKNOWN'}</option>)}
            </select>
            <select value={sortBy} onChange={e => setSortBy(e.target.value as any)}
              style={{ backgroundColor: C.bg, color: C.white, fontSize: 9, fontWeight: 'bold', border: `1px solid ${C.border}`, padding: '4px 8px', outline: 'none', fontFamily: C.font, cursor: 'pointer', borderRadius: 1 }}>
              <option value="volume">VOL</option>
              <option value="liquidity">LIQ</option>
              <option value="recent">RECENT</option>
            </select>
            <label style={{ display: 'flex', alignItems: 'center', gap: 4, padding: '4px 8px', backgroundColor: showClosed ? C.orange : C.bg, color: showClosed ? '#000' : C.muted, border: `1px solid ${C.border}`, cursor: 'pointer', fontSize: 9, fontWeight: 'bold', fontFamily: C.font, borderRadius: 1 }}>
              <input type="checkbox" checked={showClosed} onChange={e => { setShowClosed(e.target.checked); setCurrentPage(0); }} style={{ cursor: 'pointer' }} />
              CLOSED
            </label>
          </div>
        )}
        {/* Resolved search bar */}
        {isResolved && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr auto', gap: 6, padding: '6px 12px' }}>
            <div style={{ display: 'flex', alignItems: 'center', backgroundColor: C.bg, border: `1px solid ${C.border}`, padding: '4px 8px' }}>
              <Search size={11} style={{ color: C.muted, marginRight: 6, flexShrink: 0 }} />
              <input type="text" placeholder="SEARCH RESOLVED EVENTS..." value={resolvedSearchInput}
                onChange={e => setResolvedSearchInput(e.target.value)}
                onKeyPress={e => { if (e.key === 'Enter') { setResolvedSearch(resolvedSearchInput); setResolvedPage(0); } }}
                style={{ flex: 1, backgroundColor: 'transparent', border: 'none', outline: 'none', fontSize: 10, color: C.white, fontFamily: C.font }} />
            </div>
            <button
              onClick={() => { setResolvedSearch(resolvedSearchInput); setResolvedPage(0); }}
              style={{ padding: '4px 12px', backgroundColor: C.green, color: '#000', border: 'none', fontSize: 9, fontWeight: 'bold', cursor: 'pointer', fontFamily: C.font }}>
              SEARCH
            </button>
          </div>
        )}
      </div>

      {/* ── Body ─────────────────────────────────────────────────────────────── */}
      {isFullscreen ? (
        <div style={{ flex: 1, overflow: 'hidden' }}>
          <PortfolioPanel />
        </div>
      ) : (
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
          {/* Left list panel */}
          <div style={{ width: 360, flexShrink: 0, borderRight: `1px solid ${C.border}`, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
            {/* List header */}
            <div style={{ padding: '5px 10px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
              <span style={{ fontSize: 9, fontWeight: 'bold', color: isResolved ? C.green : C.orange, textTransform: 'uppercase', letterSpacing: '0.5px', fontFamily: C.font }}>
                {activeView === 'markets' ? `MARKETS (${markets.length})` : activeView === 'events' ? `EVENTS (${events.length})` : isResolved ? `RESOLVED EVENTS (${resolvedEvents.length})` : 'SPORTS'}
              </span>
            </div>
            {/* Scrollable list */}
            <div style={{ flex: 1, overflow: 'auto' }}>
              <MarketList
                activeView={activeView as 'markets' | 'events' | 'sports' | 'resolved'}
                markets={markets} events={events} sports={sports}
                resolvedEvents={resolvedEvents}
                selectedMarket={selectedMarket} selectedEvent={selectedEvent} selectedSport={selectedSport}
                selectedResolved={selectedResolved}
                livePrices={livePrices} loading={loading} error={error}
                onMarketClick={loadMarketDetails}
                onEventClick={setSelectedEvent}
                onSportClick={s => { setSelectedSport(s); setCurrentPage(0); }}
                onSportBack={() => setSelectedSport(null)}
                onResolvedClick={setSelectedResolved}
                isMarketTradeable={m => polymarketApiService.isMarketTradeable(m)}
              />
            </div>
            {/* Active markets/events pagination */}
            {showPagination && (
              <div style={{ display: 'flex', alignItems: 'center', gap: 1, borderTop: `1px solid ${C.border}`, backgroundColor: C.header, flexShrink: 0 }}>
                <button onClick={() => setCurrentPage(Math.max(0, currentPage - 1))} disabled={currentPage === 0}
                  style={{ flex: 1, padding: '6px 8px', backgroundColor: 'transparent', color: currentPage === 0 ? C.faint : C.white, border: 'none', fontSize: 9, fontWeight: 'bold', cursor: currentPage === 0 ? 'not-allowed' : 'pointer', fontFamily: C.font, borderRight: `1px solid ${C.border}` }}>
                  PREV
                </button>
                <div style={{ padding: '6px 10px', backgroundColor: C.orange, color: '#000', fontSize: 9, fontWeight: 'bold', fontFamily: C.font }}>
                  {currentPage + 1}
                </div>
                <button onClick={() => setCurrentPage(currentPage + 1)}
                  style={{ flex: 1, padding: '6px 8px', backgroundColor: 'transparent', color: C.white, border: 'none', fontSize: 9, fontWeight: 'bold', cursor: 'pointer', fontFamily: C.font, borderLeft: `1px solid ${C.border}` }}>
                  NEXT
                </button>
              </div>
            )}
            {/* Resolved events pagination */}
            {showResolvedPagination && (
              <div style={{ display: 'flex', alignItems: 'center', gap: 1, borderTop: `1px solid ${C.border}`, backgroundColor: C.header, flexShrink: 0 }}>
                <button onClick={() => setResolvedPage(Math.max(0, resolvedPage - 1))} disabled={resolvedPage === 0}
                  style={{ flex: 1, padding: '6px 8px', backgroundColor: 'transparent', color: resolvedPage === 0 ? C.faint : C.white, border: 'none', fontSize: 9, fontWeight: 'bold', cursor: resolvedPage === 0 ? 'not-allowed' : 'pointer', fontFamily: C.font, borderRight: `1px solid ${C.border}` }}>
                  PREV
                </button>
                <div style={{ padding: '6px 10px', backgroundColor: C.green, color: '#000', fontSize: 9, fontWeight: 'bold', fontFamily: C.font }}>
                  {resolvedPage + 1}
                </div>
                <button onClick={() => setResolvedPage(resolvedPage + 1)} disabled={resolvedEvents.length < resolvedPageSize}
                  style={{ flex: 1, padding: '6px 8px', backgroundColor: 'transparent', color: resolvedEvents.length < resolvedPageSize ? C.faint : C.white, border: 'none', fontSize: 9, fontWeight: 'bold', cursor: resolvedEvents.length < resolvedPageSize ? 'not-allowed' : 'pointer', fontFamily: C.font, borderLeft: `1px solid ${C.border}` }}>
                  NEXT
                </button>
              </div>
            )}
          </div>

          {/* Right detail panel */}
          <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
            {activeView === 'bot-deploy' ? (
              <BotDeployView onDeployed={() => setActiveView('bots')} />
            ) : activeView === 'bots' ? (
              <BotMonitorView onCreateNew={() => setActiveView('bot-deploy')} />
            ) : activeView === 'resolved' ? (
              <ResolvedDetail
                event={selectedResolved}
                onClose={() => setSelectedResolved(null)}
              />
            ) : activeView === 'events' ? (
              <EventDetail
                event={selectedEvent}
                livePrices={livePrices}
                onMarketClick={loadMarketDetails}
                onClose={() => setSelectedEvent(null)}
                formatVolume={v => polymarketApiService.formatVolume(String(v))}
              />
            ) : (
              <MarketDetail
                market={selectedMarket}
                livePrices={livePrices}
                wsConnected={wsConnected}
                priceHistory={priceHistory}
                noPriceHistory={noPriceHistory}
                priceChartLoading={priceChartLoading}
                priceChartError={priceChartError}
                chartInterval={chartInterval}
                onIntervalChange={iv => { setChartInterval(iv); if (selectedMarket) loadMarketDetails(selectedMarket, iv); }}
                yesOrderBook={yesOrderBook}
                noOrderBook={noOrderBook}
                orderBookLoading={orderBookLoading}
                orderBookError={orderBookError}
                topHolders={topHolders}
                holdersLoading={holdersLoading}
                holdersError={holdersError}
                recentTrades={recentTrades}
                onClose={() => setSelectedMarket(null)}
              />
            )}
          </div>
        </div>
      )}

      {/* Footer */}
      <TabFooter
        tabName="POLYMARKET"
        leftInfo={[
          { label: 'PREDICTION MARKETS', color: C.orange },
          { label: activeView.toUpperCase(), color: '#a3a3a3' },
          ...(selectedMarket ? [{ label: selectedMarket.question?.slice(0, 40) + (selectedMarket.question?.length > 40 ? '…' : ''), color: '#6b7280' }] : []),
        ]}
        statusInfo={loading ? 'Loading...' : error ?? undefined}
        backgroundColor="#0a0a0a"
        borderColor={C.orange}
      />
    </div>
  );
};

// Inject scrollbar styles once
if (typeof document !== 'undefined') {
  if (!document.getElementById('pm-scrollbar-style')) {
    const s = document.createElement('style');
    s.id = 'pm-scrollbar-style';
    s.textContent = `*::-webkit-scrollbar{width:6px;height:6px}*::-webkit-scrollbar-track{background:#000}*::-webkit-scrollbar-thumb{background:#2A2A2A}*::-webkit-scrollbar-thumb:hover{background:#3A3A3A}`;
    document.head.appendChild(s);
  }
}

export default PolymarketTab;
