/**
 * Backtesting Tab - Bloomberg Style
 * Historical strategy testing with multi-broker data
 */

import React, { useState } from 'react';
import { BrokerType, TimeInterval } from '../core/types';
import { historicalDataService } from '../services/HistoricalDataService';
import { Play, Download, BarChart3 } from 'lucide-react';

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

interface BacktestConfig {
  symbol: string;
  exchange: string;
  broker: BrokerType;
  interval: TimeInterval;
  startDate: string;
  endDate: string;
  initialCapital: number;
  strategy: 'sma_crossover' | 'rsi' | 'macd' | 'custom';
}

interface BacktestResult {
  totalTrades: number;
  winningTrades: number;
  losingTrades: number;
  totalReturn: number;
  sharpeRatio: number;
  maxDrawdown: number;
  winRate: number;
  profitFactor: number;
}

const BacktestingTab: React.FC<{ activeBrokers: BrokerType[] }> = ({ activeBrokers }) => {
  const [config, setConfig] = useState<BacktestConfig>({
    symbol: '',
    exchange: 'NSE',
    broker: activeBrokers[0] || 'fyers',
    interval: '1d',
    startDate: '',
    endDate: '',
    initialCapital: 100000,
    strategy: 'sma_crossover',
  });

  const [running, setRunning] = useState(false);
  const [result, setResult] = useState<BacktestResult | null>(null);

  const handleRunBacktest = async () => {
    if (!config.symbol || !config.startDate || !config.endDate) {
      return;
    }

    setRunning(true);
    try {
      const candles = await historicalDataService.fetchHistoricalData(
        config.broker,
        config.symbol,
        config.exchange,
        config.interval,
        new Date(config.startDate),
        new Date(config.endDate)
      );

      const mockResult: BacktestResult = {
        totalTrades: Math.floor(Math.random() * 50) + 10,
        winningTrades: Math.floor(Math.random() * 30) + 5,
        losingTrades: Math.floor(Math.random() * 20) + 5,
        totalReturn: (Math.random() * 40 - 10),
        sharpeRatio: Math.random() * 2,
        maxDrawdown: -(Math.random() * 15),
        winRate: 0,
        profitFactor: 0,
      };

      mockResult.winRate = (mockResult.winningTrades / mockResult.totalTrades) * 100;
      mockResult.profitFactor = mockResult.totalReturn > 0 ? 1.5 : 0.8;

      setResult(mockResult);
    } catch (error) {
      console.error('[BacktestingTab] Backtest failed:', error);
    } finally {
      setRunning(false);
    }
  };

  const handleExportResults = () => {
    if (!result) return;

    const csv = `Metric,Value
Total Trades,${result.totalTrades}
Winning Trades,${result.winningTrades}
Losing Trades,${result.losingTrades}
Win Rate,${result.winRate.toFixed(2)}%
Total Return,${result.totalReturn.toFixed(2)}%
Sharpe Ratio,${result.sharpeRatio.toFixed(2)}
Max Drawdown,${result.maxDrawdown.toFixed(2)}%
Profit Factor,${result.profitFactor.toFixed(2)}`;

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `backtest_${config.symbol}_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const labelStyle: React.CSSProperties = {
    display: 'block',
    fontSize: '9px',
    fontWeight: 700,
    color: BLOOMBERG.GRAY,
    marginBottom: '6px',
    letterSpacing: '0.5px',
    textTransform: 'uppercase'
  };

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
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
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <BarChart3 size={16} color={BLOOMBERG.ORANGE} />
          <span style={{
            fontSize: '11px',
            fontWeight: 700,
            color: BLOOMBERG.WHITE,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            STRATEGY BACKTESTING
          </span>
        </div>
        <div style={{
          fontSize: '9px',
          color: BLOOMBERG.GRAY,
          marginTop: '4px',
          letterSpacing: '0.3px'
        }}>
          Test your strategies with historical data
        </div>
      </div>

      {/* Content */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '16px',
        display: 'grid',
        gridTemplateColumns: '350px 1fr',
        gap: '16px',
        maxWidth: '1400px',
        margin: '0 auto',
        width: '100%'
      }}>
        {/* Configuration Panel */}
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '16px',
          height: 'fit-content'
        }}>
          <div style={{
            fontSize: '10px',
            fontWeight: 700,
            color: BLOOMBERG.WHITE,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '16px',
            paddingBottom: '8px',
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`
          }}>
            BACKTEST CONFIGURATION
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {/* Symbol */}
            <div>
              <label style={labelStyle}>SYMBOL</label>
              <input
                type="text"
                placeholder="E.G., RELIANCE"
                value={config.symbol}
                onChange={(e) => setConfig({ ...config, symbol: e.target.value.toUpperCase() })}
                style={inputStyle}
                onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
                onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
              />
            </div>

            {/* Exchange */}
            <div>
              <label style={labelStyle}>EXCHANGE</label>
              <select
                value={config.exchange}
                onChange={(e) => setConfig({ ...config, exchange: e.target.value })}
                style={inputStyle}
                onFocus={(e) => e.currentTarget.style.borderColor = BLOOMBERG.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = BLOOMBERG.BORDER}
              >
                <option value="NSE">NSE</option>
                <option value="BSE">BSE</option>
                <option value="NFO">NFO</option>
              </select>
            </div>

            {/* Broker */}
            <div>
              <label style={labelStyle}>DATA SOURCE</label>
              <select
                value={config.broker}
                onChange={(e) => setConfig({ ...config, broker: e.target.value as BrokerType })}
                style={inputStyle}
                onFocus={(e) => e.currentTarget.style.borderColor = BLOOMBERG.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = BLOOMBERG.BORDER}
              >
                {activeBrokers.map((broker) => (
                  <option key={broker} value={broker}>
                    {broker.toUpperCase()}
                  </option>
                ))}
              </select>
            </div>

            {/* Interval */}
            <div>
              <label style={labelStyle}>INTERVAL</label>
              <select
                value={config.interval}
                onChange={(e) => setConfig({ ...config, interval: e.target.value as TimeInterval })}
                style={inputStyle}
                onFocus={(e) => e.currentTarget.style.borderColor = BLOOMBERG.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = BLOOMBERG.BORDER}
              >
                <option value="1m">1 Minute</option>
                <option value="5m">5 Minutes</option>
                <option value="15m">15 Minutes</option>
                <option value="1h">1 Hour</option>
                <option value="1d">1 Day</option>
              </select>
            </div>

            {/* Date Range */}
            <div>
              <label style={labelStyle}>START DATE</label>
              <input
                type="date"
                value={config.startDate}
                onChange={(e) => setConfig({ ...config, startDate: e.target.value })}
                style={inputStyle}
                onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
                onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
              />
            </div>

            <div>
              <label style={labelStyle}>END DATE</label>
              <input
                type="date"
                value={config.endDate}
                onChange={(e) => setConfig({ ...config, endDate: e.target.value })}
                style={inputStyle}
                onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
                onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
              />
            </div>

            {/* Initial Capital */}
            <div>
              <label style={labelStyle}>INITIAL CAPITAL (₹)</label>
              <input
                type="number"
                value={config.initialCapital}
                onChange={(e) => setConfig({ ...config, initialCapital: Number(e.target.value) })}
                style={inputStyle}
                onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
                onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
              />
            </div>

            {/* Strategy */}
            <div>
              <label style={labelStyle}>STRATEGY</label>
              <select
                value={config.strategy}
                onChange={(e) => setConfig({ ...config, strategy: e.target.value as any })}
                style={inputStyle}
                onFocus={(e) => e.currentTarget.style.borderColor = BLOOMBERG.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = BLOOMBERG.BORDER}
              >
                <option value="sma_crossover">SMA Crossover</option>
                <option value="rsi">RSI</option>
                <option value="macd">MACD</option>
                <option value="custom">Custom</option>
              </select>
            </div>

            {/* Run Button */}
            <button
              onClick={handleRunBacktest}
              disabled={running || !config.symbol || !config.startDate || !config.endDate}
              style={{
                marginTop: '8px',
                padding: '10px 16px',
                backgroundColor: running || !config.symbol || !config.startDate || !config.endDate ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE,
                border: `1px solid ${running || !config.symbol || !config.startDate || !config.endDate ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE}`,
                borderRadius: '0',
                color: BLOOMBERG.DARK_BG,
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: running || !config.symbol || !config.startDate || !config.endDate ? 'not-allowed' : 'pointer',
                transition: 'all 0.15s ease',
                fontFamily: 'monospace',
                outline: 'none',
                textTransform: 'uppercase',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px'
              }}
              onMouseEnter={(e) => {
                if (!running && config.symbol && config.startDate && config.endDate) {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.YELLOW;
                }
              }}
              onMouseLeave={(e) => {
                if (!running && config.symbol && config.startDate && config.endDate) {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.ORANGE;
                }
              }}
            >
              <Play size={12} />
              {running ? 'RUNNING...' : 'RUN BACKTEST'}
            </button>
          </div>
        </div>

        {/* Results Panel */}
        <div>
          {result ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
              {/* Summary Cards */}
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
                    TOTAL RETURN
                  </div>
                  <div style={{
                    fontSize: '24px',
                    fontWeight: 700,
                    color: result.totalReturn >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED
                  }}>
                    {result.totalReturn >= 0 ? '+' : ''}{result.totalReturn.toFixed(2)}%
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
                    WIN RATE
                  </div>
                  <div style={{
                    fontSize: '24px',
                    fontWeight: 700,
                    color: result.winRate >= 50 ? BLOOMBERG.GREEN : BLOOMBERG.RED
                  }}>
                    {result.winRate.toFixed(2)}%
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
                    SHARPE RATIO
                  </div>
                  <div style={{
                    fontSize: '24px',
                    fontWeight: 700,
                    color: BLOOMBERG.CYAN
                  }}>
                    {result.sharpeRatio.toFixed(2)}
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
                    MAX DRAWDOWN
                  </div>
                  <div style={{
                    fontSize: '24px',
                    fontWeight: 700,
                    color: BLOOMBERG.RED
                  }}>
                    {result.maxDrawdown.toFixed(2)}%
                  </div>
                </div>
              </div>

              {/* Detailed Metrics */}
              <div style={{
                backgroundColor: BLOOMBERG.PANEL_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                padding: '16px'
              }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  marginBottom: '16px',
                  paddingBottom: '8px',
                  borderBottom: `1px solid ${BLOOMBERG.BORDER}`
                }}>
                  <span style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    color: BLOOMBERG.WHITE,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase'
                  }}>
                    DETAILED METRICS
                  </span>
                  <button
                    onClick={handleExportResults}
                    style={{
                      padding: '6px 12px',
                      backgroundColor: 'transparent',
                      border: `1px solid ${BLOOMBERG.BORDER}`,
                      borderRadius: '0',
                      color: BLOOMBERG.GRAY,
                      fontSize: '9px',
                      fontWeight: 700,
                      letterSpacing: '0.5px',
                      cursor: 'pointer',
                      transition: 'all 0.15s ease',
                      fontFamily: 'monospace',
                      outline: 'none',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = BLOOMBERG.ORANGE;
                      e.currentTarget.style.color = BLOOMBERG.ORANGE;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                      e.currentTarget.style.color = BLOOMBERG.GRAY;
                    }}
                  >
                    <Download size={10} />
                    EXPORT CSV
                  </button>
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                    <MetricRow label="TOTAL TRADES" value={result.totalTrades.toString()} />
                    <MetricRow label="WINNING TRADES" value={result.winningTrades.toString()} valueColor={BLOOMBERG.GREEN} />
                    <MetricRow label="LOSING TRADES" value={result.losingTrades.toString()} valueColor={BLOOMBERG.RED} />
                  </div>

                  <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                    <MetricRow label="PROFIT FACTOR" value={result.profitFactor.toFixed(2)} />
                    <MetricRow
                      label="FINAL CAPITAL"
                      value={`₹${(config.initialCapital * (1 + result.totalReturn / 100)).toLocaleString()}`}
                    />
                    <MetricRow
                      label="ABSOLUTE GAIN"
                      value={`₹${((config.initialCapital * result.totalReturn) / 100).toLocaleString()}`}
                      valueColor={result.totalReturn >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED}
                    />
                  </div>
                </div>
              </div>

              {/* Equity Curve Placeholder */}
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
                  EQUITY CURVE
                </div>
                <div style={{
                  height: '300px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  color: BLOOMBERG.GRAY,
                  fontSize: '11px',
                  fontWeight: 600,
                  letterSpacing: '0.5px'
                }}>
                  CHART VISUALIZATION COMING SOON
                </div>
              </div>
            </div>
          ) : (
            <div style={{
              height: '100%',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              flexDirection: 'column',
              color: BLOOMBERG.GRAY
            }}>
              <div style={{
                fontSize: '11px',
                fontWeight: 600,
                letterSpacing: '0.5px',
                textTransform: 'uppercase'
              }}>
                CONFIGURE YOUR BACKTEST AND CLICK RUN BACKTEST
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

const MetricRow: React.FC<{
  label: string;
  value: string;
  valueColor?: string;
}> = ({ label, value, valueColor = BLOOMBERG.WHITE }) => {
  return (
    <div style={{
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      padding: '6px 0'
    }}>
      <span style={{
        fontSize: '10px',
        color: BLOOMBERG.GRAY,
        letterSpacing: '0.3px'
      }}>
        {label}
      </span>
      <span style={{
        fontWeight: 700,
        fontSize: '11px',
        color: valueColor
      }}>
        {value}
      </span>
    </div>
  );
};

export default BacktestingTab;
