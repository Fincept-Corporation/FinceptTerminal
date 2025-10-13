// File: src/components/payment/PaymentProcessingScreen.tsx
// Payment processing screen that handles the waiting and verification process

import React, { useState, useEffect, useRef } from 'react';
import { Button } from "@/components/ui/button";
import {
  CheckCircle,
  XCircle,
  Clock,
  RefreshCw,
  AlertTriangle,
  CreditCard,
  Shield,
  Loader2
} from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';
import { PaymentApiService, PaymentUtils } from '@/services/paymentApi';

interface PaymentProcessingScreenProps {
  onNavigate: (screen: Screen) => void;
  onPaymentComplete: () => void;
}

type PaymentStatus = 'processing' | 'waiting' | 'completed' | 'failed' | 'cancelled' | 'timeout';

const PaymentProcessingScreen: React.FC<PaymentProcessingScreenProps> = ({
  onNavigate,
  onPaymentComplete
}) => {
  const { session, refreshUserData } = useAuth();
  const [status, setStatus] = useState<PaymentStatus>('processing');
  const [checkCount, setCheckCount] = useState(0);
  const [timeElapsed, setTimeElapsed] = useState(0);
  const [lastChecked, setLastChecked] = useState<Date>(new Date());
  const [paymentData, setPaymentData] = useState<any>(null);
  const [error, setError] = useState<string>('');

  const intervalRef = useRef<NodeJS.Timeout | undefined>(undefined);
  const timeoutRef = useRef<NodeJS.Timeout | undefined>(undefined);
  const maxChecks = 60; // Check for 5 minutes (every 5 seconds)
  const checkInterval = 5000; // 5 seconds
  const maxWaitTime = 300000; // 5 minutes

  // Check payment status
  const checkPaymentStatus = async () => {
    if (!session?.api_key) {
      setError('Authentication required');
      setStatus('failed');
      return;
    }

    try {
      console.log(`Payment status check #${checkCount + 1}`);

      // Get updated user subscription to see if payment completed
      const subscriptionResult = await PaymentApiService.getUserSubscription(session.api_key);

      if (subscriptionResult.success && subscriptionResult.data?.has_subscription) {
        console.log('Payment completed - user now has active subscription');
        setStatus('completed');
        setPaymentData(subscriptionResult.data);

        // Refresh user data in context
        await refreshUserData();

        // Clear intervals
        if (intervalRef.current) clearInterval(intervalRef.current);
        if (timeoutRef.current) clearTimeout(timeoutRef.current);

        return;
      }

      setCheckCount(prev => prev + 1);
      setLastChecked(new Date());

      // If we've checked too many times, timeout
      if (checkCount >= maxChecks) {
        console.log('Payment check timeout reached');
        setStatus('timeout');
        if (intervalRef.current) clearInterval(intervalRef.current);
        if (timeoutRef.current) clearTimeout(timeoutRef.current);
      }

    } catch (error) {
      console.error('Payment status check failed:', error);
      setError('Failed to check payment status');
    }
  };

  // Start checking payment status
  useEffect(() => {
    console.log('Starting payment status monitoring...');
    setStatus('waiting');

    // Initial delay before first check
    const initialDelay = setTimeout(() => {
      checkPaymentStatus();

      // Set up interval for subsequent checks
      intervalRef.current = setInterval(checkPaymentStatus, checkInterval);
    }, 3000);

    // Set overall timeout
    timeoutRef.current = setTimeout(() => {
      console.log('Payment processing timeout');
      setStatus('timeout');
      if (intervalRef.current) clearInterval(intervalRef.current);
    }, maxWaitTime);

    // Track elapsed time
    const timeTracker = setInterval(() => {
      setTimeElapsed(prev => prev + 1);
    }, 1000);

    return () => {
      clearTimeout(initialDelay);
      if (intervalRef.current) clearInterval(intervalRef.current);
      if (timeoutRef.current) clearTimeout(timeoutRef.current);
      clearInterval(timeTracker);
    };
  }, []);

  const handleManualRefresh = async () => {
    setError('');
    setStatus('processing');
    await checkPaymentStatus();
  };

  const handleContinueToDashboard = () => {
    onPaymentComplete();
  };

  const handleBackToPricing = () => {
    onNavigate('pricing');
  };

  const formatTimeElapsed = (seconds: number): string => {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  };

  const getStatusDisplay = () => {
    switch (status) {
      case 'processing':
        return {
          icon: <Loader2 className="w-8 h-8 animate-spin text-blue-500" />,
          title: 'Initializing Payment Check',
          subtitle: 'Setting up payment verification...',
          color: 'text-blue-500'
        };
      case 'waiting':
        return {
          icon: <Clock className="w-8 h-8 text-yellow-500 animate-pulse" />,
          title: 'Waiting for Payment Confirmation',
          subtitle: 'Please complete your payment in the browser window that opened.',
          color: 'text-yellow-500'
        };
      case 'completed':
        return {
          icon: <CheckCircle className="w-8 h-8 text-green-500" />,
          title: 'Payment Successful!',
          subtitle: 'Your subscription has been activated successfully.',
          color: 'text-green-500'
        };
      case 'failed':
        return {
          icon: <XCircle className="w-8 h-8 text-red-500" />,
          title: 'Payment Failed',
          subtitle: 'There was an issue processing your payment.',
          color: 'text-red-500'
        };
      case 'cancelled':
        return {
          icon: <XCircle className="w-8 h-8 text-orange-500" />,
          title: 'Payment Cancelled',
          subtitle: 'The payment was cancelled or not completed.',
          color: 'text-orange-500'
        };
      case 'timeout':
        return {
          icon: <AlertTriangle className="w-8 h-8 text-orange-500" />,
          title: 'Payment Verification Timeout',
          subtitle: 'We\'re still waiting for payment confirmation. This may take a few more minutes.',
          color: 'text-orange-500'
        };
      default:
        return {
          icon: <Clock className="w-8 h-8 text-gray-500" />,
          title: 'Processing...',
          subtitle: 'Please wait...',
          color: 'text-gray-500'
        };
    }
  };

  const statusDisplay = getStatusDisplay();

  return (
    <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-8 w-full max-w-md mx-auto shadow-2xl">
      <div className="text-center">
        {/* Status Icon */}
        <div className="mb-6">
          {statusDisplay.icon}
        </div>

        {/* Status Title */}
        <h2 className={`text-xl font-semibold mb-2 ${statusDisplay.color}`}>
          {statusDisplay.title}
        </h2>

        {/* Status Subtitle */}
        <p className="text-zinc-400 text-sm mb-6">
          {statusDisplay.subtitle}
        </p>

        {/* Payment Status Details */}
        {status === 'waiting' && (
          <div className="bg-zinc-800/50 rounded-lg p-4 mb-6 space-y-3">
            <div className="flex items-center justify-center space-x-2 text-zinc-300">
              <CreditCard className="w-4 h-4" />
              <span className="text-sm">Secure Payment Processing</span>
            </div>

            <div className="text-xs text-zinc-500 space-y-1">
              <div>Time elapsed: {formatTimeElapsed(timeElapsed)}</div>
              <div>Status checks: {checkCount}/{maxChecks}</div>
              <div>Last checked: {lastChecked.toLocaleTimeString()}</div>
            </div>

            <div className="flex items-center justify-center space-x-1 text-zinc-400">
              <Shield className="w-3 h-3" />
              <span className="text-xs">Encrypted & Secure</span>
            </div>
          </div>
        )}

        {/* Success Details */}
        {status === 'completed' && paymentData && (
          <div className="bg-green-900/20 border border-green-900/50 rounded-lg p-4 mb-6">
            <div className="text-sm text-green-300 space-y-2">
              <div className="font-medium">
                {paymentData.subscription?.plan?.name} Plan Activated
              </div>
              <div className="text-xs text-green-400">
                {PaymentUtils.formatCurrency(paymentData.subscription?.plan?.price || 0)}/month
              </div>
              <div className="text-xs text-zinc-400">
                {paymentData.subscription?.days_remaining} days remaining in current period
              </div>
            </div>
          </div>
        )}

        {/* Error Display */}
        {error && (
          <div className="bg-red-900/20 border border-red-900/50 rounded-lg p-3 mb-6">
            <div className="flex items-center text-red-400 text-sm">
              <AlertTriangle className="w-4 h-4 mr-2 flex-shrink-0" />
              {error}
            </div>
          </div>
        )}

        {/* Action Buttons */}
        <div className="space-y-3">
          {status === 'completed' && (
            <Button
              onClick={handleContinueToDashboard}
              className="w-full bg-green-600 hover:bg-green-500 text-white font-medium"
            >
              Continue to Dashboard
            </Button>
          )}

          {(status === 'waiting' || status === 'processing') && (
            <div className="space-y-2">
              <Button
                onClick={handleManualRefresh}
                variant="outline"
                className="w-full bg-zinc-800 border-zinc-600 text-white hover:bg-zinc-700 hover:border-zinc-500"
              >
                <RefreshCw className="w-4 h-4 mr-2" />
                Check Status Now
              </Button>
              <Button
                onClick={handleBackToPricing}
                variant="outline"
                className="w-full bg-zinc-800 border-zinc-600 text-white hover:bg-zinc-700 hover:border-zinc-500"
              >
                Back to Plans
              </Button>
            </div>
          )}

          {(status === 'failed' || status === 'cancelled' || status === 'timeout') && (
            <div className="space-y-2">
              <Button
                onClick={handleManualRefresh}
                className="w-full bg-blue-600 hover:bg-blue-500 text-white font-medium"
              >
                <RefreshCw className="w-4 h-4 mr-2" />
                Check Again
              </Button>
              <Button
                onClick={handleBackToPricing}
                variant="outline"
                className="w-full bg-zinc-800 border-zinc-600 text-white hover:bg-zinc-700 hover:border-zinc-500"
              >
                Back to Plans
              </Button>
            </div>
          )}
        </div>

        {/* Help Text */}
        <div className="mt-6 pt-4 border-t border-zinc-700/50">
          <div className="text-xs text-zinc-500 space-y-1">
            {status === 'waiting' && (
              <>
                <p>Complete your payment in the opened browser window.</p>
                <p>This page will automatically update when payment is confirmed.</p>
              </>
            )}
            {status === 'timeout' && (
              <>
                <p>Payment processing may take up to 10 minutes.</p>
                <p>You can safely close this window and check your account later.</p>
              </>
            )}
            {(status === 'failed' || status === 'cancelled') && (
              <>
                <p>If you completed the payment, please wait a few minutes and check again.</p>
                <p>Contact support if issues persist.</p>
              </>
            )}
          </div>
        </div>

        {/* Progress Indicator for waiting status */}
        {status === 'waiting' && (
          <div className="mt-4">
            <div className="w-full bg-zinc-700 rounded-full h-1">
              <div
                className="bg-blue-500 h-1 rounded-full transition-all duration-300"
                style={{
                  width: `${Math.min((checkCount / maxChecks) * 100, 100)}%`
                }}
              />
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default PaymentProcessingScreen;