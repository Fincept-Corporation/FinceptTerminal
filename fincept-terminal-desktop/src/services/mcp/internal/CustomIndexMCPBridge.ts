// Custom Index MCP Bridge - Connects customIndexService to MCP Internal Tools

import { terminalMCPProvider } from './TerminalMCPProvider';
import { customIndexService } from '@/services/portfolio/customIndexService';
import type { IndexConstituentConfig } from '@/components/tabs/portfolio-tab/custom-index/types';

const CONTEXT_KEYS = [
  'createCustomIndex', 'getAllCustomIndices', 'getCustomIndexSummary', 'deleteCustomIndex',
  'addIndexConstituent', 'removeIndexConstituent', 'getIndexConstituents',
  'saveIndexSnapshot', 'getIndexSnapshots',
  'createIndexFromPortfolio', 'rebaseCustomIndex',
] as const;

export class CustomIndexMCPBridge {
  private connected = false;

  connect(): void {
    if (this.connected) return;

    terminalMCPProvider.setContexts({
      // ── CRUD ──────────────────────────────────────────────────────────────
      createCustomIndex: (params: {
        name: string;
        description?: string;
        calculation_method?: string;
        base_value?: number;
        currency?: string;
        cap_weight?: number;
        historical_start_date?: string;
        portfolio_id?: string;
      }) =>
        customIndexService.createIndex(
          params.name,
          params.description,
          (params.calculation_method as any) || 'equal_weighted',
          params.base_value ?? 100,
          params.cap_weight ?? 0,
          params.currency || 'USD',
          params.portfolio_id,
          params.historical_start_date,
        ),

      getAllCustomIndices: () => customIndexService.getAllIndices(),

      getCustomIndexSummary: (indexId: string) =>
        customIndexService.calculateIndexSummary(indexId),

      deleteCustomIndex: (indexId: string) =>
        customIndexService.deleteIndex(indexId),

      // ── Constituents ──────────────────────────────────────────────────────
      addIndexConstituent: (indexId: string, config: IndexConstituentConfig) =>
        customIndexService.addConstituent(indexId, config),

      removeIndexConstituent: (indexId: string, symbol: string) =>
        customIndexService.removeConstituent(indexId, symbol),

      getIndexConstituents: (indexId: string) =>
        customIndexService.getConstituents(indexId),

      // ── Snapshots ─────────────────────────────────────────────────────────
      saveIndexSnapshot: (indexId: string) =>
        customIndexService.saveDailySnapshot(indexId),

      getIndexSnapshots: (indexId: string, limit?: number) =>
        customIndexService.getSnapshots(indexId, limit || 30),

      // ── Utilities ─────────────────────────────────────────────────────────
      createIndexFromPortfolio: (
        portfolioId: string,
        indexName: string,
        method: string,
        baseValue: number,
      ) =>
        customIndexService.createIndexFromPortfolio(portfolioId, indexName, method as any, baseValue),

      rebaseCustomIndex: (indexId: string, newBaseValue: number) =>
        customIndexService.rebaseIndex(indexId, newBaseValue),
    });

    this.connected = true;
    console.log('[CustomIndexMCPBridge] Connected');
  }

  disconnect(): void {
    if (!this.connected) return;
    const cleared = Object.fromEntries(CONTEXT_KEYS.map(k => [k, undefined]));
    terminalMCPProvider.setContexts(cleared);
    this.connected = false;
    console.log('[CustomIndexMCPBridge] Disconnected');
  }

  isConnected(): boolean {
    return this.connected;
  }
}

export const customIndexMCPBridge = new CustomIndexMCPBridge();
