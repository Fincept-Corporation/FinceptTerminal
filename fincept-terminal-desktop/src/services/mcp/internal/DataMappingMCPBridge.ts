// Data Mapping MCP Bridge - Connects data mapping services to MCP internal tools

import { terminalMCPProvider } from './TerminalMCPProvider';
import { mappingDatabase } from '@/components/tabs/data-mapping/services/MappingDatabase';
import { apiClient } from '@/components/tabs/data-mapping/services/APIClient';
import { mappingEngine } from '@/components/tabs/data-mapping/engine/MappingEngine';
import { UNIFIED_SCHEMAS } from '@/components/tabs/data-mapping/schemas';
import { INDIAN_BROKER_TEMPLATES } from '@/components/tabs/data-mapping/templates/indian-brokers';
import { DataMappingConfig, ParserEngine, APIAuthConfig } from '@/components/tabs/data-mapping/types';
import type {
  DataMappingSummary,
  DataMappingDetail,
  CreateDataMappingParams,
  DataMappingExecutionResult,
  DataMappingTemplateSummary,
  DataMappingSchemaSummary,
} from './types';

/**
 * Converts a full DataMappingConfig to a lightweight summary for list views
 */
function toSummary(config: DataMappingConfig): DataMappingSummary {
  return {
    id: config.id,
    name: config.name,
    description: config.description,
    schemaType: config.target.schemaType,
    schema: config.target.schema,
    method: config.source.apiConfig?.method || 'GET',
    baseUrl: config.source.apiConfig?.baseUrl || '',
    endpoint: config.source.apiConfig?.endpoint || '',
    fieldCount: config.fieldMappings.length,
    cacheEnabled: config.source.apiConfig?.cacheEnabled ?? false,
    createdAt: config.metadata.createdAt,
    updatedAt: config.metadata.updatedAt,
    tags: config.metadata.tags,
  };
}

/**
 * Converts a full DataMappingConfig to the detailed view
 */
function toDetail(config: DataMappingConfig): DataMappingDetail {
  return {
    ...toSummary(config),
    authentication: {
      type: config.source.apiConfig?.authentication.type || 'none',
    },
    headers: config.source.apiConfig?.headers || {},
    queryParams: config.source.apiConfig?.queryParams || {},
    fieldMappings: config.fieldMappings.map(fm => ({
      targetField: fm.targetField,
      sourceExpression: fm.source?.path || fm.sourceExpression || '',
      parser: fm.parser || 'direct',
    })),
    extraction: {
      engine: config.extraction.engine,
      rootPath: config.extraction.rootPath,
      isArray: config.extraction.isArray,
    },
    validation: {
      enabled: config.validation.enabled,
      strictMode: config.validation.strictMode,
    },
  };
}

/**
 * Builds a DataMappingConfig from CreateDataMappingParams
 */
function buildConfig(params: CreateDataMappingParams): DataMappingConfig {
  const now = new Date().toISOString();
  const id = `map_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`;

  // Build authentication config
  const authConfig: DataMappingConfig['source']['apiConfig'] = {
    id,
    name: params.name,
    description: params.description || '',
    baseUrl: params.baseUrl,
    endpoint: params.endpoint,
    method: params.method || 'GET',
    authentication: {
      type: params.authType || 'none',
      config: buildAuthConfig(params),
    },
    headers: params.headers || {},
    queryParams: params.queryParams || {},
    cacheEnabled: params.cacheEnabled ?? true,
    cacheTTL: params.cacheTTL ?? 300,
  };

  return {
    id,
    name: params.name,
    description: params.description || '',
    source: {
      type: 'api',
      apiConfig: authConfig,
    },
    target: {
      schemaType: params.schemaType || 'predefined',
      schema: params.schema || 'OHLCV',
    },
    extraction: {
      engine: ParserEngine.JSONPATH,
      rootPath: params.rootPath,
      isArray: params.isArray ?? true,
    },
    fieldMappings: [],
    validation: {
      enabled: false,
      strictMode: false,
      rules: [],
    },
    metadata: {
      createdAt: now,
      updatedAt: now,
      version: 1,
      tags: params.tags || [],
      aiGenerated: true,
    },
  };
}

function buildAuthConfig(params: CreateDataMappingParams): APIAuthConfig['config'] | undefined {
  if (!params.authType || params.authType === 'none') return undefined;

  if (params.authType === 'bearer' && params.authToken) {
    return { bearer: { token: params.authToken } };
  }
  if (params.authType === 'apikey' && params.authToken) {
    return {
      apiKey: {
        location: params.authKeyLocation || 'header',
        keyName: params.authKeyName || 'X-API-Key',
        value: params.authToken,
      },
    };
  }
  if (params.authType === 'basic' && params.authUsername) {
    return {
      basic: {
        username: params.authUsername,
        password: params.authPassword || '',
      },
    };
  }
  return undefined;
}

/**
 * Bridge that connects data mapping services to MCP tools
 * Allows AI to fully automate the Data Mapping tab
 */
export class DataMappingMCPBridge {
  private connected = false;

  connect(): void {
    if (this.connected) return;

    terminalMCPProvider.setContexts({

      // ── List all mappings ─────────────────────────────────────────────────
      listDataMappings: async (): Promise<DataMappingSummary[]> => {
        const configs = await mappingDatabase.getAllMappings();
        return configs.map(toSummary);
      },

      // ── Get single mapping ────────────────────────────────────────────────
      getDataMapping: async (id: string): Promise<DataMappingDetail | null> => {
        const config = await mappingDatabase.getMapping(id);
        if (!config) return null;
        return toDetail(config);
      },

      // ── Create mapping ────────────────────────────────────────────────────
      createDataMapping: async (params: CreateDataMappingParams): Promise<DataMappingSummary> => {
        const config = buildConfig(params);
        await mappingDatabase.saveMapping(config);
        return toSummary(config);
      },

      // ── Update mapping ────────────────────────────────────────────────────
      updateDataMapping: async (id: string, updates: Partial<CreateDataMappingParams>): Promise<DataMappingSummary> => {
        const existing = await mappingDatabase.getMapping(id);
        if (!existing) throw new Error(`Mapping '${id}' not found`);

        const now = new Date().toISOString();

        // Merge top-level fields
        if (updates.name) existing.name = updates.name;
        if (updates.description !== undefined) existing.description = updates.description;
        if (updates.tags) existing.metadata.tags = updates.tags;
        existing.metadata.updatedAt = now;

        // Merge API config fields
        if (existing.source.apiConfig) {
          if (updates.baseUrl) existing.source.apiConfig.baseUrl = updates.baseUrl;
          if (updates.endpoint) existing.source.apiConfig.endpoint = updates.endpoint;
          if (updates.method) existing.source.apiConfig.method = updates.method as any;
          if (updates.headers) existing.source.apiConfig.headers = updates.headers;
          if (updates.queryParams) existing.source.apiConfig.queryParams = updates.queryParams;
          if (updates.cacheEnabled !== undefined) existing.source.apiConfig.cacheEnabled = updates.cacheEnabled;
          if (updates.cacheTTL) existing.source.apiConfig.cacheTTL = updates.cacheTTL;
          if (updates.authType) {
            existing.source.apiConfig.authentication = {
              type: updates.authType,
              config: buildAuthConfig(updates as CreateDataMappingParams),
            };
          }
        }

        await mappingDatabase.saveMapping(existing);
        return toSummary(existing);
      },

      // ── Delete mapping ────────────────────────────────────────────────────
      deleteDataMapping: async (id: string): Promise<void> => {
        await mappingDatabase.deleteMapping(id);
      },

      // ── Duplicate mapping ─────────────────────────────────────────────────
      duplicateDataMapping: async (id: string): Promise<DataMappingSummary> => {
        const duplicate = await mappingDatabase.duplicateMapping(id);
        if (!duplicate) throw new Error(`Mapping '${id}' not found`);
        return toSummary(duplicate);
      },

      // ── Test mapping (fetch + transform) ─────────────────────────────────
      testDataMapping: async (id: string, parameters?: Record<string, string>): Promise<DataMappingExecutionResult> => {
        const config = await mappingDatabase.getMapping(id);
        if (!config) throw new Error(`Mapping '${id}' not found`);
        if (!config.source.apiConfig) throw new Error(`Mapping '${id}' has no API config`);

        const startTime = Date.now();

        // Fetch from API
        const apiResponse = await apiClient.executeRequest(config.source.apiConfig, parameters || {});
        if (!apiResponse.success || !apiResponse.data) {
          return {
            success: false,
            errors: [apiResponse.error?.message || 'API fetch failed'],
            warnings: [],
            recordsProcessed: 0,
            recordsReturned: 0,
            duration: Date.now() - startTime,
            executedAt: new Date().toISOString(),
          };
        }

        // Run through mapping engine
        const result = await mappingEngine.execute({
          mapping: config,
          data: apiResponse.data,
          parameters,
        });

        // Cache result if enabled
        if (config.source.apiConfig.cacheEnabled && result.success) {
          await mappingDatabase.setCachedResponse(
            id,
            parameters || {},
            apiResponse.data,
            config.source.apiConfig.cacheTTL
          );
        }

        return {
          success: result.success,
          data: result.data,
          rawData: apiResponse.data,
          errors: result.errors,
          warnings: result.warnings,
          recordsProcessed: result.metadata.recordsProcessed,
          recordsReturned: result.metadata.recordsReturned,
          duration: result.metadata.duration,
          executedAt: result.metadata.executedAt,
        };
      },

      // ── Execute mapping (cached preferred) ───────────────────────────────
      executeDataMapping: async (id: string, parameters?: Record<string, string>): Promise<DataMappingExecutionResult> => {
        const config = await mappingDatabase.getMapping(id);
        if (!config) throw new Error(`Mapping '${id}' not found`);
        if (!config.source.apiConfig) throw new Error(`Mapping '${id}' has no API config`);

        const startTime = Date.now();

        // Try cache first
        if (config.source.apiConfig.cacheEnabled) {
          const cached = await mappingDatabase.getCachedResponse(id, parameters || {});
          if (cached) {
            const result = await mappingEngine.execute({
              mapping: config,
              data: cached,
              parameters,
            });
            return {
              success: result.success,
              data: result.data,
              errors: result.errors,
              warnings: result.warnings,
              recordsProcessed: result.metadata.recordsProcessed,
              recordsReturned: result.metadata.recordsReturned,
              duration: Date.now() - startTime,
              executedAt: new Date().toISOString(),
            };
          }
        }

        // No cache — fetch live
        const apiResponse = await apiClient.executeRequest(config.source.apiConfig, parameters || {});
        if (!apiResponse.success || !apiResponse.data) {
          return {
            success: false,
            errors: [apiResponse.error?.message || 'API fetch failed'],
            warnings: [],
            recordsProcessed: 0,
            recordsReturned: 0,
            duration: Date.now() - startTime,
            executedAt: new Date().toISOString(),
          };
        }

        const result = await mappingEngine.execute({
          mapping: config,
          data: apiResponse.data,
          parameters,
        });

        if (config.source.apiConfig.cacheEnabled && result.success) {
          await mappingDatabase.setCachedResponse(
            id,
            parameters || {},
            apiResponse.data,
            config.source.apiConfig.cacheTTL
          );
        }

        return {
          success: result.success,
          data: result.data,
          errors: result.errors,
          warnings: result.warnings,
          recordsProcessed: result.metadata.recordsProcessed,
          recordsReturned: result.metadata.recordsReturned,
          duration: Date.now() - startTime,
          executedAt: new Date().toISOString(),
        };
      },

      // ── List templates ────────────────────────────────────────────────────
      listMappingTemplates: async (): Promise<DataMappingTemplateSummary[]> => {
        return INDIAN_BROKER_TEMPLATES.map(t => ({
          id: t.id,
          name: t.name,
          description: t.description,
          category: t.category,
          tags: t.tags,
          fieldCount: t.mappingConfig.fieldMappings?.length ?? 0,
          schema: t.mappingConfig.target?.schema,
          verified: t.verified,
          official: t.official,
          rating: t.rating,
          usageCount: t.usageCount,
          instructions: t.instructions,
        }));
      },

      // ── Apply template ────────────────────────────────────────────────────
      applyMappingTemplate: async (
        templateId: string,
        overrides?: { name?: string; description?: string }
      ): Promise<DataMappingSummary> => {
        const template = INDIAN_BROKER_TEMPLATES.find(t => t.id === templateId);
        if (!template) throw new Error(`Template '${templateId}' not found`);

        const now = new Date().toISOString();
        const id = `map_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`;

        const config: DataMappingConfig = {
          id,
          name: overrides?.name || template.name,
          description: overrides?.description || template.description,
          source: template.mappingConfig.source || { type: 'api' },
          target: template.mappingConfig.target || { schemaType: 'predefined', schema: 'OHLCV' },
          extraction: template.mappingConfig.extraction || {
            engine: ParserEngine.JSONPATH,
            isArray: true,
          },
          fieldMappings: template.mappingConfig.fieldMappings || [],
          validation: template.mappingConfig.validation || {
            enabled: false,
            strictMode: false,
            rules: [],
          },
          metadata: {
            createdAt: now,
            updatedAt: now,
            version: 1,
            tags: template.tags,
            aiGenerated: true,
            author: 'MCP (template)',
          },
        };

        await mappingDatabase.saveMapping(config);
        return toSummary(config);
      },

      // ── Clear cache ───────────────────────────────────────────────────────
      clearDataMappingCache: async (mappingId?: string): Promise<void> => {
        await mappingDatabase.clearCache(mappingId);
      },

      // ── List schemas ──────────────────────────────────────────────────────
      listMappingSchemas: async (): Promise<DataMappingSchemaSummary[]> => {
        return Object.entries(UNIFIED_SCHEMAS).map(([key, schema]) => ({
          name: key,
          category: schema.category,
          description: schema.description,
          fieldCount: Object.keys(schema.fields).length,
          requiredFields: Object.entries(schema.fields)
            .filter(([, spec]) => spec.required)
            .map(([name]) => name),
        }));
      },

    });

    this.connected = true;
    console.log('[DataMappingMCPBridge] Connected data mapping services to MCP');
  }

  disconnect(): void {
    if (!this.connected) return;

    terminalMCPProvider.setContexts({
      listDataMappings: undefined,
      getDataMapping: undefined,
      createDataMapping: undefined,
      updateDataMapping: undefined,
      deleteDataMapping: undefined,
      duplicateDataMapping: undefined,
      testDataMapping: undefined,
      executeDataMapping: undefined,
      listMappingTemplates: undefined,
      applyMappingTemplate: undefined,
      clearDataMappingCache: undefined,
      listMappingSchemas: undefined,
    });

    this.connected = false;
    console.log('[DataMappingMCPBridge] Disconnected');
  }

  isConnected(): boolean {
    return this.connected;
  }
}

export const dataMappingMCPBridge = new DataMappingMCPBridge();
