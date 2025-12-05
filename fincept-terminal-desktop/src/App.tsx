// File: src/App.tsx
// Main application file that handles routing between authentication screens, payment flow, and dashboard

import React, { useState, useEffect } from 'react';
import { useAuth } from './contexts/AuthContext';
import { NavigationProvider } from './contexts/NavigationContext';
import { ThemeProvider } from './contexts/ThemeContext';
import { DataSourceProvider } from './contexts/DataSourceContext';
import { ProviderProvider } from './contexts/ProviderContext';
import { setPaymentWindowManager } from './services/paymentApi';
import { workflowService } from './services/workflowService';

// Import screens
import LoginScreen from './components/auth/LoginScreen';
import RegisterScreen from './components/auth/RegisterScreen';
import ForgotPasswordScreen from './components/auth/ForgotPasswordScreen';
import HelpScreen from './components/auth/HelpScreen';
import PricingScreen from './components/auth/PricingScreen';
import PaymentProcessingScreen from './components/payment/PaymentProcessingScreen';
import PaymentSuccessScreen from './components/payment/PaymentSuccessScreen';
import ContactUsScreen from './components/info/ContactUsScreen';
import TermsOfServiceScreen from './components/info/TermsOfServiceScreen';
import TrademarksScreen from './components/info/TrademarksScreen';
import PrivacyPolicyScreen from './components/info/PrivacyPolicyScreen';
import DashboardScreen from './components/dashboard/DashboardScreen';
import BackgroundPattern from './components/common/BackgroundPattern';
import Header from './components/common/Header';
import Footer from './components/common/Footer';
import PaymentOverlay from './components/payment/PaymentOverlay';
import SetupScreen from './components/setup/SetupScreen';

export type Screen =
  | 'login'
  | 'register'
  | 'forgotPassword'
  | 'help'
  | 'contactUs'
  | 'termsOfService'
  | 'trademarks'
  | 'privacyPolicy'
  | 'pricing'
  | 'paymentProcessing'
  | 'paymentSuccess'
  | 'dashboard';

interface PaymentWindowState {
  isOpen: boolean;
  checkoutUrl: string;
  planName: string;
  planPrice: number;
  onSuccess?: () => void;
  onCancel?: () => void;
  onError?: (error: string) => void;
}

const App: React.FC = () => {
  const { session, isLoading } = useAuth();
  const [currentScreen, setCurrentScreen] = useState<Screen>('login');
  const [hasChosenFreePlan, setHasChosenFreePlan] = useState(false);
  const [cameFromLogin, setCameFromLogin] = useState(false);
  const [activeTab, setActiveTab] = useState('dashboard');
  const [setupComplete, setSetupComplete] = useState(false);
  const [checkingSetup, setCheckingSetup] = useState(true);
  const [paymentWindow, setPaymentWindow] = useState<PaymentWindowState>({
    isOpen: false,
    checkoutUrl: '',
    planName: '',
    planPrice: 0
  });

  // Check setup status on app initialization
  useEffect(() => {
    const checkSetup = async () => {
      try {
        const { invoke } = await import('@tauri-apps/api/core');
        const status: any = await invoke('check_setup_status');
        setSetupComplete(!status.needs_setup);
      } catch (err) {
        console.error('Failed to check setup status:', err);
        // If check fails, assume setup is needed
        setSetupComplete(false);
      } finally {
        setCheckingSetup(false);
      }
    };

    checkSetup();
  }, []);

  // Set up payment window manager on app initialization
  useEffect(() => {
    const paymentManager = {
      openPaymentWindow: (
        url: string,
        planName: string,
        planPrice: number
      ): Promise<boolean> => {
        return new Promise((resolve) => {
          console.log('PaymentManager: Opening payment window for:', planName);

          setPaymentWindow({
            isOpen: true,
            checkoutUrl: url,
            planName,
            planPrice,
            onSuccess: () => {
              console.log('PaymentManager: Payment completed successfully');
              setPaymentWindow(prev => ({ ...prev, isOpen: false }));
              setCurrentScreen('paymentProcessing');
              resolve(true);
            },
            onCancel: () => {
              console.log('PaymentManager: Payment cancelled by user');
              setPaymentWindow(prev => ({ ...prev, isOpen: false }));
              setCurrentScreen('pricing');
              resolve(false);
            },
            onError: (error: string) => {
              console.error('PaymentManager: Payment error:', error);
              setPaymentWindow(prev => ({ ...prev, isOpen: false }));
              setCurrentScreen('pricing');
              resolve(false);
            }
          });
        });
      }
    };

    setPaymentWindowManager(paymentManager);
    console.log('PaymentManager: Initialized in-app payment window manager');
  }, []);

  // Cleanup running workflows on app unmount
  useEffect(() => {
    return () => {
      console.log('[App] App unmounting, cleaning up running workflows...');
      workflowService.cleanupRunningWorkflows().catch((error: Error) => {
        console.error('[App] Failed to cleanup running workflows:', error);
      });
    };
  }, []);

  // Handle payment success URL parameters
  useEffect(() => {
    const urlParams = new URLSearchParams(window.location.search);
    const sessionId = urlParams.get('session_id');
    const paymentId = urlParams.get('payment_id');

    // If we have payment parameters, go directly to success screen (don't wait for session)
    if (sessionId || paymentId) {
      console.log('Payment success URL detected, redirecting to payment success screen');
      setCurrentScreen('paymentSuccess');
      // Clear URL parameters to prevent issues on refresh
      window.history.replaceState({}, document.title, window.location.pathname);
      return;
    }
  }, []); // Run only once on mount

  // Auto-navigation based on authentication state
  useEffect(() => {
    console.log('App.tsx useEffect triggered:', {
      session: session?.authenticated,
      isLoading,
      currentScreen,
      userType: session?.user_type,
      accountType: session?.user_info?.account_type,
      hasSubscription: session?.subscription?.has_subscription,
      cameFromLogin,
      hasChosenFreePlan
    });

    if (!isLoading) {
      if (session?.authenticated) {
        console.log('Session is authenticated');

        // Check if user is on free plan and needs to see pricing
        const isFreePlan = session.user_info?.account_type === 'free';
        const hasActiveSubscription = (session.subscription as any)?.data?.has_subscription || session.subscription?.has_subscription;

        // Show pricing if:
        // 1. User is registered with free account type AND
        // 2. No active subscription AND
        // 3. Either came from fresh login OR hasn't chosen free plan yet
        if (session.user_type === 'registered' &&
            isFreePlan &&
            !hasActiveSubscription &&
            currentScreen !== 'pricing' &&
            currentScreen !== 'paymentProcessing' &&
            currentScreen !== 'paymentSuccess' &&
            (cameFromLogin || !hasChosenFreePlan)) {
          console.log('Free user without subscription detected, showing pricing screen');
          setCurrentScreen('pricing');
        } else if (hasActiveSubscription ||
                   session.user_type === 'guest' ||
                   (hasChosenFreePlan && !cameFromLogin) ||
                   (isFreePlan && hasChosenFreePlan)) {
          console.log('User has subscription, is guest, or chose free plan - redirecting to dashboard');
          // Don't force redirect if user manually navigated to pricing
          if (currentScreen !== 'paymentProcessing' &&
              currentScreen !== 'paymentSuccess' &&
              currentScreen !== 'pricing') {
            setCurrentScreen('dashboard');
          }
        }
      } else {
        console.log('Session not authenticated, staying on auth screens');
        // Don't redirect if on payment success screen - let it handle authentication internally
        if (currentScreen === 'dashboard' ||
            currentScreen === 'pricing' ||
            currentScreen === 'paymentProcessing') {
          setCurrentScreen('login');
        }
        // Don't reset flags or redirect from paymentSuccess - it needs to complete first
        if (currentScreen !== 'paymentSuccess') {
          setHasChosenFreePlan(false);
          setCameFromLogin(false);
        }
      }
    }
  }, [session, isLoading, currentScreen, cameFromLogin, hasChosenFreePlan]);

  // Show setup screen if setup not complete
  if (checkingSetup) {
    return (
      <div className="min-h-screen bg-black relative overflow-hidden flex items-center justify-center" style={{ minHeight: '100vh', height: '100%' }}>
        <BackgroundPattern />
        <div className="relative z-10 text-center">
          <div className="w-8 h-8 border-2 border-white/30 border-t-white rounded-full animate-spin mx-auto mb-4"></div>
          <p className="text-zinc-400 text-sm">Initializing Fincept Terminal...</p>
        </div>
      </div>
    );
  }

  if (!setupComplete) {
    return <SetupScreen onSetupComplete={() => setSetupComplete(true)} />;
  }

  // Show loading screen while checking authentication
  if (isLoading) {
    return (
      <div className="min-h-screen bg-black relative overflow-hidden flex items-center justify-center" style={{ minHeight: '100vh', height: '100%' }}>
        <BackgroundPattern />
        <div className="relative z-10 text-center">
          <div className="w-8 h-8 border-2 border-white/30 border-t-white rounded-full animate-spin mx-auto mb-4"></div>
          <p className="text-zinc-400 text-sm">Initializing Fincept Terminal...</p>
        </div>
      </div>
    );
  }

  // If authenticated and should show dashboard
  if (session?.authenticated && currentScreen === 'dashboard') {
    return (
      <>
        <ProviderProvider>
          <DataSourceProvider>
            <ThemeProvider>
              <NavigationProvider onNavigate={setCurrentScreen} onSetActiveTab={setActiveTab}>
                <DashboardScreen />
              </NavigationProvider>
            </ThemeProvider>
          </DataSourceProvider>
        </ProviderProvider>
        {/* In-App Payment Window Overlay */}
        <PaymentOverlay paymentWindow={paymentWindow} />
      </>
    );
  }

  // Payment processing and success screens (full screen, no header/footer)
  if (currentScreen === 'paymentProcessing' || currentScreen === 'paymentSuccess') {
    return (
      <div className="min-h-screen bg-black relative overflow-hidden flex items-center justify-center" style={{ minHeight: '100vh', height: '100%' }}>
        <BackgroundPattern />
        <div className="relative z-10 flex items-center justify-center">
          {renderPaymentScreen()}
        </div>
        {/* In-App Payment Window Overlay */}
        <PaymentOverlay paymentWindow={paymentWindow} />
      </div>
    );
  }

  // Payment screen renderer
  function renderPaymentScreen() {
    switch (currentScreen) {
      case 'paymentProcessing':
        return (
          <PaymentProcessingScreen
            onNavigate={setCurrentScreen}
            onPaymentComplete={() => {
              setHasChosenFreePlan(false); // Reset since they now have a paid plan
              setCameFromLogin(false);
              setCurrentScreen('dashboard');
            }}
          />
        );
      case 'paymentSuccess':
        return (
          <PaymentSuccessScreen
            onNavigate={setCurrentScreen}
            onComplete={() => {
              setHasChosenFreePlan(false); // Reset since they now have a paid plan
              setCameFromLogin(false);
              setCurrentScreen('dashboard');
            }}
          />
        );
      default:
        return null;
    }
  }

  // Authentication, pricing, and info screens with header/footer
  const renderCurrentScreen = () => {
    switch (currentScreen) {
      case 'login':
        return <LoginScreen onNavigate={(screen) => {
          if (screen === 'dashboard') {
            // User successfully logged in via login screen
            setCameFromLogin(true);
          }
          setCurrentScreen(screen);
        }} />;

      case 'register':
        return <RegisterScreen onNavigate={setCurrentScreen} />;

      case 'forgotPassword':
        return <ForgotPasswordScreen onNavigate={setCurrentScreen} />;

      case 'help':
        return <HelpScreen onNavigate={setCurrentScreen} />;

      case 'pricing':
        return (
          <PricingScreen
            onNavigate={(screen) => {
              // Handle navigation from pricing screen
              if (screen === 'paymentProcessing') {
                setCurrentScreen('paymentProcessing');
              } else {
                setCurrentScreen(screen);
              }
            }}
            onProceedToDashboard={() => {
              setHasChosenFreePlan(true);
              setCameFromLogin(false); // Reset after choice is made
              setCurrentScreen('dashboard');
            }}
            userType="existing"
          />
        );

      case 'contactUs':
        return <ContactUsScreen onNavigate={setCurrentScreen} />;

      case 'termsOfService':
        return <TermsOfServiceScreen onNavigate={setCurrentScreen} />;

      case 'trademarks':
        return <TrademarksScreen onNavigate={setCurrentScreen} />;

      case 'privacyPolicy':
        return <PrivacyPolicyScreen onNavigate={setCurrentScreen} />;

      default:
        return <LoginScreen onNavigate={setCurrentScreen} />;
    }
  };

  return (
    <div className="min-h-screen bg-black relative overflow-hidden flex flex-col" style={{ minHeight: '100vh', height: '100%' }}>
      <BackgroundPattern />
      <Header onNavigate={setCurrentScreen} />

      <div className="relative z-10 flex items-center justify-center flex-1">
        {renderCurrentScreen()}
      </div>

      <Footer onNavigate={setCurrentScreen} />

      {/* In-App Payment Window Overlay - Available on all screens */}
      <PaymentOverlay paymentWindow={paymentWindow} />
    </div>
  );
};

export default App;