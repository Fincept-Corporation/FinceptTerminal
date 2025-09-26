// File: src/components/common/Footer.tsx
// Application footer with copyright and navigation links

import React from 'react';
import { Screen } from '../../App';

interface FooterProps {
  onNavigate: (screen: Screen) => void;
}

const Footer: React.FC<FooterProps> = ({ onNavigate }) => {
  return (
    <div className="relative z-10 p-4 text-xs text-zinc-500 flex justify-between items-center mt-auto border-t border-zinc-800">
      <div>© 2025 Fincept LP All rights reserved.</div>
      <div className="flex space-x-4">
        <button
          onClick={() => onNavigate('contactUs')}
          className="hover:text-zinc-400 transition-colors"
        >
          Contact Us
        </button>
        <button
          onClick={() => onNavigate('termsOfService')}
          className="hover:text-zinc-400 transition-colors"
        >
          Terms of Service
        </button>
        <button
          onClick={() => onNavigate('trademarks')}
          className="hover:text-zinc-400 transition-colors"
        >
          Trademarks
        </button>
        <button
          onClick={() => onNavigate('privacyPolicy')}
          className="hover:text-zinc-400 transition-colors"
        >
          Privacy Policy
        </button>
        <button
          onClick={() => onNavigate('help')}
          className="hover:text-zinc-400 transition-colors"
        >
          Help Center
        </button>
      </div>
    </div>
  );
};

export default Footer;