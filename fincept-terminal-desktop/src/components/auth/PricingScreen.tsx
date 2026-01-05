// File: src/components/auth/PricingScreen.tsx
// Pricing screen for plan selection after login/signup with API integration

import React, { useState, useEffect, useRef } from 'react';
import { Button } from "@/components/ui/button";
import { ArrowLeft, Check, Star, Zap, Crown, Rocket, Shield, Loader2, AlertCircle } from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';
import { PaymentUtils } from '@/services/paymentApi';

interface PricingScreenProps {
  onNavigate: (screen: Screen) => void;
  onProceedToDashboard: () => void;
  userType?: 'new' | 'existing'; // new = after signup, existing = after login
}

const PricingScreen: React.FC<PricingScreenProps> = ({
  onNavigate,
  onProceedToDashboard,
  userType = 'new'
}) => {
  const { availablePlans, isLoadingPlans, fetchPlans, createPaymentSession, session } = useAuth();
  const [selectedPlan, setSelectedPlan] = useState<string>('');
  const [billingPeriod, setBillingPeriod] = useState<'monthly' | 'annually'>('monthly');
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string>('');
  const [hasLoadedPlans, setHasLoadedPlans] = useState(false);
  const [showDowngradeConfirm, setShowDowngradeConfirm] = useState(false);
  const [pendingPlanSelection, setPendingPlanSelection] = useState<string>('');

  // Prevent multiple simultaneous requests
  const loadingRef = useRef(false);
  const retryTimeoutRef = useRef<NodeJS.Timeout | undefined>(undefined);
  const retryCountRef = useRef(0);
  const MAX_RETRIES = 2; // Only retry twice to prevent infinite loops

  // Load plans on component mount - with debouncing
  useEffect(() => {
    const loadPlans = async () => {
      if (loadingRef.current || hasLoadedPlans || isLoadingPlans) {
        return;
      }

      if (availablePlans.length > 0) {
        setHasLoadedPlans(true);
        return;
      }

      loadingRef.current = true;

      try {
        const result = await fetchPlans();
        if (result.success) {
          setHasLoadedPlans(true);
          setError('');
          retryCountRef.current = 0;
        } else {
          setError(result.error || 'Failed to load subscription plans');

          if (retryCountRef.current < MAX_RETRIES) {
            retryCountRef.current += 1;

            retryTimeoutRef.current = setTimeout(() => {
              loadingRef.current = false;
              setHasLoadedPlans(false);
              loadPlans();
            }, 3000);
          } else {
            setError('Unable to load plans after multiple attempts. Please try again later or continue with free plan.');
          }
        }
      } catch (err) {
        console.error('Error loading plans:', err);
        setError('Failed to load subscription plans');

        if (retryCountRef.current < MAX_RETRIES) {
          retryCountRef.current += 1;

          retryTimeoutRef.current = setTimeout(() => {
            loadingRef.current = false;
            setHasLoadedPlans(false);
            loadPlans();
          }, 3000);
        }
      } finally {
        loadingRef.current = false;
      }
    };

    // Small delay to ensure component is fully mounted
    const timer = setTimeout(loadPlans, 100);

    return () => {
      clearTimeout(timer);
      if (retryTimeoutRef.current) {
        clearTimeout(retryTimeoutRef.current);
      }
    };
  }, []); // Only run once on mount

  // Separate effect to set default selection when plans are loaded
  useEffect(() => {
    if (availablePlans.length > 0 && !selectedPlan) {
      const currentPlanId = session?.user_info?.account_type || 'free';
      // Auto-select user's current plan
      setSelectedPlan(currentPlanId);
    }
  }, [availablePlans.length, selectedPlan, session?.user_info?.account_type]);

  const getAnnualPrice = (monthlyPrice: number) => {
    return monthlyPrice * 10; // 2 months free when billed annually
  };

  const getPlanIcon = (planId: string) => {
    const iconMap: Record<string, React.ReactNode> = {
      'free': <Shield className="w-4 h-4" />,
      'basic': <Star className="w-4 h-4" />,
      'standard': <Zap className="w-4 h-4" />,
      'pro': <Crown className="w-4 h-4" />,
      'enterprise': <Rocket className="w-4 h-4" />
    };
    return iconMap[planId] || <Shield className="w-4 h-4" />;
  };

  // Check if selecting a plan is a downgrade
  const isDowngrade = (targetPlanId: string): boolean => {
    const currentPlanId = session?.user_info?.account_type || 'free';
    const planTiers: { [key: string]: number } = {
      'free': 0,
      'basic': 1,
      'standard': 2,
      'pro': 3,
      'enterprise': 4
    };

    const currentTier = planTiers[currentPlanId] || 0;
    const targetTier = planTiers[targetPlanId] || 0;

    return targetTier < currentTier && currentTier > 0;
  };

  // Handle plan selection with downgrade confirmation
  const handlePlanClick = (planId: string) => {
    if (isDowngrade(planId)) {
      setPendingPlanSelection(planId);
      setShowDowngradeConfirm(true);
    } else {
      setSelectedPlan(planId);
    }
  };

  // Confirm downgrade
  const confirmDowngrade = () => {
    setSelectedPlan(pendingPlanSelection);
    setShowDowngradeConfirm(false);
    setPendingPlanSelection('');
  };

  // Cancel downgrade
  const cancelDowngrade = () => {
    setShowDowngradeConfirm(false);
    setPendingPlanSelection('');
  };

const handleSelectPlan = async () => {
  if (!selectedPlan) {
    setError('Please select a plan');
    return;
  }

  if (!session?.api_key) {
    setError('Authentication required. Please log in again.');
    return;
  }

  setIsLoading(true);
  setError('');

  try {
    const result = await createPaymentSession(selectedPlan, 'USD');

    if (result.success && result.data) {
      console.log('=== PAYMENT SESSION DEBUG ===');
      console.log('Full response data:', JSON.stringify(result.data, null, 2));
      console.log('checkout_url:', result.data.checkout_url);
      console.log('session_id:', result.data.session_id);
      console.log('order_id:', result.data.payment_uuid);
      console.log('========================');

      // Use checkout_url from the new API format
      const checkoutUrl = result.data.checkout_url;

      if (checkoutUrl && checkoutUrl !== 'undefined') {
        console.log('Opening payment in external browser:', checkoutUrl);

        // Store order_id in localStorage for payment processing screen
        localStorage.setItem('pending_payment_order_id', result.data.payment_uuid);

        // Open in external browser using Tauri shell
        const { open } = await import('@tauri-apps/plugin-shell');
        await open(checkoutUrl);

        // Navigate to payment processing screen which will poll for status
        onNavigate('paymentProcessing' as Screen);
      } else {
        console.error('[ERROR] No checkout_url found in response!');
        console.error('Available fields:', Object.keys(result.data));
        console.error('Full response:', result.data);
        setError(`No payment session received. Available fields: ${Object.keys(result.data).join(', ')}`);
      }
    } else {
      console.error('Failed to create payment session:', result.error);
      setError(result.error || 'Failed to create payment session. Please try again.');
    }
  } catch (err) {
    console.error('Payment session error:', err);
    setError('An unexpected error occurred. Please try again.');
  } finally {
    setIsLoading(false);
  }
};

  const handleContinueWithFree = () => {
    onProceedToDashboard();
  };

  const handleRetryLoadPlans = async () => {
    setError('');
    setHasLoadedPlans(false);
    loadingRef.current = false;
    retryCountRef.current = 0;

    const result = await fetchPlans();
    if (result.success) {
      setHasLoadedPlans(true);
    } else {
      setError(result.error || 'Failed to load subscription plans');
    }
  };

  const getPlanCard = (plan: any) => {
    const isSelected = selectedPlan === plan.plan_id;
    const displayPrice = plan.price_usd;

    // Use API's is_popular flag
    const isPopular = plan.is_popular;
    const isRecommended = plan.plan_id === 'pro' || plan.plan_id === 'enterprise';

    // Check if this is the current plan
    const currentPlanId = session?.user_info?.account_type || 'free';
    const isCurrentPlan = currentPlanId === plan.plan_id;

    return (
      <div
        key={plan.plan_id}
        onClick={() => handlePlanClick(plan.plan_id)}
        className={`
          relative rounded-lg border-2 p-4 transition-all duration-200 flex flex-col
          ${isCurrentPlan
            ? 'border-green-500 bg-green-600/10 cursor-default'
            : isSelected
            ? 'border-blue-500 bg-blue-600/10 shadow-lg shadow-blue-500/20 cursor-pointer hover:scale-[1.02]'
            : 'border-zinc-600 bg-zinc-800/50 hover:border-zinc-500 cursor-pointer hover:scale-[1.02]'
          }
        `}
      >
        {isCurrentPlan && (
          <div className="absolute -top-2 left-1/2 transform -translate-x-1/2">
            <span className="bg-green-500 text-white text-xs font-medium px-2 py-0.5 rounded-full">
              Current Plan
            </span>
          </div>
        )}

        {!isCurrentPlan && isPopular && (
          <div className="absolute -top-2 left-1/2 transform -translate-x-1/2">
            <span className="bg-blue-500 text-white text-xs font-medium px-2 py-0.5 rounded-full">
              Popular
            </span>
          </div>
        )}

        {!isCurrentPlan && isRecommended && (
          <div className="absolute -top-2 left-1/2 transform -translate-x-1/2">
            <span className="bg-orange-500 text-white text-xs font-medium px-2 py-0.5 rounded-full">
              Best Value
            </span>
          </div>
        )}

        {/* Header Section - Fixed Height */}
        <div className="text-center mb-3">
          <div className={`
            inline-flex items-center justify-center w-8 h-8 rounded-full mb-2
            ${isSelected ? 'bg-blue-500 text-white' : 'bg-zinc-700 text-zinc-300'}
          `}>
            {getPlanIcon(plan.plan_id)}
          </div>

          <h3 className="text-white text-lg font-semibold mb-1">{plan.name}</h3>

          <div className="mb-2">
            <div className="flex items-baseline justify-center">
              <span className="text-2xl font-bold text-white">
                {plan.price_display}
              </span>
            </div>
          </div>

          <p className="text-zinc-400 text-xs h-10 line-clamp-2">{plan.description}</p>
        </div>

        {/* Credits/Validity Section - Fixed Height */}
        <div className="mb-3">
          <div className="grid grid-cols-2 gap-1 text-xs">
            <div className="bg-zinc-800/30 p-1.5 rounded text-center">
              <div className="text-zinc-400 text-xs">{plan.credits_amount.toLocaleString()}</div>
              <div className="text-white font-medium text-xs">Credits</div>
            </div>
            <div className="bg-zinc-800/30 p-1.5 rounded text-center">
              <div className="text-zinc-400 text-xs">{plan.validity_display}</div>
              <div className="text-white font-medium text-xs">Validity</div>
            </div>
          </div>
        </div>

        {/* Features Section - Flexible Height */}
        <div className="flex-1">
          <div className="text-xs text-zinc-300 space-y-0.5">
            <div className="flex items-center">
              <Check className="w-3 h-3 text-green-400 mr-2 flex-shrink-0" />
              <span>{plan.support_display}</span>
            </div>
            <div className="flex items-center">
              <Check className="w-3 h-3 text-green-400 mr-2 flex-shrink-0" />
              <span>All API endpoints</span>
            </div>
            <div className="flex items-center">
              <Check className="w-3 h-3 text-green-400 mr-2 flex-shrink-0" />
              <span>Real-time data access</span>
            </div>
          </div>
        </div>
      </div>
    );
  };

  if (isLoadingPlans && !hasLoadedPlans) {
    return (
      <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-8 w-full max-w-md mx-auto shadow-2xl">
        <div className="text-center">
          <Loader2 className="w-8 h-8 animate-spin text-blue-500 mx-auto mb-4" />
          <h2 className="text-white text-xl font-semibold mb-2">Loading Plans</h2>
          <p className="text-zinc-400 text-sm">Fetching subscription options...</p>
        </div>
      </div>
    );
  }

  if (error && availablePlans.length === 0) {
    return (
      <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-8 w-full max-w-md mx-auto shadow-2xl">
        <div className="text-center">
          <AlertCircle className="w-8 h-8 text-red-500 mx-auto mb-4" />
          <h2 className="text-white text-xl font-semibold mb-2">Unable to Load Plans</h2>
          <p className="text-zinc-400 text-sm mb-4">{error}</p>
          <div className="space-y-2">
            <Button
              onClick={handleRetryLoadPlans}
              className="w-full bg-blue-600 hover:bg-blue-500 text-white"
              disabled={isLoadingPlans}
            >
              {isLoadingPlans ? (
                <div className="flex items-center">
                  <Loader2 className="w-4 h-4 animate-spin mr-2" />
                  Loading...
                </div>
              ) : (
                'Retry'
              )}
            </Button>
          </div>
        </div>
      </div>
    );
  }

  const selectedPlanData = availablePlans.find(p => p.plan_id === selectedPlan);

  // Get current plan info (credit-based system)
  // In credit system, users can always buy more credits
  // Free plan users have 350 credits, can purchase any plan
  const currentPlanId = session?.user_info?.account_type || 'free';
  const currentPlanData = availablePlans.find(p => p.plan_id === currentPlanId);
  const hasActiveSubscription = !!session?.user_info?.account_type && session.user_info.account_type !== 'free';
  const isCurrentPlanSelected = selectedPlan === currentPlanId;
  const pendingPlanData = availablePlans.find(p => p.plan_id === pendingPlanSelection);

  return (
    <>
      {/* Downgrade Confirmation Modal */}
      {showDowngradeConfirm && pendingPlanData && currentPlanData && (
        <div className="fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50">
          <div className="bg-zinc-900 border border-zinc-700 rounded-xl p-6 max-w-md mx-4 shadow-2xl">
            <div className="flex items-center mb-4">
              <AlertCircle className="w-6 h-6 text-orange-500 mr-3" />
              <h3 className="text-white text-lg font-semibold">Confirm Plan Downgrade</h3>
            </div>

            <div className="space-y-3 mb-6">
              <p className="text-zinc-300 text-sm">
                You are about to downgrade from <span className="font-semibold text-white">{currentPlanData.name}</span> to <span className="font-semibold text-white">{pendingPlanData.name}</span>.
              </p>

              <div className="bg-orange-500/10 border border-orange-500/30 rounded-lg p-3">
                <p className="text-orange-200 text-xs font-medium mb-2">What happens when you downgrade:</p>
                <ul className="text-orange-200/80 text-xs space-y-1">
                  <li>• Credits: {currentPlanData.credits_amount.toLocaleString()} → {pendingPlanData.credits_amount.toLocaleString()} per month</li>
                  <li>• Price: {currentPlanData.price_display} → {pendingPlanData.price_display} per month</li>
                  <li>• Your remaining credits from the current plan will be lost</li>
                  <li>• The new plan will take effect immediately after payment</li>
                </ul>
              </div>

              <p className="text-zinc-400 text-xs">
                Are you sure you want to continue with the downgrade?
              </p>
            </div>

            <div className="flex gap-3">
              <Button
                onClick={cancelDowngrade}
                variant="outline"
                className="flex-1 bg-zinc-800 border-zinc-600 text-white hover:bg-zinc-700"
              >
                Cancel
              </Button>
              <Button
                onClick={confirmDowngrade}
                className="flex-1 bg-orange-500 hover:bg-orange-600 text-white"
              >
                Confirm Downgrade
              </Button>
            </div>
          </div>
        </div>
      )}

      <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-6 w-full max-w-6xl mx-auto shadow-2xl overflow-y-auto max-h-[90vh]">
      <div className="mb-6">
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center">
            <button
              onClick={() => {
                // Navigate based on authentication status
                if (session?.authenticated) {
                  onProceedToDashboard();
                } else {
                  onNavigate('login' as Screen);
                }
              }}
              className="text-zinc-400 hover:text-white transition-colors mr-3"
              disabled={isLoading}
            >
              <ArrowLeft className="h-4 w-4" />
            </button>
            <h2 className="text-white text-xl font-semibold">
              {userType === 'existing' ? 'Upgrade Your Plan' : 'Choose Your Plan'}
            </h2>
          </div>

        </div>

        <p className="text-zinc-400 text-sm">
          {hasActiveSubscription && currentPlanData
            ? `Currently on ${currentPlanData.name} plan. Select a different plan to upgrade or downgrade.`
            : userType === 'existing'
            ? 'Currently on Free plan. Upgrade for advanced features and higher limits.'
            : 'Select a plan that fits your financial analysis needs. Start with 14-day free trial.'
          }
        </p>
      </div>

      {availablePlans.length === 0 ? (
        <div className="text-center py-8">
          <p className="text-zinc-400">No subscription plans available at the moment.</p>
          <Button
            onClick={handleRetryLoadPlans}
            className="mt-4 bg-blue-600 hover:bg-blue-500 text-white"
            disabled={isLoadingPlans}
          >
            {isLoadingPlans ? 'Loading...' : 'Retry Loading Plans'}
          </Button>
        </div>
      ) : (
        <>
          <div className="grid grid-cols-5 gap-3 mb-6">
            {availablePlans.map(getPlanCard)}
          </div>

          {error && (
            <div className="mb-4 p-3 bg-red-900/20 border border-red-900/50 rounded-lg">
              <div className="flex items-center text-red-400 text-sm">
                <AlertCircle className="w-4 h-4 mr-2 flex-shrink-0" />
                {error}
              </div>
            </div>
          )}

          <div className="border-t border-zinc-700/50 pt-4">
            <div className="flex flex-col sm:flex-row items-center justify-between gap-3">
              <div className="text-center sm:text-left">
                <div className="text-white text-sm font-medium">
                  Selected: {selectedPlanData?.name || 'None'}
                </div>
                <div className="text-zinc-400 text-xs">
                  {selectedPlanData ? (
                    <>
                      {selectedPlanData.price_display} • {selectedPlanData.credits_amount.toLocaleString()} credits • {selectedPlanData.validity_display}
                    </>
                  ) : (
                    'Select a plan to continue'
                  )}
                </div>
              </div>

              <div className="flex gap-3">
                <Button
                  onClick={selectedPlan === 'free' ? handleContinueWithFree : handleSelectPlan}
                  disabled={!selectedPlan || isLoading}
                  className="bg-blue-600 hover:bg-blue-500 text-white font-medium px-6 py-2 text-sm transition-colors disabled:opacity-50"
                >
                  {isLoading ? (
                    <div className="flex items-center">
                      <Loader2 className="w-3 h-3 animate-spin mr-2" />
                      Processing...
                    </div>
                  ) : selectedPlan === 'free' ? (
                    'Continue with Free'
                  ) : (
                    `Proceed with ${selectedPlanData?.name || 'Plan'}`
                  )}
                </Button>
              </div>
            </div>
          </div>
        </>
      )}
    </div>
    </>
  );
};

export default PricingScreen;