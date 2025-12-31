/**
 * Market Data Tab - Historical & Live Quotes
 * Fetch historical candles and real-time quotes from Fyers
 */

import React, { useState } from 'react';
import { TrendingUp, Clock, Calendar } from 'lucide-react';
import { authManager } from '../services/AuthManager';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  INPUT_BG: '#0A0A0A',
  CYAN: '#00E5FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

const MarketDataTab: React.FC = () => {
  const [symbol, setSymbol] = useState('SBIN');
  const [exchange, setExchange] = useState('NSE');
  const [interval, setInterval] = useState<'1m' | '5m' | '15m' | '1h' | '1d'>('1d');
  const [fromDate, setFromDate] = useState('2024-01-01');
  const [toDate, setToDate] = useState('2024-12-31');
  const [loading, setLoading] = useState(false);
  const [historicalData, setHistoricalData] = useState<any[]>([]);
  const [quoteData, setQuoteData] = useState<any>(null);

  const fetchHistorical = async () => {
    setLoading(true);
    try {
      const adapter = authManager.getAdapter('fyers');
      if (!adapter) throw new Error('Fyers not connected');

      // Fyers requires -EQ suffix for equities
      const fyersSymbol = `${symbol}-EQ`;

      const candles = await adapter.getHistoricalData(
        fyersSymbol,
        exchange,
        interval,
        new Date(fromDate),
        new Date(toDate)
      );

      setHistoricalData(candles);
    } catch (error: any) {
      console.error('Historical data error:', error);
      alert(error.message);
    } finally {
      setLoading(false);
    }
  };

  const fetchQuote = async () => {
    setLoading(true);
    try {
      const adapter = authManager.getAdapter('fyers');
      if (!adapter) throw new Error('Fyers not connected');

      // Fyers requires -EQ suffix for equities
      const fyersSymbol = `${symbol}-EQ`;

      const quote = await adapter.getQuote(fyersSymbol, exchange);
      setQuoteData(quote);
    } catch (error: any) {
      console.error('Quote error:', error);
      alert(error.message);
    } finally {
      setLoading(false);
    }
  };

  const inputStyle: React.CSSProperties = {
    padding: '8px 12px',
    backgroundColor: BLOOMBERG.DARK_BG,
    border: `1px solid ${BLOOMBERG.BORDER}`,
    color: BLOOMBERG.WHITE,
    fontSize: '11px',
    fontWeight: 600,
    fontFamily: 'monospace',
    outline: 'none',
  };

  const buttonStyle: React.CSSProperties = {
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
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
      color: BLOOMBERG.WHITE,
      fontFamily: 'monospace',
      overflow: 'auto',
      padding: '16px'
    }}>
      {/* Quote Section */}
      <div style={{
        marginBottom: '24px',
        padding: '16px',
        backgroundColor: BLOOMBERG.PANEL_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          marginBottom: '16px'
        }}>
          <TrendingUp size={16} color={BLOOMBERG.CYAN} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: BLOOMBERG.CYAN }}>
            LIVE QUOTE
          </span>
        </div>

        <div style={{ display: 'flex', gap: '12px', marginBottom: '12px' }}>
          <input
            type="text"
            value={symbol}
            onChange={(e) => setSymbol(e.target.value.toUpperCase())}
            placeholder="Symbol (e.g., SBIN)"
            style={{ ...inputStyle, flex: 1 }}
          />
          <select
            value={exchange}
            onChange={(e) => setExchange(e.target.value)}
            style={{ ...inputStyle, width: '120px' }}
          >
            <option value="NSE">NSE</option>
            <option value="BSE">BSE</option>
          </select>
          <button onClick={fetchQuote} disabled={loading} style={buttonStyle}>
            Get Quote
          </button>
        </div>

        {quoteData && (
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(4, 1fr)',
            gap: '12px',
            padding: '12px',
            backgroundColor: BLOOMBERG.INPUT_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`
          }}>
            <div>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY }}>LAST PRICE</div>
              <div style={{ fontSize: '14px', fontWeight: 700, color: BLOOMBERG.GREEN }}>
                ₹{quoteData.lastPrice?.toFixed(2)}
              </div>
            </div>
            <div>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY }}>CHANGE</div>
              <div style={{
                fontSize: '12px',
                fontWeight: 700,
                color: quoteData.change >= 0 ? BLOOMBERG.GREEN : '#FF3B3B'
              }}>
                {quoteData.change >= 0 ? '+' : ''}{quoteData.change?.toFixed(2)} ({quoteData.changePercent?.toFixed(2)}%)
              </div>
            </div>
            <div>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY }}>VOLUME</div>
              <div style={{ fontSize: '12px', fontWeight: 700 }}>{quoteData.volume?.toLocaleString()}</div>
            </div>
            <div>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY }}>O/H/L/C</div>
              <div style={{ fontSize: '10px' }}>
                {quoteData.open?.toFixed(2)}/{quoteData.high?.toFixed(2)}/{quoteData.low?.toFixed(2)}/{quoteData.close?.toFixed(2)}
              </div>
            </div>
          </div>
        )}
      </div>

      {/* Historical Data Section */}
      <div style={{
        padding: '16px',
        backgroundColor: BLOOMBERG.PANEL_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          marginBottom: '16px'
        }}>
          <Calendar size={16} color={BLOOMBERG.ORANGE} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: BLOOMBERG.ORANGE }}>
            HISTORICAL DATA
          </span>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '12px', marginBottom: '12px' }}>
          <select
            value={interval}
            onChange={(e) => setInterval(e.target.value as any)}
            style={inputStyle}
          >
            <option value="1m">1 Minute</option>
            <option value="5m">5 Minutes</option>
            <option value="15m">15 Minutes</option>
            <option value="1h">1 Hour</option>
            <option value="1d">1 Day</option>
          </select>
          <input
            type="date"
            value={fromDate}
            onChange={(e) => setFromDate(e.target.value)}
            style={inputStyle}
          />
          <input
            type="date"
            value={toDate}
            onChange={(e) => setToDate(e.target.value)}
            style={inputStyle}
          />
          <div style={{ gridColumn: 'span 2' }}>
            <button onClick={fetchHistorical} disabled={loading} style={{ ...buttonStyle, width: '100%' }}>
              Fetch Historical Data
            </button>
          </div>
        </div>

        {historicalData.length > 0 && (
          <div style={{
            maxHeight: '400px',
            overflow: 'auto',
            backgroundColor: BLOOMBERG.INPUT_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`
          }}>
            <table style={{ width: '100%', borderCollapse: 'collapse' }}>
              <thead style={{ position: 'sticky', top: 0, backgroundColor: BLOOMBERG.HEADER_BG }}>
                <tr>
                  <th style={{ padding: '8px', textAlign: 'left', fontSize: '9px', color: BLOOMBERG.GRAY }}>DATE</th>
                  <th style={{ padding: '8px', textAlign: 'right', fontSize: '9px', color: BLOOMBERG.GRAY }}>OPEN</th>
                  <th style={{ padding: '8px', textAlign: 'right', fontSize: '9px', color: BLOOMBERG.GRAY }}>HIGH</th>
                  <th style={{ padding: '8px', textAlign: 'right', fontSize: '9px', color: BLOOMBERG.GRAY }}>LOW</th>
                  <th style={{ padding: '8px', textAlign: 'right', fontSize: '9px', color: BLOOMBERG.GRAY }}>CLOSE</th>
                  <th style={{ padding: '8px', textAlign: 'right', fontSize: '9px', color: BLOOMBERG.GRAY }}>VOLUME</th>
                </tr>
              </thead>
              <tbody>
                {historicalData.map((candle, idx) => (
                  <tr key={idx} style={{ borderTop: `1px solid ${BLOOMBERG.BORDER}` }}>
                    <td style={{ padding: '8px', fontSize: '10px' }}>
                      {candle.timestamp ? new Date(candle.timestamp).toLocaleString() : 'N/A'}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontSize: '10px' }}>
                      {candle.open?.toFixed(2) || 'N/A'}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontSize: '10px' }}>
                      {candle.high?.toFixed(2) || 'N/A'}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontSize: '10px' }}>
                      {candle.low?.toFixed(2) || 'N/A'}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontSize: '10px' }}>
                      {candle.close?.toFixed(2) || 'N/A'}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontSize: '10px' }}>
                      {candle.volume?.toLocaleString() || 'N/A'}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
};

export default MarketDataTab;
