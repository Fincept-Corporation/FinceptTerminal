/**
 * Backtesting Tab
 *
 * Dedicated interface for creating, running, and analyzing backtests.
 * Features strategy editor, configuration, results display, and history.
 */

import React, { useState, useEffect } from 'react';
import {
  Activity,
  Play,
  Save,
  FolderOpen,
  TrendingUp,
  TrendingDown,
  BarChart3,
  Calendar,
  DollarSign,
  Settings,
  RefreshCw,
  Download,
  Code,
  Eye,
  Loader,
  CheckCircle,
  AlertCircle,
  FileText,
  ChevronDown,
  ChevronUp,
} from 'lucide-react';
import { LineChart, Line, AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { backtestingService } from '@/services/backtesting/BacktestingService';
import { backtestingRegistry } from '@/services/backtesting/BacktestingProviderRegistry';
import { sqliteService, type BacktestRun } from '@/services/sqliteService';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { BacktestRequest, BacktestResult } from '@/services/backtesting/interfaces/types';
import { StrategyDefinition } from '@/services/backtesting/interfaces/IStrategyDefinition';
import { STRATEGY_TEMPLATES, fillTemplate, type StrategyTemplate } from '@/services/backtesting/StrategyTemplates';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';

type EditorMode = 'code' | 'visual' | 'template';

export default function BacktestingTab() {
  const { colors } = useTerminalTheme();
  const { t } = useTranslation('backtesting');

  // State
  const [editorMode, setEditorMode] = useState<EditorMode>('code');
  const [strategyCode, setStrategyCode] = useState(DEFAULT_STRATEGY_CODE);
  const [strategyName, setStrategyName] = useState('My Strategy');

  // Configuration
  const [config, setConfig] = useState({
    startDate: new Date(Date.now() - 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    endDate: new Date().toISOString().split('T')[0],
    initialCapital: 100000,
    assets: [{ symbol: 'SPY', assetClass: 'stocks' as const, timeframe: 'daily' as const }],
  });

  // Advanced configuration (Backtesting.py specific)
  const [advancedConfig, setAdvancedConfig] = useState({
    commission: 0.001, // 0.1% commission
    tradeOnClose: false,
    hedging: false,
    exclusiveOrders: true,
    margin: 1.0, // No leverage by default
  });

  const [showAdvancedSettings, setShowAdvancedSettings] = useState(false);

  // Execution state
  const [isRunning, setIsRunning] = useState(false);
  const [currentResult, setCurrentResult] = useState<BacktestResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [executionLogs, setExecutionLogs] = useState<string[]>([]);

  // History
  const [backtestHistory, setBacktestHistory] = useState<BacktestRun[]>([]);
  const [selectedHistoryId, setSelectedHistoryId] = useState<string | null>(null);

  // Provider
  const [activeProvider, setActiveProvider] = useState<string | null>(null);
  const [availableProviders, setAvailableProviders] = useState<string[]>([]);
  const [showProviderDropdown, setShowProviderDropdown] = useState(false);
  const [providerStatus, setProviderStatus] = useState<'connected' | 'disconnected' | 'error'>('disconnected');

  // Template state
  const [selectedTemplate, setSelectedTemplate] = useState<StrategyTemplate | null>(null);
  const [templateParams, setTemplateParams] = useState<Record<string, any>>({});

  // Visual builder state
  const [visualNodes, setVisualNodes] = useState<any[]>([]);

  useEffect(() => {
    loadBacktestHistory();
    checkAndInitializeProvider();
  }, []);

  const checkAndInitializeProvider = async () => {
    try {
      // Check if provider is active in registry
      let provider = backtestingRegistry.getActiveProvider();
      console.log('[BacktestingTab] Active provider from registry:', provider);

      // If no active provider, try to load from database and activate
      if (!provider) {
        console.log('[BacktestingTab] No active provider, checking database...');
        const dbProviders = await sqliteService.getBacktestingProviders();
        const activeDbProvider = dbProviders.find(p => p.is_active);

        console.log('[BacktestingTab] Active provider from DB:', activeDbProvider);

        if (activeDbProvider) {
          // Get provider from registry
          const registeredProvider = backtestingRegistry.getProvider(activeDbProvider.name);

          if (registeredProvider) {
            console.log('[BacktestingTab] Found registered provider:', registeredProvider.name);

            // Initialize it
            const config = JSON.parse(activeDbProvider.config);
            await registeredProvider.initialize({
              name: activeDbProvider.name,
              adapterType: activeDbProvider.adapter_type,
              settings: config
            });

            // Set as active
            await backtestingRegistry.setActiveProvider(activeDbProvider.name);

            provider = registeredProvider;
            console.log('[BacktestingTab] Provider initialized and activated');
          }
        }
      }

      console.log('[BacktestingTab] All registered providers:', backtestingRegistry.listProviders());
      setActiveProvider(provider?.name || null);

      // Load all available providers - extract just the names
      const allProviders = backtestingRegistry.listProviders();
      const providerNames = allProviders.map(p => typeof p === 'string' ? p : p.name);
      setAvailableProviders(providerNames);

      // Set provider status
      if (provider) {
        setProviderStatus('connected');
      } else {
        setProviderStatus('disconnected');
      }
    } catch (error) {
      console.error('[BacktestingTab] Failed to initialize provider:', error);
      setProviderStatus('error');
    }
  };

  // Provider switching function
  const handleProviderSwitch = async (providerName: string) => {
    try {
      setShowProviderDropdown(false);
      setProviderStatus('disconnected');

      // Get provider from registry
      const provider = backtestingRegistry.getProvider(providerName);
      if (!provider) {
        throw new Error(`Provider ${providerName} not found`);
      }

      // Load config from database
      const dbProviders = await sqliteService.getBacktestingProviders();
      const dbProvider = dbProviders.find(p => p.name === providerName);

      if (dbProvider) {
        const config = JSON.parse(dbProvider.config);
        await provider.initialize({
          name: dbProvider.name,
          adapterType: dbProvider.adapter_type,
          settings: config
        });
      }

      // Set as active
      await backtestingRegistry.setActiveProvider(providerName);
      await sqliteService.setActiveBacktestingProvider(providerName);

      setActiveProvider(providerName);
      setProviderStatus('connected');

      addLog(`Switched to provider: ${providerName}`);
    } catch (error) {
      console.error('[BacktestingTab] Failed to switch provider:', error);
      setProviderStatus('error');
      setError(`Failed to switch provider: ${error instanceof Error ? error.message : String(error)}`);
    }
  };

  const loadBacktestHistory = async () => {
    try {
      const runs = await sqliteService.getBacktestRuns();
      setBacktestHistory(runs);
    } catch (error) {
      console.error('Failed to load backtest history:', error);
    }
  };

  const validatePythonSyntax = (code: string): { valid: boolean; error?: string } => {
    // Basic Python syntax checks
    const lines = code.split('\n');

    // Check for common syntax errors
    for (let i = 0; i < lines.length; i++) {
      const line = lines[i].trim();

      // Skip empty lines and comments
      if (!line || line.startsWith('#')) continue;

      // Check for unmatched parentheses in the line
      const openParen = (line.match(/\(/g) || []).length;
      const closeParen = (line.match(/\)/g) || []).length;
      const openBracket = (line.match(/\[/g) || []).length;
      const closeBracket = (line.match(/\]/g) || []).length;

      if (line.endsWith(':') && i + 1 < lines.length) {
        const nextLine = lines[i + 1];
        if (nextLine.trim() && !nextLine.startsWith(' ') && !nextLine.startsWith('\t')) {
          return { valid: false, error: `Line ${i + 1}: Expected indentation after ':'` };
        }
      }
    }

    // Check for required Lean class
    if (!code.includes('class ') || !code.includes('QCAlgorithm')) {
      return { valid: false, error: 'Strategy must define a class inheriting from QCAlgorithm' };
    }

    if (!code.includes('def Initialize(')) {
      return { valid: false, error: 'Strategy must have an Initialize() method' };
    }

    return { valid: true };
  };

  const addLog = (message: string) => {
    setExecutionLogs(prev => [...prev, `[${new Date().toLocaleTimeString()}] ${message}`]);
  };

  const handleRunBacktest = async () => {
    if (!activeProvider) {
      setError('No backtesting provider active. Please activate a provider in Settings → Backtesting Providers.');
      return;
    }

    // Validate syntax first
    const validation = validatePythonSyntax(strategyCode);
    if (!validation.valid) {
      setError(`Syntax Error: ${validation.error}`);
      return;
    }

    setIsRunning(true);
    setError(null);
    setCurrentResult(null);
    setExecutionLogs([]);

    // Set timeout to prevent infinite loading
    const timeoutId = setTimeout(() => {
      if (isRunning) {
        setError('Backtest timed out after 5 minutes. The backtest may still be running in the background.');
        setIsRunning(false);
      }
    }, 300000); // 5 minutes

    try {
      addLog('Starting backtest...');
      console.log('[Backtest] Starting backtest with provider:', activeProvider);
      console.log('[Backtest] Config:', config);

      addLog(`Using provider: ${activeProvider}`);
      addLog(`Strategy: ${strategyName}`);
      addLog(`Period: ${config.startDate} to ${config.endDate}`);
      addLog(`Initial Capital: $${config.initialCapital.toLocaleString()}`);

      // Build strategy definition
      const strategy: StrategyDefinition = {
        name: strategyName,
        version: '1.0.0',
        description: 'User-created strategy',
        author: 'User',
        type: 'code',
        code: {
          language: 'python',
          source: strategyCode,
        },
        parameters: [],
        requires: {
          dataFeeds: [],
          indicators: [],
          assetClasses: config.assets.map(a => a.assetClass),
        },
      };

      addLog('Building backtest request...');

      // Build backtest request with advanced parameters
      const request: BacktestRequest = {
        strategy,
        startDate: config.startDate,
        endDate: config.endDate,
        initialCapital: config.initialCapital,
        assets: config.assets,
        parameters: {},
        // Advanced Backtesting.py parameters
        commission: advancedConfig.commission,
        tradeOnClose: advancedConfig.tradeOnClose,
        hedging: advancedConfig.hedging,
        exclusiveOrders: advancedConfig.exclusiveOrders,
        margin: advancedConfig.margin,
      };

      console.log('[Backtest] Request:', request);
      addLog('Submitting to Lean engine...');

      // Run backtest
      const result = await backtestingService.runBacktest(request);
      console.log('[Backtest] Result:', result);

      clearTimeout(timeoutId);

      if (result.status === 'completed') {
        addLog('✓ Backtest completed successfully!');
        setCurrentResult(result);

        // Save to database
        await sqliteService.saveBacktestRun({
          id: result.id,
          strategy_id: '',
          provider_name: activeProvider,
          config: JSON.stringify(config),
          results: JSON.stringify(result),
          status: result.status,
          performance_metrics: JSON.stringify(result.performance),
          created_at: new Date().toISOString(),
          error_message: result.error,
        });

        addLog('Results saved to history');

        // Reload history
        await loadBacktestHistory();
      } else if (result.status === 'failed') {
        addLog('✗ Backtest failed');
        setError(result.error || 'Backtest failed with unknown error');
      }
    } catch (err) {
      clearTimeout(timeoutId);
      const errorMessage = err instanceof Error ? err.message : String(err);
      addLog(`✗ Error: ${errorMessage}`);
      setError(errorMessage);
      console.error('[Backtest] Failed:', err);
      console.error('[Backtest] Stack:', err instanceof Error ? err.stack : 'No stack');
    } finally {
      setIsRunning(false);
    }
  };

  const handleLoadHistory = async (runId: string) => {
    const run = backtestHistory.find((r) => r.id === runId);
    if (run && run.results) {
      try {
        const result = JSON.parse(run.results);
        setCurrentResult(result);
        setSelectedHistoryId(runId);

        // Load config if available
        if (run.config) {
          const savedConfig = JSON.parse(run.config);
          setConfig(savedConfig);
        }
      } catch (error) {
        console.error('Failed to load backtest result:', error);
      }
    }
  };

  const formatPercentage = (value: number | undefined) => {
    return ((value ?? 0) * 100).toFixed(2) + '%';
  };

  const formatCurrency = (value: number | undefined) => {
    return '$' + (value ?? 0).toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
  };

  const handleSelectTemplate = (template: StrategyTemplate) => {
    setSelectedTemplate(template);

    // Initialize params with defaults
    const defaultParams: Record<string, any> = {};
    template.parameters.forEach(param => {
      defaultParams[param.name] = param.default;
    });
    setTemplateParams(defaultParams);
  };

  const handleApplyTemplate = () => {
    if (!selectedTemplate) return;

    const filledCode = fillTemplate(selectedTemplate, templateParams);
    setStrategyCode(filledCode);
    setStrategyName(selectedTemplate.name);
    setEditorMode('code');
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      background: colors.background,
      color: colors.text,
      overflow: 'hidden',
    }}>
      {/* Bloomberg-Style Header */}
      <div style={{
        backgroundColor: '#1A1A1A',
        borderBottom: `2px solid #FF8800`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
        boxShadow: '0 2px 8px rgba(255, 136, 0, 0.2)'
      }}>
        {/* Left Section - Title and Provider */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '20px' }}>
          {/* Title */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <Activity size={20} color="#FF8800" style={{ filter: 'drop-shadow(0 0 4px #FF8800)' }} />
            <span style={{
              color: '#FF8800',
              fontWeight: 700,
              fontSize: '14px',
              letterSpacing: '0.5px',
              textShadow: '0 0 10px rgba(255, 136, 0, 0.4)'
            }}>
              {t('title')}
            </span>
          </div>

          {/* Provider Selector */}
          <div style={{ position: 'relative' }}>
            <button
              onClick={() => setShowProviderDropdown(!showProviderDropdown)}
              style={{
                backgroundColor: '#0F0F0F',
                border: `1px solid ${providerStatus === 'connected' ? '#00D66F' : providerStatus === 'error' ? '#FF3B3B' : '#2A2A2A'}`,
                borderRadius: '4px',
                padding: '6px 12px',
                color: '#FFFFFF',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                fontSize: '12px',
                fontWeight: 500,
                minWidth: '180px',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#1F1F1F'}
              onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#0F0F0F'}
            >
              {/* Status Indicator */}
              <div style={{
                width: '8px',
                height: '8px',
                borderRadius: '50%',
                backgroundColor: providerStatus === 'connected' ? '#00D66F' : providerStatus === 'error' ? '#FF3B3B' : '#787878',
                boxShadow: providerStatus === 'connected' ? '0 0 6px #00D66F' : providerStatus === 'error' ? '0 0 6px #FF3B3B' : 'none'
              }} />

              {/* Provider Name */}
              <span style={{ flex: 1, textAlign: 'left' }}>
                {activeProvider || 'No Provider'}
              </span>

              {/* Dropdown Icon */}
              <ChevronDown size={14} color="#787878" />
            </button>

            {/* Provider Dropdown */}
            {showProviderDropdown && (
              <div style={{
                position: 'absolute',
                top: '100%',
                left: 0,
                marginTop: '4px',
                backgroundColor: '#0F0F0F',
                border: '1px solid #2A2A2A',
                borderRadius: '4px',
                minWidth: '180px',
                boxShadow: '0 4px 12px rgba(0, 0, 0, 0.5)',
                zIndex: 1000,
                maxHeight: '300px',
                overflowY: 'auto'
              }}>
                {availableProviders.length > 0 ? (
                  availableProviders.map(provider => (
                    <div
                      key={provider}
                      onClick={() => handleProviderSwitch(provider)}
                      style={{
                        padding: '10px 12px',
                        cursor: 'pointer',
                        fontSize: '12px',
                        color: activeProvider === provider ? '#FF8800' : '#FFFFFF',
                        backgroundColor: activeProvider === provider ? '#1F1F1F' : 'transparent',
                        borderLeft: activeProvider === provider ? '3px solid #FF8800' : '3px solid transparent',
                        transition: 'all 0.2s'
                      }}
                      onMouseEnter={(e) => {
                        if (activeProvider !== provider) {
                          e.currentTarget.style.backgroundColor = '#1A1A1A';
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (activeProvider !== provider) {
                          e.currentTarget.style.backgroundColor = 'transparent';
                        }
                      }}
                    >
                      {provider}
                    </div>
                  ))
                ) : (
                  <div style={{
                    padding: '10px 12px',
                    fontSize: '12px',
                    color: '#787878',
                    fontStyle: 'italic'
                  }}>
                    No providers available
                  </div>
                )}
              </div>
            )}
          </div>

          {/* Provider Status */}
          <div style={{
            fontSize: '11px',
            color: '#787878',
            display: 'flex',
            alignItems: 'center',
            gap: '6px'
          }}>
            {providerStatus === 'connected' && (
              <>
                <CheckCircle size={12} color="#00D66F" />
                <span style={{ color: '#00D66F' }}>Connected</span>
              </>
            )}
            {providerStatus === 'disconnected' && (
              <>
                <AlertCircle size={12} color="#787878" />
                <span>Disconnected</span>
              </>
            )}
            {providerStatus === 'error' && (
              <>
                <AlertCircle size={12} color="#FF3B3B" />
                <span style={{ color: '#FF3B3B' }}>Error</span>
              </>
            )}
          </div>
        </div>

        {/* Right Section - Action Buttons */}
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          {/* Refresh Provider Button */}
          <button
            onClick={checkAndInitializeProvider}
            style={{
              padding: '6px 12px',
              backgroundColor: '#0F0F0F',
              color: '#FFFFFF',
              border: '1px solid #2A2A2A',
              borderRadius: '4px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontSize: '12px',
              fontWeight: 500,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#1F1F1F'}
            onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#0F0F0F'}
          >
            <RefreshCw size={14} />
            Refresh
          </button>

          {/* Run Backtest Button */}
          <button
            onClick={handleRunBacktest}
            disabled={isRunning || !activeProvider}
            style={{
              padding: '6px 16px',
              backgroundColor: isRunning || !activeProvider ? '#2A2A2A' : '#FF8800',
              color: '#FFFFFF',
              border: 'none',
              borderRadius: '4px',
              cursor: isRunning || !activeProvider ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontSize: '12px',
              fontWeight: 600,
              opacity: isRunning || !activeProvider ? 0.5 : 1,
              transition: 'all 0.2s',
              boxShadow: !isRunning && activeProvider ? '0 0 10px rgba(255, 136, 0, 0.3)' : 'none'
            }}
            onMouseEnter={(e) => {
              if (!isRunning && activeProvider) {
                e.currentTarget.style.backgroundColor = '#FF9922';
                e.currentTarget.style.boxShadow = '0 0 15px rgba(255, 136, 0, 0.5)';
              }
            }}
            onMouseLeave={(e) => {
              if (!isRunning && activeProvider) {
                e.currentTarget.style.backgroundColor = '#FF8800';
                e.currentTarget.style.boxShadow = '0 0 10px rgba(255, 136, 0, 0.3)';
              }
            }}
          >
            {isRunning ? (
              <>
                <Loader size={14} className="animate-spin" />
                Running...
              </>
            ) : (
              <>
                <Play size={16} />
                Run Backtest
              </>
            )}
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Strategy Editor */}
        <div style={{
          width: '40%',
          borderRight: `1px solid ${colors.textMuted}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}>
          {/* Editor Tabs */}
          <div style={{
            display: 'flex',
            borderBottom: `1px solid ${colors.textMuted}`,
            background: colors.panel,
          }}>
            {(['code', 'visual', 'template'] as EditorMode[]).map((mode) => (
              <button
                key={mode}
                onClick={() => setEditorMode(mode)}
                style={{
                  padding: '12px 20px',
                  background: editorMode === mode ? colors.background : 'transparent',
                  color: editorMode === mode ? colors.text : colors.textMuted,
                  border: 'none',
                  borderBottom: editorMode === mode ? `2px solid ${colors.accent}` : '2px solid transparent',
                  cursor: 'pointer',
                  fontSize: '0.9rem',
                  fontWeight: editorMode === mode ? '600' : '400',
                  textTransform: 'capitalize',
                }}
              >
                {mode}
              </button>
            ))}
          </div>

          {/* Strategy Name */}
          <div style={{ padding: '12px 16px', borderBottom: `1px solid ${colors.textMuted}` }}>
            <input
              type="text"
              value={strategyName}
              onChange={(e) => setStrategyName(e.target.value)}
              placeholder="Strategy Name"
              style={{
                width: '100%',
                padding: '8px 12px',
                background: colors.panel,
                border: `1px solid ${colors.textMuted}`,
                borderRadius: '4px',
                color: colors.text,
                fontSize: '0.9rem',
              }}
            />
          </div>

          {/* Code Editor */}
          {editorMode === 'code' && (
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
              <div style={{ padding: '12px 16px', background: colors.panel, borderBottom: `1px solid ${colors.textMuted}` }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '0.85rem' }}>
                  <Code size={14} />
                  <span>Python Strategy Code</span>
                </div>
              </div>
              <textarea
                value={strategyCode}
                onChange={(e) => setStrategyCode(e.target.value)}
                style={{
                  flex: 1,
                  padding: '16px',
                  background: colors.background,
                  color: colors.text,
                  border: 'none',
                  fontFamily: 'Consolas, Monaco, monospace',
                  fontSize: '0.85rem',
                  resize: 'none',
                  outline: 'none',
                }}
                spellCheck={false}
              />
            </div>
          )}

          {editorMode === 'visual' && (
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', padding: '16px' }}>
              <div style={{ marginBottom: '16px' }}>
                <h3 style={{ fontSize: '1rem', marginBottom: '8px' }}>Visual Strategy Builder</h3>
                <p style={{ fontSize: '0.85rem', color: colors.textMuted }}>Build strategies using blocks - no coding required</p>
              </div>

              {/* Building Blocks */}
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', marginBottom: '16px' }}>
                {[
                  { name: 'Entry Condition', type: 'condition', icon: '🎯' },
                  { name: 'Exit Condition', type: 'condition', icon: '🚪' },
                  { name: 'Indicator (SMA)', type: 'indicator', icon: '📊' },
                  { name: 'Indicator (RSI)', type: 'indicator', icon: '📈' },
                  { name: 'Buy Order', type: 'order', icon: '🟢' },
                  { name: 'Sell Order', type: 'order', icon: '🔴' },
                ].map((block) => (
                  <div
                    key={block.name}
                    style={{
                      padding: '12px',
                      background: colors.panel,
                      border: `1px solid ${colors.textMuted}`,
                      borderRadius: '6px',
                      cursor: 'pointer',
                      textAlign: 'center',
                    }}
                    onClick={() => setVisualNodes([...visualNodes, { ...block, id: Date.now() }])}
                  >
                    <div style={{ fontSize: '2rem', marginBottom: '4px' }}>{block.icon}</div>
                    <div style={{ fontSize: '0.8rem' }}>{block.name}</div>
                  </div>
                ))}
              </div>

              {/* Canvas */}
              <div style={{ flex: 1, background: colors.background, border: `1px solid ${colors.textMuted}`, borderRadius: '6px', padding: '16px', overflowY: 'auto' }}>
                {visualNodes.length === 0 ? (
                  <div style={{ textAlign: 'center', color: colors.textMuted, paddingTop: '40px' }}>
                    <Eye size={48} style={{ opacity: 0.5, margin: '0 auto 16px' }} />
                    <p>Click blocks above to add them to your strategy</p>
                  </div>
                ) : (
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    {visualNodes.map((node, idx) => (
                      <div
                        key={node.id}
                        style={{
                          padding: '12px',
                          background: colors.panel,
                          border: `1px solid ${colors.accent}`,
                          borderRadius: '4px',
                          display: 'flex',
                          justifyContent: 'space-between',
                          alignItems: 'center',
                        }}
                      >
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                          <span style={{ fontSize: '1.5rem' }}>{node.icon}</span>
                          <span>{node.name}</span>
                        </div>
                        <button
                          onClick={() => setVisualNodes(visualNodes.filter((_, i) => i !== idx))}
                          style={{
                            background: colors.alert,
                            color: 'white',
                            border: 'none',
                            padding: '4px 8px',
                            borderRadius: '4px',
                            cursor: 'pointer',
                            fontSize: '0.75rem',
                          }}
                        >
                          Remove
                        </button>
                      </div>
                    ))}
                  </div>
                )}
              </div>

              {/* Generate Code Button */}
              {visualNodes.length > 0 && (
                <button
                  onClick={() => {
                    // Simple code generation
                    const code = `# Auto-generated from visual builder\nfrom AlgorithmImports import *\n\nclass VisualStrategy(QCAlgorithm):\n    def Initialize(self):\n        self.SetStartDate(2020, 1, 1)\n        self.SetCash(100000)\n        # Add your logic here based on blocks\n`;
                    setStrategyCode(code);
                    setStrategyName('Visual Strategy');
                    setEditorMode('code');
                  }}
                  style={{
                    marginTop: '12px',
                    padding: '10px 20px',
                    background: colors.accent,
                    color: 'white',
                    border: 'none',
                    borderRadius: '6px',
                    cursor: 'pointer',
                    fontWeight: '600',
                    width: '100%',
                  }}
                >
                  Generate Code from Blocks
                </button>
              )}
            </div>
          )}

          {editorMode === 'template' && (
            <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
              {/* Template List */}
              <div style={{ width: '300px', borderRight: `1px solid ${colors.textMuted}`, overflowY: 'auto', padding: '16px' }}>
                <h3 style={{ fontSize: '0.9rem', marginBottom: '12px', color: colors.text }}>Strategy Templates</h3>
                {STRATEGY_TEMPLATES.map((template) => (
                  <div
                    key={template.id}
                    onClick={() => handleSelectTemplate(template)}
                    style={{
                      padding: '12px',
                      marginBottom: '8px',
                      background: selectedTemplate?.id === template.id ? colors.accent + '20' : colors.panel,
                      border: `1px solid ${selectedTemplate?.id === template.id ? colors.accent : colors.textMuted}`,
                      borderRadius: '4px',
                      cursor: 'pointer',
                    }}
                  >
                    <div style={{ fontWeight: '600', fontSize: '0.85rem', marginBottom: '4px' }}>{template.name}</div>
                    <div style={{ fontSize: '0.75rem', color: colors.textMuted, marginBottom: '4px' }}>{template.description}</div>
                    <div style={{ display: 'flex', gap: '6px', fontSize: '0.7rem' }}>
                      <span style={{ padding: '2px 6px', background: colors.background, borderRadius: '3px' }}>{template.category}</span>
                      <span style={{ padding: '2px 6px', background: colors.background, borderRadius: '3px' }}>{template.difficulty}</span>
                    </div>
                  </div>
                ))}
              </div>

              {/* Template Details & Parameters */}
              <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
                {selectedTemplate ? (
                  <>
                    <h2 style={{ fontSize: '1.2rem', marginBottom: '8px' }}>{selectedTemplate.name}</h2>
                    <p style={{ fontSize: '0.85rem', color: colors.textMuted, marginBottom: '16px' }}>{selectedTemplate.description}</p>

                    {/* Parameters */}
                    <div style={{ marginBottom: '20px' }}>
                      <h3 style={{ fontSize: '0.9rem', marginBottom: '12px' }}>Parameters</h3>
                      {selectedTemplate.parameters.map((param) => (
                        <div key={param.name} style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', fontSize: '0.85rem', marginBottom: '4px' }}>
                            {param.name} - {param.description}
                          </label>
                          {param.type === 'number' ? (
                            <input
                              type="number"
                              value={templateParams[param.name] ?? param.default}
                              onChange={(e) => setTemplateParams({ ...templateParams, [param.name]: Number(e.target.value) })}
                              min={param.min}
                              max={param.max}
                              style={{
                                width: '100%',
                                padding: '8px',
                                background: colors.background,
                                border: `1px solid ${colors.textMuted}`,
                                borderRadius: '4px',
                                color: colors.text,
                                fontSize: '0.85rem',
                              }}
                            />
                          ) : param.type === 'boolean' ? (
                            <input
                              type="checkbox"
                              checked={templateParams[param.name] ?? param.default}
                              onChange={(e) => setTemplateParams({ ...templateParams, [param.name]: e.target.checked })}
                            />
                          ) : (
                            <input
                              type="text"
                              value={templateParams[param.name] ?? param.default}
                              onChange={(e) => setTemplateParams({ ...templateParams, [param.name]: e.target.value })}
                              style={{
                                width: '100%',
                                padding: '8px',
                                background: colors.background,
                                border: `1px solid ${colors.textMuted}`,
                                borderRadius: '4px',
                                color: colors.text,
                                fontSize: '0.85rem',
                              }}
                            />
                          )}
                        </div>
                      ))}
                    </div>

                    {/* Expected Performance */}
                    {selectedTemplate.expectedPerformance && (
                      <div style={{ marginBottom: '20px', padding: '12px', background: colors.panel, borderRadius: '4px' }}>
                        <h3 style={{ fontSize: '0.9rem', marginBottom: '8px' }}>Expected Performance</h3>
                        <div style={{ fontSize: '0.85rem' }}>
                          <div>Avg Return: {selectedTemplate.expectedPerformance.avgReturn}</div>
                          <div>Sharpe Ratio: {selectedTemplate.expectedPerformance.sharpeRange}</div>
                          <div>Max Drawdown: {selectedTemplate.expectedPerformance.maxDD}</div>
                        </div>
                      </div>
                    )}

                    {/* Apply Button */}
                    <button
                      onClick={handleApplyTemplate}
                      style={{
                        padding: '10px 20px',
                        background: colors.accent,
                        color: 'white',
                        border: 'none',
                        borderRadius: '6px',
                        cursor: 'pointer',
                        fontWeight: '600',
                        width: '100%',
                      }}
                    >
                      Apply Template to Code Editor
                    </button>
                  </>
                ) : (
                  <div style={{ textAlign: 'center', color: colors.textMuted, paddingTop: '40px' }}>
                    <FileText size={48} style={{ opacity: 0.5, margin: '0 auto 16px' }} />
                    <p>Select a template from the list</p>
                  </div>
                )}
              </div>
            </div>
          )}

          {/* Configuration Panel */}
          <div style={{
            padding: '16px',
            borderTop: `1px solid ${colors.textMuted}`,
            background: colors.panel,
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
              <Settings size={16} />
              <span style={{ fontWeight: '600' }}>Configuration</span>
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <div>
                <label style={{ display: 'block', fontSize: '0.85rem', marginBottom: '4px', color: colors.textMuted }}>
                  Start Date
                </label>
                <input
                  type="date"
                  value={config.startDate}
                  onChange={(e) => setConfig({ ...config, startDate: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px',
                    background: colors.background,
                    border: `1px solid ${colors.textMuted}`,
                    borderRadius: '4px',
                    color: colors.text,
                    fontSize: '0.85rem',
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '0.85rem', marginBottom: '4px', color: colors.textMuted }}>
                  End Date
                </label>
                <input
                  type="date"
                  value={config.endDate}
                  onChange={(e) => setConfig({ ...config, endDate: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px',
                    background: colors.background,
                    border: `1px solid ${colors.textMuted}`,
                    borderRadius: '4px',
                    color: colors.text,
                    fontSize: '0.85rem',
                  }}
                />
              </div>

              <div style={{ gridColumn: '1 / -1' }}>
                <label style={{ display: 'block', fontSize: '0.85rem', marginBottom: '4px', color: colors.textMuted }}>
                  Initial Capital
                </label>
                <input
                  type="number"
                  value={config.initialCapital}
                  onChange={(e) => setConfig({ ...config, initialCapital: Number(e.target.value) })}
                  style={{
                    width: '100%',
                    padding: '8px',
                    background: colors.background,
                    border: `1px solid ${colors.textMuted}`,
                    borderRadius: '4px',
                    color: colors.text,
                    fontSize: '0.85rem',
                  }}
                />
              </div>
            </div>

            {/* Advanced Settings Toggle */}
            <button
              onClick={() => setShowAdvancedSettings(!showAdvancedSettings)}
              style={{
                width: '100%',
                marginTop: '12px',
                padding: '8px 12px',
                background: colors.background,
                color: colors.text,
                border: `1px solid ${colors.textMuted}`,
                borderRadius: '4px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                fontSize: '0.85rem',
                fontWeight: '500',
              }}
            >
              <span>Advanced Settings</span>
              {showAdvancedSettings ? <ChevronUp size={16} /> : <ChevronDown size={16} />}
            </button>

            {/* Advanced Settings Panel */}
            {showAdvancedSettings && (
              <div style={{
                marginTop: '12px',
                padding: '12px',
                background: colors.background,
                border: `1px solid ${colors.textMuted}`,
                borderRadius: '4px',
              }}>
                <div style={{ marginBottom: '8px', fontSize: '0.8rem', color: colors.textMuted, fontStyle: 'italic' }}>
                  Advanced options for Backtesting.py provider
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                  {/* Commission */}
                  <div>
                    <label style={{ display: 'block', fontSize: '0.85rem', marginBottom: '4px', color: colors.textMuted }}>
                      Commission (decimal)
                    </label>
                    <input
                      type="number"
                      step="0.0001"
                      min="0"
                      max="1"
                      value={advancedConfig.commission}
                      onChange={(e) => setAdvancedConfig({ ...advancedConfig, commission: Number(e.target.value) })}
                      style={{
                        width: '100%',
                        padding: '8px',
                        background: colors.panel,
                        border: `1px solid ${colors.textMuted}`,
                        borderRadius: '4px',
                        color: colors.text,
                        fontSize: '0.85rem',
                      }}
                    />
                    <div style={{ fontSize: '0.7rem', color: colors.textMuted, marginTop: '2px' }}>
                      {((advancedConfig.commission ?? 0) * 100).toFixed(2)}% per trade
                    </div>
                  </div>

                  {/* Margin/Leverage */}
                  <div>
                    <label style={{ display: 'block', fontSize: '0.85rem', marginBottom: '4px', color: colors.textMuted }}>
                      Margin/Leverage
                    </label>
                    <input
                      type="number"
                      step="0.1"
                      min="0.1"
                      max="10"
                      value={advancedConfig.margin}
                      onChange={(e) => setAdvancedConfig({ ...advancedConfig, margin: Number(e.target.value) })}
                      style={{
                        width: '100%',
                        padding: '8px',
                        background: colors.panel,
                        border: `1px solid ${colors.textMuted}`,
                        borderRadius: '4px',
                        color: colors.text,
                        fontSize: '0.85rem',
                      }}
                    />
                    <div style={{ fontSize: '0.7rem', color: colors.textMuted, marginTop: '2px' }}>
                      {advancedConfig.margin}x leverage (1.0 = no leverage)
                    </div>
                  </div>

                  {/* Trade on Close */}
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <input
                      type="checkbox"
                      checked={advancedConfig.tradeOnClose}
                      onChange={(e) => setAdvancedConfig({ ...advancedConfig, tradeOnClose: e.target.checked })}
                      style={{
                        width: '16px',
                        height: '16px',
                        cursor: 'pointer',
                      }}
                    />
                    <label style={{ fontSize: '0.85rem', color: colors.text, cursor: 'pointer' }}>
                      Trade on Close
                    </label>
                  </div>

                  {/* Hedging */}
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <input
                      type="checkbox"
                      checked={advancedConfig.hedging}
                      onChange={(e) => setAdvancedConfig({ ...advancedConfig, hedging: e.target.checked })}
                      style={{
                        width: '16px',
                        height: '16px',
                        cursor: 'pointer',
                      }}
                    />
                    <label style={{ fontSize: '0.85rem', color: colors.text, cursor: 'pointer' }}>
                      Allow Hedging
                    </label>
                  </div>

                  {/* Exclusive Orders */}
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', gridColumn: '1 / -1' }}>
                    <input
                      type="checkbox"
                      checked={advancedConfig.exclusiveOrders}
                      onChange={(e) => setAdvancedConfig({ ...advancedConfig, exclusiveOrders: e.target.checked })}
                      style={{
                        width: '16px',
                        height: '16px',
                        cursor: 'pointer',
                      }}
                    />
                    <label style={{ fontSize: '0.85rem', color: colors.text, cursor: 'pointer' }}>
                      Exclusive Orders (auto-close previous positions)
                    </label>
                  </div>
                </div>

                {/* Tooltips */}
                <div style={{
                  marginTop: '12px',
                  padding: '8px',
                  background: colors.panel,
                  borderRadius: '4px',
                  fontSize: '0.75rem',
                  color: colors.textMuted,
                  lineHeight: '1.4',
                }}>
                  <div style={{ marginBottom: '4px' }}><strong>Trade on Close:</strong> Execute orders at bar close vs next bar open</div>
                  <div style={{ marginBottom: '4px' }}><strong>Hedging:</strong> Allow simultaneous long and short positions</div>
                  <div><strong>Exclusive Orders:</strong> Auto-close existing positions when opening new ones</div>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Right Panel - Results */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Results Tabs */}
          <div style={{
            display: 'flex',
            borderBottom: `1px solid ${colors.textMuted}`,
            background: colors.panel,
          }}>
            <button
              onClick={() => setSelectedHistoryId(null)}
              style={{
                padding: '12px 20px',
                background: !selectedHistoryId ? colors.background : 'transparent',
                color: !selectedHistoryId ? colors.text : colors.textMuted,
                border: 'none',
                borderBottom: !selectedHistoryId ? `2px solid ${colors.accent}` : '2px solid transparent',
                cursor: 'pointer',
                fontSize: '0.9rem',
                fontWeight: !selectedHistoryId ? '600' : '400',
              }}
            >
              Results
            </button>
            <button
              onClick={() => setSelectedHistoryId('history')}
              style={{
                padding: '12px 20px',
                background: selectedHistoryId === 'history' ? colors.background : 'transparent',
                color: selectedHistoryId === 'history' ? colors.text : colors.textMuted,
                border: 'none',
                borderBottom: selectedHistoryId === 'history' ? `2px solid ${colors.accent}` : '2px solid transparent',
                cursor: 'pointer',
                fontSize: '0.9rem',
                fontWeight: selectedHistoryId === 'history' ? '600' : '400',
              }}
            >
              History ({backtestHistory.length})
            </button>
          </div>

          {/* Results Content */}
          <div style={{ flex: 1, overflow: 'auto', padding: '20px' }}>
            {/* Show running state with logs */}
            {isRunning && (
              <div>
                <div style={{
                  padding: '20px',
                  textAlign: 'center',
                  color: colors.text,
                  borderBottom: `1px solid ${colors.textMuted}`,
                }}>
                  <Loader size={48} className="animate-spin" style={{ margin: '0 auto 16px', color: colors.accent }} />
                  <p style={{ fontSize: '1.1rem', fontWeight: '600' }}>Running Backtest...</p>
                  <p style={{ fontSize: '0.85rem', color: colors.textMuted, marginTop: '8px' }}>
                    {executionLogs.length > 0 ? executionLogs[executionLogs.length - 1] : 'Initializing...'}
                  </p>

                  {/* Progress Bar */}
                  <div style={{
                    width: '100%',
                    maxWidth: '400px',
                    height: '8px',
                    background: colors.panel,
                    borderRadius: '4px',
                    margin: '16px auto 0',
                    overflow: 'hidden',
                    border: `1px solid ${colors.textMuted}`,
                  }}>
                    <div
                      style={{
                        height: '100%',
                        background: `linear-gradient(90deg, ${colors.accent}, ${colors.success})`,
                        width: `${Math.min((executionLogs.length / 10) * 100, 90)}%`,
                        transition: 'width 0.3s ease',
                        animation: 'pulse 2s ease-in-out infinite',
                      }}
                    />
                  </div>
                </div>

                {/* Execution Logs */}
                <div style={{
                  padding: '16px',
                  background: colors.background,
                  fontFamily: 'Consolas, Monaco, monospace',
                  fontSize: '0.8rem',
                  maxHeight: '400px',
                  overflowY: 'auto',
                }}>
                  <h4 style={{ fontSize: '0.9rem', marginBottom: '12px', color: colors.text }}>Execution Log:</h4>
                  {executionLogs.map((log, idx) => (
                    <div key={idx} style={{
                      padding: '4px 0',
                      color: log.includes('✓') ? colors.success : log.includes('✗') ? colors.alert : colors.textMuted,
                      borderLeft: `2px solid ${log.includes('✓') ? colors.success : log.includes('✗') ? colors.alert : 'transparent'}`,
                      paddingLeft: '8px',
                      marginBottom: '2px',
                    }}>
                      {log}
                    </div>
                  ))}
                  {executionLogs.length === 0 && (
                    <div style={{ color: colors.textMuted, fontStyle: 'italic' }}>Waiting for logs...</div>
                  )}
                </div>
              </div>
            )}

            {error && !isRunning && (
              <div style={{
                padding: '16px',
                background: colors.alert + '20',
                border: `1px solid ${colors.alert}`,
                borderRadius: '6px',
                marginBottom: '20px',
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
              }}>
                <AlertCircle size={20} color={colors.alert} />
                <div>
                  <div style={{ fontWeight: '600', marginBottom: '4px' }}>Backtest Failed</div>
                  <div style={{ fontSize: '0.85rem', color: colors.textMuted }}>{error}</div>
                </div>
              </div>
            )}

            {/* History View */}
            {selectedHistoryId === 'history' && (
              <div>
                <h3 style={{ fontSize: '1.1rem', marginBottom: '16px' }}>Backtest History</h3>
                {backtestHistory.length === 0 ? (
                  <div style={{ textAlign: 'center', color: colors.textMuted, paddingTop: '40px' }}>
                    <Activity size={48} style={{ opacity: 0.3, margin: '0 auto 16px' }} />
                    <p>No backtest history yet</p>
                  </div>
                ) : (
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                    {backtestHistory.map((run) => (
                      <div
                        key={run.id}
                        onClick={() => handleLoadHistory(run.id)}
                        style={{
                          padding: '16px',
                          background: colors.panel,
                          border: `1px solid ${colors.textMuted}`,
                          borderRadius: '6px',
                          cursor: 'pointer',
                          transition: 'all 0.2s',
                        }}
                        onMouseEnter={(e) => e.currentTarget.style.borderColor = colors.accent}
                        onMouseLeave={(e) => e.currentTarget.style.borderColor = colors.textMuted}
                      >
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                          <div style={{ fontWeight: '600' }}>Run #{run.id.slice(0, 8)}</div>
                          <div style={{ fontSize: '0.85rem', color: colors.textMuted }}>
                            {new Date(run.created_at || Date.now()).toLocaleString()}
                          </div>
                        </div>
                        <div style={{ fontSize: '0.85rem', color: colors.textMuted }}>
                          Provider: {run.provider_name} | Status: {run.status}
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            )}

            {/* Results View */}
            {!selectedHistoryId && currentResult && currentResult.status === 'completed' && (
              <div>
                {/* Performance Metrics */}
                <div style={{ marginBottom: '24px' }}>
                  <h3 style={{ fontSize: '1.1rem', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <TrendingUp size={18} />
                    Performance Metrics
                  </h3>

                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '12px' }}>
                    {[
                      { label: 'Total Return', value: formatPercentage(currentResult.performance.totalReturn ?? 0), positive: (currentResult.performance.totalReturn ?? 0) > 0 },
                      { label: 'Annualized Return', value: formatPercentage(currentResult.performance.annualizedReturn ?? 0), positive: (currentResult.performance.annualizedReturn ?? 0) > 0 },
                      { label: 'Sharpe Ratio', value: (currentResult.performance.sharpeRatio ?? 0).toFixed(2), positive: (currentResult.performance.sharpeRatio ?? 0) > 0 },
                      { label: 'Max Drawdown', value: formatPercentage(currentResult.performance.maxDrawdown ?? 0), positive: false },
                      { label: 'Win Rate', value: formatPercentage(currentResult.performance.winRate ?? 0), positive: true },
                      { label: 'Total Trades', value: (currentResult.performance.totalTrades ?? 0).toString(), positive: true },
                    ].map((metric) => (
                      <div key={metric.label} style={{
                        padding: '16px',
                        background: colors.panel,
                        border: `1px solid ${colors.textMuted}`,
                        borderRadius: '6px',
                      }}>
                        <div style={{ fontSize: '0.85rem', color: colors.textMuted, marginBottom: '4px' }}>
                          {metric.label}
                        </div>
                        <div style={{
                          fontSize: '1.5rem',
                          fontWeight: 'bold',
                          color: metric.positive ? colors.success : colors.alert,
                        }}>
                          {metric.value}
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                {/* Equity Curve Chart */}
                {currentResult.equity && currentResult.equity.length > 0 && (
                  <div style={{ marginBottom: '24px' }}>
                    <h3 style={{ fontSize: '1.1rem', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <TrendingUp size={18} />
                      Equity Curve
                    </h3>
                    <div style={{
                      padding: '16px',
                      background: colors.panel,
                      border: `1px solid ${colors.textMuted}`,
                      borderRadius: '6px',
                      height: '300px',
                    }}>
                      <ResponsiveContainer width="100%" height="100%">
                        <LineChart data={currentResult.equity}>
                          <CartesianGrid strokeDasharray="3 3" stroke={colors.textMuted} opacity={0.3} />
                          <XAxis
                            dataKey="date"
                            stroke={colors.textMuted}
                            tick={{ fill: colors.textMuted, fontSize: 12 }}
                            tickFormatter={(value) => new Date(value).toLocaleDateString()}
                          />
                          <YAxis
                            stroke={colors.textMuted}
                            tick={{ fill: colors.textMuted, fontSize: 12 }}
                            tickFormatter={(value) => `$${(value / 1000).toFixed(0)}k`}
                          />
                          <Tooltip
                            contentStyle={{
                              backgroundColor: colors.background,
                              border: `1px solid ${colors.textMuted}`,
                              borderRadius: '4px',
                              color: colors.text
                            }}
                            labelFormatter={(value) => new Date(value).toLocaleDateString()}
                            formatter={(value: any) => [`$${Number(value).toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`, 'Equity']}
                          />
                          <Legend wrapperStyle={{ color: colors.text }} />
                          <Line
                            type="monotone"
                            dataKey="equity"
                            stroke={colors.accent}
                            strokeWidth={2}
                            dot={false}
                            name="Portfolio Value"
                          />
                        </LineChart>
                      </ResponsiveContainer>
                    </div>
                  </div>
                )}

                {/* Drawdown Chart */}
                {currentResult.equity && currentResult.equity.length > 0 && (
                  <div style={{ marginBottom: '24px' }}>
                    <h3 style={{ fontSize: '1.1rem', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <TrendingDown size={18} />
                      Drawdown
                    </h3>
                    <div style={{
                      padding: '16px',
                      background: colors.panel,
                      border: `1px solid ${colors.textMuted}`,
                      borderRadius: '6px',
                      height: '250px',
                    }}>
                      <ResponsiveContainer width="100%" height="100%">
                        <AreaChart data={currentResult.equity}>
                          <CartesianGrid strokeDasharray="3 3" stroke={colors.textMuted} opacity={0.3} />
                          <XAxis
                            dataKey="date"
                            stroke={colors.textMuted}
                            tick={{ fill: colors.textMuted, fontSize: 12 }}
                            tickFormatter={(value) => new Date(value).toLocaleDateString()}
                          />
                          <YAxis
                            stroke={colors.textMuted}
                            tick={{ fill: colors.textMuted, fontSize: 12 }}
                            tickFormatter={(value) => `${(value * 100).toFixed(0)}%`}
                          />
                          <Tooltip
                            contentStyle={{
                              backgroundColor: colors.background,
                              border: `1px solid ${colors.textMuted}`,
                              borderRadius: '4px',
                              color: colors.text
                            }}
                            labelFormatter={(value) => new Date(value).toLocaleDateString()}
                            formatter={(value: any) => [`${(Number(value) * 100).toFixed(2)}%`, 'Drawdown']}
                          />
                          <Legend wrapperStyle={{ color: colors.text }} />
                          <Area
                            type="monotone"
                            dataKey="drawdown"
                            stroke={colors.alert}
                            fill={colors.alert}
                            fillOpacity={0.3}
                            name="Drawdown %"
                          />
                        </AreaChart>
                      </ResponsiveContainer>
                    </div>
                  </div>
                )}

                {/* Statistics */}
                <div style={{ marginBottom: '24px' }}>
                  <h3 style={{ fontSize: '1.1rem', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <BarChart3 size={18} />
                    Statistics
                  </h3>

                  <div style={{
                    padding: '16px',
                    background: colors.panel,
                    border: `1px solid ${colors.textMuted}`,
                    borderRadius: '6px',
                  }}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', fontSize: '0.9rem' }}>
                      <div>
                        <span style={{ color: colors.textMuted }}>Initial Capital:</span>
                        <span style={{ float: 'right', fontWeight: '600' }}>
                          {formatCurrency(currentResult.statistics.initialCapital)}
                        </span>
                      </div>
                      <div>
                        <span style={{ color: colors.textMuted }}>Final Capital:</span>
                        <span style={{ float: 'right', fontWeight: '600' }}>
                          {formatCurrency(currentResult.statistics.finalCapital)}
                        </span>
                      </div>
                      <div>
                        <span style={{ color: colors.textMuted }}>Total Fees:</span>
                        <span style={{ float: 'right', fontWeight: '600' }}>
                          {formatCurrency(currentResult.statistics.totalFees)}
                        </span>
                      </div>
                      <div>
                        <span style={{ color: colors.textMuted }}>Total Trades:</span>
                        <span style={{ float: 'right', fontWeight: '600' }}>
                          {currentResult.statistics.totalTrades}
                        </span>
                      </div>
                    </div>
                  </div>
                </div>

                {/* Success Badge */}
                <div style={{
                  padding: '12px',
                  background: colors.success + '20',
                  border: `1px solid ${colors.success}`,
                  borderRadius: '6px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '12px',
                }}>
                  <CheckCircle size={20} color={colors.success} />
                  <span>Backtest completed successfully</span>
                </div>
              </div>
            )}

            {!selectedHistoryId && !currentResult && !error && !isRunning && (
              <div style={{
                height: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                color: colors.textMuted,
              }}>
                <div style={{ textAlign: 'center' }}>
                  <Activity size={64} style={{ opacity: 0.3, margin: '0 auto 20px' }} />
                  <p>No backtest results yet</p>
                  <p style={{ fontSize: '0.85rem' }}>Configure your strategy and click "Run Backtest"</p>
                </div>
              </div>
            )}

            {/* Debug info */}
            {!isRunning && (
              <div style={{ marginTop: '20px', padding: '12px', background: colors.panel, borderRadius: '4px', fontSize: '0.75rem', color: colors.textMuted }}>
                <div>Debug: isRunning={String(isRunning)} | hasError={String(!!error)} | hasResult={String(!!currentResult)} | resultStatus={currentResult?.status}</div>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Footer / Status Bar */}
      <TabFooter
        tabName="BACKTESTING"
        leftInfo={[
          { label: 'Strategy testing and optimization platform', color: colors.textMuted },
          {
            label: activeProvider ? `Provider: ${activeProvider}` : 'No provider active',
            color: colors.textMuted
          },
          {
            label: backtestHistory.length > 0 ? `History: ${backtestHistory.length} runs` : 'No backtest history',
            color: colors.textMuted
          },
        ]}
        statusInfo={
          <div style={{ display: 'flex', gap: '16px', alignItems: 'center' }}>
            {currentResult && currentResult.status === 'completed' && (
              <>
                <span>Total Return: {formatPercentage(currentResult.performance.totalReturn ?? 0)}</span>
                <span>Sharpe: {(currentResult.performance.sharpeRatio ?? 0).toFixed(2)}</span>
                <span>Max DD: {formatPercentage(currentResult.performance.maxDrawdown ?? 0)}</span>
              </>
            )}
            <span style={{ color: isRunning ? colors.warning : colors.secondary, fontWeight: 'bold' }}>
              {isRunning ? 'RUNNING...' : 'READY'}
            </span>
          </div>
        }
        backgroundColor={colors.panel}
        borderColor={colors.textMuted}
      />
    </div>
  );
}

const DEFAULT_STRATEGY_CODE = `# Simple Moving Average Crossover Strategy
# This is a basic example - modify as needed

from AlgorithmImports import *

class MyStrategy(QCAlgorithm):
    def Initialize(self):
        self.SetStartDate(2020, 1, 1)
        self.SetEndDate(2023, 12, 31)
        self.SetCash(100000)

        # Add equity
        self.symbol = self.AddEquity("SPY", Resolution.Daily).Symbol

        # Create indicators
        self.fast_sma = self.SMA(self.symbol, 50, Resolution.Daily)
        self.slow_sma = self.SMA(self.symbol, 200, Resolution.Daily)

    def OnData(self, data):
        if not self.fast_sma.IsReady or not self.slow_sma.IsReady:
            return

        # Buy signal: fast SMA crosses above slow SMA
        if self.fast_sma.Current.Value > self.slow_sma.Current.Value:
            if not self.Portfolio.Invested:
                self.SetHoldings(self.symbol, 1.0)

        # Sell signal: fast SMA crosses below slow SMA
        elif self.fast_sma.Current.Value < self.slow_sma.Current.Value:
            if self.Portfolio.Invested:
                self.Liquidate(self.symbol)
`;
