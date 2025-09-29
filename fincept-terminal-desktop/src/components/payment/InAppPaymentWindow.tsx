// File: src/components/payment/InAppPaymentWindow.tsx
// In-app payment window that opens within the Tauri application

import React, { useState, useEffect, useRef } from 'react';
import { Button } from "@/components/ui/button";
import { X, ExternalLink, RefreshCw, AlertCircle, Shield, CreditCard } from "lucide-react";

interface InAppPaymentWindowProps {
  checkoutUrl: string;
  onSuccess: () => void;
  onCancel: () => void;
  onError: (error: string) => void;
  planName: string;
  planPrice: number;
}

const InAppPaymentWindow: React.FC<InAppPaymentWindowProps> = ({
  checkoutUrl,
  onSuccess,
  onCancel,
  onError,
  planName,
  planPrice
}) => {
  const [isLoading, setIsLoading] = useState(true);
  const [hasError, setHasError] = useState(false);
  const [errorMessage, setErrorMessage] = useState('');
  const iframeRef = useRef<HTMLIFrameElement>(null);

  useEffect(() => {
    // Set up message listener for payment completion
    const handleMessage = (event: MessageEvent) => {
      // Only accept messages from trusted payment domains
      const trustedDomains = ['dodopayments.com', 'stripe.com', 'paypal.com'];
      const origin = new URL(event.origin);

      if (!trustedDomains.some(domain => origin.hostname.includes(domain))) {
        return;
      }

      console.log('Payment message received:', event.data);

      if (event.data.type === 'payment_success') {
        console.log('Payment completed successfully');
        onSuccess();
      } else if (event.data.type === 'payment_error') {
        console.log('Payment failed:', event.data.error);
        onError(event.data.error || 'Payment failed');
      } else if (event.data.type === 'payment_cancelled') {
        console.log('Payment cancelled by user');
        onCancel();
      }
    };

    window.addEventListener('message', handleMessage);

    // Set up URL monitoring for payment completion
    const checkPaymentStatus = () => {
      try {
        if (iframeRef.current?.contentWindow) {
          const currentUrl = iframeRef.current.contentWindow.location.href;

          // Check for success/failure URLs
          if (currentUrl.includes('payment-success') || currentUrl.includes('success')) {
            console.log('Payment success detected via URL');
            onSuccess();
          } else if (currentUrl.includes('payment-error') || currentUrl.includes('cancelled')) {
            console.log('Payment cancelled/failed detected via URL');
            onCancel();
          }
        }
      } catch (error) {
        // Cross-origin restrictions prevent URL access - this is normal
        console.log('URL monitoring blocked by CORS (expected)');
      }
    };

    const urlCheckInterval = setInterval(checkPaymentStatus, 2000);

    return () => {
      window.removeEventListener('message', handleMessage);
      clearInterval(urlCheckInterval);
    };
  }, [onSuccess, onError, onCancel]);

  const handleIframeLoad = () => {
    setIsLoading(false);
    setHasError(false);
  };

  const handleIframeError = () => {
    setIsLoading(false);
    setHasError(true);
    setErrorMessage('Failed to load payment page. Please check your internet connection.');
  };

  const handleRetry = () => {
    setIsLoading(true);
    setHasError(false);
    setErrorMessage('');

    if (iframeRef.current) {
      iframeRef.current.src = checkoutUrl;
    }
  };

  const handleOpenInBrowser = () => {
    window.open(checkoutUrl, '_blank');
  };

  const formatCurrency = (amount: number) => {
    return new Intl.NumberFormat('en-US', {
      style: 'currency',
      currency: 'USD'
    }).format(amount);
  };

  return (
    <div className="fixed inset-0 bg-black/80 backdrop-blur-sm z-50 flex items-center justify-center p-4">
      <div className="bg-zinc-900 border border-zinc-700 rounded-xl w-full max-w-4xl h-full max-h-[90vh] flex flex-col shadow-2xl">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b border-zinc-700">
          <div className="flex items-center space-x-3">
            <div className="p-2 bg-blue-600/20 rounded-lg">
              <CreditCard className="w-5 h-5 text-blue-400" />
            </div>
            <div>
              <h2 className="text-white font-semibold">Complete Your Payment</h2>
              <p className="text-zinc-400 text-sm">
                {planName} - {formatCurrency(planPrice)}
              </p>
            </div>
          </div>

          <div className="flex items-center space-x-2">
            <div className="flex items-center space-x-1 text-xs text-zinc-500">
              <Shield className="w-3 h-3" />
              <span>Secure Payment</span>
            </div>

            <Button
              onClick={handleRetry}
              variant="outline"
              size="sm"
              className="border-zinc-600 text-zinc-300 hover:bg-zinc-800"
              disabled={isLoading}
            >
              <RefreshCw className={`w-4 h-4 ${isLoading ? 'animate-spin' : ''}`} />
            </Button>

            <Button
              onClick={handleOpenInBrowser}
              variant="outline"
              size="sm"
              className="border-zinc-600 text-zinc-300 hover:bg-zinc-800"
              title="Open in browser"
            >
              <ExternalLink className="w-4 h-4" />
            </Button>

            <Button
              onClick={onCancel}
              variant="outline"
              size="sm"
              className="border-zinc-600 text-zinc-300 hover:bg-zinc-800"
            >
              <X className="w-4 h-4" />
            </Button>
          </div>
        </div>

        {/* Content Area */}
        <div className="flex-1 relative">
          {/* Loading State */}
          {isLoading && (
            <div className="absolute inset-0 flex items-center justify-center bg-zinc-900">
              <div className="text-center">
                <div className="w-8 h-8 border border-white/30 border-t-white rounded-full animate-spin mx-auto mb-4"></div>
                <h3 className="text-white font-medium mb-2">Loading Payment Page</h3>
                <p className="text-zinc-400 text-sm">Please wait while we prepare your checkout...</p>
              </div>
            </div>
          )}

          {/* Error State */}
          {hasError && (
            <div className="absolute inset-0 flex items-center justify-center bg-zinc-900">
              <div className="text-center max-w-md">
                <AlertCircle className="w-12 h-12 text-red-500 mx-auto mb-4" />
                <h3 className="text-white font-medium mb-2">Payment Page Failed to Load</h3>
                <p className="text-zinc-400 text-sm mb-6">
                  {errorMessage || 'There was an issue loading the payment page. Please try again or open in your browser.'}
                </p>
                <div className="space-y-3">
                  <Button
                    onClick={handleRetry}
                    className="w-full bg-blue-600 hover:bg-blue-500 text-white"
                  >
                    <RefreshCw className="w-4 h-4 mr-2" />
                    Try Again
                  </Button>
                  <Button
                    onClick={handleOpenInBrowser}
                    variant="outline"
                    className="w-full border-zinc-600 text-zinc-300 hover:bg-zinc-800"
                  >
                    <ExternalLink className="w-4 h-4 mr-2" />
                    Open in Browser
                  </Button>
                </div>
              </div>
            </div>
          )}

          {/* Payment Iframe */}
          <iframe
            ref={iframeRef}
            src={checkoutUrl}
            className="w-full h-full border-0"
            onLoad={handleIframeLoad}
            onError={handleIframeError}
            title="Payment Checkout"
            sandbox="allow-same-origin allow-scripts allow-forms allow-top-navigation allow-popups"
            allow="payment; encrypted-media"
          />
        </div>

        {/* Footer */}
        <div className="p-4 border-t border-zinc-700 bg-zinc-900/50">
          <div className="flex items-center justify-between text-xs text-zinc-500">
            <div className="flex items-center space-x-4">
              <div className="flex items-center space-x-1">
                <Shield className="w-3 h-3" />
                <span>SSL Encrypted</span>
              </div>
              <span>•</span>
              <span>PCI Compliant</span>
              <span>•</span>
              <span>Secure Payment Processing</span>
            </div>

            <div className="text-right">
              <div>Need help? Contact support</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default InAppPaymentWindow;