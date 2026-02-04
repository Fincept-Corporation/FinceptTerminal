/**
 * EquityOrderEntry - Right sidebar order entry section with loading/connecting states
 */
import React from 'react';
import { AlertCircle, Loader2, RefreshCw } from 'lucide-react';
import { StockOrderForm } from './StockOrderForm';
import { FINCEPT } from '../constants';
import type { EquityOrderEntryProps } from '../types';

export const EquityOrderEntry: React.FC<EquityOrderEntryProps> = ({
  selectedSymbol,
  selectedExchange,
  currentPrice,
  isAuthenticated,
  isLoading,
  isConnecting,
  onConnectBroker,
}) => {
  return (
    <div style={{
      height: '50%',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      <div style={{
        padding: '8px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '10px',
        fontWeight: 700,
        color: FINCEPT.ORANGE,
        letterSpacing: '0.5px'
      }}>
        ORDER ENTRY
      </div>
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {/* Loading state */}
        {isLoading ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            gap: '12px',
          }}>
            <Loader2 size={24} color={FINCEPT.ORANGE} style={{ animation: 'spin 1s linear infinite' }} />
            <div style={{ color: FINCEPT.GRAY, fontSize: '11px' }}>Loading...</div>
          </div>
        ) : isConnecting ? (
          /* Connecting state */
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            gap: '12px',
            textAlign: 'center',
          }}>
            <Loader2 size={28} color={FINCEPT.YELLOW} style={{ animation: 'spin 1s linear infinite' }} />
            <div>
              <p style={{ color: FINCEPT.WHITE, fontSize: '12px', fontWeight: 600, margin: '0 0 4px 0' }}>
                Connecting to Broker...
              </p>
              <p style={{ color: FINCEPT.GRAY, fontSize: '10px', margin: 0 }}>
                Please wait while we establish connection
              </p>
            </div>
          </div>
        ) : !isAuthenticated ? (
          /* Not connected state */
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            gap: '12px',
            textAlign: 'center',
          }}>
            <AlertCircle size={32} color={FINCEPT.ORANGE} />
            <div>
              <p style={{ color: FINCEPT.WHITE, fontSize: '12px', fontWeight: 600, margin: '0 0 4px 0' }}>
                No Broker Connected
              </p>
              <p style={{ color: FINCEPT.GRAY, fontSize: '10px', margin: 0 }}>
                Configure your broker in the BROKERS tab below
              </p>
            </div>
            <button
              onClick={onConnectBroker}
              style={{
                padding: '8px 16px',
                backgroundColor: FINCEPT.ORANGE,
                border: 'none',
                color: FINCEPT.DARK_BG,
                fontSize: '10px',
                fontWeight: 700,
                cursor: 'pointer',
                transition: 'all 0.15s',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = '#FF9922';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = FINCEPT.ORANGE;
              }}
            >
              CONNECT BROKER
            </button>
          </div>
        ) : (
          <StockOrderForm
            symbol={selectedSymbol}
            exchange={selectedExchange}
            lastPrice={currentPrice}
          />
        )}
      </div>
    </div>
  );
};
