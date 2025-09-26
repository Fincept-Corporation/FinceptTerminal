// File: src/App.tsx
// Main application file that handles routing between authentication screens and dashboard

import React, { useState, useEffect } from 'react';
import { useAuth } from './contexts/AuthContext';
import LoginScreen from './components/auth/LoginScreen';
import RegisterScreen from './components/auth/RegisterScreen';
import ForgotPasswordScreen from './components/auth/ForgotPasswordScreen';
import HelpScreen from './components/auth/HelpScreen';
import ContactUsScreen from './components/info/ContactUsScreen';
import TermsOfServiceScreen from './components/info/TermsOfServiceScreen';
import TrademarksScreen from './components/info/TrademarksScreen';
import PrivacyPolicyScreen from './components/info/PrivacyPolicyScreen';
import DashboardScreen from './components/dashboard/DashboardScreen';
import BackgroundPattern from './components/common/BackgroundPattern';
import Header from './components/common/Header';
import Footer from './components/common/Footer';

export type Screen =
  | 'login'
  | 'register'
  | 'forgotPassword'
  | 'help'
  | 'contactUs'
  | 'termsOfService'
  | 'trademarks'
  | 'privacyPolicy'
  | 'dashboard';

const App: React.FC = () => {
  const { session, isLoading } = useAuth();
  const [currentScreen, setCurrentScreen] = useState<Screen>('login');

  // Auto-navigation based on authentication state
  useEffect(() => {
    console.log('App.tsx useEffect triggered:', {
      session: session?.authenticated,
      isLoading,
      currentScreen,
      userType: session?.user_type
    });

    if (!isLoading) {
      if (session?.authenticated) {
        console.log('Session is authenticated, redirecting to dashboard');
        setCurrentScreen('dashboard');
      } else {
        console.log('Session not authenticated, staying on auth screens');
        if (currentScreen === 'dashboard') {
          setCurrentScreen('login');
        }
      }
    }
  }, [session, isLoading, currentScreen]);

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

  // If authenticated, show dashboard without header/footer
  if (session?.authenticated && currentScreen === 'dashboard') {
    return <DashboardScreen />;
  }

  // Authentication and info screens with header/footer
  const renderCurrentScreen = () => {
    switch (currentScreen) {
      case 'login':
        return <LoginScreen onNavigate={setCurrentScreen} />;
      case 'register':
        return <RegisterScreen onNavigate={setCurrentScreen} />;
      case 'forgotPassword':
        return <ForgotPasswordScreen onNavigate={setCurrentScreen} />;
      case 'help':
        return <HelpScreen onNavigate={setCurrentScreen} />;
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
    </div>
  );
};

export default App;