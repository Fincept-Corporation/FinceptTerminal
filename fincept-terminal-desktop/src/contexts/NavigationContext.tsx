// File: src/contexts/NavigationContext.tsx
// Reusable navigation context for sharing navigation functions across components

import React, { createContext, useContext, ReactNode } from 'react';
import { Screen } from '../App';

interface NavigationContextType {
  navigateToScreen: (screen: Screen) => void;
  navigateToPricing: () => void;
  navigateToDashboard: () => void;
  navigateToProfile: () => void;
  setActiveTab?: (tab: string) => void;
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
}

export const NavigationProvider: React.FC<NavigationProviderProps> = ({
  children,
  onNavigate,
  onSetActiveTab
}) => {
  const navigateToScreen = (screen: Screen) => {
    onNavigate(screen);
  };

  const navigateToPricing = () => {
    onNavigate('pricing');
  };

  const navigateToDashboard = () => {
    onNavigate('dashboard');
  };

  const navigateToProfile = () => {
    onNavigate('dashboard');
    if (onSetActiveTab) {
      // Set active tab to profile after a short delay to ensure dashboard is mounted
      setTimeout(() => onSetActiveTab('profile'), 100);
    }
  };

  const value: NavigationContextType = {
    navigateToScreen,
    navigateToPricing,
    navigateToDashboard,
    navigateToProfile,
    setActiveTab: onSetActiveTab
  };

  return (
    <NavigationContext.Provider value={value}>
      {children}
    </NavigationContext.Provider>
  );
};
