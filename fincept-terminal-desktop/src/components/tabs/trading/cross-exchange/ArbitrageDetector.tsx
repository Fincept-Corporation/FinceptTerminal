/**
 * ArbitrageDetector - Real-time arbitrage opportunity detection across exchanges
 * Bloomberg Terminal Style
 */

import React from 'react';
import { Zap, RefreshCw, Loader, TrendingUp, Clock, AlertTriangle, ExternalLink } from 'lucide-react';
import { useArbitrageDetection } from '../hooks/useCrossExchange';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

export function ArbitrageDetector() {
  const { opportunities, isScanning, lastScanTime, scan } = useArbitrageDetection();

  const formatPrice = (price: number): string => {
    return price.toLocaleString('en-US', {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    });
  };

  const formatTime = (timestamp: number): string => {
    const seconds = Math.floor((Date.now() - timestamp) / 1000);
    if (seconds < 60) return `${seconds}s ago`;
    const minutes = Math.floor(seconds / 60);
    if (minutes < 60) return `${minutes}m ago`;
    const hours = Math.floor(minutes / 60);
    return `${hours}h ago`;
  };

  return (
    <div style={{
      backgroundColor: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderRadius: '4px',
      padding: '16px',
      height: '100%',
      overflow: 'auto'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '16px',
        paddingBottom: '12px',
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Zap style={{ width: 18, height: 18, color: BLOOMBERG.YELLOW }} />
          <span style={{ fontSize: '13px', fontWeight: 600, color: BLOOMBERG.WHITE, letterSpacing: '0.5px' }}>
            ARBITRAGE OPPORTUNITIES
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          {lastScanTime && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '10px', color: BLOOMBERG.MUTED }}>
              <Clock style={{ width: 12, height: 12 }} />
              {formatTime(lastScanTime)}
            </div>
          )}
          <button
            onClick={scan}
            disabled={isScanning}
            style={{
              padding: '6px 12px',
              fontSize: '10px',
              color: isScanning ? BLOOMBERG.MUTED : BLOOMBERG.YELLOW,
              backgroundColor: 'transparent',
              border: `1px solid ${isScanning ? BLOOMBERG.BORDER : BLOOMBERG.BORDER}`,
              borderRadius: '2px',
              cursor: isScanning ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              opacity: isScanning ? 0.5 : 1,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (!isScanning) {
                e.currentTarget.style.borderColor = BLOOMBERG.YELLOW;
                e.currentTarget.style.backgroundColor = `${BLOOMBERG.YELLOW}15`;
              }
            }}
            onMouseLeave={(e) => {
              if (!isScanning) {
                e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                e.currentTarget.style.backgroundColor = 'transparent';
              }
            }}
          >
            <RefreshCw style={{ width: 12, height: 12, animation: isScanning ? 'spin 1s linear infinite' : 'none' }} />
            {isScanning ? 'SCANNING...' : 'SCAN'}
          </button>
        </div>
      </div>

      {/* Risk Warning */}
      <div style={{
        marginBottom: '16px',
        padding: '10px',
        backgroundColor: `${BLOOMBERG.YELLOW}08`,
        border: `1px solid ${BLOOMBERG.YELLOW}25`,
        borderRadius: '2px',
        display: 'flex',
        alignItems: 'flex-start',
        gap: '8px'
      }}>
        <AlertTriangle style={{ width: 14, height: 14, color: BLOOMBERG.YELLOW, flexShrink: 0, marginTop: '2px' }} />
        <div style={{ fontSize: '10px', color: BLOOMBERG.YELLOW, lineHeight: '1.5' }}>
          <strong>RISK WARNING:</strong> Arbitrage opportunities include trading fees, withdrawal
          fees, and execution risk. Always calculate net profit after all costs. Prices can change
          rapidly.
        </div>
      </div>

      {/* Scanning Status */}
      {isScanning && (
        <div style={{
          marginBottom: '12px',
          padding: '10px',
          backgroundColor: `${BLOOMBERG.CYAN}08`,
          border: `1px solid ${BLOOMBERG.CYAN}25`,
          borderRadius: '2px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <Loader style={{ width: 14, height: 14, color: BLOOMBERG.CYAN, animation: 'spin 1s linear infinite' }} />
          <span style={{ fontSize: '11px', color: BLOOMBERG.CYAN }}>SCANNING FOR ARBITRAGE OPPORTUNITIES...</span>
        </div>
      )}

      {/* Opportunities List */}
      <div style={{ marginBottom: '16px' }}>
        {opportunities.length === 0 ? (
          <div style={{
            textAlign: 'center',
            padding: '40px 0',
            color: BLOOMBERG.MUTED
          }}>
            <Zap style={{ width: 48, height: 48, margin: '0 auto 12px', color: BLOOMBERG.MUTED }} />
            <div style={{ fontSize: '11px', marginBottom: '6px' }}>NO ARBITRAGE OPPORTUNITIES FOUND</div>
            <div style={{ fontSize: '10px' }}>Spread must be &gt; 0.2% to be considered viable</div>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {opportunities.map((opp, idx) => (
              <div
                key={idx}
                style={{
                  padding: '14px',
                  background: `linear-gradient(135deg, ${BLOOMBERG.YELLOW}05, ${BLOOMBERG.ORANGE}05)`,
                  border: `2px solid ${BLOOMBERG.YELLOW}40`,
                  borderRadius: '2px',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = `${BLOOMBERG.YELLOW}80`;
                  e.currentTarget.style.backgroundColor = BLOOMBERG.HEADER_BG;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = `${BLOOMBERG.YELLOW}40`;
                  e.currentTarget.style.background = `linear-gradient(135deg, ${BLOOMBERG.YELLOW}05, ${BLOOMBERG.ORANGE}05)`;
                }}
              >
                {/* Opportunity Header */}
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  marginBottom: '12px'
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: BLOOMBERG.YELLOW }}>
                      <Zap style={{ width: 18, height: 18 }} fill="currentColor" />
                      <span style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.WHITE, fontFamily: 'monospace' }}>
                        {opp.symbol}
                      </span>
                    </div>
                    <div style={{
                      padding: '4px 10px',
                      backgroundColor: `${BLOOMBERG.YELLOW}20`,
                      border: `1px solid ${BLOOMBERG.YELLOW}40`,
                      borderRadius: '2px',
                      fontSize: '10px',
                      color: BLOOMBERG.YELLOW,
                      fontWeight: 700,
                      letterSpacing: '0.5px'
                    }}>
                      {opp.spreadPercent.toFixed(2)}% SPREAD
                    </div>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '2px' }}>POTENTIAL PROFIT</div>
                    <div style={{ fontSize: '18px', fontWeight: 700, color: BLOOMBERG.GREEN, fontFamily: 'monospace' }}>
                      ${opp.potentialProfit.toFixed(2)}
                    </div>
                  </div>
                </div>

                {/* Trade Details */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '10px' }}>
                  {/* Buy Side */}
                  <div style={{
                    padding: '12px',
                    backgroundColor: `${BLOOMBERG.GREEN}10`,
                    border: `1px solid ${BLOOMBERG.GREEN}30`,
                    borderRadius: '2px'
                  }}>
                    <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                      BUY ON
                    </div>
                    <div style={{ fontSize: '11px', fontWeight: 600, color: BLOOMBERG.WHITE, marginBottom: '8px', textTransform: 'uppercase' }}>
                      {opp.buyExchange}
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <TrendingUp style={{ width: 16, height: 16, color: BLOOMBERG.GREEN }} />
                      <span style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.GREEN, fontFamily: 'monospace' }}>
                        ${formatPrice(opp.buyPrice)}
                      </span>
                    </div>
                  </div>

                  {/* Sell Side */}
                  <div style={{
                    padding: '12px',
                    backgroundColor: `${BLOOMBERG.RED}10`,
                    border: `1px solid ${BLOOMBERG.RED}30`,
                    borderRadius: '2px'
                  }}>
                    <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                      SELL ON
                    </div>
                    <div style={{ fontSize: '11px', fontWeight: 600, color: BLOOMBERG.WHITE, marginBottom: '8px', textTransform: 'uppercase' }}>
                      {opp.sellExchange}
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <TrendingUp style={{ width: 16, height: 16, color: BLOOMBERG.RED, transform: 'rotate(180deg)' }} />
                      <span style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.RED, fontFamily: 'monospace' }}>
                        ${formatPrice(opp.sellPrice)}
                      </span>
                    </div>
                  </div>
                </div>

                {/* Spread Info */}
                <div style={{
                  padding: '8px',
                  backgroundColor: `${BLOOMBERG.DARK_BG}80`,
                  borderRadius: '2px',
                  marginBottom: '10px'
                }}>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    fontSize: '10px'
                  }}>
                    <span style={{ color: BLOOMBERG.GRAY }}>PRICE DIFFERENCE:</span>
                    <span style={{ color: BLOOMBERG.WHITE, fontWeight: 600, fontFamily: 'monospace' }}>
                      ${opp.spread.toFixed(2)}
                    </span>
                  </div>
                </div>

                {/* Action Buttons */}
                <div style={{ display: 'flex', gap: '8px' }}>
                  <button style={{
                    flex: 1,
                    padding: '10px 14px',
                    backgroundColor: BLOOMBERG.GREEN,
                    color: BLOOMBERG.WHITE,
                    fontSize: '11px',
                    fontWeight: 700,
                    border: 'none',
                    borderRadius: '2px',
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '6px',
                    transition: 'all 0.2s',
                    letterSpacing: '0.5px'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = '#00FF7F';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = BLOOMBERG.GREEN;
                  }}>
                    <TrendingUp style={{ width: 14, height: 14 }} />
                    EXECUTE ARBITRAGE
                  </button>
                  <button style={{
                    padding: '10px 14px',
                    backgroundColor: BLOOMBERG.HEADER_BG,
                    color: BLOOMBERG.WHITE,
                    fontSize: '11px',
                    border: `1px solid ${BLOOMBERG.BORDER}`,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.borderColor = BLOOMBERG.CYAN;
                    e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                    e.currentTarget.style.backgroundColor = BLOOMBERG.HEADER_BG;
                  }}>
                    <ExternalLink style={{ width: 14, height: 14 }} />
                  </button>
                </div>

                {/* Timestamp */}
                <div style={{
                  marginTop: '8px',
                  fontSize: '9px',
                  color: BLOOMBERG.MUTED,
                  textAlign: 'center',
                  letterSpacing: '0.5px'
                }}>
                  DETECTED {formatTime(opp.timestamp)}
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Info Section */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        <div style={{
          padding: '10px',
          backgroundColor: BLOOMBERG.HEADER_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '2px'
        }}>
          <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.WHITE, marginBottom: '8px', letterSpacing: '0.5px' }}>
            HOW IT WORKS
          </div>
          <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <div>
              1. <strong style={{ color: BLOOMBERG.WHITE }}>BUY</strong> the asset on the exchange with the lower price
            </div>
            <div>
              2. <strong style={{ color: BLOOMBERG.WHITE }}>TRANSFER</strong> the asset to the exchange with the higher price
            </div>
            <div>
              3. <strong style={{ color: BLOOMBERG.WHITE }}>SELL</strong> the asset for profit
            </div>
          </div>
        </div>

        <div style={{
          padding: '10px',
          backgroundColor: `${BLOOMBERG.CYAN}08`,
          border: `1px solid ${BLOOMBERG.CYAN}25`,
          borderRadius: '2px'
        }}>
          <div style={{ fontSize: '10px', color: BLOOMBERG.CYAN, lineHeight: '1.5' }}>
            <strong>COST CONSIDERATIONS:</strong> Factor in trading fees (maker/taker), withdrawal
            fees, network fees, and execution time. The spread must exceed total costs for
            profitable arbitrage.
          </div>
        </div>

        <div style={{
          padding: '10px',
          backgroundColor: `${BLOOMBERG.PURPLE}08`,
          border: `1px solid ${BLOOMBERG.PURPLE}25`,
          borderRadius: '2px'
        }}>
          <div style={{ fontSize: '10px', color: BLOOMBERG.PURPLE, lineHeight: '1.5' }}>
            <strong>AUTO-SCAN:</strong> Opportunities are automatically scanned every 10 seconds.
            Click "SCAN" to manually refresh.
          </div>
        </div>
      </div>
    </div>
  );
}
