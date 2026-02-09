/**
 * Code Node
 * Execute custom JavaScript code to transform data
 *
 * Use cases:
 * - Custom calculations
 * - Complex data transformations
 * - Financial formulas
 * - Custom algorithms
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
  IDataObject,
  INodeExecutionData,
} from '../../types';

export class CodeNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Code',
    name: 'code',
    icon: 'fa:code',
    iconColor: '#8b5cf6',
    group: ['transform'],
    version: 1,
    description: 'Execute custom JavaScript code to transform data',
    subtitle: 'Run JavaScript Code',
    defaults: {
      name: 'Code',
      color: '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        noDataExpression: true,
        options: [
          {
            name: 'Run Once for All Items',
            value: 'runOnceForAllItems',
            description: 'Execute code once with all items available',
          },
          {
            name: 'Run Once for Each Item',
            value: 'runOnceForEachItem',
            description: 'Execute code separately for each item',
          },
        ],
        default: 'runOnceForEachItem',
      },
      {
        displayName: 'JavaScript Code',
        name: 'jsCode',
        type: NodePropertyType.String,
        typeOptions: {
          editor: 'code',
          rows: 15,
        },
        default: `// Access input data
// For "Run Once for Each Item": use $input.item
// For "Run Once for All Items": use $input.all()

// Example 1: Calculate percentage change
const item = $input.item.json;
const percentChange = ((item.close - item.open) / item.open) * 100;

return {
  ...item,
  percentChange: percentChange.toFixed(2)
};

// Example 2: Filter and transform
// const items = $input.all();
// return items
//   .filter(item => item.json.price > 100)
//   .map(item => ({
//     ...item.json,
//     priceCategory: 'premium'
//   }));`,
        description: 'JavaScript code to execute. Return the transformed data.',
        required: true,
      },
      {
        displayName: 'Options',
        name: 'options',
        type: NodePropertyType.Collection,
        placeholder: 'Add Option',
        default: [],
        options: [
          {
            displayName: 'Continue On Fail',
            name: 'continueOnFail',
            type: NodePropertyType.Boolean,
            default: false,
            description:
              'Whether to continue execution if the code throws an error',
          },
        ],
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const mode = this.getNodeParameter('mode', 0) as string;
    const jsCode = this.getNodeParameter('jsCode', 0) as string;
    const options = this.getNodeParameter('options', 0, {}) as IDataObject;
    const continueOnFail = options.continueOnFail || false;

    const returnData: INodeExecutionData[] = [];

    try {
      if (mode === 'runOnceForAllItems') {
        // Execute code once with all items
        const result = await executeCodeForAllItems(jsCode, items);
        returnData.push(...result);
      } else {
        // Execute code for each item
        for (let itemIndex = 0; itemIndex < items.length; itemIndex++) {
          try {
            const result = await executeCodeForItem(jsCode, items[itemIndex], itemIndex);
            if (Array.isArray(result)) {
              returnData.push(...result);
            } else if (result !== null && result !== undefined) {
              returnData.push(
                typeof result === 'object' && 'json' in result
                  ? (result as INodeExecutionData)
                  : { json: result as IDataObject }
              );
            }
          } catch (error: any) {
            if (continueOnFail) {
              returnData.push({
                json: {
                  error: error.message,
                  itemIndex,
                },
              });
            } else {
              throw error;
            }
          }
        }
      }
    } catch (error: any) {
      if (continueOnFail) {
        returnData.push({
          json: {
            error: error.message,
          },
        });
      } else {
        throw error;
      }
    }

    return [returnData];
  }
}

/**
 * Execute code for a single item
 */
async function executeCodeForItem(
  code: string,
  item: INodeExecutionData,
  itemIndex: number
): Promise<any> {
  // Create execution context
  const $input = {
    item,
    index: itemIndex,
    first: itemIndex === 0,
    last: false, // Will be set correctly in actual implementation
  };

  // Helper functions
  const helpers = {
    // Add helper functions here
    getWorkflowStaticData: () => ({}),
  };

  // Execute code in isolated context
  // Using Function constructor for code execution
  // This is a simplified version - production should use vm module or similar
  const executeFunction = new Function('$input', 'helpers', `
    'use strict';
    ${code}
  `);

  const result = executeFunction($input, helpers);
  return result;
}

/**
 * Execute code for all items
 */
async function executeCodeForAllItems(
  code: string,
  items: INodeExecutionData[]
): Promise<INodeExecutionData[]> {
  // Create execution context
  const $input = {
    all: () => items,
    first: () => items[0],
    last: () => items[items.length - 1],
    item: items[0], // For backward compatibility
  };

  // Helper functions
  const helpers = {
    getWorkflowStaticData: () => ({}),
  };

  // Execute code
  const executeFunction = new Function('$input', 'helpers', `
    'use strict';
    ${code}
  `);

  const result = executeFunction($input, helpers);

  // Ensure result is array of items
  if (!result) {
    return [];
  }

  if (Array.isArray(result)) {
    return result.map((item) =>
      typeof item === 'object' && 'json' in item
        ? (item as INodeExecutionData)
        : { json: item as IDataObject }
    );
  }

  // Single item returned
  return [
    typeof result === 'object' && 'json' in result
      ? (result as INodeExecutionData)
      : { json: result as IDataObject },
  ];
}
