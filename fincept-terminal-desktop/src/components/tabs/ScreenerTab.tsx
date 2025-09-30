import React, { useState, useEffect } from 'react';

interface Stock {
  ticker: string;
  name: string;
  sector: string;
  price: number;
  change: number;
  changePercent: number;
  marketCap: string;
  pe: number;
  ps: number;
  pb: number;
  forwardPE: number;
  peg: number;
  dividend: number;
  roe: number;
  roa: number;
  debtToEquity: number;
  currentRatio: number;
  eps: number;
  revenue: string;
  revenueGrowth: number;
  profitMargin: number;
  beta: number;
  volume: string;
  avgVolume: string;
  week52High: number;
  week52Low: number;
}

const ScreenerTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [filters, setFilters] = useState({
    marketCapMin: '',
    marketCapMax: '',
    peMin: '',
    peMax: '',
    dividendMin: '',
    priceMin: '',
    priceMax: '',
    sector: 'ALL',
    volumeMin: ''
  });

  const COLOR_ORANGE = '#FFA500';
  const COLOR_WHITE = '#FFFFFF';
  const COLOR_RED = '#FF0000';
  const COLOR_GREEN = '#00C800';
  const COLOR_YELLOW = '#FFFF00';
  const COLOR_GRAY = '#787878';
  const COLOR_BLUE = '#6496FA';
  const COLOR_PURPLE = '#C864FF';
  const COLOR_CYAN = '#00FFFF';
  const COLOR_DARK_BG = '#000000';
  const COLOR_PANEL_BG = '#0a0a0a';

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  const stocks: Stock[] = [
    {
      ticker: 'AAPL',
      name: 'Apple Inc',
      sector: 'Technology',
      price: 178.45,
      change: 2.34,
      changePercent: 1.33,
      marketCap: '$2.78T',
      pe: 28.4,
      ps: 7.2,
      pb: 45.6,
      forwardPE: 26.8,
      peg: 2.1,
      dividend: 0.55,
      roe: 147.3,
      roa: 27.8,
      debtToEquity: 1.89,
      currentRatio: 0.98,
      eps: 6.29,
      revenue: '$394.3B',
      revenueGrowth: 2.1,
      profitMargin: 25.3,
      beta: 1.24,
      volume: '$42.5M',
      avgVolume: '$38.2M',
      week52High: 199.62,
      week52Low: 164.08
    },
    {
      ticker: 'MSFT',
      name: 'Microsoft Corp',
      sector: 'Technology',
      price: 412.34,
      change: 5.67,
      changePercent: 1.39,
      marketCap: '$3.07T',
      pe: 35.2,
      ps: 12.4,
      pb: 12.8,
      forwardPE: 31.4,
      peg: 2.4,
      dividend: 0.72,
      roe: 42.7,
      roa: 16.4,
      debtToEquity: 0.45,
      currentRatio: 1.77,
      eps: 11.73,
      revenue: '$227.6B',
      revenueGrowth: 13.2,
      profitMargin: 36.7,
      beta: 0.89,
      volume: '$28.3M',
      avgVolume: '$25.7M',
      week52High: 425.22,
      week52Low: 309.45
    },
    {
      ticker: 'GOOGL',
      name: 'Alphabet Inc',
      sector: 'Technology',
      price: 145.67,
      change: -1.23,
      changePercent: -0.84,
      marketCap: '$1.82T',
      pe: 24.6,
      ps: 5.8,
      pb: 6.4,
      forwardPE: 22.1,
      peg: 1.8,
      dividend: 0.00,
      roe: 27.8,
      roa: 18.9,
      debtToEquity: 0.12,
      currentRatio: 2.56,
      eps: 5.93,
      revenue: '$307.4B',
      revenueGrowth: 8.7,
      profitMargin: 25.9,
      beta: 1.05,
      volume: '$19.8M',
      avgVolume: '$21.4M',
      week52High: 156.23,
      week52Low: 121.46
    },
    {
      ticker: 'NVDA',
      name: 'NVIDIA Corp',
      sector: 'Technology',
      price: 878.34,
      change: 12.45,
      changePercent: 1.44,
      marketCap: '$2.18T',
      pe: 68.7,
      ps: 38.2,
      pb: 55.4,
      forwardPE: 52.3,
      peg: 1.2,
      dividend: 0.04,
      roe: 98.4,
      roa: 56.7,
      debtToEquity: 0.28,
      currentRatio: 3.45,
      eps: 12.79,
      revenue: '$60.9B',
      revenueGrowth: 125.9,
      profitMargin: 53.4,
      beta: 1.67,
      volume: '$52.3M',
      avgVolume: '$48.9M',
      week52High: 974.23,
      week52Low: 389.12
    },
    {
      ticker: 'JPM',
      name: 'JPMorgan Chase',
      sector: 'Financials',
      price: 187.23,
      change: 1.89,
      changePercent: 1.02,
      marketCap: '$535.6B',
      pe: 11.2,
      ps: 3.4,
      pb: 1.8,
      forwardPE: 10.4,
      peg: 1.1,
      dividend: 2.84,
      roe: 16.8,
      roa: 1.3,
      debtToEquity: 1.34,
      currentRatio: 0.78,
      eps: 16.72,
      revenue: '$158.1B',
      revenueGrowth: 22.3,
      profitMargin: 32.1,
      beta: 1.12,
      volume: '$8.9M',
      avgVolume: '$9.2M',
      week52High: 199.45,
      week52Low: 135.23
    },
    {
      ticker: 'JNJ',
      name: 'Johnson & Johnson',
      sector: 'Healthcare',
      price: 156.78,
      change: 0.45,
      changePercent: 0.29,
      marketCap: '$389.2B',
      pe: 15.4,
      ps: 4.2,
      pb: 5.6,
      forwardPE: 14.8,
      peg: 2.8,
      dividend: 2.95,
      roe: 23.4,
      roa: 9.8,
      debtToEquity: 0.52,
      currentRatio: 1.12,
      eps: 10.18,
      revenue: '$85.2B',
      revenueGrowth: -1.2,
      profitMargin: 16.8,
      beta: 0.54,
      volume: '$5.6M',
      avgVolume: '$6.1M',
      week52High: 169.23,
      week52Low: 143.45
    },
    {
      ticker: 'XOM',
      name: 'Exxon Mobil',
      sector: 'Energy',
      price: 112.45,
      change: -0.89,
      changePercent: -0.79,
      marketCap: '$456.7B',
      pe: 13.8,
      ps: 1.2,
      pb: 2.1,
      forwardPE: 12.6,
      peg: 1.5,
      dividend: 3.45,
      roe: 17.2,
      roa: 9.4,
      debtToEquity: 0.28,
      currentRatio: 1.34,
      eps: 8.15,
      revenue: '$344.6B',
      revenueGrowth: -8.9,
      profitMargin: 10.8,
      beta: 0.87,
      volume: '$12.3M',
      avgVolume: '$14.2M',
      week52High: 121.34,
      week52Low: 95.67
    },
    {
      ticker: 'TSLA',
      name: 'Tesla Inc',
      sector: 'Consumer Discretionary',
      price: 187.92,
      change: 3.45,
      changePercent: 1.87,
      marketCap: '$598.3B',
      pe: 58.9,
      ps: 7.4,
      pb: 12.3,
      forwardPE: 48.7,
      peg: 2.9,
      dividend: 0.00,
      roe: 22.4,
      roa: 8.9,
      debtToEquity: 0.17,
      currentRatio: 1.73,
      eps: 3.19,
      revenue: '$96.8B',
      revenueGrowth: 18.8,
      profitMargin: 14.4,
      beta: 2.01,
      volume: '$98.4M',
      avgVolume: '$102.3M',
      week52High: 299.29,
      week52Low: 138.80
    },
    {
      ticker: 'WMT',
      name: 'Walmart Inc',
      sector: 'Consumer Staples',
      price: 178.34,
      change: 1.23,
      changePercent: 0.69,
      marketCap: '$478.9B',
      pe: 28.7,
      ps: 0.8,
      pb: 6.9,
      forwardPE: 26.4,
      peg: 3.2,
      dividend: 1.42,
      roe: 21.8,
      roa: 6.7,
      debtToEquity: 0.56,
      currentRatio: 0.79,
      eps: 6.21,
      revenue: '$648.1B',
      revenueGrowth: 5.4,
      profitMargin: 2.4,
      beta: 0.52,
      volume: '$6.8M',
      avgVolume: '$7.3M',
      week52High: 183.45,
      week52Low: 142.67
    },
    {
      ticker: 'BA',
      name: 'Boeing Co',
      sector: 'Industrials',
      price: 189.23,
      change: -2.34,
      changePercent: -1.22,
      marketCap: '$113.4B',
      pe: -24.5,
      ps: 1.8,
      pb: -8.9,
      forwardPE: 34.2,
      peg: -1.4,
      dividend: 0.00,
      roe: -28.4,
      roa: -3.2,
      debtToEquity: 8.45,
      currentRatio: 1.12,
      eps: -7.72,
      revenue: '$77.8B',
      revenueGrowth: 10.2,
      profitMargin: -8.9,
      beta: 1.78,
      volume: '$9.8M',
      avgVolume: '$11.2M',
      week52High: 267.54,
      week52Low: 173.12
    }
  ];

  const sectors = ['ALL', 'Technology', 'Financials', 'Healthcare', 'Energy', 'Consumer Discretionary', 'Consumer Staples', 'Industrials'];

  const presetScreens = [
    { name: 'Dividend Aristocrats', desc: 'Dividend > 2%, PE < 20' },
    { name: 'Growth Stocks', desc: 'Revenue Growth > 20%, PE < 40' },
    { name: 'Value Stocks', desc: 'PE < 15, P/B < 3' },
    { name: 'Large Cap Tech', desc: 'Market Cap > $100B, Sector: Tech' },
    { name: 'High Momentum', desc: 'Beta > 1.5, 52W High < 20%' }
  ];

  const parseMarketCap = (mcap: string): number => {
    const value = parseFloat(mcap.replace(/[^0-9.]/g, ''));
    if (mcap.includes('T')) return value * 1000000;
    if (mcap.includes('B')) return value * 1000;
    if (mcap.includes('M')) return value;
    return value;
  };

  const filteredStocks = stocks.filter(stock => {
    const mcap = parseMarketCap(stock.marketCap);

    if (filters.marketCapMin && mcap < parseFloat(filters.marketCapMin)) return false;
    if (filters.marketCapMax && mcap > parseFloat(filters.marketCapMax)) return false;
    if (filters.peMin && stock.pe < parseFloat(filters.peMin)) return false;
    if (filters.peMax && stock.pe > parseFloat(filters.peMax)) return false;
    if (filters.dividendMin && stock.dividend < parseFloat(filters.dividendMin)) return false;
    if (filters.priceMin && stock.price < parseFloat(filters.priceMin)) return false;
    if (filters.priceMax && stock.price > parseFloat(filters.priceMax)) return false;
    if (filters.sector !== 'ALL' && stock.sector !== filters.sector) return false;

    return true;
  });

  const updateFilter = (key: string, value: string) => {
    setFilters({ ...filters, [key]: value });
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: COLOR_DARK_BG,
      color: COLOR_WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px'
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: COLOR_PANEL_BG,
        borderBottom: `2px solid ${COLOR_GRAY}`,
        padding: '8px 16px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '16px' }}>
              STOCK SCREENER
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_CYAN }}>ADVANCED FILTERING • TECHNICAL • FUNDAMENTAL</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>
              {filteredStocks.length} matches
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>
              {currentTime.toLocaleTimeString()}
            </span>
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, overflow: 'auto', display: 'flex', gap: '12px', padding: '12px' }}>
        {/* Left Panel - Filters */}
        <div style={{ width: '320px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Preset Screens */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_PURPLE}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              PRESET SCREENS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {presetScreens.map((preset, index) => (
                <div key={index}
                  style={{
                    padding: '10px',
                    backgroundColor: 'rgba(200,100,255,0.05)',
                    border: `1px solid ${COLOR_PURPLE}`,
                    cursor: 'pointer'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = 'rgba(200,100,255,0.15)';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'rgba(200,100,255,0.05)';
                  }}>
                  <div style={{ color: COLOR_PURPLE, fontWeight: 'bold', fontSize: '11px', marginBottom: '4px' }}>
                    {preset.name}
                  </div>
                  <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>
                    {preset.desc}
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Price Filters */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_GREEN}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              PRICE FILTERS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              <div>
                <label style={{ color: COLOR_GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  Min Price ($)
                </label>
                <input
                  type="number"
                  value={filters.priceMin}
                  onChange={(e) => updateFilter('priceMin', e.target.value)}
                  style={{
                    width: '100%',
                    padding: '6px',
                    backgroundColor: COLOR_DARK_BG,
                    border: `2px solid ${COLOR_GRAY}`,
                    color: COLOR_CYAN,
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
              <div>
                <label style={{ color: COLOR_GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  Max Price ($)
                </label>
                <input
                  type="number"
                  value={filters.priceMax}
                  onChange={(e) => updateFilter('priceMax', e.target.value)}
                  style={{
                    width: '100%',
                    padding: '6px',
                    backgroundColor: COLOR_DARK_BG,
                    border: `2px solid ${COLOR_GRAY}`,
                    color: COLOR_CYAN,
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
            </div>
          </div>

          {/* Valuation Filters */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_YELLOW}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              VALUATION FILTERS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              <div>
                <label style={{ color: COLOR_GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  Min P/E Ratio
                </label>
                <input
                  type="number"
                  value={filters.peMin}
                  onChange={(e) => updateFilter('peMin', e.target.value)}
                  style={{
                    width: '100%',
                    padding: '6px',
                    backgroundColor: COLOR_DARK_BG,
                    border: `2px solid ${COLOR_GRAY}`,
                    color: COLOR_CYAN,
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
              <div>
                <label style={{ color: COLOR_GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  Max P/E Ratio
                </label>
                <input
                  type="number"
                  value={filters.peMax}
                  onChange={(e) => updateFilter('peMax', e.target.value)}
                  style={{
                    width: '100%',
                    padding: '6px',
                    backgroundColor: COLOR_DARK_BG,
                    border: `2px solid ${COLOR_GRAY}`,
                    color: COLOR_CYAN,
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
              <div>
                <label style={{ color: COLOR_GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  Min Dividend Yield (%)
                </label>
                <input
                  type="number"
                  value={filters.dividendMin}
                  onChange={(e) => updateFilter('dividendMin', e.target.value)}
                  style={{
                    width: '100%',
                    padding: '6px',
                    backgroundColor: COLOR_DARK_BG,
                    border: `2px solid ${COLOR_GRAY}`,
                    color: COLOR_CYAN,
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
            </div>
          </div>

          {/* Market Cap Filter */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_CYAN}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              MARKET CAP ($M)
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              <div>
                <label style={{ color: COLOR_GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  Min Market Cap
                </label>
                <input
                  type="number"
                  value={filters.marketCapMin}
                  onChange={(e) => updateFilter('marketCapMin', e.target.value)}
                  style={{
                    width: '100%',
                    padding: '6px',
                    backgroundColor: COLOR_DARK_BG,
                    border: `2px solid ${COLOR_GRAY}`,
                    color: COLOR_CYAN,
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
              <div>
                <label style={{ color: COLOR_GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  Max Market Cap
                </label>
                <input
                  type="number"
                  value={filters.marketCapMax}
                  onChange={(e) => updateFilter('marketCapMax', e.target.value)}
                  style={{
                    width: '100%',
                    padding: '6px',
                    backgroundColor: COLOR_DARK_BG,
                    border: `2px solid ${COLOR_GRAY}`,
                    color: COLOR_CYAN,
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
            </div>
          </div>

          {/* Sector Filter */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_BLUE}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              SECTOR
            </div>
            <select
              value={filters.sector}
              onChange={(e) => updateFilter('sector', e.target.value)}
              style={{
                width: '100%',
                padding: '8px',
                backgroundColor: COLOR_DARK_BG,
                border: `2px solid ${COLOR_GRAY}`,
                color: COLOR_CYAN,
                fontSize: '11px',
                fontFamily: 'Consolas, monospace',
                fontWeight: 'bold'
              }}>
              {sectors.map(sector => (
                <option key={sector} value={sector}>{sector}</option>
              ))}
            </select>
          </div>

          {/* Clear Filters Button */}
          <button
            onClick={() => setFilters({
              marketCapMin: '',
              marketCapMax: '',
              peMin: '',
              peMax: '',
              dividendMin: '',
              priceMin: '',
              priceMax: '',
              sector: 'ALL',
              volumeMin: ''
            })}
            style={{
              padding: '12px',
              backgroundColor: COLOR_RED,
              border: 'none',
              color: COLOR_DARK_BG,
              fontSize: '12px',
              fontWeight: 'bold',
              cursor: 'pointer',
              fontFamily: 'Consolas, monospace'
            }}>
            CLEAR ALL FILTERS
          </button>
        </div>

        {/* Right Panel - Results */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            flex: 1,
            overflow: 'auto'
          }}>
            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '80px 180px 120px 90px 90px 100px 70px 70px 80px 80px 80px 80px',
              padding: '10px 12px',
              backgroundColor: 'rgba(255,165,0,0.1)',
              borderBottom: `2px solid ${COLOR_ORANGE}`,
              position: 'sticky',
              top: 0,
              zIndex: 10,
              fontSize: '10px'
            }}>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>TICKER</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>NAME</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>SECTOR</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>PRICE</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>CHANGE</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>MKT CAP</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>P/E</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>P/B</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>DIV%</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>ROE%</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>REV GR%</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>BETA</div>
            </div>

            {/* Table Body */}
            {filteredStocks.map((stock, index) => (
              <div key={index}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '80px 180px 120px 90px 90px 100px 70px 70px 80px 80px 80px 80px',
                  padding: '10px 12px',
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                  borderBottom: `1px solid ${COLOR_GRAY}`,
                  cursor: 'pointer',
                  fontSize: '11px'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = 'rgba(255,165,0,0.1)';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent';
                }}>
                <div style={{ color: COLOR_CYAN, fontWeight: 'bold' }}>{stock.ticker}</div>
                <div style={{ color: COLOR_WHITE }}>{stock.name}</div>
                <div style={{ color: COLOR_BLUE }}>{stock.sector}</div>
                <div style={{ color: COLOR_WHITE, fontWeight: 'bold' }}>${stock.price.toFixed(2)}</div>
                <div style={{ color: stock.change > 0 ? COLOR_GREEN : COLOR_RED }}>
                  {stock.change > 0 ? '+' : ''}{stock.change.toFixed(2)} ({stock.changePercent.toFixed(2)}%)
                </div>
                <div style={{ color: COLOR_PURPLE }}>{stock.marketCap}</div>
                <div style={{ color: stock.pe > 0 ? COLOR_YELLOW : COLOR_RED }}>
                  {stock.pe > 0 ? stock.pe.toFixed(1) : 'N/A'}
                </div>
                <div style={{ color: COLOR_CYAN }}>{stock.pb.toFixed(1)}</div>
                <div style={{ color: stock.dividend > 0 ? COLOR_GREEN : COLOR_GRAY }}>
                  {stock.dividend > 0 ? stock.dividend.toFixed(2) : '-'}
                </div>
                <div style={{ color: stock.roe > 0 ? COLOR_GREEN : COLOR_RED }}>
                  {stock.roe.toFixed(1)}
                </div>
                <div style={{ color: stock.revenueGrowth > 0 ? COLOR_GREEN : COLOR_RED }}>
                  {stock.revenueGrowth > 0 ? '+' : ''}{stock.revenueGrowth.toFixed(1)}
                </div>
                <div style={{ color: COLOR_ORANGE }}>{stock.beta.toFixed(2)}</div>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Footer */}
      <div style={{
        borderTop: `3px solid ${COLOR_ORANGE}`,
        backgroundColor: COLOR_PANEL_BG,
        padding: '12px 16px',
        fontSize: '11px',
        color: COLOR_WHITE,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            <span style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '13px' }}>
              STOCK SCREENER v1.0
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_CYAN }}>
              Advanced filtering & analysis
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GREEN }}>
              {filteredStocks.length} stocks found
            </span>
          </div>
        </div>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          fontSize: '10px',
          paddingTop: '8px',
          borderTop: `1px solid ${COLOR_GRAY}`
        }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F1</span> Help</span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F2</span> Save Screen</span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F3</span> Export Results</span>
          </div>
          <div style={{ color: COLOR_GRAY }}>
            © 2025 Fincept Labs • All Rights Reserved
          </div>
        </div>
      </div>
    </div>
  );
};

export default ScreenerTab;