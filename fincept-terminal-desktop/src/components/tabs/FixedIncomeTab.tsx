import React, { useState, useEffect } from 'react';

interface Bond {
  ticker: string;
  issuer: string;
  coupon: number;
  maturity: string;
  yield: number;
  price: number;
  duration: number;
  rating: string;
  type: 'TREASURY' | 'CORPORATE' | 'MUNICIPAL' | 'AGENCY';
  sector: string;
  callDate?: string;
  ytm: number;
  yieldChange: number;
  volume: string;
  spread: number;
}

const FixedIncomeTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedType, setSelectedType] = useState('ALL');
  const [selectedRating, setSelectedRating] = useState('ALL');

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

  const bonds: Bond[] = [
    {
      ticker: 'US10Y',
      issuer: 'US Treasury',
      coupon: 4.125,
      maturity: '2034-02-15',
      yield: 4.234,
      price: 98.75,
      duration: 8.4,
      rating: 'AAA',
      type: 'TREASURY',
      sector: 'Government',
      ytm: 4.234,
      yieldChange: 0.023,
      volume: '$12.5B',
      spread: 0
    },
    {
      ticker: 'US2Y',
      issuer: 'US Treasury',
      coupon: 4.625,
      maturity: '2026-11-30',
      yield: 4.156,
      price: 99.82,
      duration: 1.9,
      rating: 'AAA',
      type: 'TREASURY',
      sector: 'Government',
      ytm: 4.156,
      yieldChange: 0.012,
      volume: '$8.2B',
      spread: 0
    },
    {
      ticker: 'AAPL 3.2',
      issuer: 'Apple Inc',
      coupon: 3.200,
      maturity: '2029-05-11',
      yield: 4.456,
      price: 94.23,
      duration: 4.8,
      rating: 'AA+',
      type: 'CORPORATE',
      sector: 'Technology',
      callDate: '2027-05-11',
      ytm: 4.456,
      yieldChange: 0.034,
      volume: '$450M',
      spread: 98
    },
    {
      ticker: 'JPM 3.875',
      issuer: 'JPMorgan Chase',
      coupon: 3.875,
      maturity: '2032-09-10',
      yield: 4.789,
      price: 91.45,
      duration: 7.2,
      rating: 'A+',
      type: 'CORPORATE',
      sector: 'Financials',
      ytm: 4.789,
      yieldChange: 0.045,
      volume: '$380M',
      spread: 112
    },
    {
      ticker: 'MSFT 2.675',
      issuer: 'Microsoft Corp',
      coupon: 2.675,
      maturity: '2030-06-01',
      yield: 4.234,
      price: 89.67,
      duration: 5.6,
      rating: 'AAA',
      type: 'CORPORATE',
      sector: 'Technology',
      callDate: '2028-06-01',
      ytm: 4.234,
      yieldChange: 0.028,
      volume: '$520M',
      spread: 87
    },
    {
      ticker: 'XOM 4.114',
      issuer: 'Exxon Mobil',
      coupon: 4.114,
      maturity: '2046-03-01',
      yield: 5.123,
      price: 87.34,
      duration: 14.3,
      rating: 'AA',
      type: 'CORPORATE',
      sector: 'Energy',
      ytm: 5.123,
      yieldChange: 0.056,
      volume: '$290M',
      spread: 145
    },
    {
      ticker: 'TSLA 5.3',
      issuer: 'Tesla Inc',
      coupon: 5.300,
      maturity: '2028-08-15',
      yield: 6.234,
      price: 95.12,
      duration: 3.8,
      rating: 'BB+',
      type: 'CORPORATE',
      sector: 'Consumer Discretionary',
      ytm: 6.234,
      yieldChange: 0.089,
      volume: '$180M',
      spread: 234
    },
    {
      ticker: 'BAC 4.45',
      issuer: 'Bank of America',
      coupon: 4.450,
      maturity: '2031-03-03',
      yield: 4.923,
      price: 93.78,
      duration: 6.4,
      rating: 'A',
      type: 'CORPORATE',
      sector: 'Financials',
      ytm: 4.923,
      yieldChange: 0.041,
      volume: '$340M',
      spread: 128
    }
  ];

  const yieldCurveData = [
    { maturity: '1M', yield: 5.234, label: '1 Month' },
    { maturity: '3M', yield: 5.123, label: '3 Month' },
    { maturity: '6M', yield: 4.987, label: '6 Month' },
    { maturity: '1Y', yield: 4.756, label: '1 Year' },
    { maturity: '2Y', yield: 4.156, label: '2 Year' },
    { maturity: '5Y', yield: 3.987, label: '5 Year' },
    { maturity: '10Y', yield: 4.234, label: '10 Year' },
    { maturity: '30Y', yield: 4.523, label: '30 Year' }
  ];

  const creditSpreads = [
    { rating: 'AAA', spread: 45, change: 2 },
    { rating: 'AA', spread: 78, change: 3 },
    { rating: 'A', spread: 112, change: 5 },
    { rating: 'BBB', spread: 165, change: 8 },
    { rating: 'BB', spread: 234, change: 12 },
    { rating: 'B', spread: 389, change: 18 }
  ];

  const filteredBonds = bonds.filter(b =>
    (selectedType === 'ALL' || b.type === selectedType) &&
    (selectedRating === 'ALL' || b.rating.startsWith(selectedRating))
  );

  const getTypeColor = (type: string) => {
    switch(type) {
      case 'TREASURY': return COLOR_BLUE;
      case 'CORPORATE': return COLOR_GREEN;
      case 'MUNICIPAL': return COLOR_PURPLE;
      case 'AGENCY': return COLOR_CYAN;
      default: return COLOR_WHITE;
    }
  };

  const getRatingColor = (rating: string) => {
    if (rating.startsWith('AA')) return COLOR_GREEN;
    if (rating.startsWith('A')) return COLOR_CYAN;
    if (rating.startsWith('BB')) return COLOR_YELLOW;
    if (rating.startsWith('B')) return COLOR_ORANGE;
    return COLOR_RED;
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
              FIXED INCOME MARKETS
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_CYAN }}>US TREASURIES • CORPORATES • CREDIT</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>
              LAST UPDATE: {currentTime.toLocaleTimeString()}
            </span>
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, overflow: 'auto', display: 'flex', gap: '12px', padding: '12px' }}>
        {/* Left Panel - Yield Curve & Spreads */}
        <div style={{ width: '320px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Yield Curve */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_BLUE}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              US TREASURY YIELD CURVE
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {yieldCurveData.map((point, index) => (
                <div key={index} style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  padding: '6px 8px',
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent'
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <span style={{ color: COLOR_GRAY, fontSize: '11px', width: '60px' }}>
                      {point.label}
                    </span>
                    <div style={{
                      width: `${point.yield * 15}px`,
                      height: '16px',
                      backgroundColor: COLOR_BLUE,
                      opacity: 0.7
                    }} />
                  </div>
                  <span style={{ color: COLOR_CYAN, fontSize: '12px', fontWeight: 'bold' }}>
                    {point.yield.toFixed(3)}%
                  </span>
                </div>
              ))}
            </div>
          </div>

          {/* Credit Spreads */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_YELLOW}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              CREDIT SPREADS (OAS)
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {creditSpreads.map((spread, index) => (
                <div key={index} style={{
                  padding: '8px',
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  border: `1px solid ${COLOR_GRAY}`
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                    <span style={{ color: getRatingColor(spread.rating), fontWeight: 'bold', fontSize: '11px' }}>
                      {spread.rating}
                    </span>
                    <span style={{ color: COLOR_CYAN, fontWeight: 'bold', fontSize: '11px' }}>
                      {spread.spread} bps
                    </span>
                  </div>
                  <div style={{ fontSize: '10px', color: spread.change > 0 ? COLOR_RED : COLOR_GREEN }}>
                    {spread.change > 0 ? '▲' : '▼'} {Math.abs(spread.change)} bps
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Key Metrics */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_GREEN}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              KEY METRICS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '11px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLOR_GRAY }}>10Y-2Y Spread:</span>
                <span style={{ color: COLOR_CYAN, fontWeight: 'bold' }}>78 bps</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLOR_GRAY }}>Fed Funds Rate:</span>
                <span style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>5.50%</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLOR_GRAY }}>Inflation (CPI):</span>
                <span style={{ color: COLOR_YELLOW, fontWeight: 'bold' }}>3.2%</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLOR_GRAY }}>Real Yield (10Y):</span>
                <span style={{ color: COLOR_PURPLE, fontWeight: 'bold' }}>1.03%</span>
              </div>
            </div>
          </div>
        </div>

        {/* Right Panel - Bond List */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
          {/* Filters */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            padding: '12px',
            marginBottom: '12px'
          }}>
            <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
              <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>TYPE:</span>
              {['ALL', 'TREASURY', 'CORPORATE', 'MUNICIPAL', 'AGENCY'].map((type) => (
                <button key={type}
                  onClick={() => setSelectedType(type)}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: selectedType === type ? COLOR_ORANGE : COLOR_DARK_BG,
                    border: `2px solid ${selectedType === type ? COLOR_ORANGE : COLOR_GRAY}`,
                    color: selectedType === type ? COLOR_DARK_BG : COLOR_WHITE,
                    fontSize: '10px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace'
                  }}>
                  {type}
                </button>
              ))}
              <span style={{ color: COLOR_GRAY, marginLeft: '16px' }}>|</span>
              <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>RATING:</span>
              {['ALL', 'AAA', 'AA', 'A', 'BBB', 'BB'].map((rating) => (
                <button key={rating}
                  onClick={() => setSelectedRating(rating)}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: selectedRating === rating ? COLOR_CYAN : COLOR_DARK_BG,
                    border: `2px solid ${selectedRating === rating ? COLOR_CYAN : COLOR_GRAY}`,
                    color: selectedRating === rating ? COLOR_DARK_BG : COLOR_WHITE,
                    fontSize: '10px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace'
                  }}>
                  {rating}
                </button>
              ))}
            </div>
          </div>

          {/* Bond Table */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            flex: 1,
            overflow: 'auto'
          }}>
            {/* Table Header */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '100px 180px 80px 100px 80px 80px 70px 60px 80px 80px',
              padding: '10px 12px',
              backgroundColor: 'rgba(255,165,0,0.1)',
              borderBottom: `2px solid ${COLOR_ORANGE}`,
              position: 'sticky',
              top: 0,
              zIndex: 10
            }}>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>TICKER</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>ISSUER</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>COUPON</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>MATURITY</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>YIELD</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>PRICE</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>RATING</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>DUR</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>SPREAD</div>
              <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', fontSize: '10px' }}>VOLUME</div>
            </div>

            {/* Table Body */}
            {filteredBonds.map((bond, index) => (
              <div key={index}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '100px 180px 80px 100px 80px 80px 70px 60px 80px 80px',
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
                <div style={{ color: getTypeColor(bond.type), fontWeight: 'bold' }}>{bond.ticker}</div>
                <div style={{ color: COLOR_WHITE }}>{bond.issuer}</div>
                <div style={{ color: COLOR_CYAN }}>{bond.coupon.toFixed(3)}%</div>
                <div style={{ color: COLOR_GRAY }}>{bond.maturity}</div>
                <div>
                  <div style={{ color: COLOR_YELLOW, fontWeight: 'bold' }}>{bond.yield.toFixed(3)}%</div>
                  <div style={{ color: bond.yieldChange > 0 ? COLOR_RED : COLOR_GREEN, fontSize: '9px' }}>
                    {bond.yieldChange > 0 ? '▲' : '▼'} {Math.abs(bond.yieldChange).toFixed(3)}%
                  </div>
                </div>
                <div style={{ color: COLOR_WHITE }}>{bond.price.toFixed(2)}</div>
                <div style={{ color: getRatingColor(bond.rating), fontWeight: 'bold' }}>{bond.rating}</div>
                <div style={{ color: COLOR_PURPLE }}>{bond.duration.toFixed(1)}</div>
                <div style={{ color: bond.spread > 150 ? COLOR_RED : COLOR_GREEN }}>
                  {bond.spread > 0 ? `+${bond.spread}` : '-'}
                </div>
                <div style={{ color: COLOR_GRAY }}>{bond.volume}</div>
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
              FIXED INCOME v1.0
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_CYAN }}>
              Real-time bond prices & yields
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GREEN }}>
              Showing: {filteredBonds.length} bonds
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
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F2</span> Export</span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F3</span> Calculator</span>
          </div>
          <div style={{ color: COLOR_GRAY }}>
            © 2025 Fincept Labs • All Rights Reserved
          </div>
        </div>
      </div>
    </div>
  );
};

export default FixedIncomeTab;