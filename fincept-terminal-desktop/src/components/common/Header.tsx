// File: src/components/common/Header.tsx
// Application header with logo and help button

import React from 'react';
import { Screen } from '../../App';

interface HeaderProps {
  onNavigate: (screen: Screen) => void;
}

const Header: React.FC<HeaderProps> = ({ onNavigate }) => {
  return (
    <div className="relative z-10 p-8 flex justify-between items-center">
      <button
        onClick={() => onNavigate('login')}
        className="text-white text-2xl font-bold hover:text-zinc-300 transition-colors"
      >
        Fincept
      </button>

      <button
        onClick={() => onNavigate('help')}
        className="text-zinc-400 hover:text-white transition-colors text-sm flex items-center gap-2"
      >
        <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8.228 9c.549-1.165 2.03-2 3.772-2 2.21 0 4 1.343 4 3 0 1.4-1.278 2.575-3.006 2.907-.542.104-.994.54-.994 1.093m0 3h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
        </svg>
        Help
      </button>
    </div>
  );
};

export default Header;