/**
 * AI Quant Lab Tab - REVAMPED UI/UX
 * Complete Microsoft Qlib + RD-Agent integration
 * Full-featured quantitative research platform with modern UX
 */

import React, { useState, useEffect, useRef, useMemo } from 'react';
import {
  Brain,
  Zap,
  TrendingUp,
  Database,
  RefreshCw,
  Settings,
  BarChart3,
  Activity,
  Sparkles,
  CheckCircle2,
  Clock,
  Sigma,
  Calculator,
  Search,
  ChevronRight,
  Home,
  Layers,
  BookOpen,
  X,
  Info,
  Play,
  Target
} from 'lucide-react';
import { TabFooter } from '@/components/common/TabFooter';
import { qlibService } from '@/services/aiQuantLab/qlibService';
import { rdAgentService } from '@/services/aiQuantLab/rdAgentService';

// Sub-components
import { FactorDiscoveryPanel } from './FactorDiscoveryPanel';
import { ModelLibraryPanel } from './ModelLibraryPanel';
import { BacktestingPanel } from './BacktestingPanel';
import { LiveSignalsPanel } from './LiveSignalsPanel';
import { FFNAnalyticsPanel } from './FFNAnalyticsPanel';
import { FunctimePanel } from './FunctimePanel';
import { FortitudoPanel } from './FortitudoPanel';
import { StatsmodelsPanel } from './StatsmodelsPanel';
import { CFAQuantPanel } from './CFAQuantPanel';
import { DeepAgentPanel } from './DeepAgentPanel';
import { RLTradingPanel } from './RLTradingPanel';
import { OnlineLearningPanel } from './OnlineLearningPanel';
import { HFTPanel } from './HFTPanel';
import { MetaLearningPanel } from './MetaLearningPanel';
import { RollingRetrainingPanel } from './RollingRetrainingPanel';
import { AdvancedModelsPanel } from './AdvancedModelsPanel';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CARD_BG: '#141414'
};

type ViewMode = 'dashboard' | 'factor_discovery' | 'model_library' | 'backtesting' | 'live_signals' | 'ffn_analytics' | 'functime' | 'fortitudo' | 'statsmodels' | 'cfa_quant' | 'deep_agent' | 'rl_trading' | 'online_learning' | 'hft' | 'meta_learning' | 'rolling_retraining' | 'advanced_models';

interface NavItem {
  id: ViewMode;
  label: string;
  icon: React.ComponentType<{ size?: number; color?: string }>;
  description: string;
  category: 'core' | 'ai' | 'advanced' | 'analytics';
  popular?: boolean;
}

export default function AIQuantLabTab() {
  // State
  const [activeView, setActiveView] = useState<ViewMode>('dashboard');
  const [qlibStatus, setQlibStatus] = useState({ available: true, initialized: false });
  const [rdAgentStatus, setRDAgentStatus] = useState({ available: true, initialized: false });
  const [isInitializing, setIsInitializing] = useState(false);
  const [showSettings, setShowSettings] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [sidebarCollapsed, setSidebarCollapsed] = useState(false);
  const initAttemptedRef = useRef(false);

  // Navigation items organized by category
  const navItems: NavItem[] = [
    // Core Modules
    { id: 'factor_discovery', label: 'Factor Discovery', icon: Sparkles, description: 'AI-powered factor mining with RD-Agent', category: 'core', popular: true },
    { id: 'model_library', label: 'Model Library', icon: Brain, description: '15+ pre-trained Qlib models', category: 'core', popular: true },
    { id: 'backtesting', label: 'Backtesting', icon: BarChart3, description: 'Strategy validation & analysis', category: 'core', popular: true },
    { id: 'live_signals', label: 'Live Signals', icon: Activity, description: 'Real-time predictions', category: 'core' },

    // AI & Agents
    { id: 'deep_agent', label: 'Deep Agent', icon: Brain, description: 'Autonomous multi-step workflows', category: 'ai', popular: true },
    { id: 'rl_trading', label: 'RL Trading', icon: Target, description: 'Reinforcement learning agents', category: 'ai' },
    { id: 'online_learning', label: 'Online Learning', icon: Zap, description: 'Real-time model updates', category: 'ai' },
    { id: 'meta_learning', label: 'Meta Learning', icon: Layers, description: 'AutoML & model selection', category: 'ai' },

    // Advanced Trading
    { id: 'hft', label: 'HFT', icon: Activity, description: 'High-frequency trading', category: 'advanced' },
    { id: 'rolling_retraining', label: 'Rolling Retrain', icon: RefreshCw, description: 'Automated retraining', category: 'advanced' },
    { id: 'advanced_models', label: 'Advanced Models', icon: Brain, description: 'Time-series neural networks', category: 'advanced' },

    // Analytics
    { id: 'ffn_analytics', label: 'FFN Analytics', icon: TrendingUp, description: 'Portfolio performance metrics', category: 'analytics' },
    { id: 'functime', label: 'Functime', icon: Zap, description: 'ML time series forecasting', category: 'analytics' },
    { id: 'fortitudo', label: 'Fortitudo', icon: Database, description: 'Risk analytics & derivatives', category: 'analytics' },
    { id: 'statsmodels', label: 'Statsmodels', icon: Sigma, description: 'Statistical modeling', category: 'analytics' },
    { id: 'cfa_quant', label: 'CFA Quant', icon: Calculator, description: 'CFA-level quant analytics', category: 'analytics' }
  ];

  // Filtered nav items based on search
  const filteredNavItems = useMemo(() => {
    if (!searchQuery) return navItems;
    const query = searchQuery.toLowerCase();
    return navItems.filter(item =>
      item.label.toLowerCase().includes(query) ||
      item.description.toLowerCase().includes(query)
    );
  }, [searchQuery]);

  // Group items by category
  const groupedItems = useMemo(() => {
    const groups: Record<string, NavItem[]> = {
      core: [],
      ai: [],
      advanced: [],
      analytics: []
    };
    filteredNavItems.forEach(item => {
      groups[item.category].push(item);
    });
    return groups;
  }, [filteredNavItems]);

  // Auto-initialize on mount
  useEffect(() => {
    if (!initAttemptedRef.current) {
      initAttemptedRef.current = true;
      silentAutoInit();
    }
  }, []);

  const silentAutoInit = async () => {
    setIsInitializing(true);
    const initPromises: Promise<void>[] = [];

    initPromises.push(
      (async () => {
        try {
          const statusRes = await qlibService.checkStatus();
          if (statusRes.qlib_available && !statusRes.initialized) {
            await qlibService.initialize();
          }
          const finalStatus = await qlibService.checkStatus();
          setQlibStatus({
            available: finalStatus.qlib_available || true,
            initialized: finalStatus.initialized || false
          });
        } catch (error) {
          console.warn('[AI Quant Lab] Qlib init:', error);
          setQlibStatus({ available: true, initialized: false });
        }
      })()
    );

    initPromises.push(
      (async () => {
        try {
          const statusRes = await rdAgentService.checkStatus();
          if (statusRes.rdagent_available && !statusRes.initialized) {
            await rdAgentService.initialize();
          }
          const finalStatus = await rdAgentService.checkStatus();
          setRDAgentStatus({
            available: finalStatus.rdagent_available || true,
            initialized: finalStatus.initialized || false
          });
        } catch (error) {
          console.warn('[AI Quant Lab] RD-Agent init:', error);
          setRDAgentStatus({ available: true, initialized: false });
        }
      })()
    );

    await Promise.allSettled(initPromises);
    setIsInitializing(false);
  };

  const refreshStatus = async () => {
    setIsInitializing(true);
    try {
      const [qlibRes, rdAgentRes] = await Promise.all([
        qlibService.checkStatus(),
        rdAgentService.checkStatus()
      ]);
      setQlibStatus({
        available: qlibRes.qlib_available || true,
        initialized: qlibRes.initialized || false
      });
      setRDAgentStatus({
        available: rdAgentRes.rdagent_available || true,
        initialized: rdAgentRes.initialized || false
      });
    } catch (error) {
      console.error('[AI Quant Lab] Failed to refresh status:', error);
    } finally {
      setIsInitializing(false);
    }
  };

  return (
    <div className="flex flex-col h-full" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      {/* Header */}
      <div
        className="flex items-center justify-between px-4 py-2.5 border-b"
        style={{ backgroundColor: FINCEPT.HEADER_BG, borderColor: FINCEPT.BORDER }}
      >
        <div className="flex items-center gap-3">
          <div className="flex items-center gap-2 px-3 py-1.5" style={{ backgroundColor: FINCEPT.ORANGE }}>
            <Brain size={18} color={FINCEPT.DARK_BG} />
            <span className="font-bold text-xs tracking-wider" style={{ color: FINCEPT.DARK_BG }}>
              AI QUANT LAB
            </span>
          </div>
          <div className="h-6 w-px" style={{ backgroundColor: FINCEPT.BORDER }} />
          <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
            Microsoft Qlib + RD-Agent • Professional Quantitative Research Platform
          </span>
        </div>

        <div className="flex items-center gap-2">
          {/* Status Indicators */}
          <div
            className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono"
            style={{
              backgroundColor: qlibStatus.initialized ? FINCEPT.PANEL_BG : FINCEPT.DARK_BG,
              borderLeft: `2px solid ${qlibStatus.initialized ? FINCEPT.GREEN : isInitializing ? FINCEPT.YELLOW : FINCEPT.GRAY}`
            }}
          >
            {qlibStatus.initialized ? (
              <CheckCircle2 size={12} color={FINCEPT.GREEN} />
            ) : isInitializing ? (
              <RefreshCw size={12} color={FINCEPT.YELLOW} className="animate-spin" />
            ) : (
              <Clock size={12} color={FINCEPT.GRAY} />
            )}
            <span style={{ color: qlibStatus.initialized ? FINCEPT.GREEN : FINCEPT.GRAY }}>
              QLIB
            </span>
          </div>

          <div
            className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono"
            style={{
              backgroundColor: rdAgentStatus.initialized ? FINCEPT.PANEL_BG : FINCEPT.DARK_BG,
              borderLeft: `2px solid ${rdAgentStatus.initialized ? FINCEPT.GREEN : isInitializing ? FINCEPT.YELLOW : FINCEPT.GRAY}`
            }}
          >
            {rdAgentStatus.initialized ? (
              <CheckCircle2 size={12} color={FINCEPT.GREEN} />
            ) : isInitializing ? (
              <RefreshCw size={12} color={FINCEPT.YELLOW} className="animate-spin" />
            ) : (
              <Clock size={12} color={FINCEPT.GRAY} />
            )}
            <span style={{ color: rdAgentStatus.initialized ? FINCEPT.GREEN : FINCEPT.GRAY }}>
              RD-AGENT
            </span>
          </div>

          <div className="h-6 w-px" style={{ backgroundColor: FINCEPT.BORDER }} />

          <button
            onClick={refreshStatus}
            className="p-1.5 hover:bg-opacity-80 transition-colors"
            disabled={isInitializing}
            title="Refresh status"
          >
            <RefreshCw size={14} color={FINCEPT.GRAY} className={isInitializing ? 'animate-spin' : ''} />
          </button>

          <button
            onClick={() => setShowSettings(!showSettings)}
            className="p-1.5 hover:bg-opacity-80 transition-colors"
            title="Settings"
          >
            <Settings size={14} color={FINCEPT.GRAY} />
          </button>
        </div>
      </div>

      {/* Main Content: Sidebar + Content */}
      <div className="flex flex-1 overflow-hidden">
        {/* Sidebar Navigation */}
        <div
          className="flex flex-col border-r transition-all duration-300"
          style={{
            backgroundColor: FINCEPT.PANEL_BG,
            borderColor: FINCEPT.BORDER,
            width: sidebarCollapsed ? '60px' : '280px'
          }}
        >
          {/* Sidebar Header */}
          <div className="p-3 border-b" style={{ borderColor: FINCEPT.BORDER }}>
            {!sidebarCollapsed && (
              <>
                {/* Search Bar */}
                <div
                  className="flex items-center gap-2 px-3 py-2 mb-3"
                  style={{ backgroundColor: FINCEPT.DARK_BG, borderRadius: '6px' }}
                >
                  <Search size={14} color={FINCEPT.GRAY} />
                  <input
                    type="text"
                    placeholder="Search modules..."
                    value={searchQuery}
                    onChange={(e) => setSearchQuery(e.target.value)}
                    className="flex-1 bg-transparent text-xs outline-none"
                    style={{ color: FINCEPT.WHITE }}
                  />
                  {searchQuery && (
                    <button onClick={() => setSearchQuery('')}>
                      <X size={14} color={FINCEPT.GRAY} />
                    </button>
                  )}
                </div>

                {/* Dashboard Button */}
                <button
                  onClick={() => setActiveView('dashboard')}
                  className="w-full flex items-center gap-2 px-3 py-2 text-xs font-semibold transition-all"
                  style={{
                    backgroundColor: activeView === 'dashboard' ? FINCEPT.ORANGE : 'transparent',
                    color: activeView === 'dashboard' ? FINCEPT.DARK_BG : FINCEPT.WHITE,
                    borderRadius: '6px'
                  }}
                >
                  <Home size={14} />
                  <span>Dashboard</span>
                </button>
              </>
            )}

            {sidebarCollapsed && (
              <button
                onClick={() => setActiveView('dashboard')}
                className="w-full flex items-center justify-center py-2"
                style={{
                  backgroundColor: activeView === 'dashboard' ? FINCEPT.ORANGE : 'transparent',
                  borderRadius: '6px'
                }}
              >
                <Home size={16} color={activeView === 'dashboard' ? FINCEPT.DARK_BG : FINCEPT.WHITE} />
              </button>
            )}
          </div>

          {/* Sidebar Content */}
          <div className="flex-1 overflow-y-auto p-2 space-y-4">
            {!sidebarCollapsed ? (
              <>
                {/* Core Modules */}
                {groupedItems.core.length > 0 && (
                  <div>
                    <div className="px-2 py-1 text-xs font-bold tracking-wider" style={{ color: FINCEPT.MUTED }}>
                      CORE
                    </div>
                    <div className="space-y-1">
                      {groupedItems.core.map((item) => (
                        <SidebarItem
                          key={item.id}
                          item={item}
                          isActive={activeView === item.id}
                          onClick={() => setActiveView(item.id)}
                        />
                      ))}
                    </div>
                  </div>
                )}

                {/* AI & Agents */}
                {groupedItems.ai.length > 0 && (
                  <div>
                    <div className="px-2 py-1 text-xs font-bold tracking-wider" style={{ color: FINCEPT.MUTED }}>
                      AI & AGENTS
                    </div>
                    <div className="space-y-1">
                      {groupedItems.ai.map((item) => (
                        <SidebarItem
                          key={item.id}
                          item={item}
                          isActive={activeView === item.id}
                          onClick={() => setActiveView(item.id)}
                        />
                      ))}
                    </div>
                  </div>
                )}

                {/* Advanced Trading */}
                {groupedItems.advanced.length > 0 && (
                  <div>
                    <div className="px-2 py-1 text-xs font-bold tracking-wider" style={{ color: FINCEPT.MUTED }}>
                      ADVANCED
                    </div>
                    <div className="space-y-1">
                      {groupedItems.advanced.map((item) => (
                        <SidebarItem
                          key={item.id}
                          item={item}
                          isActive={activeView === item.id}
                          onClick={() => setActiveView(item.id)}
                        />
                      ))}
                    </div>
                  </div>
                )}

                {/* Analytics */}
                {groupedItems.analytics.length > 0 && (
                  <div>
                    <div className="px-2 py-1 text-xs font-bold tracking-wider" style={{ color: FINCEPT.MUTED }}>
                      ANALYTICS
                    </div>
                    <div className="space-y-1">
                      {groupedItems.analytics.map((item) => (
                        <SidebarItem
                          key={item.id}
                          item={item}
                          isActive={activeView === item.id}
                          onClick={() => setActiveView(item.id)}
                        />
                      ))}
                    </div>
                  </div>
                )}
              </>
            ) : (
              // Collapsed view - icons only
              <div className="space-y-2">
                {filteredNavItems.map((item) => {
                  const Icon = item.icon;
                  return (
                    <button
                      key={item.id}
                      onClick={() => setActiveView(item.id)}
                      className="w-full flex items-center justify-center py-2 transition-all"
                      style={{
                        backgroundColor: activeView === item.id ? FINCEPT.ORANGE : 'transparent',
                        borderRadius: '6px'
                      }}
                      title={item.label}
                    >
                      <Icon size={16} color={activeView === item.id ? FINCEPT.DARK_BG : FINCEPT.WHITE} />
                    </button>
                  );
                })}
              </div>
            )}
          </div>

          {/* Collapse Toggle */}
          <div className="p-2 border-t" style={{ borderColor: FINCEPT.BORDER }}>
            <button
              onClick={() => setSidebarCollapsed(!sidebarCollapsed)}
              className="w-full flex items-center justify-center py-2 text-xs font-semibold transition-all"
              style={{ color: FINCEPT.GRAY }}
            >
              <ChevronRight
                size={16}
                className="transition-transform duration-300"
                style={{ transform: sidebarCollapsed ? 'rotate(0deg)' : 'rotate(180deg)' }}
              />
            </button>
          </div>
        </div>

        {/* Main Content Area */}
        <div className="flex-1 overflow-hidden">
          {activeView === 'dashboard' && <DashboardView navItems={navItems} onNavigate={setActiveView} />}
          {activeView === 'factor_discovery' && <FactorDiscoveryPanel />}
          {activeView === 'deep_agent' && <DeepAgentPanel />}
          {activeView === 'model_library' && <ModelLibraryPanel />}
          {activeView === 'backtesting' && <BacktestingPanel />}
          {activeView === 'live_signals' && <LiveSignalsPanel />}
          {activeView === 'rl_trading' && <RLTradingPanel />}
          {activeView === 'online_learning' && <OnlineLearningPanel />}
          {activeView === 'hft' && <HFTPanel />}
          {activeView === 'meta_learning' && <MetaLearningPanel />}
          {activeView === 'rolling_retraining' && <RollingRetrainingPanel />}
          {activeView === 'advanced_models' && <AdvancedModelsPanel />}
          {activeView === 'ffn_analytics' && <FFNAnalyticsPanel />}
          {activeView === 'functime' && <FunctimePanel />}
          {activeView === 'fortitudo' && <FortitudoPanel />}
          {activeView === 'statsmodels' && <StatsmodelsPanel />}
          {activeView === 'cfa_quant' && <CFAQuantPanel />}
        </div>
      </div>

      {/* Footer */}
      <TabFooter tabName="AI Quant Lab" />
    </div>
  );
}

// Sidebar Item Component
interface SidebarItemProps {
  item: NavItem;
  isActive: boolean;
  onClick: () => void;
}

function SidebarItem({ item, isActive, onClick }: SidebarItemProps) {
  const Icon = item.icon;

  return (
    <button
      onClick={onClick}
      className="w-full flex items-center gap-2 px-2 py-2 text-xs transition-all group relative"
      style={{
        backgroundColor: isActive ? FINCEPT.ORANGE : 'transparent',
        color: isActive ? FINCEPT.DARK_BG : FINCEPT.WHITE,
        borderRadius: '6px'
      }}
    >
      <Icon size={14} />
      <span className="flex-1 text-left font-medium">{item.label}</span>
      {item.popular && !isActive && (
        <span className="text-[10px] px-1.5 py-0.5" style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG, borderRadius: '3px' }}>
          HOT
        </span>
      )}
      {isActive && <ChevronRight size={12} />}
    </button>
  );
}

// Dashboard View Component
interface DashboardViewProps {
  navItems: NavItem[];
  onNavigate: (view: ViewMode) => void;
}

function DashboardView({ navItems, onNavigate }: DashboardViewProps) {
  const popularItems = navItems.filter(item => item.popular);

  return (
    <div className="h-full overflow-y-auto p-6" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      <div className="max-w-7xl mx-auto space-y-6">
        {/* Welcome Section */}
        <div>
          <h1 className="text-2xl font-bold mb-2" style={{ color: FINCEPT.WHITE }}>
            Welcome to AI Quant Lab
          </h1>
          <p className="text-sm" style={{ color: FINCEPT.GRAY }}>
            Professional quantitative research platform powered by Microsoft Qlib and RD-Agent
          </p>
        </div>

        {/* Quick Actions */}
        <div>
          <h2 className="text-sm font-bold tracking-wider mb-3" style={{ color: FINCEPT.MUTED }}>
            POPULAR MODULES
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
            {popularItems.map((item) => {
              const Icon = item.icon;
              return (
                <button
                  key={item.id}
                  onClick={() => onNavigate(item.id)}
                  className="flex flex-col p-4 text-left transition-all hover:scale-105"
                  style={{
                    backgroundColor: FINCEPT.CARD_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '8px'
                  }}
                >
                  <div className="flex items-center gap-3 mb-3">
                    <div className="p-2" style={{ backgroundColor: FINCEPT.ORANGE, borderRadius: '6px' }}>
                      <Icon size={20} color={FINCEPT.DARK_BG} />
                    </div>
                    <h3 className="font-bold text-sm" style={{ color: FINCEPT.WHITE }}>
                      {item.label}
                    </h3>
                  </div>
                  <p className="text-xs mb-3" style={{ color: FINCEPT.GRAY }}>
                    {item.description}
                  </p>
                  <div className="flex items-center gap-1 text-xs font-semibold" style={{ color: FINCEPT.ORANGE }}>
                    <span>Open</span>
                    <ChevronRight size={12} />
                  </div>
                </button>
              );
            })}
          </div>
        </div>

        {/* Feature Highlights */}
        <div>
          <h2 className="text-sm font-bold tracking-wider mb-3" style={{ color: FINCEPT.MUTED }}>
            PLATFORM FEATURES
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <FeatureCard
              icon={Brain}
              title="15+ Pre-trained Models"
              description="LightGBM, XGBoost, CatBoost, MLP, GATs, GRU, LSTM, Transformer, and more"
              color={FINCEPT.BLUE}
            />
            <FeatureCard
              icon={Sparkles}
              title="Autonomous Factor Mining"
              description="RD-Agent automatically discovers and validates alpha factors"
              color={FINCEPT.PURPLE}
            />
            <FeatureCard
              icon={Activity}
              title="Real-time Trading Signals"
              description="Live predictions with automated model updates and drift detection"
              color={FINCEPT.CYAN}
            />
            <FeatureCard
              icon={BarChart3}
              title="Professional Backtesting"
              description="Portfolio optimization, risk metrics, and performance analytics"
              color={FINCEPT.GREEN}
            />
          </div>
        </div>

        {/* Getting Started */}
        <div
          className="p-4"
          style={{
            backgroundColor: FINCEPT.CARD_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '8px'
          }}
        >
          <div className="flex items-start gap-3">
            <Info size={20} color={FINCEPT.ORANGE} />
            <div>
              <h3 className="font-bold text-sm mb-1" style={{ color: FINCEPT.WHITE }}>
                Getting Started
              </h3>
              <p className="text-xs mb-3" style={{ color: FINCEPT.GRAY }}>
                New to AI Quant Lab? Start with Factor Discovery to mine alpha factors, then train models in the Model Library, and backtest your strategies.
              </p>
              <button
                onClick={() => onNavigate('factor_discovery')}
                className="flex items-center gap-2 px-3 py-1.5 text-xs font-semibold transition-all"
                style={{
                  backgroundColor: FINCEPT.ORANGE,
                  color: FINCEPT.DARK_BG,
                  borderRadius: '6px'
                }}
              >
                <Play size={12} />
                <span>Start with Factor Discovery</span>
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

// Feature Card Component
interface FeatureCardProps {
  icon: React.ComponentType<{ size?: number; color?: string }>;
  title: string;
  description: string;
  color: string;
}

function FeatureCard({ icon: Icon, title, description, color }: FeatureCardProps) {
  return (
    <div
      className="flex items-start gap-3 p-4"
      style={{
        backgroundColor: FINCEPT.CARD_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '8px'
      }}
    >
      <div className="p-2" style={{ backgroundColor: `${color}20`, borderRadius: '6px' }}>
        <Icon size={18} color={color} />
      </div>
      <div>
        <h3 className="font-bold text-xs mb-1" style={{ color: FINCEPT.WHITE }}>
          {title}
        </h3>
        <p className="text-xs" style={{ color: FINCEPT.GRAY }}>
          {description}
        </p>
      </div>
    </div>
  );
}
