/**
 * Robust JSON parser for AKShare API responses
 * Handles various data structures returned by Python scripts
 */

export interface ParsedDataResult {
  data: any[];
  count: number;
  columns: string[];
  metadata?: Record<string, any>;
  warnings?: string[];
}

/**
 * Recursively searches for array data in nested objects
 */
function findArrayData(obj: any, depth: number = 0, maxDepth: number = 5): any[] | null {
  if (depth > maxDepth) return null;

  if (Array.isArray(obj)) {
    return obj;
  }

  if (obj && typeof obj === 'object') {
    // Common keys where data might be stored
    const dataKeys = ['data', 'records', 'rows', 'items', 'results', 'content', 'list'];

    for (const key of dataKeys) {
      if (obj[key] !== undefined) {
        const result = findArrayData(obj[key], depth + 1, maxDepth);
        if (result) return result;
      }
    }

    // Try all keys if data keys didn't work
    for (const key of Object.keys(obj)) {
      const value = obj[key];
      if (Array.isArray(value) && value.length > 0) {
        return value;
      }
    }

    // Recursively search nested objects
    for (const key of Object.keys(obj)) {
      const value = obj[key];
      if (value && typeof value === 'object' && !Array.isArray(value)) {
        const result = findArrayData(value, depth + 1, maxDepth);
        if (result) return result;
      }
    }
  }

  return null;
}

/**
 * Converts various data formats to a normalized array of objects
 */
function normalizeToObjectArray(data: any): any[] {
  // Already an array of objects
  if (Array.isArray(data) && data.length > 0 && typeof data[0] === 'object') {
    return data;
  }

  // Array of primitives - convert to objects
  if (Array.isArray(data) && data.length > 0 && typeof data[0] !== 'object') {
    return data.map((item, index) => ({ index, value: item }));
  }

  // Single object - wrap in array
  if (data && typeof data === 'object' && !Array.isArray(data)) {
    // Check if it's a record-style object (keys are IDs, values are records)
    const values = Object.values(data);
    if (values.length > 0 && typeof values[0] === 'object') {
      // Convert to array of objects with keys included
      return Object.entries(data).map(([key, value]) => {
        if (typeof value === 'object') {
          return { id: key, ...value };
        }
        return { id: key, value };
      });
    }

    // Single record - wrap in array
    return [data];
  }

  // Primitive value - wrap in object and array
  if (data !== null && data !== undefined) {
    return [{ value: data }];
  }

  return [];
}

/**
 * Validates and cleans column names
 */
function cleanColumnNames(columns: string[]): string[] {
  return columns.map(col => {
    // Remove null/undefined
    if (col === null || col === undefined) return 'unnamed';

    // Convert to string
    const str = String(col);

    // Remove leading/trailing whitespace
    let cleaned = str.trim();

    // Replace empty strings
    if (cleaned === '') cleaned = 'unnamed';

    // Remove special characters that might cause issues
    // Keep: letters, numbers, Chinese characters, spaces, underscores, hyphens
    cleaned = cleaned.replace(/[^\w\u4e00-\u9fa5\s-]/g, '_');

    return cleaned;
  });
}

/**
 * Ensures all objects in array have consistent keys
 */
function normalizeObjectKeys(data: any[]): any[] {
  if (data.length === 0) return data;

  // Collect all unique keys from all objects
  const allKeys = new Set<string>();
  data.forEach(obj => {
    if (obj && typeof obj === 'object') {
      Object.keys(obj).forEach(key => allKeys.add(key));
    }
  });

  const keyArray = Array.from(allKeys);

  // Ensure every object has all keys
  return data.map(obj => {
    if (!obj || typeof obj !== 'object') {
      return { value: obj };
    }

    const normalized: Record<string, any> = {};
    keyArray.forEach(key => {
      normalized[key] = obj[key] !== undefined ? obj[key] : null;
    });

    return normalized;
  });
}

/**
 * Extracts metadata from response
 */
function extractMetadata(response: any): Record<string, any> {
  const metadata: Record<string, any> = {};

  if (response && typeof response === 'object') {
    // Extract common metadata fields
    const metadataKeys = [
      'timestamp', 'source', 'data_quality', 'version',
      'message', 'warnings', 'info', 'metadata'
    ];

    metadataKeys.forEach(key => {
      if (response[key] !== undefined) {
        metadata[key] = response[key];
      }
    });
  }

  return metadata;
}

/**
 * Main parser function for AKShare API responses
 */
export function parseAKShareResponse(response: any): ParsedDataResult {
  const warnings: string[] = [];

  try {
    // Handle null/undefined response
    if (response === null || response === undefined) {
      return {
        data: [],
        count: 0,
        columns: [],
        warnings: ['Response is null or undefined']
      };
    }

    // Handle error responses
    if (response.error) {
      return {
        data: [],
        count: 0,
        columns: [],
        warnings: [typeof response.error === 'string' ? response.error : JSON.stringify(response.error)]
      };
    }

    // Extract metadata
    const metadata = extractMetadata(response);

    // Find the actual data - could be at various levels
    let rawData: any;

    // Check if response.data exists and is the data
    if (response.data !== undefined) {
      rawData = response.data;
    } else if (response.success !== undefined) {
      // Response might be wrapped in success/data structure
      rawData = response.data || response;
    } else {
      // Response itself might be the data
      rawData = response;
    }

    // Handle nested data structure (common in Python responses)
    // e.g., { data: { data: [...] } } or { data: { records: [...] } }
    if (rawData && typeof rawData === 'object' && !Array.isArray(rawData)) {
      const nestedArray = findArrayData(rawData);
      if (nestedArray) {
        rawData = nestedArray;
      }
    }

    // Normalize to array of objects
    let normalizedData = normalizeToObjectArray(rawData);

    // Handle empty data
    if (normalizedData.length === 0) {
      warnings.push('No data records found in response');
      return {
        data: [],
        count: 0,
        columns: [],
        metadata,
        warnings
      };
    }

    // Ensure consistent object keys
    normalizedData = normalizeObjectKeys(normalizedData);

    // Extract and clean column names
    const columns = normalizedData.length > 0
      ? cleanColumnNames(Object.keys(normalizedData[0]))
      : [];

    // Validate data structure
    if (columns.length === 0) {
      warnings.push('No columns detected in data');
    }

    // Clean up data - handle special values
    const cleanedData = normalizedData.map(row => {
      const cleanedRow: Record<string, any> = {};
      Object.entries(row).forEach(([key, value]) => {
        // Handle NaN, Infinity, -Infinity
        if (typeof value === 'number') {
          if (isNaN(value)) {
            cleanedRow[key] = null;
          } else if (!isFinite(value)) {
            cleanedRow[key] = value > 0 ? 'Infinity' : '-Infinity';
          } else {
            cleanedRow[key] = value;
          }
        }
        // Handle undefined
        else if (value === undefined) {
          cleanedRow[key] = null;
        }
        // Keep everything else as is
        else {
          cleanedRow[key] = value;
        }
      });
      return cleanedRow;
    });

    return {
      data: cleanedData,
      count: cleanedData.length,
      columns,
      metadata,
      warnings: warnings.length > 0 ? warnings : undefined
    };

  } catch (error) {
    console.error('Error parsing AKShare response:', error);
    return {
      data: [],
      count: 0,
      columns: [],
      warnings: [`Parse error: ${error instanceof Error ? error.message : String(error)}`]
    };
  }
}

/**
 * Validates if parsed data is valid for display
 */
export function isValidParsedData(parsed: ParsedDataResult): boolean {
  return parsed.data.length > 0 && parsed.columns.length > 0;
}

/**
 * Gets a summary of the parsed data
 */
export function getDataSummary(parsed: ParsedDataResult): string {
  if (!isValidParsedData(parsed)) {
    return 'No valid data';
  }

  const { data, columns } = parsed;
  return `${data.length} rows × ${columns.length} columns`;
}

/**
 * Debug function to log parsing details
 */
export function debugParseResponse(response: any): void {
  console.group('AKShare Response Debug');
  console.log('Raw response:', response);
  console.log('Response type:', typeof response);
  console.log('Is array:', Array.isArray(response));

  if (response && typeof response === 'object') {
    console.log('Keys:', Object.keys(response));
    console.log('Has data key:', 'data' in response);
    console.log('Has success key:', 'success' in response);
    console.log('Has error key:', 'error' in response);
  }

  const parsed = parseAKShareResponse(response);
  console.log('Parsed result:', parsed);
  console.log('Is valid:', isValidParsedData(parsed));
  console.log('Summary:', getDataSummary(parsed));
  console.groupEnd();
}
