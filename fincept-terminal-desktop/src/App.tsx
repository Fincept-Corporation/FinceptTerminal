// File: src/App.tsx
// Main application file that handles routing between authentication screens, payment flow, and dashboard

import React, { useState, useEffect } from 'react';
import { useAuth } from './contexts/AuthContext';
import { NavigationProvider } from './contexts/NavigationContext';
import { ThemeProvider } from './contexts/ThemeContext';
import { DataSourceProvider } from './contexts/DataSourceContext';
import { ProviderProvider } from './contexts/ProviderContext';
import { workflowService } from './services/core/workflowService';
import { initializeNodeSystem } from './services/nodeSystem';
import { initializeStockBrokers } from './brokers/stocks';

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
import SetupScreen from './components/setup/SetupScreen';
import { Toaster } from 'sonner';
import { NotificationProvider } from './utils/notifications';

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

const App: React.FC = () => {
  const { session, isLoading } = useAuth();
  const [currentScreen, setCurrentScreen] = useState<Screen>('login');
  const [hasChosenFreePlan, setHasChosenFreePlan] = useState(false);
  const [cameFromLogin, setCameFromLogin] = useState(false);
  const [activeTab, setActiveTab] = useState('dashboard');
  const [setupComplete, setSetupComplete] = useState(false);
  const [checkingSetup, setCheckingSetup] = useState(true);

  // Check setup status on app initialization and initialize node system
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

    // Initialize node system (loads all nodes into registry)
    initializeNodeSystem().catch((err) => {
      console.error('Failed to initialize node system:', err);
    });

    // Initialize stock broker adapters
    try {
      initializeStockBrokers();
    } catch (err) {
      console.error('Failed to initialize stock brokers:', err);
    }

    checkSetup();
  }, []);

  // Payment window manager no longer needed - using external browser
  // Removed in-app payment window code

  // Cleanup running workflows on app unmount
  useEffect(() => {
    return () => {
      workflowService.cleanupRunningWorkflows().catch((error: Error) => {
        console.error('Failed to cleanup running workflows:', error);
      });
    };
  }, []);

  // Handle payment success URL parameters
  useEffect(() => {
    const urlParams = new URLSearchParams(window.location.search);
    const sessionId = urlParams.get('session_id');
    const paymentId = urlParams.get('payment_id');

    if (sessionId || paymentId) {
      setCurrentScreen('paymentSuccess');
      window.history.replaceState({}, document.title, window.location.pathname);
      return;
    }
  }, []);

  // Auto-navigation based on authentication state
  useEffect(() => {
    if (!isLoading) {
      if (session?.authenticated) {
        const isFreePlan = session.user_info?.account_type === 'free';
        const hasPaidPlan = session.user_info?.account_type &&
                           ['basic', 'standard', 'pro', 'enterprise'].includes(session.user_info.account_type);

        // If user has a paid plan (basic, standard, pro, enterprise), go to dashboard
        if (session.user_type === 'registered' && hasPaidPlan) {
          if (currentScreen !== 'paymentProcessing' &&
              currentScreen !== 'paymentSuccess' &&
              currentScreen !== 'pricing' &&
              currentScreen !== 'dashboard') {
            setCurrentScreen('dashboard');
          }
        }
        // If free plan user just logged in, show pricing
        else if (session.user_type === 'registered' &&
            isFreePlan &&
            currentScreen !== 'pricing' &&
            currentScreen !== 'paymentProcessing' &&
            currentScreen !== 'paymentSuccess' &&
            (cameFromLogin || !hasChosenFreePlan)) {
          setCurrentScreen('pricing');
        }
        // Guest users or free plan users who chose to continue with free
        else if (session.user_type === 'guest' ||
                   (hasChosenFreePlan && !cameFromLogin) ||
                   (isFreePlan && hasChosenFreePlan)) {
          if (currentScreen !== 'paymentProcessing' &&
              currentScreen !== 'paymentSuccess' &&
              currentScreen !== 'pricing') {
            setCurrentScreen('dashboard');
          }
        }
      } else {
        if (currentScreen === 'dashboard' ||
            currentScreen === 'pricing' ||
            currentScreen === 'paymentProcessing') {
          setCurrentScreen('login');
        }
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
      <NotificationProvider>
        <ProviderProvider>
          <DataSourceProvider>
            <ThemeProvider>
              <NavigationProvider onNavigate={setCurrentScreen} onSetActiveTab={setActiveTab} activeTab={activeTab}>
                <DashboardScreen />
              </NavigationProvider>
            </ThemeProvider>
          </DataSourceProvider>
        </ProviderProvider>
        {/* Toast Notifications */}
        <Toaster position="top-right" richColors closeButton />
      </NotificationProvider>
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
    <div className="h-screen bg-black relative overflow-hidden flex flex-col">
      <BackgroundPattern />
      <Header onNavigate={setCurrentScreen} />

      <div className="relative z-10 flex items-center justify-center flex-1 overflow-hidden">
        {renderCurrentScreen()}
      </div>

      <Footer onNavigate={setCurrentScreen} />
    </div>
  );
};

export default App;