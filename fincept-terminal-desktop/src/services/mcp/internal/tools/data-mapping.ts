// Data Mapping MCP Tools - AI-callable tools for the Data Mapping tab

import { InternalTool } from '../types';

export const dataMappingTools: InternalTool[] = [

  // ── List all saved mappings ──────────────────────────────────────────────────
  {
    name: 'list_data_mappings',
    description: 'List all saved data mappings. Returns name, description, schema, endpoint, field count, and status for each mapping.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.listDataMappings) {
        return { success: false, error: 'Data mapping context not available' };
      }
      const mappings = await contexts.listDataMappings();
      return {
        success: true,
        data: mappings,
        message: `Found ${mappings.length} mapping(s)`,
        count: mappings.length,
      };
    },
  },

  // ── Get a single mapping by ID ───────────────────────────────────────────────
  {
    name: 'get_data_mapping',
    description: 'Get full details of a specific data mapping including field mappings, authentication config, headers, and extraction settings.',
    inputSchema: {
      type: 'object',
      properties: {
        mapping_id: { type: 'string', description: 'The mapping ID to retrieve' },
      },
      required: ['mapping_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getDataMapping) {
        return { success: false, error: 'Data mapping context not available' };
      }
      const mapping = await contexts.getDataMapping(args.mapping_id);
      if (!mapping) {
        return { success: false, error: `Mapping '${args.mapping_id}' not found` };
      }
      return { success: true, data: mapping };
    },
  },

  // ── Create a new mapping ─────────────────────────────────────────────────────
  {
    name: 'create_data_mapping',
    description: 'Create a new API data mapping. Configures the API endpoint, authentication, and target schema. Use list_mapping_schemas to see available schemas.',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Mapping name (e.g. "Upstox OHLCV")' },
        description: { type: 'string', description: 'Brief description of what this mapping does' },
        base_url: { type: 'string', description: 'API base URL (e.g. "https://api.upstox.com")' },
        endpoint: { type: 'string', description: 'Endpoint path, supports {placeholders} (e.g. "/v2/historical-candle/{symbol}/1minute/{to}/{from}")' },
        method: { type: 'string', description: 'HTTP method', enum: ['GET', 'POST', 'PUT', 'DELETE', 'PATCH'], default: 'GET' },
        auth_type: { type: 'string', description: 'Authentication type', enum: ['none', 'apikey', 'bearer', 'basic'], default: 'none' },
        auth_token: { type: 'string', description: 'Bearer token or API key value' },
        auth_key_name: { type: 'string', description: 'Header/query param name for API key (e.g. "X-API-Key")' },
        auth_key_location: { type: 'string', description: 'Where to send API key', enum: ['header', 'query'], default: 'header' },
        auth_username: { type: 'string', description: 'Username for basic auth' },
        auth_password: { type: 'string', description: 'Password for basic auth' },
        headers: { type: 'string', description: 'JSON object of request headers (e.g. {"Accept": "application/json"})' },
        query_params: { type: 'string', description: 'JSON object of query parameters, supports {placeholder} syntax' },
        schema: { type: 'string', description: 'Target schema name (OHLCV, QUOTE, TICK, ORDER, POSITION, PORTFOLIO, INSTRUMENT) or "custom"', default: 'OHLCV' },
        root_path: { type: 'string', description: 'JSONPath/JSONata expression to root data array (e.g. "$.data.candles")' },
        is_array: { type: 'string', description: 'Whether the source data root is an array', enum: ['true', 'false'], default: 'true' },
        cache_enabled: { type: 'string', description: 'Enable response caching', enum: ['true', 'false'], default: 'true' },
        cache_ttl: { type: 'string', description: 'Cache TTL in seconds (default 300)', default: '300' },
        tags: { type: 'string', description: 'Comma-separated tags for categorization' },
      },
      required: ['name', 'base_url', 'endpoint'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createDataMapping) {
        return { success: false, error: 'Data mapping context not available' };
      }

      const headers = args.headers ? (() => { try { return JSON.parse(args.headers); } catch { return {}; } })() : {};
      const queryParams = args.query_params ? (() => { try { return JSON.parse(args.query_params); } catch { return {}; } })() : {};

      const result = await contexts.createDataMapping({
        name: args.name,
        description: args.description || '',
        method: args.method || 'GET',
        baseUrl: args.base_url,
        endpoint: args.endpoint,
        authType: args.auth_type || 'none',
        authToken: args.auth_token,
        authKeyName: args.auth_key_name,
        authKeyLocation: args.auth_key_location || 'header',
        authUsername: args.auth_username,
        authPassword: args.auth_password,
        headers,
        queryParams,
        schemaType: args.schema && !['OHLCV', 'QUOTE', 'TICK', 'ORDER', 'POSITION', 'PORTFOLIO', 'INSTRUMENT'].includes(args.schema) ? 'custom' : 'predefined',
        schema: args.schema || 'OHLCV',
        rootPath: args.root_path,
        isArray: args.is_array !== 'false',
        cacheEnabled: args.cache_enabled !== 'false',
        cacheTTL: parseInt(args.cache_ttl || '300', 10),
        tags: args.tags ? args.tags.split(',').map((t: string) => t.trim()) : [],
      });

      return {
        success: true,
        data: result,
        message: `Mapping '${args.name}' created with ID: ${result.id}`,
      };
    },
  },

  // ── Update an existing mapping ───────────────────────────────────────────────
  {
    name: 'update_data_mapping',
    description: 'Update fields of an existing data mapping. Only provide the fields you want to change.',
    inputSchema: {
      type: 'object',
      properties: {
        mapping_id: { type: 'string', description: 'The mapping ID to update' },
        name: { type: 'string', description: 'New name' },
        description: { type: 'string', description: 'New description' },
        base_url: { type: 'string', description: 'New base URL' },
        endpoint: { type: 'string', description: 'New endpoint path' },
        method: { type: 'string', description: 'New HTTP method', enum: ['GET', 'POST', 'PUT', 'DELETE', 'PATCH'] },
        auth_type: { type: 'string', description: 'New auth type', enum: ['none', 'apikey', 'bearer', 'basic'] },
        auth_token: { type: 'string', description: 'New bearer token or API key value' },
        headers: { type: 'string', description: 'JSON object of headers to set/replace' },
        query_params: { type: 'string', description: 'JSON object of query params to set/replace' },
        cache_enabled: { type: 'string', description: 'Enable/disable cache', enum: ['true', 'false'] },
        cache_ttl: { type: 'string', description: 'New cache TTL in seconds' },
      },
      required: ['mapping_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.updateDataMapping) {
        return { success: false, error: 'Data mapping context not available' };
      }

      const updates: Record<string, any> = {};
      if (args.name) updates.name = args.name;
      if (args.description) updates.description = args.description;
      if (args.base_url) updates.baseUrl = args.base_url;
      if (args.endpoint) updates.endpoint = args.endpoint;
      if (args.method) updates.method = args.method;
      if (args.auth_type) updates.authType = args.auth_type;
      if (args.auth_token) updates.authToken = args.auth_token;
      if (args.headers) { try { updates.headers = JSON.parse(args.headers); } catch { /* skip */ } }
      if (args.query_params) { try { updates.queryParams = JSON.parse(args.query_params); } catch { /* skip */ } }
      if (args.cache_enabled !== undefined) updates.cacheEnabled = args.cache_enabled !== 'false';
      if (args.cache_ttl) updates.cacheTTL = parseInt(args.cache_ttl, 10);

      const result = await contexts.updateDataMapping(args.mapping_id, updates);
      return {
        success: true,
        data: result,
        message: `Mapping '${result.name}' updated`,
      };
    },
  },

  // ── Delete a mapping ─────────────────────────────────────────────────────────
  {
    name: 'delete_data_mapping',
    description: 'Delete a saved data mapping and its associated cache.',
    inputSchema: {
      type: 'object',
      properties: {
        mapping_id: { type: 'string', description: 'The mapping ID to delete' },
      },
      required: ['mapping_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.deleteDataMapping) {
        return { success: false, error: 'Data mapping context not available' };
      }
      await contexts.deleteDataMapping(args.mapping_id);
      return { success: true, message: `Mapping '${args.mapping_id}' deleted` };
    },
  },

  // ── Duplicate a mapping ──────────────────────────────────────────────────────
  {
    name: 'duplicate_data_mapping',
    description: 'Duplicate an existing data mapping. Useful for creating variations of a working mapping.',
    inputSchema: {
      type: 'object',
      properties: {
        mapping_id: { type: 'string', description: 'The mapping ID to duplicate' },
      },
      required: ['mapping_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.duplicateDataMapping) {
        return { success: false, error: 'Data mapping context not available' };
      }
      const result = await contexts.duplicateDataMapping(args.mapping_id);
      return {
        success: true,
        data: result,
        message: `Mapping duplicated as '${result.name}' (ID: ${result.id})`,
      };
    },
  },

  // ── Test a mapping (fetch + transform) ──────────────────────────────────────
  {
    name: 'test_data_mapping',
    description: 'Test a data mapping by fetching live data from the API and running the transformation. Returns raw + transformed output with metrics.',
    inputSchema: {
      type: 'object',
      properties: {
        mapping_id: { type: 'string', description: 'The mapping ID to test' },
        parameters: { type: 'string', description: 'JSON object of runtime parameters for {placeholder} values (e.g. {"symbol": "AAPL", "from": "2024-01-01"})' },
      },
      required: ['mapping_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.testDataMapping) {
        return { success: false, error: 'Data mapping context not available' };
      }
      const params = args.parameters ? (() => { try { return JSON.parse(args.parameters); } catch { return {}; } })() : {};
      const result = await contexts.testDataMapping(args.mapping_id, params);
      return {
        success: result.success,
        data: result,
        message: result.success
          ? `Test passed — ${result.recordsReturned} records returned in ${result.duration}ms`
          : `Test failed: ${result.errors?.join(', ')}`,
        error: result.success ? undefined : result.errors?.join(', '),
      };
    },
  },

  // ── Execute a mapping (get transformed data) ─────────────────────────────────
  {
    name: 'execute_data_mapping',
    description: 'Execute a data mapping and return only the transformed output data. Uses cache if available. Ideal for consuming mapped data in workflows.',
    inputSchema: {
      type: 'object',
      properties: {
        mapping_id: { type: 'string', description: 'The mapping ID to execute' },
        parameters: { type: 'string', description: 'JSON object of runtime parameters for {placeholder} values' },
      },
      required: ['mapping_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.executeDataMapping) {
        return { success: false, error: 'Data mapping context not available' };
      }
      const params = args.parameters ? (() => { try { return JSON.parse(args.parameters); } catch { return {}; } })() : {};
      const result = await contexts.executeDataMapping(args.mapping_id, params);
      return {
        success: result.success,
        data: result.data,
        message: result.success
          ? `Executed — ${result.recordsReturned} records`
          : `Execution failed: ${result.errors?.join(', ')}`,
        error: result.success ? undefined : result.errors?.join(', '),
      };
    },
  },

  // ── List templates ───────────────────────────────────────────────────────────
  {
    name: 'list_mapping_templates',
    description: 'List all available mapping templates (Indian brokers: Upstox, Zerodha, Dhan, Fyers, AngelOne, ShipNext etc.). Templates are pre-configured mappings ready to use.',
    inputSchema: {
      type: 'object',
      properties: {
        filter: { type: 'string', description: 'Filter by broker/tag name (e.g. "upstox", "zerodha", "dhan")' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.listMappingTemplates) {
        return { success: false, error: 'Data mapping context not available' };
      }
      let templates = await contexts.listMappingTemplates();
      if (args.filter) {
        const f = args.filter.toLowerCase();
        templates = templates.filter(t =>
          t.tags.some(tag => tag.toLowerCase().includes(f)) ||
          t.name.toLowerCase().includes(f) ||
          t.category.toLowerCase().includes(f)
        );
      }
      return {
        success: true,
        data: templates,
        message: `Found ${templates.length} template(s)`,
        count: templates.length,
      };
    },
  },

  // ── Apply a template ─────────────────────────────────────────────────────────
  {
    name: 'apply_mapping_template',
    description: 'Apply a mapping template to instantly create a pre-configured mapping. Use list_mapping_templates to find template IDs.',
    inputSchema: {
      type: 'object',
      properties: {
        template_id: { type: 'string', description: 'Template ID to apply' },
        name: { type: 'string', description: 'Custom name for the new mapping (optional, defaults to template name)' },
        description: { type: 'string', description: 'Custom description (optional)' },
      },
      required: ['template_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.applyMappingTemplate) {
        return { success: false, error: 'Data mapping context not available' };
      }
      const result = await contexts.applyMappingTemplate(args.template_id, {
        name: args.name,
        description: args.description,
      });
      return {
        success: true,
        data: result,
        message: `Template applied — mapping '${result.name}' created (ID: ${result.id})`,
      };
    },
  },

  // ── Clear cache ──────────────────────────────────────────────────────────────
  {
    name: 'clear_data_mapping_cache',
    description: 'Clear cached API responses for a specific mapping or all mappings.',
    inputSchema: {
      type: 'object',
      properties: {
        mapping_id: { type: 'string', description: 'Mapping ID to clear cache for. Omit to clear all caches.' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.clearDataMappingCache) {
        return { success: false, error: 'Data mapping context not available' };
      }
      await contexts.clearDataMappingCache(args.mapping_id);
      return {
        success: true,
        message: args.mapping_id
          ? `Cache cleared for mapping '${args.mapping_id}'`
          : 'All mapping caches cleared',
      };
    },
  },

  // ── List schemas ─────────────────────────────────────────────────────────────
  {
    name: 'list_mapping_schemas',
    description: 'List all predefined unified data schemas available as mapping targets: OHLCV, QUOTE, TICK, ORDER, POSITION, PORTFOLIO, INSTRUMENT. Shows fields and which are required.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.listMappingSchemas) {
        return { success: false, error: 'Data mapping context not available' };
      }
      const schemas = await contexts.listMappingSchemas();
      return {
        success: true,
        data: schemas,
        message: `${schemas.length} predefined schemas available`,
        count: schemas.length,
      };
    },
  },
];
