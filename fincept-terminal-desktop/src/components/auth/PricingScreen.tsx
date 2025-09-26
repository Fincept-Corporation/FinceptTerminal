// File: src/components/auth/PricingScreen.tsx
// Pricing screen for plan selection after login/signup

import React, { useState } from 'react';
import { Button } from "@/components/ui/button";
import { ArrowLeft, Check, Star, Zap, Crown, Rocket, Shield } from "lucide-react";
import { Screen } from '../../App';

interface PricingScreenProps {
  onNavigate: (screen: Screen) => void;
  onProceedToDashboard: (plan: PricingPlan) => void;
  userType?: 'new' | 'existing'; // new = after signup, existing = after login
}

interface PricingPlan {
  id: string;
  name: string;
  price: number;
  period: 'month';
  icon: React.ReactNode;
  description: string;
  features: string[];
  limits: {
    apiRequests: string;
    dataStreams: string;
    portfolios: string;
    alerts: string;
    history: string;
  };
  popular?: boolean;
  recommended?: boolean;
}

const PricingScreen: React.FC<PricingScreenProps> = ({
  onNavigate,
  onProceedToDashboard,
  userType = 'new'
}) => {
  const [selectedPlan, setSelectedPlan] = useState<string>('free');
  const [billingPeriod, setBillingPeriod] = useState<'monthly' | 'annually'>('monthly');
  const [isLoading, setIsLoading] = useState(false);

  const plans: PricingPlan[] = [
    {
      id: 'free',
      name: 'Free',
      price: 0,
      period: 'month',
      icon: <Shield className="w-5 h-5" />,
      description: 'Perfect for getting started with basic market data',
      features: [
        'Basic market data access',
        'Limited portfolio tracking',
        'Standard charts and graphs',
        'Email support',
        'Basic alerts',
      ],
      limits: {
        apiRequests: '1,000/month',
        dataStreams: '5 concurrent',
        portfolios: '1 portfolio',
        alerts: '10 alerts',
        history: '1 year'
      }
    },
    {
      id: 'starter',
      name: 'Starter',
      price: 20,
      period: 'month',
      icon: <Star className="w-5 h-5" />,
      description: 'Ideal for individual investors and small portfolios',
      features: [
        'Real-time market data',
        'Advanced portfolio analytics',
        'Custom watchlists',
        'Priority email support',
        'Advanced charting tools',
        'Mobile app access',
        'Basic API access'
      ],
      limits: {
        apiRequests: '10,000/month',
        dataStreams: '25 concurrent',
        portfolios: '5 portfolios',
        alerts: '50 alerts',
        history: '5 years'
      }
    },
    {
      id: 'professional',
      name: 'Professional',
      price: 49,
      period: 'month',
      icon: <Zap className="w-5 h-5" />,
      description: 'Advanced tools for active traders and analysts',
      popular: true,
      features: [
        'Premium real-time data',
        'Advanced risk analytics',
        'Multiple portfolio management',
        'Phone & chat support',
        'Professional charting suite',
        'Options & derivatives data',
        'Extended API access',
        'Custom indicators',
        'Backtesting tools'
      ],
      limits: {
        apiRequests: '50,000/month',
        dataStreams: '100 concurrent',
        portfolios: '25 portfolios',
        alerts: '250 alerts',
        history: '10 years'
      }
    },
    {
      id: 'enterprise',
      name: 'Enterprise',
      price: 99,
      period: 'month',
      icon: <Crown className="w-5 h-5" />,
      description: 'Comprehensive solution for investment professionals',
      features: [
        'Level 2 market data',
        'Institutional-grade analytics',
        'Unlimited portfolios',
        'Dedicated support manager',
        'Advanced trading tools',
        'Alternative data sources',
        'Full API access',
        'Custom integrations',
        'White-label options',
        'Team collaboration tools'
      ],
      limits: {
        apiRequests: '200,000/month',
        dataStreams: '500 concurrent',
        portfolios: 'Unlimited',
        alerts: '1,000 alerts',
        history: '20 years'
      }
    },
    {
      id: 'institutional',
      name: 'Institutional',
      price: 199,
      period: 'month',
      icon: <Rocket className="w-5 h-5" />,
      description: 'Premium solution for large organizations and funds',
      recommended: true,
      features: [
        'Complete market data coverage',
        'Advanced risk management',
        'Unlimited everything',
        '24/7 dedicated support',
        'Custom development',
        'On-premise deployment',
        'Enterprise API',
        'Advanced compliance tools',
        'Multi-tenant architecture',
        'Custom reporting',
        'SLA guarantees'
      ],
      limits: {
        apiRequests: 'Unlimited',
        dataStreams: 'Unlimited',
        portfolios: 'Unlimited',
        alerts: 'Unlimited',
        history: 'Complete history'
      }
    }
  ];

  const getAnnualPrice = (monthlyPrice: number) => {
    return monthlyPrice * 10; // 2 months free when billed annually
  };

  const handleSelectPlan = async () => {
    if (!selectedPlan) return;

    setIsLoading(true);

    try {
      // TODO: Replace with actual API call to process plan selection
      setTimeout(() => {
        const plan = plans.find(p => p.id === selectedPlan);
        if (plan) {
          console.log('Plan selected:', plan);
          onProceedToDashboard(plan);
        }
        setIsLoading(false);
      }, 2000);
    } catch (err) {
      console.error('Plan selection error:', err);
      setIsLoading(false);
    }
  };

  const getPlanCard = (plan: PricingPlan) => {
    const isSelected = selectedPlan === plan.id;
    const displayPrice = billingPeriod === 'annually' && plan.price > 0
      ? getAnnualPrice(plan.price)
      : plan.price;
    const priceUnit = billingPeriod === 'annually' ? 'year' : 'month';

    return (
      <div
        key={plan.id}
        onClick={() => setSelectedPlan(plan.id)}
        className={`
          relative cursor-pointer rounded-lg border-2 p-6 transition-all duration-200 hover:scale-105
          ${isSelected
            ? 'border-blue-500 bg-blue-600/10 shadow-lg shadow-blue-500/20'
            : 'border-zinc-600 bg-zinc-800/50 hover:border-zinc-500'
          }
        `}
      >
        {plan.popular && (
          <div className="absolute -top-3 left-1/2 transform -translate-x-1/2">
            <span className="bg-blue-500 text-white text-xs font-medium px-3 py-1 rounded-full">
              Most Popular
            </span>
          </div>
        )}

        {plan.recommended && (
          <div className="absolute -top-3 left-1/2 transform -translate-x-1/2">
            <span className="bg-green-500 text-white text-xs font-medium px-3 py-1 rounded-full">
              Recommended
            </span>
          </div>
        )}

        <div className="text-center mb-4">
          <div className={`
            inline-flex items-center justify-center w-12 h-12 rounded-full mb-3
            ${isSelected ? 'bg-blue-500 text-white' : 'bg-zinc-700 text-zinc-300'}
          `}>
            {plan.icon}
          </div>

          <h3 className="text-white text-xl font-semibold mb-2">{plan.name}</h3>

          <div className="mb-2">
            {plan.price === 0 ? (
              <span className="text-3xl font-bold text-green-400">Free</span>
            ) : (
              <div className="flex items-baseline justify-center">
                <span className="text-3xl font-bold text-white">${displayPrice}</span>
                <span className="text-zinc-400 text-sm ml-1">/{priceUnit}</span>
              </div>
            )}

            {billingPeriod === 'annually' && plan.price > 0 && (
              <div className="text-green-400 text-xs mt-1">
                Save ${plan.price * 2}/year
              </div>
            )}
          </div>

          <p className="text-zinc-400 text-xs leading-relaxed">{plan.description}</p>
        </div>

        <div className="space-y-3 mb-6">
          <div className="text-white text-sm font-medium">Usage Limits:</div>
          <div className="grid grid-cols-2 gap-2 text-xs">
            <div className="bg-zinc-800/50 p-2 rounded">
              <div className="text-zinc-400">API Requests</div>
              <div className="text-white font-medium">{plan.limits.apiRequests}</div>
            </div>
            <div className="bg-zinc-800/50 p-2 rounded">
              <div className="text-zinc-400">Data Streams</div>
              <div className="text-white font-medium">{plan.limits.dataStreams}</div>
            </div>
            <div className="bg-zinc-800/50 p-2 rounded">
              <div className="text-zinc-400">Portfolios</div>
              <div className="text-white font-medium">{plan.limits.portfolios}</div>
            </div>
            <div className="bg-zinc-800/50 p-2 rounded">
              <div className="text-zinc-400">History</div>
              <div className="text-white font-medium">{plan.limits.history}</div>
            </div>
          </div>
        </div>

        <div className="space-y-2">
          <div className="text-white text-sm font-medium">Features:</div>
          <ul className="space-y-1">
            {plan.features.map((feature, index) => (
              <li key={index} className="flex items-start text-xs text-zinc-300">
                <Check className="w-3 h-3 text-green-400 mr-2 mt-0.5 flex-shrink-0" />
                <span>{feature}</span>
              </li>
            ))}
          </ul>
        </div>
      </div>
    );
  };

  return (
    <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-7xl mx-4 shadow-2xl max-h-[90vh] overflow-y-auto">
      <div className="mb-6">
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center">
            <button
              onClick={() => onNavigate('login')}
              className="text-zinc-400 hover:text-white transition-colors mr-3"
            >
              <ArrowLeft className="h-4 w-4" />
            </button>
            <h2 className="text-white text-2xl font-light">Choose Your Plan</h2>
          </div>

          <div className="flex items-center bg-zinc-800 rounded-lg p-1">
            <button
              onClick={() => setBillingPeriod('monthly')}
              className={`px-3 py-1 text-xs rounded transition-colors ${
                billingPeriod === 'monthly'
                  ? 'bg-zinc-600 text-white'
                  : 'text-zinc-400 hover:text-white'
              }`}
            >
              Monthly
            </button>
            <button
              onClick={() => setBillingPeriod('annually')}
              className={`px-3 py-1 text-xs rounded transition-colors ${
                billingPeriod === 'annually'
                  ? 'bg-zinc-600 text-white'
                  : 'text-zinc-400 hover:text-white'
              }`}
            >
              Annual <span className="text-green-400 text-xs">(Save 17%)</span>
            </button>
          </div>
        </div>

        <p className="text-zinc-400 text-sm leading-5">
          {userType === 'new'
            ? 'Welcome to Fincept! Select a plan that fits your needs to get started with professional financial analytics.'
            : 'Upgrade your plan to unlock more features and higher limits for your financial analysis.'
          }
        </p>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 xl:grid-cols-5 gap-6 mb-8">
        {plans.map(getPlanCard)}
      </div>

      <div className="border-t border-zinc-700 pt-6">
        <div className="flex flex-col sm:flex-row items-center justify-between gap-4">
          <div className="text-center sm:text-left">
            <div className="text-white text-sm font-medium mb-1">
              Selected: {plans.find(p => p.id === selectedPlan)?.name || 'None'}
            </div>
            <div className="text-zinc-400 text-xs">
              {selectedPlan === 'free'
                ? 'Start exploring with our free plan - no credit card required'
                : 'You can change or cancel your plan anytime'
              }
            </div>
          </div>

          <div className="flex gap-3">
            {selectedPlan !== 'free' && (
              <Button
                onClick={() => setSelectedPlan('free')}
                className="bg-zinc-700 hover:bg-zinc-600 text-white px-6 py-2 text-sm font-normal transition-colors"
                disabled={isLoading}
              >
                Try Free Plan
              </Button>
            )}

            <Button
              onClick={handleSelectPlan}
              disabled={!selectedPlan || isLoading}
              className={`px-8 py-2 text-sm font-normal transition-colors disabled:opacity-50 ${
                selectedPlan === 'free'
                  ? 'bg-green-600 hover:bg-green-500 text-white'
                  : 'bg-blue-600 hover:bg-blue-500 text-white'
              }`}
            >
              {isLoading ? (
                <div className="flex items-center">
                  <div className="w-4 h-4 border border-white/30 border-t-white rounded-full animate-spin mr-2"></div>
                  Processing...
                </div>
              ) : (
                selectedPlan === 'free'
                  ? 'Start with Free Plan'
                  : `Continue with ${plans.find(p => p.id === selectedPlan)?.name || 'Selected'} Plan`
              )}
            </Button>
          </div>
        </div>

        <div className="mt-4 text-center">
          <div className="text-zinc-500 text-xs space-y-1">
            <p>• All plans include 14-day free trial (except Free plan)</p>
            <p>• No setup fees or long-term contracts</p>
            <p>• Enterprise support available for all paid plans</p>
            <p>• Cancel or upgrade anytime</p>
          </div>
        </div>
      </div>
    </div>
  );
};

export default PricingScreen;