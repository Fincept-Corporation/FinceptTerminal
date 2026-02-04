import React, { useState, useEffect , useCallback } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { TrendingDown, Building2, Shield, Gem, Lock, Calendar, Package, ShieldCheck, Target, Bitcoin, ChevronDown, ChevronUp, Activity } from 'lucide-react';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { BondsView } from './categories/BondsView';
import { RealEstateView } from './categories/RealEstateView';
import { HedgeFundsView } from './categories/HedgeFundsView';
import { CommoditiesView } from './categories/CommoditiesView';
import { PrivateCapitalView } from './categories/PrivateCapitalView';
import { AnnuitiesView } from './categories/AnnuitiesView';
import { StructuredProductsView } from './categories/StructuredProductsView';
import { InflationProtectedView } from './categories/InflationProtectedView';
import { StrategiesView } from './categories/StrategiesView';
import { CryptoView } from './categories/CryptoView';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  GREEN: '#00D66F',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  RED: '#FF4444',
  PURPLE: '#9D4EDD',
};

type CategoryView = 'bonds' | 'real-estate' | 'hedge-funds' | 'commodities' | 'private-capital' | 'annuities' | 'structured' | 'inflation-protected' | 'strategies' | 'crypto';

interface CategoryItem {
  id: CategoryView;
  name: string;
  shortName: string;
  icon: React.ComponentType<{ size?: number; color?: string }>;
  color: string;
  analyzerCount: number;
  description: string;
  category: 'Fixed Income' | 'Real Assets' | 'Alternatives' | 'Structured';
}

const CATEGORIES: CategoryItem[] = [
  { id: 'bonds', name: 'Bonds & Fixed Income', shortName: 'BONDS', icon: TrendingDown, color: FINCEPT.ORANGE, analyzerCount: 4, description: 'High-yield, EM, convertible, preferred', category: 'Fixed Income' },
  { id: 'real-estate', name: 'Real Estate', shortName: 'REALESTATE', icon: Building2, color: FINCEPT.GREEN, analyzerCount: 2, description: 'Direct property, REITs', category: 'Real Assets' },
  { id: 'hedge-funds', name: 'Hedge Funds', shortName: 'HEDGE', icon: Shield, color: FINCEPT.CYAN, analyzerCount: 3, description: 'Long/short, managed futures, market neutral', category: 'Alternatives' },
  { id: 'commodities', name: 'Commodities', shortName: 'COMMODITIES', icon: Gem, color: FINCEPT.YELLOW, analyzerCount: 2, description: 'Precious metals, natural resources', category: 'Real Assets' },
  { id: 'private-capital', name: 'Private Capital', shortName: 'PE/VC', icon: Lock, color: FINCEPT.PURPLE, analyzerCount: 1, description: 'Private equity, venture capital', category: 'Alternatives' },
  { id: 'annuities', name: 'Annuities', shortName: 'ANNUITIES', icon: Calendar, color: FINCEPT.CYAN, analyzerCount: 4, description: 'Fixed, variable, equity-indexed, inflation', category: 'Structured' },
  { id: 'structured', name: 'Structured Products', shortName: 'STRUCTURED', icon: Package, color: FINCEPT.RED, analyzerCount: 2, description: 'Structured notes, leveraged funds', category: 'Structured' },
  { id: 'inflation-protected', name: 'Inflation Protected', shortName: 'TIPS', icon: ShieldCheck, color: FINCEPT.GREEN, analyzerCount: 3, description: 'TIPS, I-Bonds, stable value', category: 'Fixed Income' },
  { id: 'strategies', name: 'Strategies', shortName: 'STRATEGIES', icon: Target, color: FINCEPT.ORANGE, analyzerCount: 5, description: 'Covered calls, SRI, asset location, performance, risk', category: 'Alternatives' },
  { id: 'crypto', name: 'Digital Assets', shortName: 'CRYPTO', icon: Bitcoin, color: FINCEPT.CYAN, analyzerCount: 1, description: 'Cryptocurrency analysis', category: 'Alternatives' },
];

export const AlternativeInvestmentsTab: React.FC = () => {
  const [activeCategory, setActiveCategory] = useState<CategoryView>('bonds');
  const [isLeftPanelMinimized, setIsLeftPanelMinimized] = useState(false);

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    activeCategory, isLeftPanelMinimized,
  }), [activeCategory, isLeftPanelMinimized]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.activeCategory === "string") setActiveCategory(state.activeCategory as any);
    if (typeof state.isLeftPanelMinimized === "boolean") setIsLeftPanelMinimized(state.isLeftPanelMinimized);
  }, []);

  useWorkspaceTabState("alternative-investments", getWorkspaceState, setWorkspaceState);
  const [currentTime, setCurrentTime] = useState(new Date());

  const activeItem = CATEGORIES.find(c => c.id === activeCategory) || CATEGORIES[0];

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', backgroundColor: FINCEPT.DARK_BG, fontFamily: '"IBM Plex Mono", monospace' }}>

      {/* TOP BAR */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        height: '48px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', padding: '4px 12px', backgroundColor: FINCEPT.ORANGE }}>
            <Activity size={16} color={FINCEPT.DARK_BG} />
            <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.DARK_BG, letterSpacing: '1px' }}>
              ALTERNATIVE INVESTMENTS
            </span>
          </div>
          <div style={{ height: '20px', width: '1px', backgroundColor: FINCEPT.BORDER }} />
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
            CFA-Level Analytics • 27 Analyzers
          </div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ fontSize: '10px', fontFamily: 'monospace', color: FINCEPT.GRAY }}>
            {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </div>
        </div>
      </div>

      {/* 3-PANEL LAYOUT */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>

        {/* LEFT PANEL */}
        {!isLeftPanelMinimized && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            width: '200px',
          }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '8px 12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '1px' }}>CATEGORIES</span>
              <button onClick={() => setIsLeftPanelMinimized(true)} style={{ padding: '2px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }} title="Minimize panel">
                <ChevronUp size={12} color={FINCEPT.GRAY} />
              </button>
            </div>
            <div style={{ flex: 1, overflowY: 'auto', padding: '8px' }}>
              {['Fixed Income', 'Real Assets', 'Alternatives', 'Structured'].map((categoryGroup) => (
                <div key={categoryGroup} style={{ marginBottom: '12px' }}>
                  <div style={{ padding: '4px 8px', fontSize: '9px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>
                    {categoryGroup}
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
                    {CATEGORIES.filter(c => c.category === categoryGroup).map((cat) => {
                      const isActive = activeCategory === cat.id;
                      return (
                        <button
                          key={cat.id}
                          onClick={() => setActiveCategory(cat.id)}
                          style={{
                            display: 'flex',
                            alignItems: 'center',
                            gap: '8px',
                            padding: '8px',
                            backgroundColor: isActive ? `${cat.color}20` : 'transparent',
                            border: 'none',
                            borderLeft: isActive ? `2px solid ${cat.color}` : '2px solid transparent',
                            cursor: 'pointer',
                            transition: 'all 0.2s',
                          }}
                          onMouseEnter={(e) => {
                            if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                          }}
                          onMouseLeave={(e) => {
                            if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
                          }}
                        >
                          <cat.icon size={13} color={isActive ? cat.color : FINCEPT.GRAY} />
                          <div style={{ flex: 1, textAlign: 'left' }}>
                            <div style={{ fontSize: '9px', fontWeight: 600, color: isActive ? cat.color : FINCEPT.GRAY }}>
                              {cat.shortName}
                            </div>
                            <div style={{ fontSize: '7px', color: FINCEPT.MUTED }}>
                              {cat.analyzerCount} tools
                            </div>
                          </div>
                        </button>
                      );
                    })}
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {isLeftPanelMinimized && (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', backgroundColor: FINCEPT.PANEL_BG, borderRight: `1px solid ${FINCEPT.BORDER}`, width: '32px' }}>
            <button onClick={() => setIsLeftPanelMinimized(false)} style={{ padding: '4px', marginTop: '4px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }} title="Expand panel">
              <ChevronDown size={12} color={FINCEPT.GRAY} />
            </button>
          </div>
        )}

        {/* CENTER PANEL */}
        <div style={{ display: 'flex', flexDirection: 'column', flex: 1, overflow: 'hidden' }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '8px 12px', backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <activeItem.icon size={14} color={activeItem.color} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
                {activeItem.name}
              </span>
            </div>
            <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
              {activeItem.category}
            </div>
          </div>
          <div style={{ flex: 1, overflow: 'auto' }}>
            {activeCategory === 'bonds' && <BondsView />}
            {activeCategory === 'real-estate' && <RealEstateView />}
            {activeCategory === 'hedge-funds' && <HedgeFundsView />}
            {activeCategory === 'commodities' && <CommoditiesView />}
            {activeCategory === 'private-capital' && <PrivateCapitalView />}
            {activeCategory === 'annuities' && <AnnuitiesView />}
            {activeCategory === 'structured' && <StructuredProductsView />}
            {activeCategory === 'inflation-protected' && <InflationProtectedView />}
            {activeCategory === 'strategies' && <StrategiesView />}
            {activeCategory === 'crypto' && <CryptoView />}
          </div>
        </div>


      </div>

      {/* BOTTOM STATUS BAR */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '6px 16px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
      }}>
        <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>
          Active: {activeItem.name} • {activeItem.analyzerCount} analyzer{activeItem.analyzerCount > 1 ? 's' : ''} available
        </div>
        <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
          Python Analytics Engine v3.1
        </div>
      </div>

    </div>
  );
};

const AlternativeInvestmentsTabWithBoundary: React.FC = () => (
  <ErrorBoundary name="AlternativeInvestmentsTab" variant="minimal">
    <AlternativeInvestmentsTab />
  </ErrorBoundary>
);

export default AlternativeInvestmentsTabWithBoundary;
