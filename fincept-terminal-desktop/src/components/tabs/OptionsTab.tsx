import React, { useState, useEffect } from 'react';

interface OptionContract {
  strike: number;
  callBid: number;
  callAsk: number;
  callLast: number;
  callVolume: number;
  callOI: number;
  callIV: number;
  callDelta: number;
  callGamma: number;
  callTheta: number;
  callVega: number;
  putBid: number;
  putAsk: number;
  putLast: number;
  putVolume: number;
  putOI: number;
  putIV: number;
  putDelta: number;
  putGamma: number;
  putTheta: number;
  putVega: number;
}

const OptionsTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedSymbol, setSelectedSymbol] = useState('AAPL');
  const [selectedExpiry, setSelectedExpiry] = useState('2025-03-21');
  const [showGreeks, setShowGreeks] = useState(true);

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

  const underlyingPrice = 178.45;
  const symbols = ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'TSLA', 'NVDA', 'META', 'SPY', 'QQQ'];
  const expiryDates = ['2025-03-21', '2025-04-18', '2025-05-16', '2025-06-20', '2025-09-19', '2025-12-19'];

  const optionsChain: OptionContract[] = [
    {
      strike: 170,
      callBid: 9.80, callAsk: 10.10, callLast: 9.95, callVolume: 1245, callOI: 8934,
      callIV: 0.284, callDelta: 0.78, callGamma: 0.042, callTheta: -0.089, callVega: 0.156,
      putBid: 1.35, putAsk: 1.42, putLast: 1.38, putVolume: 892, putOI: 5623,
      putIV: 0.298, putDelta: -0.22, putGamma: 0.042, putTheta: -0.067, putVega: 0.156
    },
    {
      strike: 172.50,
      callBid: 7.90, callAsk: 8.15, callLast: 8.05, callVolume: 2134, callOI: 12456,
      callIV: 0.276, callDelta: 0.72, callGamma: 0.048, callTheta: -0.094, callVega: 0.167,
      putBid: 1.75, putAsk: 1.83, putLast: 1.79, putVolume: 1456, putOI: 8234,
      putIV: 0.291, putDelta: -0.28, putGamma: 0.048, putTheta: -0.072, putVega: 0.167
    },
    {
      strike: 175,
      callBid: 6.20, callAsk: 6.45, callLast: 6.35, callVolume: 3456, callOI: 18934,
      callIV: 0.268, callDelta: 0.65, callGamma: 0.054, callTheta: -0.101, callVega: 0.178,
      putBid: 2.30, putAsk: 2.40, putLast: 2.35, putVolume: 2234, putOI: 11234,
      putIV: 0.284, putDelta: -0.35, putGamma: 0.054, putTheta: -0.078, putVega: 0.178
    },
    {
      strike: 177.50,
      callBid: 4.70, callAsk: 4.90, callLast: 4.82, callVolume: 5678, callOI: 23456,
      callIV: 0.261, callDelta: 0.56, callGamma: 0.059, callTheta: -0.108, callVega: 0.189,
      putBid: 3.05, putAsk: 3.18, putLast: 3.12, putVolume: 3456, putOI: 15678,
      putIV: 0.278, putDelta: -0.44, putGamma: 0.059, putTheta: -0.085, putVega: 0.189
    },
    {
      strike: 180,
      callBid: 3.45, callAsk: 3.62, callLast: 3.55, callVolume: 8923, callOI: 34567,
      callIV: 0.255, callDelta: 0.47, callGamma: 0.062, callTheta: -0.114, callVega: 0.198,
      putBid: 4.10, putAsk: 4.28, putLast: 4.20, putVolume: 5678, putOI: 22345,
      putIV: 0.272, putDelta: -0.53, putGamma: 0.062, putTheta: -0.092, putVega: 0.198
    },
    {
      strike: 182.50,
      callBid: 2.45, callAsk: 2.58, callLast: 2.52, callVolume: 6789, callOI: 28934,
      callIV: 0.249, callDelta: 0.38, callGamma: 0.063, callTheta: -0.118, callVega: 0.205,
      putBid: 5.45, putAsk: 5.68, putLast: 5.58, putVolume: 4567, putOI: 19234,
      putIV: 0.267, putDelta: -0.62, putGamma: 0.063, putTheta: -0.098, putVega: 0.205
    },
    {
      strike: 185,
      callBid: 1.68, callAsk: 1.78, callLast: 1.73, callVolume: 4567, callOI: 21234,
      callIV: 0.244, callDelta: 0.29, callGamma: 0.059, callTheta: -0.120, callVega: 0.209,
      putBid: 7.20, putAsk: 7.48, putLast: 7.35, putVolume: 3234, putOI: 16789,
      putIV: 0.263, putDelta: -0.71, putGamma: 0.059, putTheta: -0.103, putVega: 0.209
    },
    {
      strike: 187.50,
      callBid: 1.12, callAsk: 1.20, callLast: 1.16, callVolume: 2345, callOI: 14567,
      callIV: 0.240, callDelta: 0.21, callGamma: 0.052, callTheta: -0.119, callVega: 0.210,
      putBid: 9.35, putAsk: 9.68, putLast: 9.52, putVolume: 1892, putOI: 12456,
      putIV: 0.259, putDelta: -0.79, putGamma: 0.052, putTheta: -0.106, putVega: 0.210
    }
  ];

  const volatilitySurface = [
    { expiry: '1M', atm: 0.255, skew: -0.012 },
    { expiry: '2M', atm: 0.268, skew: -0.015 },
    { expiry: '3M', atm: 0.281, skew: -0.018 },
    { expiry: '6M', atm: 0.302, skew: -0.022 },
    { expiry: '1Y', atm: 0.334, skew: -0.028 }
  ];

  const portfolioGreeks = {
    totalDelta: 245.67,
    totalGamma: 12.34,
    totalTheta: -89.23,
    totalVega: 456.78,
    totalRho: 34.56
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
              OPTIONS CHAIN
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
              <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>SYMBOL:</span>
              <select
                value={selectedSymbol}
                onChange={(e) => setSelectedSymbol(e.target.value)}
                style={{
                  backgroundColor: COLOR_DARK_BG,
                  color: COLOR_CYAN,
                  border: `2px solid ${COLOR_GRAY}`,
                  padding: '4px 8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace',
                  fontWeight: 'bold'
                }}>
                {symbols.map(sym => (
                  <option key={sym} value={sym}>{sym}</option>
                ))}
              </select>
              <span style={{ color: COLOR_GRAY }}>|</span>
              <span style={{ color: COLOR_GREEN, fontSize: '14px', fontWeight: 'bold' }}>
                ${underlyingPrice.toFixed(2)}
              </span>
            </div>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
              <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>EXPIRY:</span>
              <select
                value={selectedExpiry}
                onChange={(e) => setSelectedExpiry(e.target.value)}
                style={{
                  backgroundColor: COLOR_DARK_BG,
                  color: COLOR_YELLOW,
                  border: `2px solid ${COLOR_GRAY}`,
                  padding: '4px 8px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace',
                  fontWeight: 'bold'
                }}>
                {expiryDates.map(date => (
                  <option key={date} value={date}>{date}</option>
                ))}
              </select>
            </div>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <button
              onClick={() => setShowGreeks(!showGreeks)}
              style={{
                padding: '6px 12px',
                backgroundColor: showGreeks ? COLOR_ORANGE : COLOR_DARK_BG,
                border: `2px solid ${COLOR_ORANGE}`,
                color: showGreeks ? COLOR_DARK_BG : COLOR_ORANGE,
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}>
              {showGreeks ? 'HIDE GREEKS' : 'SHOW GREEKS'}
            </button>
            <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>
              {currentTime.toLocaleTimeString()}
            </span>
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, overflow: 'auto', display: 'flex', gap: '12px', padding: '12px' }}>
        {/* Left Panel - Greeks & IV */}
        <div style={{ width: '280px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Portfolio Greeks */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_GREEN}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              PORTFOLIO GREEKS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <div style={{
                padding: '8px',
                backgroundColor: 'rgba(0,200,0,0.05)',
                border: `1px solid ${COLOR_GREEN}`
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '10px', marginBottom: '2px' }}>DELTA</div>
                <div style={{ color: COLOR_GREEN, fontSize: '16px', fontWeight: 'bold' }}>
                  {portfolioGreeks.totalDelta.toFixed(2)}
                </div>
                <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>Directional exposure</div>
              </div>
              <div style={{
                padding: '8px',
                backgroundColor: 'rgba(100,150,250,0.05)',
                border: `1px solid ${COLOR_BLUE}`
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '10px', marginBottom: '2px' }}>GAMMA</div>
                <div style={{ color: COLOR_BLUE, fontSize: '16px', fontWeight: 'bold' }}>
                  {portfolioGreeks.totalGamma.toFixed(2)}
                </div>
                <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>Delta sensitivity</div>
              </div>
              <div style={{
                padding: '8px',
                backgroundColor: 'rgba(255,0,0,0.05)',
                border: `1px solid ${COLOR_RED}`
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '10px', marginBottom: '2px' }}>THETA</div>
                <div style={{ color: COLOR_RED, fontSize: '16px', fontWeight: 'bold' }}>
                  {portfolioGreeks.totalTheta.toFixed(2)}
                </div>
                <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>Time decay (per day)</div>
              </div>
              <div style={{
                padding: '8px',
                backgroundColor: 'rgba(200,100,255,0.05)',
                border: `1px solid ${COLOR_PURPLE}`
              }}>
                <div style={{ color: COLOR_GRAY, fontSize: '10px', marginBottom: '2px' }}>VEGA</div>
                <div style={{ color: COLOR_PURPLE, fontSize: '16px', fontWeight: 'bold' }}>
                  {portfolioGreeks.totalVega.toFixed(2)}
                </div>
                <div style={{ color: COLOR_GRAY, fontSize: '9px' }}>Volatility exposure</div>
              </div>
            </div>
          </div>

          {/* Volatility Surface */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_PURPLE}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              IMPLIED VOLATILITY
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {volatilitySurface.map((vol, index) => (
                <div key={index} style={{
                  padding: '8px',
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                  borderBottom: `1px solid ${COLOR_GRAY}`
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                    <span style={{ color: COLOR_GRAY, fontSize: '11px' }}>{vol.expiry}</span>
                    <span style={{ color: COLOR_PURPLE, fontSize: '12px', fontWeight: 'bold' }}>
                      {(vol.atm * 100).toFixed(1)}%
                    </span>
                  </div>
                  <div style={{ fontSize: '9px', color: vol.skew < 0 ? COLOR_RED : COLOR_GREEN }}>
                    Skew: {(vol.skew * 100).toFixed(2)}%
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Quick Stats */}
          <div style={{
            backgroundColor: COLOR_PANEL_BG,
            border: `2px solid ${COLOR_GRAY}`,
            borderLeft: `6px solid ${COLOR_CYAN}`,
            padding: '12px'
          }}>
            <div style={{ color: COLOR_ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px' }}>
              QUICK STATS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: '11px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLOR_GRAY }}>Total Call Vol:</span>
                <span style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>35,198</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLOR_GRAY }}>Total Put Vol:</span>
                <span style={{ color: COLOR_RED, fontWeight: 'bold' }}>23,409</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLOR_GRAY }}>Put/Call Ratio:</span>
                <span style={{ color: COLOR_YELLOW, fontWeight: 'bold' }}>0.67</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLOR_GRAY }}>Max Pain:</span>
                <span style={{ color: COLOR_CYAN, fontWeight: 'bold' }}>$177.50</span>
              </div>
            </div>
          </div>
        </div>

        {/* Right Panel - Options Chain */}
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
              gridTemplateColumns: showGreeks
                ? '80px 70px 70px 70px 80px 60px 60px 60px 60px 60px 80px 70px 70px 70px 80px 60px 60px 60px 60px 60px'
                : '100px 80px 80px 80px 100px 80px 100px 80px 80px 80px 100px 80px',
              padding: '10px 12px',
              backgroundColor: 'rgba(255,165,0,0.1)',
              borderBottom: `2px solid ${COLOR_ORANGE}`,
              position: 'sticky',
              top: 0,
              zIndex: 10,
              fontSize: '9px'
            }}>
              {showGreeks ? (
                <>
                  {/* Calls */}
                  <div style={{ color: COLOR_GREEN, fontWeight: 'bold', textAlign: 'center' }}>
                    ========== CALLS ==========
                  </div>
                  <div style={{ color: COLOR_ORANGE, fontWeight: 'bold', textAlign: 'center' }}>STRIKE</div>
                  {/* Puts */}
                  <div style={{ color: COLOR_RED, fontWeight: 'bold', textAlign: 'center' }}>
                    ========== PUTS ==========
                  </div>
                </>
              ) : (
                <>
                  <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>C.BID</div>
                  <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>C.ASK</div>
                  <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>C.LAST</div>
                  <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>C.VOL</div>
                  <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>C.OI</div>
                  <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>C.IV</div>
                  <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>STRIKE</div>
                  <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>P.BID</div>
                  <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>P.ASK</div>
                  <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>P.LAST</div>
                  <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>P.VOL</div>
                  <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>P.OI</div>
                  <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>P.IV</div>
                </>
              )}
            </div>

            {showGreeks && (
              <div style={{
                display: 'grid',
                gridTemplateColumns: '80px 70px 70px 70px 80px 60px 60px 60px 60px 60px 80px 70px 70px 70px 80px 60px 60px 60px 60px 60px',
                padding: '8px 12px',
                backgroundColor: COLOR_PANEL_BG,
                borderBottom: `1px solid ${COLOR_GRAY}`,
                position: 'sticky',
                top: '32px',
                zIndex: 9,
                fontSize: '9px'
              }}>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>BID</div>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>ASK</div>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>LAST</div>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>VOL</div>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>OI</div>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>IV</div>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>Δ</div>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>Γ</div>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>Θ</div>
                <div style={{ color: COLOR_GREEN, fontWeight: 'bold' }}>V</div>
                <div style={{ color: COLOR_ORANGE, fontWeight: 'bold' }}>STRIKE</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>BID</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>ASK</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>LAST</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>VOL</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>OI</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>IV</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>Δ</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>Γ</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>Θ</div>
                <div style={{ color: COLOR_RED, fontWeight: 'bold' }}>V</div>
              </div>
            )}

            {/* Table Body */}
            {optionsChain.map((option, index) => {
              const isITM = option.strike < underlyingPrice;
              const isATM = Math.abs(option.strike - underlyingPrice) < 2.5;

              return (
                <div key={index}
                  style={{
                    display: 'grid',
                    gridTemplateColumns: showGreeks
                      ? '80px 70px 70px 70px 80px 60px 60px 60px 60px 60px 80px 70px 70px 70px 80px 60px 60px 60px 60px 60px'
                      : '100px 80px 80px 80px 100px 80px 100px 80px 80px 80px 100px 80px',
                    padding: '8px 12px',
                    backgroundColor: isATM ? 'rgba(255,165,0,0.1)' : (index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent'),
                    borderBottom: `1px solid ${COLOR_GRAY}`,
                    fontSize: '10px'
                  }}>
                  {/* Calls */}
                  <div style={{ color: COLOR_GREEN }}>${option.callBid.toFixed(2)}</div>
                  <div style={{ color: COLOR_GREEN }}>${option.callAsk.toFixed(2)}</div>
                  <div style={{ color: COLOR_WHITE, fontWeight: 'bold' }}>${option.callLast.toFixed(2)}</div>
                  <div style={{ color: COLOR_CYAN }}>{option.callVolume.toLocaleString()}</div>
                  <div style={{ color: COLOR_GRAY }}>{option.callOI.toLocaleString()}</div>
                  <div style={{ color: COLOR_PURPLE }}>{(option.callIV * 100).toFixed(1)}%</div>
                  {showGreeks && (
                    <>
                      <div style={{ color: COLOR_GREEN }}>{option.callDelta.toFixed(2)}</div>
                      <div style={{ color: COLOR_BLUE }}>{option.callGamma.toFixed(3)}</div>
                      <div style={{ color: COLOR_RED }}>{option.callTheta.toFixed(3)}</div>
                      <div style={{ color: COLOR_PURPLE }}>{option.callVega.toFixed(3)}</div>
                    </>
                  )}

                  {/* Strike */}
                  <div style={{
                    color: isATM ? COLOR_ORANGE : COLOR_WHITE,
                    fontWeight: isATM ? 'bold' : 'normal',
                    fontSize: isATM ? '12px' : '10px',
                    textAlign: 'center',
                    backgroundColor: isATM ? 'rgba(255,165,0,0.2)' : 'transparent',
                    padding: '4px'
                  }}>
                    ${option.strike.toFixed(2)}
                  </div>

                  {/* Puts */}
                  <div style={{ color: COLOR_RED }}>${option.putBid.toFixed(2)}</div>
                  <div style={{ color: COLOR_RED }}>${option.putAsk.toFixed(2)}</div>
                  <div style={{ color: COLOR_WHITE, fontWeight: 'bold' }}>${option.putLast.toFixed(2)}</div>
                  <div style={{ color: COLOR_CYAN }}>{option.putVolume.toLocaleString()}</div>
                  <div style={{ color: COLOR_GRAY }}>{option.putOI.toLocaleString()}</div>
                  <div style={{ color: COLOR_PURPLE }}>{(option.putIV * 100).toFixed(1)}%</div>
                  {showGreeks && (
                    <>
                      <div style={{ color: COLOR_RED }}>{option.putDelta.toFixed(2)}</div>
                      <div style={{ color: COLOR_BLUE }}>{option.putGamma.toFixed(3)}</div>
                      <div style={{ color: COLOR_RED }}>{option.putTheta.toFixed(3)}</div>
                      <div style={{ color: COLOR_PURPLE }}>{option.putVega.toFixed(3)}</div>
                    </>
                  )}
                </div>
              );
            })}
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
              OPTIONS ANALYTICS v1.0
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_CYAN }}>
              Real-time options chain with Greeks
            </span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GREEN }}>
              {selectedSymbol} • Exp: {selectedExpiry}
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
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F2</span> Strategy Builder</span>
            <span style={{ color: COLOR_GRAY }}>|</span>
            <span style={{ color: COLOR_GRAY }}><span style={{ color: COLOR_BLUE }}>F3</span> Risk Analysis</span>
          </div>
          <div style={{ color: COLOR_GRAY }}>
            © 2025 Fincept Labs • All Rights Reserved
          </div>
        </div>
      </div>
    </div>
  );
};

export default OptionsTab;