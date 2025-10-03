// Fyers Holdings Panel
// Display long-term holdings with P&L

import React, { useState, useEffect } from 'react';
import { Package, RefreshCw } from 'lucide-react';
import { fyersService, FyersHoldingsResponse } from '../../../services/fyersService';

const FyersHoldingsPanel: React.FC = () => {
  const [holdings, setHoldings] = useState<FyersHoldingsResponse | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState('');

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_RED = '#FF0000';

  useEffect(() => {
    loadHoldings();
  }, []);

  const loadHoldings = async () => {
    setIsLoading(true);
    setError('');

    try {
      const data = await fyersService.getHoldings();
      setHoldings(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load holdings');
    } finally {
      setIsLoading(false);
    }
  };

  const formatCurrency = (amount: number) => {
    return `₹${amount.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`;
  };

  const formatPercent = (value: number) => {
    return `${value >= 0 ? '+' : ''}${value.toFixed(2)}%`;
  };

  if (isLoading) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: BLOOMBERG_DARK_BG,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center'
      }}>
        <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px' }}>Loading holdings...</div>
      </div>
    );
  }

  if (error || !holdings) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: BLOOMBERG_DARK_BG,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        flexDirection: 'column',
        gap: '10px'
      }}>
        <div style={{ color: BLOOMBERG_RED, fontSize: '10px' }}>{error || 'Failed to load holdings'}</div>
        <button
          onClick={loadHoldings}
          style={{
            backgroundColor: BLOOMBERG_ORANGE,
            color: 'black',
            border: 'none',
            padding: '6px 12px',
            fontSize: '9px',
            fontWeight: 'bold',
            cursor: 'pointer'
          }}
        >
          RETRY
        </button>
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Header with Summary */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '12px',
        flexShrink: 0
      }}>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '12px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Package size={12} color={BLOOMBERG_ORANGE} />
            <span style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
              HOLDINGS ({holdings.overall.count_total})
            </span>
          </div>

          <button
            onClick={loadHoldings}
            style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '4px 8px',
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <RefreshCw size={10} />
            REFRESH
          </button>
        </div>

        {/* Overall Summary */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(4, 1fr)',
          gap: '12px'
        }}>
          <div style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '8px'
          }}>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px', marginBottom: '4px' }}>
              TOTAL INVESTMENT
            </div>
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', fontWeight: 'bold' }}>
              {formatCurrency(holdings.overall.total_investment)}
            </div>
          </div>

          <div style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '8px'
          }}>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px', marginBottom: '4px' }}>
              CURRENT VALUE
            </div>
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', fontWeight: 'bold' }}>
              {formatCurrency(holdings.overall.total_current_value)}
            </div>
          </div>

          <div style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${holdings.overall.total_pl >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED}`,
            padding: '8px'
          }}>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px', marginBottom: '4px' }}>
              TOTAL P&L
            </div>
            <div style={{
              color: holdings.overall.total_pl >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              fontSize: '11px',
              fontWeight: 'bold'
            }}>
              {formatCurrency(holdings.overall.total_pl)}
            </div>
          </div>

          <div style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${holdings.overall.pnl_perc >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED}`,
            padding: '8px'
          }}>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px', marginBottom: '4px' }}>
              P&L %
            </div>
            <div style={{
              color: holdings.overall.pnl_perc >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              fontSize: '11px',
              fontWeight: 'bold'
            }}>
              {formatPercent(holdings.overall.pnl_perc)}
            </div>
          </div>
        </div>
      </div>

      {/* Holdings Table */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {holdings.holdings.length === 0 ? (
          <div style={{
            textAlign: 'center',
            color: BLOOMBERG_GRAY,
            padding: '40px',
            fontSize: '10px'
          }}>
            No holdings found
          </div>
        ) : (
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`
          }}>
            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '9px' }}>
              <thead>
                <tr style={{ backgroundColor: BLOOMBERG_DARK_BG, borderBottom: `1px solid ${BLOOMBERG_GRAY}` }}>
                  <th style={{ padding: '8px', textAlign: 'left', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    SYMBOL
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    QTY
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    AVG COST
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    LTP
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    MARKET VALUE
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    P&L
                  </th>
                  <th style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    TYPE
                  </th>
                </tr>
              </thead>
              <tbody>
                {holdings.holdings.map((holding, index) => (
                  <tr
                    key={holding.fyToken}
                    style={{
                      borderBottom: index < holdings.holdings.length - 1 ? `1px solid ${BLOOMBERG_GRAY}` : 'none',
                      backgroundColor: index % 2 === 0 ? BLOOMBERG_DARK_BG : BLOOMBERG_PANEL_BG
                    }}
                  >
                    <td style={{ padding: '8px', color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>
                      {holding.symbol.replace('NSE:', '').replace('-EQ', '')}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_WHITE, fontFamily: 'Consolas, monospace' }}>
                      {holding.quantity}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_GRAY, fontFamily: 'Consolas, monospace' }}>
                      {formatCurrency(holding.costPrice)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_WHITE, fontFamily: 'Consolas, monospace' }}>
                      {formatCurrency(holding.ltp)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_WHITE, fontFamily: 'Consolas, monospace' }}>
                      {formatCurrency(holding.marketVal)}
                    </td>
                    <td style={{
                      padding: '8px',
                      textAlign: 'right',
                      color: holding.pl >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                      fontFamily: 'Consolas, monospace',
                      fontWeight: 'bold'
                    }}>
                      {formatCurrency(holding.pl)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_GRAY, fontSize: '8px' }}>
                      {holding.holdingType}
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

export default FyersHoldingsPanel;
