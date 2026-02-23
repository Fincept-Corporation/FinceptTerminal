// File: src/contexts/NavigationContext.tsx
// Reusable navigation context for sharing navigation functions across components

import React, { createContext, useContext, useRef, useEffect, useCallback, ReactNode } from 'react';
import { Screen } from '../App';

interface NavigationContextType {
  navigateToScreen: (screen: Screen) => void;
  navigateToPricing: () => void;
  navigateToDashboard: () => void;
  navigateToProfile: () => void;
  setActiveTab?: (tab: string) => void;
  activeTab?: string;  // Current active tab
}

const NavigationContext = createContext<NavigationContextType | undefined>(undefined);

export const useNavigation = () => {
  const context = useContext(NavigationContext);
  if (!context) {
    throw new Error('useNavigation must be used within NavigationProvider');
  }
  return context;
};

interface NavigationProviderProps {
  children: ReactNode;
  onNavigate: (screen: Screen) => void;
  onSetActiveTab?: (tab: string) => void;
  activeTab?: string;  // Pass activeTab from parent
}

export const NavigationProvider: React.FC<NavigationProviderProps> = ({
  children,
  onNavigate,
  onSetActiveTab,
  activeTab
}) => {
  const profileTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    return () => {
      if (profileTimerRef.current) {
        clearTimeout(profileTimerRef.current);
      }
    };
  }, []);

  const navigateToScreen = useCallback((screen: Screen) => {
    onNavigate(screen);
  }, [onNavigate]);

  const navigateToPricing = useCallback(() => {
    onNavigate('pricing');
  }, [onNavigate]);

  const navigateToDashboard = useCallback(() => {
    onNavigate('dashboard');
  }, [onNavigate]);

  const navigateToProfile = useCallback(() => {
    onNavigate('dashboard');
    if (onSetActiveTab) {
      // Set active tab to profile after a short delay to ensure dashboard is mounted
      if (profileTimerRef.current) clearTimeout(profileTimerRef.current);
      profileTimerRef.current = setTimeout(() => onSetActiveTab('profile'), 100);
    }
  }, [onNavigate, onSetActiveTab]);

  const value: NavigationContextType = {
    navigateToScreen,
    navigateToPricing,
    navigateToDashboard,
    navigateToProfile,
    setActiveTab: onSetActiveTab,
    activeTab
  };

  return (
    <NavigationContext.Provider value={value}>
      {children}
    </NavigationContext.Provider>
  );
};
