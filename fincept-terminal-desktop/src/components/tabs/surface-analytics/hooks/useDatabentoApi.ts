/**
 * Surface Analytics - Databento API Hook
 * Handles all Databento API interactions for volatility surfaces,
 * correlation matrices, and historical OHLCV data.
 *
 * Types extracted to ./databentoTypes.ts
 */

import { useState, useCallback, useRef, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { listen, UnlistenFn } from '@tauri-apps/api/event';
import { sqliteService } from '@/services/core/sqliteService';
import type { DatabentoResponse } from '../types';
import type {
  SymbolResolution,
  DatasetInfo,
  CostEstimate,
  SecurityMaster,
  CorporateAction,
  AdjustmentFactor,
  LiveStreamInfo,
  LiveStreamRecord,
  LiveStreamCallback,
  FuturesContract,
  FuturesDefinition,
  TermStructurePoint,
  BatchJob,
  BatchFile,
  BatchJobSubmitResult,
  TradeRecord,
  ImbalanceRecord,
  StatisticsRecord,
  StatusRecord,
  UseDatabentoApiResult,
} from './databentoTypes';

export function useDatabentoApi(): UseDatabentoApiResult {
  const [apiKey, setApiKey] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [activeStreams, setActiveStreams] = useState<string[]>([]);

  // Store event listeners for cleanup
  const streamListeners = useRef<Map<string, UnlistenFn>>(new Map());

  // Cleanup listeners on unmount
  useEffect(() => {
    return () => {
      // Cleanup all active stream listeners
      streamListeners.current.forEach((unlisten) => unlisten());
      streamListeners.current.clear();
    };
  }, []);

  const loadApiKey = useCallback(async () => {
    try {
      const key = await sqliteService.getApiKey('DATABENTO_API_KEY');
      setApiKey(key);
    } catch (err) {
      console.error('Failed to load Databento API key:', err);
      setError('Failed to load API key');
    }
  }, []);

  /**
   * Test the Databento API connection with current API key
   */
  const testConnection = useCallback(async (): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured. Please add it in Settings.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_test_connection', {
        apiKey: apiKey,
      });
      return JSON.parse(result);
    } catch (err) {
      const errorMsg = `Connection test failed: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Fetch options chain data for volatility surface
   * NOTE: Requires OPRA.PILLAR access (paid dataset)
   */
  const fetchOptionsChain = useCallback(async (
    symbol: string,
    date?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured. Please add it in Settings > Credentials.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_options_chain', {
        apiKey: apiKey,
        symbol,
        date: date || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch options chain: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Fetch options definitions (instrument metadata)
   * NOTE: Requires OPRA.PILLAR access (paid dataset)
   */
  const fetchOptionsDefinitions = useCallback(async (
    symbol: string,
    date?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_options_definitions', {
        apiKey: apiKey,
        symbol,
        date: date || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch options definitions: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Fetch multi-asset historical data for correlation matrix
   * Uses DBEQ.BASIC (free) first, falls back to XNAS.ITCH
   * Supports multiple timeframes: ohlcv-1d, ohlcv-1h, ohlcv-1m, ohlcv-1s, ohlcv-eod
   */
  const fetchHistoricalBars = useCallback(async (
    symbols: string[],
    days: number = 60,
    schema?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured. Please add it in Settings > Credentials.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_multi_asset_data', {
        apiKey: apiKey,
        symbols,
        days,
        schema: schema || 'ohlcv-1d',
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch historical data: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Fetch historical OHLCV data with optional dataset and schema override
   * Supports multiple timeframes: ohlcv-1d, ohlcv-1h, ohlcv-1m, ohlcv-1s, ohlcv-eod
   */
  const fetchHistoricalOHLCV = useCallback(async (
    symbols: string[],
    days: number = 30,
    dataset?: string,
    schema?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_historical_ohlcv', {
        apiKey: apiKey,
        symbols,
        days,
        dataset: dataset || null,
        schema: schema || 'ohlcv-1d',
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch OHLCV data: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Resolve/validate symbols using Databento symbology
   */
  const resolveSymbols = useCallback(async (
    symbols: string[],
    dataset?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_resolve_symbols', {
        apiKey: apiKey,
        symbols,
        dataset: dataset || null,
        stypeIn: null,
        stypeOut: null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to resolve symbols: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Search for a symbol (wrapper around resolveSymbols for single symbol)
   */
  const searchSymbol = useCallback(async (
    query: string,
    dataset?: string
  ): Promise<DatabentoResponse> => {
    return resolveSymbols([query.toUpperCase().trim()], dataset);
  }, [resolveSymbols]);

  /**
   * List all available datasets
   */
  const listDatasets = useCallback(async (): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_list_datasets', {
        apiKey: apiKey,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to list datasets: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get detailed info about a dataset
   */
  const getDatasetInfo = useCallback(async (
    dataset: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_dataset_info', {
        apiKey: apiKey,
        dataset,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to get dataset info: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * List available schemas for a dataset
   */
  const listSchemas = useCallback(async (
    dataset?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_list_schemas', {
        apiKey: apiKey,
        dataset: dataset || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to list schemas: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get cost estimate before executing a query
   */
  const getCostEstimate = useCallback(async (
    dataset: string,
    symbols: string[],
    schema: string,
    startDate: string,
    endDate: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_cost_estimate', {
        apiKey: apiKey,
        dataset,
        symbols,
        schema,
        startDate,
        endDate,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to get cost estimate: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get security master data (instrument metadata, fundamental info)
   */
  const getSecurityMaster = useCallback(async (
    symbols: string[],
    dataset?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_security_master', {
        apiKey: apiKey,
        symbols,
        dataset: dataset || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to get security master: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get corporate actions (dividends, splits, spinoffs, etc.)
   */
  const getCorporateActions = useCallback(async (
    symbols: string[],
    startDate?: string,
    endDate?: string,
    actionType?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_corporate_actions', {
        apiKey: apiKey,
        symbols,
        startDate: startDate || null,
        endDate: endDate || null,
        actionType: actionType || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to get corporate actions: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get adjustment factors for historical price adjustments
   */
  const getAdjustmentFactors = useCallback(async (
    symbols: string[],
    startDate?: string,
    endDate?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_adjustment_factors', {
        apiKey: apiKey,
        symbols,
        startDate: startDate || null,
        endDate: endDate || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to get adjustment factors: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  // ==========================================================================
  // Live Streaming API
  // ==========================================================================

  /**
   * Get live streaming capabilities and info
   */
  const getLiveStreamInfo = useCallback(async (): Promise<DatabentoResponse> => {
    try {
      const result = await invoke<string>('databento_live_info');
      return JSON.parse(result);
    } catch (err) {
      const errorMsg = `Failed to get live stream info: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    }
  }, []);

  /**
   * Start a live data stream
   */
  const startLiveStream = useCallback(async (
    dataset: string,
    schema: string,
    symbols: string[],
    callback: LiveStreamCallback,
    options?: { stypeIn?: string; snapshot?: boolean }
  ): Promise<{ streamId: string } | { error: true; message: string }> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
      };
    }

    try {
      const result = await invoke<string>('databento_live_start', {
        apiKey: apiKey,
        dataset,
        schema,
        symbols,
        stypeIn: options?.stypeIn || 'raw_symbol',
        snapshot: options?.snapshot || false,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        return { error: true, message: parsed.message };
      }

      const streamId = parsed.stream_id;

      // Set up event listener for this stream
      const eventName = `databento-live-${streamId}`;
      const unlisten = await listen<LiveStreamRecord>(eventName, (event) => {
        callback(event.payload);
      });

      // Store listener for cleanup
      streamListeners.current.set(streamId, unlisten);
      setActiveStreams((prev) => [...prev, streamId]);

      return { streamId };
    } catch (err) {
      const errorMsg = `Failed to start live stream: ${err}`;
      setError(errorMsg);
      return { error: true, message: errorMsg };
    }
  }, [apiKey]);

  /**
   * Stop a live data stream
   */
  const stopLiveStream = useCallback(async (
    streamId: string
  ): Promise<DatabentoResponse> => {
    try {
      // Remove event listener
      const unlisten = streamListeners.current.get(streamId);
      if (unlisten) {
        unlisten();
        streamListeners.current.delete(streamId);
      }

      const result = await invoke<string>('databento_live_stop', {
        streamId,
      });
      const parsed = JSON.parse(result);

      // Update active streams
      setActiveStreams((prev) => prev.filter((id) => id !== streamId));

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to stop live stream: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    }
  }, []);

  /**
   * Stop all active live streams
   */
  const stopAllLiveStreams = useCallback(async (): Promise<DatabentoResponse> => {
    try {
      // Remove all event listeners
      streamListeners.current.forEach((unlisten) => unlisten());
      streamListeners.current.clear();

      const result = await invoke<string>('databento_live_stop_all');
      const parsed = JSON.parse(result);

      setActiveStreams([]);

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to stop all streams: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    }
  }, []);

  /**
   * List all active live streams
   */
  const listLiveStreams = useCallback(async (): Promise<DatabentoResponse> => {
    try {
      const result = await invoke<string>('databento_live_list');
      return JSON.parse(result);
    } catch (err) {
      const errorMsg = `Failed to list live streams: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    }
  }, []);

  // ==========================================================================
  // Futures API (GLBX.MDP3, SMART symbology)
  // ==========================================================================

  /**
   * List available futures contracts with specifications
   */
  const listFuturesContracts = useCallback(async (): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    try {
      const result = await invoke<string>('databento_list_futures', {
        apiKey: apiKey,
      });
      return JSON.parse(result);
    } catch (err) {
      const errorMsg = `Failed to list futures contracts: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    }
  }, [apiKey]);

  /**
   * Fetch futures data using SMART symbology
   */
  const getFuturesData = useCallback(async (
    symbols: string[],
    days: number = 30,
    schema: string = 'ohlcv-1d',
    stypeIn: string = 'smart'
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_futures_data', {
        apiKey: apiKey,
        symbols,
        days,
        schema,
        stypeIn,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch futures data: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get futures contract definitions
   */
  const getFuturesDefinitions = useCallback(async (
    symbols: string[],
    date?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_futures_definitions', {
        apiKey: apiKey,
        symbols,
        date: date || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch futures definitions: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get futures term structure (curve)
   */
  const getFuturesTermStructure = useCallback(async (
    rootSymbol: string,
    numContracts: number = 6
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_futures_term_structure', {
        apiKey: apiKey,
        rootSymbol,
        numContracts,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch term structure: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  // ==========================================================================
  // Batch Download API
  // ==========================================================================

  /**
   * Submit a batch download job for large data requests
   */
  const submitBatchJob = useCallback(async (
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
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_submit_batch_job', {
        apiKey: apiKey,
        dataset,
        symbols,
        schema,
        startDate,
        endDate,
        encoding: options?.encoding || null,
        compression: options?.compression || null,
        stypeIn: options?.stypeIn || null,
        splitDuration: options?.splitDuration || null,
        delivery: options?.delivery || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to submit batch job: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * List batch download jobs
   */
  const listBatchJobs = useCallback(async (
    states?: string,
    sinceDays?: number
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_list_batch_jobs', {
        apiKey: apiKey,
        states: states || null,
        sinceDays: sinceDays || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to list batch jobs: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * List files from a batch job
   */
  const listBatchFiles = useCallback(async (
    jobId: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_list_batch_files', {
        apiKey: apiKey,
        jobId,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to list batch files: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Download files from a completed batch job
   */
  const downloadBatchJob = useCallback(async (
    jobId: string,
    outputDir?: string,
    filename?: string
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_download_batch_job', {
        apiKey: apiKey,
        jobId,
        outputDir: outputDir || null,
        filename: filename || null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to download batch job: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  // ==========================================================================
  // Additional Schemas API (TRADES, IMBALANCE, STATISTICS, STATUS)
  // ==========================================================================

  /**
   * Get trades data for symbols
   */
  const getTrades = useCallback(async (
    symbols: string[],
    dataset?: string,
    days: number = 1,
    limit: number = 1000
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_trades', {
        apiKey: apiKey,
        symbols,
        dataset: dataset || null,
        days,
        limit,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch trades: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get auction imbalance data
   */
  const getImbalance = useCallback(async (
    symbols: string[],
    dataset?: string,
    days: number = 1
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_imbalance', {
        apiKey: apiKey,
        symbols,
        dataset: dataset || null,
        days,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch imbalance data: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get exchange statistics
   */
  const getStatistics = useCallback(async (
    symbols: string[],
    dataset?: string,
    days: number = 5
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_statistics', {
        apiKey: apiKey,
        symbols,
        dataset: dataset || null,
        days,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch statistics: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get instrument status
   */
  const getStatus = useCallback(async (
    symbols: string[],
    dataset?: string,
    days: number = 5
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_status', {
        apiKey: apiKey,
        symbols,
        dataset: dataset || null,
        days,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch status data: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  /**
   * Get generic schema data
   */
  const getSchemaData = useCallback(async (
    symbols: string[],
    schema: string,
    dataset?: string,
    days: number = 1,
    limit: number = 1000
  ): Promise<DatabentoResponse> => {
    if (!apiKey) {
      return {
        error: true,
        message: 'Databento API key not configured.',
        timestamp: Date.now(),
      };
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_schema_data', {
        apiKey: apiKey,
        symbols,
        schema,
        dataset: dataset || null,
        days,
        limit,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      }

      return parsed;
    } catch (err) {
      const errorMsg = `Failed to fetch ${schema} data: ${err}`;
      setError(errorMsg);
      return {
        error: true,
        message: errorMsg,
        timestamp: Date.now(),
      };
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  return {
    testConnection,
    fetchOptionsChain,
    fetchOptionsDefinitions,
    fetchHistoricalBars,
    fetchHistoricalOHLCV,
    resolveSymbols,
    searchSymbol,
    listDatasets,
    getDatasetInfo,
    listSchemas,
    getCostEstimate,
    // Reference Data
    getSecurityMaster,
    getCorporateActions,
    getAdjustmentFactors,
    // Live Streaming
    getLiveStreamInfo,
    startLiveStream,
    stopLiveStream,
    stopAllLiveStreams,
    listLiveStreams,
    activeStreams,
    // Futures
    listFuturesContracts,
    getFuturesData,
    getFuturesDefinitions,
    getFuturesTermStructure,
    // Batch Download
    submitBatchJob,
    listBatchJobs,
    listBatchFiles,
    downloadBatchJob,
    // Additional Schemas
    getTrades,
    getImbalance,
    getStatistics,
    getStatus,
    getSchemaData,
    apiKey,
    loadApiKey,
    hasApiKey: !!apiKey,
    loading,
    error,
  };
}
