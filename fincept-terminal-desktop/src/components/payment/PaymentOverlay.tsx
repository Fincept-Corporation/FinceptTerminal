import React from 'react';
import InAppPaymentWindow from './InAppPaymentWindow';

interface PaymentWindowState {
  isOpen: boolean;
  checkoutUrl: string;
  planName: string;
  planPrice: number;
  onSuccess?: () => void;
  onCancel?: () => void;
  onError?: (error: string) => void;
}

const PaymentOverlay: React.FC<{ paymentWindow: PaymentWindowState }> = ({ paymentWindow }) => {
  if (!paymentWindow.isOpen) return null;

  return (
    <InAppPaymentWindow
      checkoutUrl={paymentWindow.checkoutUrl}
      planName={paymentWindow.planName}
      planPrice={paymentWindow.planPrice}
      onSuccess={paymentWindow.onSuccess || (() => {})}
      onCancel={paymentWindow.onCancel || (() => {})}
      onError={paymentWindow.onError || (() => {})}
    />
  );
};

export default PaymentOverlay;
