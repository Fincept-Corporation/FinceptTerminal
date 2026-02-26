/**
 * Parameter Processor
 * Handles parameter value extraction, transformation, and routing
 *
 * This module provides utilities for:
 * - Extracting specific values from complex parameter objects
 * - Routing parameter values to HTTP requests
 * - Transforming parameter values based on configuration
 * - Type conversion and validation
 *
 * Based on n8n's parameter handling patterns.
 */

import type {
  INode,
  INodeProperties,
  INodePropertyRouting,
  NodeParameterValue,
  NodeParameterValueType,
  IDataObject,
  IGetNodeParameterOptions,
  IHttpRequestOptions,
} from './types';

/**
 * Extract a specific value from a parameter based on extractValue configuration
 *
 * Example:
 * If parameter value is: { id: 123, name: "Test", meta: { ... } }
 * And extractValue is: { type: "id", property: "id" }
 * Returns: 123
 */
export function extractParameterValue(
  value: NodeParameterValueType | object,
  extractConfig?: { type: string; property: string }
): NodeParameterValue {
  if (!extractConfig || value === null || value === undefined) {
    return value as NodeParameterValue;
  }

  // Handle different extraction types
  switch (extractConfig.type) {
    case 'id':
    case 'value':
    case 'name':
      // Extract a specific property from an object
      if (typeof value === 'object' && !Array.isArray(value)) {
        const obj = value as IDataObject;
        return obj[extractConfig.property] as NodeParameterValue;
      }
      return value as NodeParameterValue;

    case 'list':
      // Extract from first item in array
      if (Array.isArray(value) && value.length > 0) {
        const firstItem = value[0];
        if (typeof firstItem === 'object' && firstItem !== null) {
          return (firstItem as IDataObject)[extractConfig.property] as NodeParameterValue;
        }
        return firstItem as NodeParameterValue;
      }
      return value as NodeParameterValue;

    case 'object':
      // Extract entire object at property path
      if (typeof value === 'object' && !Array.isArray(value)) {
        return getNestedValue(value as IDataObject, extractConfig.property);
      }
      return value as NodeParameterValue;

    default:
      return value as NodeParameterValue;
  }
}

/**
 * Get nested value from object using dot notation
 */
function getNestedValue(obj: IDataObject, path: string): any {
  const keys = path.split('.');
  let current: any = obj;

  for (const key of keys) {
    if (current === null || current === undefined) {
      return undefined;
    }
    current = current[key];
  }

  return current;
}

/**
 * Apply routing configuration to transform parameter value for HTTP requests
 *
 * Routing allows parameters to automatically configure HTTP request options.
 * For example, a parameter can specify that it should be sent as a header,
 * or as part of the request body.
 */
export function applyParameterRouting(
  parameterValue: NodeParameterValue,
  routing?: INodePropertyRouting,
  requestOptions?: IHttpRequestOptions
): IHttpRequestOptions {
  if (!routing || !requestOptions) {
    return requestOptions || { url: '', method: 'GET' };
  }

  const options = { ...requestOptions };

  // Apply request routing (modify request URL, method, headers, body)
  if (routing.request) {
    if (routing.request.method) {
      options.method = routing.request.method as 'GET' | 'POST' | 'PUT' | 'DELETE' | 'PATCH' | 'HEAD' | 'OPTIONS';
    }

    if (routing.request.url) {
      // Replace placeholders in URL with parameter value
      options.url = routing.request.url.replace('{{value}}', String(parameterValue));
    }

    if (routing.request.headers) {
      options.headers = {
        ...(options.headers || {}),
        ...routing.request.headers,
      };
    }

    if (routing.request.body) {
      options.body = {
        ...(typeof options.body === 'object' ? options.body : {}),
        ...routing.request.body,
      };
    }
  }

  // Apply send routing (specify where to send the parameter value)
  if (routing.send) {
    const { type, property, value } = routing.send;

    const valueToSend = value ? value : parameterValue;

    switch (type) {
      case 'header':
        // Send as HTTP header
        options.headers = {
          ...(options.headers || {}),
          [property || 'X-Custom-Header']: String(valueToSend),
        };
        break;

      case 'query':
        // Send as query parameter
        if (!options.qs) {
          options.qs = {};
        }
        options.qs[property || 'param'] = valueToSend;
        break;

      case 'body':
        // Send in request body
        if (options.json !== false) {
          if (typeof options.body !== 'object') {
            options.body = {};
          }
          (options.body as IDataObject)[property || 'value'] = valueToSend;
        }
        break;

      case 'path':
        // Send as part of URL path
        if (options.url && property) {
          options.url = options.url.replace(`{${property}}`, String(valueToSend));
        }
        break;

      default:
        // Default: send in body
        if (typeof options.body !== 'object') {
          options.body = {};
        }
        (options.body as IDataObject)[property || 'value'] = valueToSend;
    }
  }

  return options;
}

/**
 * Convert parameter value to specified type
 *
 * Handles type conversion with proper error handling and fallbacks.
 */
export function convertParameterValue(
  value: NodeParameterValue,
  targetType: 'string' | 'number' | 'boolean' | 'array' | 'object'
): NodeParameterValue {
  if (value === null || value === undefined) {
    return value;
  }

  try {
    switch (targetType) {
      case 'string':
        return String(value);

      case 'number': {
        if (typeof value === 'number') {
          return value;
        }
        const num = Number(value);
        return isNaN(num) ? 0 : num;
      }

      case 'boolean': {
        if (typeof value === 'boolean') {
          return value;
        }
        if (typeof value === 'string') {
          const lower = value.toLowerCase();
          return lower === 'true' || lower === '1' || lower === 'yes';
        }
        return Boolean(value);
      }

      case 'array': {
        if (Array.isArray(value)) {
          return value;
        }
        if (typeof value === 'string') {
          try {
            const parsed = JSON.parse(value);
            return Array.isArray(parsed) ? parsed : [value];
          } catch {
            // Try splitting by comma
            return value.split(',').map((s) => s.trim());
          }
        }
        return [value];
      }

      case 'object': {
        if (typeof value === 'object' && !Array.isArray(value)) {
          return value;
        }
        if (typeof value === 'string') {
          try {
            return JSON.parse(value);
          } catch {
            return {};
          }
        }
        return {};
      }

      default:
        return value;
    }
  } catch (error) {
    console.error('[ParameterProcessor] Type conversion error:', error);
    return value; // Return original value on error
  }
}

/**
 * Validate parameter value against field type
 *
 * Returns true if value is valid, false otherwise.
 * Used for runtime validation of parameter values.
 */
export function validateParameterValue(
  value: NodeParameterValue,
  fieldType: string
): { valid: boolean; error?: string } {
  if (value === null || value === undefined) {
    return { valid: true }; // null/undefined are always valid
  }

  switch (fieldType) {
    case 'string':
      if (typeof value !== 'string') {
        return { valid: false, error: `Expected string, got ${typeof value}` };
      }
      return { valid: true };

    case 'number':
      if (typeof value !== 'number' || isNaN(value)) {
        return { valid: false, error: 'Expected valid number' };
      }
      return { valid: true };

    case 'boolean':
      if (typeof value !== 'boolean') {
        return { valid: false, error: `Expected boolean, got ${typeof value}` };
      }
      return { valid: true };

    case 'array':
      if (!Array.isArray(value)) {
        return { valid: false, error: `Expected array, got ${typeof value}` };
      }
      return { valid: true };

    case 'object':
      if (typeof value !== 'object' || Array.isArray(value)) {
        return { valid: false, error: 'Expected object' };
      }
      return { valid: true };

    case 'dateTime': {
      // Check if valid date
      const date = new Date(value as string | number);
      if (isNaN(date.getTime())) {
        return { valid: false, error: 'Expected valid date/time' };
      }
      return { valid: true };
    }

    case 'url':
      // Basic URL validation
      if (typeof value !== 'string') {
        return { valid: false, error: 'Expected string URL' };
      }
      try {
        new URL(value);
        return { valid: true };
      } catch {
        return { valid: false, error: 'Invalid URL format' };
      }

    case 'email': {
      // Basic email validation
      if (typeof value !== 'string') {
        return { valid: false, error: 'Expected string email' };
      }
      const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
      if (!emailRegex.test(value)) {
        return { valid: false, error: 'Invalid email format' };
      }
      return { valid: true };
    }

    default:
      return { valid: true }; // Unknown types are considered valid
  }
}

/**
 * Process all parameters for a node
 *
 * This is a comprehensive function that:
 * 1. Extracts values if needed
 * 2. Applies type conversions
 * 3. Validates values
 * 4. Prepares routing information
 */
export function processNodeParameters(
  node: INode,
  parameterDefinitions: INodeProperties[],
  options?: {
    validateTypes?: boolean;
    convertTypes?: boolean;
    extractValues?: boolean;
  }
): {
  parameters: IDataObject;
  routing: Map<string, INodePropertyRouting>;
  errors: Array<{ parameter: string; error: string }>;
} {
  const processed: IDataObject = {};
  const routing = new Map<string, INodePropertyRouting>();
  const errors: Array<{ parameter: string; error: string }> = [];

  for (const paramDef of parameterDefinitions) {
    const paramName = paramDef.name;
    let value = node.parameters[paramName];

    if (value === undefined) {
      value = paramDef.default;
    }

    // Extract value if configured
    if (options?.extractValues && paramDef.extractValue) {
      value = extractParameterValue(value, paramDef.extractValue);
    }

    // Convert type if needed
    if (options?.convertTypes && paramDef.validateType) {
      const targetType = paramDef.validateType as any;
      value = convertParameterValue(value as NodeParameterValue, targetType);
    }

    // Validate if configured
    if (options?.validateTypes && paramDef.validateType && !paramDef.ignoreValidationDuringExecution) {
      const validation = validateParameterValue(value as NodeParameterValue, paramDef.validateType);
      if (!validation.valid) {
        errors.push({
          parameter: paramName,
          error: validation.error || 'Validation failed',
        });
      }
    }

    // Store routing information
    if (paramDef.routing) {
      routing.set(paramName, paramDef.routing);
    }

    processed[paramName] = value;
  }

  return { parameters: processed, routing, errors };
}

/**
 * Build HTTP request options from parameters with routing
 *
 * This function takes node parameters and their routing configurations
 * and builds a complete HTTP request options object.
 */
export function buildRequestFromRouting(
  parameters: IDataObject,
  routing: Map<string, INodePropertyRouting>,
  baseRequest?: IHttpRequestOptions
): IHttpRequestOptions {
  let options: IHttpRequestOptions = baseRequest || { url: '', method: 'GET' };

  // Apply routing from each parameter
  for (const [paramName, paramRouting] of routing.entries()) {
    const paramValue = parameters[paramName] as NodeParameterValue;
    options = applyParameterRouting(paramValue, paramRouting, options);
  }

  return options;
}
