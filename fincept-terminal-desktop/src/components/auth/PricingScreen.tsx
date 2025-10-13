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

  // Load plans on component mount - with debouncing
  useEffect(() => {
    const loadPlans = async () => {
      // Prevent multiple simultaneous requests
      if (loadingRef.current || hasLoadedPlans || isLoadingPlans) {
        console.log('Skipping plan load - already in progress or completed');
        return;
      }

      if (availablePlans.length > 0) {
        console.log('Plans already available, skipping load');
        setHasLoadedPlans(true);
        return;
      }

      loadingRef.current = true;
      console.log('Loading subscription plans from API...');

      try {
        const result = await fetchPlans();
        if (result.success) {
          setHasLoadedPlans(true);
          setError('');
        } else {
          setError(result.error || 'Failed to load subscription plans');

          // Retry after 3 seconds if it failed
          retryTimeoutRef.current = setTimeout(() => {
            console.log('Retrying to load plans...');
            loadingRef.current = false;
            setHasLoadedPlans(false);
            loadPlans();
          }, 3000);
        }
      } catch (err) {
        console.error('Error loading plans:', err);
        setError('Failed to load subscription plans');
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
      const currentPlanId = (session?.subscription as any)?.data?.subscription?.plan?.plan_id ||
                            session?.subscription?.subscription?.plan?.plan_id;
      // Don't auto-select if user already has current plan, otherwise select starter
      if (!currentPlanId) {
        setSelectedPlan('starter_20');
      }
    }
  }, [availablePlans.length, selectedPlan, session?.subscription]);

  const getAnnualPrice = (monthlyPrice: number) => {
    return monthlyPrice * 10; // 2 months free when billed annually
  };

  const getPlanIcon = (planId: string) => {
    const iconMap: Record<string, React.ReactNode> = {
      'starter_20': <Star className="w-4 h-4" />,
      'professional_49': <Zap className="w-4 h-4" />,
      'enterprise_99': <Crown className="w-4 h-4" />,
      'unlimited_199': <Rocket className="w-4 h-4" />
    };
    return iconMap[planId] || <Shield className="w-4 h-4" />;
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
    console.log('Creating payment session for plan:', selectedPlan);

    const result = await createPaymentSession(selectedPlan);

    if (result.success && result.data) {
      console.log('Payment session created:', result.data);

      // DEBUG: Log the exact structure
      console.log('result.data structure:', JSON.stringify(result.data, null, 2));
      console.log('checkout_url direct access:', (result.data as any).checkout_url);
      console.log('checkout_url via data.data:', (result.data as any).data?.checkout_url);

      // Try different data access patterns
      const checkoutUrl = (result.data as any).checkout_url || (result.data as any).data?.checkout_url;

      if (checkoutUrl) {
        console.log('Found checkout URL:', checkoutUrl);


        PaymentUtils.openPaymentUrl(checkoutUrl);
        onNavigate('paymentProcessing' as Screen);
      } else {
        console.error('No checkout URL found in result data');
        setError('No payment URL received. Please try again.');
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
    console.log('Continuing with free plan');
    onProceedToDashboard();
  };

  const handleRetryLoadPlans = async () => {
    setError('');
    setHasLoadedPlans(false);
    loadingRef.current = false;

    console.log('Manual retry - loading plans...');
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
    const priceUnit = 'month';

    // Determine if plan is popular or recommended
    const isPopular = plan.plan_id === 'starter_20';
    const isRecommended = plan.plan_id === 'unlimited_199';

    // Check if this is the current plan
    const currentPlanId = (session?.subscription as any)?.data?.subscription?.plan?.plan_id ||
                          session?.subscription?.subscription?.plan?.plan_id;
    const isCurrentPlan = currentPlanId === plan.plan_id;

    return (
      <div
        key={plan.plan_id}
        onClick={() => !isCurrentPlan && setSelectedPlan(plan.plan_id)}
        className={`
          relative rounded-lg border-2 p-4 transition-all duration-200
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
                {PaymentUtils.formatCurrency(displayPrice)}
              </span>
              <span className="text-zinc-400 text-xs ml-1">/{priceUnit}</span>
            </div>
          </div>

          <p className="text-zinc-400 text-xs">{plan.description}</p>
        </div>

        <div className="space-y-2 mb-3">
          <div className="grid grid-cols-2 gap-1 text-xs">
            <div className="bg-zinc-800/30 p-1.5 rounded text-center">
              <div className="text-zinc-400 text-xs">{plan.features.api_calls}</div>
              <div className="text-white font-medium text-xs">API Calls</div>
            </div>
            <div className="bg-zinc-800/30 p-1.5 rounded text-center">
              <div className="text-zinc-400 text-xs">{plan.features.databases}</div>
              <div className="text-white font-medium text-xs">Databases</div>
            </div>
          </div>
        </div>

        <div className="space-y-1">
          <div className="text-xs text-zinc-300 space-y-0.5">
            <div className="flex items-center">
              <Check className="w-3 h-3 text-green-400 mr-2 flex-shrink-0" />
              <span>{plan.features.support}</span>
            </div>
            {plan.priority_support && (
              <div className="flex items-center">
                <Check className="w-3 h-3 text-green-400 mr-2 flex-shrink-0" />
                <span>Priority support</span>
              </div>
            )}
            <div className="flex items-center">
              <Check className="w-3 h-3 text-green-400 mr-2 flex-shrink-0" />
              <span>Real-time data</span>
            </div>
            <div className="flex items-center">
              <Check className="w-3 h-3 text-green-400 mr-2 flex-shrink-0" />
              <span>Custom dashboards</span>
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
            {userType === 'existing' && (
              <Button
                onClick={handleContinueWithFree}
                variant="outline"
                className="w-full bg-zinc-800 border-zinc-600 text-white hover:bg-zinc-700 hover:border-zinc-500"
              >
                Continue with Free Plan
              </Button>
            )}
          </div>
        </div>
      </div>
    );
  }

  const selectedPlanData = availablePlans.find(p => p.plan_id === selectedPlan);

  // Get current plan info
  const currentPlanId = (session?.subscription as any)?.data?.subscription?.plan?.plan_id ||
                        session?.subscription?.subscription?.plan?.plan_id;
  const currentPlanData = availablePlans.find(p => p.plan_id === currentPlanId);
  const hasActiveSubscription = (session?.subscription as any)?.data?.has_subscription ||
                                 session?.subscription?.has_subscription;
  const isCurrentPlanSelected = selectedPlan === currentPlanId;

  return (
    <div className="bg-zinc-900/95 backdrop-blur-sm border border-zinc-700 rounded-xl p-6 w-full max-w-5xl mx-auto shadow-2xl">
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
          <div className="grid grid-cols-2 lg:grid-cols-4 gap-4 mb-6">
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
                      {PaymentUtils.formatCurrency(selectedPlanData.price_usd)}/month • 14-day free trial • Cancel anytime
                    </>
                  ) : (
                    'Select a plan to continue'
                  )}
                </div>
              </div>

              <div className="flex gap-3">
                {userType === 'existing' && !hasActiveSubscription && (
                  <Button
                    onClick={handleContinueWithFree}
                    variant="outline"
                    className="bg-zinc-800 border-zinc-600 text-white hover:bg-zinc-700 hover:border-zinc-500 px-4 py-2 text-sm transition-colors"
                    disabled={isLoading}
                  >
                    Stay Free
                  </Button>
                )}

                {hasActiveSubscription && (
                  <Button
                    onClick={onProceedToDashboard}
                    variant="outline"
                    className="bg-zinc-800 border-zinc-600 text-white hover:bg-zinc-700 hover:border-zinc-500 px-4 py-2 text-sm transition-colors"
                    disabled={isLoading}
                  >
                    Keep Current Plan
                  </Button>
                )}

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
                  ) : hasActiveSubscription ? (
                    `Change to ${selectedPlanData?.name || 'Plan'}`
                  ) : (
                    `Start Free Trial - ${selectedPlanData?.name || 'Plan'}`
                  )}
                </Button>
              </div>
            </div>
          </div>
        </>
      )}
    </div>
  );
};

export default PricingScreen;