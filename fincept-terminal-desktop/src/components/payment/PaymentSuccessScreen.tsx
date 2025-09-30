// File: src/components/payment/PaymentSuccessScreen.tsx
// Payment success screen with confetti celebration and auto-redirect

import React, { useState, useEffect } from 'react';
import Confetti from 'react-confetti';
import { Button } from "@/components/ui/button";
import {
  CheckCircle,
  Download,
  ArrowRight,
  Loader2,
  AlertCircle,
  Sparkles
} from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';
import { PaymentApiService } from '@/services/paymentApi';

interface PaymentSuccessScreenProps {
  onNavigate: (screen: Screen) => void;
  onComplete: () => void;
}

const PaymentSuccessScreen: React.FC<PaymentSuccessScreenProps> = ({
  onComplete
}) => {
  const { session, refreshUserData } = useAuth();
  const [isLoading, setIsLoading] = useState(true);
  const [paymentData, setPaymentData] = useState<any>(null);
  const [subscriptionData, setSubscriptionData] = useState<any>(null);
  const [error, setError] = useState<string>('');
  const [countdown, setCountdown] = useState(5);
  const [showConfetti, setShowConfetti] = useState(true);
  const [windowSize, setWindowSize] = useState({ width: window.innerWidth, height: window.innerHeight });

  // Handle window resize for confetti
  useEffect(() => {
    const handleResize = () => {
      setWindowSize({ width: window.innerWidth, height: window.innerHeight });
    };
    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  // Stop confetti after 5 seconds
  useEffect(() => {
    const timer = setTimeout(() => {
      setShowConfetti(false);
    }, 5000);
    return () => clearTimeout(timer);
  }, []);

  // Extract payment parameters from URL and process payment - RUN ONLY ONCE
  useEffect(() => {
    const urlParams = new URLSearchParams(window.location.search);
    const sessionId = urlParams.get('session_id');
    const paymentId = urlParams.get('payment_id');

    console.log('Payment success page loaded with params:', { sessionId, paymentId });

    // Clear URL parameters immediately to prevent re-processing on refresh
    if (sessionId || paymentId) {
      window.history.replaceState({}, document.title, window.location.pathname);
    }

    const handlePaymentSuccess = async () => {
      if (!session?.api_key) {
        console.log('Waiting for session to load...');
        return; // Will retry when session becomes available
      }

      try {
        console.log('Processing payment success...');

        // Handle payment success with API (optional - backend may have already processed via webhook)
        if (sessionId || paymentId) {
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
        }

        // Get updated subscription data
        const subscriptionResult = await PaymentApiService.getUserSubscription(session.api_key);

        if (subscriptionResult.success && subscriptionResult.data) {
          console.log('Subscription data retrieved:', subscriptionResult.data);
          setSubscriptionData(subscriptionResult.data);

          // Refresh user data in context
          await refreshUserData();

          console.log('Payment processing complete');
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
  }, [session?.api_key]); // Only depend on api_key, run when it becomes available

  // Countdown and auto-redirect
  useEffect(() => {
    if (!isLoading && !error && countdown > 0) {
      const timer = setTimeout(() => {
        setCountdown(countdown - 1);
      }, 1000);
      return () => clearTimeout(timer);
    } else if (countdown === 0) {
      onComplete();
    }
  }, [isLoading, error, countdown, onComplete]);

  const handleContinueToDashboard = () => {
    onComplete();
  };

  const handleDownloadReceipt = () => {
    if (paymentData) {
      const receiptContent = `
FINCEPT API - PAYMENT RECEIPT
============================

Receipt ID: ${paymentData.payment_uuid || 'N/A'}
Date: ${new Date().toLocaleDateString()}
Description: ${subscriptionData?.subscription?.plan?.name || 'Subscription'} Plan
Amount: $${paymentData.amount || subscriptionData?.subscription?.plan?.price || 0}
Status: Completed

Thank you for your subscription!
Visit: https://fincept.in
Support: support@fincept.in
      `.trim();

      const blob = new Blob([receiptContent], { type: 'text/plain' });
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `fincept-receipt-${Date.now()}.txt`;
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      URL.revokeObjectURL(url);
    }
  };

  if (isLoading) {
    return (
      <div className="flex items-center justify-center min-h-[400px]">
        <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-8 w-full max-w-md mx-auto shadow-2xl">
          <div className="text-center">
            <Loader2 className="w-12 h-12 animate-spin text-blue-500 mx-auto mb-4" />
            <h2 className="text-white text-xl font-semibold mb-2">Confirming Payment</h2>
            <p className="text-zinc-400 text-sm">Activating your subscription...</p>
          </div>
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div className="flex items-center justify-center min-h-[400px]">
        <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-8 w-full max-w-md mx-auto shadow-2xl">
          <div className="text-center">
            <AlertCircle className="w-12 h-12 text-red-500 mx-auto mb-4" />
            <h2 className="text-white text-xl font-semibold mb-2">Payment Confirmation Error</h2>
            <p className="text-zinc-400 text-sm mb-6">{error}</p>
            <Button
              onClick={handleContinueToDashboard}
              className="w-full bg-blue-600 hover:bg-blue-500 text-white"
            >
              Continue to Dashboard
            </Button>
            <p className="text-xs text-zinc-500 mt-3">
              Check your account dashboard for subscription status
            </p>
          </div>
        </div>
      </div>
    );
  }

  const subscription = subscriptionData?.subscription;
  const planName = subscription?.plan?.name || 'Premium';
  const planPrice = subscription?.plan?.price || 0;

  return (
    <>
      {/* Confetti Animation */}
      {showConfetti && (
        <Confetti
          width={windowSize.width}
          height={windowSize.height}
          recycle={false}
          numberOfPieces={500}
          gravity={0.3}
        />
      )}

      <div className="flex items-center justify-center min-h-[400px] p-4">
        <div className="bg-gradient-to-br from-zinc-900/95 to-zinc-800/95 backdrop-blur-sm border border-green-500/30 rounded-xl p-6 sm:p-8 w-full max-w-lg mx-auto shadow-2xl">

          {/* Success Icon with Animation */}
          <div className="text-center mb-6">
            <div className="relative inline-block">
              <CheckCircle className="w-16 h-16 sm:w-20 sm:h-20 text-green-500 mx-auto animate-bounce" />
              <Sparkles className="w-6 h-6 text-yellow-400 absolute -top-2 -right-2 animate-pulse" />
            </div>
          </div>

          {/* Success Title */}
          <h2 className="text-2xl sm:text-3xl font-bold text-white mb-2 text-center">
            Payment Successful!
          </h2>

          <p className="text-zinc-300 text-sm sm:text-base mb-6 text-center">
            🎉 Your subscription is now active!
          </p>

          {/* Plan Details Card */}
          <div className="bg-gradient-to-r from-green-900/30 to-blue-900/30 border border-green-500/50 rounded-lg p-4 sm:p-5 mb-6">
            <div className="text-center space-y-2">
              <h3 className="text-lg sm:text-xl font-semibold text-white">{planName} Plan</h3>
              <div className="text-3xl sm:text-4xl font-bold text-green-400">
                ${planPrice}<span className="text-lg text-zinc-400">/month</span>
              </div>

              <div className="flex flex-wrap justify-center gap-2 mt-3 text-xs sm:text-sm">
                <span className="bg-green-500/20 text-green-300 px-3 py-1 rounded-full">
                  ✓ Unlimited API Calls
                </span>
                <span className="bg-blue-500/20 text-blue-300 px-3 py-1 rounded-full">
                  ✓ Priority Support
                </span>
                <span className="bg-purple-500/20 text-purple-300 px-3 py-1 rounded-full">
                  ✓ Premium Features
                </span>
              </div>
            </div>
          </div>

          {/* Auto-redirect countdown */}
          <div className="text-center mb-6">
            <p className="text-zinc-400 text-sm">
              Redirecting to dashboard in <span className="text-green-400 font-bold">{countdown}s</span>
            </p>
          </div>

          {/* Action Buttons */}
          <div className="space-y-3">
            <Button
              onClick={handleContinueToDashboard}
              className="w-full bg-green-600 hover:bg-green-500 text-white font-semibold py-3 text-base"
            >
              <ArrowRight className="w-5 h-5 mr-2" />
              Continue to Dashboard Now
            </Button>

            <Button
              onClick={handleDownloadReceipt}
              variant="outline"
              className="w-full border-zinc-600 text-zinc-300 hover:bg-zinc-800 py-3"
              disabled={!paymentData && !subscriptionData}
            >
              <Download className="w-4 h-4 mr-2" />
              Download Receipt
            </Button>
          </div>

          {/* Payment ID */}
          {paymentData?.payment_uuid && (
            <div className="mt-6 pt-4 border-t border-zinc-700/50 text-center">
              <p className="text-xs text-zinc-500">
                Payment ID: {paymentData.payment_uuid}
              </p>
            </div>
          )}
        </div>
      </div>
    </>
  );
};

export default PaymentSuccessScreen;