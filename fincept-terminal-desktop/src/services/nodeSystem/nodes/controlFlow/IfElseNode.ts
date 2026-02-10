/**
 * If/Else Node
 *
 * Conditional routing node that splits workflow execution based on conditions.
 * Routes items to different outputs based on expression evaluation.
 *
 * Features:
 * - Multiple condition evaluation
 * - Expression support
 * - True/False outputs
 * - Fallthrough handling
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class IfElseNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Conditional Branch',
    name: 'ifElse',
    icon: 'fa:code-branch',
    group: ['Control Flow'],
    version: 1,
    description: 'Route items based on conditional logic',
    defaults: {
      name: 'Conditional Branch',
      color: '#3b82f6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [
      { type: NodeConnectionType.Main, displayName: 'True' },
      { type: NodeConnectionType.Main, displayName: 'False' },
    ],
    properties: [
      {
        displayName: 'Conditions',
        name: 'conditions',
        type: NodePropertyType.FixedCollection,
        typeOptions: {
          multipleValues: true,
        },
        default: {},
        options: [
          {
            name: 'conditions',
            displayName: 'Condition',
            values: [
              {
                displayName: 'Field',
                name: 'field',
                type: NodePropertyType.String,
                default: '',
                description: 'Field to evaluate',
              },
              {
                displayName: 'Operation',
                name: 'operation',
                type: NodePropertyType.Options,
                options: [
                  { name: 'Equals', value: 'equals' },
                  { name: 'Not Equals', value: 'notEquals' },
                  { name: 'Greater Than', value: 'greaterThan' },
                  { name: 'Less Than', value: 'lessThan' },
                  { name: 'Greater Than or Equal', value: 'greaterThanOrEqual' },
                  { name: 'Less Than or Equal', value: 'lessThanOrEqual' },
                  { name: 'Contains', value: 'contains' },
                  { name: 'Not Contains', value: 'notContains' },
                  { name: 'Starts With', value: 'startsWith' },
                  { name: 'Ends With', value: 'endsWith' },
                  { name: 'Is Empty', value: 'isEmpty' },
                  { name: 'Is Not Empty', value: 'isNotEmpty' },
                ],
                default: 'equals',
              },
              {
                displayName: 'Value',
                name: 'value',
                type: NodePropertyType.String,
                default: '',
                description: 'Value to compare against',
                displayOptions: {
                  hide: {
                    operation: ['isEmpty', 'isNotEmpty'],
                  },
                },
              },
            ],
          },
        ],
        description: 'Conditions to evaluate',
      },
      {
        displayName: 'Combine',
        name: 'combineOperation',
        type: NodePropertyType.Options,
        options: [
          { name: 'ALL conditions must be true', value: 'all' },
          { name: 'AT LEAST ONE condition must be true', value: 'any' },
        ],
        default: 'all',
        description: 'How to combine multiple conditions',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const trueItems: any[] = [];
    const falseItems: any[] = [];

    const conditions = this.getNodeParameter('conditions', 0, {}) as any;
    const combineOperation = this.getNodeParameter('combineOperation', 0) as string;

    for (let itemIndex = 0; itemIndex < items.length; itemIndex++) {
      const item = items[itemIndex];
      const conditionResults: boolean[] = [];

      // Evaluate each condition
      if (conditions.conditions && Array.isArray(conditions.conditions)) {
        for (const condition of conditions.conditions) {
          const field = condition.field;
          const operation = condition.operation;
          const compareValue = condition.value;

          // Get field value from item
          const fieldValue = getFieldValue(item.json, field);

          // Evaluate condition
          const result = evaluateCondition(
            fieldValue,
            operation,
            compareValue
          );

          conditionResults.push(result);
        }
      }

      // Combine condition results
      let finalResult = false;
      if (conditionResults.length > 0) {
        if (combineOperation === 'all') {
          finalResult = conditionResults.every((r) => r === true);
        } else {
          finalResult = conditionResults.some((r) => r === true);
        }
      }

      // Route to appropriate output
      if (finalResult) {
        trueItems.push({
          ...item,
          pairedItem: itemIndex,
        });
      } else {
        falseItems.push({
          ...item,
          pairedItem: itemIndex,
        });
      }
    }

    return [trueItems, falseItems];
  }

}

/**
 * Get field value from object using dot notation
 */
function getFieldValue(obj: any, path: string): any {
  const keys = path.split('.');
  let value = obj;

  for (const key of keys) {
    if (value === null || value === undefined) {
      return undefined;
    }
    value = value[key];
  }

  return value;
}

/**
 * Evaluate a single condition
 */
function evaluateCondition(
  fieldValue: any,
  operation: string,
  compareValue: any
): boolean {
    // Convert types if needed
    const fieldStr = String(fieldValue);
    const compareStr = String(compareValue);

    switch (operation) {
      case 'equals':
        return fieldValue == compareValue;

      case 'notEquals':
        return fieldValue != compareValue;

      case 'greaterThan':
        return Number(fieldValue) > Number(compareValue);

      case 'lessThan':
        return Number(fieldValue) < Number(compareValue);

      case 'greaterThanOrEqual':
        return Number(fieldValue) >= Number(compareValue);

      case 'lessThanOrEqual':
        return Number(fieldValue) <= Number(compareValue);

      case 'contains':
        return fieldStr.includes(compareStr);

      case 'notContains':
        return !fieldStr.includes(compareStr);

      case 'startsWith':
        return fieldStr.startsWith(compareStr);

      case 'endsWith':
        return fieldStr.endsWith(compareStr);

      case 'isEmpty':
        return (
          fieldValue === null ||
          fieldValue === undefined ||
          fieldValue === '' ||
          (Array.isArray(fieldValue) && fieldValue.length === 0) ||
          (typeof fieldValue === 'object' &&
            Object.keys(fieldValue).length === 0)
        );

      case 'isNotEmpty':
        return !(
          fieldValue === null ||
          fieldValue === undefined ||
          fieldValue === '' ||
          (Array.isArray(fieldValue) && fieldValue.length === 0) ||
          (typeof fieldValue === 'object' &&
            Object.keys(fieldValue).length === 0)
        );

      default:
        return false;
    }
}
