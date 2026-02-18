import React from 'react';
import {
  BarChart3, Activity, Target, Layers,
  FileText, Shield, Maximize2, TrendingUp, Compass,
} from 'lucide-react';
import { FINCEPT } from '../finceptStyles';
import type { BottomPanelTab, DetailView } from '../types';

interface BottomPanelTabsProps {
  onOpenDetail: (view: DetailView) => void;
}

const TABS: { id: BottomPanelTab; label: string; icon: React.ElementType }[] = [
  { id: 'analytics-sectors', label: 'ANALYTICS', icon: BarChart3 },
  { id: 'perf-risk',         label: 'PERF & RISK', icon: Shield },
  { id: 'optimization',      label: 'OPTIMIZE', icon: Target },
  { id: 'quantstats',        label: 'QUANTSTATS', icon: Activity },
  { id: 'reports-pme',       label: 'REPORTS', icon: FileText },
  { id: 'indices',           label: 'INDICES', icon: Layers },
  { id: 'risk-mgmt',         label: 'RISK MGMT', icon: Shield },
  { id: 'planning',          label: 'PLANNING', icon: Compass },
  { id: 'economics',         label: 'ECONOMICS', icon: TrendingUp },
];

const BottomPanelTabs: React.FC<BottomPanelTabsProps> = ({ onOpenDetail }) => (
  <div style={{ height: '28px', flexShrink: 0, display: 'flex', alignItems: 'stretch', borderTop: `1px solid ${FINCEPT.ORANGE}40`, backgroundColor: '#0A0A0A', overflowX: 'auto', overflowY: 'hidden' }}>
    {TABS.map(tab => {
      const Icon = tab.icon;
      return (
        <button
          key={tab.id}
          onClick={() => onOpenDetail(tab.id as DetailView)}
          style={{
            padding: '0 10px', whiteSpace: 'nowrap', backgroundColor: 'transparent',
            borderBottom: '2px solid transparent', border: 'none',
            color: FINCEPT.CYAN, cursor: 'pointer',
            fontSize: '8px', fontWeight: 700, letterSpacing: '0.3px',
            display: 'flex', alignItems: 'center', gap: '3px', flexShrink: 0,
          }}
          onMouseEnter={e => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; e.currentTarget.style.color = FINCEPT.WHITE; }}
          onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; e.currentTarget.style.color = FINCEPT.CYAN; }}
        >
          <Icon size={8} />
          {tab.label}
          <Maximize2 size={6} style={{ opacity: 0.5 }} />
        </button>
      );
    })}
  </div>
);

export default BottomPanelTabs;
