// Core Mapping Engine

import { DataMappingConfig, MappingExecutionContext, MappingExecutionResult, FieldMapping, ParserEngine } from '../types';
import { getParser } from './parsers';
import { applyTransform } from '../utils/transformFunctions';
import { getSchema } from '../schemas';
import { createAdapter } from '../../data-sources/adapters';
import { DataSourceConnection } from '../../data-sources/types';

export class MappingEngine {
  /**
   * Execute a mapping configuration on data
   */
  async execute(context: MappingExecutionContext): Promise<MappingExecutionResult> {
    const startTime = Date.now();

    try {
      const { mapping, parameters, options } = context;

      // 1. Fetch data if needed
      let rawData: any;
      if (context.data) {
        rawData = context.data;
      } else if (mapping.source.connectionId) {
        rawData = await this.fetchData(mapping, parameters);
      } else if (mapping.source.sampleData) {
        rawData = mapping.source.sampleData;
      } else {
        throw new Error('No data source provided');
      }

      // 2. Extract data using parser
      const extractedData = await this.extractData(rawData, mapping);

      // 3. Transform data
      const transformedData = await this.transformData(extractedData, mapping);

      // 4. Validate if enabled
      let validationResults;
      if (mapping.validation.enabled && !options?.skipValidation) {
        validationResults = await this.validateData(transformedData, mapping);
        if (mapping.validation.strictMode && !validationResults.valid) {
          throw new Error(`Validation failed: ${validationResults.errors.map((e: any) => e.message).join(', ')}`);
        }
      }

      const duration = Date.now() - startTime;

      return {
        success: true,
        data: transformedData,
        rawData: options?.returnRaw ? rawData : undefined,
        metadata: {
          executedAt: new Date().toISOString(),
          duration,
          recordsProcessed: Array.isArray(extractedData) ? extractedData.length : 1,
          recordsReturned: Array.isArray(transformedData) ? transformedData.length : 1,
          validationResults,
        },
      };
    } catch (error) {
      const duration = Date.now() - startTime;

      return {
        success: false,
        errors: [error instanceof Error ? error.message : String(error)],
        metadata: {
          executedAt: new Date().toISOString(),
          duration,
          recordsProcessed: 0,
          recordsReturned: 0,
        },
      };
    }
  }

  /**
   * Fetch data from connection
   */
  private async fetchData(mapping: DataMappingConfig, parameters?: Record<string, any>): Promise<any> {
    if (!mapping.source.connection) {
      throw new Error('Connection not loaded');
    }

    // Get adapter for this connection
    const adapter = createAdapter(mapping.source.connection);
    if (!adapter) {
      throw new Error(`No adapter found for ${mapping.source.connection.type}`);
    }

    // Build endpoint with parameters
    let endpoint = mapping.source.endpoint || '';
    if (parameters) {
      for (const [key, value] of Object.entries(parameters)) {
        endpoint = endpoint.replace(`{${key}}`, String(value));
      }
    }

    // Execute request (adapter handles authentication)
    const request = {
      endpoint,
      method: mapping.source.method || 'GET',
      headers: mapping.source.headers,
      body: mapping.source.body,
    };

    // Note: This assumes adapters have an executeRequest method
    // You may need to extend your adapter interface
    const response = await (adapter as any).executeRequest?.(request);

    return response;
  }

  /**
   * Extract data using configured parser
   */
  private async extractData(rawData: any, mapping: DataMappingConfig): Promise<any> {
    const { extraction } = mapping;

    // Get parser
    const parser = getParser(extraction.engine);

    // If rootPath is specified, extract that first
    let data = rawData;
    if (extraction.rootPath) {
      const rootParser = getParser(extraction.engine);
      data = await rootParser.execute(rawData, extraction.rootPath);
    }

    // Handle array vs single object
    if (extraction.isArray && !Array.isArray(data)) {
      return [data];
    }

    return data;
  }

  /**
   * Transform extracted data using field mappings
   */
  private async transformData(extractedData: any, mapping: DataMappingConfig): Promise<any> {
    // Handle array of items
    if (Array.isArray(extractedData)) {
      const transformed = await Promise.all(
        extractedData.map(item => this.transformSingle(item, mapping))
      );

      // Apply post-processing
      return await this.applyPostProcessing(transformed, mapping);
    } else {
      return this.transformSingle(extractedData, mapping);
    }
  }

  /**
   * Transform single data item
   */
  private async transformSingle(sourceData: any, mapping: DataMappingConfig): Promise<any> {
    const result: any = {};

    for (const fieldMapping of mapping.fieldMappings) {
      try {
        let value = await this.extractFieldValue(sourceData, fieldMapping);

        // Apply transformation if specified
        if (fieldMapping.transform && value !== null && value !== undefined) {
          value = applyTransform(value, fieldMapping.transform, fieldMapping.transformParams);
        }

        // Use default value if null/undefined and required
        if ((value === null || value === undefined)) {
          if (fieldMapping.required && fieldMapping.defaultValue !== undefined) {
            value = fieldMapping.defaultValue;
          } else if (fieldMapping.required) {
            throw new Error(`Required field ${fieldMapping.targetField} is missing`);
          }
        }

        result[fieldMapping.targetField] = value;
      } catch (error) {
        console.error(`Error mapping field ${fieldMapping.targetField}:`, error);
        if (fieldMapping.required) {
          throw error;
        }
      }
    }

    // Add derived field calculations for POSITION schema
    if (mapping.target.schema === 'POSITION') {
      if (!result.costBasis && result.quantity && result.averagePrice) {
        result.costBasis = Math.abs(result.quantity * result.averagePrice);
      }
      if (!result.marketValue && result.quantity && result.currentPrice) {
        result.marketValue = result.quantity * result.currentPrice;
      }
    }

    return result;
  }

  /**
   * Extract field value using parser
   */
  private async extractFieldValue(sourceData: any, fieldMapping: FieldMapping): Promise<any> {
    // Use field-specific parser if specified, otherwise use default
    const parser = fieldMapping.parserEngine
      ? getParser(fieldMapping.parserEngine)
      : getParser(ParserEngine.DIRECT);

    try {
      return await parser.execute(sourceData, fieldMapping.sourceExpression);
    } catch (error) {
      console.error(`Failed to extract ${fieldMapping.targetField}:`, error);
      return fieldMapping.defaultValue;
    }
  }

  /**
   * Apply post-processing (filter, sort, limit, deduplicate)
   */
  private async applyPostProcessing(data: any[], mapping: DataMappingConfig): Promise<any[]> {
    let processed = data;

    const { postProcessing } = mapping;
    if (!postProcessing) return processed;

    // Filter
    if (postProcessing.filter) {
      // Use JSONata for filtering
      const parser = getParser(ParserEngine.JSONATA);
      const filterResults = await Promise.all(
        processed.map(async item => {
          try {
            const result = await parser.execute(item, postProcessing.filter!);
            return { item, include: result };
          } catch {
            return { item, include: true };
          }
        })
      );
      processed = filterResults.filter(r => r.include).map(r => r.item);
    }

    // Sort
    if (postProcessing.sort) {
      const { field, order } = postProcessing.sort;
      processed = processed.sort((a, b) => {
        const aVal = a[field];
        const bVal = b[field];
        const comparison = aVal < bVal ? -1 : aVal > bVal ? 1 : 0;
        return order === 'desc' ? -comparison : comparison;
      });
    }

    // Deduplicate
    if (postProcessing.deduplicate) {
      const seen = new Set();
      const field = postProcessing.deduplicate;
      processed = processed.filter(item => {
        const key = item[field];
        if (seen.has(key)) return false;
        seen.add(key);
        return true;
      });
    }

    // Limit
    if (postProcessing.limit && postProcessing.limit > 0) {
      processed = processed.slice(0, postProcessing.limit);
    }

    return processed;
  }

  /**
   * Validate transformed data against schema
   */
  private async validateData(data: any, mapping: DataMappingConfig): Promise<any> {
    const schema = getSchema(mapping.target.schema);
    if (!schema) {
      return { valid: true, errors: [] };
    }

    const errors: any[] = [];
    const items = Array.isArray(data) ? data : [data];

    for (let i = 0; i < items.length; i++) {
      const item = items[i];

      // Validate each field
      for (const [fieldName, fieldSpec] of Object.entries(schema.fields)) {
        const value = item[fieldName];

        // Required check
        if (fieldSpec.required && (value === null || value === undefined)) {
          errors.push({
            field: fieldName,
            message: `Field '${fieldName}' is required but missing`,
            rule: 'required',
          });
          continue;
        }

        // Type check
        if (value !== null && value !== undefined) {
          const actualType = Array.isArray(value) ? 'array' : typeof value;
          const expectedType = fieldSpec.type;

          // Special case for datetime (must be valid date string)
          if (expectedType === 'datetime') {
            if (actualType !== 'string') {
              errors.push({
                field: fieldName,
                message: `Field '${fieldName}' should be a date string, got ${actualType}`,
                rule: 'type',
              });
            } else if (isNaN(Date.parse(value))) {
              errors.push({
                field: fieldName,
                message: `Field '${fieldName}' is not a valid date string`,
                rule: 'type',
              });
            }
          } else if (expectedType !== actualType) {
            errors.push({
              field: fieldName,
              message: `Field '${fieldName}' should be ${expectedType}, got ${actualType}`,
              rule: 'type',
            });
          }

          // Validation rules
          if (fieldSpec.validation) {
            const { min, max, pattern } = fieldSpec.validation;

            if (min !== undefined && typeof value === 'number' && value < min) {
              errors.push({
                field: fieldName,
                message: `Field '${fieldName}' must be >= ${min}`,
                rule: 'range',
              });
            }

            if (max !== undefined && typeof value === 'number' && value > max) {
              errors.push({
                field: fieldName,
                message: `Field '${fieldName}' must be <= ${max}`,
                rule: 'range',
              });
            }

            if (pattern && typeof value === 'string' && !new RegExp(pattern).test(value)) {
              errors.push({
                field: fieldName,
                message: `Field '${fieldName}' does not match pattern ${pattern}`,
                rule: 'pattern',
              });
            }
          }
        }
      }
    }

    return {
      valid: errors.length === 0,
      errors,
    };
  }

  /**
   * Test mapping with sample data
   */
  async test(mapping: DataMappingConfig, sampleData: any, parameters?: Record<string, any>): Promise<MappingExecutionResult> {
    return this.execute({
      mapping,
      data: sampleData,
      parameters,
      options: { debug: true, returnRaw: true },
    });
  }
}

// Export singleton instance
export const mappingEngine = new MappingEngine();
