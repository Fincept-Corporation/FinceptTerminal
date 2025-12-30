/**
 * Filter Node
 * Remove items that don't match specified conditions
 *
 * Use cases:
 * - Filter stocks by price range
 * - Remove rows with missing data
 * - Select only profitable trades
 * - Filter news by sentiment
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
  IDataObject,
} from '../../services/nodeSystem/types';

export class FilterNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Filter',
    name: 'filter',
    icon: 'fa:filter',
    iconColor: '#60a5fa',
    group: ['transform'],
    version: 1,
    description: 'Remove items that do not match specified conditions',
    subtitle: '={{$parameter["operation"]}}',
    defaults: {
      name: 'Filter',
      color: '#60a5fa',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        noDataExpression: true,
        options: [
          {
            name: 'Keep Matching Items',
            value: 'keep',
            description: 'Keep only items that match the conditions',
          },
          {
            name: 'Remove Matching Items',
            value: 'remove',
            description: 'Remove items that match the conditions',
          },
        ],
        default: 'keep',
      },
      {
        displayName: 'Conditions',
        name: 'conditions',
        placeholder: 'Add Condition',
        type: NodePropertyType.Collection,
        typeOptions: {
          multipleValues: true,
        },
        default: [],
        options: [
          {
            name: 'condition',
            description: 'Condition',
            values: [
              {
                displayName: 'Field',
                name: 'field',
                type: NodePropertyType.String,
                default: '',
                placeholder: 'e.g., price',
                description: 'The field name to check',
                required: true,
              },
              {
                displayName: 'Operation',
                name: 'operator',
                type: NodePropertyType.Options,
                options: [
                  {
                    name: 'Equal',
                    value: 'equal',
                  },
                  {
                    name: 'Not Equal',
                    value: 'notEqual',
                  },
                  {
                    name: 'Greater Than',
                    value: 'greaterThan',
                  },
                  {
                    name: 'Greater Than or Equal',
                    value: 'greaterThanOrEqual',
                  },
                  {
                    name: 'Less Than',
                    value: 'lessThan',
                  },
                  {
                    name: 'Less Than or Equal',
                    value: 'lessThanOrEqual',
                  },
                  {
                    name: 'Contains',
                    value: 'contains',
                  },
                  {
                    name: 'Not Contains',
                    value: 'notContains',
                  },
                  {
                    name: 'Starts With',
                    value: 'startsWith',
                  },
                  {
                    name: 'Ends With',
                    value: 'endsWith',
                  },
                  {
                    name: 'Is Empty',
                    value: 'isEmpty',
                  },
                  {
                    name: 'Is Not Empty',
                    value: 'isNotEmpty',
                  },
                ],
                default: 'equal',
              },
              {
                displayName: 'Value',
                name: 'value',
                type: NodePropertyType.String,
                default: '',
                description: 'The value to compare against',
                displayOptions: {
                  hide: {
                    operator: ['isEmpty', 'isNotEmpty'],
                  },
                },
              },
            ],
          },
        ],
      },
      {
        displayName: 'Combine Conditions',
        name: 'combineOperation',
        type: NodePropertyType.Options,
        options: [
          {
            name: 'AND (All conditions must match)',
            value: 'and',
          },
          {
            name: 'OR (Any condition can match)',
            value: 'or',
          },
        ],
        default: 'and',
        description: 'How to combine multiple conditions',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: IDataObject[] = [];

    const operation = this.getNodeParameter('operation', 0) as string;
    const combineOperation = this.getNodeParameter('combineOperation', 0) as string;

    for (let itemIndex = 0; itemIndex < items.length; itemIndex++) {
      const item = items[itemIndex];
      const conditions = this.getNodeParameter('conditions.condition', itemIndex, []) as Array<{
        field: string;
        operator: string;
        value: string;
      }>;

      // Evaluate all conditions
      const conditionResults = conditions.map((condition) => {
        return evaluateCondition(item.json, condition);
      });

      // Combine conditions based on combineOperation
      let matches = false;
      if (combineOperation === 'and') {
        matches = conditionResults.every((result) => result === true);
      } else {
        matches = conditionResults.some((result) => result === true);
      }

      // Apply operation
      const shouldKeep =
        (operation === 'keep' && matches) || (operation === 'remove' && !matches);

      if (shouldKeep) {
        returnData.push(item.json);
      }
    }

    return [this.helpers.returnJsonArray(returnData)];
  }
}

/**
 * Evaluate a single condition
 */
function evaluateCondition(
  data: IDataObject,
  condition: { field: string; operator: string; value: string }
): boolean {
  const { field, operator, value } = condition;

  // Get field value (supports dot notation like "price.close")
  const fieldValue = getNestedValue(data, field);

  switch (operator) {
    case 'equal':
      return String(fieldValue) === String(value);

    case 'notEqual':
      return String(fieldValue) !== String(value);

    case 'greaterThan':
      return Number(fieldValue) > Number(value);

    case 'greaterThanOrEqual':
      return Number(fieldValue) >= Number(value);

    case 'lessThan':
      return Number(fieldValue) < Number(value);

    case 'lessThanOrEqual':
      return Number(fieldValue) <= Number(value);

    case 'contains':
      return String(fieldValue).toLowerCase().includes(String(value).toLowerCase());

    case 'notContains':
      return !String(fieldValue).toLowerCase().includes(String(value).toLowerCase());

    case 'startsWith':
      return String(fieldValue).toLowerCase().startsWith(String(value).toLowerCase());

    case 'endsWith':
      return String(fieldValue).toLowerCase().endsWith(String(value).toLowerCase());

    case 'isEmpty':
      return fieldValue === null || fieldValue === undefined || fieldValue === '';

    case 'isNotEmpty':
      return fieldValue !== null && fieldValue !== undefined && fieldValue !== '';

    default:
      return false;
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
