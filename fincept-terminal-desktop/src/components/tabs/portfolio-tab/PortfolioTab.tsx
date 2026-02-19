/**
 * Portfolio Management Tab - Fincept Terminal
 * Bloomberg-style dense multi-panel layout with real-time data
 * Thin shell composing modular sub-components
 */

import React, { useState, useCallback, useMemo } from 'react';
import { useEffect } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { Briefcase, RefreshCw, Bot, X } from 'lucide-react';
import { FINCEPT, COMMON_STYLES } from './finceptStyles';
import agentService from '@/services/agentService';
import { sqliteService } from '@/services/core/sqliteService';
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
  const handleAnalyzeWithAI = useCallback(async () => {
    if (!portfolioSummary || aiAnalyzing) return;
    setAiAnalyzing(true);
    setShowAiPanel(true);
    setAiResult(null);
    try {
      // Load active LLM config from DB (same pattern as Agent Config tab)
      const llmConfigs = await sqliteService.getLLMConfigs();
      const activeLLM = llmConfigs.find(c => c.is_active) ?? llmConfigs[0] ?? null;

      if (!activeLLM) {
        setAiResult('No LLM configured. Please add a model in Settings → LLM Configuration.');
        return;
      }

      // Build apiKeys map (same as useAgentConfig.getApiKeys)
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

      // Build AgentConfig using active LLM
      const agentConfig = {
        model: {
          provider: activeLLM.provider,
          model_id: activeLLM.model,
          temperature: 0.7,
          max_tokens: 4096,
        },
        instructions: 'You are a professional portfolio analyst with CFA-level expertise. Provide clear, actionable financial analysis.',
        tools: [],
      };

      const result = await agentService.runPortfolioAnalysis(portfolioSummary, 'full', apiKeys, agentConfig);
      setAiResult(result.response || result.error || 'No analysis returned.');
    } catch (err: any) {
      setAiResult(`Analysis failed: ${err?.message || 'Unknown error'}`);
    } finally {
      setAiAnalyzing(false);
    }
  }, [portfolioSummary, aiAnalyzing]);

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
          <div style={{ flex: 1, minWidth: 0, overflow: 'hidden' }}>
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
          width: '420px', maxHeight: '70vh',
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
          {/* Content */}
          <div style={{
            flex: 1, overflowY: 'auto', padding: '12px',
            fontSize: '11px', color: '#FFFFFF', lineHeight: '1.6',
            whiteSpace: 'pre-wrap', fontFamily: '"IBM Plex Mono", monospace',
          }}>
            {aiAnalyzing ? (
              <div style={{ color: '#9D4EDD', opacity: 0.7 }}>Analyzing portfolio with AI agent...</div>
            ) : aiResult ? (
              aiResult
            ) : null}
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
