import React, { useState, useEffect, useRef } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { invoke } from '@tauri-apps/api/core';
import { createChart, CandlestickSeries, HistogramSeries, LineSeries, BarSeries } from 'lightweight-charts';

const COLORS = {
  ORANGE: 'colors.primary',
  WHITE: 'colors.text',
  RED: 'colors.alert',
  GREEN: 'colors.secondary',
  YELLOW: 'colors.warning',
  GRAY: 'colors.textMuted',
  BLUE: 'colors.info',
  CYAN: 'colors.accent',
  DARK_BG: 'colors.background',
  PANEL_BG: 'colors.panel',
  BORDER: '#333333',
  MAGENTA: '#FF00FF',
  PURPLE: '#9D4EDD',
};

interface StockInfo {
  symbol: string;
  company_name: string;
  sector: string;
  industry: string;
  market_cap: number | null;
  pe_ratio: number | null;
  forward_pe: number | null;
  dividend_yield: number | null;
  beta: number | null;
  fifty_two_week_high: number | null;
  fifty_two_week_low: number | null;
  average_volume: number | null;
  description: string;
  website: string;
  country: string;
  currency: string;
  exchange: string;
  employees: number | null;
  current_price: number | null;
  target_high_price: number | null;
  target_low_price: number | null;
  target_mean_price: number | null;
  recommendation_mean: number | null;
  recommendation_key: string | null;
  number_of_analyst_opinions: number | null;
  total_cash: number | null;
  total_debt: number | null;
  total_revenue: number | null;
  revenue_per_share: number | null;
  return_on_assets: number | null;
  return_on_equity: number | null;
  gross_profits: number | null;
  free_cashflow: number | null;
  operating_cashflow: number | null;
  earnings_growth: number | null;
  revenue_growth: number | null;
  gross_margins: number | null;
  operating_margins: number | null;
  ebitda_margins: number | null;
  profit_margins: number | null;
  book_value: number | null;
  price_to_book: number | null;
  enterprise_value: number | null;
  enterprise_to_revenue: number | null;
  enterprise_to_ebitda: number | null;
  shares_outstanding: number | null;
  float_shares: number | null;
  held_percent_insiders: number | null;
  held_percent_institutions: number | null;
  short_ratio: number | null;
  short_percent_of_float: number | null;
  peg_ratio: number | null;
}

interface HistoricalData {
  symbol: string;
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

interface QuoteData {
  symbol: string;
  price: number;
  change: number;
  change_percent: number;
  volume: number | null;
  high: number | null;
  low: number | null;
  open: number | null;
  previous_close: number | null;
}

interface FinancialsData {
  symbol: string;
  income_statement: Record<string, Record<string, number>>;
  balance_sheet: Record<string, Record<string, number>>;
  cash_flow: Record<string, Record<string, number>>;
  timestamp: number;
}

const EquityResearchTab: React.FC = () => {
  const { colors, fontSize: themeFontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [searchSymbol, setSearchSymbol] = useState('');
  const [currentSymbol, setCurrentSymbol] = useState('AAPL');
  const [stockInfo, setStockInfo] = useState<StockInfo | null>(null);
  const [quoteData, setQuoteData] = useState<QuoteData | null>(null);
  const [historicalData, setHistoricalData] = useState<HistoricalData[]>([]);
  const [financials, setFinancials] = useState<FinancialsData | null>(null);
  const [loading, setLoading] = useState(false);
  const [chartPeriod, setChartPeriod] = useState<'1M' | '3M' | '6M' | '1Y' | '5Y'>('6M');
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeTab, setActiveTab] = useState<'overview' | 'financials' | 'analysis'>('overview');
  const [fontSize, setFontSize] = useState(12);
  const [selectedMetrics, setSelectedMetrics] = useState<string[]>(['Total Revenue', 'Gross Profit', 'Operating Income', 'Net Income', 'EBITDA', 'Basic EPS', 'Diluted EPS']);
  const [showYearsCount, setShowYearsCount] = useState(4);

  // 3 Chart Metrics Selection
  const [chart1Metrics, setChart1Metrics] = useState<string[]>(['Total Revenue', 'Gross Profit']);
  const [chart2Metrics, setChart2Metrics] = useState<string[]>(['Net Income', 'Operating Income']);
  const [chart3Metrics, setChart3Metrics] = useState<string[]>(['EBITDA']);

  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<any>(null);
  const candlestickSeriesRef = useRef<any>(null);
  const volumeSeriesRef = useRef<any>(null);

  const financialChart1Ref = useRef<HTMLDivElement>(null);
  const financialChart2Ref = useRef<HTMLDivElement>(null);
  const financialChart3Ref = useRef<HTMLDivElement>(null);
  const financialChart1InstanceRef = useRef<any>(null);
  const financialChart2InstanceRef = useRef<any>(null);
  const financialChart3InstanceRef = useRef<any>(null);

  // Data Cache: { symbol: { info, quote, historical: { period: data }, financials, timestamp } }
  const dataCache = useRef<Record<string, {
    stockInfo: StockInfo;
    quoteData: QuoteData;
    historicalData: Record<string, HistoricalData[]>;
    financials: FinancialsData;
    timestamp: number;
  }>>({});

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  const fetchStockData = async (symbol: string, forceRefresh = false) => {
    setLoading(true);
    console.log('Fetching data for:', symbol, '| Force refresh:', forceRefresh);

    try {
      // Check cache first
      const cached = dataCache.current[symbol];
      const now = Date.now();
      const CACHE_DURATION = 5 * 60 * 1000; // 5 minutes

      if (cached && !forceRefresh && (now - cached.timestamp < CACHE_DURATION)) {
        console.log('Using cached data for', symbol);
        setStockInfo(cached.stockInfo);
        setQuoteData(cached.quoteData);
        setFinancials(cached.financials);

        // Check if we have historical data for this period
        if (cached.historicalData[chartPeriod]) {
          setHistoricalData(cached.historicalData[chartPeriod]);
          setLoading(false);
          return;
        }
      }

      // Fetch fresh data
      console.log('Fetching fresh data for', symbol);

      // Fetch quote
      const quoteResponse: any = await invoke('get_market_quote', { symbol });
      let newQuote: QuoteData | null = null;
      if (quoteResponse.success && quoteResponse.data) {
        setQuoteData(quoteResponse.data);
        newQuote = quoteResponse.data;
      }

      // Fetch stock info
      const infoResponse: any = await invoke('get_stock_info', { symbol });
      let newInfo: StockInfo | null = null;
      if (infoResponse.success && infoResponse.data) {
        setStockInfo(infoResponse.data);
        newInfo = infoResponse.data;
      }

      // Fetch historical data
      const endDate = new Date();
      const startDate = new Date();
      switch (chartPeriod) {
        case '1M': startDate.setMonth(endDate.getMonth() - 1); break;
        case '3M': startDate.setMonth(endDate.getMonth() - 3); break;
        case '6M': startDate.setMonth(endDate.getMonth() - 6); break;
        case '1Y': startDate.setFullYear(endDate.getFullYear() - 1); break;
        case '5Y': startDate.setFullYear(endDate.getFullYear() - 5); break;
      }

      const historicalResponse: any = await invoke('get_historical_data', {
        symbol,
        startDate: startDate.toISOString().split('T')[0],
        endDate: endDate.toISOString().split('T')[0],
      });
      let newHistorical: HistoricalData[] = [];
      if (historicalResponse.success && historicalResponse.data) {
        setHistoricalData(historicalResponse.data);
        newHistorical = historicalResponse.data;
      }

      // Fetch financials
      const financialsResponse: any = await invoke('get_financials', { symbol });
      let newFinancials: FinancialsData | null = null;
      if (financialsResponse.success && financialsResponse.data) {
        setFinancials(financialsResponse.data);
        newFinancials = financialsResponse.data;
      }

      // Update cache
      if (newInfo && newQuote && newFinancials) {
        dataCache.current[symbol] = {
          stockInfo: newInfo,
          quoteData: newQuote,
          historicalData: {
            ...((cached?.historicalData) || {}),
            [chartPeriod]: newHistorical,
          },
          financials: newFinancials,
          timestamp: now,
        };
      }
    } catch (error) {
      console.error('Error fetching stock data:', error);
    } finally {
      setLoading(false);
    }
  };

  // Initialize price chart with volume
  useEffect(() => {
    if (chartContainerRef.current && !chartRef.current) {
      const chart = createChart(chartContainerRef.current, {
        layout: {
          background: { color: COLORS.DARK_BG },
          textColor: COLORS.WHITE,
        },
        grid: {
          vertLines: { color: COLORS.BORDER },
          horzLines: { color: COLORS.BORDER },
        },
        width: chartContainerRef.current.clientWidth,
        height: 400,
        timeScale: {
          borderColor: COLORS.GRAY,
          timeVisible: true,
        },
        rightPriceScale: {
          borderColor: COLORS.GRAY,
        },
      });

      const candlestickSeries = chart.addSeries(CandlestickSeries, {
        upColor: COLORS.GREEN,
        downColor: COLORS.RED,
        borderVisible: false,
        wickUpColor: COLORS.GREEN,
        wickDownColor: COLORS.RED,
      });

      const volumeSeries = chart.addSeries(HistogramSeries, {
        color: COLORS.BLUE,
        priceFormat: {
          type: 'volume',
        },
        priceScaleId: '',
      });

      volumeSeries.priceScale().applyOptions({
        scaleMargins: {
          top: 0.8,
          bottom: 0,
        },
      });

      chartRef.current = chart;
      candlestickSeriesRef.current = candlestickSeries;
      volumeSeriesRef.current = volumeSeries;

      const handleResize = () => {
        if (chartContainerRef.current && chartRef.current) {
          chartRef.current.applyOptions({ width: chartContainerRef.current.clientWidth });
        }
      };

      window.addEventListener('resize', handleResize);
      return () => {
        window.removeEventListener('resize', handleResize);
        chart.remove();
        chartRef.current = null;
        candlestickSeriesRef.current = null;
        volumeSeriesRef.current = null;
      };
    }
  }, []);

  // Update price chart data
  useEffect(() => {
    if (candlestickSeriesRef.current && volumeSeriesRef.current && historicalData.length > 0) {
      const chartData = historicalData.map((d) => ({
        time: Math.floor(d.timestamp) as any,
        open: d.open,
        high: d.high,
        low: d.low,
        close: d.close,
      })).sort((a, b) => a.time - b.time);

      const volumeData = historicalData.map((d) => ({
        time: Math.floor(d.timestamp) as any,
        value: d.volume,
        color: d.close >= d.open ? COLORS.GREEN + '80' : COLORS.RED + '80',
      })).sort((a, b) => a.time - b.time);

      candlestickSeriesRef.current.setData(chartData);
      volumeSeriesRef.current.setData(volumeData);

      if (chartRef.current) {
        chartRef.current.timeScale().fitContent();
      }
    }
  }, [historicalData]);

  // Helper function to create a financial chart with dynamic metrics
  const createFinancialChart = (
    container: HTMLDivElement | null,
    metrics: string[],
    colors: string[]
  ) => {
    if (!container || !financials) return null;

    const chart = createChart(container, {
      layout: {
        background: { color: COLORS.DARK_BG },
        textColor: COLORS.WHITE,
      },
      grid: {
        vertLines: { color: COLORS.BORDER },
        horzLines: { color: COLORS.BORDER },
      },
      width: container.clientWidth,
      height: 250,
      timeScale: {
        borderColor: COLORS.GRAY,
        timeVisible: true,
      },
      rightPriceScale: {
        borderColor: COLORS.GRAY,
      },
    });

    const periods = Object.keys(financials.income_statement);
    const seriesArray: any[] = [];

    metrics.forEach((metric, idx) => {
      const color = colors[idx % colors.length];
      const series = chart.addSeries(LineSeries, {
        color,
        lineWidth: 2,
        title: metric,
      });

      const data: any[] = [];
      periods.forEach((period) => {
        const timestamp = new Date(period).getTime() / 1000;
        const statements = financials.income_statement[period];

        if (statements[metric] !== undefined && statements[metric] !== null) {
          const value = metric.includes('EPS')
            ? statements[metric]
            : statements[metric] / 1e9; // Convert to billions
          data.push({ time: timestamp, value });
        }
      });

      if (data.length > 0) {
        series.setData(data.sort((a, b) => a.time - b.time));
      }
      seriesArray.push(series);
    });

    chart.timeScale().fitContent();
    return chart;
  };

  // Update financial charts when data or metrics change
  useEffect(() => {
    if (activeTab === 'financials' && financials) {
      // Cleanup existing charts
      if (financialChart1InstanceRef.current) {
        financialChart1InstanceRef.current.remove();
        financialChart1InstanceRef.current = null;
      }
      if (financialChart2InstanceRef.current) {
        financialChart2InstanceRef.current.remove();
        financialChart2InstanceRef.current = null;
      }
      if (financialChart3InstanceRef.current) {
        financialChart3InstanceRef.current.remove();
        financialChart3InstanceRef.current = null;
      }

      // Create new charts
      const chartColors = [COLORS.GREEN, COLORS.CYAN, COLORS.ORANGE, COLORS.YELLOW, COLORS.MAGENTA, COLORS.BLUE];

      if (financialChart1Ref.current && chart1Metrics.length > 0) {
        financialChart1InstanceRef.current = createFinancialChart(
          financialChart1Ref.current,
          chart1Metrics,
          chartColors
        );
      }

      if (financialChart2Ref.current && chart2Metrics.length > 0) {
        financialChart2InstanceRef.current = createFinancialChart(
          financialChart2Ref.current,
          chart2Metrics,
          chartColors
        );
      }

      if (financialChart3Ref.current && chart3Metrics.length > 0) {
        financialChart3InstanceRef.current = createFinancialChart(
          financialChart3Ref.current,
          chart3Metrics,
          chartColors
        );
      }

      // Handle window resize
      const handleResize = () => {
        if (financialChart1Ref.current && financialChart1InstanceRef.current) {
          financialChart1InstanceRef.current.applyOptions({ width: financialChart1Ref.current.clientWidth });
        }
        if (financialChart2Ref.current && financialChart2InstanceRef.current) {
          financialChart2InstanceRef.current.applyOptions({ width: financialChart2Ref.current.clientWidth });
        }
        if (financialChart3Ref.current && financialChart3InstanceRef.current) {
          financialChart3InstanceRef.current.applyOptions({ width: financialChart3Ref.current.clientWidth });
        }
      };

      window.addEventListener('resize', handleResize);
      return () => {
        window.removeEventListener('resize', handleResize);
        if (financialChart1InstanceRef.current) financialChart1InstanceRef.current.remove();
        if (financialChart2InstanceRef.current) financialChart2InstanceRef.current.remove();
        if (financialChart3InstanceRef.current) financialChart3InstanceRef.current.remove();
      };
    }
  }, [financials, activeTab, chart1Metrics, chart2Metrics, chart3Metrics]);

  // Load initial data
  useEffect(() => {
    fetchStockData(currentSymbol);
  }, [currentSymbol, chartPeriod]);

  const handleSearch = () => {
    if (searchSymbol.trim()) {
      setCurrentSymbol(searchSymbol.trim().toUpperCase());
      setSearchSymbol('');
    }
  };

  const formatNumber = (num: number | null | undefined, decimals = 2) => {
    if (num === null || num === undefined) return 'N/A';
    return num.toLocaleString(undefined, { minimumFractionDigits: decimals, maximumFractionDigits: decimals });
  };

  const formatLargeNumber = (num: number | null | undefined) => {
    if (num === null || num === undefined) return 'N/A';
    if (num >= 1e12) return `$${(num / 1e12).toFixed(2)}T`;
    if (num >= 1e9) return `$${(num / 1e9).toFixed(2)}B`;
    if (num >= 1e6) return `$${(num / 1e6).toFixed(2)}M`;
    return `$${num.toFixed(2)}`;
  };

  const formatPercent = (num: number | null | undefined) => {
    if (num === null || num === undefined) return 'N/A';
    return `${(num * 100).toFixed(2)}%`;
  };

  const currentPrice = quoteData?.price || stockInfo?.current_price || 0;
  const priceChange = quoteData?.change || 0;
  const priceChangePercent = quoteData?.change_percent || 0;

  const getRecommendationColor = (key: string | null) => {
    if (!key) return COLORS.GRAY;
    if (key === 'buy' || key === 'strong_buy') return COLORS.GREEN;
    if (key === 'sell' || key === 'strong_sell') return COLORS.RED;
    return COLORS.YELLOW;
  };

  const getRecommendationText = (key: string | null) => {
    if (!key) return 'N/A';
    return key.toUpperCase().replace('_', ' ');
  };

  // Helper function to get year from period string
  const getYearFromPeriod = (period: string) => {
    return new Date(period).getFullYear();
  };

  // Get all available metrics from income statement
  const getAllMetrics = () => {
    if (!financials || Object.keys(financials.income_statement).length === 0) return [];
    const firstPeriod = Object.keys(financials.income_statement)[0];
    return Object.keys(financials.income_statement[firstPeriod]);
  };

  const toggleMetric = (metric: string) => {
    setSelectedMetrics(prev =>
      prev.includes(metric)
        ? prev.filter(m => m !== metric)
        : [...prev, metric]
    );
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: COLORS.DARK_BG,
      color: COLORS.WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: `1px solid ${COLORS.BORDER}`,
        padding: '6px 12px',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: COLORS.ORANGE, fontWeight: 'bold', fontSize: '13px' }}>EQUITY RESEARCH TERMINAL</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <input
                type="text"
                value={searchSymbol}
                onChange={(e) => setSearchSymbol(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
                placeholder="Enter symbol..."
                style={{
                  backgroundColor: COLORS.DARK_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  color: COLORS.WHITE,
                  padding: '3px 8px',
                  fontSize: '11px',
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
                  padding: '3px 10px',
                  fontSize: '10px',
                  cursor: 'pointer',
                  fontWeight: 'bold',
                }}
              >
                SEARCH
              </button>
            </div>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '10px' }}>
            <span style={{ color: COLORS.GRAY }}>{currentTime.toLocaleTimeString()}</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <span style={{ color: COLORS.CYAN, fontWeight: 'bold' }}>{currentSymbol}</span>
            {loading && <span style={{ color: COLORS.YELLOW }}>● LOADING...</span>}
            {!loading && <span style={{ color: COLORS.GREEN }}>● LIVE</span>}
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        scrollbarColor: 'rgba(120, 120, 120, 0.3) transparent',
        scrollbarWidth: 'thin',
      }} className="custom-scrollbar">

        {/* Stock Header with Price */}
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.BORDER}`,
          padding: '12px',
          margin: '8px',
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '4px' }}>
                <span style={{ color: COLORS.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>{currentSymbol}</span>
                <span style={{ color: COLORS.WHITE, fontSize: '14px' }}>{stockInfo?.company_name || 'Loading...'}</span>
                {stockInfo?.sector && (
                  <span style={{
                    color: COLORS.BLUE,
                    fontSize: '10px',
                    padding: '2px 6px',
                    backgroundColor: 'rgba(100,150,250,0.2)',
                    border: `1px solid ${COLORS.BLUE}`,
                  }}>
                    {stockInfo.sector}
                  </span>
                )}
                {stockInfo?.recommendation_key && (
                  <span style={{
                    color: getRecommendationColor(stockInfo.recommendation_key),
                    fontSize: '10px',
                    padding: '2px 6px',
                    backgroundColor: `${getRecommendationColor(stockInfo.recommendation_key)}20`,
                    border: `1px solid ${getRecommendationColor(stockInfo.recommendation_key)}`,
                    fontWeight: 'bold',
                  }}>
                    {getRecommendationText(stockInfo.recommendation_key)}
                  </span>
                )}
              </div>
              <div style={{ fontSize: '10px', color: COLORS.GRAY }}>
                {stockInfo?.exchange || 'N/A'} | {stockInfo?.industry || 'N/A'} | {stockInfo?.country || 'N/A'}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{ fontSize: '24px', fontWeight: 'bold', color: COLORS.WHITE, marginBottom: '2px' }}>
                ${formatNumber(currentPrice)}
              </div>
              <div style={{ fontSize: '12px', color: priceChange >= 0 ? COLORS.GREEN : COLORS.RED }}>
                {priceChange >= 0 ? '▲' : '▼'} ${Math.abs(priceChange).toFixed(2)} ({priceChangePercent.toFixed(2)}%)
              </div>
            </div>
          </div>
        </div>

        {/* Tab Navigation */}
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.BORDER}`,
          margin: '0 8px 8px 8px',
          display: 'flex',
          gap: '1px',
        }}>
          {(['overview', 'financials', 'analysis'] as const).map((tab) => (
            <button
              key={tab}
              onClick={() => setActiveTab(tab)}
              style={{
                flex: 1,
                backgroundColor: activeTab === tab ? COLORS.ORANGE : COLORS.DARK_BG,
                border: 'none',
                color: activeTab === tab ? COLORS.DARK_BG : COLORS.WHITE,
                padding: '8px 16px',
                fontSize: '11px',
                cursor: 'pointer',
                fontWeight: activeTab === tab ? 'bold' : 'normal',
                textTransform: 'uppercase',
              }}
            >
              {tab}
            </button>
          ))}
        </div>

        {/* Overview Tab */}
        {activeTab === 'overview' && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 2fr 1fr', gap: '8px', padding: '0 8px 8px 8px' }}>
            {/* Left Column - Quick Stats */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {/* Today's Trading */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  TODAY'S TRADING
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: '10px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>OPEN:</span>
                    <span style={{ color: COLORS.CYAN }}>${formatNumber(quoteData?.open)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>HIGH:</span>
                    <span style={{ color: COLORS.GREEN }}>${formatNumber(quoteData?.high)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>LOW:</span>
                    <span style={{ color: COLORS.RED }}>${formatNumber(quoteData?.low)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>PREV CLOSE:</span>
                    <span style={{ color: COLORS.WHITE }}>${formatNumber(quoteData?.previous_close)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>VOLUME:</span>
                    <span style={{ color: COLORS.YELLOW }}>{formatNumber(quoteData?.volume || 0, 0)}</span>
                  </div>
                </div>
              </div>

              {/* Valuation Metrics */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.CYAN, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  VALUATION
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: '10px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>MARKET CAP:</span>
                    <span style={{ color: COLORS.CYAN }}>{formatLargeNumber(stockInfo?.market_cap)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>P/E RATIO:</span>
                    <span style={{ color: COLORS.YELLOW }}>{formatNumber(stockInfo?.pe_ratio)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>FWD P/E:</span>
                    <span style={{ color: COLORS.YELLOW }}>{formatNumber(stockInfo?.forward_pe)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>PEG RATIO:</span>
                    <span style={{ color: COLORS.YELLOW }}>{formatNumber(stockInfo?.peg_ratio)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>P/B RATIO:</span>
                    <span style={{ color: COLORS.CYAN }}>{formatNumber(stockInfo?.price_to_book)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>DIV YIELD:</span>
                    <span style={{ color: COLORS.GREEN }}>
                      {formatPercent(stockInfo?.dividend_yield)}
                    </span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>BETA:</span>
                    <span style={{ color: COLORS.WHITE }}>{formatNumber(stockInfo?.beta)}</span>
                  </div>
                </div>
              </div>

              {/* Analyst Targets */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.MAGENTA, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  ANALYST TARGETS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: '10px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>HIGH:</span>
                    <span style={{ color: COLORS.GREEN }}>${formatNumber(stockInfo?.target_high_price)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>MEAN:</span>
                    <span style={{ color: COLORS.YELLOW }}>${formatNumber(stockInfo?.target_mean_price)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>LOW:</span>
                    <span style={{ color: COLORS.RED }}>${formatNumber(stockInfo?.target_low_price)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>ANALYSTS:</span>
                    <span style={{ color: COLORS.CYAN }}>{stockInfo?.number_of_analyst_opinions || 'N/A'}</span>
                  </div>
                </div>
              </div>
            </div>

            {/* Center Column - Chart */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {/* Chart Period Selector */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '6px 8px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
              }}>
                <span style={{ color: COLORS.GRAY, fontSize: '10px' }}>PERIOD:</span>
                {(['1M', '3M', '6M', '1Y', '5Y'] as const).map((period) => (
                  <button
                    key={period}
                    onClick={() => setChartPeriod(period)}
                    style={{
                      backgroundColor: chartPeriod === period ? COLORS.ORANGE : COLORS.DARK_BG,
                      border: `1px solid ${COLORS.BORDER}`,
                      color: chartPeriod === period ? COLORS.DARK_BG : COLORS.WHITE,
                      padding: '4px 10px',
                      fontSize: '9px',
                      cursor: 'pointer',
                      fontWeight: chartPeriod === period ? 'bold' : 'normal',
                    }}
                  >
                    {period}
                  </button>
                ))}
              </div>

              {/* Chart with Volume */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
                  PRICE CHART & VOLUME - {chartPeriod}
                </div>
                <div ref={chartContainerRef} style={{ backgroundColor: COLORS.DARK_BG }} />
              </div>

              {/* Company Description */}
              {stockInfo?.description && stockInfo.description !== 'N/A' && (
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '8px',
                }}>
                  <div style={{ color: COLORS.CYAN, fontSize: '11px', fontWeight: 'bold', marginBottom: '6px' }}>
                    COMPANY OVERVIEW
                  </div>
                  <div style={{ color: COLORS.WHITE, fontSize: '11px', lineHeight: '1.6', maxHeight: '120px', overflow: 'auto' }}>
                    {stockInfo.description}
                  </div>
                </div>
              )}
            </div>

            {/* Right Column - Additional Metrics */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {/* 52 Week Range */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.YELLOW, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  52 WEEK RANGE
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: '10px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>HIGH:</span>
                    <span style={{ color: COLORS.GREEN }}>${formatNumber(stockInfo?.fifty_two_week_high)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>LOW:</span>
                    <span style={{ color: COLORS.RED }}>${formatNumber(stockInfo?.fifty_two_week_low)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>AVG VOL:</span>
                    <span style={{ color: COLORS.CYAN }}>{formatNumber(stockInfo?.average_volume, 0)}</span>
                  </div>
                </div>
              </div>

              {/* Profitability */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.GREEN, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  PROFITABILITY
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: '10px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>GROSS MARGIN:</span>
                    <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.gross_margins)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>OPERATING MARGIN:</span>
                    <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.operating_margins)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>PROFIT MARGIN:</span>
                    <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.profit_margins)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>ROA:</span>
                    <span style={{ color: COLORS.CYAN }}>{formatPercent(stockInfo?.return_on_assets)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>ROE:</span>
                    <span style={{ color: COLORS.CYAN }}>{formatPercent(stockInfo?.return_on_equity)}</span>
                  </div>
                </div>
              </div>

              {/* Growth */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.BLUE, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  GROWTH
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: '10px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>REVENUE GROWTH:</span>
                    <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.revenue_growth)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>EARNINGS GROWTH:</span>
                    <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.earnings_growth)}</span>
                  </div>
                </div>
              </div>

              {/* Share Statistics */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.CYAN, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  SHARE STATS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: '10px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>SHARES OUT:</span>
                    <span style={{ color: COLORS.CYAN }}>{formatLargeNumber(stockInfo?.shares_outstanding)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>FLOAT:</span>
                    <span style={{ color: COLORS.CYAN }}>{formatLargeNumber(stockInfo?.float_shares)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>INSIDERS:</span>
                    <span style={{ color: COLORS.YELLOW }}>{formatPercent(stockInfo?.held_percent_insiders)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>INSTITUTIONS:</span>
                    <span style={{ color: COLORS.YELLOW }}>{formatPercent(stockInfo?.held_percent_institutions)}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>SHORT %:</span>
                    <span style={{ color: COLORS.RED }}>{formatPercent(stockInfo?.short_percent_of_float)}</span>
                  </div>
                </div>
              </div>

              {/* Company Info */}
              {stockInfo && (
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '8px',
                }}>
                  <div style={{ color: COLORS.WHITE, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                    COMPANY INFO
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: '9px' }}>
                    <div><span style={{ color: COLORS.GRAY }}>EMPLOYEES:</span> <span style={{ color: COLORS.CYAN }}>{formatNumber(stockInfo.employees, 0)}</span></div>
                    {stockInfo.website && stockInfo.website !== 'N/A' && (
                      <div><span style={{ color: COLORS.GRAY }}>WEBSITE:</span> <span style={{ color: COLORS.BLUE }}>{stockInfo.website.substring(0, 30)}</span></div>
                    )}
                  </div>
                </div>
              )}
            </div>
          </div>
        )}

        {/* Financials Tab */}
        {activeTab === 'financials' && financials && (
          <div style={{ padding: '0 8px 8px 8px' }}>
            {/* Controls Panel */}
            <div style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '10px 12px',
              marginBottom: '8px',
              display: 'flex',
              gap: '20px',
              alignItems: 'center',
              flexWrap: 'wrap',
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ color: COLORS.GRAY, fontSize: `${fontSize}px` }}>FONT SIZE:</span>
                <button
                  onClick={() => setFontSize(Math.max(10, fontSize - 1))}
                  style={{
                    backgroundColor: COLORS.DARK_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    padding: '4px 8px',
                    fontSize: `${fontSize}px`,
                    cursor: 'pointer',
                  }}
                >
                  -
                </button>
                <span style={{ color: COLORS.ORANGE, fontSize: `${fontSize}px`, fontWeight: 'bold', minWidth: '30px', textAlign: 'center' }}>{fontSize}px</span>
                <button
                  onClick={() => setFontSize(Math.min(20, fontSize + 1))}
                  style={{
                    backgroundColor: COLORS.DARK_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    padding: '4px 8px',
                    fontSize: `${fontSize}px`,
                    cursor: 'pointer',
                  }}
                >
                  +
                </button>
              </div>

              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ color: COLORS.GRAY, fontSize: `${fontSize}px` }}>SHOW YEARS:</span>
                {[2, 3, 4, 5].map((count) => (
                  <button
                    key={count}
                    onClick={() => setShowYearsCount(count)}
                    style={{
                      backgroundColor: showYearsCount === count ? COLORS.ORANGE : COLORS.DARK_BG,
                      border: `1px solid ${COLORS.BORDER}`,
                      color: showYearsCount === count ? COLORS.DARK_BG : COLORS.WHITE,
                      padding: '4px 10px',
                      fontSize: `${fontSize - 1}px`,
                      cursor: 'pointer',
                      fontWeight: showYearsCount === count ? 'bold' : 'normal',
                    }}
                  >
                    {count}
                  </button>
                ))}
              </div>

              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ color: COLORS.GRAY, fontSize: `${fontSize}px` }}>SELECT METRICS:</span>
                <button
                  onClick={() => {
                    const allMetrics = getAllMetrics();
                    setSelectedMetrics(selectedMetrics.length === allMetrics.length ? [] : allMetrics);
                  }}
                  style={{
                    backgroundColor: COLORS.BLUE,
                    border: 'none',
                    color: COLORS.WHITE,
                    padding: '4px 10px',
                    fontSize: `${fontSize - 1}px`,
                    cursor: 'pointer',
                    fontWeight: 'bold',
                  }}
                >
                  {selectedMetrics.length === getAllMetrics().length ? 'DESELECT ALL' : 'SELECT ALL'}
                </button>
              </div>
            </div>

            {/* Metric Selector */}
            <div style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '12px',
              marginBottom: '8px',
            }}>
              <div style={{ color: COLORS.CYAN, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '10px' }}>
                SELECT METRICS TO COMPARE
              </div>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                {getAllMetrics().map((metric) => (
                  <button
                    key={metric}
                    onClick={() => toggleMetric(metric)}
                    style={{
                      backgroundColor: selectedMetrics.includes(metric) ? COLORS.GREEN : COLORS.DARK_BG,
                      border: `1px solid ${selectedMetrics.includes(metric) ? COLORS.GREEN : COLORS.BORDER}`,
                      color: selectedMetrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                      padding: '6px 12px',
                      fontSize: `${fontSize - 1}px`,
                      cursor: 'pointer',
                      fontWeight: selectedMetrics.includes(metric) ? 'bold' : 'normal',
                      borderRadius: '3px',
                    }}
                  >
                    {metric}
                  </button>
                ))}
              </div>
            </div>

            {/* Financial Comparison Charts - 3 Charts in a Row */}
            <div style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '12px',
              marginBottom: '8px',
            }}>
              <div style={{ color: COLORS.ORANGE, fontSize: `${fontSize + 3}px`, fontWeight: 'bold', marginBottom: '12px' }}>
                FINANCIAL TRENDS COMPARISON (in Billions $)
              </div>

              {/* Chart Controls */}
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px', marginBottom: '12px' }}>
                {/* Chart 1 Selector */}
                <div>
                  <div style={{ color: COLORS.CYAN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px' }}>
                    CHART 1 METRICS:
                  </div>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {getAllMetrics().slice(0, 8).map((metric) => (
                      <button
                        key={metric}
                        onClick={() => {
                          setChart1Metrics(prev =>
                            prev.includes(metric)
                              ? prev.filter(m => m !== metric)
                              : [...prev, metric]
                          );
                        }}
                        style={{
                          backgroundColor: chart1Metrics.includes(metric) ? COLORS.GREEN : COLORS.DARK_BG,
                          border: `1px solid ${chart1Metrics.includes(metric) ? COLORS.GREEN : COLORS.BORDER}`,
                          color: chart1Metrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                          padding: '3px 6px',
                          fontSize: `${fontSize - 3}px`,
                          cursor: 'pointer',
                          fontWeight: chart1Metrics.includes(metric) ? 'bold' : 'normal',
                        }}
                      >
                        {metric.substring(0, 15)}
                      </button>
                    ))}
                  </div>
                </div>

                {/* Chart 2 Selector */}
                <div>
                  <div style={{ color: COLORS.CYAN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px' }}>
                    CHART 2 METRICS:
                  </div>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {getAllMetrics().slice(0, 8).map((metric) => (
                      <button
                        key={metric}
                        onClick={() => {
                          setChart2Metrics(prev =>
                            prev.includes(metric)
                              ? prev.filter(m => m !== metric)
                              : [...prev, metric]
                          );
                        }}
                        style={{
                          backgroundColor: chart2Metrics.includes(metric) ? COLORS.ORANGE : COLORS.DARK_BG,
                          border: `1px solid ${chart2Metrics.includes(metric) ? COLORS.ORANGE : COLORS.BORDER}`,
                          color: chart2Metrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                          padding: '3px 6px',
                          fontSize: `${fontSize - 3}px`,
                          cursor: 'pointer',
                          fontWeight: chart2Metrics.includes(metric) ? 'bold' : 'normal',
                        }}
                      >
                        {metric.substring(0, 15)}
                      </button>
                    ))}
                  </div>
                </div>

                {/* Chart 3 Selector */}
                <div>
                  <div style={{ color: COLORS.CYAN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px' }}>
                    CHART 3 METRICS:
                  </div>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {getAllMetrics().slice(0, 8).map((metric) => (
                      <button
                        key={metric}
                        onClick={() => {
                          setChart3Metrics(prev =>
                            prev.includes(metric)
                              ? prev.filter(m => m !== metric)
                              : [...prev, metric]
                          );
                        }}
                        style={{
                          backgroundColor: chart3Metrics.includes(metric) ? COLORS.MAGENTA : COLORS.DARK_BG,
                          border: `1px solid ${chart3Metrics.includes(metric) ? COLORS.MAGENTA : COLORS.BORDER}`,
                          color: chart3Metrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                          padding: '3px 6px',
                          fontSize: `${fontSize - 3}px`,
                          cursor: 'pointer',
                          fontWeight: chart3Metrics.includes(metric) ? 'bold' : 'normal',
                        }}
                      >
                        {metric.substring(0, 15)}
                      </button>
                    ))}
                  </div>
                </div>
              </div>

              {/* 3 Charts Side by Side */}
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
                <div>
                  <div style={{ color: COLORS.GREEN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px', textAlign: 'center' }}>
                    {chart1Metrics.join(', ') || 'Select metrics'}
                  </div>
                  <div ref={financialChart1Ref} style={{ backgroundColor: COLORS.DARK_BG }} />
                </div>
                <div>
                  <div style={{ color: COLORS.ORANGE, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px', textAlign: 'center' }}>
                    {chart2Metrics.join(', ') || 'Select metrics'}
                  </div>
                  <div ref={financialChart2Ref} style={{ backgroundColor: COLORS.DARK_BG }} />
                </div>
                <div>
                  <div style={{ color: COLORS.MAGENTA, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px', textAlign: 'center' }}>
                    {chart3Metrics.join(', ') || 'Select metrics'}
                  </div>
                  <div ref={financialChart3Ref} style={{ backgroundColor: COLORS.DARK_BG }} />
                </div>
              </div>
            </div>

            {/* Financial Comparison Table */}
            <div style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '12px',
              marginBottom: '8px',
            }}>
              <div style={{ color: COLORS.CYAN, fontSize: `${fontSize + 3}px`, fontWeight: 'bold', marginBottom: '12px' }}>
                YEAR-OVER-YEAR COMPARISON ({selectedMetrics.length} Metrics)
              </div>

              {selectedMetrics.length === 0 ? (
                <div style={{ color: COLORS.YELLOW, fontSize: `${fontSize + 1}px`, textAlign: 'center', padding: '20px' }}>
                  Please select at least one metric to compare
                </div>
              ) : (
                <div style={{ overflowX: 'auto' }} className="custom-scrollbar">
                  <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: `${fontSize}px` }}>
                    <thead>
                      <tr style={{ borderBottom: `2px solid ${COLORS.BORDER}` }}>
                        <th style={{ textAlign: 'left', padding: '10px', color: COLORS.GRAY, fontSize: `${fontSize + 1}px` }}>METRIC</th>
                        {Object.keys(financials.income_statement).slice(0, showYearsCount).map((period) => (
                          <th key={period} style={{ textAlign: 'right', padding: '10px', color: COLORS.ORANGE, fontSize: `${fontSize + 1}px` }}>
                            {getYearFromPeriod(period)}
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {selectedMetrics.map((metric, idx) => {
                        const periods = Object.keys(financials.income_statement).slice(0, showYearsCount);
                        return (
                          <tr key={metric} style={{ borderBottom: `1px solid ${COLORS.BORDER}20` }}>
                            <td style={{ padding: '10px', color: COLORS.WHITE, fontWeight: 'bold', fontSize: `${fontSize}px` }}>{metric}</td>
                            {periods.map((period) => {
                              const value = financials.income_statement[period][metric];
                              return (
                                <td key={period} style={{ textAlign: 'right', padding: '10px', color: COLORS.CYAN, fontSize: `${fontSize}px` }}>
                                  {value ? (metric.includes('EPS') ? `$${formatNumber(value)}` : formatLargeNumber(value)) : 'N/A'}
                                </td>
                              );
                            })}
                          </tr>
                        );
                      })}
                    </tbody>
                  </table>
                </div>
              )}
            </div>

            {/* Detailed Financial Statements */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
              {/* Income Statement */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.GREEN, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  INCOME STATEMENT (Latest)
                </div>
                <div style={{ fontSize: `${fontSize}px`, maxHeight: '500px', overflow: 'auto' }} className="custom-scrollbar">
                  {Object.keys(financials.income_statement).length > 0 ? (
                    <>
                      {Object.entries(Object.values(financials.income_statement)[0] || {}).map(([metric, _], idx) => {
                        const latestPeriod = Object.keys(financials.income_statement)[0];
                        const value = financials.income_statement[latestPeriod][metric];
                        return (
                          <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', paddingBottom: '6px', borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                            <span style={{ color: COLORS.GRAY, fontSize: `${fontSize - 1}px`, maxWidth: '60%' }}>{metric}:</span>
                            <span style={{ color: COLORS.CYAN, fontSize: `${fontSize}px` }}>{formatLargeNumber(value)}</span>
                          </div>
                        );
                      })}
                    </>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '20px', fontSize: `${fontSize}px` }}>No data available</div>
                  )}
                </div>
              </div>

              {/* Balance Sheet */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.YELLOW, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  BALANCE SHEET (Latest)
                </div>
                <div style={{ fontSize: `${fontSize}px`, maxHeight: '500px', overflow: 'auto' }} className="custom-scrollbar">
                  {Object.keys(financials.balance_sheet).length > 0 ? (
                    <>
                      {Object.entries(Object.values(financials.balance_sheet)[0] || {}).map(([metric, _], idx) => {
                        const latestPeriod = Object.keys(financials.balance_sheet)[0];
                        const value = financials.balance_sheet[latestPeriod][metric];
                        return (
                          <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', paddingBottom: '6px', borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                            <span style={{ color: COLORS.GRAY, fontSize: `${fontSize - 1}px`, maxWidth: '60%' }}>{metric}:</span>
                            <span style={{ color: COLORS.CYAN, fontSize: `${fontSize}px` }}>{formatLargeNumber(value)}</span>
                          </div>
                        );
                      })}
                    </>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '20px', fontSize: `${fontSize}px` }}>No data available</div>
                  )}
                </div>
              </div>

              {/* Cash Flow */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.CYAN, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  CASH FLOW STATEMENT (Latest)
                </div>
                <div style={{ fontSize: `${fontSize}px`, maxHeight: '500px', overflow: 'auto' }} className="custom-scrollbar">
                  {Object.keys(financials.cash_flow).length > 0 ? (
                    <>
                      {Object.entries(Object.values(financials.cash_flow)[0] || {}).map(([metric, _], idx) => {
                        const latestPeriod = Object.keys(financials.cash_flow)[0];
                        const value = financials.cash_flow[latestPeriod][metric];
                        return (
                          <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', paddingBottom: '6px', borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                            <span style={{ color: COLORS.GRAY, fontSize: `${fontSize - 1}px`, maxWidth: '60%' }}>{metric}:</span>
                            <span style={{ color: COLORS.CYAN, fontSize: `${fontSize}px` }}>{formatLargeNumber(value)}</span>
                          </div>
                        );
                      })}
                    </>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '20px', fontSize: `${fontSize}px` }}>No data available</div>
                  )}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Analysis Tab */}
        {activeTab === 'analysis' && (
          <div style={{ padding: '0 8px 8px 8px' }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              {/* Financial Health */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '12px',
              }}>
                <div style={{ color: COLORS.ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '6px' }}>
                  FINANCIAL HEALTH
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', fontSize: '11px' }}>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>TOTAL CASH</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.total_cash)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>TOTAL DEBT</div>
                    <div style={{ color: COLORS.RED, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.total_debt)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>FREE CASHFLOW</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.free_cashflow)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>OPERATING CASHFLOW</div>
                    <div style={{ color: COLORS.BLUE, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.operating_cashflow)}</div>
                  </div>
                </div>
              </div>

              {/* Enterprise Value */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '12px',
              }}>
                <div style={{ color: COLORS.CYAN, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '6px' }}>
                  ENTERPRISE VALUE METRICS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', fontSize: '11px' }}>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>ENTERPRISE VALUE</div>
                    <div style={{ color: COLORS.YELLOW, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.enterprise_value)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>EV/REVENUE</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '16px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.enterprise_to_revenue)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>EV/EBITDA</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '16px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.enterprise_to_ebitda)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>BOOK VALUE</div>
                    <div style={{ color: COLORS.WHITE, fontSize: '16px', fontWeight: 'bold' }}>${formatNumber(stockInfo?.book_value)}</div>
                  </div>
                </div>
              </div>

              {/* Revenue & Profits */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '12px',
              }}>
                <div style={{ color: COLORS.GREEN, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '6px' }}>
                  REVENUE & PROFITS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', fontSize: '11px' }}>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>TOTAL REVENUE</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.total_revenue)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>REVENUE PER SHARE</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '16px', fontWeight: 'bold' }}>${formatNumber(stockInfo?.revenue_per_share)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>GROSS PROFITS</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.gross_profits)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>EBITDA MARGINS</div>
                    <div style={{ color: COLORS.YELLOW, fontSize: '16px', fontWeight: 'bold' }}>{formatPercent(stockInfo?.ebitda_margins)}</div>
                  </div>
                </div>
              </div>

              {/* Key Ratios Summary */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '12px',
              }}>
                <div style={{ color: COLORS.YELLOW, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '6px' }}>
                  KEY RATIOS SUMMARY
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', fontSize: '10px' }}>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>P/E RATIO</div>
                    <div style={{ color: COLORS.WHITE, fontSize: '14px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.pe_ratio)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>PEG RATIO</div>
                    <div style={{ color: COLORS.WHITE, fontSize: '14px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.peg_ratio)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>ROE</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold' }}>{formatPercent(stockInfo?.return_on_equity)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>ROA</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold' }}>{formatPercent(stockInfo?.return_on_assets)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>BETA</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '14px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.beta)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>SHORT RATIO</div>
                    <div style={{ color: COLORS.RED, fontSize: '14px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.short_ratio)}</div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${COLORS.BORDER}`,
        backgroundColor: COLORS.PANEL_BG,
        padding: '3px 12px',
        fontSize: '9px',
        color: COLORS.GRAY,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
          <span>Fincept Equity Research Terminal | Real-time Data via YFinance | Powered by Rust + React</span>
          <span>{currentSymbol} | {stockInfo?.exchange || 'N/A'} | {currentTime.toLocaleString()}</span>
        </div>
      </div>

      {/* Custom Scrollbar Styles */}
      <style>{`
        .custom-scrollbar::-webkit-scrollbar {
          width: 4px;
          height: 4px;
        }
        .custom-scrollbar::-webkit-scrollbar-track {
          background: transparent;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb {
          background: rgba(120, 120, 120, 0.3);
          border-radius: 2px;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb:hover {
          background: rgba(120, 120, 120, 0.6);
        }
        .custom-scrollbar::-webkit-scrollbar-corner {
          background: transparent;
        }
      `}</style>
    </div>
  );
};

export default EquityResearchTab;
