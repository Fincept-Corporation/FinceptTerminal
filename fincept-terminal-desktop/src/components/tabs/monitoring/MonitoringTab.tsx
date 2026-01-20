// File: src/components/tabs/MonitoringTab.tsx
// Professional Fincept Terminal-Grade Market Monitoring Interface

import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import { toast } from 'sonner';
import {
  Bell,
  Plus,
  Trash2,
  TrendingUp,
  TrendingDown,
  Activity,
  AlertCircle,
  CheckCircle,
  Eye,
  EyeOff,
  BarChart3,
  Zap,
  Target,
  Settings as SettingsIcon,
  RefreshCw,
  Filter,
  Search,
  X,
  ChevronDown,
  Clock,
  DollarSign,
} from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { useBrokerContext } from '@/contexts/BrokerContext';

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
  MUTED: '#4A4A4A'
};

// ============================================================================
// TYPES
// ============================================================================

interface MonitorCondition {
  id?: number;
  provider: string;
  symbol: string;
  field: 'price' | 'volume' | 'change_percent' | 'spread';
  operator: '>' | '<' | '>=' | '<=' | '==' | 'between';
  value: number;
  value2?: number;
  enabled: boolean;
}

interface MonitorAlert {
  id?: number;
  condition_id: number;
  provider: string;
  symbol: string;
  field: string;
  triggered_value: number;
  triggered_at: number;
}

// ============================================================================
// MAIN COMPONENT
// ============================================================================

export default function MonitoringTab() {
  const { t } = useTranslation('monitoring');
  const { availableBrokers } = useBrokerContext();
  const [conditions, setConditions] = useState<MonitorCondition[]>([]);
  const [alerts, setAlerts] = useState<MonitorAlert[]>([]);
  const [loading, setLoading] = useState(false);
  const [showAddForm, setShowAddForm] = useState(false);
  const [filterProvider, setFilterProvider] = useState<string>('all');
  const [filterField, setFilterField] = useState<string>('all');
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedView, setSelectedView] = useState<'conditions' | 'alerts'>('conditions');
  const [currentTime, setCurrentTime] = useState(new Date());

  // Extended provider list - includes both crypto and equity brokers
  const allProviders = [
    ...availableBrokers,
    { id: 'fyers', name: 'Fyers' },
    { id: 'kite', name: 'Zerodha Kite' },
  ];

  // New condition form state
  const [newCondition, setNewCondition] = useState<MonitorCondition>({
    provider: allProviders[0]?.id || 'fyers',
    symbol: 'NSE:RELIANCE-EQ',
    field: 'price',
    operator: '>',
    value: 1500,
    enabled: true,
  });

  // Update clock every second
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Load conditions and alerts
  useEffect(() => {
    loadConditions();
    loadAlerts();
    loadExistingConditions();

    // Listen for new alerts
    const unlistenPromise = listen<MonitorAlert>('monitor_alert', (event) => {
      const alert = event.payload;
      setAlerts((prev) => [alert, ...prev].slice(0, 100));

      // Show toast notification
      toast.success(`🔔 ALERT: ${alert.symbol}`, {
        description: `${alert.field.toUpperCase()} = ${alert.triggered_value.toFixed(2)}`,
      });
    });

    return () => {
      unlistenPromise.then((unlisten) => unlisten());
    };
  }, []);

  const loadConditions = async () => {
    try {
      await invoke('monitor_load_conditions');
    } catch (error) {
      console.error('Failed to load conditions:', error);
    }
  };

  const loadExistingConditions = async () => {
    try {
      const data = await invoke<MonitorCondition[]>('monitor_get_conditions');
      setConditions(data);
    } catch (error) {
      console.error('Failed to get conditions:', error);
    }
  };

  const loadAlerts = async () => {
    try {
      const data = await invoke<MonitorAlert[]>('monitor_get_alerts', { limit: 50 });
      setAlerts(data);
    } catch (error) {
      console.error('Failed to get alerts:', error);
    }
  };

  const addCondition = async () => {
    setLoading(true);
    try {
      await invoke('monitor_add_condition', { condition: newCondition });
      await loadExistingConditions();
      toast.success('✓ Condition Added', {
        description: `Monitoring ${newCondition.symbol} on ${newCondition.provider}`,
      });

      // Reset form and close
      setNewCondition({
        provider: allProviders[0]?.id || 'fyers',
        symbol: 'NSE:RELIANCE-EQ',
        field: 'price',
        operator: '>',
        value: 1500,
        enabled: true,
      });
      setShowAddForm(false);
    } catch (error) {
      toast.error('Failed to add condition: ' + error);
    } finally {
      setLoading(false);
    }
  };

  const deleteCondition = async (id: number) => {
    try {
      await invoke('monitor_delete_condition', { id });
      await loadExistingConditions();
      toast.success('✓ Condition Deleted');
    } catch (error) {
      toast.error('Failed to delete condition: ' + error);
    }
  };

  const getFieldIcon = (field: string) => {
    switch (field) {
      case 'price':
        return <DollarSign className="w-4 h-4" />;
      case 'volume':
        return <Activity className="w-4 h-4" />;
      case 'change_percent':
        return <TrendingUp className="w-4 h-4" />;
      case 'spread':
        return <BarChart3 className="w-4 h-4" />;
      default:
        return <Bell className="w-4 h-4" />;
    }
  };

  const filteredConditions = conditions.filter((c) => {
    if (filterProvider !== 'all' && c.provider !== filterProvider) return false;
    if (filterField !== 'all' && c.field !== filterField) return false;
    if (searchQuery && !c.symbol.toLowerCase().includes(searchQuery.toLowerCase())) return false;
    return true;
  });

  const filteredAlerts = alerts.filter((a) => {
    if (searchQuery && !a.symbol.toLowerCase().includes(searchQuery.toLowerCase())) return false;
    return true;
  });

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      {/* Fincept-Style Header */}
      <div
        className="px-6 py-4 border-b flex items-center justify-between"
        style={{ backgroundColor: FINCEPT.HEADER_BG, borderColor: FINCEPT.BORDER }}
      >
        <div className="flex items-center gap-6">
          <div className="flex items-center gap-3">
            <Bell className="w-6 h-6" style={{ color: FINCEPT.ORANGE }} />
            <div>
              <h1 className="text-xl font-bold" style={{ color: FINCEPT.WHITE }}>
                MARKET MONITORING
              </h1>
              <p className="text-xs" style={{ color: FINCEPT.MUTED }}>
                Real-time Alert System
              </p>
            </div>
          </div>

          {/* View Selector */}
          <div className="flex gap-2 ml-8">
            <button
              onClick={() => setSelectedView('conditions')}
              className="px-4 py-2 text-sm font-mono transition-colors"
              style={{
                backgroundColor: selectedView === 'conditions' ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
                color: selectedView === 'conditions' ? FINCEPT.DARK_BG : FINCEPT.MUTED,
                border: `1px solid ${FINCEPT.BORDER}`,
              }}
            >
              CONDITIONS ({conditions.length})
            </button>
            <button
              onClick={() => setSelectedView('alerts')}
              className="px-4 py-2 text-sm font-mono transition-colors"
              style={{
                backgroundColor: selectedView === 'alerts' ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
                color: selectedView === 'alerts' ? FINCEPT.DARK_BG : FINCEPT.MUTED,
                border: `1px solid ${FINCEPT.BORDER}`,
              }}
            >
              ALERTS ({alerts.length})
            </button>
          </div>
        </div>

        <div className="flex items-center gap-4">
          {/* Time Display */}
          <div className="flex items-center gap-2" style={{ color: FINCEPT.CYAN }}>
            <Clock className="w-4 h-4" />
            <span className="text-sm font-mono">
              {currentTime.toLocaleTimeString('en-US', { hour12: false })}
            </span>
          </div>

          {/* Stats */}
          <div className="flex items-center gap-2 px-3 py-1.5 border rounded" style={{ borderColor: FINCEPT.BORDER }}>
            <CheckCircle className="w-4 h-4" style={{ color: FINCEPT.GREEN }} />
            <span className="text-sm font-mono" style={{ color: FINCEPT.WHITE }}>
              {conditions.filter(c => c.enabled).length} ACTIVE
            </span>
          </div>

          {/* Add Button */}
          <button
            onClick={() => setShowAddForm(!showAddForm)}
            className="px-4 py-2 font-mono text-sm transition-all hover:brightness-110"
            style={{
              backgroundColor: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
            }}
          >
            <Plus className="w-4 h-4 inline mr-2" />
            NEW CONDITION
          </button>
        </div>
      </div>

      {/* Add Condition Form */}
      {showAddForm && (
        <div
          className="px-6 py-4 border-b"
          style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
        >
          <div className="grid grid-cols-6 gap-4">
            {/* Provider */}
            <div>
              <label className="block text-xs mb-2 font-mono" style={{ color: FINCEPT.ORANGE }}>
                PROVIDER
              </label>
              <select
                value={newCondition.provider}
                onChange={(e) => setNewCondition({ ...newCondition, provider: e.target.value })}
                className="w-full px-3 py-2 text-sm font-mono border outline-none"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  borderColor: FINCEPT.BORDER,
                }}
              >
                {allProviders.map((broker) => (
                  <option key={broker.id} value={broker.id}>
                    {broker.name.toUpperCase()}
                  </option>
                ))}
              </select>
            </div>

            {/* Symbol */}
            <div>
              <label className="block text-xs mb-2 font-mono" style={{ color: FINCEPT.ORANGE }}>
                SYMBOL
              </label>
              <input
                type="text"
                value={newCondition.symbol}
                onChange={(e) => setNewCondition({ ...newCondition, symbol: e.target.value.toUpperCase() })}
                placeholder="BTC/USD"
                className="w-full px-3 py-2 text-sm font-mono border outline-none"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  borderColor: FINCEPT.BORDER,
                }}
              />
            </div>

            {/* Field */}
            <div>
              <label className="block text-xs mb-2 font-mono" style={{ color: FINCEPT.ORANGE }}>
                FIELD
              </label>
              <select
                value={newCondition.field}
                onChange={(e) => setNewCondition({ ...newCondition, field: e.target.value as any })}
                className="w-full px-3 py-2 text-sm font-mono border outline-none"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  borderColor: FINCEPT.BORDER,
                }}
              >
                <option value="price">PRICE</option>
                <option value="volume">VOLUME</option>
                <option value="change_percent">% CHANGE</option>
                <option value="spread">SPREAD</option>
              </select>
            </div>

            {/* Operator */}
            <div>
              <label className="block text-xs mb-2 font-mono" style={{ color: FINCEPT.ORANGE }}>
                OPERATOR
              </label>
              <select
                value={newCondition.operator}
                onChange={(e) => setNewCondition({ ...newCondition, operator: e.target.value as any })}
                className="w-full px-3 py-2 text-sm font-mono border outline-none"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  borderColor: FINCEPT.BORDER,
                }}
              >
                <option value=">">&gt;</option>
                <option value="<">&lt;</option>
                <option value=">=">&gt;=</option>
                <option value="<=">&lt;=</option>
                <option value="==">==</option>
                <option value="between">BETWEEN</option>
              </select>
            </div>

            {/* Value */}
            <div>
              <label className="block text-xs mb-2 font-mono" style={{ color: FINCEPT.ORANGE }}>
                VALUE
              </label>
              <input
                type="number"
                value={newCondition.value}
                onChange={(e) => setNewCondition({ ...newCondition, value: parseFloat(e.target.value) })}
                className="w-full px-3 py-2 text-sm font-mono border outline-none"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  borderColor: FINCEPT.BORDER,
                }}
              />
            </div>

            {/* Action Buttons */}
            <div className="flex items-end gap-2">
              <button
                onClick={addCondition}
                disabled={loading}
                className="flex-1 px-4 py-2 font-mono text-sm transition-all hover:brightness-110"
                style={{
                  backgroundColor: FINCEPT.GREEN,
                  color: FINCEPT.DARK_BG,
                }}
              >
                ADD
              </button>
              <button
                onClick={() => setShowAddForm(false)}
                className="px-4 py-2 font-mono text-sm transition-all"
                style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  color: FINCEPT.MUTED,
                  border: `1px solid ${FINCEPT.BORDER}`,
                }}
              >
                <X className="w-4 h-4" />
              </button>
            </div>
          </div>

          {newCondition.operator === 'between' && (
            <div className="mt-4">
              <label className="block text-xs mb-2 font-mono" style={{ color: FINCEPT.ORANGE }}>
                VALUE 2
              </label>
              <input
                type="number"
                value={newCondition.value2 || 0}
                onChange={(e) => setNewCondition({ ...newCondition, value2: parseFloat(e.target.value) })}
                className="w-64 px-3 py-2 text-sm font-mono border outline-none"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  borderColor: FINCEPT.BORDER,
                }}
              />
            </div>
          )}
        </div>
      )}

      {/* Filters & Search */}
      <div
        className="px-6 py-3 border-b flex items-center gap-4"
        style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
      >
        <div className="flex items-center gap-2 flex-1">
          <Search className="w-4 h-4" style={{ color: FINCEPT.MUTED }} />
          <input
            type="text"
            placeholder="Search symbols..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="flex-1 bg-transparent outline-none text-sm font-mono"
            style={{ color: FINCEPT.WHITE }}
          />
        </div>

        {selectedView === 'conditions' && (
          <>
            <select
              value={filterProvider}
              onChange={(e) => setFilterProvider(e.target.value)}
              className="px-3 py-1.5 text-sm font-mono border outline-none"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                borderColor: FINCEPT.BORDER,
              }}
            >
              <option value="all">ALL PROVIDERS</option>
              {allProviders.map((broker) => (
                <option key={broker.id} value={broker.id}>
                  {broker.name.toUpperCase()}
                </option>
              ))}
            </select>

            <select
              value={filterField}
              onChange={(e) => setFilterField(e.target.value)}
              className="px-3 py-1.5 text-sm font-mono border outline-none"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                borderColor: FINCEPT.BORDER,
              }}
            >
              <option value="all">ALL FIELDS</option>
              <option value="price">PRICE</option>
              <option value="volume">VOLUME</option>
              <option value="change_percent">% CHANGE</option>
              <option value="spread">SPREAD</option>
            </select>
          </>
        )}

        <button
          onClick={selectedView === 'conditions' ? loadExistingConditions : loadAlerts}
          className="px-3 py-1.5 transition-all hover:brightness-110"
          style={{
            backgroundColor: FINCEPT.PANEL_BG,
            color: FINCEPT.CYAN,
            border: `1px solid ${FINCEPT.BORDER}`,
          }}
        >
          <RefreshCw className="w-4 h-4" />
        </button>
      </div>

      {/* Content Area */}
      <div className="flex-1 overflow-auto p-6">
        {selectedView === 'conditions' ? (
          // Conditions View
          <div className="space-y-2">
            {filteredConditions.length === 0 ? (
              <div className="text-center py-16">
                <Bell className="w-16 h-16 mx-auto mb-4" style={{ color: FINCEPT.MUTED }} />
                <p className="text-lg font-mono" style={{ color: FINCEPT.MUTED }}>
                  NO MONITORING CONDITIONS
                </p>
                <p className="text-sm mt-2" style={{ color: FINCEPT.GRAY }}>
                  Add conditions to start monitoring market events
                </p>
              </div>
            ) : (
              filteredConditions.map((condition) => (
                <div
                  key={condition.id}
                  className="p-4 border transition-all hover:brightness-110"
                  style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    borderColor: condition.enabled ? FINCEPT.BORDER : FINCEPT.MUTED,
                  }}
                >
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-4">
                      {/* Icon */}
                      <div
                        className="w-10 h-10 flex items-center justify-center"
                        style={{
                          backgroundColor: FINCEPT.DARK_BG,
                          color: FINCEPT.ORANGE,
                        }}
                      >
                        {getFieldIcon(condition.field)}
                      </div>

                      {/* Info */}
                      <div>
                        <div className="flex items-center gap-3">
                          <span className="text-lg font-mono font-bold" style={{ color: FINCEPT.WHITE }}>
                            {condition.symbol}
                          </span>
                          <span className="text-xs px-2 py-0.5 font-mono" style={{ backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.CYAN }}>
                            {condition.provider.toUpperCase()}
                          </span>
                          {!condition.enabled && (
                            <span className="text-xs px-2 py-0.5 font-mono" style={{ backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.GRAY }}>
                              DISABLED
                            </span>
                          )}
                        </div>
                        <div className="text-sm font-mono mt-1" style={{ color: FINCEPT.MUTED }}>
                          {condition.field.toUpperCase()} {condition.operator} {condition.value.toLocaleString()}
                          {condition.value2 && ` AND ${condition.value2.toLocaleString()}`}
                        </div>
                      </div>
                    </div>

                    {/* Actions */}
                    <button
                      onClick={() => condition.id && deleteCondition(condition.id)}
                      className="px-3 py-2 transition-all hover:brightness-110"
                      style={{
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.RED,
                      }}
                    >
                      <Trash2 className="w-4 h-4" />
                    </button>
                  </div>
                </div>
              ))
            )}
          </div>
        ) : (
          // Alerts View
          <div className="space-y-2">
            {filteredAlerts.length === 0 ? (
              <div className="text-center py-16">
                <AlertCircle className="w-16 h-16 mx-auto mb-4" style={{ color: FINCEPT.MUTED }} />
                <p className="text-lg font-mono" style={{ color: FINCEPT.MUTED }}>
                  NO ALERTS YET
                </p>
                <p className="text-sm mt-2" style={{ color: FINCEPT.GRAY }}>
                  Alerts will appear here when conditions are triggered
                </p>
              </div>
            ) : (
              filteredAlerts.map((alert, index) => (
                <div
                  key={alert.id || index}
                  className="p-4 border transition-all hover:brightness-110"
                  style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    borderColor: FINCEPT.BORDER,
                    borderLeftWidth: '4px',
                    borderLeftColor: FINCEPT.YELLOW,
                  }}
                >
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-4">
                      {/* Icon */}
                      <div
                        className="w-10 h-10 flex items-center justify-center"
                        style={{
                          backgroundColor: FINCEPT.DARK_BG,
                          color: FINCEPT.YELLOW,
                        }}
                      >
                        <Bell className="w-5 h-5" />
                      </div>

                      {/* Info */}
                      <div>
                        <div className="flex items-center gap-3">
                          <span className="text-lg font-mono font-bold" style={{ color: FINCEPT.WHITE }}>
                            {alert.symbol}
                          </span>
                          <span className="text-xs px-2 py-0.5 font-mono" style={{ backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.CYAN }}>
                            {alert.provider.toUpperCase()}
                          </span>
                        </div>
                        <div className="text-sm font-mono mt-1" style={{ color: FINCEPT.MUTED }}>
                          {alert.field.toUpperCase()}: {alert.triggered_value.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                        </div>
                      </div>
                    </div>

                    {/* Timestamp */}
                    <div className="text-sm font-mono" style={{ color: FINCEPT.GRAY }}>
                      {new Date(alert.triggered_at).toLocaleString()}
                    </div>
                  </div>
                </div>
              ))
            )}
          </div>
        )}
      </div>
    </div>
  );
}
