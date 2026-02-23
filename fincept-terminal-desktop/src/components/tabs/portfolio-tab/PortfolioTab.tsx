/**
 * Portfolio Management Tab - Fincept Terminal
 * Bloomberg-style dense multi-panel layout with real-time data
 * Thin shell composing modular sub-components
 */

import React, { useState, useCallback, useMemo, useRef } from 'react';
import { useEffect } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { Briefcase, RefreshCw, Bot, X, ChevronDown } from 'lucide-react';
import { FINCEPT, COMMON_STYLES } from './finceptStyles';
import agentService, { buildAgentConfig } from '@/services/agentService';
import { withTimeout } from '@/services/core/apiUtils';
import { sqliteService, getSetting, getAgentConfigs } from '@/services/core/sqliteService';
import type { AgentConfig } from '@/services/core/sqliteService';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';
import { usePortfolioOperations } from './hooks/usePortfolioOperations';
import { usePortfolioMetrics } from './hooks/usePortfolioMetrics';
import { useModalState } from './hooks/useModalState';
import {
  CommandBar, StatsRibbon, HoldingsHeatmap,
  PerformanceChartPanel, SectorCorrelationPanel,
  PositionsBlotter, OrderPanel, StatusBar,
  DetailViewWrapper,
} from './components';
import { FFNView } from './ffn';
import CreatePortfolioModal from './modals/CreatePortfolioModal';
import ImportPortfolioModal from './modals/ImportPortfolioModal';
import AddAssetModal from './modals/AddAssetModal';
import SellAssetModal from './modals/SellAssetModal';
import ConfirmDeleteModal from './modals/ConfirmDeleteModal';
import { CreateIndexModal } from './custom-index';
import type { HeatmapMode, DetailView, SortColumn, SortDirection } from './types';
import { useTranslation } from 'react-i18next';

const PortfolioTab: React.FC = () => {
  const { t } = useTranslation('portfolio');
  const { fontFamily } = useTerminalTheme();

  // ── Core data ──
  const operations = usePortfolioOperations();
  const { portfolios, selectedPortfolio, setSelectedPortfolio, portfolioSummary, transactions, refreshing, refreshIntervalMs, setRefreshIntervalMs } = operations;
  const currency = selectedPortfolio?.currency || 'USD';
  const holdings = portfolioSummary?.holdings ?? [];

  // ── Computed metrics ──
  const metrics = usePortfolioMetrics(portfolioSummary);

  // ── Modal state ──
  const modals = useModalState(operations);

  // ── Local UI state ──
  const [selectedSymbol, setSelectedSymbol] = useState<string | null>(null);
  const [heatmapMode, setHeatmapMode] = useState<HeatmapMode>('pnl');
  const [sortCol, setSortCol] = useState<SortColumn>('weight');
  const [sortDir, setSortDir] = useState<SortDirection>('desc');
  const [showOrderPanel, setShowOrderPanel] = useState(false);
  const [orderSide, setOrderSide] = useState<'BUY' | 'SELL'>('BUY');
  const [detailView, setDetailView] = useState<DetailView | null>(null);
  const [showFFN, setShowFFN] = useState(false);

  // ── AI Analysis state ──
  const [aiAnalyzing, setAiAnalyzing] = useState(false);
  const [aiResult, setAiResult] = useState<string | null>(null);
  const [showAiPanel, setShowAiPanel] = useState(false);
  const [aiAnalysisType, setAiAnalysisType] = useState<'full' | 'risk' | 'rebalance' | 'opportunities'>('full');
  // Cache: keyed by analysisType, cleared when portfolio changes
  const aiResultCache = useRef<Record<string, string>>({});
  const agentResultCache = useRef<Record<string, string>>({});
  const lastPortfolioId = useRef<string | null>(null);

  // Clear caches when the selected portfolio changes
  useEffect(() => {
    const currentId = selectedPortfolio?.id ?? null;
    if (lastPortfolioId.current !== currentId) {
      aiResultCache.current = {};
      agentResultCache.current = {};
      lastPortfolioId.current = currentId;
      setAiResult(null);
      setAgentResult(null);
    }
  }, [selectedPortfolio?.id]);

  // ── Agent Runner state ──
  const [agentRunning, setAgentRunning] = useState(false);
  const [agentResult, setAgentResult] = useState<string | null>(null);
  const [showAgentPanel, setShowAgentPanel] = useState(false);
  const [savedAgents, setSavedAgents] = useState<AgentConfig[]>([]);
  const [selectedAgentId, setSelectedAgentId] = useState<string>('');

  // Load saved agents once on mount
  useEffect(() => {
    getAgentConfigs().then(agents => {
      setSavedAgents(agents);
      if (agents.length > 0) setSelectedAgentId(agents[0].id);
    }).catch(() => {});
  }, []);

  // ── Run saved agent on portfolio ──
  const handleRunAgent = useCallback(async (forceRefresh = false) => {
    if (!portfolioSummary || agentRunning) return;
    setShowAgentPanel(true);
    if (!selectedAgentId) return;
    const agentCfgRow = savedAgents.find(a => a.id === selectedAgentId);
    if (!agentCfgRow) return;
    // Show cached result immediately if available
    if (!forceRefresh && agentResultCache.current[selectedAgentId]) {
      setAgentResult(agentResultCache.current[selectedAgentId]);
      return;
    }
    setAgentRunning(true);
    setAgentResult(null);

    try {
      const savedCfg = JSON.parse(agentCfgRow.config_json || '{}');

      // Build api keys from LLM configs
      const llmConfigs = await sqliteService.getLLMConfigs();
      const apiKeys: Record<string, string> = {};
      for (const cfg of llmConfigs) {
        if (cfg.api_key) {
          apiKeys[cfg.provider.toUpperCase() + '_API_KEY'] = cfg.api_key;
          apiKeys[cfg.provider] = cfg.api_key;
          apiKeys[cfg.provider.toLowerCase()] = cfg.api_key;
        }
        if (cfg.base_url) {
          apiKeys[cfg.provider.toUpperCase() + '_BASE_URL'] = cfg.base_url;
          apiKeys[cfg.provider + '_base_url'] = cfg.base_url;
        }
      }
      if (!apiKeys['fincept']) {
        const finceptKey = await getSetting('fincept_api_key');
        if (finceptKey) { apiKeys['fincept'] = finceptKey; apiKeys['FINCEPT_API_KEY'] = finceptKey; }
      }

      // Resolve provider/model — prefer saved config, fall back to active LLM
      const activeLLM = llmConfigs.find(c => c.is_active) ?? llmConfigs[0] ?? null;
      const provider = savedCfg.model?.provider ?? activeLLM?.provider ?? 'fincept';
      const model_id = savedCfg.model?.model_id ?? activeLLM?.model ?? 'fincept-llm';

      if (!activeLLM && !apiKeys['fincept']) {
        setAgentResult('No LLM configured. Please add a model in Settings → LLM Configuration.');
        return;
      }

      // Build proper agent config using the shared builder
      const agentConfig = buildAgentConfig(
        provider,
        model_id,
        savedCfg.instructions || 'You are a professional portfolio analyst. Analyze the provided portfolio data and give actionable insights.',
        {
          tools: savedCfg.tools ?? [],
          temperature: savedCfg.model?.temperature ?? 0.3,
          maxTokens: savedCfg.model?.max_tokens ?? 4096,
          memory: savedCfg.memory,
          reasoning: savedCfg.reasoning,
          guardrails: savedCfg.guardrails,
        }
      );

      // Build structured portfolio context query
      const portfolioHoldings = portfolioSummary.holdings || [];
      const holdingsSummary = portfolioHoldings.map((h: any) =>
        `${h.symbol}: ${h.quantity} units @ $${(h.current_price || 0).toFixed(2)} | PnL: ${(h.unrealized_pnl_percent || 0).toFixed(2)}% | Weight: ${(h.weight || 0).toFixed(1)}%`
      ).join('\n');
      const portfolioContext = [
        `Portfolio: ${portfolioSummary.portfolio?.name || 'Unknown'} | NAV: ${currency} ${portfolioSummary.total_market_value.toFixed(2)}`,
        `Total P&L: ${portfolioSummary.total_unrealized_pnl_percent?.toFixed(2) ?? 'N/A'}% | Day Change: ${portfolioSummary.total_day_change_percent?.toFixed(2) ?? 'N/A'}%`,
        `Holdings (${portfolioHoldings.length}):`,
        holdingsSummary,
        '',
        'Please analyze this portfolio and provide actionable insights with specific recommendations.',
      ].join('\n');

      const result = await withTimeout(
        agentService.runAgent(portfolioContext, agentConfig, apiKeys),
        180000,
        'Agent timed out. Check your LLM API key and network connection.'
      );

      const agentText = result.response || result.error || 'No response returned.';
      agentResultCache.current[selectedAgentId] = agentText;
      setAgentResult(agentText);
    } catch (err: any) {
      setAgentResult(`Agent run failed: ${err?.message || 'Unknown error'}`);
    } finally {
      setAgentRunning(false);
    }
  }, [portfolioSummary, agentRunning, selectedAgentId, savedAgents, currency]);

  // Auto-select highest weighted holding when holdings change
  useEffect(() => {
    if (holdings.length > 0 && !selectedSymbol) {
      const topHolding = [...holdings].sort((a, b) => b.weight - a.weight)[0];
      setSelectedSymbol(topHolding.symbol);
    }
  }, [holdings, selectedSymbol]);

  // ── Workspace persistence ──
  const getWorkspaceState = useCallback(() => ({
    heatmapMode, detailView, showFFN,
    selectedPortfolioId: selectedPortfolio?.id ?? null,
  }), [heatmapMode, detailView, showFFN, selectedPortfolio]);
  const setWorkspaceState = useCallback((state: any) => {
    if (state.heatmapMode) setHeatmapMode(state.heatmapMode);
    if (state.detailView !== undefined) setDetailView(state.detailView);
    if (state.showFFN !== undefined) setShowFFN(state.showFFN);
    // Portfolio restoration is handled by usePortfolioOperations via localStorage
  }, []);
  useWorkspaceTabState('portfolio', getWorkspaceState, setWorkspaceState);

  // ── Sort handler ──
  const handleSort = useCallback((col: SortColumn) => {
    setSortCol(prev => {
      if (prev === col) { setSortDir(d => d === 'asc' ? 'desc' : 'asc'); return prev; }
      setSortDir('desc');
      return col;
    });
  }, []);

  // ── Derived data ──
  const selectedHolding = useMemo(() =>
    holdings.find(h => h.symbol === selectedSymbol) || null,
  [holdings, selectedSymbol]);

  // ── Buy/Sell with order panel toggle ──
  const handleBuy = useCallback(() => {
    setOrderSide('BUY');
    setShowOrderPanel(true);
  }, []);
  const handleSell = useCallback(() => {
    setOrderSide('SELL');
    setShowOrderPanel(true);
  }, []);

  // ── AI portfolio analysis ──
  const handleAnalyzeWithAI = useCallback(async (forceRefresh = false) => {
    if (!portfolioSummary || aiAnalyzing) return;
    // Invalidate cache if portfolio changed
    const currentPortfolioId = portfolioSummary.portfolio?.id ?? 'unknown';
    if (lastPortfolioId.current !== currentPortfolioId) {
      aiResultCache.current = {};
      agentResultCache.current = {};
      lastPortfolioId.current = currentPortfolioId;
    }
    // Always open the panel
    setShowAiPanel(true);
    // If we have a cached result and not forcing refresh, show it immediately
    if (!forceRefresh && aiResultCache.current[aiAnalysisType]) {
      setAiResult(aiResultCache.current[aiAnalysisType]);
      return;
    }
    setAiAnalyzing(true);
    setAiResult(null);
    try {
      const llmConfigs = await sqliteService.getLLMConfigs();

      // Build apiKeys from LLM configs
      const apiKeys: Record<string, string> = {};
      for (const cfg of llmConfigs) {
        if (cfg.api_key) {
          apiKeys[`${cfg.provider.toUpperCase()}_API_KEY`] = cfg.api_key;
          apiKeys[cfg.provider] = cfg.api_key;
          apiKeys[cfg.provider.toLowerCase()] = cfg.api_key;
        }
        if (cfg.base_url) {
          apiKeys[`${cfg.provider.toUpperCase()}_BASE_URL`] = cfg.base_url;
          apiKeys[`${cfg.provider}_base_url`] = cfg.base_url;
        }
      }

      // Always include the Fincept session key (available to every logged-in user)
      if (!apiKeys['fincept']) {
        const finceptKey = await getSetting('fincept_api_key');
        if (finceptKey) {
          apiKeys['fincept'] = finceptKey;
          apiKeys['FINCEPT_API_KEY'] = finceptKey;
        }
      }

      // Resolve which LLM to use — prefer active, fall back to fincept
      const activeLLM = llmConfigs.find(c => c.is_active) ?? llmConfigs[0] ?? null;
      const provider = activeLLM?.provider ?? 'fincept';
      const model_id = activeLLM?.model ?? 'fincept-llm';

      if (!activeLLM && !apiKeys['fincept']) {
        setAiResult('No LLM configured. Please add a model in Settings → LLM Configuration.');
        return;
      }

      const agentConfig = {
        model: { provider, model_id, temperature: 0.3 },
        instructions: 'You are a professional portfolio analyst with CFA-level expertise. Use ALL available tools to gather live data before making recommendations. Cite specific numbers in your analysis.',
        tools: [],
        markdown: true,
      };

      const result = await agentService.runPortfolioAnalysis(portfolioSummary, aiAnalysisType, apiKeys, agentConfig);
      const analysisText = result.response || result.error || 'No analysis returned.';
      aiResultCache.current[aiAnalysisType] = analysisText;
      setAiResult(analysisText);
    } catch (err: any) {
      setAiResult(`Analysis failed: ${err?.message || 'Unknown error'}`);
    } finally {
      setAiAnalyzing(false);
    }
  }, [portfolioSummary, aiAnalyzing, aiAnalysisType]);

  return (
    <div style={{
      height: '100%', display: 'flex', flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE,
      fontFamily, overflow: 'hidden', position: 'relative',
    }}>
      <style>{`
        ${COMMON_STYLES.scrollbarCSS}
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
        @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
        .port-row:hover { background-color: ${FINCEPT.HOVER} !important; }
      `}</style>

      {/* ═══ COMMAND BAR ═══ */}
      <CommandBar
        portfolios={portfolios}
        selectedPortfolio={selectedPortfolio}
        onSelectPortfolio={setSelectedPortfolio}
        portfolioSummary={portfolioSummary}
        onCreatePortfolio={() => modals.setShowCreatePortfolio(true)}
        onDeletePortfolio={modals.handleDeletePortfolio}
        onBuy={handleBuy}
        onSell={handleSell}
        onRefresh={operations.refreshPortfolioData}
        onExport={operations.exportToCSV}
        onExportJSON={operations.exportToJSON}
        onImport={() => modals.setShowImportPortfolio(true)}
        refreshing={refreshing}
        currency={currency}
        refreshIntervalMs={refreshIntervalMs}
        onSetRefreshInterval={setRefreshIntervalMs}
        showFFN={showFFN}
        onToggleFFN={() => { setShowFFN(v => !v); setDetailView(null); }}
        onAnalyzeWithAI={portfolioSummary ? handleAnalyzeWithAI : undefined}
        aiAnalyzing={aiAnalyzing}
        onRunAgent={portfolioSummary ? () => setShowAgentPanel(true) : undefined}
        agentRunning={agentRunning}
        detailView={detailView}
        onSetDetailView={selectedPortfolio ? (view) => { setDetailView(view); if (view) setShowFFN(false); } : undefined}
      />

      {/* ═══ STATS RIBBON — hidden when a full-screen view is active ═══ */}
      {!showFFN && !detailView && (
        <StatsRibbon
          portfolioSummary={portfolioSummary}
          metrics={metrics}
          currency={currency}
        />
      )}

      {/* ═══ MAIN CONTENT ═══ */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>

        {/* Empty state when nothing selected */}
        {!selectedPortfolio ? (
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
            <Briefcase size={36} color={FINCEPT.MUTED} style={{ opacity: 0.2, marginBottom: '16px' }} />
            <div style={{ color: FINCEPT.GRAY, fontSize: '12px', fontWeight: 700, letterSpacing: '1px' }}>
              {t('extracted.noPortfolioSelected', 'NO PORTFOLIO SELECTED')}
            </div>
            <div style={{ color: FINCEPT.MUTED, fontSize: '10px', marginTop: '6px' }}>
              {t('content.selectOrCreate', 'Select a portfolio or create a new one')}
            </div>
          </div>
        ) : !portfolioSummary ? (
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
            <RefreshCw size={16} color={FINCEPT.ORANGE} className="animate-spin" style={{ marginBottom: '12px' }} />
            <div style={{ color: FINCEPT.GRAY, fontSize: '11px', fontWeight: 600, letterSpacing: '0.5px' }}>
              {t('extracted.loadingPortfolioData', 'Loading portfolio data...')}
            </div>
          </div>
        ) : showFFN ? (
          /* ═══ FFN ANALYTICS VIEW ═══ */
          <div style={{ flex: 1, minWidth: 0, overflow: 'hidden' }}>
            <FFNView
              portfolioSummary={portfolioSummary}
              currency={currency}
              onBack={() => setShowFFN(false)}
            />
          </div>
        ) : detailView ? (
          /* ═══ FULL-SCREEN DETAIL VIEW ═══ */
          <div style={{ flex: 1, minWidth: 0, overflow: 'auto' }}>
            <DetailViewWrapper
              view={detailView}
              onBack={() => setDetailView(null)}
              portfolioSummary={portfolioSummary}
              transactions={transactions}
              selectedPortfolio={selectedPortfolio}
              currency={currency}
              indexRefreshKey={modals.indexRefreshKey}
              onCreateIndex={() => modals.setShowCreateIndex(true)}
            />
          </div>
        ) : (
          <>
            {/* ─── LEFT: Holdings Heatmap ─── */}
            <HoldingsHeatmap
              holdings={holdings}
              selectedSymbol={selectedSymbol}
              onSelectSymbol={setSelectedSymbol}
              heatmapMode={heatmapMode}
              onSetHeatmapMode={setHeatmapMode}
              metrics={metrics}
              currency={currency}
              portfolios={portfolios}
              selectedPortfolio={selectedPortfolio}
              onSelectPortfolio={setSelectedPortfolio}
              onCreatePortfolio={() => modals.setShowCreatePortfolio(true)}
            />

            {/* ─── CENTER ─── */}
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
              {/* Top: Chart + Sector/Correlation */}
              <div style={{ height: '42%', minHeight: '180px', display: 'flex', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <PerformanceChartPanel portfolioSummary={portfolioSummary} currency={currency} />
                <SectorCorrelationPanel holdings={holdings} currency={currency} />
              </div>

              {/* Middle: Positions Blotter */}
              <PositionsBlotter
                holdings={holdings}
                selectedSymbol={selectedSymbol}
                onSelectSymbol={setSelectedSymbol}
                sortCol={sortCol}
                sortDir={sortDir}
                onSort={handleSort}
                currency={currency}
              />

            </div>

            {/* ─── RIGHT: Order Panel ─── */}
            <OrderPanel
              show={showOrderPanel}
              onClose={() => setShowOrderPanel(false)}
              orderSide={orderSide}
              onSetOrderSide={setOrderSide}
              selectedHolding={selectedHolding}
              onSubmitBuy={() => modals.setShowAddAsset(true)}
              onSubmitSell={() => modals.setShowSellAsset(true)}
              currency={currency}
            />
          </>
        )}
      </div>

      {/* ═══ AI ANALYSIS PANEL ═══ */}
      {showAiPanel && (
        <div style={{
          position: 'absolute', top: '40px', right: '12px', zIndex: 200,
          width: '480px', maxHeight: '75vh',
          backgroundColor: '#0A0A0A', border: '1px solid #9D4EDD',
          boxShadow: '0 8px 32px rgba(157,78,221,0.3)',
          display: 'flex', flexDirection: 'column',
        }}>
          {/* Header */}
          <div style={{
            padding: '8px 12px', borderBottom: '1px solid #2A2A2A',
            display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            backgroundColor: '#9D4EDD18',
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Bot size={12} color="#9D4EDD" />
              <span style={{ fontSize: '10px', fontWeight: 800, color: '#9D4EDD', letterSpacing: '1px' }}>
                AI PORTFOLIO ANALYSIS
              </span>
              {aiAnalyzing && <RefreshCw size={9} color="#9D4EDD" className="animate-spin" />}
            </div>
            <button
              onClick={() => setShowAiPanel(false)}
              style={{ background: 'none', border: 'none', color: '#787878', cursor: 'pointer', padding: '2px', display: 'flex' }}
            >
              <X size={12} />
            </button>
          </div>

          {/* Analysis type selector */}
          <div style={{ display: 'flex', borderBottom: '1px solid #2A2A2A', backgroundColor: '#0F0F0F' }}>
            {(['full', 'risk', 'rebalance', 'opportunities'] as const).map(type => (
              <button
                key={type}
                onClick={() => {
                  if (aiAnalyzing) return;
                  setAiAnalysisType(type);
                  // Show cached result immediately if available
                  if (aiResultCache.current[type]) {
                    setAiResult(aiResultCache.current[type]);
                  } else {
                    setAiResult(null);
                  }
                }}
                disabled={aiAnalyzing}
                style={{
                  flex: 1, padding: '5px 4px',
                  backgroundColor: aiAnalysisType === type ? '#9D4EDD22' : 'transparent',
                  border: 'none',
                  borderBottom: aiAnalysisType === type ? '2px solid #9D4EDD' : '2px solid transparent',
                  color: aiAnalysisType === type ? '#9D4EDD' : '#787878',
                  fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
                  cursor: aiAnalyzing ? 'wait' : 'pointer', textTransform: 'uppercase',
                }}
              >
                {type === 'full' ? 'FULL' : type === 'risk' ? 'RISK' : type === 'rebalance' ? 'REBALANCE' : 'OPPORTUNITIES'}
              </button>
            ))}
          </div>

          {/* Run button when no result yet or to re-run */}
          {!aiAnalyzing && (
            <div style={{ padding: '6px 12px', borderBottom: '1px solid #1A1A1A', backgroundColor: '#080808' }}>
              <button
                onClick={() => {
                  // Clear cache entry so re-run fetches fresh data
                  delete aiResultCache.current[aiAnalysisType];
                  handleAnalyzeWithAI(true);
                }}
                style={{
                  width: '100%', padding: '5px',
                  backgroundColor: '#9D4EDD22', border: '1px solid #9D4EDD44',
                  color: '#9D4EDD', fontSize: '9px', fontWeight: 700,
                  letterSpacing: '0.5px', cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '5px',
                }}
              >
                <Bot size={9} />
                {aiResult ? `RE-RUN ${aiAnalysisType.toUpperCase()} ANALYSIS` : `RUN ${aiAnalysisType.toUpperCase()} ANALYSIS`}
              </button>
            </div>
          )}

          {/* Content — rendered markdown */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
            {aiAnalyzing ? (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', alignItems: 'center', paddingTop: '24px' }}>
                <RefreshCw size={16} color="#9D4EDD" className="animate-spin" />
                <span style={{ color: '#9D4EDD', fontSize: '10px', opacity: 0.8 }}>
                  Running {aiAnalysisType} analysis with live tools...
                </span>
              </div>
            ) : aiResult ? (
              <div style={{ fontSize: '11px', lineHeight: '1.6' }}>
                <MarkdownRenderer content={aiResult} />
              </div>
            ) : (
              <div style={{ color: '#555', fontSize: '10px', textAlign: 'center', paddingTop: '24px' }}>
                Select an analysis type above and click Run.
              </div>
            )}
          </div>
        </div>
      )}

      {/* ═══ AGENT RUNNER PANEL ═══ */}
      {showAgentPanel && (
        <div style={{
          position: 'absolute', top: '40px', right: '508px', zIndex: 200,
          width: '480px', maxHeight: '75vh',
          backgroundColor: '#0A0A0A', border: '1px solid #00D4AA',
          boxShadow: '0 8px 32px rgba(0,212,170,0.2)',
          display: 'flex', flexDirection: 'column',
        }}>
          {/* Header */}
          <div style={{
            padding: '8px 12px', borderBottom: '1px solid #2A2A2A',
            display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            backgroundColor: '#00D4AA12',
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Bot size={12} color="#00D4AA" />
              <span style={{ fontSize: '10px', fontWeight: 800, color: '#00D4AA', letterSpacing: '1px' }}>
                AGENT RUNNER
              </span>
              {agentRunning && <RefreshCw size={9} color="#00D4AA" className="animate-spin" />}
            </div>
            <button
              onClick={() => setShowAgentPanel(false)}
              style={{ background: 'none', border: 'none', color: '#787878', cursor: 'pointer', padding: '2px', display: 'flex' }}
            >
              <X size={12} />
            </button>
          </div>

          {/* Agent selector */}
          <div style={{ padding: '8px 12px', borderBottom: '1px solid #1A1A1A', backgroundColor: '#0D0D0D' }}>
            <div style={{ fontSize: '8px', color: '#555', fontWeight: 700, letterSpacing: '0.5px', marginBottom: '4px' }}>SELECT AGENT</div>
            <div style={{ position: 'relative' }}>
              <select
                value={selectedAgentId}
                onChange={e => {
                  const newId = e.target.value;
                  setSelectedAgentId(newId);
                  if (agentResultCache.current[newId]) {
                    setAgentResult(agentResultCache.current[newId]);
                  } else {
                    setAgentResult(null);
                  }
                }}
                disabled={agentRunning}
                style={{
                  width: '100%', padding: '5px 8px',
                  backgroundColor: '#0A0A0A', border: '1px solid #2A2A2A',
                  color: '#E0E0E0', fontSize: '10px', cursor: 'pointer',
                  appearance: 'none', outline: 'none',
                }}
              >
                {savedAgents.length === 0 && (
                  <option value="">No agents saved — create one in Agent Config tab</option>
                )}
                {savedAgents.map(a => (
                  <option key={a.id} value={a.id}>
                    {a.name}{a.description ? ' — ' + a.description.slice(0, 40) : ''}
                  </option>
                ))}
              </select>
              <ChevronDown size={10} color="#555" style={{ position: 'absolute', right: '8px', top: '50%', transform: 'translateY(-50%)', pointerEvents: 'none' }} />
            </div>

            {/* Run button */}
            {!agentRunning && savedAgents.length > 0 && (
              <button
                onClick={() => {
                  if (agentResult) delete agentResultCache.current[selectedAgentId];
                  handleRunAgent(agentResult ? true : false);
                }}
                style={{
                  marginTop: '6px', width: '100%', padding: '5px',
                  backgroundColor: '#00D4AA22', border: '1px solid #00D4AA44',
                  color: '#00D4AA', fontSize: '9px', fontWeight: 700,
                  letterSpacing: '0.5px', cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '5px',
                }}
              >
                <Bot size={9} />
                {agentResult ? 'RE-RUN AGENT' : 'RUN AGENT ON PORTFOLIO'}
              </button>
            )}
          </div>

          {/* Result */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
            {agentRunning ? (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', alignItems: 'center', paddingTop: '24px' }}>
                <RefreshCw size={16} color="#00D4AA" className="animate-spin" />
                <span style={{ color: '#00D4AA', fontSize: '10px', opacity: 0.8 }}>
                  Agent is analyzing your portfolio...
                </span>
              </div>
            ) : agentResult ? (
              <div style={{ fontSize: '11px', lineHeight: '1.6' }}>
                <MarkdownRenderer content={agentResult} />
              </div>
            ) : (
              <div style={{ color: '#555', fontSize: '10px', textAlign: 'center', paddingTop: '24px' }}>
                Select an agent above and click Run.
              </div>
            )}
          </div>
        </div>
      )}

      {/* ═══ STATUS BAR ═══ */}
      <StatusBar
        selectedPortfolio={selectedPortfolio}
        portfolioSummary={portfolioSummary}
        currency={currency}
        holdingsCount={holdings.length}
      />

      {/* ═══ MODALS ═══ */}
      <CreatePortfolioModal
        show={modals.showCreatePortfolio}
        formState={{ name: modals.newPortfolioName, owner: modals.newPortfolioOwner, currency: modals.newPortfolioCurrency }}
        onNameChange={modals.setNewPortfolioName}
        onOwnerChange={modals.setNewPortfolioOwner}
        onCurrencyChange={modals.setNewPortfolioCurrency}
        onClose={() => modals.setShowCreatePortfolio(false)}
        onCreate={modals.handleCreatePortfolio}
      />
      <AddAssetModal
        show={modals.showAddAsset}
        formState={{ symbol: modals.addAssetSymbol, quantity: modals.addAssetQuantity, price: modals.addAssetPrice }}
        onSymbolChange={modals.setAddAssetSymbol}
        onQuantityChange={modals.setAddAssetQuantity}
        onPriceChange={modals.setAddAssetPrice}
        onClose={() => modals.setShowAddAsset(false)}
        onAdd={modals.handleAddAsset}
      />
      <SellAssetModal
        show={modals.showSellAsset}
        formState={{ symbol: modals.sellAssetSymbol, quantity: modals.sellAssetQuantity, price: modals.sellAssetPrice }}
        onSymbolChange={modals.setSellAssetSymbol}
        onQuantityChange={modals.setSellAssetQuantity}
        onPriceChange={modals.setSellAssetPrice}
        onClose={() => modals.setShowSellAsset(false)}
        onSell={modals.handleSellAsset}
      />
      <ConfirmDeleteModal
        show={modals.showDeleteConfirm}
        portfolioName={portfolios.find(p => p.id === modals.portfolioToDelete)?.name || ''}
        onClose={() => { modals.setShowDeleteConfirm(false); }}
        onConfirm={modals.confirmDeletePortfolio}
      />
      <CreateIndexModal
        isOpen={modals.showCreateIndex}
        onClose={() => modals.setShowCreateIndex(false)}
        onCreated={modals.handleIndexCreated}
        portfolioSummary={portfolioSummary}
      />
      <ImportPortfolioModal
        show={modals.showImportPortfolio}
        portfolios={portfolios}
        onClose={() => modals.setShowImportPortfolio(false)}
        onImportComplete={modals.handleImportComplete}
        onImport={operations.importPortfolio}
      />
    </div>
  );
};

export default PortfolioTab;
