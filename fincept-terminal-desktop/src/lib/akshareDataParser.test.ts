/**
 * Test cases for AKShare data parser
 * Demonstrates various JSON structures that the parser can handle
 */

import { parseAKShareResponse, isValidParsedData, getDataSummary } from './akshareDataParser';

// Test Case 1: Simple array of objects
export const testCase1_SimpleArray = {
  input: [
    { date: '2024-01-01', price: 100, volume: 1000 },
    { date: '2024-01-02', price: 105, volume: 1200 },
    { date: '2024-01-03', price: 103, volume: 900 }
  ],
  description: 'Simple array of objects'
};

// Test Case 2: Wrapped in success/data structure
export const testCase2_WrappedData = {
  input: {
    success: true,
    data: [
      { symbol: 'AAPL', price: 150 },
      { symbol: 'GOOGL', price: 2800 }
    ],
    count: 2,
    timestamp: 1234567890
  },
  description: 'Data wrapped in success/data structure'
};

// Test Case 3: Nested data.data structure (common in Python)
export const testCase3_NestedData = {
  input: {
    success: true,
    data: {
      data: [
        { ticker: 'TSLA', open: 200, close: 205 },
        { ticker: 'NVDA', open: 400, close: 410 }
      ]
    },
    timestamp: 1234567890
  },
  description: 'Nested data.data structure'
};

// Test Case 4: Data in 'records' key
export const testCase4_RecordsKey = {
  input: {
    success: true,
    records: [
      { id: 1, name: 'Item 1', value: 100 },
      { id: 2, name: 'Item 2', value: 200 }
    ]
  },
  description: 'Data in records key'
};

// Test Case 5: Single object (not an array)
export const testCase5_SingleObject = {
  input: {
    success: true,
    data: {
      total: 1000,
      average: 50,
      median: 45
    }
  },
  description: 'Single object response'
};

// Test Case 6: Inconsistent object keys
export const testCase6_InconsistentKeys = {
  input: [
    { name: 'A', price: 100, volume: 1000 },
    { name: 'B', price: 200 }, // missing volume
    { name: 'C', volume: 1500, extra: 'data' } // missing price, has extra
  ],
  description: 'Inconsistent object keys'
};

// Test Case 7: Array of primitives
export const testCase7_PrimitiveArray = {
  input: {
    data: [100, 200, 300, 400, 500]
  },
  description: 'Array of primitive values'
};

// Test Case 8: Deep nesting
export const testCase8_DeepNesting = {
  input: {
    success: true,
    response: {
      results: {
        data: {
          items: [
            { id: 1, value: 'A' },
            { id: 2, value: 'B' }
          ]
        }
      }
    }
  },
  description: 'Deeply nested data structure'
};

// Test Case 9: Chinese column names
export const testCase9_ChineseColumns = {
  input: [
    { '日期': '2024-01-01', '价格': 100, '成交量': 1000 },
    { '日期': '2024-01-02', '价格': 105, '成交量': 1200 }
  ],
  description: 'Chinese column names'
};

// Test Case 10: Mixed data types
export const testCase10_MixedTypes = {
  input: [
    { id: 1, name: 'Test', value: 100.5, active: true, tags: ['a', 'b'], meta: null },
    { id: 2, name: 'Test2', value: 200.7, active: false, tags: ['c'], meta: { x: 1 } }
  ],
  description: 'Mixed data types'
};

// Test Case 11: Special numeric values
export const testCase11_SpecialNumbers = {
  input: [
    { value: 100 },
    { value: NaN },
    { value: Infinity },
    { value: -Infinity },
    { value: null }
  ],
  description: 'Special numeric values (NaN, Infinity)'
};

// Test Case 12: Empty or null responses
export const testCase12_EmptyData = {
  input: {
    success: true,
    data: []
  },
  description: 'Empty array response'
};

export const testCase13_NullData = {
  input: {
    success: true,
    data: null
  },
  description: 'Null data response'
};

// Test Case 14: Error response
export const testCase14_ErrorResponse = {
  input: {
    success: false,
    error: 'Failed to fetch data from API'
  },
  description: 'Error response'
};

// Test Case 15: Record-style object (keys are IDs)
export const testCase15_RecordStyle = {
  input: {
    data: {
      'AAPL': { price: 150, volume: 1000 },
      'GOOGL': { price: 2800, volume: 500 },
      'MSFT': { price: 380, volume: 800 }
    }
  },
  description: 'Record-style object (keys are identifiers)'
};

// Test Case 16: Complex nested arrays
export const testCase16_NestedArrays = {
  input: {
    success: true,
    data: {
      metadata: { source: 'AKShare', version: '1.0' },
      results: {
        data: [
          { ticker: 'AAPL', prices: [150, 151, 149] },
          { ticker: 'GOOGL', prices: [2800, 2810, 2795] }
        ]
      }
    }
  },
  description: 'Complex nested structure with metadata'
};

/**
 * Run all test cases
 */
export function runAllTests() {
  const testCases = [
    testCase1_SimpleArray,
    testCase2_WrappedData,
    testCase3_NestedData,
    testCase4_RecordsKey,
    testCase5_SingleObject,
    testCase6_InconsistentKeys,
    testCase7_PrimitiveArray,
    testCase8_DeepNesting,
    testCase9_ChineseColumns,
    testCase10_MixedTypes,
    testCase11_SpecialNumbers,
    testCase12_EmptyData,
    testCase13_NullData,
    testCase14_ErrorResponse,
    testCase15_RecordStyle,
    testCase16_NestedArrays
  ];

  console.group('AKShare Parser Test Results');

  testCases.forEach((testCase, index) => {
    console.group(`Test ${index + 1}: ${testCase.description}`);
    console.log('Input:', testCase.input);

    const parsed = parseAKShareResponse(testCase.input);
    console.log('Parsed:', parsed);
    console.log('Valid:', isValidParsedData(parsed));
    console.log('Summary:', getDataSummary(parsed));

    if (parsed.warnings) {
      console.warn('Warnings:', parsed.warnings);
    }

    console.groupEnd();
  });

  console.groupEnd();
}

/**
 * Example usage in console:
 *
 * import { runAllTests } from '@/lib/akshareDataParser.test';
 * runAllTests();
 */
