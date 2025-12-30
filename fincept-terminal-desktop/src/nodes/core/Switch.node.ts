/**
 * Switch Node
 * Route items to different outputs based on conditions
 *
 * Use cases:
 * - Route stocks by sector
 * - Separate buy/sell signals
 * - Split data by price range
 * - Classify trades by profitability
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

export class SwitchNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Switch',
    name: 'switch',
    icon: 'fa:map-signs',
    iconColor: '#f59e0b',
    group: ['transform'],
    version: 1,
    description: 'Route items to different outputs based on conditions',
    subtitle: '={{$parameter["mode"]}} Mode',
    defaults: {
      name: 'Switch',
      color: '#f59e0b',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [
      NodeConnectionType.Main,
      NodeConnectionType.Main,
      NodeConnectionType.Main,
      NodeConnectionType.Main,
    ],
    outputNames: ['Output 1', 'Output 2', 'Output 3', 'Fallback'],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        noDataExpression: true,
        options: [
          {
            name: 'Rules',
            value: 'rules',
            description: 'Route based on conditional rules',
          },
          {
            name: 'Expression',
            value: 'expression',
            description: 'Route based on expression result',
          },
        ],
        default: 'rules',
      },

      // Rules mode
      {
        displayName: 'Routing Rules',
        name: 'rules',
        placeholder: 'Add Routing Rule',
        type: NodePropertyType.FixedCollection,
        typeOptions: {
          multipleValues: true,
        },
        displayOptions: {
          show: {
            mode: ['rules'],
          },
        },
        default: [],
        description: 'Route items to different outputs based on rules',
        options: [
          {
            name: 'rule',
            description: 'Routing Rule',
            values: [
              {
                displayName: 'Output',
                name: 'output',
                type: NodePropertyType.Number,
                default: 0,
                description: 'Output index to route to (0-2, or 3 for fallback)',
                typeOptions: {
                  minValue: 0,
                  maxValue: 3,
                },
              },
              {
                displayName: 'Field',
                name: 'field',
                type: NodePropertyType.String,
                default: '',
                placeholder: 'e.g., price',
                description: 'Field to check',
                required: true,
              },
              {
                displayName: 'Operation',
                name: 'operator',
                type: NodePropertyType.Options,
                options: [
                  { name: 'Equal', value: 'equal' },
                  { name: 'Not Equal', value: 'notEqual' },
                  { name: 'Greater Than', value: 'greaterThan' },
                  { name: 'Less Than', value: 'lessThan' },
                  { name: 'Contains', value: 'contains' },
                  { name: 'Starts With', value: 'startsWith' },
                  { name: 'Ends With', value: 'endsWith' },
                  { name: 'Is Empty', value: 'isEmpty' },
                ],
                default: 'equal',
              },
              {
                displayName: 'Value',
                name: 'value',
                type: NodePropertyType.String,
                default: '',
                description: 'Value to compare against',
                displayOptions: {
                  hide: {
                    operator: ['isEmpty'],
                  },
                },
              },
            ],
          },
        ],
      },

      // Expression mode
      {
        displayName: 'Output Expression',
        name: 'outputExpression',
        type: NodePropertyType.String,
        displayOptions: {
          show: {
            mode: ['expression'],
          },
        },
        default: '',
        placeholder: '={{$json.category === "technology" ? 0 : 1}}',
        description: 'Expression that returns output index (0-3)',
      },

      {
        displayName: 'Fallback Output',
        name: 'fallbackOutput',
        type: NodePropertyType.Options,
        options: [
          {
            name: 'Output 4 (Default)',
            value: 3,
          },
          {
            name: 'Discard Item',
            value: -1,
          },
        ],
        default: 3,
        description: 'Where to send items that do not match any rule',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const mode = this.getNodeParameter('mode', 0) as string;
    const fallbackOutput = this.getNodeParameter('fallbackOutput', 0) as number;

    // Initialize output arrays
    const outputs: INodeExecutionData[][] = [[], [], [], []];

    for (let itemIndex = 0; itemIndex < items.length; itemIndex++) {
      const item = items[itemIndex];
      let outputIndex = -1;

      if (mode === 'rules') {
        // Rules-based routing
        const rules = this.getNodeParameter('rules.rule', itemIndex, []) as Array<{
          output: number;
          field: string;
          operator: string;
          value: string;
        }>;

        // Find first matching rule
        for (const rule of rules) {
          if (evaluateRule(item.json, rule)) {
            outputIndex = rule.output;
            break;
          }
        }
      } else if (mode === 'expression') {
        // Expression-based routing
        const expression = this.getNodeParameter('outputExpression', itemIndex) as string;

        // Simple expression evaluation
        // TODO: Replace with full expression engine
        outputIndex = evaluateExpression(expression, item.json);
      }

      // If no rule matched, use fallback
      if (outputIndex === -1) {
        outputIndex = fallbackOutput;
      }

      // Add item to appropriate output
      if (outputIndex >= 0 && outputIndex < outputs.length) {
        outputs[outputIndex].push(item);
      }
      // If outputIndex is -1, item is discarded
    }

    return outputs;
  }
}

/**
 * Evaluate a routing rule
 */
function evaluateRule(
  data: IDataObject,
  rule: { field: string; operator: string; value: string }
): boolean {
  const { field, operator, value } = rule;

  const fieldValue = getNestedValue(data, field);

  switch (operator) {
    case 'equal':
      return String(fieldValue) === String(value);

    case 'notEqual':
      return String(fieldValue) !== String(value);

    case 'greaterThan':
      return Number(fieldValue) > Number(value);

    case 'lessThan':
      return Number(fieldValue) < Number(value);

    case 'contains':
      return String(fieldValue).toLowerCase().includes(String(value).toLowerCase());

    case 'startsWith':
      return String(fieldValue).toLowerCase().startsWith(String(value).toLowerCase());

    case 'endsWith':
      return String(fieldValue).toLowerCase().endsWith(String(value).toLowerCase());

    case 'isEmpty':
      return fieldValue === null || fieldValue === undefined || fieldValue === '';

    default:
      return false;
  }
}

/**
 * Simple expression evaluation
 * Returns output index (0-3)
 */
function evaluateExpression(expression: string, data: IDataObject): number {
  // Very basic implementation - will be replaced with full expression engine
  // For now, just return 0 as default
  // TODO: Implement proper expression evaluation
  return 0;
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
