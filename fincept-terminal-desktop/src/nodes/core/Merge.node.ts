/**
 * Merge Node
 * Combine data from multiple inputs
 *
 * Use cases:
 * - Combine price data from multiple sources
 * - Merge technical indicators with price data
 * - Join fundamental data with market data
 * - Append multiple datasets
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
} from '../../services/nodeSystem/types';

export class MergeNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Merge',
    name: 'merge',
    icon: 'fa:code-branch',
    iconColor: '#10b981',
    group: ['transform'],
    version: 1,
    description: 'Combine data from multiple inputs',
    subtitle: '={{$parameter["mode"]}}',
    defaults: {
      name: 'Merge',
      color: '#10b981',
    },
    inputs: [NodeConnectionType.Main, NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        noDataExpression: true,
        options: [
          {
            name: 'Append',
            value: 'append',
            description: 'Combine all items from both inputs into one output',
          },
          {
            name: 'Merge By Position',
            value: 'mergeByPosition',
            description: 'Merge items at the same position (1st with 1st, 2nd with 2nd, etc.)',
          },
          {
            name: 'Merge By Key',
            value: 'mergeByKey',
            description: 'Join items based on matching field values (like SQL JOIN)',
          },
          {
            name: 'Choose Branch',
            value: 'chooseBranch',
            description: 'Output items from only one input',
          },
        ],
        default: 'append',
      },

      // Merge By Key options
      {
        displayName: 'Join',
        name: 'joinMode',
        type: NodePropertyType.Options,
        displayOptions: {
          show: {
            mode: ['mergeByKey'],
          },
        },
        options: [
          {
            name: 'Inner Join',
            value: 'inner',
            description: 'Keep only items that have matches in both inputs',
          },
          {
            name: 'Left Join',
            value: 'left',
            description: 'Keep all items from input 1, add matching data from input 2',
          },
          {
            name: 'Outer Join',
            value: 'outer',
            description: 'Keep all items from both inputs',
          },
        ],
        default: 'inner',
      },
      {
        displayName: 'Key (Input 1)',
        name: 'key1',
        type: NodePropertyType.String,
        displayOptions: {
          show: {
            mode: ['mergeByKey'],
          },
        },
        default: '',
        placeholder: 'e.g., symbol',
        required: true,
        description: 'Field name to match on in Input 1',
      },
      {
        displayName: 'Key (Input 2)',
        name: 'key2',
        type: NodePropertyType.String,
        displayOptions: {
          show: {
            mode: ['mergeByKey'],
          },
        },
        default: '',
        placeholder: 'e.g., ticker',
        required: true,
        description: 'Field name to match on in Input 2',
      },

      // Choose Branch options
      {
        displayName: 'Output',
        name: 'output',
        type: NodePropertyType.Options,
        displayOptions: {
          show: {
            mode: ['chooseBranch'],
          },
        },
        options: [
          {
            name: 'Input 1',
            value: 'input1',
          },
          {
            name: 'Input 2',
            value: 'input2',
          },
        ],
        default: 'input1',
      },

      // Merge By Position options
      {
        displayName: 'Clash Handling',
        name: 'clashHandling',
        type: NodePropertyType.Options,
        displayOptions: {
          show: {
            mode: ['mergeByPosition'],
          },
        },
        options: [
          {
            name: 'Prefer Input 1',
            value: 'preferInput1',
            description: 'If both inputs have the same field, use value from Input 1',
          },
          {
            name: 'Prefer Input 2',
            value: 'preferInput2',
            description: 'If both inputs have the same field, use value from Input 2',
          },
          {
            name: 'Add Suffix',
            value: 'addSuffix',
            description: 'If both inputs have the same field, add suffix (_1, _2)',
          },
        ],
        default: 'preferInput1',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const mode = this.getNodeParameter('mode', 0) as string;

    // Get input data
    const input1 = this.getInputData(0) || [];
    const input2 = this.getInputData(1) || [];

    let returnData: INodeExecutionData[] = [];

    switch (mode) {
      case 'append':
        returnData = mergeAppend(input1, input2);
        break;

      case 'mergeByPosition':
        const clashHandling = this.getNodeParameter('clashHandling', 0) as string;
        returnData = mergeByPosition(input1, input2, clashHandling);
        break;

      case 'mergeByKey':
        const joinMode = this.getNodeParameter('joinMode', 0) as string;
        const key1 = this.getNodeParameter('key1', 0) as string;
        const key2 = this.getNodeParameter('key2', 0) as string;
        returnData = mergeByKey(input1, input2, key1, key2, joinMode);
        break;

      case 'chooseBranch':
        const output = this.getNodeParameter('output', 0) as string;
        returnData = output === 'input1' ? input1 : input2;
        break;

      default:
        returnData = input1;
    }

    return [returnData];
  }
}

/**
 * Append: Combine all items from both inputs
 */
function mergeAppend(
  input1: INodeExecutionData[],
  input2: INodeExecutionData[]
): INodeExecutionData[] {
  return [...input1, ...input2];
}

/**
 * Merge By Position: Merge items at the same index
 */
function mergeByPosition(
  input1: INodeExecutionData[],
  input2: INodeExecutionData[],
  clashHandling: string
): INodeExecutionData[] {
  const result: INodeExecutionData[] = [];
  const maxLength = Math.max(input1.length, input2.length);

  for (let i = 0; i < maxLength; i++) {
    const item1 = input1[i]?.json || {};
    const item2 = input2[i]?.json || {};

    let merged: IDataObject = {};

    if (clashHandling === 'preferInput1') {
      merged = { ...item2, ...item1 };
    } else if (clashHandling === 'preferInput2') {
      merged = { ...item1, ...item2 };
    } else if (clashHandling === 'addSuffix') {
      // Add suffix to conflicting fields
      const keys1 = Object.keys(item1);
      const keys2 = Object.keys(item2);
      const commonKeys = keys1.filter((key) => keys2.includes(key));

      merged = { ...item1, ...item2 };

      for (const key of commonKeys) {
        merged[`${key}_1`] = item1[key];
        merged[`${key}_2`] = item2[key];
        delete merged[key];
      }
    }

    result.push({ json: merged });
  }

  return result;
}

/**
 * Merge By Key: Join items based on matching field values
 */
function mergeByKey(
  input1: INodeExecutionData[],
  input2: INodeExecutionData[],
  key1: string,
  key2: string,
  joinMode: string
): INodeExecutionData[] {
  const result: INodeExecutionData[] = [];

  // Create lookup map for input2
  const input2Map = new Map<any, IDataObject[]>();
  for (const item of input2) {
    const keyValue = getNestedValue(item.json, key2);
    if (!input2Map.has(keyValue)) {
      input2Map.set(keyValue, []);
    }
    input2Map.get(keyValue)!.push(item.json);
  }

  if (joinMode === 'inner' || joinMode === 'left') {
    // Process input1 items
    for (const item1 of input1) {
      const keyValue = getNestedValue(item1.json, key1);
      const matches = input2Map.get(keyValue) || [];

      if (matches.length > 0) {
        // Found matches - merge with each
        for (const item2 of matches) {
          result.push({
            json: { ...item1.json, ...item2 },
          });
        }
      } else if (joinMode === 'left') {
        // Left join - keep item1 even without matches
        result.push({
          json: { ...item1.json },
        });
      }
    }
  }

  if (joinMode === 'outer') {
    // Outer join - include all items from both inputs
    const processedKeys = new Set<any>();

    // Process input1
    for (const item1 of input1) {
      const keyValue = getNestedValue(item1.json, key1);
      processedKeys.add(keyValue);
      const matches = input2Map.get(keyValue) || [];

      if (matches.length > 0) {
        for (const item2 of matches) {
          result.push({
            json: { ...item1.json, ...item2 },
          });
        }
      } else {
        result.push({
          json: { ...item1.json },
        });
      }
    }

    // Add unmatched items from input2
    for (const [keyValue, items] of input2Map) {
      if (!processedKeys.has(keyValue)) {
        for (const item of items) {
          result.push({
            json: { ...item },
          });
        }
      }
    }
  }

  return result;
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
