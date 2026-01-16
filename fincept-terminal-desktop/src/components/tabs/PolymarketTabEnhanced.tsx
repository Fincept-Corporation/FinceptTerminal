// File: src/components/tabs/PolymarketTab.tsx
// Polymarket prediction markets integration - Bloomberg Terminal Style

import React, { useState, useEffect } from 'react';
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
  PricePoint
} from '@/services/polymarket/polymarketServiceEnhanced';

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

  const loadMarketDetails = async (market: PolymarketMarket) => {
    setSelectedMarket(market);
    setPriceHistory([]); // Reset price history
    setRecentTrades([]); // Reset trades

    // Parse token IDs - they might be a JSON string
    let tokenIds: string[] = [];
    try {
      if (market.clobTokenIds) {
        if (typeof market.clobTokenIds === 'string') {
          tokenIds = JSON.parse(market.clobTokenIds);
        } else if (Array.isArray(market.clobTokenIds)) {
          tokenIds = market.clobTokenIds;
        }
      }
    } catch (error) {
      console.error('Error parsing clobTokenIds:', error);
    }

    // Load data if we have token IDs
    if (tokenIds.length > 0) {
      const tokenId = tokenIds[0]; // Use first token (YES outcome)

      console.log('Loading data for token:', tokenId);

      // Load recent trades
      try {
        const trades = await polymarketService.getTrades({
          token_id: tokenId,
          limit: 50
        });
        setRecentTrades(trades || []);
      } catch (err) {
        console.error('Error loading trades:', err);
      }

      // Load price history - use 'max' interval to get all available data
      try {
        console.log('Fetching price history for token:', tokenId);

        const history = await polymarketServiceEnhanced.getPriceHistory({
          token_id: tokenId,
          interval: 'max', // Get all available history
          fidelity: 60 // 1 hour resolution
        });

        console.log('Price history received:', history?.prices?.length || 0, 'data points');
        setPriceHistory(history?.prices || []);
      } catch (err) {
        console.error('Error loading price history:', err);
      }
    } else {
      console.warn('No token IDs available for this market');
    }
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

  // Render helpers
  const renderMarketCard = (market: PolymarketMarket) => {
    const status = polymarketService.getMarketStatus(market);
    const isTradeable = polymarketService.isMarketTradeable(market);

    // Parse prices - outcomePrices might be a JSON string or an array
    let yesPrice = 0;
    let noPrice = 0;

    try {
      let prices = market.outcomePrices;

      // If it's a string, parse it as JSON
      if (typeof prices === 'string') {
        prices = JSON.parse(prices);
      }

      // Now extract the prices from the array
      if (Array.isArray(prices) && prices.length >= 2) {
        yesPrice = parseFloat(prices[0]) || 0;
        noPrice = parseFloat(prices[1]) || 0;
      }
    } catch (error) {
      console.error('Error parsing outcome prices:', error);
    }

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

        {/* Outcome prices - Bloomberg style */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '8px' }}>
          <div style={{
            backgroundColor: '#1A1A1A',
            border: '1px solid #333333',
            padding: '8px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ color: '#787878', fontSize: '9px', fontWeight: 'bold' }}>YES</span>
              <span style={{
                color: '#00D66F',
                fontSize: '13px',
                fontWeight: 'bold'
              }}>
                {(yesPrice * 100).toFixed(1)}%
              </span>
            </div>
            <div style={{ color: '#787878', fontSize: '9px', marginTop: '2px' }}>
              ${yesPrice.toFixed(4)}
            </div>
          </div>
          <div style={{
            backgroundColor: '#1A1A1A',
            border: '1px solid #333333',
            padding: '8px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ color: '#787878', fontSize: '9px', fontWeight: 'bold' }}>NO</span>
              <span style={{
                color: '#FF3B3B',
                fontSize: '13px',
                fontWeight: 'bold'
              }}>
                {(noPrice * 100).toFixed(1)}%
              </span>
            </div>
            <div style={{ color: '#787878', fontSize: '9px', marginTop: '2px' }}>
              ${noPrice.toFixed(4)}
            </div>
          </div>
        </div>

        {/* Market stats */}
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', fontSize: '10px', color: '#787878' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <div>VOL: {polymarketService.formatVolume(market.volume || '0')}</div>
            {market.liquidity && (
              <div>LIQ: {polymarketService.formatVolume(market.liquidity)}</div>
            )}
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
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

    // Parse prices - outcomePrices might be a JSON string or an array
    let yesPrice = 0;
    let noPrice = 0;

    try {
      let prices = selectedMarket.outcomePrices;

      // If it's a string, parse it as JSON
      if (typeof prices === 'string') {
        prices = JSON.parse(prices);
      }

      // Now extract the prices from the array
      if (Array.isArray(prices) && prices.length >= 2) {
        yesPrice = parseFloat(prices[0]) || 0;
        noPrice = parseFloat(prices[1]) || 0;
      }
    } catch (error) {
      console.error('Error parsing outcome prices in details:', error);
    }

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
            {/* Price Display */}
            <div style={{ backgroundColor: '#0A0A0A', border: '1px solid #333333', padding: '16px', marginBottom: '16px' }}>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginBottom: '20px' }}>
                <div style={{
                  backgroundColor: '#000000',
                  border: '2px solid #333333',
                  padding: '16px'
                }}>
                  <div style={{ color: '#787878', fontSize: '10px', marginBottom: '8px', fontWeight: 'bold' }}>YES</div>
                  <div style={{
                    color: '#00D66F',
                    fontSize: '32px',
                    fontWeight: 'bold',
                    marginBottom: '4px'
                  }}>
                    {(yesPrice * 100).toFixed(1)}%
                  </div>
                  <div style={{ color: '#787878', fontSize: '11px' }}>
                    ${yesPrice.toFixed(4)} per share
                  </div>
                </div>
                <div style={{
                  backgroundColor: '#000000',
                  border: '2px solid #333333',
                  padding: '16px'
                }}>
                  <div style={{ color: '#787878', fontSize: '10px', marginBottom: '8px', fontWeight: 'bold' }}>NO</div>
                  <div style={{
                    color: '#FF3B3B',
                    fontSize: '32px',
                    fontWeight: 'bold',
                    marginBottom: '4px'
                  }}>
                    {(noPrice * 100).toFixed(1)}%
                  </div>
                  <div style={{ color: '#787878', fontSize: '11px' }}>
                    ${noPrice.toFixed(4)} per share
                  </div>
                </div>
              </div>

              {/* Price Chart */}
              <div style={{
                height: '200px',
                border: '1px solid #333333',
                backgroundColor: '#000000',
                position: 'relative'
              }}>
                {priceHistory.length > 1 ? (
                  <svg width="100%" height="200" viewBox="0 0 800 200" preserveAspectRatio="none" style={{ display: 'block' }}>
                    {/* Grid lines */}
                    {[0, 0.25, 0.5, 0.75, 1.0].map((priceLevel, i) => (
                      <line
                        key={i}
                        x1="0"
                        y1={200 - (priceLevel * 200)}
                        x2="800"
                        y2={200 - (priceLevel * 200)}
                        stroke="#333333"
                        strokeWidth="1"
                        strokeDasharray="4,4"
                      />
                    ))}

                    {/* Price line */}
                    <polyline
                      points={priceHistory.map((point, index) => {
                        const x = (index / (priceHistory.length - 1)) * 800;
                        const price = typeof point.price === 'number' ? point.price : parseFloat(point.price);
                        const y = 200 - (price * 200);
                        return `${x},${y}`;
                      }).join(' ')}
                      fill="none"
                      stroke="#00D66F"
                      strokeWidth="2"
                      vectorEffect="non-scaling-stroke"
                    />

                    {/* Price labels on Y axis */}
                    {[0, 0.25, 0.5, 0.75, 1.0].map((price, i) => (
                      <text
                        key={i}
                        x="10"
                        y={200 - (price * 200) + 4}
                        fill="#787878"
                        fontSize="12"
                        fontFamily="IBM Plex Mono, monospace"
                      >
                        ${price.toFixed(2)}
                      </text>
                    ))}
                  </svg>
                ) : (
                  <div style={{
                    height: '100%',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    color: '#787878'
                  }}>
                    <div style={{ textAlign: 'center' }}>
                      <BarChart3 size={48} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
                      <div style={{ fontSize: '11px' }}>Loading price chart...</div>
                    </div>
                  </div>
                )}
              </div>
            </div>

            {/* Market Stats Grid */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(4, 1fr)',
              gap: '1px',
              backgroundColor: '#333333',
              border: '1px solid #333333',
              marginBottom: '16px'
            }}>
              {[
                { label: 'VOLUME', value: polymarketService.formatVolume(selectedMarket.volume || '0') },
                { label: 'LIQUIDITY', value: polymarketService.formatVolume(selectedMarket.liquidity || '0') },
                { label: 'END DATE', value: selectedMarket.endDate ? new Date(selectedMarket.endDate).toLocaleDateString() : 'N/A' },
                {
                  label: 'SPREAD',
                  value: (() => {
                    const spread = selectedMarket.spread;
                    if (spread !== null && spread !== undefined && !isNaN(spread)) {
                      return `${spread.toFixed(2)}%`;
                    }
                    return 'N/A';
                  })()
                },
              ].map(stat => (
                <div key={stat.label} style={{
                  backgroundColor: '#000000',
                  padding: '12px'
                }}>
                  <div style={{ color: '#787878', fontSize: '9px', marginBottom: '4px', fontWeight: 'bold' }}>
                    {stat.label}
                  </div>
                  <div style={{ color: '#FFFFFF', fontSize: '12px', fontWeight: 'bold' }}>
                    {stat.value}
                  </div>
                </div>
              ))}
            </div>

            {/* Recent Trades */}
            {recentTrades.length > 0 && (
              <div style={{ backgroundColor: '#0A0A0A', border: '1px solid #333333', padding: '16px' }}>
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
            {selectedMarket.slug && (
              <div style={{ marginTop: '16px' }}>
                <a
                  href={`https://polymarket.com/event/${selectedMarket.slug}`}
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
