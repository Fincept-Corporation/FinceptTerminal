// Order Confirmation Dialog - Terminal Style
import React from 'react';
import { X, TrendingUp, TrendingDown, AlertTriangle } from 'lucide-react';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
};

const TYPOGRAPHY = {
  MONO: '"IBM Plex Mono", "Consolas", monospace',
};

interface OrderConfirmationDialogProps {
  isOpen: boolean;
  onConfirm: () => void;
  onCancel: () => void;
  orderDetails: {
    side: 'buy' | 'sell';
    type: string;
    symbol: string;
    quantity: number;
    price?: number;
    total: number;
  };
}

export const OrderConfirmationDialog: React.FC<OrderConfirmationDialogProps> = ({
  isOpen,
  onConfirm,
  onCancel,
  orderDetails,
}) => {
  if (!isOpen) return null;

  const { side, type, symbol, quantity, price, total } = orderDetails;
  const isBuy = side === 'buy';

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0,0,0,0.90)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 9999,
        fontFamily: TYPOGRAPHY.MONO,
      }}
      onClick={onCancel}
    >
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `2px solid ${isBuy ? FINCEPT.GREEN : FINCEPT.RED}`,
          borderRadius: '2px',
          padding: '0',
          width: '420px',
          maxWidth: '90vw',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div
          style={{
            backgroundColor: isBuy ? FINCEPT.GREEN : FINCEPT.RED,
            padding: '12px 16px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            {isBuy ? (
              <TrendingUp size={16} style={{ color: FINCEPT.DARK_BG }} />
            ) : (
              <TrendingDown size={16} style={{ color: FINCEPT.DARK_BG }} />
            )}
            <span
              style={{
                fontSize: '12px',
                fontWeight: 700,
                color: FINCEPT.DARK_BG,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
              }}
            >
              Confirm {side} Order
            </span>
          </div>
          <button
            onClick={onCancel}
            style={{
              background: 'transparent',
              border: `1px solid ${FINCEPT.DARK_BG}`,
              color: FINCEPT.DARK_BG,
              cursor: 'pointer',
              padding: '2px 6px',
              fontSize: '9px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              borderRadius: '2px',
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: 700,
            }}
          >
            <X size={10} /> ESC
          </button>
        </div>

        {/* Warning Banner */}
        <div
          style={{
            backgroundColor: `${FINCEPT.ORANGE}15`,
            border: `1px solid ${FINCEPT.ORANGE}`,
            borderLeft: 'none',
            borderRight: 'none',
            padding: '10px 16px',
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
          }}
        >
          <AlertTriangle size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '10px', color: FINCEPT.ORANGE, fontWeight: 600 }}>
            Please review your order carefully before confirming
          </span>
        </div>

        {/* Order Details */}
        <div style={{ padding: '16px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {/* Order Type */}
            <div
              style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                padding: '8px 12px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
              }}
            >
              <span
                style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                }}
              >
                Order Type
              </span>
              <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.CYAN }}>
                {type.toUpperCase()}
              </span>
            </div>

            {/* Symbol */}
            <div
              style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                padding: '8px 12px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
              }}
            >
              <span
                style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                }}
              >
                Symbol
              </span>
              <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
                {symbol}
              </span>
            </div>

            {/* Quantity */}
            <div
              style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                padding: '8px 12px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
              }}
            >
              <span
                style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                }}
              >
                Quantity
              </span>
              <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
                {quantity}
              </span>
            </div>

            {/* Price (if limit order) */}
            {price && (
              <div
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  padding: '8px 12px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                }}
              >
                <span
                  style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                  }}
                >
                  Price
                </span>
                <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
                  ${price.toFixed(2)}
                </span>
              </div>
            )}

            {/* Total */}
            <div
              style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                padding: '10px 12px',
                backgroundColor: isBuy ? `${FINCEPT.GREEN}15` : `${FINCEPT.RED}15`,
                border: `2px solid ${isBuy ? FINCEPT.GREEN : FINCEPT.RED}`,
                borderRadius: '2px',
              }}
            >
              <span
                style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.WHITE,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                }}
              >
                Total
              </span>
              <span
                style={{
                  fontSize: '14px',
                  fontWeight: 700,
                  color: isBuy ? FINCEPT.GREEN : FINCEPT.RED,
                }}
              >
                ${total.toFixed(2)}
              </span>
            </div>
          </div>
        </div>

        {/* Action Buttons */}
        <div
          style={{
            display: 'flex',
            gap: '8px',
            padding: '16px',
            paddingTop: '0',
          }}
        >
          <button
            onClick={onCancel}
            style={{
              flex: 1,
              padding: '10px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.WHITE,
              cursor: 'pointer',
              fontFamily: TYPOGRAPHY.MONO,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              e.currentTarget.style.borderColor = FINCEPT.GRAY;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
            }}
          >
            Cancel
          </button>
          <button
            onClick={onConfirm}
            style={{
              flex: 1,
              padding: '10px',
              backgroundColor: isBuy ? FINCEPT.GREEN : FINCEPT.RED,
              border: 'none',
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.DARK_BG,
              cursor: 'pointer',
              fontFamily: TYPOGRAPHY.MONO,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.8';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}
          >
            Confirm {side}
          </button>
        </div>
      </div>
    </div>
  );
};

export default OrderConfirmationDialog;
