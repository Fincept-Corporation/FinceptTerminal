/**
 * Surface Analytics - Databento API Types
 *
 * Interface and type definitions for the Databento API hook.
 * Covers symbol resolution, dataset metadata, live streaming,
 * futures, batch download, and additional schema record types.
 */

import type { DatabentoResponse } from '../types';

export interface SymbolResolution {
  raw_symbol: string;
  instrument_id: string;
  start_date?: string;
  end_date?: string;
}

export interface DatasetInfo {
  id: string;
  name: string;
  start_date?: string;
  end_date?: string;
  schemas?: string[];
}

export interface CostEstimate {
  dataset: string;
  symbols: string[];
  schema: string;
  record_count: number;
  cost_usd: number;
}

export interface SecurityMaster {
  raw_symbol: string;
  instrument_id: string;
  security_type: string;
  exchange: string;
  currency: string;
  name: string;
  description: string;
  sector: string;
  industry: string;
  market_cap: number | null;
  shares_outstanding: number | null;
}

export interface CorporateAction {
  raw_symbol: string;
  action_type: string;
  ex_date: string;
  record_date: string;
  payment_date: string;
  amount: number | null;
  ratio: string | null;
  currency: string;
  description: string;
}

export interface AdjustmentFactor {
  raw_symbol: string;
  date: string;
  split_factor: number;
  dividend_factor: number;
  cumulative_factor: number;
}

// Live Streaming Types
export interface LiveStreamInfo {
  databento_available: boolean;
  schemas: string[];
  datasets: string[];
  stype_options: string[];
}

export interface LiveStreamRecord {
  type: 'data' | 'status' | 'error' | 'info';
  timestamp: number;
  record?: {
    record_type: string;
    ts_event: string;
    symbol: string;
    schema: string;
    price?: number;
    size?: number;
    side?: string;
    levels?: Array<{
      bid_px: number;
      ask_px: number;
      bid_sz: number;
      ask_sz: number;
    }>;
    [key: string]: unknown;
  };
  message?: string;
  connected?: boolean;
}

export type LiveStreamCallback = (record: LiveStreamRecord) => void;

// Futures Types
export interface FuturesContract {
  symbol: string;
  smart_symbol: string;
  name: string;
  exchange: string;
  tick_size: number;
  multiplier: number;
}

export interface FuturesDefinition {
  raw_symbol: string;
  instrument_id: number;
  exchange: string;
  underlying: string;
  expiration: string | null;
  contract_multiplier: number;
  min_price_increment: number;
  currency: string;
}

export interface TermStructurePoint {
  contract_month: number;
  smart_symbol: string;
  close: number;
  volume: number;
}

// Batch Download Types
export interface BatchJob {
  job_id: string;
  state: 'queued' | 'processing' | 'done' | 'expired';
  dataset: string;
  schema: string;
  symbols: string[];
  start: string;
  end: string;
  encoding: string;
  compression: string;
  cost: number;
  record_count: number;
  ts_received: string;
  ts_process_start: string;
  ts_process_done: string;
  size_bytes: number;
}

export interface BatchFile {
  filename: string;
  size_bytes: number;
  hash: string;
  urls: Record<string, string>;
}

export interface BatchJobSubmitResult {
  job_id: string;
  state: string;
  dataset: string;
  symbols: string[];
  schema: string;
  start_date: string;
  end_date: string;
  encoding: string;
  compression: string;
  cost: number;
  record_count: number;
  ts_received: string;
}

// Additional Schema Types
export interface TradeRecord {
  ts_event: string;
  symbol: string;
  price: number;
  size: number;
  action: string | null;
  side: string | null;
  sequence: number;
}

export interface ImbalanceRecord {
  ts_event: string;
  symbol: string;
  ref_price: number;
  paired_qty: number;
  imbalance_qty: number;
  imbalance_side: string | null;
  auction_type: string | null;
}

export interface StatisticsRecord {
  ts_event: string;
  symbol: string;
  stat_type: string | null;
  price: number;
  quantity: number;
  sequence: number;
  ts_ref: string | null;
}

export interface StatusRecord {
  ts_event: string;
  symbol: string;
  action: string | null;
  reason: string | null;
  trading_event: string | null;
  is_short_sell_restricted: boolean | null;
}

export interface UseDatabentoApiResult {
  // Data fetching
  testConnection: () => Promise<DatabentoResponse>;
  fetchOptionsChain: (symbol: string, date?: string) => Promise<DatabentoResponse>;
  fetchOptionsDefinitions: (symbol: string, date?: string) => Promise<DatabentoResponse>;
  fetchHistoricalBars: (symbols: string[], days?: number, schema?: string) => Promise<DatabentoResponse>;
  fetchHistoricalOHLCV: (symbols: string[], days?: number, dataset?: string, schema?: string) => Promise<DatabentoResponse>;

  // Symbol search & resolution
  resolveSymbols: (symbols: string[], dataset?: string) => Promise<DatabentoResponse<SymbolResolution[]>>;
  searchSymbol: (query: string, dataset?: string) => Promise<DatabentoResponse<SymbolResolution[]>>;

  // Metadata
  listDatasets: () => Promise<DatabentoResponse<DatasetInfo[]>>;
  getDatasetInfo: (dataset: string) => Promise<DatabentoResponse<DatasetInfo>>;
  listSchemas: (dataset?: string) => Promise<DatabentoResponse<string[]>>;

  // Cost estimation
  getCostEstimate: (dataset: string, symbols: string[], schema: string, startDate: string, endDate: string) => Promise<DatabentoResponse<CostEstimate>>;

  // Reference Data
  getSecurityMaster: (symbols: string[], dataset?: string) => Promise<DatabentoResponse<{ securities: SecurityMaster[] }>>;
  getCorporateActions: (symbols: string[], startDate?: string, endDate?: string, actionType?: string) => Promise<DatabentoResponse<{ actions: CorporateAction[] }>>;
  getAdjustmentFactors: (symbols: string[], startDate?: string, endDate?: string) => Promise<DatabentoResponse<{ factors: AdjustmentFactor[] }>>;

  // Live Streaming
  getLiveStreamInfo: () => Promise<DatabentoResponse<LiveStreamInfo>>;
  startLiveStream: (
    dataset: string,
    schema: string,
    symbols: string[],
    callback: LiveStreamCallback,
    options?: { stypeIn?: string; snapshot?: boolean }
  ) => Promise<{ streamId: string } | { error: true; message: string }>;
  stopLiveStream: (streamId: string) => Promise<DatabentoResponse>;
  stopAllLiveStreams: () => Promise<DatabentoResponse>;
  listLiveStreams: () => Promise<DatabentoResponse<{ streams: string[]; count: number }>>;
  activeStreams: string[];

  // Futures (GLBX.MDP3, SMART symbology)
  listFuturesContracts: () => Promise<DatabentoResponse<{ contracts: FuturesContract[]; count: number }>>;
  getFuturesData: (symbols: string[], days?: number, schema?: string, stypeIn?: string) => Promise<DatabentoResponse>;
  getFuturesDefinitions: (symbols: string[], date?: string) => Promise<DatabentoResponse<{ definitions: FuturesDefinition[] }>>;
  getFuturesTermStructure: (rootSymbol: string, numContracts?: number) => Promise<DatabentoResponse<{ term_structure: TermStructurePoint[] }>>;

  // Batch Download
  submitBatchJob: (
    dataset: string,
    symbols: string[],
    schema: string,
    startDate: string,
    endDate: string,
    options?: {
      encoding?: string;
      compression?: string;
      stypeIn?: string;
      splitDuration?: string;
      delivery?: string;
    }
  ) => Promise<DatabentoResponse<BatchJobSubmitResult>>;
  listBatchJobs: (states?: string, sinceDays?: number) => Promise<DatabentoResponse<{ jobs: BatchJob[]; count: number }>>;
  listBatchFiles: (jobId: string) => Promise<DatabentoResponse<{ files: BatchFile[]; count: number; total_size_bytes: number }>>;
  downloadBatchJob: (jobId: string, outputDir?: string, filename?: string) => Promise<DatabentoResponse<{ files: string[]; count: number; output_dir: string }>>;

  // Additional Schemas
  getTrades: (symbols: string[], dataset?: string, days?: number, limit?: number) => Promise<DatabentoResponse<{ trades: TradeRecord[]; count: number }>>;
  getImbalance: (symbols: string[], dataset?: string, days?: number) => Promise<DatabentoResponse<{ imbalances: ImbalanceRecord[]; count: number }>>;
  getStatistics: (symbols: string[], dataset?: string, days?: number) => Promise<DatabentoResponse<{ statistics: StatisticsRecord[]; count: number }>>;
  getStatus: (symbols: string[], dataset?: string, days?: number) => Promise<DatabentoResponse<{ statuses: StatusRecord[]; count: number }>>;
  getSchemaData: (symbols: string[], schema: string, dataset?: string, days?: number, limit?: number) => Promise<DatabentoResponse>;

  // API key management
  apiKey: string | null;
  loadApiKey: () => Promise<void>;
  hasApiKey: boolean;

  // State
  loading: boolean;
  error: string | null;
}
