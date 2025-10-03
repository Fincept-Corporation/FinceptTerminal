// Fyers Positions Panel
// Display open positions with live updates

import React, { useState, useEffect } from 'react';
import { Activity, RefreshCw } from 'lucide-react';
import { fyersService, FyersPosition } from '../../../services/fyersService';
import { fyersWebSocket } from '../../../services/fyersWebSocket';

interface FyersPositionsPanelProps {
  wsConnected: boolean;
}

const FyersPositionsPanel: React.FC<FyersPositionsPanelProps> = ({ wsConnected }) => {
  const [positions, setPositions] = useState<FyersPosition[]>([]);
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
    loadPositions();

    // Subscribe to position updates if WebSocket connected
    if (wsConnected) {
      fyersWebSocket.onPositionUpdate((update) => {
        console.log('[Positions] WebSocket update:', update);
        loadPositions(); // Refresh positions on update
      });
    }

    return () => {
      fyersWebSocket.clearCallbacks();
    };
  }, [wsConnected]);

  const loadPositions = async () => {
    setIsLoading(true);
    setError('');

    try {
      const data = await fyersService.getPositions();
      setPositions(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load positions');
    } finally {
      setIsLoading(false);
    }
  };

  const formatCurrency = (amount: number) => {
    return `₹${amount.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`;
  };

  if (isLoading && positions.length === 0) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: BLOOMBERG_DARK_BG,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center'
      }}>
        <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px' }}>Loading positions...</div>
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
      {/* Header */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '12px',
        flexShrink: 0,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Activity size={12} color={BLOOMBERG_ORANGE} />
          <span style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
            POSITIONS ({positions.length})
          </span>
          {wsConnected && (
            <span style={{
              color: BLOOMBERG_GREEN,
              fontSize: '8px',
              backgroundColor: BLOOMBERG_DARK_BG,
              padding: '2px 6px',
              border: `1px solid ${BLOOMBERG_GREEN}`
            }}>
              LIVE
            </span>
          )}
        </div>

        <button
          onClick={loadPositions}
          disabled={isLoading}
          style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '4px 8px',
            fontSize: '9px',
            cursor: isLoading ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            opacity: isLoading ? 0.5 : 1
          }}
        >
          <RefreshCw size={10} />
          REFRESH
        </button>
      </div>

      {/* Positions Table */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {error && (
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_RED}`,
            color: BLOOMBERG_RED,
            padding: '10px',
            fontSize: '9px',
            marginBottom: '12px'
          }}>
            {error}
          </div>
        )}

        {positions.length === 0 ? (
          <div style={{
            textAlign: 'center',
            color: BLOOMBERG_GRAY,
            padding: '40px',
            fontSize: '10px'
          }}>
            No open positions
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
                  <th style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    PRODUCT
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    QTY
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    AVG PRICE
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    LTP
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    P&L
                  </th>
                </tr>
              </thead>
              <tbody>
                {positions.map((position, index) => (
                  <tr
                    key={position.id || index}
                    style={{
                      borderBottom: index < positions.length - 1 ? `1px solid ${BLOOMBERG_GRAY}` : 'none',
                      backgroundColor: index % 2 === 0 ? BLOOMBERG_DARK_BG : BLOOMBERG_PANEL_BG
                    }}
                  >
                    <td style={{ padding: '8px', color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>
                      {position.symbol}
                      <span style={{
                        marginLeft: '6px',
                        color: position.netQty > 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                        fontSize: '8px',
                        fontWeight: 'bold'
                      }}>
                        {position.netQty > 0 ? 'LONG' : 'SHORT'}
                      </span>
                    </td>
                    <td style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_GRAY, fontSize: '8px' }}>
                      {position.productType}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_WHITE, fontFamily: 'Consolas, monospace' }}>
                      {Math.abs(position.netQty)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_GRAY, fontFamily: 'Consolas, monospace' }}>
                      {formatCurrency(position.netAvg)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_WHITE, fontFamily: 'Consolas, monospace' }}>
                      {formatCurrency(position.ltp)}
                    </td>
                    <td style={{
                      padding: '8px',
                      textAlign: 'right',
                      color: position.pl >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                      fontFamily: 'Consolas, monospace',
                      fontWeight: 'bold'
                    }}>
                      {formatCurrency(position.pl)}
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

export default FyersPositionsPanel;
