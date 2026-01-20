/**
 * Header Component
 * Top header bar with title and status
 */

import React from 'react';
import { Shield, CheckCircle2 } from 'lucide-react';
import { FINCEPT } from '../constants';

export const Header: React.FC = () => {
  return (
    <div
      className="flex items-center justify-between p-3 rounded border"
      style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
    >
      <div className="flex items-center gap-3">
        <div className="p-2 rounded" style={{ backgroundColor: FINCEPT.ORANGE }}>
          <Shield size={20} color={FINCEPT.DARK_BG} />
        </div>
        <div>
          <h2
            className="font-bold text-sm uppercase tracking-wide"
            style={{ color: FINCEPT.WHITE }}
          >
            FORTITUDO.TECH RISK ANALYTICS
          </h2>
          <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
            VaR, CVaR, Option Pricing, Entropy Pooling
          </p>
        </div>
      </div>
      <div className="flex items-center gap-2">
        <CheckCircle2 size={16} color={FINCEPT.GREEN} />
        <span className="text-xs font-mono" style={{ color: FINCEPT.GREEN }}>
          READY
        </span>
      </div>
    </div>
  );
};
