import React, { useState } from 'react';
import { Calculator, TrendingUp, BarChart2, Combine } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';

interface ValuationPanelProps {
  selectedDeal: any;
}

export const ValuationPanel: React.FC<ValuationPanelProps> = ({ selectedDeal }) => {
  const [valuationMethod, setValuationMethod] = useState<'dcf' | 'comps' | 'precedent' | 'football'>('dcf');

  return (
    <div style={{ padding: SPACING.LARGE }}>
      <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>VALUATION ANALYSIS</div>

      {/* Method Selector */}
      <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.LARGE }}>
        {[
          { id: 'dcf', label: 'DCF', icon: Calculator },
          { id: 'comps', label: 'TRADING COMPS', icon: BarChart2 },
          { id: 'precedent', label: 'PRECEDENT TX', icon: TrendingUp },
          { id: 'football', label: 'FOOTBALL FIELD', icon: Combine },
        ].map((method) => {
          const Icon = method.icon;
          return (
            <button
              key={method.id}
              onClick={() => setValuationMethod(method.id as any)}
              style={{
                ...COMMON_STYLES.tabButton(valuationMethod === method.id),
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.TINY,
              }}
            >
              <Icon size={12} />
              {method.label}
            </button>
          );
        })}
      </div>

      <div style={COMMON_STYLES.emptyState}>
        <Calculator size={48} style={{ opacity: 0.3, marginBottom: SPACING.DEFAULT }} />
        <div style={{ fontSize: TYPOGRAPHY.BODY, color: FINCEPT.GRAY }}>
          Valuation Module - Under Construction
        </div>
        <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, marginTop: SPACING.SMALL }}>
          DCF, Trading Comps, Precedent Transactions, Football Field
        </div>
      </div>
    </div>
  );
};
