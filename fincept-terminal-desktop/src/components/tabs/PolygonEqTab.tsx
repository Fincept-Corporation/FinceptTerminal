import React, { useState, useEffect } from 'react';
import { polygonService } from '../../services/polygonService';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { yfinanceService, HistoricalDataPoint } from '../../services/yfinanceService';
import CandlestickChart, { CandlestickDataPoint } from '../charts/CandlestickChart';

const PolygonEqTab: React.FC = () => {
  const { colors } = useTerminalTheme();
  const COLORS = {
    ORANGE: colors.primary,
    WHITE: colors.text,
    RED: colors.alert,
    GREEN: colors.secondary,
    YELLOW: colors.warning,
    GRAY: colors.textMuted,
    BLUE: colors.info,
    CYAN: colors.accent,
    DARK_BG: colors.panel,
    PANEL_BG: colors.background,
    BORDER: '#222222',
    PANEL_HEADER_BG: '#1a1a1a',
  };
  const [symbol, setSymbol] = useState('AAPL');
  const [searchInput, setSearchInput] = useState('');
  const [loading, setLoading] = useState(false);
  const [apiKeyConfigured, setApiKeyConfigured] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<'financials' | 'actions' | 'short' | 'market' | 'reference'>('financials');

  // All data states (31 endpoints)
  const [tickerDetails, setTickerDetails] = useState<any>(null);
  const [snapshot, setSnapshot] = useState<any>(null);
  const [news, setNews] = useState<any[]>([]);
  const [relatedTickers, setRelatedTickers] = useState<any[]>([]);
  const [marketStatus, setMarketStatus] = useState<any>(null);
  const [dividends, setDividends] = useState<any[]>([]);
  const [splits, setSplits] = useState<any[]>([]);
  const [lastTrade, setLastTrade] = useState<any>(null);
  const [lastQuote, setLastQuote] = useState<any>(null);
  const [smaData, setSmaData] = useState<any>(null);
  const [emaData, setEmaData] = useState<any>(null);
  const [macdData, setMacdData] = useState<any>(null);
  const [rsiData, setRsiData] = useState<any>(null);
  const [incomeStmt, setIncomeStmt] = useState<any>(null);
  const [balanceSheet, setBalanceSheet] = useState<any>(null);
  const [allTickers, setAllTickers] = useState<any[]>([]);
  const [tickerTypes, setTickerTypes] = useState<any[]>([]);
  const [tickerEvents, setTickerEvents] = useState<any>(null);
  const [exchanges, setExchanges] = useState<any[]>([]);
  const [conditionCodes, setConditionCodes] = useState<any[]>([]);
  const [marketHolidays, setMarketHolidays] = useState<any[]>([]);
  const [marketStatusFull, setMarketStatusFull] = useState<any>(null);
  const [smaFull, setSmaFull] = useState<any>(null);
  const [emaFull, setEmaFull] = useState<any>(null);
  const [macdFull, setMacdFull] = useState<any>(null);
  const [cashFlowStatements, setCashFlowStatements] = useState<any>(null);
  const [financialRatios, setFinancialRatios] = useState<any>(null);
  const [shortInterest, setShortInterest] = useState<any>(null);
  const [shortVolume, setShortVolume] = useState<any>(null);
  const [trades, setTrades] = useState<any>(null);
  const [quotes, setQuotes] = useState<any>(null);
  const [chartData, setChartData] = useState<CandlestickDataPoint[]>([]);

  useEffect(() => {
    const loadApiKey = async () => {
      const apiKey = await polygonService.loadApiKey();
      setApiKeyConfigured(!!apiKey);
    };
    loadApiKey();
  }, []);

  const handleGoToSettings = () => {
    const settingsTab = document.querySelector('[data-tab="settings"]') as HTMLElement;
    if (settingsTab) {
      settingsTab.click();
    }
  };

  const fetchChartData = async (sym: string) => {
    try {
      // Fetch 30 days of historical data from yfinance
      const historicalData = await yfinanceService.getRecentHistory(sym, 30);

      if (historicalData && historicalData.length > 0) {
        const candlesticks: CandlestickDataPoint[] = historicalData.map((point: HistoricalDataPoint) => ({
          timestamp: point.timestamp,
          open: point.open,
          high: point.high,
          low: point.low,
          close: point.close,
          volume: point.volume,
        }));
        setChartData(candlesticks);
        console.log(`[PolygonEqTab] Loaded ${candlesticks.length} candlesticks for ${sym}`);
      } else {
        setChartData([]);
        console.warn(`[PolygonEqTab] No chart data available for ${sym}`);
      }
    } catch (error) {
      console.error('[PolygonEqTab] Error fetching chart data:', error);
      setChartData([]);
    }
  };

  const fetchAllData = async () => {
    const apiKey = await polygonService.loadApiKey();
    if (!apiKey) {
      console.log('[PolygonEqTab] No API key found, skipping data fetch');
      return;
    }

    console.log('=== FETCHING DATA FOR:', symbol, '===');
    setLoading(true);
    setError(null);

    try {
      // Fetch chart data (uses yfinance, not Polygon)
      fetchChartData(symbol);

      // Helper function to add delay between API calls
      const delay = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));
      const DELAY_MS = 200; // 200ms between requests = 5 requests per second (free tier limit)

      // Fetch data sequentially with delays to respect rate limits
      console.log('[PolygonEqTab] Fetching ticker details...');
      const detailsResp = await polygonService.getTickerDetails(symbol).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching snapshot...');
      const snapResp = await polygonService.getSingleTickerSnapshot(symbol).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching news...');
      const newsResp = await polygonService.getNews({ ticker: symbol, limit: 10 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching related tickers...');
      const relatedResp = await polygonService.getRelatedTickers(symbol).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching market status...');
      const marketStatusResp = await polygonService.getMarketStatus().catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching dividends...');
      const dividendsResp = await polygonService.getDividends({ ticker: symbol, limit: 10 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching splits...');
      const splitsResp = await polygonService.getSplits({ ticker: symbol, limit: 10 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching last trade...');
      const lastTradeResp = await polygonService.getLastTrade(symbol).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching last quote...');
      const lastQuoteResp = await polygonService.getLastQuote(symbol).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching SMA(20)...');
      const smaResp = await polygonService.getSMA(symbol, { window: 20, timespan: 'day', limit: 50 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching EMA(20)...');
      const emaResp = await polygonService.getEMA(symbol, { window: 20, timespan: 'day', limit: 50 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching MACD...');
      const macdResp = await polygonService.getMACD(symbol, { shortWindow: 12, longWindow: 26, signalWindow: 9, timespan: 'day', limit: 50 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching RSI...');
      const rsiResp = await polygonService.getRSI(symbol, { window: 14, timespan: 'day', limit: 50 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching income statements...');
      const incomeResp = await polygonService.getIncomeStatements(symbol, { timeframe: 'annual', limit: 5 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching balance sheets...');
      const balanceResp = await polygonService.getBalanceSheets(symbol, { timeframe: 'annual', limit: 5 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching all tickers...');
      const allTickersResp = await polygonService.getAllTickers({ search: symbol, limit: 10 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching ticker types...');
      const tickerTypesResp = await polygonService.getTickerTypes().catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching ticker events...');
      const tickerEventsResp = await polygonService.getTickerEvents(symbol).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching exchanges...');
      const exchangesResp = await polygonService.getExchanges().catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching condition codes...');
      const conditionCodesResp = await polygonService.getConditionCodes({ limit: 20 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching market holidays...');
      const marketHolidaysResp = await polygonService.getMarketHolidays().catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      const marketStatusFullResp = marketStatusResp; // Reuse market status

      console.log('[PolygonEqTab] Fetching SMA(50)...');
      const smaFullResp = await polygonService.getSMA(symbol, { window: 50, timespan: 'day', limit: 100 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching EMA(50)...');
      const emaFullResp = await polygonService.getEMA(symbol, { window: 50, timespan: 'day', limit: 100 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching MACD (full)...');
      const macdFullResp = await polygonService.getMACD(symbol, { shortWindow: 12, longWindow: 26, signalWindow: 9, timespan: 'day', limit: 100 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching cash flow statements...');
      const cashFlowResp = await polygonService.getCashFlowStatements(symbol, { timeframe: 'annual', limit: 5 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching financial ratios...');
      const financialRatiosResp = await polygonService.getFinancialRatios(symbol, { limit: 10 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching short interest...');
      const shortInterestResp = await polygonService.getShortInterest(symbol, { limit: 10 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching short volume...');
      const shortVolumeResp = await polygonService.getShortVolume(symbol, { limit: 10 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching trades...');
      const tradesResp = await polygonService.getTrades(symbol, { limit: 10 }).catch((e) => ({ success: false, error: e.message }));
      await delay(DELAY_MS);

      console.log('[PolygonEqTab] Fetching quotes...');
      const quotesResp = await polygonService.getQuotes(symbol, { limit: 10 }).catch((e) => ({ success: false, error: e.message }));

      // Parse all responses - check for both explicit success=true or presence of data (success undefined means data exists)
      console.log('[PolygonEqTab] detailsResp:', detailsResp);
      console.log('[PolygonEqTab] snapResp:', snapResp);
      console.log('[PolygonEqTab] newsResp:', newsResp);
      console.log('[PolygonEqTab] lastTradeResp:', lastTradeResp);
      console.log('[PolygonEqTab] lastQuoteResp:', lastQuoteResp);

      if ((detailsResp?.success !== false) && detailsResp?.ticker_details) setTickerDetails(detailsResp.ticker_details);
      if ((snapResp?.success !== false) && snapResp?.snapshot) setSnapshot(snapResp.snapshot);
      if ((newsResp?.success !== false) && newsResp?.news_articles) setNews(newsResp.news_articles);
      else setNews([]);

      if ((relatedResp?.success !== false) && relatedResp?.related_tickers) {
        const tickersArray = relatedResp.related_tickers.map((ticker: any) =>
          typeof ticker === 'string' ? { ticker: ticker, name: null } : ticker
        );
        setRelatedTickers(tickersArray);
      } else setRelatedTickers([]);

      if ((marketStatusResp?.success !== false) && marketStatusResp?.market_status) setMarketStatus(marketStatusResp.market_status);
      if ((dividendsResp?.success !== false) && dividendsResp?.dividends) setDividends(dividendsResp.dividends);
      else setDividends([]);

      if ((splitsResp?.success !== false) && splitsResp?.splits) setSplits(splitsResp.splits);
      else setSplits([]);

      if ((lastTradeResp?.success !== false) && lastTradeResp?.last_trade) setLastTrade(lastTradeResp.last_trade);
      if ((lastQuoteResp?.success !== false) && lastQuoteResp?.last_quote) setLastQuote(lastQuoteResp.last_quote);
      if ((smaResp?.success !== false) && smaResp?.sma_data) setSmaData(smaResp.sma_data);
      if ((emaResp?.success !== false) && emaResp?.ema_data) setEmaData(emaResp.ema_data);
      if ((macdResp?.success !== false) && macdResp?.macd_data) setMacdData(macdResp.macd_data);
      if ((rsiResp?.success !== false) && rsiResp?.rsi_data) setRsiData(rsiResp.rsi_data);

      if ((incomeResp?.success !== false) && incomeResp?.income_statement_data) {
        setIncomeStmt(incomeResp.income_statement_data);
      }

      if ((balanceResp?.success !== false) && balanceResp?.balance_sheet_data) {
        setBalanceSheet(balanceResp.balance_sheet_data);
      }

      if ((allTickersResp?.success !== false) && allTickersResp?.tickers) setAllTickers(allTickersResp.tickers);
      else setAllTickers([]);

      if ((tickerTypesResp?.success !== false) && tickerTypesResp?.ticker_types) setTickerTypes(tickerTypesResp.ticker_types);
      else setTickerTypes([]);

      if ((tickerEventsResp?.success !== false) && tickerEventsResp?.ticker_events) setTickerEvents(tickerEventsResp.ticker_events);
      else setTickerEvents(null);

      if ((exchangesResp?.success !== false) && exchangesResp?.exchanges) setExchanges(exchangesResp.exchanges);
      else setExchanges([]);

      if ((conditionCodesResp?.success !== false) && conditionCodesResp?.condition_codes) setConditionCodes(conditionCodesResp.condition_codes);
      else setConditionCodes([]);

      if ((marketHolidaysResp?.success !== false) && marketHolidaysResp?.market_holidays) setMarketHolidays(marketHolidaysResp.market_holidays);
      else setMarketHolidays([]);

      if ((marketStatusFullResp?.success !== false) && marketStatusFullResp?.market_status) setMarketStatusFull(marketStatusFullResp.market_status);
      else setMarketStatusFull(null);

      if ((smaFullResp?.success !== false) && smaFullResp?.sma_data) setSmaFull(smaFullResp.sma_data);
      else setSmaFull(null);

      if ((emaFullResp?.success !== false) && emaFullResp?.ema_data) setEmaFull(emaFullResp.ema_data);
      else setEmaFull(null);

      if ((macdFullResp?.success !== false) && macdFullResp?.macd_data) setMacdFull(macdFullResp.macd_data);
      else setMacdFull(null);

      if (cashFlowResp?.success !== false && cashFlowResp?.cash_flow_data) setCashFlowStatements(cashFlowResp);
      else setCashFlowStatements(null);

      if (financialRatiosResp?.success !== false && financialRatiosResp?.ratios_data) setFinancialRatios(financialRatiosResp);
      else setFinancialRatios(null);

      if (shortInterestResp?.success !== false && shortInterestResp?.short_interest_data) setShortInterest(shortInterestResp);
      else setShortInterest(null);

      if (shortVolumeResp?.success !== false && shortVolumeResp?.short_volume_data) setShortVolume(shortVolumeResp);
      else setShortVolume(null);

      if (tradesResp?.success !== false && tradesResp?.trades) setTrades(tradesResp);
      else setTrades(null);

      if (quotesResp?.success !== false && quotesResp?.quotes) setQuotes(quotesResp);
      else setQuotes(null);

      console.log('=== DATA FETCH COMPLETE (31/31 ENDPOINTS) ===');

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
    if (num >= 1e3) return `$${(num / 1e3).toFixed(2)}K`;
    return `$${num.toFixed(2)}`;
  };

  // API Key Config Screen
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
          maxWidth: '600px',
          textAlign: 'center',
        }}>
          <div style={{ color: COLORS.ORANGE, fontSize: '24px', fontWeight: 'bold', marginBottom: '20px' }}>
            POLYGON.IO API KEY REQUIRED
          </div>
          <div style={{ color: COLORS.GRAY, fontSize: '14px', marginBottom: '20px', lineHeight: '1.6' }}>
            Configure your Polygon.io API key to access institutional-grade financial data.
            <br /><br />
            <strong style={{ color: COLORS.CYAN }}>Steps to configure:</strong>
            <ol style={{ textAlign: 'left', marginTop: '10px', paddingLeft: '40px' }}>
              <li>Get your free API key at: <span style={{ color: COLORS.CYAN }}>polygon.io</span></li>
              <li>Go to Settings → Credentials section</li>
              <li>Add new API key with service name: <span style={{ color: COLORS.CYAN }}>Polygon.io</span></li>
              <li>Paste your API key and save</li>
              <li>Return to this tab and refresh</li>
            </ol>
          </div>
          <button
            onClick={handleGoToSettings}
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
            GO TO SETTINGS
          </button>
        </div>
      </div>
    );
  }

  // Calculate price change
  const currentPrice = snapshot?.day?.close || snapshot?.prev_day?.close || 0;
  const prevClose = snapshot?.prev_day?.close || 0;
  let priceChange = snapshot?.todays_change || 0;
  let priceChangePercent = snapshot?.todays_change_perc || 0;

  if (priceChange === 0 && currentPrice > 0 && prevClose > 0) {
    priceChange = currentPrice - prevClose;
    priceChangePercent = ((priceChange / prevClose) * 100);
  }

  // Section Header Component
  const SectionHeader: React.FC<{ title: string; color?: string }> = ({ title, color = COLORS.ORANGE }) => (
    <div style={{
      backgroundColor: COLORS.PANEL_HEADER_BG,
      borderBottom: `1px solid ${color}`,
      padding: '6px 10px',
      color: color,
      fontSize: '12px',
      fontWeight: 'bold',
      letterSpacing: '0.5px',
    }}>
      {title}
    </div>
  );

  // Data Row Component
  const DataRow: React.FC<{ label: string; value: string | number; valueColor?: string; small?: boolean }> =
    ({ label, value, valueColor = COLORS.WHITE, small = false }) => (
    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: small ? '3px' : '6px', fontSize: small ? '11px' : '12px' }}>
      <span style={{ color: COLORS.GRAY }}>{label}:</span>
      <span style={{ color: valueColor, fontWeight: 'bold' }}>{value}</span>
    </div>
  );

  return (
    <div style={{
      height: '100%',
      backgroundColor: COLORS.DARK_BG,
      color: COLORS.WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px',
      overflow: 'auto',
      overflowX: 'hidden',
    }} className="custom-scrollbar">

      {/* TOP BAR WITH RELATED TICKERS */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: `1px solid ${COLORS.BORDER}`,
        padding: '8px 12px',
        display: 'flex',
        flexDirection: 'column',
        gap: '8px',
        flexShrink: 0,
      }}>
        {/* First Row: Search and Main Info */}
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flexWrap: 'wrap' }}>
            <span style={{ color: COLORS.ORANGE, fontWeight: 'bold', fontSize: '13px' }}>POLYGON.IO</span>
            <input
              type="text"
              value={searchInput}
              onChange={(e) => setSearchInput(e.target.value)}
              onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
              placeholder="Symbol..."
              style={{
                backgroundColor: COLORS.DARK_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                padding: '4px 10px',
                fontSize: '12px',
                width: '120px',
                fontFamily: 'Consolas, monospace',
              }}
            />
            <button
              onClick={handleSearch}
              style={{
                backgroundColor: COLORS.ORANGE,
                border: 'none',
                color: COLORS.DARK_BG,
                padding: '4px 14px',
                fontSize: '11px',
                cursor: 'pointer',
                fontWeight: 'bold',
              }}
            >
              GO
            </button>

            {/* Related Tickers as Small Boxes */}
            {relatedTickers.length > 0 && (
              <>
                <span style={{ color: COLORS.GRAY, fontSize: '10px', fontWeight: 'bold', marginLeft: '8px' }}>RELATED:</span>
                {relatedTickers.slice(0, 8).map((ticker: any, idx: number) => (
                  <div
                    key={idx}
                    onClick={() => handleRelatedTickerClick(ticker.ticker)}
                    style={{
                      backgroundColor: COLORS.PANEL_HEADER_BG,
                      border: `1px solid ${COLORS.BORDER}`,
                      padding: '3px 8px',
                      cursor: 'pointer',
                      fontSize: '10px',
                      color: COLORS.CYAN,
                      fontWeight: 'bold',
                      transition: 'all 0.2s',
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.backgroundColor = COLORS.CYAN;
                      e.currentTarget.style.color = COLORS.DARK_BG;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.backgroundColor = COLORS.PANEL_HEADER_BG;
                      e.currentTarget.style.color = COLORS.CYAN;
                    }}
                  >
                    {ticker.ticker}
                  </div>
                ))}
              </>
            )}
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '15px' }}>
            <span style={{ color: COLORS.CYAN, fontSize: '18px', fontWeight: 'bold' }}>{symbol}</span>
            <span style={{ color: COLORS.GRAY, fontSize: '12px', maxWidth: '250px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
              {tickerDetails?.name || ''}
            </span>
            <span style={{ color: COLORS.WHITE, fontSize: '16px', fontWeight: 'bold' }}>
              ${formatNumber(currentPrice)}
            </span>
            <span style={{ color: priceChange >= 0 ? COLORS.GREEN : COLORS.RED, fontSize: '13px', fontWeight: 'bold' }}>
              {priceChange >= 0 ? '▲' : '▼'} {Math.abs(priceChange).toFixed(2)} ({priceChangePercent.toFixed(2)}%)
            </span>
            {loading && <span style={{ color: COLORS.YELLOW, fontSize: '10px' }}>● LOAD</span>}
            {!loading && !error && <span style={{ color: COLORS.GREEN, fontSize: '10px' }}>● LIVE</span>}
            <button
              onClick={() => fetchAllData()}
              style={{
                backgroundColor: COLORS.BLUE,
                border: 'none',
                color: COLORS.WHITE,
                padding: '4px 12px',
                fontSize: '10px',
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
          backgroundColor: '#1a0000',
          borderBottom: `1px solid ${COLORS.RED}`,
          padding: '6px 12px',
          color: COLORS.RED,
          fontSize: '11px',
          flexShrink: 0,
        }}>
          <strong>ERROR:</strong> {error}
        </div>
      )}

      {/* MAIN CONTENT - All data visible, main window scrolls */}
      <div style={{ padding: '6px', flexShrink: 0, maxWidth: '100%', boxSizing: 'border-box' }}>

        {/* ROW 1: Quote Data + Market Status + Technical Indicators - NO INTERNAL SCROLL */}
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, minmax(0, 1fr))', gap: '8px', marginBottom: '8px' }}>

          {/* Quote Panel */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <SectionHeader title="QUOTE" color={COLORS.CYAN} />
            <div style={{ padding: '10px', fontSize: '11px' }}>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px', marginBottom: '6px' }}>
                <div><span style={{ color: COLORS.GRAY, fontSize: '10px' }}>OPEN:</span> <span style={{ color: COLORS.CYAN }}>${formatNumber(snapshot?.day?.open)}</span></div>
                <div><span style={{ color: COLORS.GRAY, fontSize: '10px' }}>HIGH:</span> <span style={{ color: COLORS.GREEN }}>${formatNumber(snapshot?.day?.high)}</span></div>
                <div><span style={{ color: COLORS.GRAY, fontSize: '10px' }}>LOW:</span> <span style={{ color: COLORS.RED }}>${formatNumber(snapshot?.day?.low)}</span></div>
                <div><span style={{ color: COLORS.GRAY, fontSize: '10px' }}>VOL:</span> <span style={{ color: COLORS.YELLOW }}>{formatNumber(snapshot?.day?.volume, 0)}</span></div>
              </div>
              <DataRow label="VWAP" value={`$${formatNumber(snapshot?.day?.vwap)}`} valueColor={COLORS.CYAN} small />
              <div style={{ borderTop: `1px solid ${COLORS.BORDER}`, marginTop: '6px', paddingTop: '6px' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '3px' }}>LAST TRADE</div>
                <DataRow label="Price" value={`$${formatNumber(lastTrade?.price)}`} valueColor={COLORS.GREEN} small />
                <DataRow label="Size" value={formatNumber(lastTrade?.size, 0)} valueColor={COLORS.WHITE} small />
              </div>
            </div>
          </div>

          {/* Bid/Ask Panel */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <SectionHeader title="BID/ASK" color={COLORS.GREEN} />
            <div style={{ padding: '10px', fontSize: '11px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                <div>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>BID</div>
                  <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold' }}>${formatNumber(lastQuote?.bid_price)}</div>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>x{formatNumber(lastQuote?.bid_size, 0)}</div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>ASK</div>
                  <div style={{ color: COLORS.RED, fontSize: '14px', fontWeight: 'bold' }}>${formatNumber(lastQuote?.ask_price)}</div>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>x{formatNumber(lastQuote?.ask_size, 0)}</div>
                </div>
              </div>
              <DataRow label="Spread" value={`$${formatNumber((lastQuote?.ask_price || 0) - (lastQuote?.bid_price || 0), 3)}`} valueColor={COLORS.YELLOW} small />
              <div style={{ borderTop: `1px solid ${COLORS.BORDER}`, marginTop: '6px', paddingTop: '6px' }}>
                <DataRow label="Mkt Cap" value={formatLargeNumber(tickerDetails?.market_cap)} valueColor={COLORS.GREEN} small />
                <DataRow label="Exchange" value={tickerDetails?.primary_exchange || 'N/A'} valueColor={COLORS.WHITE} small />
              </div>
            </div>
          </div>

          {/* Market Status */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <SectionHeader title="MARKET STATUS" color={COLORS.GREEN} />
            <div style={{ padding: '10px', fontSize: '11px', textAlign: 'center' }}>
              <div style={{
                color: marketStatus?.market === 'open' ? COLORS.GREEN : COLORS.RED,
                fontSize: '16px',
                fontWeight: 'bold',
                marginBottom: '6px'
              }}>
                {marketStatus?.market?.toUpperCase() || 'N/A'}
              </div>
              <div style={{ color: COLORS.CYAN, fontSize: '10px', marginBottom: '8px' }}>
                {marketStatus?.serverTime || 'N/A'}
              </div>
              <DataRow label="Exchanges" value={marketStatus?.exchanges ? Object.keys(marketStatus.exchanges).length : 'N/A'} valueColor={COLORS.BLUE} small />
              <DataRow label="Currencies" value={marketStatus?.currencies ? Object.keys(marketStatus.currencies).length : 'N/A'} valueColor={COLORS.YELLOW} small />
            </div>
          </div>

          {/* Technical Indicators */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <SectionHeader title="TECHNICALS" color={COLORS.YELLOW} />
            <div style={{ padding: '10px', fontSize: '11px' }}>
              <DataRow label="SMA(20)" value={`$${formatNumber(smaData?.statistics?.latest_value)}`} valueColor={COLORS.CYAN} small />
              <DataRow label="EMA(20)" value={`$${formatNumber(emaData?.statistics?.latest_value)}`} valueColor={COLORS.BLUE} small />
              <DataRow label="RSI(14)" value={formatNumber(rsiData?.statistics?.rsi_stats?.latest_value, 1)}
                valueColor={(rsiData?.statistics?.rsi_stats?.latest_value || 0) > 70 ? COLORS.RED : (rsiData?.statistics?.rsi_stats?.latest_value || 0) < 30 ? COLORS.GREEN : COLORS.YELLOW} small />
              <div style={{ borderTop: `1px solid ${COLORS.BORDER}`, marginTop: '6px', paddingTop: '6px' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '3px' }}>MACD Signal</div>
                <div style={{
                  color: macdData?.macd_values?.[0]?.crossover_analysis?.macd_above_signal ? COLORS.GREEN : COLORS.RED,
                  fontSize: '12px',
                  fontWeight: 'bold',
                  textAlign: 'center'
                }}>
                  {macdData?.macd_values?.[0]?.crossover_analysis?.macd_above_signal ? '▲ BULLISH' : '▼ BEARISH'}
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* ROW 2: News (2 columns) + Trades & Quotes + Chart Placeholder */}
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr 1fr', gap: '8px', marginBottom: '8px' }}>

          {/* News - Show 4 articles */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <SectionHeader title={`LATEST NEWS (${news.length})`} color={COLORS.ORANGE} />
            <div style={{ padding: '10px', maxHeight: '240px', overflowY: 'auto' }}>
              {news.length > 0 ? news.slice(0, 4).map((article: any) => (
                <div key={article.id} style={{
                  marginBottom: '10px',
                  paddingBottom: '10px',
                  borderBottom: `1px solid ${COLORS.BORDER}`,
                  fontSize: '11px',
                  display: 'flex',
                  gap: '10px'
                }}>
                  {/* Article Image */}
                  {article.image_url && (
                    <div style={{
                      flexShrink: 0,
                      width: '80px',
                      height: '60px',
                      overflow: 'hidden',
                      borderRadius: '4px',
                      border: `1px solid ${COLORS.BORDER}`,
                      backgroundColor: COLORS.DARK_BG
                    }}>
                      <img
                        src={article.image_url}
                        alt={article.title}
                        style={{
                          width: '100%',
                          height: '100%',
                          objectFit: 'cover'
                        }}
                        onError={(e) => {
                          // Hide image if it fails to load
                          (e.target as HTMLImageElement).style.display = 'none';
                        }}
                      />
                    </div>
                  )}

                  {/* Article Content */}
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{
                      color: COLORS.ORANGE,
                      fontWeight: 'bold',
                      marginBottom: '3px',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      display: '-webkit-box',
                      WebkitLineClamp: 2,
                      WebkitBoxOrient: 'vertical'
                    }}>
                      {article.title}
                    </div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>
                      {article.publisher?.name} | {new Date(article.published_utc).toLocaleDateString()}
                    </div>
                    {article.description && (
                      <div style={{
                        color: COLORS.WHITE,
                        fontSize: '9px',
                        marginBottom: '4px',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        display: '-webkit-box',
                        WebkitLineClamp: 2,
                        WebkitBoxOrient: 'vertical',
                        lineHeight: '1.3'
                      }}>
                        {article.description}
                      </div>
                    )}
                    <a
                      href={article.article_url}
                      target="_blank"
                      rel="noopener noreferrer"
                      style={{ color: COLORS.CYAN, fontSize: '10px', textDecoration: 'underline' }}
                    >
                      READ MORE →
                    </a>
                  </div>
                </div>
              )) : (
                <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '15px', fontSize: '11px' }}>
                  No news available
                </div>
              )}
            </div>
          </div>

          {/* Trades & Quotes Panel */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <SectionHeader title="TRADES & QUOTES" color={COLORS.CYAN} />
            <div style={{ padding: '10px', fontSize: '11px' }}>
              {/* Recent Trades */}
              <div style={{ marginBottom: '8px' }}>
                <div style={{ color: COLORS.ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>LAST TRADE</div>
                <DataRow label="Price" value={`$${formatNumber(lastTrade?.price)}`} valueColor={COLORS.GREEN} small />
                <DataRow label="Size" value={formatNumber(lastTrade?.size, 0)} small />
                <DataRow label="Time" value={lastTrade?.participant_timestamp ? new Date(lastTrade.participant_timestamp / 1000000).toLocaleTimeString() : 'N/A'} valueColor={COLORS.GRAY} small />
              </div>

              {/* Recent Quotes */}
              <div style={{ borderTop: `1px solid ${COLORS.BORDER}`, paddingTop: '8px' }}>
                <div style={{ color: COLORS.ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>LAST QUOTE</div>
                <DataRow label="Bid" value={`$${formatNumber(lastQuote?.bid_price)}`} valueColor={COLORS.RED} small />
                <DataRow label="Ask" value={`$${formatNumber(lastQuote?.ask_price)}`} valueColor={COLORS.GREEN} small />
                <DataRow label="Spread" value={`$${formatNumber((lastQuote?.ask_price || 0) - (lastQuote?.bid_price || 0))}`} valueColor={COLORS.YELLOW} small />
              </div>
            </div>
          </div>

          {/* Candlestick Chart */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, display: 'flex', flexDirection: 'column', height: '100%' }}>
            <SectionHeader title="30-DAY CHART" color={COLORS.BLUE} />
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'stretch',
              justifyContent: 'stretch',
              padding: '4px',
            }}>
              <CandlestickChart
                data={chartData}
                width={300}
                height={230}
                bullishColor={COLORS.GREEN}
                bearishColor={COLORS.RED}
                backgroundColor={COLORS.DARK_BG}
                gridColor={COLORS.BORDER}
                textColor={COLORS.GRAY}
                showGrid={true}
                showVolume={true}
                showLabels={true}
              />
            </div>
          </div>
        </div>

        {/* ROW 3: TABBED BOTTOM SECTION - All data shown without scroll */}
        <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>

          {/* Tab Bar */}
          <div style={{ display: 'flex', borderBottom: `1px solid ${COLORS.BORDER}`, backgroundColor: COLORS.PANEL_HEADER_BG }}>
            {[
              { id: 'financials', label: 'FINANCIALS' },
              { id: 'actions', label: 'ACTIONS' },
              { id: 'short', label: 'SHORT' },
              { id: 'market', label: 'MARKET' },
              { id: 'reference', label: 'REFERENCE' },
            ].map((tab) => (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id as any)}
                style={{
                  padding: '6px 15px',
                  backgroundColor: activeTab === tab.id ? COLORS.PANEL_BG : 'transparent',
                  border: 'none',
                  borderBottom: activeTab === tab.id ? `2px solid ${COLORS.ORANGE}` : 'none',
                  color: activeTab === tab.id ? COLORS.ORANGE : COLORS.GRAY,
                  fontSize: '9px',
                  fontWeight: 'bold',
                  cursor: 'pointer',
                  fontFamily: 'Consolas, monospace',
                }}
              >
                {tab.label}
              </button>
            ))}
          </div>

          {/* Tab Content - No scroll, all visible */}
          <div style={{ padding: '8px' }}>

            {/* FINANCIALS TAB */}
            {activeTab === 'financials' && (
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>

                {/* Income Statement */}
                <div>
                  <div style={{ color: COLORS.GREEN, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    INCOME STATEMENT
                  </div>
                  {incomeStmt?.results && incomeStmt.results.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {incomeStmt.results.slice(0, 1).map((r: any, i: number) => (
                        <div key={i}>
                          <div style={{ color: COLORS.ORANGE, fontWeight: 'bold', marginBottom: '4px' }}>
                            FY {r.fiscal_year}
                          </div>
                          <DataRow label="Revenue" value={formatLargeNumber(r.revenue)} valueColor={COLORS.GREEN} small />
                          <DataRow label="Gross Profit" value={formatLargeNumber(r.gross_profit)} valueColor={COLORS.CYAN} small />
                          <DataRow label="Operating Inc" value={formatLargeNumber(r.operating_income)} valueColor={COLORS.BLUE} small />
                          <DataRow label="Net Income" value={formatLargeNumber(r.consolidated_net_income_loss)}
                            valueColor={r.consolidated_net_income_loss >= 0 ? COLORS.GREEN : COLORS.RED} small />
                          <DataRow label="EBITDA" value={formatLargeNumber(r.ebitda)} valueColor={COLORS.YELLOW} small />
                          <DataRow label="EPS" value={`$${formatNumber(r.diluted_earnings_per_share)}`} valueColor={COLORS.WHITE} small />
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No data</div>
                  )}
                </div>

                {/* Balance Sheet */}
                <div>
                  <div style={{ color: COLORS.BLUE, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    BALANCE SHEET
                  </div>
                  {balanceSheet?.results && balanceSheet.results.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {balanceSheet.results.slice(0, 1).map((r: any, i: number) => (
                        <div key={i}>
                          <div style={{ color: COLORS.ORANGE, fontWeight: 'bold', marginBottom: '4px' }}>
                            FY {r.fiscal_year}
                          </div>
                          <DataRow label="Total Assets" value={formatLargeNumber(r.total_assets)} valueColor={COLORS.GREEN} small />
                          <DataRow label="Current Assets" value={formatLargeNumber(r.total_current_assets)} valueColor={COLORS.CYAN} small />
                          <DataRow label="Total Liab" value={formatLargeNumber(r.total_liabilities)} valueColor={COLORS.RED} small />
                          <DataRow label="Total Equity" value={formatLargeNumber(r.total_equity)} valueColor={COLORS.YELLOW} small />
                          <DataRow label="Cash" value={formatLargeNumber(r.cash_and_equivalents)} valueColor={COLORS.GREEN} small />
                          <DataRow label="Current Ratio" value={formatNumber(r.financial_ratios?.liquidity_ratios?.current_ratio, 2)}
                            valueColor={r.financial_ratios?.liquidity_ratios?.current_ratio >= 1 ? COLORS.GREEN : COLORS.RED} small />
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No data</div>
                  )}
                </div>

                {/* Cash Flow & Ratios */}
                <div>
                  <div style={{ color: COLORS.YELLOW, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    CASH FLOW & RATIOS
                  </div>
                  <div style={{ fontSize: '8px' }}>
                    {cashFlowStatements?.cash_flow_data?.results && cashFlowStatements.cash_flow_data.results.length > 0 && (
                      <div style={{ marginBottom: '6px' }}>
                        {cashFlowStatements.cash_flow_data.results.slice(0, 1).map((cf: any, idx: number) => (
                          <div key={idx}>
                            <DataRow label="Operating CF" value={formatLargeNumber(cf.net_cash_from_operating_activities)} valueColor={COLORS.GREEN} small />
                            <DataRow label="Investing CF" value={formatLargeNumber(cf.net_cash_from_investing_activities)} valueColor={COLORS.BLUE} small />
                            <DataRow label="Financing CF" value={formatLargeNumber(cf.net_cash_from_financing_activities)} valueColor={COLORS.ORANGE} small />
                            <DataRow label="Free CF" value={formatLargeNumber(cf.cash_flow_ratios?.profitability_metrics?.free_cash_flow)} valueColor={COLORS.GREEN} small />
                          </div>
                        ))}
                      </div>
                    )}
                    {financialRatios?.ratios_data?.results && financialRatios.ratios_data.results.length > 0 && (
                      <div>
                        {financialRatios.ratios_data.results.slice(0, 1).map((ratio: any, idx: number) => (
                          <div key={idx}>
                            <DataRow label="P/E Ratio" value={formatNumber(ratio.price_to_earnings_ratio, 2)} valueColor={COLORS.WHITE} small />
                            <DataRow label="ROE" value={`${formatNumber(ratio.return_on_equity * 100, 2)}%`} valueColor={COLORS.GREEN} small />
                            <DataRow label="ROA" value={`${formatNumber(ratio.return_on_assets * 100, 2)}%`} valueColor={COLORS.GREEN} small />
                          </div>
                        ))}
                      </div>
                    )}
                  </div>
                </div>
              </div>
            )}

            {/* CORPORATE ACTIONS TAB */}
            {activeTab === 'actions' && (
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>

                {/* Dividends */}
                <div>
                  <div style={{ color: COLORS.GREEN, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    DIVIDENDS ({dividends.length})
                  </div>
                  {dividends.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {dividends.slice(0, 8).map((d: any, i: number) => (
                        <div key={i} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px', paddingBottom: '3px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                          <span style={{ color: COLORS.CYAN }}>{d.ex_dividend_date}</span>
                          <span style={{ color: COLORS.GREEN, fontWeight: 'bold' }}>${formatNumber(d.cash_amount, 3)}</span>
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No dividends</div>
                  )}
                </div>

                {/* Stock Splits */}
                <div>
                  <div style={{ color: COLORS.YELLOW, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    SPLITS ({splits.length})
                  </div>
                  {splits.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {splits.slice(0, 8).map((s: any, i: number) => (
                        <div key={i} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px', paddingBottom: '3px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                          <span style={{ color: COLORS.CYAN }}>{s.execution_date}</span>
                          <span style={{ color: COLORS.YELLOW, fontWeight: 'bold' }}>{s.split_from}:{s.split_to}</span>
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No splits</div>
                  )}
                </div>
              </div>
            )}

            {/* SHORT INTEREST TAB */}
            {activeTab === 'short' && (
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>

                {/* Short Interest */}
                <div>
                  <div style={{ color: COLORS.RED, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    SHORT INTEREST
                  </div>
                  {shortInterest?.short_interest_data && shortInterest.short_interest_data.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {shortInterest.short_interest_data.slice(0, 5).map((data: any, idx: number) => (
                        <div key={idx} style={{ marginBottom: '4px', paddingBottom: '4px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                          <div style={{ color: COLORS.CYAN, fontWeight: 'bold', marginBottom: '2px' }}>{data.settlement_date}</div>
                          <DataRow label="Interest" value={formatNumber(data.short_interest)} valueColor={COLORS.WHITE} small />
                          <DataRow label="% Float" value={`${formatNumber(data.percent_of_float, 2)}%`} valueColor={COLORS.RED} small />
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No data</div>
                  )}
                </div>

                {/* Short Volume */}
                <div>
                  <div style={{ color: COLORS.ORANGE, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    SHORT VOLUME
                  </div>
                  {shortVolume?.short_volume_data && shortVolume.short_volume_data.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {shortVolume.short_volume_data.slice(0, 5).map((data: any, idx: number) => (
                        <div key={idx} style={{ marginBottom: '4px', paddingBottom: '4px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                          <div style={{ color: COLORS.CYAN, fontWeight: 'bold', marginBottom: '2px' }}>{data.date}</div>
                          <DataRow label="Volume" value={formatNumber(data.volume)} valueColor={COLORS.WHITE} small />
                          <DataRow label="Short Vol" value={formatNumber(data.short_volume)} valueColor={COLORS.ORANGE} small />
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No data</div>
                  )}
                </div>

                {/* Trades & Quotes */}
                <div>
                  <div style={{ color: COLORS.GREEN, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    TRADES & QUOTES
                  </div>
                  <div style={{ fontSize: '8px' }}>
                    {trades?.trades && trades.trades.length > 0 ? (
                      <div style={{ marginBottom: '6px' }}>
                        {trades.trades.slice(0, 3).map((trade: any, idx: number) => (
                          <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px', fontSize: '7px' }}>
                            <span style={{ color: COLORS.GRAY }}>{trade.time}</span>
                            <span style={{ color: COLORS.GREEN }}>${formatNumber(trade.price, 2)}</span>
                            <span style={{ color: COLORS.WHITE }}>{formatNumber(trade.size)}</span>
                          </div>
                        ))}
                      </div>
                    ) : null}
                    {quotes?.quotes && quotes.quotes.length > 0 ? (
                      <div>
                        {quotes.quotes.slice(0, 3).map((quote: any, idx: number) => (
                          <div key={idx} style={{ marginBottom: '2px', fontSize: '7px' }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                              <span style={{ color: COLORS.GRAY }}>{quote.time}</span>
                              <span style={{ color: COLORS.GREEN }}>B: ${formatNumber(quote.bid_price, 2)}</span>
                              <span style={{ color: COLORS.RED }}>A: ${formatNumber(quote.ask_price, 2)}</span>
                            </div>
                          </div>
                        ))}
                      </div>
                    ) : null}
                  </div>
                </div>
              </div>
            )}

            {/* MARKET DATA TAB */}
            {activeTab === 'market' && (
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>

                {/* Ticker Search */}
                <div>
                  <div style={{ color: COLORS.ORANGE, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    TICKER SEARCH ({allTickers.length})
                  </div>
                  {allTickers.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {allTickers.slice(0, 8).map((t: any, i: number) => (
                        <div key={i} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px', paddingBottom: '2px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                          <span style={{ color: COLORS.CYAN, fontWeight: 'bold' }}>{t.ticker}</span>
                          <span style={{ color: COLORS.WHITE, fontSize: '7px' }}>{t.name?.substring(0, 20) || 'N/A'}</span>
                          <span style={{ color: COLORS.YELLOW }}>{t.type}</span>
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No results</div>
                  )}
                </div>

                {/* Market Holidays */}
                <div>
                  <div style={{ color: COLORS.BLUE, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    MARKET HOLIDAYS ({marketHolidays.length})
                  </div>
                  {marketHolidays.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {marketHolidays.slice(0, 8).map((holiday: any, i: number) => (
                        <div key={i} style={{ marginBottom: '3px', paddingBottom: '3px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                            <span style={{ color: COLORS.CYAN, fontWeight: 'bold' }}>{holiday.date}</span>
                            <span style={{ color: holiday.status === 'closed' ? COLORS.RED : COLORS.YELLOW, fontSize: '7px' }}>{holiday.status?.toUpperCase()}</span>
                          </div>
                          <div style={{ color: COLORS.WHITE, fontSize: '7px' }}>{holiday.name}</div>
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No holidays</div>
                  )}
                </div>
              </div>
            )}

            {/* REFERENCE DATA TAB */}
            {activeTab === 'reference' && (
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>

                {/* Ticker Types */}
                <div>
                  <div style={{ color: COLORS.BLUE, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    TICKER TYPES ({tickerTypes.length})
                  </div>
                  {tickerTypes.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {tickerTypes.slice(0, 8).map((tt: any, i: number) => (
                        <div key={i} style={{ marginBottom: '3px', paddingBottom: '3px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                          <div style={{ color: COLORS.CYAN, fontWeight: 'bold' }}>{tt.code}</div>
                          <div style={{ color: COLORS.WHITE, fontSize: '7px' }}>{tt.description?.substring(0, 30)}</div>
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No types</div>
                  )}
                </div>

                {/* Exchanges */}
                <div>
                  <div style={{ color: COLORS.GREEN, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    EXCHANGES ({exchanges.length})
                  </div>
                  {exchanges.length > 0 ? (
                    <div style={{ fontSize: '8px' }}>
                      {exchanges.slice(0, 8).map((ex: any, i: number) => (
                        <div key={i} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px', paddingBottom: '2px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
                          <span style={{ color: COLORS.CYAN, fontWeight: 'bold' }}>{ex.mic}</span>
                          <span style={{ color: COLORS.WHITE, fontSize: '7px' }}>{ex.name?.substring(0, 20)}</span>
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '10px', fontSize: '8px' }}>No exchanges</div>
                  )}
                </div>

                {/* Events & Condition Codes */}
                <div>
                  <div style={{ color: COLORS.YELLOW, fontSize: '9px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    EVENTS & CODES
                  </div>
                  <div style={{ fontSize: '8px' }}>
                    {tickerEvents && tickerEvents.events && tickerEvents.events.length > 0 ? (
                      <div style={{ marginBottom: '6px' }}>
                        <div style={{ color: COLORS.CYAN, marginBottom: '3px', fontSize: '8px' }}>Events: {tickerEvents.events.length}</div>
                        {tickerEvents.events.slice(0, 3).map((evt: any, i: number) => (
                          <div key={i} style={{ color: COLORS.WHITE, fontSize: '7px', marginBottom: '2px' }}>
                            {evt.date} - {evt.type}
                          </div>
                        ))}
                      </div>
                    ) : null}
                    {conditionCodes.length > 0 ? (
                      <div>
                        <div style={{ color: COLORS.CYAN, marginBottom: '3px', fontSize: '8px' }}>Codes: {conditionCodes.length}</div>
                        {conditionCodes.slice(0, 4).map((cc: any, i: number) => (
                          <div key={i} style={{ color: COLORS.WHITE, fontSize: '7px', marginBottom: '2px' }}>
                            {cc.id} - {cc.type}
                          </div>
                        ))}
                      </div>
                    ) : null}
                  </div>
                </div>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* FOOTER - Compact */}
      <div style={{
        borderTop: `1px solid ${COLORS.BORDER}`,
        backgroundColor: COLORS.PANEL_BG,
        padding: '4px 10px',
        fontSize: '8px',
        color: COLORS.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
        flexShrink: 0,
      }}>
        <span>Polygon.io API | 31/31 Endpoints | Institutional Data</span>
        <span>{symbol} | {new Date().toLocaleString()}</span>
      </div>

      {/* Custom Scrollbar - Only main window scrolls */}
      <style>{`
        .custom-scrollbar::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        .custom-scrollbar::-webkit-scrollbar-track {
          background: #0a0a0a;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb {
          background: rgba(255, 165, 0, 0.3);
          border-radius: 4px;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb:hover {
          background: rgba(255, 165, 0, 0.5);
        }
      `}</style>
    </div>
  );
};

export default PolygonEqTab;
