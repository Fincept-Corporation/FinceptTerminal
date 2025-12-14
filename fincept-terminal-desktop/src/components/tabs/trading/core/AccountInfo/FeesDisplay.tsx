/**
 * FeesDisplay - Shows trading fees for current exchange
 */

import React from 'react';
import { DollarSign, Info } from 'lucide-react';
import { useFees } from '../../hooks/useAccountInfo';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

// Bloomberg color palette
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

export function FeesDisplay() {
  const { fees, isLoading } = useFees();
  const { activeBroker } = useBrokerContext();

  if (isLoading || !fees) {
    return (
      <div style={{
        background: BLOOMBERG.PANEL_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        borderRadius: '4px',
        padding: '12px'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          marginBottom: '12px'
        }}>
          <DollarSign size={14} color={BLOOMBERG.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 600, color: BLOOMBERG.WHITE }}>TRADING FEES</span>
        </div>
        {isLoading ? (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '16px' }}>
            <div style={{
              width: '20px',
              height: '20px',
              border: `2px solid ${BLOOMBERG.BORDER}`,
              borderTopColor: BLOOMBERG.ORANGE,
              borderRadius: '50%',
              animation: 'spin 1s linear infinite'
            }} />
          </div>
        ) : (
          <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY }}>No fee information available</div>
        )}
      </div>
    );
  }

  return (
    <div style={{
      background: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderRadius: '4px',
      padding: '12px'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '12px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <DollarSign size={14} color={BLOOMBERG.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 600, color: BLOOMBERG.WHITE }}>TRADING FEES</span>
        </div>
        <span style={{ fontSize: '9px', color: BLOOMBERG.GRAY, fontWeight: 600 }}>{activeBroker?.toUpperCase()}</span>
      </div>

      {/* Fee Structure */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        {/* Maker Fee */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '8px',
          background: BLOOMBERG.DARK_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '3px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY }}>MAKER FEE</div>
            <div style={{ position: 'relative', display: 'inline-block' }} title="Fee for adding liquidity to the order book">
              <Info size={10} color={BLOOMBERG.MUTED} style={{ cursor: 'help' }} />
            </div>
          </div>
          <div style={{ fontSize: '11px', fontFamily: '"IBM Plex Mono", monospace', color: BLOOMBERG.GREEN, fontWeight: 600 }}>
            {fees.makerPercent}
          </div>
        </div>

        {/* Taker Fee */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '8px',
          background: BLOOMBERG.DARK_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '3px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY }}>TAKER FEE</div>
            <div style={{ position: 'relative', display: 'inline-block' }} title="Fee for taking liquidity from the order book">
              <Info size={10} color={BLOOMBERG.MUTED} style={{ cursor: 'help' }} />
            </div>
          </div>
          <div style={{ fontSize: '11px', fontFamily: '"IBM Plex Mono", monospace', color: BLOOMBERG.ORANGE, fontWeight: 600 }}>
            {fees.takerPercent}
          </div>
        </div>
      </div>

      {/* Example Calculation */}
      <div style={{
        marginTop: '12px',
        paddingTop: '12px',
        borderTop: `1px solid ${BLOOMBERG.BORDER}`
      }}>
        <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '8px' }}>EXAMPLE: $10,000 TRADE</div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', fontSize: '10px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <span style={{ color: BLOOMBERG.GRAY }}>Maker:</span>
            <span style={{ color: BLOOMBERG.GREEN, fontFamily: '"IBM Plex Mono", monospace', fontWeight: 600 }}>
              ${(10000 * fees.maker).toFixed(2)}
            </span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <span style={{ color: BLOOMBERG.GRAY }}>Taker:</span>
            <span style={{ color: BLOOMBERG.ORANGE, fontFamily: '"IBM Plex Mono", monospace', fontWeight: 600 }}>
              ${(10000 * fees.taker).toFixed(2)}
            </span>
          </div>
        </div>
      </div>

      {/* Savings Tip */}
      <div style={{
        marginTop: '12px',
        padding: '8px',
        background: `${BLOOMBERG.BLUE}15`,
        border: `1px solid ${BLOOMBERG.BLUE}40`,
        borderRadius: '3px'
      }}>
        <div style={{ fontSize: '9px', color: BLOOMBERG.CYAN }}>
          💡 TIP: Use limit orders to pay lower maker fees
        </div>
      </div>
    </div>
  );
}
