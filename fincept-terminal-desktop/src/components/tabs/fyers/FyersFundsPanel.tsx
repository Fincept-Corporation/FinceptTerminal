// Fyers Funds Panel
// Display account funds and limits

import React, { useState, useEffect } from 'react';
import { Wallet } from 'lucide-react';
import { fyersService, FyersFund } from '../../../services/fyersService';

const FyersFundsPanel: React.FC = () => {
  const [funds, setFunds] = useState<FyersFund[]>([]);
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
    loadFunds();
  }, []);

  const loadFunds = async () => {
    setIsLoading(true);
    setError('');

    try {
      const data = await fyersService.getFunds();
      setFunds(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load funds');
    } finally {
      setIsLoading(false);
    }
  };

  const formatCurrency = (amount: number) => {
    return `₹${amount.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`;
  };

  if (isLoading) {
    return (
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '12px',
        marginBottom: '12px'
      }}>
        <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px' }}>Loading funds...</div>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `1px solid #FF0000`,
        padding: '12px',
        marginBottom: '12px'
      }}>
        <div style={{ color: '#FF0000', fontSize: '10px' }}>{error}</div>
      </div>
    );
  }

  // Find key fund items
  const availableBalance = funds.find(f => f.id === 10);
  const utilizedAmount = funds.find(f => f.id === 2);
  const realizedPL = funds.find(f => f.id === 4);

  return (
    <div style={{
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_ORANGE}`,
      padding: '12px',
      marginBottom: '12px'
    }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        marginBottom: '12px',
        paddingBottom: '8px',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        <Wallet size={12} color={BLOOMBERG_ORANGE} />
        <span style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
          FUNDS & LIMITS
        </span>
      </div>

      {/* Summary Cards */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(3, 1fr)',
        gap: '12px',
        marginBottom: '12px'
      }}>
        {/* Available Balance */}
        {availableBalance && (
          <div style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GREEN}`,
            padding: '10px'
          }}>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px', marginBottom: '4px' }}>
              AVAILABLE BALANCE
            </div>
            <div style={{ color: BLOOMBERG_GREEN, fontSize: '13px', fontWeight: 'bold' }}>
              {formatCurrency(availableBalance.equityAmount)}
            </div>
            {availableBalance.commodityAmount > 0 && (
              <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px', marginTop: '2px' }}>
                Commodity: {formatCurrency(availableBalance.commodityAmount)}
              </div>
            )}
          </div>
        )}

        {/* Utilized */}
        {utilizedAmount && (
          <div style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_ORANGE}`,
            padding: '10px'
          }}>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px', marginBottom: '4px' }}>
              UTILIZED AMOUNT
            </div>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '13px', fontWeight: 'bold' }}>
              {formatCurrency(utilizedAmount.equityAmount)}
            </div>
          </div>
        )}

        {/* Realized P&L */}
        {realizedPL && (
          <div style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${realizedPL.equityAmount >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED}`,
            padding: '10px'
          }}>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px', marginBottom: '4px' }}>
              REALIZED P&L
            </div>
            <div style={{
              color: realizedPL.equityAmount >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              fontSize: '13px',
              fontWeight: 'bold'
            }}>
              {formatCurrency(realizedPL.equityAmount)}
            </div>
          </div>
        )}
      </div>

      {/* Detailed Breakdown */}
      <div style={{
        backgroundColor: BLOOMBERG_DARK_BG,
        border: `1px solid ${BLOOMBERG_GRAY}`,
        maxHeight: '200px',
        overflow: 'auto'
      }}>
        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '9px' }}>
          <thead>
            <tr style={{ backgroundColor: BLOOMBERG_PANEL_BG, borderBottom: `1px solid ${BLOOMBERG_GRAY}` }}>
              <th style={{ padding: '6px', textAlign: 'left', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                CATEGORY
              </th>
              <th style={{ padding: '6px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                EQUITY
              </th>
              <th style={{ padding: '6px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                COMMODITY
              </th>
            </tr>
          </thead>
          <tbody>
            {funds.map((fund, index) => (
              <tr
                key={fund.id}
                style={{
                  borderBottom: index < funds.length - 1 ? `1px solid ${BLOOMBERG_GRAY}` : 'none'
                }}
              >
                <td style={{ padding: '6px', color: BLOOMBERG_WHITE }}>
                  {fund.title}
                </td>
                <td style={{
                  padding: '6px',
                  textAlign: 'right',
                  color: fund.equityAmount >= 0 ? BLOOMBERG_WHITE : BLOOMBERG_RED,
                  fontFamily: 'Consolas, monospace'
                }}>
                  {formatCurrency(fund.equityAmount)}
                </td>
                <td style={{
                  padding: '6px',
                  textAlign: 'right',
                  color: BLOOMBERG_GRAY,
                  fontFamily: 'Consolas, monospace'
                }}>
                  {formatCurrency(fund.commodityAmount)}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default FyersFundsPanel;
