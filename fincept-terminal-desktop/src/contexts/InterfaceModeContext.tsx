// File: src/contexts/InterfaceModeContext.tsx
// Context for managing interface mode switching between Terminal and Chat modes

import React, { createContext, useContext, useState, ReactNode } from 'react';

export type InterfaceMode = 'terminal' | 'chat';

interface InterfaceModeContextType {
  mode: InterfaceMode;
  setMode: (mode: InterfaceMode) => void;
  toggleMode: () => void;
}

const InterfaceModeContext = createContext<InterfaceModeContextType | undefined>(undefined);

export const useInterfaceMode = () => {
  const context = useContext(InterfaceModeContext);
  if (!context) {
    throw new Error('useInterfaceMode must be used within InterfaceModeProvider');
  }
  return context;
};

interface InterfaceModeProviderProps {
  children: ReactNode;
}

export const InterfaceModeProvider: React.FC<InterfaceModeProviderProps> = ({ children }) => {
  const [mode, setMode] = useState<InterfaceMode>('terminal');

  const toggleMode = () => {
    setMode(prev => prev === 'terminal' ? 'chat' : 'terminal');
  };

  return (
    <InterfaceModeContext.Provider value={{ mode, setMode, toggleMode }}>
      {children}
    </InterfaceModeContext.Provider>
  );
};
