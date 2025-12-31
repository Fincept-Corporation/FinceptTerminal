/**
 * Nifty 100 Tab - Live Quotes for Nifty 100 Stocks
 * Displays real-time quotes from Fyers for all Nifty 100 constituents in card layout
 */

import React, { useState, useEffect } from 'react';
import { TrendingUp, RefreshCw, ArrowUp, ArrowDown } from 'lucide-react';
import { authManager } from '../services/AuthManager';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  INPUT_BG: '#0A0A0A',
  CYAN: '#00E5FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

// Nifty 100 stock symbols
const NIFTY_100_SYMBOLS = [
  'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK', 'HINDUNILVR', 'ITC', 'SBIN', 'BHARTIARTL', 'KOTAKBANK',
  'LT', 'AXISBANK', 'BAJFINANCE', 'ASIANPAINT', 'MARUTI', 'HCLTECH', 'TITAN', 'SUNPHARMA', 'ULTRACEMCO', 'WIPRO',
  'NESTLEIND', 'ONGC', 'NTPC', 'TATAMOTORS', 'TECHM', 'POWERGRID', 'M&M', 'BAJAJFINSV', 'ADANIENT', 'TATASTEEL',
  'COALINDIA', 'JSWSTEEL', 'HINDALCO', 'INDUSINDBK', 'DIVISLAB', 'DRREDDY', 'GRASIM', 'CIPLA', 'EICHERMOT', 'BAJAJ-AUTO',
  'HEROMOTOCO', 'BRITANNIA', 'TATACONSUM', 'SBILIFE', 'ADANIPORTS', 'HDFCLIFE', 'BPCL', 'IOC', 'SHREECEM', 'VEDL',
  'GODREJCP', 'APOLLOHOSP', 'SIEMENS', 'DLF', 'PIDILITIND', 'AMBUJACEM', 'HAVELLS', 'GAIL', 'DABUR', 'UPL',
  'MARICO', 'ADANIGREEN', 'TORNTPHARM', 'BERGEPAINT', 'BANKBARODA', 'HINDZINC', 'BOSCHLTD', 'SRF', 'CANBK', 'INDIGO',
  'ADANITRANS', 'LUPIN', 'PNB', 'MCDOWELL-N', 'ACC', 'COLPAL', 'ICICIPRULI', 'LTIM', 'OFSS', 'MUTHOOTFIN',
  'ABB', 'TATAPOWER', 'GLAND', 'IDEA', 'IRCTC', 'ZOMATO', 'PAYTM', 'NYKAA', 'POLICYBZR', 'DMART'
];

interface StockQuote {
  symbol: string;
  lastPrice: number;
  change: number;
  changePercent: number;
  volume: number;
  open: number;
  high: number;
  low: number;
}

const Nifty100Tab: React.FC = () => {
  const [quotes, setQuotes] = useState<StockQuote[]>([]);
  const [loading, setLoading] = useState(false);
  const [sortConfig, setSortConfig] = useState<{ key: keyof StockQuote; direction: 'asc' | 'desc' } | null>({ key: 'changePercent', direction: 'desc' });
  const [searchTerm, setSearchTerm] = useState('');
  const [progress, setProgress] = useState({ current: 0, total: 0 });

  const fetchAllQuotes = async () => {
    setLoading(true);
    setProgress({ current: 0, total: NIFTY_100_SYMBOLS.length });
    const results: StockQuote[] = [];

    try {
      const adapter = authManager.getAdapter('fyers');
      if (!adapter) throw new Error('Fyers not connected');

      // Fetch quotes sequentially to avoid API rate limits
      for (let i = 0; i < NIFTY_100_SYMBOLS.length; i++) {
        const symbol = NIFTY_100_SYMBOLS[i];
        try {
          const quote = await adapter.getQuote(`${symbol}-EQ`, 'NSE');

          results.push({
            symbol,
            lastPrice: quote.lastPrice || 0,
            change: quote.change || 0,
            changePercent: quote.changePercent || 0,
            volume: quote.volume || 0,
            open: quote.open || 0,
            high: quote.high || 0,
            low: quote.low || 0,
          });
          setProgress({ current: i + 1, total: NIFTY_100_SYMBOLS.length });

          // Update quotes incrementally for better UX
          setQuotes([...results]);

          // Small delay to avoid rate limiting (100ms between requests)
          await new Promise(resolve => setTimeout(resolve, 100));
        } catch (error) {
          console.error(`Failed to fetch quote for ${symbol}:`, error);
        }
      }

      setQuotes(results);
    } catch (error: any) {
      console.error('Failed to fetch quotes:', error);
      alert(error.message);
    } finally {
      setLoading(false);
      setProgress({ current: 0, total: 0 });
    }
  };

  useEffect(() => {
    fetchAllQuotes();
    // Auto-refresh every 60 seconds
    const interval = setInterval(fetchAllQuotes, 60000);
    return () => clearInterval(interval);
  }, []);

  const handleSort = (key: keyof StockQuote) => {
    let direction: 'asc' | 'desc' = 'asc';
    if (sortConfig && sortConfig.key === key && sortConfig.direction === 'asc') {
      direction = 'desc';
    }
    setSortConfig({ key, direction });
  };

  const getSortedQuotes = () => {
    let sorted = [...quotes];

    // Filter by search term
    if (searchTerm) {
      sorted = sorted.filter(q => q.symbol.toLowerCase().includes(searchTerm.toLowerCase()));
    }

    // Sort
    if (sortConfig) {
      sorted.sort((a, b) => {
        const aVal = a[sortConfig.key];
        const bVal = b[sortConfig.key];
        if (aVal < bVal) return sortConfig.direction === 'asc' ? -1 : 1;
        if (aVal > bVal) return sortConfig.direction === 'asc' ? 1 : -1;
        return 0;
      });
    }

    return sorted;
  };

  const sortedQuotes = getSortedQuotes();

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
      color: BLOOMBERG.WHITE,
      fontFamily: 'monospace',
      overflow: 'auto',
      padding: '16px'
    }}>
      {/* Header */}
      <div style={{
        marginBottom: '16px',
        padding: '16px',
        backgroundColor: BLOOMBERG.PANEL_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          marginBottom: '12px'
        }}>
          <TrendingUp size={16} color={BLOOMBERG.CYAN} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: BLOOMBERG.CYAN }}>
            NIFTY 100 LIVE QUOTES
          </span>
          <span style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginLeft: 'auto' }}>
            {quotes.length} / {NIFTY_100_SYMBOLS.length} stocks
          </span>
        </div>

        <div style={{ display: 'flex', gap: '12px', marginBottom: '12px' }}>
          <input
            type="text"
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            placeholder="Search symbol..."
            style={{
              padding: '8px 12px',
              backgroundColor: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.WHITE,
              fontSize: '11px',
              fontWeight: 600,
              fontFamily: 'monospace',
              outline: 'none',
              flex: 1,
            }}
          />
          <button
            onClick={fetchAllQuotes}
            disabled={loading}
            style={{
              padding: '10px 16px',
              backgroundColor: BLOOMBERG.ORANGE,
              border: 'none',
              color: BLOOMBERG.DARK_BG,
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: loading ? 'not-allowed' : 'pointer',
              fontFamily: 'monospace',
              textTransform: 'uppercase',
              opacity: loading ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
          >
            <RefreshCw size={14} />
            {loading ? `${progress.current}/${progress.total}` : 'Refresh'}
          </button>
        </div>

        {/* Sort Options */}
        <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
          <span style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginRight: '4px' }}>SORT BY:</span>
          {[
            { key: 'changePercent' as keyof StockQuote, label: '% CHANGE' },
            { key: 'change' as keyof StockQuote, label: 'CHANGE' },
            { key: 'volume' as keyof StockQuote, label: 'VOLUME' },
            { key: 'lastPrice' as keyof StockQuote, label: 'PRICE' }
          ].map(({ key, label }) => (
            <button
              key={key}
              onClick={() => handleSort(key)}
              style={{
                padding: '4px 8px',
                backgroundColor: sortConfig?.key === key ? BLOOMBERG.ORANGE : BLOOMBERG.INPUT_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                color: sortConfig?.key === key ? BLOOMBERG.DARK_BG : BLOOMBERG.WHITE,
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                fontFamily: 'monospace',
              }}
            >
              {label} {sortConfig?.key === key && (sortConfig.direction === 'asc' ? '▲' : '▼')}
            </button>
          ))}
        </div>
      </div>

      {/* Progress Bar */}
      {loading && progress.total > 0 && (
        <div style={{
          marginBottom: '16px',
          padding: '12px',
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`
        }}>
          <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
            Loading: {progress.current} / {progress.total}
          </div>
          <div style={{
            width: '100%',
            height: '4px',
            backgroundColor: BLOOMBERG.INPUT_BG,
            borderRadius: '2px',
            overflow: 'hidden'
          }}>
            <div style={{
              width: `${(progress.current / progress.total) * 100}%`,
              height: '100%',
              backgroundColor: BLOOMBERG.ORANGE,
              transition: 'width 0.3s'
            }} />
          </div>
        </div>
      )}

      {/* Stock Cards Grid */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
        gap: '12px',
        maxHeight: 'calc(100vh - 300px)',
        overflow: 'auto',
        paddingRight: '4px'
      }}>
        {sortedQuotes.map((quote) => {
          const isPositive = quote.changePercent >= 0;
          return (
            <div
              key={quote.symbol}
              style={{
                padding: '12px',
                backgroundColor: BLOOMBERG.PANEL_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                borderLeft: `3px solid ${isPositive ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
                transition: 'all 0.2s',
                cursor: 'pointer'
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                e.currentTarget.style.transform = 'translateY(-2px)';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = BLOOMBERG.PANEL_BG;
                e.currentTarget.style.transform = 'translateY(0)';
              }}
            >
              {/* Symbol & Change % */}
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                <span style={{ fontSize: '13px', fontWeight: 700, color: BLOOMBERG.WHITE }}>
                  {quote.symbol}
                </span>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  {isPositive ? <ArrowUp size={12} color={BLOOMBERG.GREEN} /> : <ArrowDown size={12} color={BLOOMBERG.RED} />}
                  <span style={{
                    fontSize: '11px',
                    fontWeight: 700,
                    color: isPositive ? BLOOMBERG.GREEN : BLOOMBERG.RED
                  }}>
                    {isPositive ? '+' : ''}{quote.changePercent.toFixed(2)}%
                  </span>
                </div>
              </div>

              {/* Last Price */}
              <div style={{ marginBottom: '8px' }}>
                <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY }}>LAST PRICE</div>
                <div style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.WHITE }}>
                  ₹{quote.lastPrice.toFixed(2)}
                </div>
                <div style={{
                  fontSize: '9px',
                  color: isPositive ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                  fontWeight: 600
                }}>
                  {isPositive ? '+' : ''}{quote.change.toFixed(2)}
                </div>
              </div>

              {/* OHLV */}
              <div style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr',
                gap: '6px',
                paddingTop: '8px',
                borderTop: `1px solid ${BLOOMBERG.BORDER}`
              }}>
                <div>
                  <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>OPEN</div>
                  <div style={{ fontSize: '10px', fontWeight: 600 }}>₹{quote.open.toFixed(2)}</div>
                </div>
                <div>
                  <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>HIGH</div>
                  <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GREEN }}>₹{quote.high.toFixed(2)}</div>
                </div>
                <div>
                  <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>LOW</div>
                  <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.RED }}>₹{quote.low.toFixed(2)}</div>
                </div>
                <div>
                  <div style={{ fontSize: '7px', color: BLOOMBERG.GRAY }}>VOLUME</div>
                  <div style={{ fontSize: '10px', fontWeight: 600 }}>
                    {quote.volume > 0 ? `${(quote.volume / 1000).toFixed(2)}K` : 'N/A'}
                  </div>
                </div>
              </div>
            </div>
          );
        })}
      </div>

      {/* Empty States */}
      {loading && quotes.length === 0 && (
        <div style={{
          padding: '40px',
          textAlign: 'center',
          fontSize: '11px',
          color: BLOOMBERG.GRAY,
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`
        }}>
          Fetching Nifty 100 quotes...
        </div>
      )}

      {!loading && sortedQuotes.length === 0 && quotes.length > 0 && (
        <div style={{
          padding: '40px',
          textAlign: 'center',
          fontSize: '11px',
          color: BLOOMBERG.GRAY,
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`
        }}>
          No stocks found matching "{searchTerm}"
        </div>
      )}
    </div>
  );
};

export default Nifty100Tab;
