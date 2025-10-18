import React, { useState, useEffect } from 'react';
import { polygonService } from '../../services/polygonService';

const COLORS = {
  ORANGE: '#FFA500',
  WHITE: '#FFFFFF',
  RED: '#FF0000',
  GREEN: '#00C800',
  YELLOW: '#FFFF00',
  GRAY: '#787878',
  BLUE: '#6496FA',
  CYAN: '#00FFFF',
  DARK_BG: '#1a1a1a',
  PANEL_BG: '#000000',
  BORDER: '#333333',
};

const PolygonEqTab: React.FC = () => {
  const [symbol, setSymbol] = useState('AAPL');
  const [searchInput, setSearchInput] = useState('');
  const [loading, setLoading] = useState(false);
  const [apiKeyConfigured, setApiKeyConfigured] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // First 5 endpoints data states
  const [tickerDetails, setTickerDetails] = useState<any>(null);
  const [snapshot, setSnapshot] = useState<any>(null);
  const [news, setNews] = useState<any[]>([]);
  const [relatedTickers, setRelatedTickers] = useState<any[]>([]);
  const [marketStatus, setMarketStatus] = useState<any>(null);

  // Next 5 endpoints data states (Phase 2)
  const [dividends, setDividends] = useState<any[]>([]);
  const [splits, setSplits] = useState<any[]>([]);
  const [lastTrade, setLastTrade] = useState<any>(null);
  const [lastQuote, setLastQuote] = useState<any>(null);
  const [smaData, setSmaData] = useState<any>(null);

  // Phase 3 endpoints data states
  const [emaData, setEmaData] = useState<any>(null);
  const [macdData, setMacdData] = useState<any>(null);
  const [rsiData, setRsiData] = useState<any>(null);
  const [incomeStmt, setIncomeStmt] = useState<any>(null);
  const [balanceSheet, setBalanceSheet] = useState<any>(null);

  useEffect(() => {
    const apiKey = polygonService.getApiKey();
    setApiKeyConfigured(!!apiKey);
  }, []);

  const handleConfigureApiKey = () => {
    const key = prompt('Enter your Polygon.io API key:');
    if (key && key.trim()) {
      polygonService.setApiKey(key.trim());
      setApiKeyConfigured(true);
      fetchAllData();
    }
  };

  const fetchAllData = async () => {
    if (!apiKeyConfigured) return;

    console.log('=== FETCHING DATA FOR:', symbol, '===');
    setLoading(true);
    setError(null);

    try {
      // Fetch first 15 endpoints in parallel (Phase 1 + Phase 2 + Phase 3)
      const [
        detailsResp,
        snapResp,
        newsResp,
        relatedResp,
        marketStatusResp,
        dividendsResp,
        splitsResp,
        lastTradeResp,
        lastQuoteResp,
        smaResp,
        emaResp,
        macdResp,
        rsiResp,
        incomeResp,
        balanceResp,
      ] = await Promise.all([
        // Phase 1 endpoints
        polygonService.getTickerDetails(symbol).catch((e) => {
          console.error('Ticker details error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getSingleTickerSnapshot(symbol).catch((e) => {
          console.error('Snapshot error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getNews({ ticker: symbol, limit: 10 }).catch((e) => {
          console.error('News error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getRelatedTickers(symbol).catch((e) => {
          console.error('Related tickers error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getMarketStatus().catch((e) => {
          console.error('Market status error:', e);
          return { success: false, error: e.message };
        }),
        // Phase 2 endpoints
        polygonService.getDividends({ ticker: symbol, limit: 10 }).catch((e) => {
          console.error('Dividends error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getSplits({ ticker: symbol, limit: 10 }).catch((e) => {
          console.error('Splits error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getLastTrade(symbol).catch((e) => {
          console.error('Last trade error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getLastQuote(symbol).catch((e) => {
          console.error('Last quote error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getSMA(symbol, { window: 20, timespan: 'day', limit: 50 }).catch((e) => {
          console.error('SMA error:', e);
          return { success: false, error: e.message };
        }),
        // Phase 3 endpoints
        polygonService.getEMA(symbol, { window: 20, timespan: 'day', limit: 50 }).catch((e) => {
          console.error('EMA error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getMACD(symbol, { shortWindow: 12, longWindow: 26, signalWindow: 9, timespan: 'day', limit: 50 }).catch((e) => {
          console.error('MACD error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getRSI(symbol, { window: 14, timespan: 'day', limit: 50 }).catch((e) => {
          console.error('RSI error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getIncomeStatements(symbol, { timeframe: 'annual', limit: 5 }).catch((e) => {
          console.error('Income statements error:', e);
          return { success: false, error: e.message };
        }),
        polygonService.getBalanceSheets(symbol, { timeframe: 'annual', limit: 5 }).catch((e) => {
          console.error('Balance sheets error:', e);
          return { success: false, error: e.message };
        }),
      ]);

      // Parse Ticker Details
      if (detailsResp?.success && detailsResp?.ticker_details) {
        setTickerDetails(detailsResp.ticker_details);
      }

      // Parse Snapshot
      if (snapResp?.success && snapResp?.snapshot) {
        setSnapshot(snapResp.snapshot);
      }

      // Parse News
      if (newsResp?.success && newsResp?.news_articles) {
        setNews(newsResp.news_articles);
      } else {
        setNews([]);
      }

      // Parse Related Tickers
      if (relatedResp?.success && relatedResp?.related_tickers) {
        // Convert string array to object array with ticker property
        const tickersArray = relatedResp.related_tickers.map((ticker: any) => {
          if (typeof ticker === 'string') {
            return { ticker: ticker, name: null };
          }
          return ticker;
        });
        setRelatedTickers(tickersArray);
      } else {
        setRelatedTickers([]);
      }

      // Parse Market Status
      if (marketStatusResp?.success && marketStatusResp?.market_status) {
        setMarketStatus(marketStatusResp.market_status);
      }

      // Parse Dividends
      if (dividendsResp?.success && dividendsResp?.dividends) {
        setDividends(dividendsResp.dividends);
      } else {
        setDividends([]);
      }

      // Parse Splits
      if (splitsResp?.success && splitsResp?.splits) {
        setSplits(splitsResp.splits);
      } else {
        setSplits([]);
      }

      // Parse Last Trade
      if (lastTradeResp?.success && lastTradeResp?.last_trade) {
        setLastTrade(lastTradeResp.last_trade);
      }

      // Parse Last Quote
      if (lastQuoteResp?.success && lastQuoteResp?.last_quote) {
        setLastQuote(lastQuoteResp.last_quote);
      }

      // Parse SMA
      if (smaResp?.success && smaResp?.sma_data) {
        setSmaData(smaResp.sma_data);
      }

      // Parse EMA
      if (emaResp?.success && emaResp?.ema_data) {
        setEmaData(emaResp.ema_data);
      }

      // Parse MACD - DEBUG ONLY
      console.log('=== MACD DEBUG ===');
      console.log('MACD Response:', JSON.stringify(macdResp, null, 2));
      if (macdResp?.success && macdResp?.macd_data) {
        console.log('MACD data structure:', JSON.stringify(macdResp.macd_data, null, 2));
        console.log('MACD statistics:', macdResp.macd_data.statistics);
        setMacdData(macdResp.macd_data);
      } else {
        console.error('MACD failed:', macdResp?.error);
      }

      // Parse RSI
      if (rsiResp?.success && rsiResp?.rsi_data) {
        setRsiData(rsiResp.rsi_data);
      }

      // Parse Income Statements - DEBUG ONLY
      console.log('=== INCOME STATEMENTS DEBUG ===');
      console.log('Income Response:', JSON.stringify(incomeResp, null, 2));
      if (incomeResp?.success && incomeResp?.income_statement_data) {
        console.log('Income statement structure:', JSON.stringify(incomeResp.income_statement_data, null, 2));
        setIncomeStmt(incomeResp.income_statement_data);
      } else {
        console.error('Income statements failed:', incomeResp?.error);
      }

      // Parse Balance Sheets - DEBUG ONLY
      console.log('=== BALANCE SHEETS DEBUG ===');
      console.log('Balance Response:', JSON.stringify(balanceResp, null, 2));
      if (balanceResp?.success && balanceResp?.balance_sheet_data) {
        console.log('Balance sheet structure:', JSON.stringify(balanceResp.balance_sheet_data, null, 2));
        setBalanceSheet(balanceResp.balance_sheet_data);
      } else {
        console.error('Balance sheets failed:', balanceResp?.error);
      }

      console.log('=== DATA FETCH COMPLETE ===');

    } catch (error: any) {
      console.error('Critical error:', error);
      setError(error.message || 'Failed to fetch data');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    if (apiKeyConfigured) {
      fetchAllData();
    }
  }, [symbol, apiKeyConfigured]);

  const handleSearch = () => {
    if (searchInput.trim()) {
      setSymbol(searchInput.trim().toUpperCase());
      setSearchInput('');
    }
  };

  const handleRelatedTickerClick = (ticker: string) => {
    console.log('Clicked related ticker:', ticker);
    setSymbol(ticker);
  };

  const formatNumber = (num?: number | null, decimals = 2) => {
    if (!num && num !== 0) return 'N/A';
    return num.toLocaleString(undefined, { minimumFractionDigits: decimals, maximumFractionDigits: decimals });
  };

  const formatLargeNumber = (num?: number | null) => {
    if (!num && num !== 0) return 'N/A';
    if (num >= 1e12) return `$${(num / 1e12).toFixed(2)}T`;
    if (num >= 1e9) return `$${(num / 1e9).toFixed(2)}B`;
    if (num >= 1e6) return `$${(num / 1e6).toFixed(2)}M`;
    return `$${num.toFixed(2)}`;
  };

  if (!apiKeyConfigured) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: COLORS.DARK_BG,
        color: COLORS.WHITE,
        fontFamily: 'Consolas, monospace',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
      }}>
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: `2px solid ${COLORS.ORANGE}`,
          padding: '40px',
          maxWidth: '500px',
          textAlign: 'center',
        }}>
          <div style={{ color: COLORS.ORANGE, fontSize: '24px', fontWeight: 'bold', marginBottom: '20px' }}>
            POLYGON.IO API KEY REQUIRED
          </div>
          <div style={{ color: COLORS.GRAY, fontSize: '14px', marginBottom: '20px' }}>
            Configure your Polygon.io API key to access institutional-grade financial data.
            <br /><br />
            Get your API key at: <span style={{ color: COLORS.CYAN }}>polygon.io</span>
          </div>
          <button
            onClick={handleConfigureApiKey}
            style={{
              backgroundColor: COLORS.ORANGE,
              border: 'none',
              color: COLORS.DARK_BG,
              padding: '12px 30px',
              fontSize: '14px',
              cursor: 'pointer',
              fontWeight: 'bold',
            }}
          >
            CONFIGURE API KEY
          </button>
        </div>
      </div>
    );
  }

  // Calculate price change - use todays_change if available, otherwise calculate from prev_day
  const currentPrice = snapshot?.day?.close || snapshot?.prev_day?.close || 0;
  const prevClose = snapshot?.prev_day?.close || 0;

  let priceChange = snapshot?.todays_change || 0;
  let priceChangePercent = snapshot?.todays_change_perc || 0;

  // If todays_change is 0 but we have current and previous prices, calculate manually
  if (priceChange === 0 && currentPrice > 0 && prevClose > 0) {
    priceChange = currentPrice - prevClose;
    priceChangePercent = ((priceChange / prevClose) * 100);
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: COLORS.DARK_BG,
      color: COLORS.WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '13px',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: `1px solid ${COLORS.BORDER}`,
        padding: '10px 15px',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '15px' }}>
            <span style={{ color: COLORS.ORANGE, fontWeight: 'bold', fontSize: '16px' }}>POLYGON.IO TERMINAL</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <input
              type="text"
              value={searchInput}
              onChange={(e) => setSearchInput(e.target.value)}
              onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
              placeholder="Enter symbol..."
              style={{
                backgroundColor: COLORS.DARK_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                padding: '6px 12px',
                fontSize: '13px',
                width: '180px',
                fontFamily: 'Consolas, monospace',
              }}
            />
            <button
              onClick={handleSearch}
              style={{
                backgroundColor: COLORS.ORANGE,
                border: 'none',
                color: COLORS.DARK_BG,
                padding: '6px 18px',
                fontSize: '13px',
                cursor: 'pointer',
                fontWeight: 'bold',
              }}
            >
              SEARCH
            </button>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '15px', fontSize: '13px' }}>
            <span style={{ color: COLORS.CYAN, fontWeight: 'bold', fontSize: '18px' }}>{symbol}</span>
            {loading && <span style={{ color: COLORS.YELLOW }}>● LOADING...</span>}
            {!loading && !error && <span style={{ color: COLORS.GREEN }}>● LIVE</span>}
            {!loading && error && <span style={{ color: COLORS.RED }}>● ERROR</span>}
            <button
              onClick={() => fetchAllData()}
              style={{
                backgroundColor: COLORS.BLUE,
                border: 'none',
                color: COLORS.WHITE,
                padding: '5px 15px',
                fontSize: '12px',
                cursor: 'pointer',
                fontWeight: 'bold',
              }}
            >
              REFRESH
            </button>
          </div>
        </div>
      </div>

      {/* Error Banner */}
      {error && (
        <div style={{
          backgroundColor: '#331111',
          border: `1px solid ${COLORS.RED}`,
          padding: '10px 15px',
          margin: '10px',
          color: COLORS.RED,
          fontSize: '13px',
          flexShrink: 0,
        }}>
          <strong>ERROR:</strong> {error}
        </div>
      )}

      {/* Price Header */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        padding: '20px',
        margin: '10px',
        marginBottom: '0',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '20px', marginBottom: '8px' }}>
              <span style={{ color: COLORS.ORANGE, fontSize: '28px', fontWeight: 'bold' }}>{symbol}</span>
              <span style={{ color: COLORS.WHITE, fontSize: '18px' }}>{tickerDetails?.name || 'Loading...'}</span>
              {tickerDetails?.type && (
                <span style={{
                  color: COLORS.BLUE,
                  fontSize: '12px',
                  padding: '4px 10px',
                  backgroundColor: 'rgba(100,150,250,0.2)',
                  border: `1px solid ${COLORS.BLUE}`,
                }}>
                  {tickerDetails.type}
                </span>
              )}
            </div>
            <div style={{ fontSize: '13px', color: COLORS.GRAY }}>
              {tickerDetails?.primary_exchange || 'N/A'} | {tickerDetails?.market || 'N/A'} | Market Cap: {formatLargeNumber(tickerDetails?.market_cap)}
            </div>
          </div>
          <div style={{ textAlign: 'right' }}>
            <div style={{ fontSize: '32px', fontWeight: 'bold', color: COLORS.WHITE, marginBottom: '5px' }}>
              ${formatNumber(currentPrice)}
            </div>
            <div style={{ fontSize: '16px', color: priceChange >= 0 ? COLORS.GREEN : COLORS.RED }}>
              {priceChange >= 0 ? '▲' : '▼'} ${Math.abs(priceChange).toFixed(2)} ({priceChangePercent.toFixed(2)}%)
            </div>
          </div>
        </div>
      </div>

      {/* Main Content - Single Screen Layout */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '10px',
        paddingTop: '0',
      }} className="custom-scrollbar">

        {/* Row 1: Today's Trading + Market Status + Related Tickers */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '10px', marginBottom: '10px' }}>

          {/* Today's Trading */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              TODAY'S TRADING DATA
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '15px', fontSize: '13px' }}>
              <div>
                <span style={{ color: COLORS.GRAY }}>OPEN:</span>
                <div style={{ color: COLORS.CYAN, fontSize: '18px', fontWeight: 'bold' }}>${formatNumber(snapshot?.day?.open)}</div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>HIGH:</span>
                <div style={{ color: COLORS.GREEN, fontSize: '18px', fontWeight: 'bold' }}>${formatNumber(snapshot?.day?.high)}</div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>LOW:</span>
                <div style={{ color: COLORS.RED, fontSize: '18px', fontWeight: 'bold' }}>${formatNumber(snapshot?.day?.low)}</div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>VOLUME:</span>
                <div style={{ color: COLORS.YELLOW, fontSize: '18px', fontWeight: 'bold' }}>{formatNumber(snapshot?.day?.volume, 0)}</div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>VWAP:</span>
                <div style={{ color: COLORS.CYAN, fontSize: '18px', fontWeight: 'bold' }}>${formatNumber(snapshot?.day?.vwap)}</div>
              </div>
            </div>
          </div>

          {/* Market Status */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              MARKET STATUS
            </div>
            <div style={{ fontSize: '13px', display: 'flex', flexDirection: 'column', gap: '15px' }}>
              <div>
                <span style={{ color: COLORS.GRAY }}>Market:</span>
                <div style={{
                  color: marketStatus?.market === 'open' ? COLORS.GREEN : COLORS.RED,
                  fontWeight: 'bold',
                  fontSize: '20px',
                  marginTop: '5px'
                }}>
                  {marketStatus?.market?.toUpperCase() || 'N/A'}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Server Time:</span>
                <div style={{ color: COLORS.CYAN, fontSize: '15px', marginTop: '5px' }}>
                  {marketStatus?.serverTime || 'N/A'}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Exchanges:</span>
                <div style={{ color: COLORS.BLUE, fontSize: '15px', marginTop: '5px' }}>
                  {marketStatus?.exchanges ? Object.keys(marketStatus.exchanges).length : 'N/A'}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Currencies:</span>
                <div style={{ color: COLORS.YELLOW, fontSize: '15px', marginTop: '5px' }}>
                  {marketStatus?.currencies ? Object.keys(marketStatus.currencies).length : 'N/A'}
                </div>
              </div>
            </div>
          </div>

          {/* Related Tickers */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.CYAN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              RELATED TICKERS ({relatedTickers.length})
            </div>
            <div style={{ maxHeight: '300px', overflow: 'auto' }} className="custom-scrollbar">
              {relatedTickers.length > 0 ? relatedTickers.map((ticker: any, idx: number) => (
                <div
                  key={idx}
                  onClick={() => handleRelatedTickerClick(ticker.ticker)}
                  style={{
                    padding: '10px',
                    borderBottom: `1px solid ${COLORS.BORDER}`,
                    cursor: 'pointer',
                    transition: 'background-color 0.2s',
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = COLORS.BORDER}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                >
                  <div style={{ color: COLORS.CYAN, fontSize: '14px', fontWeight: 'bold', marginBottom: '3px' }}>
                    {ticker.ticker}
                  </div>
                  {ticker.name && (
                    <div style={{ color: COLORS.GRAY, fontSize: '12px' }}>
                      {ticker.name}
                    </div>
                  )}
                </div>
              )) : (
                <div style={{ color: COLORS.GRAY, fontSize: '13px', padding: '15px', textAlign: 'center' }}>
                  No related tickers available
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Row 2: Latest News */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '10px', marginBottom: '10px' }}>
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              LATEST NEWS - {symbol}
            </div>
            <div style={{ maxHeight: '400px', overflow: 'auto' }} className="custom-scrollbar">
              {news.length > 0 ? news.map((article: any) => (
                <div
                  key={article.id}
                  style={{
                    marginBottom: '15px',
                    paddingBottom: '15px',
                    borderBottom: `1px solid ${COLORS.BORDER}`,
                  }}
                >
                  <div style={{ display: 'flex', gap: '15px' }}>
                    {article.image_url && (
                      <img
                        src={article.image_url}
                        alt=""
                        style={{ width: '100px', height: '75px', objectFit: 'cover', borderRadius: '4px' }}
                      />
                    )}
                    <div style={{ flex: 1 }}>
                      <div style={{ color: COLORS.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '6px' }}>
                        {article.title}
                      </div>
                      <div style={{ color: COLORS.GRAY, fontSize: '12px', marginBottom: '6px' }}>
                        {article.publisher?.name} | {new Date(article.published_utc).toLocaleString()}
                      </div>
                      {article.description && (
                        <div style={{ color: COLORS.WHITE, fontSize: '12px', marginBottom: '6px', opacity: 0.8 }}>
                          {article.description.substring(0, 150)}...
                        </div>
                      )}
                      <a
                        href={article.article_url}
                        target="_blank"
                        rel="noopener noreferrer"
                        style={{ color: COLORS.CYAN, fontSize: '12px', textDecoration: 'underline' }}
                      >
                        READ MORE →
                      </a>
                    </div>
                  </div>
                </div>
              )) : (
                <div style={{ color: COLORS.GRAY, fontSize: '13px', padding: '15px', textAlign: 'center' }}>
                  No news available
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Row 3: Last Trade + Last Quote + SMA */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '10px', marginBottom: '10px' }}>

          {/* Last Trade */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.BLUE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              LAST TRADE
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', fontSize: '13px' }}>
              <div>
                <span style={{ color: COLORS.GRAY }}>Price:</span>
                <div style={{ color: COLORS.CYAN, fontSize: '20px', fontWeight: 'bold' }}>
                  ${formatNumber(lastTrade?.price)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Size:</span>
                <div style={{ color: COLORS.YELLOW, fontSize: '16px', fontWeight: 'bold' }}>
                  {formatNumber(lastTrade?.size, 0)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Exchange:</span>
                <div style={{ color: COLORS.WHITE, fontSize: '14px' }}>
                  {lastTrade?.tape_description || 'N/A'}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Time:</span>
                <div style={{ color: COLORS.GRAY, fontSize: '12px' }}>
                  {lastTrade?.trade_time_formatted || 'N/A'}
                </div>
              </div>
            </div>
          </div>

          {/* Last Quote */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              LAST QUOTE
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', fontSize: '13px' }}>
              <div>
                <span style={{ color: COLORS.GRAY }}>Bid:</span>
                <div style={{ color: COLORS.GREEN, fontSize: '18px', fontWeight: 'bold' }}>
                  ${formatNumber(lastQuote?.bid_price)}
                  <span style={{ color: COLORS.GRAY, fontSize: '12px', marginLeft: '8px' }}>
                    x{formatNumber(lastQuote?.bid_size, 0)}
                  </span>
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Ask:</span>
                <div style={{ color: COLORS.RED, fontSize: '18px', fontWeight: 'bold' }}>
                  ${formatNumber(lastQuote?.ask_price)}
                  <span style={{ color: COLORS.GRAY, fontSize: '12px', marginLeft: '8px' }}>
                    x{formatNumber(lastQuote?.ask_size, 0)}
                  </span>
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Spread:</span>
                <div style={{ color: COLORS.YELLOW, fontSize: '16px', fontWeight: 'bold' }}>
                  ${formatNumber((lastQuote?.ask_price || 0) - (lastQuote?.bid_price || 0), 3)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Time:</span>
                <div style={{ color: COLORS.GRAY, fontSize: '12px' }}>
                  {lastQuote?.quote_time_formatted || 'N/A'}
                </div>
              </div>
            </div>
          </div>

          {/* SMA Indicator */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.CYAN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              SMA (20-DAY)
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', fontSize: '13px' }}>
              <div>
                <span style={{ color: COLORS.GRAY }}>Latest Value:</span>
                <div style={{ color: COLORS.CYAN, fontSize: '20px', fontWeight: 'bold' }}>
                  ${formatNumber(smaData?.statistics?.latest_value)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Min:</span>
                <div style={{ color: COLORS.RED, fontSize: '16px', fontWeight: 'bold' }}>
                  ${formatNumber(smaData?.statistics?.min_value)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Max:</span>
                <div style={{ color: COLORS.GREEN, fontSize: '16px', fontWeight: 'bold' }}>
                  ${formatNumber(smaData?.statistics?.max_value)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Average:</span>
                <div style={{ color: COLORS.YELLOW, fontSize: '14px' }}>
                  ${formatNumber(smaData?.statistics?.avg_value)}
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* Row 4: EMA + MACD + RSI */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '10px', marginBottom: '10px' }}>

          {/* EMA Indicator */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.BLUE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              EMA (20-DAY)
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', fontSize: '13px' }}>
              <div>
                <span style={{ color: COLORS.GRAY }}>Latest Value:</span>
                <div style={{ color: COLORS.BLUE, fontSize: '20px', fontWeight: 'bold' }}>
                  ${formatNumber(emaData?.statistics?.latest_value)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Min:</span>
                <div style={{ color: COLORS.RED, fontSize: '16px', fontWeight: 'bold' }}>
                  ${formatNumber(emaData?.statistics?.min_value)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Max:</span>
                <div style={{ color: COLORS.GREEN, fontSize: '16px', fontWeight: 'bold' }}>
                  ${formatNumber(emaData?.statistics?.max_value)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Average:</span>
                <div style={{ color: COLORS.YELLOW, fontSize: '14px' }}>
                  ${formatNumber(emaData?.statistics?.avg_value)}
                </div>
              </div>
            </div>
          </div>

          {/* MACD Indicator */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              MACD (12,26,9)
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', fontSize: '13px' }}>
              <div>
                <span style={{ color: COLORS.GRAY }}>Latest MACD:</span>
                <div style={{ color: COLORS.GREEN, fontSize: '18px', fontWeight: 'bold' }}>
                  {formatNumber(macdData?.macd_values?.[0]?.macd_value)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Signal:</span>
                <div style={{ color: COLORS.ORANGE, fontSize: '16px', fontWeight: 'bold' }}>
                  {formatNumber(macdData?.macd_values?.[0]?.signal_line)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Histogram:</span>
                <div style={{ color: macdData?.macd_values?.[0]?.histogram >= 0 ? COLORS.GREEN : COLORS.RED, fontSize: '16px', fontWeight: 'bold' }}>
                  {formatNumber(macdData?.macd_values?.[0]?.histogram)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Signal:</span>
                <div style={{
                  color: macdData?.macd_values?.[0]?.crossover_analysis?.macd_above_signal ? COLORS.GREEN : COLORS.RED,
                  fontSize: '14px',
                  fontWeight: 'bold'
                }}>
                  {macdData?.macd_values?.[0]?.crossover_analysis?.macd_above_signal ? 'BULLISH' : 'BEARISH'}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Date:</span>
                <div style={{ color: COLORS.GRAY, fontSize: '12px' }}>
                  {macdData?.macd_values?.[0]?.date || 'N/A'}
                </div>
              </div>
            </div>
          </div>

          {/* RSI Indicator */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              RSI (14-DAY)
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', fontSize: '13px' }}>
              <div>
                <span style={{ color: COLORS.GRAY }}>Latest RSI:</span>
                <div style={{
                  color: (rsiData?.statistics?.rsi_stats?.latest_value || 0) > 70 ? COLORS.RED :
                         (rsiData?.statistics?.rsi_stats?.latest_value || 0) < 30 ? COLORS.GREEN : COLORS.YELLOW,
                  fontSize: '24px',
                  fontWeight: 'bold'
                }}>
                  {formatNumber(rsiData?.statistics?.rsi_stats?.latest_value, 1)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Signal:</span>
                <div style={{
                  color: (rsiData?.statistics?.rsi_stats?.latest_value || 0) > 70 ? COLORS.RED :
                         (rsiData?.statistics?.rsi_stats?.latest_value || 0) < 30 ? COLORS.GREEN : COLORS.CYAN,
                  fontSize: '16px',
                  fontWeight: 'bold'
                }}>
                  {(rsiData?.statistics?.rsi_stats?.latest_value || 0) > 70 ? 'OVERBOUGHT' :
                   (rsiData?.statistics?.rsi_stats?.latest_value || 0) < 30 ? 'OVERSOLD' : 'NEUTRAL'}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Min:</span>
                <div style={{ color: COLORS.GREEN, fontSize: '14px' }}>
                  {formatNumber(rsiData?.statistics?.rsi_stats?.min_value, 1)}
                </div>
              </div>
              <div>
                <span style={{ color: COLORS.GRAY }}>Max:</span>
                <div style={{ color: COLORS.RED, fontSize: '14px' }}>
                  {formatNumber(rsiData?.statistics?.rsi_stats?.max_value, 1)}
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* Row 5: Income Statements (Full Width) */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '10px', marginBottom: '10px' }}>

          {/* Income Statements - Enhanced */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              INCOME STATEMENTS (ANNUAL) - DETAILED ANALYSIS
            </div>
            <div style={{ maxHeight: '400px', overflow: 'auto' }} className="custom-scrollbar">
              {incomeStmt?.results && incomeStmt.results.length > 0 ? (
                <div>
                  {incomeStmt.results.slice(0, 2).map((r: any, i: number) => (
                    <div key={i} style={{ marginBottom: '20px', paddingBottom: '20px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                      {/* Header */}
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '15px' }}>
                        <div>
                          <span style={{ color: COLORS.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>FY {r.fiscal_year}</span>
                          <span style={{ color: COLORS.GRAY, fontSize: '14px', marginLeft: '10px' }}>Period: {r.period_end}</span>
                          {r.profitability_health && (
                            <span style={{
                              color: r.profitability_health.health_rating === 'Good' || r.profitability_health.health_rating === 'Excellent' ? COLORS.GREEN : COLORS.YELLOW,
                              fontSize: '14px',
                              marginLeft: '10px',
                              padding: '3px 10px',
                              backgroundColor: 'rgba(0,200,0,0.1)',
                              border: `1px solid ${COLORS.GREEN}`,
                            }}>
                              {r.profitability_health.health_rating} ({r.profitability_health.health_score}/100)
                            </span>
                          )}
                        </div>
                        {r.business_model_analysis && (
                          <span style={{ color: COLORS.CYAN, fontSize: '14px' }}>
                            {r.business_model_analysis.model_type}
                          </span>
                        )}
                      </div>

                      {/* Core Financials */}
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '15px', marginBottom: '15px' }}>
                        <div>
                          <span style={{ color: COLORS.GRAY, fontSize: '13px' }}>Revenue</span>
                          <div style={{ color: COLORS.GREEN, fontSize: '20px', fontWeight: 'bold' }}>
                            {formatLargeNumber(r.revenue)}
                          </div>
                          <div style={{ color: COLORS.GRAY, fontSize: '12px' }}>
                            EPS: ${formatNumber(r.diluted_earnings_per_share)}
                          </div>
                        </div>
                        <div>
                          <span style={{ color: COLORS.GRAY, fontSize: '13px' }}>Gross Profit</span>
                          <div style={{ color: COLORS.CYAN, fontSize: '20px', fontWeight: 'bold' }}>
                            {formatLargeNumber(r.gross_profit)}
                          </div>
                          <div style={{ color: COLORS.GREEN, fontSize: '12px' }}>
                            Margin: {formatNumber(r.profitability_ratios?.margin_analysis?.gross_profit_margin * 100, 1)}%
                          </div>
                        </div>
                        <div>
                          <span style={{ color: COLORS.GRAY, fontSize: '13px' }}>Operating Income</span>
                          <div style={{ color: COLORS.BLUE, fontSize: '20px', fontWeight: 'bold' }}>
                            {formatLargeNumber(r.operating_income)}
                          </div>
                          <div style={{ color: COLORS.GREEN, fontSize: '12px' }}>
                            Margin: {formatNumber(r.profitability_ratios?.margin_analysis?.operating_profit_margin * 100, 1)}%
                          </div>
                        </div>
                        <div>
                          <span style={{ color: COLORS.GRAY, fontSize: '13px' }}>Net Income</span>
                          <div style={{ color: r.consolidated_net_income_loss >= 0 ? COLORS.GREEN : COLORS.RED, fontSize: '20px', fontWeight: 'bold' }}>
                            {formatLargeNumber(r.consolidated_net_income_loss)}
                          </div>
                          <div style={{ color: COLORS.GREEN, fontSize: '12px' }}>
                            Margin: {formatNumber(r.profitability_ratios?.margin_analysis?.net_profit_margin * 100, 1)}%
                          </div>
                        </div>
                      </div>

                      {/* Additional Metrics */}
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '10px', fontSize: '13px' }}>
                        <div>
                          <span style={{ color: COLORS.GRAY }}>EBITDA:</span>
                          <span style={{ color: COLORS.YELLOW, marginLeft: '5px', fontWeight: 'bold' }}>
                            {formatLargeNumber(r.ebitda)}
                          </span>
                        </div>
                        <div>
                          <span style={{ color: COLORS.GRAY }}>R&D:</span>
                          <span style={{ color: COLORS.ORANGE, marginLeft: '5px', fontWeight: 'bold' }}>
                            {formatLargeNumber(r.research_development)}
                          </span>
                        </div>
                        <div>
                          <span style={{ color: COLORS.GRAY }}>Tax Rate:</span>
                          <span style={{ color: COLORS.CYAN, marginLeft: '5px', fontWeight: 'bold' }}>
                            {formatNumber(r.profitability_ratios?.financial_metrics?.effective_tax_rate * 100, 1)}%
                          </span>
                        </div>
                        <div>
                          <span style={{ color: COLORS.GRAY }}>Shares:</span>
                          <span style={{ color: COLORS.WHITE, marginLeft: '5px', fontWeight: 'bold' }}>
                            {formatNumber(r.diluted_shares_outstanding / 1e9, 2)}B
                          </span>
                        </div>
                      </div>

                      {/* Health Factors */}
                      {r.profitability_health?.health_factors && (
                        <div style={{ marginTop: '10px' }}>
                          <div style={{ color: COLORS.GRAY, fontSize: '12px', marginBottom: '5px' }}>Health Factors:</div>
                          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '5px' }}>
                            {r.profitability_health.health_factors.slice(0, 4).map((factor: string, idx: number) => (
                              <span key={idx} style={{
                                color: COLORS.CYAN,
                                fontSize: '11px',
                                padding: '3px 8px',
                                backgroundColor: 'rgba(0,255,255,0.1)',
                                border: `1px solid ${COLORS.CYAN}`,
                              }}>
                                {factor}
                              </span>
                            ))}
                          </div>
                        </div>
                      )}
                    </div>
                  ))}
                </div>
              ) : (
                <div style={{ color: COLORS.GRAY, fontSize: '13px', padding: '15px', textAlign: 'center' }}>
                  No income statement data available
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Row 6: Balance Sheets (Full Width) */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '10px', marginBottom: '10px' }}>

          {/* Balance Sheets - Enhanced */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.BLUE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              BALANCE SHEETS (ANNUAL) - DETAILED ANALYSIS
            </div>
            <div style={{ maxHeight: '400px', overflow: 'auto' }} className="custom-scrollbar">
              {balanceSheet?.results && balanceSheet.results.length > 0 ? (
                <div>
                  {balanceSheet.results.slice(0, 2).map((r: any, i: number) => (
                    <div key={i} style={{ marginBottom: '20px', paddingBottom: '20px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                      {/* Header */}
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '15px' }}>
                        <div>
                          <span style={{ color: COLORS.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>FY {r.fiscal_year}</span>
                          <span style={{ color: COLORS.GRAY, fontSize: '14px', marginLeft: '10px' }}>Period: {r.period_end}</span>
                          {r.financial_health && (
                            <span style={{
                              color: r.financial_health.health_rating === 'Good' || r.financial_health.health_rating === 'Excellent' ? COLORS.GREEN :
                                     r.financial_health.health_rating === 'Poor' ? COLORS.RED : COLORS.YELLOW,
                              fontSize: '14px',
                              marginLeft: '10px',
                              padding: '3px 10px',
                              backgroundColor: r.financial_health.health_rating === 'Poor' ? 'rgba(255,0,0,0.1)' : 'rgba(255,165,0,0.1)',
                              border: `1px solid ${r.financial_health.health_rating === 'Poor' ? COLORS.RED : COLORS.ORANGE}`,
                            }}>
                              {r.financial_health.health_rating} ({r.financial_health.health_score}/100)
                            </span>
                          )}
                        </div>
                      </div>

                      {/* Core Balance Sheet */}
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '15px', marginBottom: '15px' }}>
                        <div>
                          <span style={{ color: COLORS.GRAY, fontSize: '13px' }}>Total Assets</span>
                          <div style={{ color: COLORS.GREEN, fontSize: '20px', fontWeight: 'bold' }}>
                            {formatLargeNumber(r.total_assets)}
                          </div>
                          <div style={{ color: COLORS.GRAY, fontSize: '12px' }}>
                            Current: {formatLargeNumber(r.total_current_assets)}
                          </div>
                        </div>
                        <div>
                          <span style={{ color: COLORS.GRAY, fontSize: '13px' }}>Total Liabilities</span>
                          <div style={{ color: COLORS.RED, fontSize: '20px', fontWeight: 'bold' }}>
                            {formatLargeNumber(r.total_liabilities)}
                          </div>
                          <div style={{ color: COLORS.GRAY, fontSize: '12px' }}>
                            Current: {formatLargeNumber(r.total_current_liabilities)}
                          </div>
                        </div>
                        <div>
                          <span style={{ color: COLORS.GRAY, fontSize: '13px' }}>Total Equity</span>
                          <div style={{ color: COLORS.YELLOW, fontSize: '20px', fontWeight: 'bold' }}>
                            {formatLargeNumber(r.total_equity)}
                          </div>
                          <div style={{ color: COLORS.GRAY, fontSize: '12px' }}>
                            Working Capital: {formatLargeNumber(r.financial_ratios?.liquidity_ratios?.working_capital)}
                          </div>
                        </div>
                      </div>

                      {/* Liquidity Ratios */}
                      <div style={{ marginBottom: '12px' }}>
                        <div style={{ color: COLORS.CYAN, fontSize: '13px', marginBottom: '8px', fontWeight: 'bold' }}>Liquidity Ratios</div>
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '10px', fontSize: '13px' }}>
                          <div>
                            <span style={{ color: COLORS.GRAY }}>Current Ratio:</span>
                            <span style={{
                              color: r.financial_ratios?.liquidity_ratios?.current_ratio >= 1 ? COLORS.GREEN : COLORS.RED,
                              marginLeft: '5px',
                              fontWeight: 'bold'
                            }}>
                              {formatNumber(r.financial_ratios?.liquidity_ratios?.current_ratio, 2)}
                            </span>
                          </div>
                          <div>
                            <span style={{ color: COLORS.GRAY }}>Quick Ratio:</span>
                            <span style={{ color: COLORS.YELLOW, marginLeft: '5px', fontWeight: 'bold' }}>
                              {formatNumber(r.financial_ratios?.liquidity_ratios?.quick_ratio, 2)}
                            </span>
                          </div>
                          <div>
                            <span style={{ color: COLORS.GRAY }}>Cash Ratio:</span>
                            <span style={{ color: COLORS.CYAN, marginLeft: '5px', fontWeight: 'bold' }}>
                              {formatNumber(r.financial_ratios?.liquidity_ratios?.cash_ratio, 2)}
                            </span>
                          </div>
                          <div>
                            <span style={{ color: COLORS.GRAY }}>Cash & Equiv:</span>
                            <span style={{ color: COLORS.GREEN, marginLeft: '5px', fontWeight: 'bold' }}>
                              {formatLargeNumber(r.cash_and_equivalents)}
                            </span>
                          </div>
                        </div>
                      </div>

                      {/* Leverage Ratios */}
                      <div style={{ marginBottom: '12px' }}>
                        <div style={{ color: COLORS.ORANGE, fontSize: '13px', marginBottom: '8px', fontWeight: 'bold' }}>Leverage Ratios</div>
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '10px', fontSize: '13px' }}>
                          <div>
                            <span style={{ color: COLORS.GRAY }}>Debt/Equity:</span>
                            <span style={{
                              color: r.financial_ratios?.leverage_ratios?.debt_to_equity > 2 ? COLORS.RED : COLORS.YELLOW,
                              marginLeft: '5px',
                              fontWeight: 'bold'
                            }}>
                              {formatNumber(r.financial_ratios?.leverage_ratios?.debt_to_equity, 2)}
                            </span>
                          </div>
                          <div>
                            <span style={{ color: COLORS.GRAY }}>Debt/Assets:</span>
                            <span style={{ color: COLORS.CYAN, marginLeft: '5px', fontWeight: 'bold' }}>
                              {formatNumber(r.financial_ratios?.leverage_ratios?.debt_to_assets, 2)}
                            </span>
                          </div>
                          <div>
                            <span style={{ color: COLORS.GRAY }}>LT Debt:</span>
                            <span style={{ color: COLORS.RED, marginLeft: '5px', fontWeight: 'bold' }}>
                              {formatLargeNumber(r.long_term_debt_and_capital_lease_obligations)}
                            </span>
                          </div>
                          <div>
                            <span style={{ color: COLORS.GRAY }}>Equity Multi:</span>
                            <span style={{ color: COLORS.YELLOW, marginLeft: '5px', fontWeight: 'bold' }}>
                              {formatNumber(r.financial_ratios?.leverage_ratios?.equity_multiplier, 2)}
                            </span>
                          </div>
                        </div>
                      </div>

                      {/* Health Factors */}
                      {r.financial_health?.health_factors && (
                        <div style={{ marginTop: '10px' }}>
                          <div style={{ color: COLORS.GRAY, fontSize: '12px', marginBottom: '5px' }}>Health Factors:</div>
                          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '5px' }}>
                            {r.financial_health.health_factors.slice(0, 4).map((factor: string, idx: number) => (
                              <span key={idx} style={{
                                color: factor.includes('concern') || factor.includes('distress') || factor.includes('Low') || factor.includes('High debt') || factor.includes('Negative') ? COLORS.RED : COLORS.CYAN,
                                fontSize: '11px',
                                padding: '3px 8px',
                                backgroundColor: factor.includes('concern') ? 'rgba(255,0,0,0.1)' : 'rgba(0,255,255,0.1)',
                                border: `1px solid ${factor.includes('concern') ? COLORS.RED : COLORS.CYAN}`,
                              }}>
                                {factor}
                              </span>
                            ))}
                          </div>
                        </div>
                      )}
                    </div>
                  ))}
                </div>
              ) : (
                <div style={{ color: COLORS.GRAY, fontSize: '13px', padding: '15px', textAlign: 'center' }}>
                  No balance sheet data available
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Row 7: Dividends + Splits */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>

          {/* Dividends */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              DIVIDEND HISTORY ({dividends.length})
            </div>
            <div style={{ maxHeight: '250px', overflow: 'auto' }} className="custom-scrollbar">
              {dividends.length > 0 ? (
                <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px' }}>
                  <thead>
                    <tr style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                      <th style={{ color: COLORS.GRAY, textAlign: 'left', padding: '8px', fontSize: '11px' }}>Ex-Date</th>
                      <th style={{ color: COLORS.GRAY, textAlign: 'right', padding: '8px', fontSize: '11px' }}>Amount</th>
                      <th style={{ color: COLORS.GRAY, textAlign: 'left', padding: '8px', fontSize: '11px' }}>Type</th>
                    </tr>
                  </thead>
                  <tbody>
                    {dividends.map((d: any, i: number) => (
                      <tr key={i} style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                        <td style={{ color: COLORS.CYAN, padding: '8px' }}>{d.ex_dividend_date}</td>
                        <td style={{ color: COLORS.GREEN, textAlign: 'right', padding: '8px', fontWeight: 'bold' }}>
                          ${formatNumber(d.cash_amount, 3)}
                        </td>
                        <td style={{ color: COLORS.GRAY, padding: '8px', fontSize: '11px' }}>{d.dividend_type}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              ) : (
                <div style={{ color: COLORS.GRAY, fontSize: '13px', padding: '15px', textAlign: 'center' }}>
                  No dividend history available
                </div>
              )}
            </div>
          </div>

          {/* Stock Splits */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '15px',
          }}>
            <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '8px' }}>
              STOCK SPLITS ({splits.length})
            </div>
            <div style={{ maxHeight: '250px', overflow: 'auto' }} className="custom-scrollbar">
              {splits.length > 0 ? (
                <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px' }}>
                  <thead>
                    <tr style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                      <th style={{ color: COLORS.GRAY, textAlign: 'left', padding: '8px', fontSize: '11px' }}>Execution Date</th>
                      <th style={{ color: COLORS.GRAY, textAlign: 'right', padding: '8px', fontSize: '11px' }}>Ratio</th>
                    </tr>
                  </thead>
                  <tbody>
                    {splits.map((s: any, i: number) => (
                      <tr key={i} style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                        <td style={{ color: COLORS.CYAN, padding: '8px' }}>{s.execution_date}</td>
                        <td style={{ color: COLORS.YELLOW, textAlign: 'right', padding: '8px', fontWeight: 'bold' }}>
                          {s.split_from}:{s.split_to}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              ) : (
                <div style={{ color: COLORS.GRAY, fontSize: '13px', padding: '15px', textAlign: 'center' }}>
                  No split history available
                </div>
              )}
            </div>
          </div>
        </div>

      </div>

      {/* Footer */}
      <div style={{
        borderTop: `1px solid ${COLORS.BORDER}`,
        backgroundColor: COLORS.PANEL_BG,
        padding: '6px 15px',
        fontSize: '11px',
        color: COLORS.GRAY,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
          <span>Powered by Polygon.io API | Phase 3: 15/31 Endpoints | Institutional-Grade Market Data</span>
          <span>{symbol} | {new Date().toLocaleString()}</span>
        </div>
      </div>

      {/* Custom Scrollbar */}
      <style>{`
        .custom-scrollbar::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        .custom-scrollbar::-webkit-scrollbar-track {
          background: transparent;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb {
          background: rgba(120, 120, 120, 0.3);
          border-radius: 4px;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb:hover {
          background: rgba(120, 120, 120, 0.5);
        }
      `}</style>
    </div>
  );
};

export default PolygonEqTab;
