// File: src/components/payment/PaymentSuccessScreen.tsx
// Payment success screen that confirms completed payment and activates subscription

import React, { useState, useEffect } from 'react';
import { Button } from "@/components/ui/button";
import {
  CheckCircle,
  Download,
  Mail,
  Calendar,
  CreditCard,
  ArrowRight,
  Star,
  Shield,
  Zap,
  AlertCircle,
  Loader2
} from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';
import { PaymentApiService, PaymentUtils } from '@/services/paymentApi';

interface PaymentSuccessScreenProps {
  onNavigate: (screen: Screen) => void;
  onComplete: () => void;
}

const PaymentSuccessScreen: React.FC<PaymentSuccessScreenProps> = ({
  onNavigate,
  onComplete
}) => {
  const { session, refreshUserData } = useAuth();
  const [isLoading, setIsLoading] = useState(true);
  const [paymentData, setPaymentData] = useState<any>(null);
  const [subscriptionData, setSubscriptionData] = useState<any>(null);
  const [error, setError] = useState<string>('');

  // Extract payment parameters from URL
  useEffect(() => {
    const urlParams = new URLSearchParams(window.location.search);
    const sessionId = urlParams.get('session_id');
    const paymentId = urlParams.get('payment_id');

    console.log('Payment success page loaded with params:', { sessionId, paymentId });

    const handlePaymentSuccess = async () => {
      if (!session?.api_key) {
        setError('Authentication required');
        setIsLoading(false);
        return;
      }

      try {
        // Handle payment success with API
        const paymentResult = await PaymentApiService.handlePaymentSuccess(
          session.api_key,
          sessionId || undefined,
          paymentId || undefined
        );

        if (paymentResult.success && paymentResult.data) {
          console.log('Payment success confirmed:', paymentResult.data);
          setPaymentData(paymentResult.data);
        } else {
          console.log('Payment success API call failed, proceeding with subscription check');
        }

        // Get updated subscription data
        const subscriptionResult = await PaymentApiService.getUserSubscription(session.api_key);

        if (subscriptionResult.success && subscriptionResult.data) {
          console.log('Subscription data retrieved:', subscriptionResult.data);
          setSubscriptionData(subscriptionResult.data);

          // Refresh user data in context
          await refreshUserData();
        } else {
          throw new Error('Failed to retrieve subscription data');
        }

      } catch (error) {
        console.error('Payment success handling failed:', error);
        setError('Failed to confirm payment. Please contact support if you were charged.');
      } finally {
        setIsLoading(false);
      }
    };

    handlePaymentSuccess();
  }, [session?.api_key, refreshUserData]);

  const handleContinueToDashboard = () => {
    onComplete();
  };

  const handleDownloadReceipt = () => {
    if (paymentData) {
      const receiptData = PaymentUtils.generateReceiptData(paymentData);

      // Create receipt content
      const receiptContent = `
FINCEPT API - PAYMENT RECEIPT
============================

Receipt ID: ${receiptData.receiptId}
Date: ${receiptData.date}
Description: ${receiptData.description}
Amount: ${receiptData.amount}
Status: ${receiptData.status}

Thank you for your subscription!
Visit: https://fincept.in
Support: support@fincept.in
      `.trim();

      // Download as text file
      const blob = new Blob([receiptContent], { type: 'text/plain' });
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `fincept-receipt-${receiptData.receiptId}.txt`;
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      URL.revokeObjectURL(url);
    }
  };

  if (isLoading) {
    return (
      <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-8 w-full max-w-md mx-auto shadow-2xl">
        <div className="text-center">
          <Loader2 className="w-8 h-8 animate-spin text-blue-500 mx-auto mb-4" />
          <h2 className="text-white text-xl font-semibold mb-2">Confirming Payment</h2>
          <p className="text-zinc-400 text-sm">Activating your subscription...</p>
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-8 w-full max-w-md mx-auto shadow-2xl">
        <div className="text-center">
          <AlertCircle className="w-8 h-8 text-red-500 mx-auto mb-4" />
          <h2 className="text-white text-xl font-semibold mb-2">Payment Confirmation Error</h2>
          <p className="text-zinc-400 text-sm mb-6">{error}</p>
          <div className="space-y-2">
            <Button
              onClick={handleContinueToDashboard}
              className="w-full bg-blue-600 hover:bg-blue-500 text-white"
            >
              Continue to Dashboard
            </Button>
            <p className="text-xs text-zinc-500">
              Check your account dashboard for subscription status
            </p>
          </div>
        </div>
      </div>
    );
  }

  const subscription = subscriptionData?.subscription;
  const planName = subscription?.plan?.name || 'Subscription';
  const planPrice = subscription?.plan?.price || 0;
  const daysRemaining = subscription?.days_remaining || 0;

  return (
    <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-8 w-full max-w-lg mx-auto shadow-2xl">
      <div className="text-center">
        {/* Success Icon */}
        <div className="mb-6">
          <CheckCircle className="w-12 h-12 text-green-500 mx-auto" />
        </div>

        {/* Success Title */}
        <h2 className="text-2xl font-bold text-white mb-2">
          Payment Successful!
        </h2>

        <p className="text-zinc-400 text-sm mb-6">
          Your subscription has been activated and you now have access to all premium features.
        </p>

        {/* Subscription Details */}
        <div className="bg-gradient-to-r from-green-900/20 to-blue-900/20 border border-green-900/50 rounded-lg p-6 mb-6">
          <div className="text-center space-y-3">
            <div className="flex items-center justify-center space-x-2">
              <Star className="w-5 h-5 text-yellow-500" />
              <h3 className="text-lg font-semibold text-white">{planName} Plan</h3>
            </div>

            <div className="text-2xl font-bold text-green-400">
              {PaymentUtils.formatCurrency(planPrice)}/month
            </div>

            <div className="grid grid-cols-2 gap-4 text-sm">
              <div className="text-center">
                <div className="text-zinc-400">API Calls</div>
                <div className="text-white font-medium">
                  {subscription?.usage?.api_calls_limit === 'unlimited'
                    ? 'Unlimited'
                    : `${subscription?.usage?.api_calls_limit?.toLocaleString()}/month`
                  }
                </div>
              </div>
              <div className="text-center">
                <div className="text-zinc-400">Billing Period</div>
                <div className="text-white font-medium">{daysRemaining} days</div>
              </div>
            </div>
          </div>
        </div>

        {/* Features Activated */}
        <div className="bg-zinc-800/50 rounded-lg p-4 mb-6">
          <h4 className="text-white font-medium mb-3 text-sm">Features Activated</h4>
          <div className="grid grid-cols-2 gap-3 text-xs">
            <div className="flex items-center space-x-2">
              <Zap className="w-3 h-3 text-blue-400" />
              <span className="text-zinc-300">Real-time Data</span>
            </div>
            <div className="flex items-center space-x-2">
              <Shield className="w-3 h-3 text-green-400" />
              <span className="text-zinc-300">Priority Support</span>
            </div>
            <div className="flex items-center space-x-2">
              <Calendar className="w-3 h-3 text-purple-400" />
              <span className="text-zinc-300">Advanced Analytics</span>
            </div>
            <div className="flex items-center space-x-2">
              <CreditCard className="w-3 h-3 text-yellow-400" />
              <span className="text-zinc-300">Custom Dashboards</span>
            </div>
          </div>
        </div>

        {/* Action Buttons */}
        <div className="space-y-3">
          <Button
            onClick={handleContinueToDashboard}
            className="w-full bg-green-600 hover:bg-green-500 text-white font-medium"
          >
            <ArrowRight className="w-4 h-4 mr-2" />
            Start Using Fincept API
          </Button>

          <div className="flex space-x-2">
            <Button
              onClick={handleDownloadReceipt}
              variant="outline"
              className="flex-1 border-zinc-600 text-zinc-300 hover:bg-zinc-800"
              disabled={!paymentData}
            >
              <Download className="w-4 h-4 mr-2" />
              Receipt
            </Button>

            <Button
              onClick={() => {
                // Email support (could integrate with email service)
                window.open('mailto:support@fincept.in?subject=New Subscription - Thank You', '_blank');
              }}
              variant="outline"
              className="flex-1 border-zinc-600 text-zinc-300 hover:bg-zinc-800"
            >
              <Mail className="w-4 h-4 mr-2" />
              Support
            </Button>
          </div>
        </div>

        {/* Next Steps */}
        <div className="mt-6 pt-4 border-t border-zinc-700/50">
          <h4 className="text-white font-medium mb-2 text-sm">What's Next?</h4>
          <div className="text-xs text-zinc-400 space-y-1 text-left">
            <div className="flex items-start space-x-2">
              <div className="w-1 h-1 bg-blue-400 rounded-full mt-2 flex-shrink-0" />
              <span>Explore your new dashboard with real-time market data</span>
            </div>
            <div className="flex items-start space-x-2">
              <div className="w-1 h-1 bg-blue-400 rounded-full mt-2 flex-shrink-0" />
              <span>Set up custom alerts and watchlists</span>
            </div>
            <div className="flex items-start space-x-2">
              <div className="w-1 h-1 bg-blue-400 rounded-full mt-2 flex-shrink-0" />
              <span>Access premium API endpoints</span>
            </div>
            <div className="flex items-start space-x-2">
              <div className="w-1 h-1 bg-blue-400 rounded-full mt-2 flex-shrink-0" />
              <span>Get priority support when you need help</span>
            </div>
          </div>
        </div>

        {/* Payment Details */}
        {paymentData && (
          <div className="mt-4 pt-3 border-t border-zinc-700/50">
            <div className="text-xs text-zinc-500 space-y-1">
              <div>Payment ID: {paymentData.payment_uuid}</div>
              <div>Amount: {PaymentUtils.formatCurrency(paymentData.amount || planPrice)}</div>
              <div>Date: {new Date().toLocaleDateString()}</div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default PaymentSuccessScreen;