import React from 'react';
import { X, ArrowUpRight, ArrowDownRight } from 'lucide-react';
import { FINCEPT } from '../finceptStyles';
import { formatCurrency } from '../portfolio/utils';
import { useTranslation } from 'react-i18next';
import type { HoldingWithQuote } from '../../../../services/portfolio/portfolioService';

interface OrderPanelProps {
  show: boolean;
  onClose: () => void;
  orderSide: 'BUY' | 'SELL';
  onSetOrderSide: (side: 'BUY' | 'SELL') => void;
  selectedHolding: HoldingWithQuote | null;
  onSubmitBuy: () => void;
  onSubmitSell: () => void;
  currency: string;
}

const OrderPanel: React.FC<OrderPanelProps> = ({
  show, onClose, orderSide, onSetOrderSide, selectedHolding, onSubmitBuy, onSubmitSell, currency,
}) => {
  const { t } = useTranslation('portfolio');
  if (!show) return null;

  const isBuy = orderSide === 'BUY';
  const sideColor = isBuy ? FINCEPT.GREEN : FINCEPT.RED;

  return (
    <div style={{
      width: '200px', flexShrink: 0,
      backgroundColor: '#050505',
      borderLeft: `1px solid ${sideColor}`,
      display: 'flex', flexDirection: 'column',
      animation: 'fadeIn 0.15s ease',
    }}>
      {/* Header */}
      <div style={{
        padding: '6px 10px', backgroundColor: '#0A0A0A',
        borderBottom: `2px solid ${sideColor}`,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      }}>
        <span style={{ fontSize: '10px', fontWeight: 800, color: sideColor }}>{t('order.orderEntry', 'ORDER ENTRY')}</span>
        <button onClick={onClose} style={{
          background: 'none', border: 'none', color: FINCEPT.GRAY, cursor: 'pointer',
          display: 'flex', padding: '2px',
        }}>
          <X size={10} />
        </button>
      </div>

      {/* Side Toggle */}
      <div style={{ display: 'flex', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {(['BUY', 'SELL'] as const).map(side => (
          <button
            key={side}
            onClick={() => onSetOrderSide(side)}
            style={{
              flex: 1, padding: '8px',
              backgroundColor: orderSide === side ? (side === 'BUY' ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`) : 'transparent',
              borderBottom: orderSide === side ? `2px solid ${side === 'BUY' ? FINCEPT.GREEN : FINCEPT.RED}` : '2px solid transparent',
              color: orderSide === side ? (side === 'BUY' ? FINCEPT.GREEN : FINCEPT.RED) : FINCEPT.GRAY,
              fontSize: '10px', fontWeight: 800, cursor: 'pointer',
              border: 'none', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
            }}
          >
            {side === 'BUY' ? <ArrowUpRight size={10} /> : <ArrowDownRight size={10} />}
            {side}
          </button>
        ))}
      </div>

      {/* Selected Holding Info */}
      {selectedHolding && (
        <div style={{ padding: '8px 10px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: '12px', fontWeight: 800, color: FINCEPT.CYAN, marginBottom: '4px' }}>
            {selectedHolding.symbol}
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', marginBottom: '2px' }}>
            <span style={{ color: FINCEPT.GRAY }}>{t('order.price', 'PRICE')}</span>
            <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{formatCurrency(selectedHolding.current_price, currency)}</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', marginBottom: '2px' }}>
            <span style={{ color: FINCEPT.GRAY }}>{t('order.qtyHeld', 'QTY HELD')}</span>
            <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{selectedHolding.quantity}</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px' }}>
            <span style={{ color: FINCEPT.GRAY }}>{t('order.mktVal', 'MKT VAL')}</span>
            <span style={{ color: FINCEPT.YELLOW, fontWeight: 600 }}>{formatCurrency(selectedHolding.market_value, currency)}</span>
          </div>
        </div>
      )}

      {/* Quick Action Button */}
      <div style={{ padding: '10px' }}>
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px', textAlign: 'center' }}>
          {selectedHolding
            ? `${isBuy ? t('order.clickToBuyMore', 'Click to buy more') : t('order.clickToSell', 'Click to sell')} ${selectedHolding.symbol}`
            : isBuy ? t('order.clickToBuy', 'Click to buy an asset') : t('order.clickToSellAsset', 'Click to sell an asset')
          }
        </div>
        <button
          onClick={isBuy ? onSubmitBuy : onSubmitSell}
          style={{
            width: '100%', padding: '10px',
            backgroundColor: `${sideColor}20`,
            border: `1px solid ${sideColor}`,
            color: sideColor,
            fontSize: '11px', fontWeight: 800, cursor: 'pointer',
            display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
          }}
        >
          {isBuy ? <ArrowUpRight size={12} /> : <ArrowDownRight size={12} />}
          {isBuy ? t('order.openBuyOrder', 'OPEN BUY ORDER') : t('order.openSellOrder', 'OPEN SELL ORDER')}
        </button>
      </div>

      {/* Spacer */}
      <div style={{ flex: 1 }} />

      {/* Footer info */}
      <div style={{ padding: '6px 10px', borderTop: `1px solid ${FINCEPT.BORDER}`, fontSize: '8px', color: FINCEPT.GRAY, textAlign: 'center' }}>
        {t('order.ordersRecorded', 'Orders are recorded in your portfolio')}
      </div>
    </div>
  );
};

export default OrderPanel;
