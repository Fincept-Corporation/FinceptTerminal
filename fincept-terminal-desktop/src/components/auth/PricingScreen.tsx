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
      const currentPlanId = session?.user_info?.account_type;
      // Auto-select user's current plan if they have one, otherwise select 'basic'
      setSelectedPlan(currentPlanId || 'basic');
    }
  }, [availablePlans.length, selectedPlan, session?.user_info?.account_type]);

  const getAnnualPrice = (monthlyPrice: number) => {
    return monthlyPrice * 10; // 2 months free when billed annually
  };

  const getPlanIcon = (planId: string) => {
    const iconMap: Record<string, React.ReactNode> = {
      'basic': <Star className="w-4 h-4" />,
      'standard': <Zap className="w-4 h-4" />,
      'pro': <Crown className="w-4 h-4" />,
      'enterprise': <Rocket className="w-4 h-4" />
    };
    return iconMap[planId] || <Star className="w-4 h-4" />;
  };

  // Handle plan selection - Block selection if user has active subscription and trying to select different plan
  const handlePlanClick = (planId: string) => {
    const currentPlanId = session?.user_info?.account_type;

    // If user has active subscription and trying to select a different plan, show message
    if (currentPlanId && currentPlanId !== 'free' && planId !== currentPlanId) {
      setError('You already have an active subscription. To change plans, cancel your current subscription first. You will keep access until the end of your billing period, then you can select a new plan.');
      return;
    }

    setSelectedPlan(planId);
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
      const checkoutUrl = result.data.checkout_url;

      if (checkoutUrl && checkoutUrl !== 'undefined' && checkoutUrl !== 'null') {
        // Store transaction_id in localStorage for payment processing screen
        if (result.data.payment_uuid) {
          localStorage.setItem('pending_payment_order_id', result.data.payment_uuid);
        }

        try {
          // Import Tauri shell plugin and open in system browser
          const { open } = await import('@tauri-apps/plugin-shell');
          await open(checkoutUrl);

          // Navigate to payment processing screen which will poll for status
          onNavigate('paymentProcessing' as Screen);
        } catch (shellError) {
          // Fallback: try to open with window.open
          const newWindow = window.open(checkoutUrl, '_blank', 'noopener,noreferrer');

          if (!newWindow) {
            // If window.open was blocked, show the URL to the user
            setError(`Please open this URL in your browser to complete payment: ${checkoutUrl}`);

            // Try to copy to clipboard
            try {
              await navigator.clipboard.writeText(checkoutUrl);
              setError(`Payment URL copied to clipboard! Please paste it in your browser to complete payment.`);
            } catch (clipboardError) {
              console.error('Failed to copy to clipboard:', clipboardError);
            }
          } else {
            // Window opened successfully, proceed to payment processing
            onNavigate('paymentProcessing' as Screen);
          }
        }
      } else {
        setError(`Payment URL not received from server. Please contact support.`);
      }
    } else {
      setError(result.error || 'Failed to create payment session. Please try again.');
    }
  } catch (err) {
    console.error('Payment session error:', err);
    setError('An unexpected error occurred. Please try again.');
  } finally {
    setIsLoading(false);
  }
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

    // Use API's is_popular flag (set by backend or computed in paymentApi)
    const isPopular = plan.is_popular;
    // Show Best Value badge based on is_popular flag
    const isRecommended = plan.is_popular;

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
              <div className="text-zinc-400 text-xs">{(plan.credits || 0).toLocaleString()}</div>
              <div className="text-white font-medium text-xs">Credits</div>
            </div>
            <div className="bg-zinc-800/30 p-1.5 rounded text-center">
              <div className="text-zinc-400 text-xs">{plan.validity_display || `${plan.validity_days} days`}</div>
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
  const currentPlanId = session?.user_info?.account_type;
  const currentPlanData = currentPlanId ? availablePlans.find(p => p.plan_id === currentPlanId) : undefined;
  const hasActiveSubscription = !!currentPlanId && currentPlanId !== 'free';
  const isCurrentPlanSelected = selectedPlan === currentPlanId;

  return (
    <>

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
            ? `Currently on ${currentPlanData.name}. To change plans, cancel your current subscription first.`
            : 'Select a plan that fits your financial analysis needs.'
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
            <div className="mb-4 p-4 bg-red-900/20 border border-red-900/50 rounded-lg">
              <div className="flex items-start text-red-400 text-sm mb-2">
                <AlertCircle className="w-5 h-5 mr-2 flex-shrink-0 mt-0.5" />
                <div className="flex-1">
                  <p className="font-medium mb-1">Payment Error</p>
                  <p className="text-red-300 text-xs">{error}</p>
                </div>
              </div>
              {error.includes('contact support') && (
                <div className="mt-3 pt-3 border-t border-red-900/30">
                  <p className="text-red-300 text-xs mb-2">Need help? Contact our support team:</p>
                  <div className="flex gap-2 text-xs">
                    <a
                      href="mailto:support@fincept.in"
                      className="text-blue-400 hover:text-blue-300 underline"
                      target="_blank"
                      rel="noopener noreferrer"
                    >
                      support@fincept.in
                    </a>
                    <span className="text-zinc-500">•</span>
                    <a
                      href="https://discord.gg/ae87a8ygbN"
                      className="text-blue-400 hover:text-blue-300 underline"
                      target="_blank"
                      rel="noopener noreferrer"
                    >
                      Discord Support
                    </a>
                  </div>
                </div>
              )}
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
                      {selectedPlanData.price_display} • {selectedPlanData.credits.toLocaleString()} credits • {selectedPlanData.validity_display}
                    </>
                  ) : (
                    'Select a plan to continue'
                  )}
                </div>
              </div>

              <div className="flex gap-3">
                <Button
                  onClick={handleSelectPlan}
                  disabled={!selectedPlan || isLoading || isCurrentPlanSelected}
                  className="bg-blue-600 hover:bg-blue-500 text-white font-medium px-6 py-2 text-sm transition-colors disabled:opacity-50"
                >
                  {isLoading ? (
                    <div className="flex items-center">
                      <Loader2 className="w-3 h-3 animate-spin mr-2" />
                      Processing...
                    </div>
                  ) : isCurrentPlanSelected ? (
                    'Current Plan'
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