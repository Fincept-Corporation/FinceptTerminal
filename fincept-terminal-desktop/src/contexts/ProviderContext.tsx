// File: src/contexts/ProviderContext.tsx
// DEPRECATED: This file is kept for backward compatibility only
// Use BrokerContext instead for new code

import React, { ReactNode } from 'react';
import { BrokerProvider, useBrokerContext } from './BrokerContext';

// Re-export for backward compatibility
export interface ProviderContextType {
  activeProvider: string;
  availableProviders: string[];
  setActiveProvider: (provider: string) => Promise<void>;
  providerConfigs: Map<string, any>;
  updateProviderConfig: (provider: string, config: Partial<any>) => Promise<void>;
  isLoading: boolean;
}

interface ProviderProviderProps {
  children: ReactNode;
}

// Wrapper that maps BrokerContext to old ProviderContext interface
export function ProviderProvider({ children }: ProviderProviderProps) {
  return <BrokerProvider>{children}</BrokerProvider>;
}

export function useProviderContext(): ProviderContextType {
  const {
    activeBroker,
    availableBrokers,
    setActiveBroker,
    providerConfigs,
    updateProviderConfig,
    isLoading,
  } = useBrokerContext();

  return {
    activeProvider: activeBroker,
    availableProviders: availableBrokers.map(b => b.id),
    setActiveProvider: setActiveBroker,
    providerConfigs,
    updateProviderConfig,
    isLoading,
  };
}

// Legacy implementation below - kept for reference but not used
/*
import React, { createContext, useContext, useState, useEffect, useCallback, ReactNode } from 'react';
import { getWebSocketManager } from '../services/websocket';
import type { ProviderConfig } from '../services/websocket/types';
import { getAvailableProviders } from '../services/websocket/adapters';

interface ProviderContextType {
  activeProvider: string;
  availableProviders: string[];
  setActiveProvider: (provider: string) => Promise<void>;
  providerConfigs: Map<string, ProviderConfig>;
  updateProviderConfig: (provider: string, config: Partial<ProviderConfig>) => Promise<void>;
  isLoading: boolean;
}

const ProviderContext = createContext<ProviderContextType | null>(null);

interface ProviderProviderProps {
  children: ReactNode;
}

export function ProviderProvider({ children }: ProviderProviderProps) {
*/
