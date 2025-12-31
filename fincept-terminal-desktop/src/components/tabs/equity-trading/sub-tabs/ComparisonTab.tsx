/**
 * Comparison Tab - Bloomberg Style
 * Side-by-side broker comparison for quotes, depth, and execution quality
 */

import React, { useState, useEffect } from 'react';
import { BrokerType, UnifiedQuote, UnifiedMarketDepth } from '../core/types';
import { dataAggregator } from '../services/DataAggregator';
import { brokerOrchestrator } from '../core/BrokerOrchestrator';
import { GitCompare, Search, Zap } from 'lucide-react';

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
  INPUT_BG: '#0A0A0A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface ComparisonTabProps {
  activeBrokers: BrokerType[];
}

type ComparisonMode = 'quotes' | 'depth' | 'latency' | 'spread';

const ComparisonTab: React.FC<ComparisonTabProps> = ({ activeBrokers }) => {
  const [mode, setMode] = useState<ComparisonMode>('quotes');
  const [symbol, setSymbol] = useState('');
  const [exchange, setExchange] = useState('NSE');
  const [comparing, setComparing] = useState(false);

  const [quoteData, setQuoteData] = useState<Map<BrokerType, UnifiedQuote>>(new Map());
  const [depthData, setDepthData] = useState<Map<BrokerType, UnifiedMarketDepth>>(new Map());
  const [latencyData, setLatencyData] = useState<Map<BrokerType, number>>(new Map());

  const handleCompare = async () => {
    if (!symbol) return;

    setComparing(true);
    try {
      if (mode === 'quotes') {
        const comparison = await brokerOrchestrator.compareQuotes(symbol, exchange);
        setQuoteData(comparison.data as Map<BrokerType, UnifiedQuote>);
        setLatencyData(comparison.latency);
      } else if (mode === 'depth') {
        const comparison = await brokerOrchestrator.compareMarketDepth(symbol, exchange);
        setDepthData(comparison.data as Map<BrokerType, UnifiedMarketDepth>);
        setLatencyData(comparison.latency);
      }
    } catch (error) {
      console.error('[ComparisonTab] Comparison failed:', error);
    } finally {
      setComparing(false);
    }
  };

  const spreadAnalysis = dataAggregator.getSpreadAnalysis(symbol, exchange);

  const inputStyle: React.CSSProperties = {
    padding: '8px 12px',
    backgroundColor: BLOOMBERG.INPUT_BG,
    border: `1px solid ${BLOOMBERG.BORDER}`,
    borderRadius: '0',
    color: BLOOMBERG.WHITE,
    fontSize: '11px',
    fontWeight: 600,
    fontFamily: 'monospace',
    outline: 'none',
    transition: 'border-color 0.15s ease'
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      fontFamily: 'monospace',
      backgroundColor: BLOOMBERG.DARK_BG
    }}>
      {/* Header */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <GitCompare size={16} color={BLOOMBERG.ORANGE} />
            <span style={{
              fontSize: '11px',
              fontWeight: 700,
              color: BLOOMBERG.WHITE,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>
              MULTI-BROKER COMPARISON
            </span>
          </div>
          <div style={{
            fontSize: '9px',
            color: BLOOMBERG.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            ACTIVE BROKERS: {activeBrokers.map(b => b.toUpperCase()).join(', ')}
          </div>
        </div>

        {/* Search Bar */}
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <div style={{ position: 'relative', flex: 1 }}>
            <Search size={14} color={BLOOMBERG.GRAY} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)' }} />
            <input
              type="text"
              placeholder="ENTER SYMBOL (E.G., RELIANCE)"
              value={symbol}
              onChange={(e) => setSymbol(e.target.value.toUpperCase())}
              style={{
                ...inputStyle,
                width: '100%',
                paddingLeft: '32px'
              }}
              onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
              onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
            />
          </div>
          <select
            value={exchange}
            onChange={(e) => setExchange(e.target.value)}
            style={{
              ...inputStyle,
              width: '120px'
            }}
            onFocus={(e) => e.currentTarget.style.borderColor = BLOOMBERG.ORANGE}
            onBlur={(e) => e.currentTarget.style.borderColor = BLOOMBERG.BORDER}
          >
            <option value="NSE">NSE</option>
            <option value="BSE">BSE</option>
            <option value="NFO">NFO</option>
            <option value="MCX">MCX</option>
          </select>
          <button
            onClick={handleCompare}
            disabled={comparing || !symbol}
            style={{
              padding: '8px 20px',
              backgroundColor: comparing || !symbol ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE,
              border: `1px solid ${comparing || !symbol ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE}`,
              borderRadius: '0',
              color: BLOOMBERG.DARK_BG,
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: comparing || !symbol ? 'not-allowed' : 'pointer',
              transition: 'all 0.15s ease',
              fontFamily: 'monospace',
              outline: 'none',
              textTransform: 'uppercase'
            }}
            onMouseEnter={(e) => {
              if (!comparing && symbol) e.currentTarget.style.backgroundColor = BLOOMBERG.YELLOW;
            }}
            onMouseLeave={(e) => {
              if (!comparing && symbol) e.currentTarget.style.backgroundColor = BLOOMBERG.ORANGE;
            }}
          >
            {comparing ? 'COMPARING...' : 'COMPARE'}
          </button>
        </div>
      </div>

      {/* Mode Selector */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: BLOOMBERG.PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
        display: 'flex',
        gap: '8px'
      }}>
        {([
          { id: 'quotes', label: 'QUOTES' },
          { id: 'depth', label: 'MARKET DEPTH' },
          { id: 'latency', label: 'LATENCY' },
          { id: 'spread', label: 'SPREAD ANALYSIS' }
        ] as const).map(({ id, label }) => (
          <button
            key={id}
            onClick={() => setMode(id)}
            style={{
              padding: '6px 16px',
              backgroundColor: mode === id ? BLOOMBERG.ORANGE : 'transparent',
              border: `1px solid ${mode === id ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
              borderRadius: '0',
              color: mode === id ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              fontFamily: 'monospace',
              outline: 'none'
            }}
            onMouseEnter={(e) => {
              if (mode !== id) {
                e.currentTarget.style.borderColor = BLOOMBERG.ORANGE;
                e.currentTarget.style.color = BLOOMBERG.ORANGE;
              }
            }}
            onMouseLeave={(e) => {
              if (mode !== id) {
                e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                e.currentTarget.style.color = BLOOMBERG.GRAY;
              }
            }}
          >
            {label}
          </button>
        ))}
      </div>

      {/* Comparison Results */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        {mode === 'quotes' && <QuotesComparison quotes={quoteData} latency={latencyData} />}
        {mode === 'depth' && <DepthComparison depth={depthData} latency={latencyData} />}
        {mode === 'latency' && <LatencyComparison latency={latencyData} />}
        {mode === 'spread' && spreadAnalysis && <SpreadAnalysis analysis={spreadAnalysis} />}
      </div>
    </div>
  );
};

const QuotesComparison: React.FC<{
  quotes: Map<BrokerType, UnifiedQuote>;
  latency: Map<BrokerType, number>;
}> = ({ quotes, latency }) => {
  if (quotes.size === 0) {
    return (
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        height: '100%',
        color: BLOOMBERG.GRAY,
        fontSize: '11px',
        fontWeight: 600,
        letterSpacing: '0.5px'
      }}>
        ENTER A SYMBOL AND CLICK COMPARE TO VIEW BROKER QUOTES
      </div>
    );
  }

  const brokers = Array.from(quotes.keys());
  const bestBid = Math.max(...Array.from(quotes.values()).map(q => q.bid || 0));
  const bestAsk = Math.min(...Array.from(quotes.values()).filter(q => q.ask).map(q => q.ask!));

  return (
    <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead style={{ position: 'sticky', top: 0, backgroundColor: BLOOMBERG.HEADER_BG, zIndex: 1 }}>
          <tr style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
            <th style={{
              padding: '8px 12px',
              textAlign: 'left',
              fontSize: '9px',
              fontWeight: 700,
              color: BLOOMBERG.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>BROKER</th>
            <th style={{
              padding: '8px 12px',
              textAlign: 'right',
              fontSize: '9px',
              fontWeight: 700,
              color: BLOOMBERG.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>BID</th>
            <th style={{
              padding: '8px 12px',
              textAlign: 'right',
              fontSize: '9px',
              fontWeight: 700,
              color: BLOOMBERG.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>ASK</th>
            <th style={{
              padding: '8px 12px',
              textAlign: 'right',
              fontSize: '9px',
              fontWeight: 700,
              color: BLOOMBERG.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>SPREAD</th>
            <th style={{
              padding: '8px 12px',
              textAlign: 'right',
              fontSize: '9px',
              fontWeight: 700,
              color: BLOOMBERG.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>LAST PRICE</th>
            <th style={{
              padding: '8px 12px',
              textAlign: 'right',
              fontSize: '9px',
              fontWeight: 700,
              color: BLOOMBERG.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>VOLUME</th>
            <th style={{
              padding: '8px 12px',
              textAlign: 'right',
              fontSize: '9px',
              fontWeight: 700,
              color: BLOOMBERG.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>LATENCY</th>
          </tr>
        </thead>
        <tbody>
          {brokers.map((broker) => {
            const quote = quotes.get(broker)!;
            const spread = (quote.ask && quote.bid) ? quote.ask - quote.bid : 0;
            const isBestBid = quote.bid === bestBid;
            const isBestAsk = quote.ask === bestAsk;

            return (
              <tr
                key={broker}
                style={{
                  borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                  backgroundColor: 'transparent',
                  transition: 'background-color 0.15s ease'
                }}
                onMouseEnter={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER}
                onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
              >
                <td style={{
                  padding: '10px 12px',
                  fontSize: '10px',
                  fontWeight: 700,
                  color: BLOOMBERG.ORANGE,
                  textTransform: 'uppercase',
                  letterSpacing: '0.3px'
                }}>
                  {broker}
                </td>
                <td style={{
                  padding: '10px 12px',
                  fontSize: '11px',
                  fontWeight: isBestBid ? 700 : 600,
                  color: isBestBid ? BLOOMBERG.GREEN : BLOOMBERG.WHITE,
                  textAlign: 'right'
                }}>
                  ₹{quote.bid?.toFixed(2) || '-'}
                  {isBestBid && ' BEST'}
                </td>
                <td style={{
                  padding: '10px 12px',
                  fontSize: '11px',
                  fontWeight: isBestAsk ? 700 : 600,
                  color: isBestAsk ? BLOOMBERG.GREEN : BLOOMBERG.WHITE,
                  textAlign: 'right'
                }}>
                  ₹{quote.ask?.toFixed(2) || '-'}
                  {isBestAsk && ' BEST'}
                </td>
                <td style={{
                  padding: '10px 12px',
                  fontSize: '11px',
                  fontWeight: 600,
                  color: BLOOMBERG.CYAN,
                  textAlign: 'right'
                }}>
                  ₹{spread.toFixed(2)}
                </td>
                <td style={{
                  padding: '10px 12px',
                  fontSize: '11px',
                  fontWeight: 700,
                  color: BLOOMBERG.YELLOW,
                  textAlign: 'right'
                }}>
                  ₹{quote.lastPrice.toFixed(2)}
                </td>
                <td style={{
                  padding: '10px 12px',
                  fontSize: '11px',
                  fontWeight: 600,
                  color: BLOOMBERG.WHITE,
                  textAlign: 'right'
                }}>
                  {quote.volume?.toLocaleString() || '-'}
                </td>
                <td style={{
                  padding: '10px 12px',
                  fontSize: '10px',
                  fontWeight: 600,
                  color: BLOOMBERG.MUTED,
                  textAlign: 'right'
                }}>
                  {latency.get(broker)}ms
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
};

const DepthComparison: React.FC<{
  depth: Map<BrokerType, UnifiedMarketDepth>;
  latency: Map<BrokerType, number>;
}> = ({ depth, latency }) => {
  if (depth.size === 0) {
    return (
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        height: '100%',
        color: BLOOMBERG.GRAY,
        fontSize: '11px',
        fontWeight: 600,
        letterSpacing: '0.5px'
      }}>
        ENTER A SYMBOL AND CLICK COMPARE TO VIEW MARKET DEPTH
      </div>
    );
  }

  return (
    <div style={{
      display: 'grid',
      gridTemplateColumns: 'repeat(auto-fit, minmax(350px, 1fr))',
      gap: '16px',
      maxWidth: '1400px',
      margin: '0 auto'
    }}>
      {Array.from(depth.entries()).map(([broker, marketDepth]) => (
        <div
          key={broker}
          style={{
            backgroundColor: BLOOMBERG.PANEL_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            padding: '16px'
          }}
        >
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: '12px',
            paddingBottom: '8px',
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`
          }}>
            <span style={{
              fontSize: '11px',
              fontWeight: 700,
              color: BLOOMBERG.ORANGE,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>
              {broker}
            </span>
            <span style={{
              fontSize: '9px',
              color: BLOOMBERG.MUTED,
              letterSpacing: '0.3px'
            }}>
              LATENCY: {latency.get(broker)}MS
            </span>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            {/* Bids */}
            <div>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: BLOOMBERG.GREEN,
                marginBottom: '8px',
                letterSpacing: '0.5px',
                textTransform: 'uppercase'
              }}>
                BIDS
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                {marketDepth.bids.slice(0, 5).map((bid, idx) => (
                  <div
                    key={idx}
                    style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      fontSize: '10px',
                      padding: '4px 6px',
                      backgroundColor: BLOOMBERG.INPUT_BG
                    }}
                  >
                    <span style={{ color: BLOOMBERG.GREEN, fontWeight: 700 }}>₹{bid.price.toFixed(2)}</span>
                    <span style={{ color: BLOOMBERG.MUTED }}>{bid.quantity}</span>
                  </div>
                ))}
              </div>
            </div>

            {/* Asks */}
            <div>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: BLOOMBERG.RED,
                marginBottom: '8px',
                letterSpacing: '0.5px',
                textTransform: 'uppercase'
              }}>
                ASKS
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                {marketDepth.asks.slice(0, 5).map((ask, idx) => (
                  <div
                    key={idx}
                    style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      fontSize: '10px',
                      padding: '4px 6px',
                      backgroundColor: BLOOMBERG.INPUT_BG
                    }}
                  >
                    <span style={{ color: BLOOMBERG.RED, fontWeight: 700 }}>₹{ask.price.toFixed(2)}</span>
                    <span style={{ color: BLOOMBERG.MUTED }}>{ask.quantity}</span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      ))}
    </div>
  );
};

const LatencyComparison: React.FC<{
  latency: Map<BrokerType, number>;
}> = ({ latency }) => {
  if (latency.size === 0) {
    return (
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        height: '100%',
        color: BLOOMBERG.GRAY,
        fontSize: '11px',
        fontWeight: 600,
        letterSpacing: '0.5px'
      }}>
        RUN A COMPARISON TO VIEW LATENCY DATA
      </div>
    );
  }

  const brokers = Array.from(latency.entries()).sort((a, b) => a[1] - b[1]);
  const fastest = brokers[0][0];

  return (
    <div style={{ maxWidth: '800px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {brokers.map(([broker, ms]) => {
        const isFastest = broker === fastest;
        const percentage = (ms / brokers[0][1]) * 100;

        return (
          <div
            key={broker}
            style={{
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${isFastest ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
              padding: '16px'
            }}
          >
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              marginBottom: '8px'
            }}>
              <span style={{
                fontSize: '11px',
                fontWeight: 700,
                color: BLOOMBERG.ORANGE,
                letterSpacing: '0.5px',
                textTransform: 'uppercase'
              }}>
                {broker}
              </span>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{
                  fontSize: '14px',
                  fontWeight: 700,
                  color: isFastest ? BLOOMBERG.GREEN : BLOOMBERG.WHITE,
                  fontFamily: 'monospace'
                }}>
                  {ms}MS
                </span>
                {isFastest && (
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <Zap size={12} color={BLOOMBERG.GREEN} />
                    <span style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: BLOOMBERG.GREEN,
                      letterSpacing: '0.5px'
                    }}>
                      FASTEST
                    </span>
                  </div>
                )}
              </div>
            </div>
            <div style={{
              width: '100%',
              height: '4px',
              backgroundColor: BLOOMBERG.INPUT_BG
            }}>
              <div
                style={{
                  height: '100%',
                  width: `${percentage}%`,
                  backgroundColor: isFastest ? BLOOMBERG.GREEN : BLOOMBERG.ORANGE,
                  transition: 'width 0.3s ease'
                }}
              />
            </div>
          </div>
        );
      })}
    </div>
  );
};

const SpreadAnalysis: React.FC<{
  analysis: {
    avgSpread: number;
    minSpread: number;
    maxSpread: number;
    bestBroker: BrokerType | null;
  };
}> = ({ analysis }) => {
  return (
    <div style={{ maxWidth: '1000px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '12px' }}>
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '16px'
        }}>
          <div style={{
            fontSize: '9px',
            color: BLOOMBERG.GRAY,
            marginBottom: '8px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            AVERAGE SPREAD
          </div>
          <div style={{
            fontSize: '20px',
            fontWeight: 700,
            color: BLOOMBERG.CYAN
          }}>
            ₹{analysis.avgSpread.toFixed(2)}
          </div>
        </div>

        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '16px'
        }}>
          <div style={{
            fontSize: '9px',
            color: BLOOMBERG.GRAY,
            marginBottom: '8px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            MIN SPREAD
          </div>
          <div style={{
            fontSize: '20px',
            fontWeight: 700,
            color: BLOOMBERG.GREEN
          }}>
            ₹{analysis.minSpread.toFixed(2)}
          </div>
        </div>

        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '16px'
        }}>
          <div style={{
            fontSize: '9px',
            color: BLOOMBERG.GRAY,
            marginBottom: '8px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            MAX SPREAD
          </div>
          <div style={{
            fontSize: '20px',
            fontWeight: 700,
            color: BLOOMBERG.RED
          }}>
            ₹{analysis.maxSpread.toFixed(2)}
          </div>
        </div>

        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '16px'
        }}>
          <div style={{
            fontSize: '9px',
            color: BLOOMBERG.GRAY,
            marginBottom: '8px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            BEST BROKER
          </div>
          <div style={{
            fontSize: '20px',
            fontWeight: 700,
            color: BLOOMBERG.ORANGE,
            textTransform: 'uppercase'
          }}>
            {analysis.bestBroker || '-'}
          </div>
        </div>
      </div>

      <div style={{
        backgroundColor: BLOOMBERG.PANEL_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        padding: '16px'
      }}>
        <div style={{
          fontSize: '10px',
          fontWeight: 700,
          color: BLOOMBERG.WHITE,
          marginBottom: '12px',
          letterSpacing: '0.5px',
          textTransform: 'uppercase'
        }}>
          ANALYSIS
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '10px', color: BLOOMBERG.GRAY }}>
          <div>
            TIGHTEST SPREAD: <strong style={{ color: BLOOMBERG.GREEN }}>{analysis.bestBroker?.toUpperCase()}</strong> WITH ₹{analysis.minSpread.toFixed(2)}
          </div>
          <div>
            SPREAD VARIANCE: ₹{(analysis.maxSpread - analysis.minSpread).toFixed(2)}
          </div>
          <div>
            RECOMMENDATION: USE <strong style={{ color: BLOOMBERG.ORANGE }}>{analysis.bestBroker?.toUpperCase()}</strong> FOR BEST EXECUTION QUALITY
          </div>
        </div>
      </div>
    </div>
  );
};

export default ComparisonTab;
